#include "umm/umm.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

bool formats_are(const umm::StorageDecision& decision,
                 const std::vector<std::string>& expected) {
  return decision.formats == expected;
}

}  // namespace

int main() {
  const std::filesystem::path jpeg{"photo.jpg"};
  const std::filesystem::path jpeg_upper{"PHOTO.JPEG"};
  const std::filesystem::path sidecar{"photo.xmp"};

  if (umm::isXmpSidecarPath(jpeg) || !umm::isXmpSidecarPath(sidecar) ||
      !umm::isXmpSidecarPath(std::filesystem::path("photo.XMP"))) {
    return fail("isXmpSidecarPath");
  }
  if (umm::sidecarPath(jpeg) != std::filesystem::path("photo.xmp")) {
    return fail("sidecarPath jpeg");
  }
  if (umm::sidecarPath(sidecar) != sidecar) {
    return fail("sidecarPath xmp");
  }
  if (umm::findSidecar(jpeg) || umm::findSidecar(sidecar)) {
    return fail("findSidecar missing pair");
  }

  umm::WriteOptions preferred;
  preferred.policy = umm::StoragePolicy::preferred;
  const auto pref = umm::evaluateStorage(jpeg, preferred);
  if (!pref.ok() || pref.value().method != umm::StorageDecision::Method::embedded ||
      !formats_are(pref.value(), {"XMP", "EXIF", "IPTC-IIM"})) {
    return fail("JPEG preferred");
  }

  umm::WriteOptions embedded;
  embedded.policy = umm::StoragePolicy::embedded_only;
  const auto emb = umm::evaluateStorage(jpeg_upper, embedded);
  if (!emb.ok() || emb.value().method != umm::StorageDecision::Method::embedded ||
      !formats_are(emb.value(), {"XMP", "EXIF", "IPTC-IIM"})) {
    return fail("JPEG embedded_only");
  }

  umm::WriteOptions sidecar_only;
  sidecar_only.policy = umm::StoragePolicy::sidecar_only;
  const auto only = umm::evaluateStorage(jpeg, sidecar_only);
  if (!only.ok() || only.value().method != umm::StorageDecision::Method::sidecar ||
      !formats_are(only.value(), {"XMP"})) {
    return fail("JPEG sidecar_only");
  }

  umm::WriteOptions required;
  required.policy = umm::StoragePolicy::sidecar_required;
  const auto req = umm::evaluateStorage(jpeg, required);
  if (!req.ok() || req.value().method != umm::StorageDecision::Method::sidecar ||
      !formats_are(req.value(), {"XMP"})) {
    return fail("JPEG sidecar_required");
  }

  const auto xmp_pref = umm::evaluateStorage(sidecar, preferred);
  if (!xmp_pref.ok() ||
      xmp_pref.value().method != umm::StorageDecision::Method::sidecar) {
    return fail("XMP preferred is sidecar");
  }
  const auto xmp_embedded = umm::evaluateStorage(sidecar, embedded);
  if (xmp_embedded.ok() ||
      xmp_embedded.error().code != umm::ErrorCode::unsupported_capability) {
    return fail("XMP embedded_only");
  }

  const auto tiff =
      umm::evaluateStorage(std::filesystem::path("a.tiff"), preferred);
  if (!tiff.ok() ||
      tiff.value().method != umm::StorageDecision::Method::embedded ||
      !formats_are(tiff.value(), {"XMP", "EXIF", "IPTC-IIM"})) {
    return fail("TIFF embedded-capable");
  }

  const auto arw = umm::evaluateStorage(std::filesystem::path("a.arw"), preferred);
  if (!arw.ok() || arw.value().method != umm::StorageDecision::Method::sidecar ||
      !formats_are(arw.value(), {"XMP"})) {
    return fail("ARW sidecar_recommended");
  }

  const auto bmp = umm::evaluateStorage(std::filesystem::path("a.bmp"), preferred);
  if (bmp.ok() || bmp.error().code != umm::ErrorCode::unsupported_capability) {
    return fail("BMP no write capability");
  }
  const auto bmp_embedded =
      umm::evaluateStorage(std::filesystem::path("a.bmp"), embedded);
  if (bmp_embedded.ok() ||
      bmp_embedded.error().code != umm::ErrorCode::unsupported_capability) {
    return fail("BMP embedded_only");
  }

  const umm::BackendManager& manager = umm::BackendManager::instance();
  const umm::Backend* listed = manager.get("exiv2");
  if (!listed || listed->id() != "exiv2") {
    return fail("const BackendManager::get");
  }
  return 0;
}
