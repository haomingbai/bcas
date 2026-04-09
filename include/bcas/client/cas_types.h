/**
 * @file cas_types.h
 * @brief Shared CAS client data types and result models.
 * @author Haoming Bai <haomingbai@hotmail.com>
 * @date   2026-04-09
 *
 * Copyright (c) 2026 Haoming Bai
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifndef BCAS_CLIENT_CAS_TYPES_H_
#define BCAS_CLIENT_CAS_TYPES_H_

#include <boost/system/error_code.hpp>
#include <unordered_map>
#include <string>
#include <vector>

#include "bsrvcore/connection/client/http_client_task.h"
#include "bsrvcore/core/trait.h"

namespace bcas::client {

/**
 * @brief CAS protocol generation used when constructing validation URLs.
 */
enum class CasProtocolVersion {
  /** @brief CAS 2.x endpoints (`/serviceValidate`, `/proxyValidate`). */
  kCas2,
  /** @brief CAS 3.x endpoints (`/p3/serviceValidate`, `/p3/proxyValidate`). */
  kCas3,
};

/**
 * @brief Validation endpoint family used by CasClientTask.
 */
enum class CasValidationEndpoint {
  /** @brief Validate a service ticket only. */
  kServiceValidate,
  /** @brief Validate a service ticket or proxy ticket. */
  kProxyValidate,
};

/**
 * @brief High-level outcome class for CAS tasks.
 */
enum class CasTaskStatus {
  /** @brief Request completed successfully and protocol payload was valid. */
  kSuccess,
  /** @brief CAS returned a protocol-level failure payload. */
  kCasFailure,
  /** @brief Network or TLS transport failed. */
  kTransportError,
  /** @brief HTTP response was unusable and did not contain a valid CAS body. */
  kHttpError,
  /** @brief XML payload could not be parsed or did not match expected shape. */
  kParseError,
  /** @brief Local arguments or generated URLs were invalid. */
  kInvalidArgument,
  /** @brief Task was cancelled before completion. */
  kCancelled,
};

/**
 * @brief Multi-valued CAS attribute map.
 *
 * Repeated XML elements under `<attributes>` are preserved as repeated vector
 * entries so clients can consume both single-valued and multi-valued claims.
 */
using CasAttributeMap = std::unordered_map<std::string, std::vector<std::string>>;

/**
 * @brief Input parameters for one CAS validation task.
 */
struct CasClientParams : public bsrvcore::CopyableMovable<CasClientParams> {
  /** @brief CAS server base URL, for example `https://cas.example.org/cas`. */
  std::string server_base_url;
  /** @brief Original service URL used when the ticket was issued. */
  std::string service_url;
  /** @brief Service ticket or proxy ticket to validate. */
  std::string ticket;
  /** @brief CAS protocol generation used to choose endpoint prefix. */
  CasProtocolVersion version{CasProtocolVersion::kCas3};
  /** @brief Validation endpoint family to invoke. */
  CasValidationEndpoint endpoint{CasValidationEndpoint::kServiceValidate};
  /** @brief Require primary-credential login when validating. */
  bool renew{false};
  /**
   * @brief Optional HTTPS callback URL used to request a proxy-granting ticket.
   *
   * When present, the URL is sent as `pgtUrl`.
   */
  std::string proxy_callback_url;
  /** @brief Transport-level options forwarded to bsrvcore::HttpClientTask. */
  bsrvcore::HttpClientOptions http_options;
};

/**
 * @brief Final validation result delivered by CasClientTask.
 */
struct CasClientResult : public bsrvcore::CopyableMovable<CasClientResult> {
  /** @brief Task completion category. */
  CasTaskStatus status{CasTaskStatus::kInvalidArgument};
  /** @brief Transport-layer error code from the underlying HTTP client task. */
  boost::system::error_code transport_ec;
  /** @brief Final HTTP status code seen on the CAS response. */
  int http_status_code{0};
  /** @brief True when completion came from explicit cancellation. */
  bool cancelled{false};
  /** @brief Authenticated CAS principal name. */
  std::string user;
  /** @brief Multi-valued attribute map parsed from CAS 2/3 responses. */
  CasAttributeMap attributes;
  /** @brief Raw `authenticationDate` attribute, if present. */
  std::string authentication_date;
  /**
   * @brief Remember-me flag reported by CAS.
   *
   * This field is meaningful when the corresponding attribute was present.
   */
  bool long_term_authentication_request_token_used{false};
  /** @brief True when the corresponding remember-me attribute was present. */
  bool has_long_term_authentication_request_token_used{false};
  /** @brief `isFromNewLogin` flag reported by CAS. */
  bool is_from_new_login{false};
  /** @brief True when `isFromNewLogin` was present in the response. */
  bool has_is_from_new_login{false};
  /**
   * @brief Proxy-granting ticket IOU returned by CAS validation.
   *
   * Despite the XML element name `proxyGrantingTicket`, validation responses
   * carry the PGT IOU rather than the actual PGT value.
   */
  std::string proxy_granting_ticket_iou;
  /** @brief Proxy chain reported by `/proxyValidate`. */
  std::vector<std::string> proxies;
  /** @brief CAS protocol failure code, if any. */
  std::string failure_code;
  /** @brief Detailed error or failure description. */
  std::string failure_message;
  /** @brief Unmodified response body for debugging and audit purposes. */
  std::string raw_response_body;

  /**
   * @brief Return true when the task completed with a successful CAS payload.
   */
  [[nodiscard]] bool Succeeded() const noexcept {
    return status == CasTaskStatus::kSuccess;
  }
};

/**
 * @brief Input parameters for one CAS `/proxy` task.
 */
struct CasProxyParams : public bsrvcore::CopyableMovable<CasProxyParams> {
  /** @brief CAS server base URL, for example `https://cas.example.org/cas`. */
  std::string server_base_url;
  /** @brief Proxy-granting ticket acquired via validation callback. */
  std::string proxy_granting_ticket;
  /** @brief Back-end target service for the generated proxy ticket. */
  std::string target_service;
  /** @brief Transport-level options forwarded to bsrvcore::HttpClientTask. */
  bsrvcore::HttpClientOptions http_options;
};

/**
 * @brief Final `/proxy` result delivered by CasProxyTask.
 */
struct CasProxyResult : public bsrvcore::CopyableMovable<CasProxyResult> {
  /** @brief Task completion category. */
  CasTaskStatus status{CasTaskStatus::kInvalidArgument};
  /** @brief Transport-layer error code from the underlying HTTP client task. */
  boost::system::error_code transport_ec;
  /** @brief Final HTTP status code seen on the CAS response. */
  int http_status_code{0};
  /** @brief True when completion came from explicit cancellation. */
  bool cancelled{false};
  /** @brief Proxy ticket returned by `/proxy`. */
  std::string proxy_ticket;
  /** @brief CAS protocol failure code, if any. */
  std::string failure_code;
  /** @brief Detailed error or failure description. */
  std::string failure_message;
  /** @brief Unmodified response body for debugging and audit purposes. */
  std::string raw_response_body;

  /**
   * @brief Return true when the task completed with a successful CAS payload.
   */
  [[nodiscard]] bool Succeeded() const noexcept {
    return status == CasTaskStatus::kSuccess;
  }
};

}  // namespace bcas::client

#endif  // BCAS_CLIENT_CAS_TYPES_H_
