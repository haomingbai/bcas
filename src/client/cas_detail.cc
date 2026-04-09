/**
 * @file cas_detail.cc
 * @brief Internal CAS URL, XML, and result-conversion helpers.
 * @author Haoming Bai <haomingbai@hotmail.com>
 * @date   2026-04-09
 *
 * Copyright (c) 2026 Haoming Bai
 * SPDX-License-Identifier: MIT
 */

#include "client/cas_detail.h"

#include <boost/system/errc.hpp>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pugixml.hpp>

#include "bsrvcore/connection/client/http_client_task.h"

namespace bcas::client::detail {

namespace {

using bsrvcore::HttpClientErrorStage;
using bsrvcore::HttpClientResult;

constexpr std::string_view kHttpScheme = "http://";
constexpr std::string_view kHttpsScheme = "https://";

std::string TrimCopy(std::string_view text) {
  std::size_t begin = 0;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
    ++begin;
  }

  std::size_t end = text.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }

  return std::string(text.substr(begin, end - begin));
}

std::string ToLowerAscii(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char ch : text) {
    out.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(ch))));
  }
  return out;
}

std::optional<bool> ParseBooleanText(std::string_view text) {
  const std::string lowered = ToLowerAscii(TrimCopy(text));
  if (lowered == "true" || lowered == "1" || lowered == "yes") {
    return true;
  }
  if (lowered == "false" || lowered == "0" || lowered == "no") {
    return false;
  }
  return std::nullopt;
}

std::string LocalName(std::string_view qualified_name) {
  const std::size_t pos = qualified_name.find(':');
  if (pos == std::string_view::npos) {
    return std::string(qualified_name);
  }
  return std::string(qualified_name.substr(pos + 1));
}

pugi::xml_node FindFirstChildByLocalName(const pugi::xml_node& node,
                                         std::string_view local_name) {
  for (const pugi::xml_node& child : node.children()) {
    if (child.type() != pugi::node_element) {
      continue;
    }
    if (LocalName(child.name()) == local_name) {
      return child;
    }
  }
  return {};
}

bool StartsWith(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         text.substr(0, prefix.size()) == prefix;
}

bool IsSupportedBaseUrl(std::string_view url) {
  if (!StartsWith(url, kHttpScheme) && !StartsWith(url, kHttpsScheme)) {
    return false;
  }

  return url.find('?') == std::string_view::npos &&
         url.find('#') == std::string_view::npos;
}

std::string RemoveTrailingSlashes(std::string_view text) {
  std::string out(text);
  while (!out.empty() && out.back() == '/') {
    out.pop_back();
  }
  return out;
}

std::string UrlEncode(std::string_view value) {
  std::string out;
  out.reserve(value.size() * 3);

  constexpr char kHex[] = "0123456789ABCDEF";
  for (const unsigned char ch : value) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' ||
        ch == '~') {
      out.push_back(static_cast<char>(ch));
      continue;
    }

    out.push_back('%');
    out.push_back(kHex[(ch >> 4U) & 0x0F]);
    out.push_back(kHex[ch & 0x0F]);
  }

  return out;
}

bool UrlDecode(std::string_view value, std::string* out) {
  std::string decoded;
  decoded.reserve(value.size());

  auto hex_value = [](char ch) -> int {
    if (ch >= '0' && ch <= '9') {
      return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
      return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
      return 10 + (ch - 'A');
    }
    return -1;
  };

  for (std::size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if (ch == '+') {
      decoded.push_back(' ');
      continue;
    }

    if (ch != '%') {
      decoded.push_back(ch);
      continue;
    }

    if (i + 2 >= value.size()) {
      return false;
    }

    const int hi = hex_value(value[i + 1]);
    const int lo = hex_value(value[i + 2]);
    if (hi < 0 || lo < 0) {
      return false;
    }

    decoded.push_back(static_cast<char>((hi << 4) | lo));
    i += 2;
  }

  *out = std::move(decoded);
  return true;
}

std::optional<std::string> FindQueryParameter(std::string_view target,
                                              std::string_view key) {
  const std::size_t query_pos = target.find('?');
  if (query_pos == std::string_view::npos || query_pos + 1 >= target.size()) {
    return std::nullopt;
  }

  std::string_view query = target.substr(query_pos + 1);
  while (!query.empty()) {
    const std::size_t amp_pos = query.find('&');
    const std::string_view part =
        amp_pos == std::string_view::npos ? query : query.substr(0, amp_pos);

    const std::size_t eq_pos = part.find('=');
    const std::string_view raw_key =
        eq_pos == std::string_view::npos ? part : part.substr(0, eq_pos);
    const std::string_view raw_value =
        eq_pos == std::string_view::npos ? std::string_view{}
                                         : part.substr(eq_pos + 1);

    std::string decoded_key;
    std::string decoded_value;
    if (!UrlDecode(raw_key, &decoded_key) || !UrlDecode(raw_value,
                                                        &decoded_value)) {
      return std::nullopt;
    }

    if (decoded_key == key) {
      return decoded_value;
    }

    if (amp_pos == std::string_view::npos) {
      break;
    }
    query.remove_prefix(amp_pos + 1);
  }

  return std::nullopt;
}

bool ValidateClientParams(const CasClientParams& params,
                          std::string* error_message) {
  if (!IsSupportedBaseUrl(params.server_base_url)) {
    if (error_message != nullptr) {
      *error_message = "server_base_url must start with http:// or https://";
    }
    return false;
  }
  if (params.service_url.empty()) {
    if (error_message != nullptr) {
      *error_message = "service_url must not be empty";
    }
    return false;
  }
  if (params.ticket.empty()) {
    if (error_message != nullptr) {
      *error_message = "ticket must not be empty";
    }
    return false;
  }
  if (!params.proxy_callback_url.empty() &&
      !StartsWith(params.proxy_callback_url, kHttpsScheme)) {
    if (error_message != nullptr) {
      *error_message = "proxy_callback_url must use https://";
    }
    return false;
  }

  return true;
}

bool ValidateProxyParams(const CasProxyParams& params,
                         std::string* error_message) {
  if (!IsSupportedBaseUrl(params.server_base_url)) {
    if (error_message != nullptr) {
      *error_message = "server_base_url must start with http:// or https://";
    }
    return false;
  }
  if (params.proxy_granting_ticket.empty()) {
    if (error_message != nullptr) {
      *error_message = "proxy_granting_ticket must not be empty";
    }
    return false;
  }
  if (params.target_service.empty()) {
    if (error_message != nullptr) {
      *error_message = "target_service must not be empty";
    }
    return false;
  }
  return true;
}

std::string JoinBaseUrlAndPath(std::string_view base_url,
                               std::string_view path) {
  std::string out = RemoveTrailingSlashes(base_url);
  if (out.empty() || path.empty()) {
    return out;
  }

  if (path.front() != '/') {
    out.push_back('/');
  }
  out.append(path);
  return out;
}

CasTaskStatus MapTransportFailure(const HttpClientResult& result) {
  if (result.cancelled) {
    return CasTaskStatus::kCancelled;
  }
  if (!result.ec) {
    return CasTaskStatus::kSuccess;
  }

  if (result.error_stage == HttpClientErrorStage::kCreate &&
      result.ec ==
          make_error_code(boost::system::errc::invalid_argument)) {
    return CasTaskStatus::kInvalidArgument;
  }

  return CasTaskStatus::kTransportError;
}

void RecordCommonFailure(CasTaskStatus status, int http_status_code,
                         std::string_view message,
                         CasClientResult* out) {
  out->status = status;
  out->http_status_code = http_status_code;
  out->failure_message = std::string(message);
}

void RecordCommonFailure(CasTaskStatus status, int http_status_code,
                         std::string_view message,
                         CasProxyResult* out) {
  out->status = status;
  out->http_status_code = http_status_code;
  out->failure_message = std::string(message);
}

template <typename ResultType>
void CopyHttpEnvelope(const HttpClientResult& input, ResultType* out) {
  out->transport_ec = input.ec;
  out->http_status_code = input.response.result_int();
  out->cancelled = input.cancelled;
  out->raw_response_body = input.response.body();
}

void StoreAttribute(const std::string& key, const std::string& value,
                    CasAttributeMap* out) {
  if (key.empty()) {
    return;
  }
  (*out)[key].push_back(value);
}

bool ParseValidationXml(const std::string& body, CasClientResult* out,
                        std::string* error_message) {
  pugi::xml_document doc;
  const pugi::xml_parse_result parse_result =
      doc.load_string(body.c_str(), pugi::parse_default | pugi::parse_trim_pcdata);
  if (!parse_result) {
    if (error_message != nullptr) {
      *error_message = std::string("XML parse failure: ") + parse_result.description();
    }
    return false;
  }

  pugi::xml_node response = doc.document_element();
  if (!response || LocalName(response.name()) != "serviceResponse") {
    response = FindFirstChildByLocalName(doc, "serviceResponse");
  }
  if (!response) {
    if (error_message != nullptr) {
      *error_message = "Missing <serviceResponse> root node";
    }
    return false;
  }

  const pugi::xml_node success =
      FindFirstChildByLocalName(response, "authenticationSuccess");
  if (success) {
    const pugi::xml_node user = FindFirstChildByLocalName(success, "user");
    if (!user) {
      if (error_message != nullptr) {
        *error_message = "Missing <user> in <authenticationSuccess>";
      }
      return false;
    }

    out->status = CasTaskStatus::kSuccess;
    out->user = TrimCopy(user.text().get());

    const pugi::xml_node pgt =
        FindFirstChildByLocalName(success, "proxyGrantingTicket");
    if (pgt) {
      out->proxy_granting_ticket_iou = TrimCopy(pgt.text().get());
    }

    const pugi::xml_node proxies =
        FindFirstChildByLocalName(success, "proxies");
    if (proxies) {
      for (const pugi::xml_node& proxy_node : proxies.children()) {
        if (proxy_node.type() != pugi::node_element ||
            LocalName(proxy_node.name()) != "proxy") {
          continue;
        }
        out->proxies.push_back(TrimCopy(proxy_node.text().get()));
      }
    }

    const pugi::xml_node attributes =
        FindFirstChildByLocalName(success, "attributes");
    if (attributes) {
      for (const pugi::xml_node& attribute : attributes.children()) {
        if (attribute.type() != pugi::node_element) {
          continue;
        }

        const std::string name = LocalName(attribute.name());
        const std::string value = TrimCopy(attribute.text().get());
        StoreAttribute(name, value, &out->attributes);

        if (name == "authenticationDate") {
          out->authentication_date = value;
        } else if (name == "longTermAuthenticationRequestTokenUsed") {
          if (const auto parsed = ParseBooleanText(value); parsed.has_value()) {
            out->long_term_authentication_request_token_used = *parsed;
            out->has_long_term_authentication_request_token_used = true;
          }
        } else if (name == "isFromNewLogin") {
          if (const auto parsed = ParseBooleanText(value); parsed.has_value()) {
            out->is_from_new_login = *parsed;
            out->has_is_from_new_login = true;
          }
        }
      }
    }

    return true;
  }

  const pugi::xml_node failure =
      FindFirstChildByLocalName(response, "authenticationFailure");
  if (failure) {
    out->status = CasTaskStatus::kCasFailure;
    out->failure_code =
        TrimCopy(failure.attribute("code").as_string());
    out->failure_message = TrimCopy(failure.text().get());
    return true;
  }

  if (error_message != nullptr) {
    *error_message =
        "Missing <authenticationSuccess> or <authenticationFailure>";
  }
  return false;
}

bool ParseProxyXml(const std::string& body, CasProxyResult* out,
                   std::string* error_message) {
  pugi::xml_document doc;
  const pugi::xml_parse_result parse_result =
      doc.load_string(body.c_str(), pugi::parse_default | pugi::parse_trim_pcdata);
  if (!parse_result) {
    if (error_message != nullptr) {
      *error_message = std::string("XML parse failure: ") + parse_result.description();
    }
    return false;
  }

  pugi::xml_node response = doc.document_element();
  if (!response || LocalName(response.name()) != "serviceResponse") {
    response = FindFirstChildByLocalName(doc, "serviceResponse");
  }
  if (!response) {
    if (error_message != nullptr) {
      *error_message = "Missing <serviceResponse> root node";
    }
    return false;
  }

  const pugi::xml_node success =
      FindFirstChildByLocalName(response, "proxySuccess");
  if (success) {
    const pugi::xml_node proxy_ticket =
        FindFirstChildByLocalName(success, "proxyTicket");
    if (!proxy_ticket) {
      if (error_message != nullptr) {
        *error_message = "Missing <proxyTicket> in <proxySuccess>";
      }
      return false;
    }

    out->status = CasTaskStatus::kSuccess;
    out->proxy_ticket = TrimCopy(proxy_ticket.text().get());
    return true;
  }

  const pugi::xml_node failure =
      FindFirstChildByLocalName(response, "proxyFailure");
  if (failure) {
    out->status = CasTaskStatus::kCasFailure;
    out->failure_code =
        TrimCopy(failure.attribute("code").as_string());
    out->failure_message = TrimCopy(failure.text().get());
    return true;
  }

  if (error_message != nullptr) {
    *error_message = "Missing <proxySuccess> or <proxyFailure>";
  }
  return false;
}

}  // namespace

std::string BuildValidateUrl(const CasClientParams& params,
                             std::string* error_message) {
  if (!ValidateClientParams(params, error_message)) {
    return {};
  }

  std::string endpoint_path;
  if (params.version == CasProtocolVersion::kCas3) {
    endpoint_path = params.endpoint == CasValidationEndpoint::kProxyValidate
                        ? "/p3/proxyValidate"
                        : "/p3/serviceValidate";
  } else {
    endpoint_path = params.endpoint == CasValidationEndpoint::kProxyValidate
                        ? "/proxyValidate"
                        : "/serviceValidate";
  }

  std::string url = JoinBaseUrlAndPath(params.server_base_url, endpoint_path);
  url.append("?service=");
  url.append(UrlEncode(params.service_url));
  url.append("&ticket=");
  url.append(UrlEncode(params.ticket));
  if (params.renew) {
    url.append("&renew=true");
  }
  if (!params.proxy_callback_url.empty()) {
    url.append("&pgtUrl=");
    url.append(UrlEncode(params.proxy_callback_url));
  }
  return url;
}

std::string BuildProxyUrl(const CasProxyParams& params,
                          std::string* error_message) {
  if (!ValidateProxyParams(params, error_message)) {
    return {};
  }

  std::string url = JoinBaseUrlAndPath(params.server_base_url, "/proxy");
  url.append("?pgt=");
  url.append(UrlEncode(params.proxy_granting_ticket));
  url.append("&targetService=");
  url.append(UrlEncode(params.target_service));
  return url;
}

CasClientResult ConvertValidationResult(const HttpClientResult& result) {
  CasClientResult out;
  CopyHttpEnvelope(result, &out);

  const CasTaskStatus transport_status = MapTransportFailure(result);
  if (transport_status != CasTaskStatus::kSuccess) {
    out.status = transport_status;
    if (transport_status == CasTaskStatus::kTransportError ||
        transport_status == CasTaskStatus::kInvalidArgument) {
      out.failure_message = result.ec.message();
    } else if (transport_status == CasTaskStatus::kCancelled) {
      out.failure_message = "request cancelled";
    }
    return out;
  }

  std::string parse_error;
  if (ParseValidationXml(out.raw_response_body, &out, &parse_error)) {
    return out;
  }

  if (out.http_status_code >= 400) {
    RecordCommonFailure(CasTaskStatus::kHttpError, out.http_status_code,
                        parse_error.empty()
                            ? ("HTTP status " + std::to_string(out.http_status_code))
                            : parse_error,
                        &out);
    return out;
  }

  RecordCommonFailure(CasTaskStatus::kParseError, out.http_status_code,
                      parse_error, &out);
  return out;
}

CasProxyResult ConvertProxyResult(const HttpClientResult& result) {
  CasProxyResult out;
  CopyHttpEnvelope(result, &out);

  const CasTaskStatus transport_status = MapTransportFailure(result);
  if (transport_status != CasTaskStatus::kSuccess) {
    out.status = transport_status;
    if (transport_status == CasTaskStatus::kTransportError ||
        transport_status == CasTaskStatus::kInvalidArgument) {
      out.failure_message = result.ec.message();
    } else if (transport_status == CasTaskStatus::kCancelled) {
      out.failure_message = "request cancelled";
    }
    return out;
  }

  std::string parse_error;
  if (ParseProxyXml(out.raw_response_body, &out, &parse_error)) {
    return out;
  }

  if (out.http_status_code >= 400) {
    RecordCommonFailure(CasTaskStatus::kHttpError, out.http_status_code,
                        parse_error.empty()
                            ? ("HTTP status " + std::to_string(out.http_status_code))
                            : parse_error,
                        &out);
    return out;
  }

  RecordCommonFailure(CasTaskStatus::kParseError, out.http_status_code,
                      parse_error, &out);
  return out;
}

std::optional<CasProxyGrantingTicketCallbackData>
ParseProxyGrantingTicketCallbackTargetView(std::string_view target) {
  const std::optional<std::string> pgt_id =
      FindQueryParameter(target, "pgtId");
  if (!pgt_id.has_value()) {
    return std::nullopt;
  }

  const std::optional<std::string> pgt_iou =
      FindQueryParameter(target, "pgtIou");
  if (!pgt_iou.has_value()) {
    return std::nullopt;
  }

  CasProxyGrantingTicketCallbackData data;
  data.proxy_granting_ticket = *pgt_id;
  data.proxy_granting_ticket_iou = *pgt_iou;
  return data;
}

bool IsHttpsUrl(std::string_view url) {
  return StartsWith(url, kHttpsScheme);
}

}  // namespace bcas::client::detail
