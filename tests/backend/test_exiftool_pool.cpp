#include "exiftool/exiftool_backend.hpp"
#include "umm/backend.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace {

int fail(const char* message) {
  std::cerr << message << '\n';
  return 1;
}

umm::internal::ExifToolBackend* as_exiftool() {
  return static_cast<umm::internal::ExifToolBackend*>(
      umm::BackendManager::instance().get("exiftool"));
}

int write_hang_script(const std::filesystem::path& path) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "sleep 30;\n";
  return out ? 0 : 1;
}

}  // namespace

int main() {
  const std::filesystem::path hang =
      std::filesystem::temp_directory_path() / "umm-exiftool-pool-hang.pl";
  if (write_hang_script(hang) != 0) {
    return fail("failed to write hang script");
  }
  const std::filesystem::path media =
      std::filesystem::path(UMM_FIXTURES_DIR) / "jpeg/minimal.jpg";

  umm::ExifToolConfig hang_config;
  hang_config.exiftool_script = hang;
  hang_config.perl_interpreter = std::filesystem::path(std::u8string(
      reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_PERL)));
  hang_config.command_timeout = std::chrono::milliseconds{2500};
  hang_config.max_processes = 2;
  umm::BackendManager::instance().configureExifTool(hang_config);

  umm::internal::ExifToolBackend* backend = as_exiftool();
  if (!backend) {
    return fail("exiftool backend missing");
  }
  std::atomic<bool> start{false};
  std::vector<std::thread> workers;
  for (int i = 0; i < 2; ++i) {
    workers.emplace_back([&] {
      while (!start.load()) {
        std::this_thread::yield();
      }
      (void)backend->readBase(media);
    });
  }
  start.store(true);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
  std::size_t peak = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    peak = std::max(peak, backend->liveProcessCount());
    if (peak >= 2) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
  }
  if (peak > 2) {
    return fail("live process count exceeded max_processes");
  }
  if (peak < 2) {
    return fail("pool did not start max_processes children");
  }

  umm::ExifToolConfig real;
  real.exiftool_script = std::filesystem::path(std::u8string(
      reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_SCRIPT)));
  real.perl_interpreter = hang_config.perl_interpreter;
  real.max_processes = 1;
  real.command_timeout = std::chrono::milliseconds{30000};
  umm::BackendManager::instance().configureExifTool(real);
  for (std::thread& worker : workers) {
    worker.join();
  }
  if (backend->liveProcessCount() != 0) {
    return fail("retired children were not reaped");
  }
  const auto read = backend->readBase(media);
  if (!read.ok()) {
    std::cerr << read.error().message << '\n';
    return fail("read after reconfigure failed");
  }
  std::error_code ec;
  std::filesystem::remove(hang, ec);
  return 0;
}
