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

  const auto tiff_caps = umm::capabilitiesForType("TIFF");
  if (!tiff_caps.ok()) {
    return fail("probe TIFF caps");
  }
  const umm::BackendCapability* tiff_row =
      find_backend(tiff_caps.value(), backend_id);
  if (!tiff_row || tiff_row->categories.exif != umm::Access::read_write ||
      tiff_row->categories.iptc_iim != umm::Access::read_write ||
      tiff_row->categories.xmp != umm::Access::read_write ||
      tiff_row->location.gps_exif != umm::Access::read_write ||
      tiff_row->location.named_place != umm::Access::read_write) {
    return fail("TIFF capability data mismatch");
  }
  const auto tiff_ok = backend->typeCapabilities("TIFF");
  if (!tiff_ok.ok()) {
    return fail("typeCapabilities TIFF");
  }
  const auto sniffed_tiff = umm::capabilities(raw_tiff("gps.tif"));
  if (!sniffed_tiff.ok() || sniffed_tiff.value().file_type != "TIFF") {
    return fail("sniff gps.tif");
  }
  const auto tiff_exif = backend->readRaw(raw_tiff("exif-only.tif"));
  if (!tiff_exif.ok() || !raw_has_family(tiff_exif.value(), "Exif")) {
    return fail("tiff exif-only fixture vs EXIF capability");
  }
  const auto tiff_iptc = backend->readRaw(raw_tiff("iptc-only.tif"));
  if (!tiff_iptc.ok() || !raw_has_family(tiff_iptc.value(), "Iptc")) {
    return fail("tiff iptc-only fixture vs IPTC capability");
  }
  const auto tiff_xmp = backend->readRaw(raw_tiff("xmp-only.tif"));
  if (!tiff_xmp.ok() || !raw_has_family(tiff_xmp.value(), "Xmp")) {
    return fail("tiff xmp-only fixture vs XMP capability");
  }
  const auto tiff_gps = backend->readRaw(raw_tiff("gps.tif"));
  if (!tiff_gps.ok()) {
    return fail("tiff gps fixture");
  }
  bool tiff_has_gps = false;
  bool tiff_has_place = false;
  for (const umm::RawEntry& entry : tiff_gps.value().entries) {
    if (entry.key.key.find("GPSLatitude") != std::string::npos) {
      tiff_has_gps = true;
    }
    if (entry.key.key.find("City") != std::string::npos) {
      tiff_has_place = true;
    }
  }
  if (!tiff_has_gps || !tiff_has_place) {
    return fail("tiff gps fixture vs GPS/named-place split");
  }

  const auto png_caps = umm::capabilitiesForType("PNG");
  if (!png_caps.ok()) {
    return fail("probe PNG caps");
  }
  const umm::BackendCapability* png_row =
      find_backend(png_caps.value(), backend_id);
  if (!png_row || png_row->categories.xmp != umm::Access::read_write ||
      png_row->categories.iptc_iim != umm::Access::read_write) {
    return fail("PNG XMP/IPTC capability data mismatch");
  }
  if (backend_id == "exiv2") {
    if (png_row->categories.exif != umm::Access::none ||
        png_row->location.gps_exif != umm::Access::none) {
      return fail("PNG Exiv2 should report no EXIF GPS");
    }
  } else if (png_row->categories.exif != umm::Access::read_write ||
             png_row->location.gps_exif != umm::Access::read_write) {
    return fail("PNG ExifTool should report EXIF GPS");
  }
  const auto png_ok = backend->typeCapabilities("PNG");
  if (!png_ok.ok()) {
    return fail("typeCapabilities PNG");
  }
  const auto sniffed_png = umm::capabilities(raw_stem("png", "gps", ".png"));
  if (!sniffed_png.ok() || sniffed_png.value().file_type != "PNG") {
    return fail("sniff gps.png");
  }
  const auto png_xmp = backend->readRaw(raw_stem("png", "xmp-only", ".png"));
  if (!png_xmp.ok() || !raw_has_family(png_xmp.value(), "Xmp")) {
    return fail("png xmp-only fixture vs XMP capability");
  }
  const auto png_agree =
      backend->readRaw(raw_stem("png", "full-agreeing", ".png"));
  if (!png_agree.ok() || !raw_has_family(png_agree.value(), "Xmp") ||
      !raw_has_family(png_agree.value(), "Iptc")) {
    return fail("png full-agreeing fixture vs IPTC/XMP capability");
  }
  const auto png_gps = backend->readRaw(raw_stem("png", "gps", ".png"));
  if (!png_gps.ok()) {
    return fail("png gps fixture");
  }
  bool png_has_gps = false;
  bool png_has_exif_gps = false;
  for (const umm::RawEntry& entry : png_gps.value().entries) {
    if (entry.key.key.find("GPSLatitude") != std::string::npos) {
      png_has_gps = true;
    }
    if (entry.key.key.find("Exif.GPSInfo.GPSLatitude") != std::string::npos) {
      png_has_exif_gps = true;
    }
  }
  if (!png_has_gps) {
    return fail("png gps fixture missing GPSLatitude");
  }
  if (backend_id == "exiftool" && !png_has_exif_gps) {
    return fail("png gps ExifTool missing EXIF GPS");
  }

  const auto webp_caps = umm::capabilitiesForType("WEBP");
  if (!webp_caps.ok()) {
    return fail("probe WEBP caps");
  }
  const umm::BackendCapability* webp_row =
      find_backend(webp_caps.value(), backend_id);
  if (!webp_row || webp_row->categories.exif != umm::Access::read_write ||
      webp_row->categories.xmp != umm::Access::read_write ||
      webp_row->categories.iptc_iim != umm::Access::none) {
    return fail("WEBP capability data mismatch");
  }
  const auto webp_ok = backend->typeCapabilities("WEBP");
  if (!webp_ok.ok()) {
    return fail("typeCapabilities WEBP");
  }
  const auto sniffed_webp =
      umm::capabilities(raw_stem("webp", "full-agreeing", ".webp"));
  if (!sniffed_webp.ok() || sniffed_webp.value().file_type != "WEBP") {
    return fail("sniff full-agreeing.webp");
  }
  const auto webp_xmp =
      backend->readRaw(raw_stem("webp", "xmp-only", ".webp"));
  if (!webp_xmp.ok() || !raw_has_family(webp_xmp.value(), "Xmp")) {
    return fail("webp xmp-only fixture vs XMP capability");
  }
  const auto webp_agree =
      backend->readRaw(raw_stem("webp", "full-agreeing", ".webp"));
  if (!webp_agree.ok() || !raw_has_family(webp_agree.value(), "Exif") ||
      !raw_has_family(webp_agree.value(), "Xmp") ||
      raw_has_family(webp_agree.value(), "Iptc")) {
    return fail("webp full-agreeing fixture vs EXIF/XMP capability");
  }

  const auto dng_caps = umm::capabilitiesForType("DNG");
  if (!dng_caps.ok()) {
    return fail("probe DNG caps");
  }
  const umm::BackendCapability* dng_row =
      find_backend(dng_caps.value(), backend_id);
  if (!dng_row || dng_row->categories.exif != umm::Access::read_write ||
      dng_row->categories.iptc_iim != umm::Access::read_write ||
      dng_row->categories.xmp != umm::Access::read_write ||
      dng_row->location.gps_exif != umm::Access::read_write ||
      dng_row->location.named_place != umm::Access::read_write) {
    return fail("DNG capability data mismatch");
  }
  const auto dng_ok = backend->typeCapabilities("DNG");
  if (!dng_ok.ok()) {
    return fail("typeCapabilities DNG");
  }
  const auto sniffed_dng =
      umm::capabilities(raw_stem("raw", "full-agreeing", ".dng"));
  if (!sniffed_dng.ok() || sniffed_dng.value().file_type != "DNG") {
    return fail("sniff full-agreeing.dng");
  }
  const auto dng_agree =
      backend->readRaw(raw_stem("raw", "full-agreeing", ".dng"));
  if (!dng_agree.ok() || !raw_has_family(dng_agree.value(), "Exif") ||
      !raw_has_family(dng_agree.value(), "Iptc") ||
      !raw_has_family(dng_agree.value(), "Xmp")) {
    return fail("dng full-agreeing fixture vs EXIF/IPTC/XMP capability");
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
