#pragma once

#include "umm/backend.hpp"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#ifndef UMM_FIXTURES_DIR
#error "UMM_FIXTURES_DIR must be defined by the build"
#endif

inline int raw_fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

inline std::string raw_from_u8(const char8_t* text) {
  return std::string(reinterpret_cast<const char*>(text));
}

inline std::filesystem::path raw_fixtures_dir() {
  const char* raw = UMM_FIXTURES_DIR;
  return std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t*>(raw)));
}

inline std::filesystem::path raw_jpeg(const char* name) {
  return raw_fixtures_dir() / "jpeg" / name;
}

inline std::filesystem::path raw_sidecar(const char* name) {
  return raw_fixtures_dir() / "sidecar" / name;
}

inline std::filesystem::path raw_unicode_filename() {
  static constexpr char8_t kName[] = {
      0xC3, 0xBC, 'b', 0xC3, 0xBC, 'n', 'g', ' ',
      0xC3, 0xBC, 'n', 0xC3, 0xAF, 'c', 'o', 'd', 'e',
      '.', 'j', 'p', 'g', 0};
  return raw_fixtures_dir() / "naming" /
         std::filesystem::path(std::u8string(kName));
}

inline bool raw_only_family(const umm::RawDocument& document,
                            std::string_view family) {
  if (document.entries.empty()) {
    return false;
  }
  for (const auto& entry : document.entries) {
    if (entry.key.family != family) {
      return false;
    }
  }
  return true;
}

inline bool raw_has_family(const umm::RawDocument& document,
                           std::string_view family) {
  for (const auto& entry : document.entries) {
    if (entry.key.family == family) {
      return true;
    }
  }
  return false;
}

inline std::optional<std::string> raw_value_of(const umm::RawDocument& document,
                                               std::string_view key) {
  for (const auto& entry : document.entries) {
    if (entry.key.key == key) {
      return entry.value;
    }
  }
  return std::nullopt;
}

inline bool raw_has_key_with_value(const umm::RawDocument& document,
                                   std::string_view key,
                                   std::string_view expected) {
  for (const auto& entry : document.entries) {
    if (entry.key.key == key ||
        entry.key.key.rfind(std::string(key) + "[", 0) == 0) {
      if (entry.value.find(std::string(expected)) != std::string::npos) {
        return true;
      }
    }
  }
  return false;
}

// Shared JPEG fixture expectations for Exiv2 and ExifTool readRaw (session 11).
inline int check_jpeg_raw_reads(umm::Backend& backend,
                                const char* backend_id) {
  const auto exif_only = backend.readRaw(raw_jpeg("exif-only.jpg"));
  if (!exif_only.ok()) {
    std::fprintf(stderr, "exif-only read failed: %s\n",
                 exif_only.error().message.c_str());
    return 1;
  }
  if (!raw_only_family(exif_only.value(), "Exif")) {
    return raw_fail("exif-only.jpg produced non-Exif keys");
  }
  if (!raw_has_key_with_value(exif_only.value(), "Exif.Image.Artist",
                              "EXIF Artist")) {
    return raw_fail("exif-only.jpg missing Exif.Image.Artist");
  }

  const auto agreeing = backend.readRaw(raw_jpeg("full-agreeing.jpg"));
  if (!agreeing.ok()) {
    std::fprintf(stderr, "full-agreeing read failed: %s\n",
                 agreeing.error().message.c_str());
    return 1;
  }
  if (!raw_has_family(agreeing.value(), "Exif") ||
      !raw_has_family(agreeing.value(), "Iptc") ||
      !raw_has_family(agreeing.value(), "Xmp")) {
    return raw_fail("full-agreeing.jpg missing a metadata family");
  }
  if (!raw_has_key_with_value(agreeing.value(), "Exif.Image.Artist",
                              "Agreeing Creator") ||
      !raw_has_key_with_value(agreeing.value(), "Iptc.Application2.Byline",
                              "Agreeing Creator") ||
      !raw_has_key_with_value(agreeing.value(), "Xmp.dc.creator",
                              "Agreeing Creator")) {
    return raw_fail("full-agreeing.jpg missing creator in all families");
  }
  if (!raw_has_key_with_value(agreeing.value(), "Exif.Photo.DateTimeOriginal",
                              "2020:01:02 03:04:05") ||
      !raw_value_of(agreeing.value(), "Iptc.Application2.DateCreated")) {
    return raw_fail("full-agreeing.jpg missing expected dates");
  }

  const auto gps = backend.readRaw(raw_jpeg("gps.jpg"));
  if (!gps.ok()) {
    std::fprintf(stderr, "gps read failed: %s\n", gps.error().message.c_str());
    return 1;
  }
  if (!raw_value_of(gps.value(), "Exif.GPSInfo.GPSLatitude") ||
      !raw_value_of(gps.value(), "Exif.GPSInfo.GPSLongitude")) {
    return raw_fail("gps.jpg missing EXIF GPS coordinates");
  }

  const auto unicode = backend.readRaw(raw_jpeg("unicode.jpg"));
  if (!unicode.ok()) {
    std::fprintf(stderr, "unicode read failed: %s\n",
                 unicode.error().message.c_str());
    return 1;
  }
  static constexpr char8_t kJurgen[] = {
      'J', 0xC3, 0xBC, 'r', 'g', 'e', 'n', ' ', 'M', 0xC3, 0xBC, 'l', 'l',
      'e', 'r', 0};
  static constexpr char8_t kCafe[] = {
      'c', 'a', 'f', 0xC3, 0xA9, ' ', 0xE2, 0x80, 0x94, ' ', 0xE6, 0x97,
      0xA5, 0xE6, 0x9C, 0xAC, 0xE8, 0xAA, 0x9E, 0};
  const std::string jurgen = raw_from_u8(kJurgen);
  const std::string cafe = raw_from_u8(kCafe);
  if (!raw_has_key_with_value(unicode.value(), "Iptc.Application2.Byline",
                              jurgen) ||
      !raw_has_key_with_value(unicode.value(), "Xmp.dc.creator", jurgen) ||
      !raw_has_key_with_value(unicode.value(), "Iptc.Application2.Caption",
                              cafe) ||
      !raw_has_key_with_value(unicode.value(), "Xmp.dc.description", cafe)) {
    return raw_fail("unicode.jpg values did not match UTF-8 expectations");
  }

  const auto unicode_path = backend.readRaw(raw_unicode_filename());
  if (!unicode_path.ok()) {
    std::fprintf(stderr, "unicode filename read failed: %s\n",
                 unicode_path.error().message.c_str());
    return 1;
  }
  if (!raw_has_key_with_value(unicode_path.value(), "Xmp.dc.creator", jurgen)) {
    return raw_fail("unicode filename fixture did not match UTF-8 creator");
  }

  const auto truncated =
      backend.readRaw(raw_fixtures_dir() / "corrupt" / "truncated.jpg");
  if (truncated.ok()) {
    return raw_fail("truncated.jpg unexpectedly succeeded");
  }
  const umm::ErrorCode code = truncated.error().code;
  if (code != umm::ErrorCode::format_corrupt &&
      code != umm::ErrorCode::format_unrecognized) {
    std::fprintf(stderr, "truncated.jpg wrong error code %d (%s / %s)\n",
                 static_cast<int>(code), truncated.error().message.c_str(),
                 truncated.error().detail.c_str());
    return 1;
  }
  if (truncated.error().backend != backend_id) {
    return raw_fail("truncated.jpg error missing backend id");
  }
  return 0;
}
