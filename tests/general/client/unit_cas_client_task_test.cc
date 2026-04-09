#include <gtest/gtest.h>

#include <boost/beast/http.hpp>
#include <future>
#include <memory>
#include <string>

#include "bcas/client/cas_client_task.h"
#include "bsrvcore/allocator/allocator.h"
#include "bsrvcore/connection/server/http_server_task.h"
#include "bsrvcore/core/http_server.h"
#include "bsrvcore/route/http_request_method.h"
#include "test_http_client_task.h"

namespace {

namespace http = boost::beast::http;
using bcas::test::ServerGuard;
using bcas::test::StartServerWithRoutes;

bcas::client::CasClientResult WaitForResult(
    bsrvcore::IoContext& ioc,
    const std::shared_ptr<bcas::client::CasClientTask>& task) {
  std::promise<bcas::client::CasClientResult> promise;
  auto future = promise.get_future();

  task->OnDone([&promise](const bcas::client::CasClientResult& result) {
    promise.set_value(result);
  });
  task->Start();
  ioc.run();
  return future.get();
}

TEST(CasClientTaskTest, InvalidArgumentsReturnImmediateFailure) {
  bsrvcore::IoContext ioc;

  bcas::client::CasClientParams params;
  params.server_base_url = "ftp://cas.example.org/cas";
  params.service_url = "https://service.example.org/app";
  params.ticket = "ST-1";

  auto task = bcas::client::CasClientTask::Create(ioc.get_executor(), params);
  const auto result = WaitForResult(ioc, task);

  EXPECT_EQ(result.status, bcas::client::CasTaskStatus::kInvalidArgument);
  EXPECT_FALSE(result.failure_message.empty());
}

TEST(CasClientTaskTest, CancelBeforeStartCompletesCancelled) {
  bsrvcore::IoContext ioc;

  bcas::client::CasClientParams params;
  params.server_base_url = "https://cas.example.org/cas";
  params.service_url = "https://service.example.org/app";
  params.ticket = "ST-1";

  auto task = bcas::client::CasClientTask::Create(ioc.get_executor(), params);
  task->Cancel();

  const auto result = WaitForResult(ioc, task);

  EXPECT_EQ(result.status, bcas::client::CasTaskStatus::kCancelled);
  EXPECT_TRUE(result.cancelled);
}

TEST(CasClientTaskTest, SuccessParsesCas3AttributesAndProxyChain) {
  auto server = bsrvcore::AllocateUnique<bsrvcore::HttpServer>(2);
  server->AddRouteEntry(
      bsrvcore::HttpRequestMethod::kGet, "/cas/p3/proxyValidate",
      [](std::shared_ptr<bsrvcore::HttpServerTask> task) {
        task->SetField(http::field::content_type, "application/xml");
        task->SetBody(
            R"(<?xml version="1.0" encoding="UTF-8"?>
<s:serviceResponse xmlns:s="http://www.yale.edu/tp/cas">
  <s:authenticationSuccess>
    <s:user>alice</s:user>
    <s:proxyGrantingTicket>PGTIOU-42</s:proxyGrantingTicket>
    <s:proxies>
      <s:proxy>https://proxy2.example.org</s:proxy>
      <s:proxy>https://proxy1.example.org</s:proxy>
    </s:proxies>
    <s:attributes>
      <s:email>alice@example.org</s:email>
      <s:memberOf>ops</s:memberOf>
      <s:memberOf>infra</s:memberOf>
      <s:isFromNewLogin>true</s:isFromNewLogin>
      <s:longTermAuthenticationRequestTokenUsed>false</s:longTermAuthenticationRequestTokenUsed>
      <s:authenticationDate>2026-04-09T00:00:00Z</s:authenticationDate>
    </s:attributes>
  </s:authenticationSuccess>
</s:serviceResponse>)");
      });

  ServerGuard guard(std::move(server));
  const auto port = StartServerWithRoutes(guard);

  bsrvcore::IoContext ioc;
  bcas::client::CasClientParams params;
  params.server_base_url = "http://127.0.0.1:" + std::to_string(port) + "/cas";
  params.service_url = "https://service.example.org/app";
  params.ticket = "PT-1";
  params.endpoint = bcas::client::CasValidationEndpoint::kProxyValidate;
  params.version = bcas::client::CasProtocolVersion::kCas3;

  auto task = bcas::client::CasClientTask::Create(ioc.get_executor(), params);
  const auto result = WaitForResult(ioc, task);

  ASSERT_EQ(result.status, bcas::client::CasTaskStatus::kSuccess);
  EXPECT_EQ(result.user, "alice");
  EXPECT_EQ(result.proxy_granting_ticket_iou, "PGTIOU-42");
  ASSERT_EQ(result.proxies.size(), 2U);
  EXPECT_EQ(result.proxies[0], "https://proxy2.example.org");
  EXPECT_EQ(result.proxies[1], "https://proxy1.example.org");
  ASSERT_TRUE(result.attributes.contains("memberOf"));
  EXPECT_EQ(result.attributes.at("memberOf").size(), 2U);
  EXPECT_EQ(result.attributes.at("memberOf")[0], "ops");
  EXPECT_EQ(result.attributes.at("memberOf")[1], "infra");
  EXPECT_TRUE(result.has_is_from_new_login);
  EXPECT_TRUE(result.is_from_new_login);
  EXPECT_TRUE(result.has_long_term_authentication_request_token_used);
  EXPECT_FALSE(result.long_term_authentication_request_token_used);
  EXPECT_EQ(result.authentication_date, "2026-04-09T00:00:00Z");
}

TEST(CasClientTaskTest, CasFailurePayloadWinsOverHttpStatus) {
  auto server = bsrvcore::AllocateUnique<bsrvcore::HttpServer>(2);
  server->AddRouteEntry(
      bsrvcore::HttpRequestMethod::kGet, "/cas/serviceValidate",
      [](std::shared_ptr<bsrvcore::HttpServerTask> task) {
        task->GetResponse().result(http::status::internal_server_error);
        task->SetField(http::field::content_type, "application/xml");
        task->SetBody(
            R"(<?xml version="1.0" encoding="UTF-8"?>
<cas:serviceResponse xmlns:cas="http://www.yale.edu/tp/cas">
  <cas:authenticationFailure code="INVALID_TICKET">
    Ticket ST-1 not recognized
  </cas:authenticationFailure>
</cas:serviceResponse>)");
      });

  ServerGuard guard(std::move(server));
  const auto port = StartServerWithRoutes(guard);

  bsrvcore::IoContext ioc;
  bcas::client::CasClientParams params;
  params.server_base_url = "http://127.0.0.1:" + std::to_string(port) + "/cas";
  params.service_url = "https://service.example.org/app";
  params.ticket = "ST-1";
  params.version = bcas::client::CasProtocolVersion::kCas2;

  auto task = bcas::client::CasClientTask::Create(ioc.get_executor(), params);
  const auto result = WaitForResult(ioc, task);

  EXPECT_EQ(result.status, bcas::client::CasTaskStatus::kCasFailure);
  EXPECT_EQ(result.failure_code, "INVALID_TICKET");
  EXPECT_NE(result.http_status_code, 0);
}

TEST(CasClientTaskTest, MalformedXmlProducesParseError) {
  auto server = bsrvcore::AllocateUnique<bsrvcore::HttpServer>(2);
  server->AddRouteEntry(
      bsrvcore::HttpRequestMethod::kGet, "/cas/serviceValidate",
      [](std::shared_ptr<bsrvcore::HttpServerTask> task) {
        task->SetField(http::field::content_type, "application/xml");
        task->SetBody("<cas:serviceResponse><cas:authenticationSuccess>");
      });

  ServerGuard guard(std::move(server));
  const auto port = StartServerWithRoutes(guard);

  bsrvcore::IoContext ioc;
  bcas::client::CasClientParams params;
  params.server_base_url = "http://127.0.0.1:" + std::to_string(port) + "/cas";
  params.service_url = "https://service.example.org/app";
  params.ticket = "ST-bad";
  params.version = bcas::client::CasProtocolVersion::kCas2;

  auto task = bcas::client::CasClientTask::Create(ioc.get_executor(), params);
  const auto result = WaitForResult(ioc, task);

  EXPECT_EQ(result.status, bcas::client::CasTaskStatus::kParseError);
  EXPECT_FALSE(result.failure_message.empty());
}

}  // namespace
