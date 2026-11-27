#include "umm/umm.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef UMM_FIXTURES_DIR
#error "UMM_FIXTURES_DIR must be defined by the build"
#endif

namespace {

std::filesystem::path fixtures_dir() {
  const char* raw = UMM_FIXTURES_DIR;
  return std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t*>(raw)));
}

std::filesystem::path corpus_heic() {
#ifdef UMM_CORPUS_DIR
  const char* raw = UMM_CORPUS_DIR;
  return std::filesystem::path(
             std::u8string(reinterpret_cast<const char8_t*>(raw))) /
         "heic" / "quicktime.heic";
#else
  return {};
#endif
}

void maybe_configure_exiftool() {
#ifdef UMM_TEST_EXIFTOOL_SCRIPT
  umm::ExifToolConfig config;
  config.exiftool_script = std::filesystem::path(std::u8string(
      reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_SCRIPT)));
  config.perl_interpreter = std::filesystem::path(std::u8string(
      reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_PERL)));
  umm::BackendManager::instance().configureExifTool(config);
#endif
}

std::vector<std::filesystem::path> copies(const std::filesystem::path& source,
                                          int count) {
  const auto dir = std::filesystem::temp_directory_path() / "umm-read-bench";
  std::filesystem::create_directories(dir);
  std::vector<std::filesystem::path> paths;
  for (int i = 0; i < count; ++i) {
    auto dest = dir / (std::to_string(i) + "-" + source.filename().string());
    std::filesystem::copy_file(source, dest,
                               std::filesystem::copy_options::overwrite_existing);
    paths.push_back(std::move(dest));
  }
  return paths;
}

double ms_since(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now() - start)
      .count();
}

void time_backend(const char* backend, const char* kind,
                  const std::filesystem::path& source) {
  if (!std::filesystem::is_regular_file(source)) {
    std::printf("%-10s %-8s %-22s skipped (missing)\n", backend, kind,
                source.filename().string().c_str());
    return;
  }
  umm::Backend* adapter = umm::BackendManager::instance().get(backend);
  if (!adapter || !adapter->availability().available) {
    std::printf("%-10s %-8s %-22s skipped (backend)\n", backend, kind,
                source.filename().string().c_str());
    return;
  }
  constexpr int kCount = 8;
  const auto paths = copies(source, kCount);
  umm::ReadOptions full;
  full.backend = backend;
  umm::ReadOptions selective = full;
  if (std::string(kind) == "video") {
    selective.properties = std::vector<std::string>{
        "iptc.video.dateCreated", "iptc.video.locationShot"};
  } else {
    selective.properties = std::vector<std::string>{
        "iptc.photo.dateCreated", "iptc.photo.locationCreated"};
  }

  auto start = std::chrono::steady_clock::now();
  for (const auto& path : paths) {
    if (!umm::read(path, full).ok()) {
      std::printf("%-10s %-8s %-22s full read failed\n", backend, kind,
                  source.filename().string().c_str());
      return;
    }
  }
  const double full_ms = ms_since(start);

  start = std::chrono::steady_clock::now();
  for (const auto& path : paths) {
    if (!umm::read(path, selective).ok()) {
      std::printf("%-10s %-8s %-22s selective read failed\n", backend, kind,
                  source.filename().string().c_str());
      return;
    }
  }
  const double selective_ms = ms_since(start);

  start = std::chrono::steady_clock::now();
  const auto many = umm::readMany(
      paths, full,
      [](const std::filesystem::path&, umm::Result<umm::Metadata> result) {
        if (!result.ok()) {
          throw std::runtime_error(result.error().message);
        }
      });
  if (!many.ok()) {
    std::printf("%-10s %-8s %-22s readMany failed: %s\n", backend, kind,
                source.filename().string().c_str(),
                many.error().message.c_str());
    return;
  }
  const double many_ms = ms_since(start);

  std::printf("%-10s %-8s %-22s %8.1f %8.1f %8.1f\n", backend, kind,
              source.filename().string().c_str(), full_ms, selective_ms,
              many_ms);
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  std::printf(
      "backend    kind     file                      full_ms  sel_ms  many_ms\n");
  std::printf("N=8 copies; times are totals, not per file. Batch size %zu.\n",
              umm::kExifToolReadBatch);
  const auto jpeg = fixtures_dir() / "jpeg" / "exif-only.jpg";
  const auto mp4 = fixtures_dir() / "video" / "gps.mp4";
  const auto heic = corpus_heic();
  for (const char* backend : {"exiftool", "exiv2"}) {
    time_backend(backend, "photo", jpeg);
    time_backend(backend, "video", mp4);
    if (!heic.empty()) {
      time_backend(backend, "photo", heic);
    }
  }
  return 0;
}
