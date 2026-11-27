#include "read_base_checks.hpp"
#include "umm/umm.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int fail(const char* message) { return raw_fail(message); }

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

bool backend_available(const char* id) {
  umm::Backend* backend = umm::BackendManager::instance().get(id);
  return backend && backend->availability().available;
}

bool dump_has_intrinsic(const umm::Metadata& meta) {
  for (const umm::BaseEntry& entry : meta.dumpAll()) {
    if (entry.key.key == "PixelWidth" || entry.key.key == "PixelHeight") {
      return true;
    }
  }
  return false;
}

int expect_pixels(const umm::Metadata& meta, const char* label,
                  const char* source_key) {
  const auto& size = meta.technical().pixelSize;
  if (!size || size->value.width != 16 || size->value.height != 16) {
    std::fprintf(stderr, "%s pixelSize missing or not 16x16\n", label);
    return 1;
  }
  if (size->source.key != source_key) {
    std::fprintf(stderr, "%s pixel source %s\n", label,
                 size->source.key.c_str());
    return 1;
  }
  if (dump_has_intrinsic(meta)) {
    return fail("intrinsics leaked into dumpAll");
  }
  return 0;
}

int expect_text(const std::optional<umm::TechnicalValue<std::string>>& value,
                const char* label, const char* source_key,
                const char* contains) {
  if (!value || value->value.empty() || value->source.key != source_key ||
      value->value.find(contains) == std::string::npos) {
    std::fprintf(stderr, "%s technical text\n", label);
    if (value) {
      std::fprintf(stderr, "  value=%s source=%s\n", value->value.c_str(),
                   value->source.key.c_str());
    }
    return 1;
  }
  return 0;
}

int check_photo(const char* backend) {
  umm::ReadOptions options;
  options.backend = backend;
  const auto jpeg = umm::read(raw_jpeg("iphone-heic-layout.jpg"), options);
  if (!jpeg.ok()) {
    std::fprintf(stderr, "%s jpeg technical read: %s\n", backend,
                 jpeg.error().message.c_str());
    return 1;
  }
  const umm::TechnicalInfo& tech = jpeg.value().technical();
  if (expect_text(tech.cameraMake, "jpeg make", "Exif.Image.Make", "Apple") !=
          0 ||
      expect_text(tech.cameraModel, "jpeg model", "Exif.Image.Model",
                  "iPhone") != 0 ||
      expect_text(tech.lensModel, "jpeg lens", "Exif.Photo.LensModel", "") !=
          0 ||
      expect_pixels(jpeg.value(), "jpeg", "PixelWidth") != 0) {
    return 1;
  }
  if (tech.orientation &&
      (tech.orientation->source.key != "Exif.Image.Orientation" ||
       tech.orientation->value.exif < 1 || tech.orientation->value.exif > 8)) {
    return fail("jpeg orientation source");
  }

  const auto dng = umm::read(raw_stem("raw", "minimal", ".dng"), options);
  if (!dng.ok()) {
    std::fprintf(stderr, "%s dng technical read: %s\n", backend,
                 dng.error().message.c_str());
    return 1;
  }
  const char* dng_source = std::string(backend) == "exiftool"
                               ? "Exif.Image.ImageWidth"
                               : "PixelWidth";
  if (expect_pixels(dng.value(), "dng", dng_source) != 0) {
    return 1;
  }
  if (dng.value().technical().cameraMake ||
      dng.value().technical().cameraModel ||
      dng.value().technical().lensModel) {
    return fail("minimal dng invented a camera field");
  }
  return 0;
}

int check_video(const char* name, bool rotated) {
  umm::ReadOptions options;
  options.backend = "exiftool";
  const auto read = umm::read(raw_stem("video", name, ""), options);
  if (!read.ok()) {
    std::fprintf(stderr, "video technical %s: %s\n", name,
                 read.error().message.c_str());
    return 1;
  }
  if (expect_pixels(read.value(), name, "ExifTool.Track1.ImageWidth") != 0) {
    return 1;
  }
  const auto& orientation = read.value().technical().orientation;
  if (rotated) {
    if (!orientation || orientation->value.exif != 6 ||
        std::fabs(orientation->value.degrees - 90.0) > 1e-3 ||
        orientation->value.mirror) {
      std::fprintf(stderr, "%s orientation\n", name);
      return 1;
    }
    if (orientation->source.key != "ExifTool.Composite.Rotation" &&
        orientation->source.key.find("MatrixStructure") == std::string::npos) {
      std::fprintf(stderr, "%s orientation source %s\n", name,
                   orientation->source.key.c_str());
      return 1;
    }
  } else if (orientation &&
             (orientation->value.exif != 1 ||
              std::fabs(orientation->value.degrees) > 1e-3)) {
    return fail("non-rotated video orientation");
  }
  return 0;
}

int check_phone_video() {
  umm::ReadOptions options;
  options.backend = "exiftool";
  const auto read = umm::read(raw_stem("video", "iphone-style.mov", ""), options);
  if (!read.ok()) {
    std::fprintf(stderr, "iphone-style technical: %s\n",
                 read.error().message.c_str());
    return 1;
  }
  const umm::TechnicalInfo& tech = read.value().technical();
  if (expect_text(tech.cameraMake, "mov make", "QuickTime.Keys.Make", "") !=
          0 ||
      expect_text(tech.cameraModel, "mov model", "QuickTime.Keys.Model", "") !=
          0) {
    return 1;
  }
  if (tech.lensModel) {
    return fail("iphone-style mov invented a lens");
  }
  return 0;
}

int check_exiv2_video_gap() {
  if (!backend_available("exiv2")) {
    return 0;
  }
  umm::ReadOptions options;
  options.backend = "exiv2";
  const auto read = umm::read(raw_stem("video", "technical.mp4", ""), options);
  if (!read.ok()) {
    if (read.error().code == umm::ErrorCode::unsupported_type ||
        read.error().code == umm::ErrorCode::format_unrecognized) {
      return 0;
    }
    std::fprintf(stderr, "exiv2 video technical: %s\n",
                 read.error().message.c_str());
    return 1;
  }
  if (read.value().technical().pixelSize ||
      read.value().technical().orientation ||
      read.value().technical().cameraMake) {
    return fail("bundled Exiv2 reported video technical values");
  }
  return 0;
}

int check_selective() {
  if (!backend_available("exiftool")) {
    return 0;
  }
  const auto path = raw_jpeg("iphone-heic-layout.jpg");
  umm::ReadOptions make_only;
  make_only.backend = "exiftool";
  make_only.properties = std::vector<std::string>{"technical.cameraMake"};
  const auto make = umm::read(path, make_only);
  if (!make.ok()) {
    return fail("selective cameraMake read");
  }
  if (!make.value().technical().cameraMake ||
      make.value().technical().cameraModel ||
      make.value().technical().pixelSize ||
      make.value().technical().orientation) {
    return fail("selective cameraMake filled other fields");
  }

  umm::ReadOptions size_only;
  size_only.backend = "exiftool";
  size_only.properties = std::vector<std::string>{"technical.pixelSize"};
  const auto size = umm::read(path, size_only);
  if (!size.ok() || expect_pixels(size.value(), "selective size", "PixelWidth") != 0) {
    return 1;
  }
  if (size.value().technical().cameraMake ||
      size.value().technical().lensModel) {
    return fail("selective pixelSize filled camera fields");
  }

  umm::ReadOptions date_only;
  date_only.backend = "exiftool";
  date_only.properties = std::vector<std::string>{"iptc.photo.dateCreated"};
  const auto date = umm::read(path, date_only);
  if (!date.ok()) {
    return fail("selective date read");
  }
  const umm::TechnicalInfo& tech = date.value().technical();
  if (tech.cameraMake || tech.cameraModel || tech.lensModel ||
      tech.orientation || tech.pixelSize) {
    return fail("unrelated selective read filled technical");
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  if (!backend_available("exiftool") && !backend_available("exiv2")) {
    return fail("no backend available");
  }
  if (backend_available("exiftool") &&
      (check_photo("exiftool") != 0 || check_video("technical.mp4", false) != 0 ||
       check_video("technical.mov", false) != 0 ||
       check_video("technical-rotated.mp4", true) != 0 ||
       check_video("technical-rotated.mov", true) != 0 ||
       check_phone_video() != 0 || check_selective() != 0)) {
    return 1;
  }
  if (backend_available("exiv2") &&
      (check_photo("exiv2") != 0 || check_exiv2_video_gap() != 0)) {
    return 1;
  }
  return 0;
}
