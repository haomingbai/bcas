/**
 * @file cas_validate_demo.cc
 * @brief Minimal CLI that validates one CAS ticket.
 */

#include <bcas/bcas.h>

#include <future>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " <cas_base_url> <service_url> <ticket>\n";
    return 1;
  }

  bsrvcore::IoContext ioc;

  bcas::client::CasClientParams params;
  params.server_base_url = argv[1];
  params.service_url = argv[2];
  params.ticket = argv[3];
  params.version = bcas::client::CasProtocolVersion::kCas3;

  auto task = bcas::client::CasClientTask::Create(ioc.get_executor(), params);

  std::promise<bcas::client::CasClientResult> promise;
  auto future = promise.get_future();
  task->OnDone([&promise](const bcas::client::CasClientResult& result) {
    promise.set_value(result);
  });

  task->Start();
  ioc.run();

  const auto result = future.get();
  if (!result.Succeeded()) {
    std::cerr << "Validation failed. status="
              << static_cast<int>(result.status)
              << " message=" << result.failure_message << '\n';
    return 2;
  }

  std::cout << "user=" << result.user << '\n';
  for (const auto& [key, values] : result.attributes) {
    for (const auto& value : values) {
      std::cout << "attribute[" << key << "]=" << value << '\n';
    }
  }
  return 0;
}
