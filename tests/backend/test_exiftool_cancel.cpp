#include "exiftool/exiftool_backend.hpp"
#include "umm/umm.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>

namespace {

int fail(const char* message) {
  std::cerr << message << '\n';
  return 1;
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}

}  // namespace

int main() {
  const std::filesystem::path hang =
      std::filesystem::temp_directory_path() / "umm-exiftool-cancel-hang.pl";
  {
    std::ofstream out(hang, std::ios::binary | std::ios::trunc);
    out << "sleep 30;\n";
    if (!out) {
      return fail("failed to write hang script");
    }
  }
  const std::filesystem::path fixture =
      std::filesystem::path(UMM_FIXTURES_DIR) / "jpeg/minimal.jpg";
  const std::filesystem::path media =
      std::filesystem::temp_directory_path() / "umm-exiftool-cancel.jpg";
  std::filesystem::copy_file(fixture, media,
                             std::filesystem::copy_options::overwrite_existing);
  const auto original = read_bytes(media);

  umm::ExifToolConfig hang_config;
  hang_config.exiftool_script = hang;
  hang_config.perl_interpreter = std::filesystem::path(std::u8string(
      reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_PERL)));
  hang_config.command_timeout = std::chrono::milliseconds{30000};
  hang_config.max_processes = 1;
  umm::BackendManager::instance().configureExifTool(hang_config);
  auto* backend = static_cast<umm::internal::ExifToolBackend*>(
      umm::BackendManager::instance().get("exiftool"));
  if (!backend) {
    return fail("exiftool backend missing");
  }

  umm::CancelToken token = umm::CancelToken::create();
  umm::Error error;
  std::thread worker([&] {
    umm::ReadOptions options;
    options.backend = "exiftool";
    options.merge_sidecar = false;
    options.cancel = token;
    const auto started = std::chrono::steady_clock::now();
    const auto read = umm::read(media, options);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    if (read.ok()) {
      error = umm::Error{umm::ErrorCode::internal, "hang read succeeded", "", ""};
      return;
    }
    error = read.error();
    if (elapsed > std::chrono::milliseconds{500}) {
      error.message = "cancel slower than 500 ms";
    }
  });
  const auto live_deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds{2};
  while (backend->liveProcessCount() < 1 &&
         std::chrono::steady_clock::now() < live_deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  }
  token.cancel();
  worker.join();
  if (error.code != umm::ErrorCode::cancelled) {
    std::cerr << error.message << '\n';
    return fail("in-flight read did not cancel");
  }
  if (backend->liveProcessCount() != 0) {
    return fail("cancelled child was not reaped");
  }

  umm::Metadata metadata;
  umm::Value headline;
  headline.data = std::string{"umm"};
  (void)metadata.set("iptc.photo.headline", headline);
  umm::CancelToken write_token = umm::CancelToken::create();
  std::thread writer([&] {
    umm::WriteOptions options;
    options.backend = "exiftool";
    options.cancel = write_token;
    const auto written = umm::write(media, metadata, options);
    if (written.ok()) {
      error = umm::Error{umm::ErrorCode::internal, "hang write succeeded", "", ""};
      return;
    }
    error = written.error();
  });
  const auto write_live =
      std::chrono::steady_clock::now() + std::chrono::seconds{2};
  while (backend->liveProcessCount() < 1 &&
         std::chrono::steady_clock::now() < write_live) {
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  }
  write_token.cancel();
  writer.join();
  if (error.code != umm::ErrorCode::cancelled) {
    std::cerr << error.message << '\n';
    return fail("in-flight write did not cancel");
  }
  if (read_bytes(media) != original) {
    return fail("cancelled write changed the original file");
  }

  umm::ExifToolConfig real;
  real.exiftool_script = std::filesystem::path(std::u8string(
      reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_SCRIPT)));
  real.perl_interpreter = hang_config.perl_interpreter;
  umm::BackendManager::instance().configureExifTool(real);
  umm::ReadOptions options;
  options.backend = "exiftool";
  options.merge_sidecar = false;
  const auto read = umm::read(media, options);
  if (!read.ok()) {
    std::cerr << read.error().message << '\n';
    return fail("read after cancel failed");
  }
  std::error_code ec;
  std::filesystem::remove(hang, ec);
  std::filesystem::remove(media, ec);
  return 0;
}
