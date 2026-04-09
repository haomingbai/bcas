#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "bcas/client/cas_proxy_callback.h"
#include "bsrvcore/core/types.h"

namespace {

TEST(CasProxyCallbackTest, ParsesTargetWithUrlDecodedValues) {
  const auto parsed = bcas::client::ParseProxyGrantingTicketCallbackTarget(
      "/callback?pgtId=PGT-1%2Fabc&pgtIou=PGTIOU-2%2Bxyz");

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->proxy_granting_ticket, "PGT-1/abc");
  EXPECT_EQ(parsed->proxy_granting_ticket_iou, "PGTIOU-2+xyz");
}

TEST(CasProxyCallbackTest, ParsesRequestObject) {
  bsrvcore::HttpRequest request;
  request.target("/cb?pgtIou=PGTIOU-3&pgtId=PGT-3");

  bcas::client::CasProxyGrantingTicketCallbackData parsed;
  ASSERT_TRUE(bcas::client::TryParseProxyGrantingTicketCallbackRequest(
      request, parsed));
  EXPECT_EQ(parsed.proxy_granting_ticket, "PGT-3");
  EXPECT_EQ(parsed.proxy_granting_ticket_iou, "PGTIOU-3");
}

TEST(CasProxyCallbackTest, MissingRequiredParameterReturnsEmpty) {
  const auto parsed = bcas::client::ParseProxyGrantingTicketCallbackTarget(
      "/callback?pgtId=PGT-1");
  EXPECT_FALSE(parsed.has_value());
}

TEST(CasProxyCallbackTest, InvalidPercentEncodingReturnsEmpty) {
  const auto parsed = bcas::client::ParseProxyGrantingTicketCallbackTarget(
      "/callback?pgtId=PGT-%GG&pgtIou=PGTIOU-1");
  EXPECT_FALSE(parsed.has_value());
}

}  // namespace
