#pragma once

#include <boost/asio/ip/address.hpp>
#include <boost/beast/http.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "bsrvcore/core/http_server.h"

namespace bcas::test {

using tcp = bsrvcore::Tcp;

inline unsigned short FindFreePort() {
  bsrvcore::IoContext ioc;
  tcp::acceptor acceptor(ioc, {boost::asio::ip::make_address("127.0.0.1"), 0});
  return acceptor.local_endpoint().port();
}

struct ServerGuard {
  explicit ServerGuard(bsrvcore::OwnedPtr<bsrvcore::HttpServer> srv)
      : server(std::move(srv)) {}

  ~ServerGuard() {
    if (server) {
      server->Stop();
    }
  }

  bsrvcore::OwnedPtr<bsrvcore::HttpServer> server;
};

inline unsigned short StartServerWithRoutes(ServerGuard& guard) {
  constexpr int kMaxAttempts = 5;
  for (int i = 0; i < kMaxAttempts; ++i) {
    const unsigned short port = FindFreePort();
    guard.server->AddListen({boost::asio::ip::make_address("127.0.0.1"), port},
                            1);
    if (guard.server->Start()) {
      return port;
    }
    guard.server->Stop();
    std::this_thread::yield();
  }
  throw std::runtime_error("Failed to start server after retries");
}

}  // namespace bcas::test
