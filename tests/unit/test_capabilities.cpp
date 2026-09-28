#include "umm/umm.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
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

}  // namespace

int main() {
  const auto jpeg = umm::capabilitiesForType("JPEG");
  if (!jpeg.ok()) {
    return fail("JPEG capabilities");
  }
  if (jpeg.value().file_type != "JPEG" ||
      jpeg.value().preferred_backend != "exiv2" ||
      jpeg.value().sidecar_recommended) {
    return fail("JPEG policy");
  }
  const umm::BackendCapability* exiv2 = find_backend(jpeg.value(), "exiv2");
  const umm::BackendCapability* exiftool = find_backend(jpeg.value(), "exiftool");
  if (!exiv2 || !exiftool) {
    return fail("JPEG backends");
  }
  if (exiv2->identify_only || exiv2->categories.exif != umm::Access::read_write ||
      exiv2->categories.iptc_iim != umm::Access::read_write ||
      exiv2->categories.xmp != umm::Access::read_write ||
      exiv2->categories.icc != umm::Access::read_write ||
      exiv2->categories.thumbnail != umm::Access::read_write) {
    return fail("JPEG Exiv2 categories");
  }
  if (exiv2->location.gps_exif != umm::Access::read_write ||
      exiv2->location.named_place != umm::Access::read_write ||
      exiv2->location.xmp_location != umm::Access::read_write ||
      exiv2->location.container_gps != umm::Access::none) {
    return fail("JPEG GPS vs named place");
  }
  if (exiftool->location.gps_exif != umm::Access::read_write ||
      exiftool->location.named_place != umm::Access::read_write) {
    return fail("JPEG ExifTool location");
  }

  const auto lower = umm::capabilitiesForType("jpeg");
  if (!lower.ok() || lower.value().file_type != "JPEG") {
    return fail("case-insensitive type");
  }

  const auto xmp = umm::capabilitiesForType("XMP");
  if (!xmp.ok() || xmp.value().preferred_backend != "exiv2") {
    return fail("XMP policy");
  }
  const umm::BackendCapability* xmp_exiv2 = find_backend(xmp.value(), "exiv2");
  const umm::BackendCapability* xmp_et = find_backend(xmp.value(), "exiftool");
  if (!xmp_exiv2 || xmp_exiv2->location.gps_exif != umm::Access::none ||
      xmp_exiv2->location.named_place != umm::Access::read_write ||
      xmp_exiv2->categories.xmp != umm::Access::read_write) {
    return fail("XMP Exiv2 location split");
  }
  if (!xmp_et || xmp_et->categories.xmp != umm::Access::create ||
      xmp_et->location.named_place != umm::Access::create) {
    return fail("XMP ExifTool create");
  }

  const auto png = umm::capabilitiesForType("PNG");
  if (!png.ok()) {
    return fail("PNG capabilities");
  }
  const umm::BackendCapability* png_exiv2 = find_backend(png.value(), "exiv2");
  const umm::BackendCapability* png_et = find_backend(png.value(), "exiftool");
  if (!png_exiv2 || png_exiv2->location.gps_exif != umm::Access::none ||
      png_exiv2->location.named_place != umm::Access::read_write) {
    return fail("PNG Exiv2 has no EXIF GPS");
  }
  if (!png_et || png_et->location.gps_exif != umm::Access::read_write) {
    return fail("PNG ExifTool EXIF GPS");
  }

  const auto cr3 = umm::capabilitiesForType("CR3");
  if (!cr3.ok() || cr3.value().preferred_backend != "exiftool" ||
      !cr3.value().sidecar_recommended) {
    return fail("CR3 preferred ExifTool");
  }
  const umm::BackendCapability* cr3_exiv2 = find_backend(cr3.value(), "exiv2");
  if (!cr3_exiv2 || cr3_exiv2->categories.exif != umm::Access::read ||
      cr3_exiv2->notes.find("BMFF") == std::string::npos) {
    return fail("CR3 BMFF note");
  }

  const auto by_path = umm::capabilities(std::filesystem::path("photo.jpg"));
  if (!by_path.ok() || by_path.value().file_type != "JPEG") {
    return fail("path extension JPEG");
  }
  const auto by_xmp = umm::capabilities(std::filesystem::path("photo.xmp"));
  if (!by_xmp.ok() || by_xmp.value().file_type != "XMP") {
    return fail("path extension XMP");
  }

  const auto unknown = umm::capabilitiesForType("NO_SUCH_TYPE");
  if (unknown.ok() || unknown.error().code != umm::ErrorCode::unsupported_type) {
    return fail("unknown type");
  }
  const auto empty = umm::capabilities(std::filesystem::path{});
  if (empty.ok() || empty.error().code != umm::ErrorCode::io_not_found) {
    return fail("empty path");
  }

  umm::Backend* exiv2_backend = umm::BackendManager::instance().get("exiv2");
  if (exiv2_backend && exiv2_backend->availability().available) {
    const auto ok = exiv2_backend->typeCapabilities("JPEG");
    if (!ok.ok()) {
      return fail("Exiv2 typeCapabilities JPEG");
    }
  }
  return 0;
}
