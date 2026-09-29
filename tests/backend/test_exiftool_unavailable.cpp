#include "umm/backend.hpp"

#include <cstdio>
#include <filesystem>

int main() {
  umm::BackendManager& manager = umm::BackendManager::instance();
  umm::ExifToolConfig config;
  config.exiftool_script =
      std::filesystem::path("umm-missing-exiftool-script");
  config.perl_interpreter =
      std::filesystem::path("umm-missing-perl-interpreter");
  manager.configureExifTool(config);

  umm::Backend* backend = manager.get("exiftool");
  if (!backend) {
    std::fprintf(stderr, "exiftool backend was not registered\n");
    return 1;
  }
  const umm::BackendAvailability status = backend->availability();
  if (status.available) {
    std::fprintf(stderr, "missing ExifTool reported available\n");
    return 1;
  }
  if (status.reason.empty()) {
    std::fprintf(stderr, "unavailable ExifTool missing reason\n");
    return 1;
  }

  const auto read = backend->readUnmapped(std::filesystem::path("no-such.jpg"));
  if (read.ok()) {
    std::fprintf(stderr, "readUnmapped succeeded without ExifTool\n");
    return 1;
  }
  if (read.error().code != umm::ErrorCode::backend_unavailable) {
    std::fprintf(stderr, "unexpected error %d (%s)\n",
                 static_cast<int>(read.error().code),
                 read.error().message.c_str());
    return 1;
  }
  if (read.error().backend != "exiftool") {
    std::fprintf(stderr, "unavailable error missing backend id\n");
    return 1;
  }
  return 0;
}
