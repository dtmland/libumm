#include "exiftool/exiftool_backend.hpp"
#include "read_raw_checks.hpp"
#include "umm/backend.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <system_error>

#ifndef UMM_TEST_EXIFTOOL_SCRIPT
#error "UMM_TEST_EXIFTOOL_SCRIPT must be defined by the build"
#endif
#ifndef UMM_TEST_EXIFTOOL_PERL
#error "UMM_TEST_EXIFTOOL_PERL must be defined by the build"
#endif
#ifndef UMM_TEST_EXIFTOOL_VERSION
#error "UMM_TEST_EXIFTOOL_VERSION must be defined by the build"
#endif

namespace {

umm::internal::ExifToolBackend* as_exiftool(umm::Backend* backend) {
  return static_cast<umm::internal::ExifToolBackend*>(backend);
}

int write_hang_script(const std::filesystem::path& path) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return raw_fail("failed to write hang script");
  }
  out << "sleep 60;\n";
  return out ? 0 : raw_fail("failed to flush hang script");
}

}  // namespace

int main() {
  umm::BackendManager& manager = umm::BackendManager::instance();
  umm::ExifToolConfig config;
  config.exiftool_script = std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_SCRIPT)));
  config.perl_interpreter = std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_PERL)));
  manager.configureExifTool(config);

  umm::Backend* backend = manager.get("exiftool");
  if (!backend) {
    return raw_fail("BackendManager does not register exiftool");
  }

  const umm::BackendAvailability status = backend->availability();
  if (!status.available) {
    std::fprintf(stderr, "exiftool unavailable: %s\n", status.reason.c_str());
    return 1;
  }
  if (status.version != UMM_TEST_EXIFTOOL_VERSION) {
    std::fprintf(stderr, "exiftool version \"%s\", expected \"%s\"\n",
                 status.version.c_str(), UMM_TEST_EXIFTOOL_VERSION);
    return 1;
  }

  if (const int rc = check_jpeg_raw_reads(*backend, "exiftool"); rc != 0) {
    return rc;
  }
  if (const int rc = check_tiff_raw_reads(*backend, "exiftool"); rc != 0) {
    return rc;
  }

  auto* adapter = as_exiftool(backend);
  const std::uint64_t after_fixtures = adapter->spawnCount();
  if (after_fixtures == 0) {
    return raw_fail("expected ExifTool process to spawn during fixture reads");
  }
  for (int i = 0; i < 100; ++i) {
    const auto read = backend->readRaw(raw_jpeg("exif-only.jpg"));
    if (!read.ok()) {
      std::fprintf(stderr, "reuse read %d failed: %s\n", i,
                   read.error().message.c_str());
      return 1;
    }
  }
  if (adapter->spawnCount() != after_fixtures) {
    std::fprintf(stderr, "100 sequential reads spawned extra processes (%llu -> %llu)\n",
                 static_cast<unsigned long long>(after_fixtures),
                 static_cast<unsigned long long>(adapter->spawnCount()));
    return 1;
  }

  adapter->killChildForTest();
  const auto after_kill = backend->readRaw(raw_jpeg("exif-only.jpg"));
  if (!after_kill.ok()) {
    std::fprintf(stderr, "read after kill failed: %s\n",
                 after_kill.error().message.c_str());
    return 1;
  }
  if (adapter->spawnCount() != after_fixtures + 1) {
    return raw_fail("ExifTool did not restart after the child was killed");
  }

  const std::filesystem::path hang =
      std::filesystem::temp_directory_path() / "umm-exiftool-hang.pl";
  if (const int rc = write_hang_script(hang); rc != 0) {
    return rc;
  }
  umm::ExifToolConfig timeout_config = config;
  timeout_config.exiftool_script = hang;
  timeout_config.command_timeout = std::chrono::milliseconds{1500};
  manager.configureExifTool(timeout_config);
  const auto timed_out = backend->readRaw(raw_jpeg("exif-only.jpg"));
  std::error_code ec;
  std::filesystem::remove(hang, ec);
  if (timed_out.ok()) {
    return raw_fail("hanging ExifTool unexpectedly succeeded");
  }
  if (timed_out.error().code != umm::ErrorCode::backend_timeout) {
    std::fprintf(stderr, "expected backend_timeout, got %d (%s / %s)\n",
                 static_cast<int>(timed_out.error().code),
                 timed_out.error().message.c_str(),
                 timed_out.error().detail.c_str());
    return 1;
  }
  return 0;
}
