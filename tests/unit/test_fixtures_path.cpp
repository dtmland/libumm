#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#ifndef UMM_FIXTURES_DIR
#error "UMM_FIXTURES_DIR must be defined by the build"
#endif

namespace {

std::filesystem::path fixtures_dir() {
  const char* raw = UMM_FIXTURES_DIR;
  return std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t*>(raw)));
}

std::filesystem::path unicode_name() {
  // UTF-8 for: übüng ünïcode.jpg
  static constexpr char8_t kName[] = {
      0xC3, 0xBC, 'b', 0xC3, 0xBC, 'n', 'g', ' ',
      0xC3, 0xBC, 'n', 0xC3, 0xAF, 'c', 'o', 'd', 'e',
      '.', 'j', 'p', 'g', 0};
  return std::filesystem::path(std::u8string(kName));
}

bool file_nonempty(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  char byte = 0;
  return static_cast<bool>(in.read(&byte, 1));
}

}  // namespace

int main() {
  const std::filesystem::path root = fixtures_dir();
  if (!std::filesystem::is_directory(root)) {
    std::fprintf(stderr, "UMM_FIXTURES_DIR is not a directory: %s\n",
                 UMM_FIXTURES_DIR);
    return 1;
  }

  const std::filesystem::path minimal = root / "jpeg" / "minimal.jpg";
  if (!file_nonempty(minimal)) {
    std::fprintf(stderr, "missing jpeg/minimal.jpg under fixture dir\n");
    return 1;
  }

  const std::filesystem::path unicode = root / "naming" / unicode_name();
  if (!file_nonempty(unicode)) {
    std::fprintf(stderr, "missing unicode filename fixture under fixture dir\n");
    return 1;
  }

  const std::filesystem::path tiff = root / "tiff" / "minimal.tif";
  if (!file_nonempty(tiff)) {
    std::fprintf(stderr, "missing tiff/minimal.tif under fixture dir\n");
    return 1;
  }

  return 0;
}
