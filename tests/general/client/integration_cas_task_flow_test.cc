#include <gtest/gtest.h>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/beast/http.hpp>
#include <future>
#include <memory>
#include <string>
#include <thread>

#include "bcas/client/cas_client_task.h"
#include "bcas/client/cas_proxy_task.h"
#include "bsrvcore/allocator/allocator.h"
#include "bsrvcore/connection/server/http_server_task.h"
#include "bsrvcore/core/http_server.h"
#include "bsrvcore/route/http_request_method.h"
#include "test_http_client_task.h"

namespace {

namespace http = boost::beast::http;
using bcas::test::ServerGuard;
using bcas::test::StartServerWithRoutes;

TEST(CasTaskFlowIntegrationTest, ValidateTaskBuildsExpectedQuery) {
  auto server = bsrvcore::AllocateUnique<bsrvcore::HttpServer>(2);
  auto captured_target = bsrvcore::AllocateShared<std::string>();

  server->AddRouteEntry(
      bsrvcore::HttpRequestMethod::kGet, "/cas/p3/proxyValidate",
      [captured_target](std::shared_ptr<bsrvcore::HttpServerTask> task) {
        *captured_target = std::string(task->GetRequest().target());
        task->SetField(http::field::content_type, "application/xml");
        task->SetBody(
            R"(<?xml version="1.0"?><cas:serviceResponse xmlns:cas="http://www.yale.edu/tp/cas"><cas:authenticationSuccess><cas:user>tester</cas:user></cas:authenticationSuccess></cas:serviceResponse>)");
      });

  ServerGuard guard(std::move(server));
  const auto port = StartServerWithRoutes(guard);

  bsrvcore::IoContext ioc;
  bcas::client::CasClientParams params;
  params.server_base_url = "http://127.0.0.1:" + std::to_string(port) + "/cas";
  params.service_url = "https://service.example.org/app?a=b c";
  params.ticket = "PT-1/2";
  params.version = bcas::client::CasProtocolVersion::kCas3;
  params.endpoint = bcas::client::CasValidationEndpoint::kProxyValidate;
  params.renew = true;
  params.proxy_callback_url = "https://service.example.org/callback";

  auto task = bcas::client::CasClientTask::Create(ioc.get_executor(), params);
  std::promise<bcas::client::CasClientResult> promise;
  auto future = promise.get_future();
  task->OnDone([&promise](const bcas::client::CasClientResult& result) {
    promise.set_value(result);
  });

  task->Start();
  ioc.run();

  const auto result = future.get();
  ASSERT_EQ(result.status, bcas::client::CasTaskStatus::kSuccess);
  EXPECT_EQ(
      *captured_target,
      "/cas/p3/proxyValidate?service=https%3A%2F%2Fservice.example.org%2Fapp%3Fa%3Db%20c&ticket=PT-1%2F2&renew=true&pgtUrl=https%3A%2F%2Fservice.example.org%2Fcallback");
}

TEST(CasTaskFlowIntegrationTest, ProxyTaskBuildsExpectedQuery) {
  auto server = bsrvcore::AllocateUnique<bsrvcore::HttpServer>(2);
  auto captured_target = bsrvcore::AllocateShared<std::string>();

  server->AddRouteEntry(
      bsrvcore::HttpRequestMethod::kGet, "/cas/proxy",
      [captured_target](std::shared_ptr<bsrvcore::HttpServerTask> task) {
        *captured_target = std::string(task->GetRequest().target());
        task->SetField(http::field::content_type, "application/xml");
        task->SetBody(
            R"(<?xml version="1.0"?><cas:serviceResponse xmlns:cas="http://www.yale.edu/tp/cas"><cas:proxySuccess><cas:proxyTicket>PT-99</cas:proxyTicket></cas:proxySuccess></cas:serviceResponse>)");
      });

  ServerGuard guard(std::move(server));
  const auto port = StartServerWithRoutes(guard);

  bsrvcore::IoContext ioc;
  bcas::client::CasProxyParams params;
  params.server_base_url = "http://127.0.0.1:" + std::to_string(port) + "/cas";
  params.proxy_granting_ticket = "PGT-42/abc";
  params.target_service = "https://backend.example.org/api?v=1";

  auto task = bcas::client::CasProxyTask::Create(ioc.get_executor(), params);
  std::promise<bcas::client::CasProxyResult> promise;
  auto future = promise.get_future();
  task->OnDone([&promise](const bcas::client::CasProxyResult& result) {
    promise.set_value(result);
  });

  task->Start();
  ioc.run();

  const auto result = future.get();
  ASSERT_EQ(result.status, bcas::client::CasTaskStatus::kSuccess);
  EXPECT_EQ(
      *captured_target,
      "/cas/proxy?pgt=PGT-42%2Fabc&targetService=https%3A%2F%2Fbackend.example.org%2Fapi%3Fv%3D1");
  EXPECT_EQ(result.proxy_ticket, "PT-99");
}

TEST(CasTaskFlowIntegrationTest, HttpErrorWithoutCasPayloadMapsToHttpError) {
  auto server = bsrvcore::AllocateUnique<bsrvcore::HttpServer>(2);

  server->AddRouteEntry(
      bsrvcore::HttpRequestMethod::kGet, "/cas/serviceValidate",
      [](std::shared_ptr<bsrvcore::HttpServerTask> task) {
        task->GetResponse().result(http::status::internal_server_error);
        task->SetField(http::field::content_type, "text/plain");
        task->SetBody("boom");
      });

  ServerGuard guard(std::move(server));
  const auto port = StartServerWithRoutes(guard);

  bsrvcore::IoContext ioc;
  bcas::client::CasClientParams params;
  params.server_base_url = "http://127.0.0.1:" + std::to_string(port) + "/cas";
  params.service_url = "https://service.example.org/app";
  params.ticket = "ST-500";
  params.version = bcas::client::CasProtocolVersion::kCas2;

  auto task = bcas::client::CasClientTask::Create(ioc.get_executor(), params);
  std::promise<bcas::client::CasClientResult> promise;
  auto future = promise.get_future();
  task->OnDone([&promise](const bcas::client::CasClientResult& result) {
    promise.set_value(result);
  });

  task->Start();
  ioc.run();

  const auto result = future.get();
  EXPECT_EQ(result.status, bcas::client::CasTaskStatus::kHttpError);
  EXPECT_EQ(result.http_status_code, 500);
  EXPECT_EQ(result.raw_response_body, "boom");
}

TEST(CasTaskFlowIntegrationTest, DoneCallbackUsesConfiguredCallbackExecutor) {
  auto server = bsrvcore::AllocateUnique<bsrvcore::HttpServer>(2);
  server->AddRouteEntry(
      bsrvcore::HttpRequestMethod::kGet, "/cas/serviceValidate",
      [](std::shared_ptr<bsrvcore::HttpServerTask> task) {
        task->SetField(http::field::content_type, "application/xml");
        task->SetBody(
            R"(<?xml version="1.0"?><cas:serviceResponse xmlns:cas="http://www.yale.edu/tp/cas"><cas:authenticationSuccess><cas:user>tester</cas:user></cas:authenticationSuccess></cas:serviceResponse>)");
      });

  ServerGuard guard(std::move(server));
  const auto port = StartServerWithRoutes(guard);

  bsrvcore::IoContext io_ioc;
  bsrvcore::IoContext callback_ioc;
  auto callback_guard = boost::asio::make_work_guard(callback_ioc);

  bcas::client::CasClientParams params;
  params.server_base_url = "http://127.0.0.1:" + std::to_string(port) + "/cas";
  params.service_url = "https://service.example.org/app";
  params.ticket = "ST-1";
  params.version = bcas::client::CasProtocolVersion::kCas2;

  auto task = bcas::client::CasClientTask::Create(
      io_ioc.get_executor(), callback_ioc.get_executor(), params);

  std::promise<std::thread::id> callback_thread_promise;
  auto callback_thread_future = callback_thread_promise.get_future();
  std::promise<std::thread::id> callback_executor_thread_promise;
  auto callback_executor_thread_future =
      callback_executor_thread_promise.get_future();

  task->OnDone([&](const bcas::client::CasClientResult& result) {
    EXPECT_EQ(result.status, bcas::client::CasTaskStatus::kSuccess);
    callback_thread_promise.set_value(std::this_thread::get_id());
    callback_guard.reset();
    io_ioc.stop();
    callback_ioc.stop();
  });

  task->Start();

  std::thread io_thread([&]() { io_ioc.run(); });
  std::thread callback_thread([&]() {
    callback_executor_thread_promise.set_value(std::this_thread::get_id());
    callback_ioc.run();
  });

  const auto callback_thread_id = callback_thread_future.get();
  const auto callback_executor_thread_id =
      callback_executor_thread_future.get();

  io_thread.join();
  callback_thread.join();

  EXPECT_EQ(callback_thread_id, callback_executor_thread_id);
}

}  // namespace
