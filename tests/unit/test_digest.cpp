// payloadDigest() walker tests (decision F15, session 73). The expected hashes
// are ExifTool's ImageDataHash (SHA256) for the committed fixtures, so these
// assertions double as an offline equivalence check; the live cross-check runs
// in the exiftool-gated backend test.
#include "umm/digest.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef UMM_FIXTURES_DIR
#error "UMM_FIXTURES_DIR must be defined by the build"
#endif

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

std::filesystem::path fixtures_dir() {
  const char* raw = UMM_FIXTURES_DIR;
  return std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t*>(raw)));
}

constexpr const char* kScheme = "exiftool-imagedatahash-sha256";

constexpr const char* kJpeg =
    "75346a2436de788b1f036704ac4d8096d1f8b2e3133f5c2d16aecc7c03d00308";
constexpr const char* kPng =
    "d5a4af38169edef216b69c8bd709b036cd062c5a09fed9e6783469408560f620";
constexpr const char* kWebp =
    "962ca9ef42d0bac5a24341cdfc1b7908de2f1adbae32b060d15079f442855525";
constexpr const char* kVideoBase =
    "965871df438722608e1fcdadab56da938fa28e23c5e9a369be728a8ef9f7ccff";
constexpr const char* kVideoTechnical =
    "3bebef6d4f2aa078523938b7ce6c11a96d8a22b6be3ec31e4f21e54536433d98";

bool expect(const std::filesystem::path& path, const char* expected) {
  umm::Result<umm::PayloadDigest> result = umm::payloadDigest(path);
  if (!result.ok()) {
    std::fprintf(stderr, "digest failed for %s: %s\n",
                 path.string().c_str(), result.error().message.c_str());
    return false;
  }
  const umm::PayloadDigest& digest = result.value();
  if (digest.scheme != kScheme) {
    std::fprintf(stderr, "unexpected scheme for %s: %s\n",
                 path.string().c_str(), digest.scheme.c_str());
    return false;
  }
  if (digest.hex != expected) {
    std::fprintf(stderr, "digest mismatch for %s\n  got %s\n  want %s\n",
                 path.string().c_str(), digest.hex.c_str(), expected);
    return false;
  }
  return true;
}

std::vector<unsigned char> readBytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void writeBytes(const std::filesystem::path& path,
                const std::vector<unsigned char>& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

}  // namespace

int main() {
  const std::filesystem::path root = fixtures_dir();

  // Every fixture of a type shares pixels/samples, so all share one digest.
  for (const auto& name : {"minimal.jpg", "gps.jpg", "xmp-only.jpg",
                           "windows-xp.jpg", "unknown-tags.jpg"}) {
    if (!expect(root / "jpeg" / name, kJpeg)) {
      return fail("jpeg digest");
    }
  }
  for (const auto& name :
       {"minimal.png", "gps.png", "text-chunks.png", "xmp-only.png"}) {
    if (!expect(root / "png" / name, kPng)) {
      return fail("png digest");
    }
  }
  for (const auto& name : {"minimal.webp", "xmp-only.webp"}) {
    if (!expect(root / "webp" / name, kWebp)) {
      return fail("webp digest");
    }
  }
  for (const auto& name : {"minimal.mp4", "minimal.mov", "full.mp4",
                           "user-data.mov", "xmp-shapes.mp4"}) {
    if (!expect(root / "video" / name, kVideoBase)) {
      return fail("video base digest");
    }
  }
  for (const auto& name : {"technical.mp4", "technical.mov",
                           "technical-rotated.mp4", "technical-rotated.mov"}) {
    if (!expect(root / "video" / name, kVideoTechnical)) {
      return fail("video technical digest");
    }
  }

  // Unsupported container type.
  {
    umm::Result<umm::PayloadDigest> result =
        umm::payloadDigest(root / "tiff" / "minimal.tif");
    if (result.ok() ||
        result.error().code != umm::ErrorCode::unsupported_capability) {
      return fail("tiff should be unsupported_capability");
    }
  }

  // Missing file.
  {
    umm::Result<umm::PayloadDigest> result =
        umm::payloadDigest(root / "jpeg" / "does-not-exist.jpg");
    if (result.ok() || result.error().code != umm::ErrorCode::io_not_found) {
      return fail("missing file should be io_not_found");
    }
  }

  const std::filesystem::path tmp =
      std::filesystem::temp_directory_path() / "umm_digest_test";
  std::filesystem::create_directories(tmp);

  // Flipping a pixel byte changes the digest.
  {
    std::vector<unsigned char> bytes = readBytes(root / "png" / "minimal.png");
    // Find first IDAT and flip a byte of its payload.
    std::size_t idat = 0;
    for (std::size_t i = 8; i + 8 <= bytes.size(); ) {
      const std::uint32_t len = (bytes[i] << 24) | (bytes[i + 1] << 16) |
                                (bytes[i + 2] << 8) | bytes[i + 3];
      if (bytes[i + 4] == 'I' && bytes[i + 5] == 'D' && bytes[i + 6] == 'A' &&
          bytes[i + 7] == 'T') {
        idat = i + 8;
        break;
      }
      i += 8 + len + 4;
    }
    if (idat == 0) {
      return fail("could not find IDAT in minimal.png");
    }
    bytes[idat] ^= 0xFF;
    const std::filesystem::path edited = tmp / "pixel-edit.png";
    writeBytes(edited, bytes);
    umm::Result<umm::PayloadDigest> result = umm::payloadDigest(edited);
    if (!result.ok() || result.value().hex == kPng) {
      return fail("pixel edit must change digest");
    }
  }

  // Editing a non-payload region (IHDR width) leaves the digest unchanged,
  // demonstrating metadata/structure independence without a backend.
  {
    std::vector<unsigned char> bytes = readBytes(root / "png" / "minimal.png");
    // IHDR data starts at offset 16 (8 sig + 4 len + 4 type); flip a width bit.
    bytes[16] ^= 0x01;
    const std::filesystem::path edited = tmp / "ihdr-edit.png";
    writeBytes(edited, bytes);
    umm::Result<umm::PayloadDigest> result = umm::payloadDigest(edited);
    if (!result.ok() || result.value().hex != kPng) {
      return fail("non-payload edit must not change digest");
    }
  }

  // Truncated JPEG scan data is a structural error.
  {
    std::vector<unsigned char> bytes = readBytes(root / "jpeg" / "minimal.jpg");
    bytes.resize(bytes.size() / 2);
    const std::filesystem::path edited = tmp / "truncated.jpg";
    writeBytes(edited, bytes);
    umm::Result<umm::PayloadDigest> result = umm::payloadDigest(edited);
    // A mid-scan truncation still hashes the available scan bytes; a truncation
    // inside a length-prefixed segment is format_corrupt. Accept either a valid
    // digest or format_corrupt, but never a crash or unsupported type.
    if (!result.ok() &&
        result.error().code != umm::ErrorCode::format_corrupt) {
      return fail("truncated jpeg must be ok-or-corrupt");
    }
  }

  std::error_code ec;
  std::filesystem::remove_all(tmp, ec);
  return 0;
}
