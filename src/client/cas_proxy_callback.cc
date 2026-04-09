/**
 * @file cas_proxy_callback.cc
 * @brief CAS PGT callback parsing helpers.
 * @author Haoming Bai <haomingbai@hotmail.com>
 * @date   2026-04-09
 *
 * Copyright (c) 2026 Haoming Bai
 * SPDX-License-Identifier: MIT
 */

#include "bcas/client/cas_proxy_callback.h"

#include "client/cas_detail.h"

namespace bcas::client {

std::optional<CasProxyGrantingTicketCallbackData>
ParseProxyGrantingTicketCallbackTarget(const std::string& target) {
  return detail::ParseProxyGrantingTicketCallbackTargetView(target);
}

bool TryParseProxyGrantingTicketCallbackTarget(
    const std::string& target, CasProxyGrantingTicketCallbackData& out) {
  const auto parsed = ParseProxyGrantingTicketCallbackTarget(target);
  if (!parsed.has_value()) {
    return false;
  }
  out = *parsed;
  return true;
}

std::optional<CasProxyGrantingTicketCallbackData>
ParseProxyGrantingTicketCallbackRequest(const bsrvcore::HttpRequest& request) {
  return ParseProxyGrantingTicketCallbackTarget(
      std::string(request.target()));
}

bool TryParseProxyGrantingTicketCallbackRequest(
    const bsrvcore::HttpRequest& request,
    CasProxyGrantingTicketCallbackData& out) {
  const auto parsed = ParseProxyGrantingTicketCallbackRequest(request);
  if (!parsed.has_value()) {
    return false;
  }
  out = *parsed;
  return true;
}

}  // namespace bcas::client
