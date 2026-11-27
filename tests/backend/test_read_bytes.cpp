// Backend-gated in-memory read checks (session 75 exit criteria). For every
// representative Tier A fixture, read(bytes) must equal read(path) minus the
// sidecar, for every available backend. The ExifTool backend stages the buffer
// in a private temp file, so no umm-readbytes-* file may survive the call.
// ExifTool-only fixtures are skipped, not failed, when ExifTool is absent.
#include "umm/diff.hpp"
#include "umm/umm.hpp"

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef UMM_FIXTURES_DIR
#error "UMM_FIXTURES_DIR must be defined by the build"
#endif

namespace {

int fail(const std::string& message) {
  std::fprintf(stderr, "%s\n", message.c_str());
  return 1;
}

std::filesystem::path fixtures_dir() {
  const char* raw = UMM_FIXTURES_DIR;
  return std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t*>(raw)));
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

std::vector<std::byte> load_bytes(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  std::vector<std::byte> data;
  if (!in) {
    return data;
  }
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  in.seekg(0, std::ios::beg);
  if (size > 0) {
    data.resize(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
  }
  return data;
}

// Count leftover private temp files so the test proves cleanup (even on error).
std::size_t temp_leftovers() {
  std::error_code ec;
  const std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
  if (ec) {
    return 0;
  }
  std::size_t count = 0;
  for (std::filesystem::directory_iterator it(dir, ec), end;
       !ec && it != end; it.increment(ec)) {
    const std::string name = it->path().filename().string();
    if (name.rfind("umm-readbytes-", 0) == 0) {
      ++count;
    }
  }
  return count;
}

struct Fixture {
  const char* path;       // relative to the fixtures dir
  const char* type_hint;  // canonical file type hint for the byte read
};

// One backend's path/byte equality check for a single fixture.
int check(const std::string& backend_id, const Fixture& fixture) {
  const std::filesystem::path file = fixtures_dir() / fixture.path;

  umm::ReadOptions options;
  options.backend = backend_id;
  options.merge_sidecar = false;  // bytes read has no sidecar to pair

  umm::Result<umm::Metadata> from_path = umm::read(file, options);
  if (!from_path.ok()) {
    // This backend does not support this fixture (e.g. Exiv2 on MP4/MOV) or is
    // unavailable; there is nothing to compare against, so skip the pair.
    return -1;
  }

  const std::vector<std::byte> bytes = load_bytes(file);
  if (bytes.empty()) {
    return fail(std::string("could not load bytes for ") + fixture.path);
  }

  umm::Result<umm::Metadata> from_bytes =
      umm::read(bytes, options, fixture.type_hint);
  if (!from_bytes.ok()) {
    return fail(std::string("byte read failed for ") + fixture.path + ": " +
                from_bytes.error().message);
  }

  umm::DiffOptions diff_options;
  diff_options.base_entries = true;
  const umm::MetadataDiff delta =
      umm::diff(from_path.value(), from_bytes.value(), diff_options);
  if (!delta.empty()) {
    return fail(std::string("byte read differs from path read for ") +
                fixture.path + " (" + backend_id + ")");
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();

  // Representative Tier A corpus across still, RAW, container, and sidecar
  // types; the hint disambiguates TIFF-based DNG and ISO-BMFF video.
  const Fixture fixtures[] = {
      {"jpeg/full-agreeing.jpg", "JPEG"},
      {"jpeg/gps.jpg", "JPEG"},
      {"jpeg/unicode.jpg", "JPEG"},
      {"tiff/full-agreeing.tif", "TIFF"},
      {"tiff/gps.tif", "TIFF"},
      {"png/full-agreeing.png", "PNG"},
      {"png/xmp-only.png", "PNG"},
      {"webp/full-agreeing.webp", "WEBP"},
      {"raw/full-agreeing.dng", "DNG"},
      {"avif/full-agreeing.avif", "AVIF"},
      {"video/full.mp4", "MP4"},
      {"video/technical.mov", "MOV"},
      {"sidecar/orphan.xmp", "XMP"},
  };

  const std::string backends[] = {"exiv2", "exiftool"};
  bool any_checked = false;
  for (const std::string& backend_id : backends) {
    for (const Fixture& fixture : fixtures) {
      const int status = check(backend_id, fixture);
      if (status > 0) {
        return status;
      }
      if (status == 0) {
        any_checked = true;
      }
    }
  }

  if (!any_checked) {
    std::fprintf(stderr, "no backend available for byte-read checks\n");
    return 1;
  }

  if (const std::size_t leftovers = temp_leftovers(); leftovers != 0) {
    return fail("private temp files from byte reads were not removed");
  }

  std::puts("read-bytes backend checks passed");
  return 0;
}
