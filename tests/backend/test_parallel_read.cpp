#include "umm/umm.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

int fail(const char* message) {
  std::cerr << message << '\n';
  return 1;
}

std::string dump_text(const umm::Metadata& metadata) {
  std::string text;
  for (const umm::BaseEntry& entry : metadata.dumpAll()) {
    text += entry.key.family;
    text += '\t';
    text += entry.key.key;
    text += '\t';
    text += entry.value;
    text += '\n';
  }
  for (const std::string& id : metadata.propertyIds()) {
    const auto property = metadata.get(id);
    text += id;
    text += '=';
    text += property ? property->value.toString() : "";
    text += '\n';
  }
  return text;
}

int read_parallel(const std::filesystem::path& media, const std::string& backend,
                  int threads) {
  umm::ReadOptions options;
  options.backend = backend;
  options.merge_sidecar = true;
  const auto first = umm::read(media, options);
  if (!first.ok()) {
    std::cerr << first.error().message << '\n';
    return fail("baseline read failed");
  }
  const std::string expected = dump_text(first.value());
  std::vector<std::string> errors(static_cast<std::size_t>(threads));
  std::vector<std::thread> workers;
  const auto started = std::chrono::steady_clock::now();
  for (int i = 0; i < threads; ++i) {
    workers.emplace_back([&, i] {
      const auto read = umm::read(media, options);
      if (!read.ok()) {
        errors[static_cast<std::size_t>(i)] = read.error().message;
        return;
      }
      if (dump_text(read.value()) != expected) {
        errors[static_cast<std::size_t>(i)] = "parallel read diverged";
      }
    });
  }
  for (std::thread& worker : workers) {
    worker.join();
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);
  std::cerr << "parallel " << backend << " x" << threads << " " << elapsed.count()
            << " ms\n";
  if (elapsed > std::chrono::seconds{60}) {
    return fail("parallel read exceeded 60s");
  }
  for (const std::string& error : errors) {
    if (!error.empty()) {
      std::cerr << error << '\n';
      return fail("parallel read failed");
    }
  }
  return 0;
}

}  // namespace

int main() {
  const std::filesystem::path root{UMM_FIXTURES_DIR};
  const std::filesystem::path jpeg = root / "jpeg/minimal.jpg";
  const std::filesystem::path mp4 = root / "video/minimal.mp4";
  constexpr int kThreads = 8;

  if (const int rc = read_parallel(jpeg, "exiv2", kThreads); rc != 0) {
    return rc;
  }
  umm::WriteOptions dry;
  dry.dry_run = true;
  umm::Metadata metadata;
  umm::Value headline;
  headline.data = std::string{"umm"};
  (void)metadata.set("iptc.photo.headline", headline);
  const auto preview = umm::write(jpeg, metadata, dry);
  if (!preview.ok()) {
    return fail("dry-run write failed");
  }
  const auto caps = umm::capabilities(jpeg);
  if (!caps.ok()) {
    return fail("capabilities failed");
  }

#ifdef UMM_TEST_EXIFTOOL_SCRIPT
  umm::ExifToolConfig config;
  config.exiftool_script = std::filesystem::path(std::u8string(
      reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_SCRIPT)));
  config.perl_interpreter = std::filesystem::path(std::u8string(
      reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_PERL)));
  config.max_processes = 1;
  umm::BackendManager::instance().configureExifTool(config);
  if (const int rc = read_parallel(jpeg, "exiftool", kThreads); rc != 0) {
    return rc;
  }
  if (const int rc = read_parallel(mp4, "exiftool", kThreads); rc != 0) {
    return rc;
  }
  const auto started = std::chrono::steady_clock::now();
  config.max_processes = 4;
  umm::BackendManager::instance().configureExifTool(config);
  if (const int rc = read_parallel(mp4, "exiftool", kThreads); rc != 0) {
    return rc;
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);
  std::cerr << "exiftool max_processes=4 suite " << elapsed.count() << " ms\n";
#endif
  return 0;
}
