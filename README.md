# bcas

`bcas` is a standalone CAS client library built on top of `bsrvcore`
client APIs.

The current `bcas` implementation has been verified to build against
`bsrvcore` `v0.14.0` through `v0.16.0`. When `bcas` auto-fetches `bsrvcore`,
it now defaults to `v0.16.0`.

It ships as a normal CMake package:

- package name: `bcas`
- imported target: `bcas::bcas`
- public namespace: `bcas::client`

## What It Covers

- CAS 2 validation: `/serviceValidate`, `/proxyValidate`
- CAS 3 validation: `/p3/serviceValidate`, `/p3/proxyValidate`
- CAS 2/3 XML attribute parsing with multi-valued attributes preserved
- `/proxy` flow via `CasProxyTask`
- PGT callback parsing helpers for `pgtId` and `pgtIou`

## Dependency Resolution

`bcas` resolves `bsrvcore` in this order:

1. Existing CMake target `bsrvcore::bsrvcore`
2. Local config package via `find_package(bsrvcore CONFIG)`
3. Explicit local source tree via `BCAS_BSRVCORE_SOURCE_DIR`
4. Network fetch via `FetchContent`

For XML parsing, `bcas` uses `pugixml` as a private dependency. It resolves
`pugixml` in this order:

1. Existing target `pugixml::static` or `pugixml::pugixml`
2. Explicit local source tree via `BCAS_PUGIXML_SOURCE_DIR`
3. Network fetch via `FetchContent`

`pugixml` is pinned to `v1.15` by default.

You can point `bcas` at a local `bsrvcore` build tree directly:

```bash
cmake -S . -B build \
  -DBCAS_BSRVCORE_CMAKE_DIR=/path/to/bsrvcore/build
```

You can point `bcas` at a local `bsrvcore` source tree:

```bash
cmake -S . -B build \
  -DBCAS_BSRVCORE_SOURCE_DIR=/path/to/bsrvcore
```

You can override fetched refs:

```bash
cmake -S . -B build \
  -DBCAS_BSRVCORE_GIT_TAG=v0.16.0 \
  -DBCAS_PUGIXML_GIT_TAG=v1.15
```

## Build

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBCAS_BUILD_TESTS=ON
cmake --build build --parallel
```

## Install

```bash
cmake --install build --prefix /tmp/bcas-install
```

## Quick Example

```cpp
#include <bcas/bcas.h>

#include <future>

bsrvcore::IoContext ioc;

bcas::client::CasClientParams params;
params.server_base_url = "https://cas.example.org/cas";
params.service_url = "https://service.example.org/app";
params.ticket = "ST-1";
params.version = bcas::client::CasProtocolVersion::kCas3;

auto task = bcas::client::CasClientTask::Create(ioc.get_executor(), params);

std::promise<bcas::client::CasClientResult> promise;
auto future = promise.get_future();

task->OnDone([&](const bcas::client::CasClientResult& result) {
  promise.set_value(result);
});

task->Start();
ioc.run();

const auto result = future.get();
if (result.Succeeded()) {
  // result.user / result.attributes / result.proxy_granting_ticket_iou ...
}
```

There is also a small CLI example in
`examples/cas_validate_demo/cas_validate_demo.cc`.

## Testing

Run unit and integration tests:

```bash
ctest --test-dir build --output-on-failure
```

Run the optional CAS container smoke check:

```bash
./scripts/cas_container_smoke.sh
```

The smoke check verifies that `apereo/cas:7.3.6` can boot with a generated
keystore and serve `/cas/login`. Protocol semantics are still covered by the
in-process integration tests.

## Docs

See [docs/manual/cas-client.md](docs/manual/cas-client.md).
