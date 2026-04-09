/**
 * @file cas_proxy_callback.h
 * @brief Helpers for parsing CAS proxy-granting ticket callback requests.
 * @author Haoming Bai <haomingbai@hotmail.com>
 * @date   2026-04-09
 *
 * Copyright (c) 2026 Haoming Bai
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifndef BCAS_CLIENT_CAS_PROXY_CALLBACK_H_
#define BCAS_CLIENT_CAS_PROXY_CALLBACK_H_

#include <optional>
#include <string>

#include "bsrvcore/core/types.h"

namespace bcas::client {

/**
 * @brief Parsed PGT callback payload sent by CAS to the service callback URL.
 */
struct CasProxyGrantingTicketCallbackData {
  /** @brief Proxy-granting ticket issued by CAS. */
  std::string proxy_granting_ticket;
  /** @brief Proxy-granting ticket IOU used to correlate validation response. */
  std::string proxy_granting_ticket_iou;
};

/**
 * @brief Parse a PGT callback target string.
 * @param target Request target, such as `/callback?pgtId=...&pgtIou=...`.
 * @return Parsed callback data when both required parameters are present.
 */
[[nodiscard]] std::optional<CasProxyGrantingTicketCallbackData>
ParseProxyGrantingTicketCallbackTarget(const std::string& target);

/**
 * @brief Parse a PGT callback target string into an output object.
 * @param target Request target, such as `/callback?pgtId=...&pgtIou=...`.
 * @param out Parsed callback data on success.
 * @return True when both required parameters were found.
 */
[[nodiscard]] bool TryParseProxyGrantingTicketCallbackTarget(
    const std::string& target, CasProxyGrantingTicketCallbackData& out);

/**
 * @brief Parse a PGT callback from a bsrvcore request object.
 * @param request Incoming callback request.
 * @return Parsed callback data when both required parameters are present.
 */
[[nodiscard]] std::optional<CasProxyGrantingTicketCallbackData>
ParseProxyGrantingTicketCallbackRequest(const bsrvcore::HttpRequest& request);

/**
 * @brief Parse a PGT callback from a bsrvcore request object.
 * @param request Incoming callback request.
 * @param out Parsed callback data on success.
 * @return True when both required parameters were found.
 */
[[nodiscard]] bool TryParseProxyGrantingTicketCallbackRequest(
    const bsrvcore::HttpRequest& request,
    CasProxyGrantingTicketCallbackData& out);

}  // namespace bcas::client

#endif  // BCAS_CLIENT_CAS_PROXY_CALLBACK_H_
