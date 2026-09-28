#include "read_raw_checks.hpp"
#include "umm/umm.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
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

const umm::BackendCapability* find_backend(const umm::Capabilities& caps,
                                           std::string_view id) {
  for (const umm::BackendCapability& row : caps.backends) {
    if (row.backend == id) {
      return &row;
    }
  }
  return nullptr;
}

int probe_backend(const std::string& backend_id) {
  umm::Backend* backend = umm::BackendManager::instance().get(backend_id);
  if (!backend || !backend->availability().available) {
    return 0;
  }

  const auto caps = umm::capabilitiesForType("JPEG");
  if (!caps.ok()) {
    return fail("probe JPEG caps");
  }
  const umm::BackendCapability* row = find_backend(caps.value(), backend_id);
  if (!row || row->categories.exif != umm::Access::read_write ||
      row->categories.iptc_iim != umm::Access::read_write ||
      row->categories.xmp != umm::Access::read_write ||
      row->location.gps_exif != umm::Access::read_write ||
      row->location.named_place != umm::Access::read_write) {
    return fail("JPEG capability data mismatch");
  }

  const auto type_ok = backend->typeCapabilities("JPEG");
  if (!type_ok.ok()) {
    return fail("typeCapabilities JPEG");
  }
  const auto xmp_ok = backend->typeCapabilities("XMP");
  if (!xmp_ok.ok()) {
    return fail("typeCapabilities XMP");
  }

  const auto sniffed = umm::capabilities(raw_jpeg("gps.jpg"));
  if (!sniffed.ok() || sniffed.value().file_type != "JPEG") {
    return fail("sniff gps.jpg");
  }

  const auto exif_only = backend->readRaw(raw_jpeg("exif-only.jpg"));
  if (!exif_only.ok() || !raw_has_family(exif_only.value(), "Exif")) {
    return fail("exif-only fixture vs EXIF capability");
  }
  const auto iptc_only = backend->readRaw(raw_jpeg("iptc-only.jpg"));
  if (!iptc_only.ok() || !raw_has_family(iptc_only.value(), "Iptc")) {
    return fail("iptc-only fixture vs IPTC capability");
  }
  const auto xmp_only = backend->readRaw(raw_jpeg("xmp-only.jpg"));
  if (!xmp_only.ok() || !raw_has_family(xmp_only.value(), "Xmp")) {
    return fail("xmp-only fixture vs XMP capability");
  }
  const auto gps = backend->readRaw(raw_jpeg("gps.jpg"));
  if (!gps.ok()) {
    return fail("gps fixture");
  }
  bool has_gps = false;
  bool has_place = false;
  for (const umm::RawEntry& entry : gps.value().entries) {
    if (entry.key.key.find("GPSLatitude") != std::string::npos) {
      has_gps = true;
    }
    if (entry.key.key.find("City") != std::string::npos) {
      has_place = true;
    }
  }
  if (!has_gps || !has_place) {
    return fail("gps fixture vs GPS/named-place split");
  }

  const auto sidecar = backend->readRaw(raw_sidecar("orphan.xmp"));
  if (!sidecar.ok()) {
    return fail("orphan sidecar");
  }
  const auto sidecar_caps = umm::capabilities(raw_sidecar("orphan.xmp"));
  if (!sidecar_caps.ok() || sidecar_caps.value().file_type != "XMP") {
    return fail("sniff orphan.xmp");
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  if (int rc = probe_backend("exiv2")) {
    return rc;
  }
  if (int rc = probe_backend("exiftool")) {
    return rc;
  }
  return 0;
}
