#include "read_raw_checks.hpp"
#include "umm/umm.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

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

int require_file(const std::filesystem::path& path, const char* label) {
  if (!std::filesystem::is_regular_file(path)) {
    std::fprintf(stderr, "missing Tier B sample %s\n", label);
    return 1;
  }
  return 0;
}

}  // namespace

int main() {
#ifndef UMM_TIER_B
  std::fprintf(stderr, "test_tier_b requires UMM_TIER_B\n");
  return 1;
#else
  maybe_configure_exiftool();

  const auto makernote = raw_jpeg("makernote.jpg");
  if (const int rc = require_file(makernote, "jpeg/makernote.jpg"); rc != 0) {
    return rc;
  }
  const auto jpeg_caps = umm::capabilities(makernote);
  if (!jpeg_caps.ok() || jpeg_caps.value().file_type != "JPEG") {
    return fail("makernote sample is not JPEG");
  }

  const auto raw = raw_corpus("raw/panasonic.rw2");
  if (const int rc = require_file(raw, "raw/panasonic.rw2"); rc != 0) {
    return rc;
  }
  const auto raw_caps = umm::capabilities(raw);
  if (!raw_caps.ok() || raw_caps.value().file_type != "RW2") {
    return fail("proprietary RAW sample is not RW2");
  }
  umm::WriteOptions preferred;
  preferred.policy = umm::StoragePolicy::preferred;
  const auto raw_storage = umm::evaluateStorage(raw, preferred);
  if (!raw_storage.ok() ||
      raw_storage.value().method != umm::StorageDecision::Method::sidecar) {
    return fail("RW2 preferred is not sidecar_recommended");
  }
  umm::ReadOptions read_opts;
  umm::Backend* exiftool = umm::BackendManager::instance().get("exiftool");
  if (exiftool && exiftool->availability().available) {
    read_opts.backend = "exiftool";
  }
  const auto raw_read = umm::read(raw, read_opts);
  if (!raw_read.ok()) {
    std::fprintf(stderr, "RW2 read failed: %s (%s)\n",
                 raw_read.error().message.c_str(),
                 raw_read.error().detail.c_str());
    return 1;
  }

  const auto mov = raw_corpus("video/camera.mov");
  if (const int rc = require_file(mov, "video/camera.mov"); rc != 0) {
    return rc;
  }
  const auto mov_caps = umm::capabilities(mov);
  if (!mov_caps.ok() || mov_caps.value().file_type != "MOV") {
    return fail("camera MOV sample is not MOV");
  }
  const auto mov_read = umm::read(mov, read_opts);
  if (!mov_read.ok()) {
    std::fprintf(stderr, "MOV read failed: %s (%s)\n",
                 mov_read.error().message.c_str(),
                 mov_read.error().detail.c_str());
    return 1;
  }

  const auto heic = raw_corpus("heic/quicktime.heic");
  if (const int rc = require_file(heic, "heic/quicktime.heic"); rc != 0) {
    return rc;
  }
  const auto heic_caps = umm::capabilities(heic);
  if (!heic_caps.ok() || heic_caps.value().file_type != "HEIC") {
    return fail("QuickTime.heic sample is not HEIC");
  }
  const auto heic_read = umm::read(heic, read_opts);
  if (!heic_read.ok()) {
    std::fprintf(stderr, "HEIC read failed: %s (%s)\n",
                 heic_read.error().message.c_str(),
                 heic_read.error().detail.c_str());
    return 1;
  }
  umm::Backend* exiv2 = umm::BackendManager::instance().get("exiv2");
  if (exiv2 && exiv2->availability().available) {
    umm::ReadOptions exiv2_opts;
    exiv2_opts.backend = "exiv2";
    const auto heic_exiv2 = umm::read(heic, exiv2_opts);
    if (!heic_exiv2.ok()) {
      std::fprintf(stderr, "HEIC Exiv2 read failed: %s (%s)\n",
                   heic_exiv2.error().message.c_str(),
                   heic_exiv2.error().detail.c_str());
      return 1;
    }
  }

  const auto cr3 = raw_corpus("raw/canon.cr3");
  if (const int rc = require_file(cr3, "raw/canon.cr3"); rc != 0) {
    return rc;
  }
  const auto cr3_caps = umm::capabilities(cr3);
  if (!cr3_caps.ok() || cr3_caps.value().file_type != "CR3") {
    return fail("CanonRaw.cr3 sample is not CR3");
  }
  const auto cr3_storage = umm::evaluateStorage(cr3, preferred);
  if (!cr3_storage.ok() ||
      cr3_storage.value().method != umm::StorageDecision::Method::sidecar) {
    return fail("CR3 preferred is not sidecar_recommended");
  }

  const auto jxl = raw_corpus("jxl/codestream.jxl");
  if (const int rc = require_file(jxl, "jxl/codestream.jxl"); rc != 0) {
    return rc;
  }
  const auto jxl_caps = umm::capabilities(jxl);
  if (!jxl_caps.ok() || jxl_caps.value().file_type != "JXL") {
    return fail("JXL2.jxl sample is not JXL");
  }
  return 0;
#endif
}
