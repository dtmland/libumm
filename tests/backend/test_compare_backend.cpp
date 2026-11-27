// Backend-gated compareFiles() checks (session 74 exit criteria). After a
// libumm metadata write on a digest-supported type the payload digest is
// unchanged, so compareFiles() reports metadata_only. An essence-byte change
// reports payload_only (or both with a metadata edit), and an unsupported
// digest type reports payload_unknown. ExifTool-only steps are skipped, not
// failed, when ExifTool is absent.
#include "umm/diff.hpp"
#include "umm/umm.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

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

// Flip one byte deep in the JPEG entropy stream (three bytes before EOI). The
// byte stays inside a scan, so the container still parses but the hashed bytes
// differ. 0x00 after a 0xFF is valid stuffing, so this never forges a marker.
bool mutate_essence_byte(const std::filesystem::path& file) {
  std::fstream stream(file, std::ios::in | std::ios::out | std::ios::binary);
  if (!stream) {
    return false;
  }
  stream.seekg(0, std::ios::end);
  const std::streamoff size = stream.tellg();
  if (size < 8) {
    return false;
  }
  const std::streamoff pos = size - 3;
  stream.seekg(pos);
  char current = 0;
  stream.read(&current, 1);
  const char replacement = current == 0 ? 0x11 : 0x00;
  stream.seekp(pos);
  stream.write(&replacement, 1);
  return static_cast<bool>(stream);
}

bool write_title(const std::filesystem::path& file, const char* text) {
  umm::Metadata metadata;
  if (!metadata
           .set("iptc.photo.title",
                umm::Value{umm::LangAlt{{"x-default", text}}})
           .ok()) {
    return false;
  }
  umm::Result<umm::WriteReport> written = umm::write(file, metadata);
  return written.ok();
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  const std::filesystem::path root = fixtures_dir();
  const std::filesystem::path tmp =
      std::filesystem::temp_directory_path() / "umm_compare_backend";
  std::error_code ec;
  std::filesystem::remove_all(tmp, ec);
  std::filesystem::create_directories(tmp);

  const std::filesystem::path original = root / "jpeg" / "minimal.jpg";

  // identical: a copy compared to itself.
  {
    const std::filesystem::path a = tmp / "identical-a.jpg";
    const std::filesystem::path b = tmp / "identical-b.jpg";
    std::filesystem::copy_file(
        original, a, std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::copy_file(
        original, b, std::filesystem::copy_options::overwrite_existing, ec);
    umm::Result<umm::FileComparison> cmp = umm::compareFiles(a, b);
    if (!cmp.ok()) {
      return fail("identical compareFiles failed");
    }
    if (cmp.value().kind != umm::ComparisonKind::identical) {
      return fail("expected identical");
    }
  }

  // metadata_only: a libumm metadata write leaves the payload digest intact.
  {
    const std::filesystem::path a = tmp / "meta-a.jpg";
    const std::filesystem::path b = tmp / "meta-b.jpg";
    std::filesystem::copy_file(
        original, a, std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::copy_file(
        original, b, std::filesystem::copy_options::overwrite_existing, ec);
    if (!write_title(b, "compare metadata_only")) {
      return fail("metadata write failed");
    }
    umm::Result<umm::FileComparison> cmp = umm::compareFiles(a, b);
    if (!cmp.ok()) {
      return fail("metadata_only compareFiles failed");
    }
    const umm::FileComparison& value = cmp.value();
    if (value.kind != umm::ComparisonKind::metadata_only) {
      std::fprintf(stderr, "kind=%d\n", static_cast<int>(value.kind));
      return fail("expected metadata_only after libumm write");
    }
    if (value.metadata.empty()) {
      return fail("metadata_only should carry a non-empty diff");
    }
  }

  // payload_only: an essence byte change with identical metadata.
  {
    const std::filesystem::path a = tmp / "payload-a.jpg";
    const std::filesystem::path b = tmp / "payload-b.jpg";
    std::filesystem::copy_file(
        original, a, std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::copy_file(
        original, b, std::filesystem::copy_options::overwrite_existing, ec);
    if (!mutate_essence_byte(b)) {
      return fail("essence mutation failed");
    }
    umm::Result<umm::FileComparison> cmp = umm::compareFiles(a, b);
    if (!cmp.ok()) {
      return fail("payload_only compareFiles failed");
    }
    if (cmp.value().kind != umm::ComparisonKind::payload_only) {
      return fail("expected payload_only on essence change");
    }
  }

  // both: metadata edit and essence change together.
  {
    const std::filesystem::path a = tmp / "both-a.jpg";
    const std::filesystem::path b = tmp / "both-b.jpg";
    std::filesystem::copy_file(
        original, a, std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::copy_file(
        original, b, std::filesystem::copy_options::overwrite_existing, ec);
    if (!write_title(b, "compare both")) {
      return fail("both metadata write failed");
    }
    if (!mutate_essence_byte(b)) {
      return fail("both essence mutation failed");
    }
    umm::Result<umm::FileComparison> cmp = umm::compareFiles(a, b);
    if (!cmp.ok()) {
      return fail("both compareFiles failed");
    }
    if (cmp.value().kind != umm::ComparisonKind::both) {
      return fail("expected both on metadata+essence change");
    }
  }

  // payload_unknown: a type with no payload digest (TIFF).
  {
    const std::filesystem::path src = root / "tiff" / "minimal.tif";
    const std::filesystem::path a = tmp / "unknown-a.tif";
    const std::filesystem::path b = tmp / "unknown-b.tif";
    std::filesystem::copy_file(
        src, a, std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::copy_file(
        src, b, std::filesystem::copy_options::overwrite_existing, ec);
    umm::Result<umm::FileComparison> cmp = umm::compareFiles(a, b);
    if (!cmp.ok()) {
      return fail("payload_unknown compareFiles failed");
    }
    const umm::FileComparison& value = cmp.value();
    if (value.kind != umm::ComparisonKind::payload_unknown) {
      return fail("expected payload_unknown for TIFF");
    }
    if (value.a.has_value() || value.b.has_value()) {
      return fail("payload_unknown should carry no digests for TIFF");
    }
  }

  std::puts("test_compare_backend ok");
  return 0;
}
