#include "umm/backend.hpp"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#ifndef UMM_FIXTURES_DIR
#error "UMM_FIXTURES_DIR must be defined by the build"
#endif

#ifndef UMM_TEST_EXIV2_VERSION
#error "UMM_TEST_EXIV2_VERSION must be defined by the build"
#endif

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

std::string utf8(std::u8string_view text) {
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

std::filesystem::path fixtures_dir() {
  const char* raw = UMM_FIXTURES_DIR;
  return std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t*>(raw)));
}

std::filesystem::path jpeg(const char* name) {
  return fixtures_dir() / "jpeg" / name;
}

bool only_family(const umm::RawDocument& document, std::string_view family) {
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

bool has_family(const umm::RawDocument& document, std::string_view family) {
  for (const auto& entry : document.entries) {
    if (entry.key.family == family) {
      return true;
    }
  }
  return false;
}

std::optional<std::string> value_of(const umm::RawDocument& document,
                                    std::string_view key) {
  for (const auto& entry : document.entries) {
    if (entry.key.key == key) {
      return entry.value;
    }
  }
  return std::nullopt;
}

bool has_key_with_value(const umm::RawDocument& document, std::string_view key,
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

}  // namespace

int main() {
  umm::BackendManager& manager = umm::BackendManager::instance();
  const auto ids = manager.backendIds();
  if (ids.empty() || ids.front() != "exiv2") {
    return fail("BackendManager does not register exiv2 first");
  }
  umm::Backend* backend = manager.get("exiv2");
  if (!backend || backend != manager.firstAvailable()) {
    return fail("get(exiv2) / firstAvailable mismatch");
  }

  const umm::BackendAvailability status = backend->availability();
  if (!status.available) {
    std::fprintf(stderr, "exiv2 unavailable: %s\n", status.reason.c_str());
    return 1;
  }
  if (status.version != UMM_TEST_EXIV2_VERSION) {
    std::fprintf(stderr, "exiv2 version \"%s\", expected \"%s\"\n",
                 status.version.c_str(), UMM_TEST_EXIV2_VERSION);
    return 1;
  }

  const auto exif_only = backend->readRaw(jpeg("exif-only.jpg"));
  if (!exif_only.ok()) {
    std::fprintf(stderr, "exif-only read failed: %s\n",
                 exif_only.error().message.c_str());
    return 1;
  }
  if (!only_family(exif_only.value(), "Exif")) {
    return fail("exif-only.jpg produced non-Exif keys");
  }
  if (!has_key_with_value(exif_only.value(), "Exif.Image.Artist",
                          "EXIF Artist")) {
    return fail("exif-only.jpg missing Exif.Image.Artist");
  }

  const auto agreeing = backend->readRaw(jpeg("full-agreeing.jpg"));
  if (!agreeing.ok()) {
    std::fprintf(stderr, "full-agreeing read failed: %s\n",
                 agreeing.error().message.c_str());
    return 1;
  }
  if (!has_family(agreeing.value(), "Exif") ||
      !has_family(agreeing.value(), "Iptc") ||
      !has_family(agreeing.value(), "Xmp")) {
    return fail("full-agreeing.jpg missing a metadata family");
  }
  if (!has_key_with_value(agreeing.value(), "Exif.Image.Artist",
                          "Agreeing Creator") ||
      !has_key_with_value(agreeing.value(), "Iptc.Application2.Byline",
                          "Agreeing Creator") ||
      !has_key_with_value(agreeing.value(), "Xmp.dc.creator",
                          "Agreeing Creator")) {
    return fail("full-agreeing.jpg missing creator in all families");
  }
  if (!has_key_with_value(agreeing.value(), "Exif.Photo.DateTimeOriginal",
                          "2020:01:02 03:04:05") ||
      !value_of(agreeing.value(), "Iptc.Application2.DateCreated")) {
    return fail("full-agreeing.jpg missing expected dates");
  }

  const auto gps = backend->readRaw(jpeg("gps.jpg"));
  if (!gps.ok()) {
    std::fprintf(stderr, "gps read failed: %s\n", gps.error().message.c_str());
    return 1;
  }
  if (!value_of(gps.value(), "Exif.GPSInfo.GPSLatitude") ||
      !value_of(gps.value(), "Exif.GPSInfo.GPSLongitude")) {
    return fail("gps.jpg missing EXIF GPS coordinates");
  }

  const auto unicode = backend->readRaw(jpeg("unicode.jpg"));
  if (!unicode.ok()) {
    std::fprintf(stderr, "unicode read failed: %s\n",
                 unicode.error().message.c_str());
    return 1;
  }
  const std::string jurgen = utf8(u8"Jürgen Müller");
  const std::string cafe = utf8(u8"café — 日本語");
  if (!has_key_with_value(unicode.value(), "Iptc.Application2.Byline", jurgen) ||
      !has_key_with_value(unicode.value(), "Xmp.dc.creator", jurgen) ||
      !has_key_with_value(unicode.value(), "Iptc.Application2.Caption", cafe) ||
      !has_key_with_value(unicode.value(), "Xmp.dc.description", cafe)) {
    return fail("unicode.jpg values did not match UTF-8 expectations");
  }

  const auto truncated = backend->readRaw(jpeg("truncated.jpg"));
  if (truncated.ok()) {
    return fail("truncated.jpg unexpectedly succeeded");
  }
  const umm::ErrorCode code = truncated.error().code;
  if (code != umm::ErrorCode::format_corrupt &&
      code != umm::ErrorCode::format_unrecognized) {
    std::fprintf(stderr, "truncated.jpg wrong error code %d (%s / %s)\n",
                 static_cast<int>(code), truncated.error().message.c_str(),
                 truncated.error().detail.c_str());
    return 1;
  }
  if (truncated.error().backend != "exiv2") {
    return fail("truncated.jpg error missing backend id");
  }

  return 0;
}
