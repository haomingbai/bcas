/**
 * @file cas_detail.h
 * @brief Internal helpers shared by bcas task implementations.
 */

#pragma once

#ifndef BCAS_SRC_CLIENT_CAS_DETAIL_H_
#define BCAS_SRC_CLIENT_CAS_DETAIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "bcas/client/cas_proxy_callback.h"
#include "bcas/client/cas_types.h"

namespace bsrvcore {
struct HttpClientResult;
}  // namespace bsrvcore

namespace bcas::client::detail {

[[nodiscard]] std::string BuildValidateUrl(const CasClientParams& params,
                                           std::string* error_message);

[[nodiscard]] std::string BuildProxyUrl(const CasProxyParams& params,
                                        std::string* error_message);

[[nodiscard]] CasClientResult ConvertValidationResult(
    const bsrvcore::HttpClientResult& result);

[[nodiscard]] CasProxyResult ConvertProxyResult(
    const bsrvcore::HttpClientResult& result);

[[nodiscard]] std::optional<CasProxyGrantingTicketCallbackData>
ParseProxyGrantingTicketCallbackTargetView(std::string_view target);

[[nodiscard]] bool IsHttpsUrl(std::string_view url);

}  // namespace bcas::client::detail

#endif  // BCAS_SRC_CLIENT_CAS_DETAIL_H_
