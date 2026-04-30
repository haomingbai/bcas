# CAS client

This chapter maps to:

- `include/bcas/client/cas_client_task.h`
- `include/bcas/client/cas_proxy_task.h`
- `include/bcas/client/cas_proxy_callback.h`
- `examples/cas_validate_demo/cas_validate_demo.cc`

## One-sentence idea

`bcas::client` wraps `bsrvcore::HttpClientTask` into CAS-specific one-shot
tasks that build the right URLs, parse XML, and report a single typed result.

The current `bcas` tree has been verified to build and pass its
unit/integration test suite against `bsrvcore` `v0.14.0` through `v0.18.2`.

## Data model

- `CasClientParams`: one validation request
- `CasClientResult`: final validation result
- `CasProxyParams`: one `/proxy` request
- `CasProxyResult`: final `/proxy` result
- `CasProxyGrantingTicketCallbackData`: parsed callback query payload

`CasClientResult::attributes` preserves repeated CAS attributes as
`std::vector<std::string>`, so CAS 2/3 attribute blocks such as repeated
`memberOf` or `affiliation` values remain intact.

## Validation task

Use `CasClientTask` for:

- `/serviceValidate`
- `/proxyValidate`
- `/p3/serviceValidate`
- `/p3/proxyValidate`

The chosen endpoint is controlled by:

- `CasProtocolVersion`
- `CasValidationEndpoint`

The task:

1. validates local arguments
2. constructs a `bsrvcore::HttpClientTask`
3. performs the HTTP GET
4. parses CAS XML
5. invokes `OnDone` exactly once

## Result semantics

`CasTaskStatus` is the main error surface:

- `kSuccess`: valid CAS success payload
- `kCasFailure`: valid CAS failure payload
- `kTransportError`: DNS/TCP/TLS/request transport failure
- `kHttpError`: no usable CAS XML and HTTP status was an error
- `kParseError`: response arrived but XML was malformed or incomplete
- `kInvalidArgument`: bad local input or invalid generated URL
- `kCancelled`: task was cancelled

Important precedence rule:

- if the body is valid CAS XML, protocol success/failure wins even when the
  HTTP status code is non-2xx

## CAS 2/3 attributes

`bcas` pays special attention to CAS 2/3 attribute blocks:

- all attributes under `<attributes>` are copied into `CasAttributeMap`
- repeated elements remain repeated
- common fields are also exposed as convenience fields:
  `authentication_date`,
  `long_term_authentication_request_token_used`,
  `is_from_new_login`

The raw body is always preserved in `raw_response_body`.

## Proxy flow

Use `CasProxyTask` for the `/proxy` endpoint.

Typical flow:

1. validate a ticket with `CasClientTask`
2. pass `pgtUrl` so CAS calls your callback endpoint
3. correlate `proxyGrantingTicket` from the validation result with the callback
   `pgtId`
4. call `CasProxyTask` with the actual PGT and target service

`CasProxyTask` only covers the `/proxy` request itself. `bcas` does not host
your callback server or store `PGTIOU -> PGT` mappings for you.

## PGT callback parsing

For an incoming callback request such as:

```text
/cas/pgtCallback?pgtId=PGT-1&pgtIou=PGTIOU-1
```

use:

- `ParseProxyGrantingTicketCallbackTarget(...)`
- `ParseProxyGrantingTicketCallbackRequest(...)`

This keeps callback parsing independent from how you build your actual HTTPS
service endpoint.
