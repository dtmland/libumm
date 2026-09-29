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

  const auto png =
      umm::evaluateStorage(std::filesystem::path("a.png"), preferred);
  if (!png.ok() || png.value().method != umm::StorageDecision::Method::embedded ||
      !formats_are(png.value(), {"XMP", "IPTC-IIM"})) {
    return fail("PNG Exiv2 preferred has no EXIF");
  }
  umm::WriteOptions png_et;
  png_et.policy = umm::StoragePolicy::preferred;
  png_et.backend = "exiftool";
  const auto png_exiftool =
      umm::evaluateStorage(std::filesystem::path("a.png"), png_et);
  if (!png_exiftool.ok() ||
      png_exiftool.value().method != umm::StorageDecision::Method::embedded ||
      !formats_are(png_exiftool.value(), {"XMP", "EXIF", "IPTC-IIM"})) {
    return fail("PNG ExifTool includes EXIF");
  }

  const auto webp =
      umm::evaluateStorage(std::filesystem::path("a.webp"), preferred);
  if (!webp.ok() ||
      webp.value().method != umm::StorageDecision::Method::embedded ||
      !formats_are(webp.value(), {"XMP", "EXIF"})) {
    return fail("WEBP Exiv2 preferred has no IPTC");
  }

  const auto dng =
      umm::evaluateStorage(std::filesystem::path("a.dng"), preferred);
  if (!dng.ok() || dng.value().method != umm::StorageDecision::Method::embedded ||
      !formats_are(dng.value(), {"XMP", "EXIF", "IPTC-IIM"})) {
    return fail("DNG embedded-capable");
  }

  const auto arw = umm::evaluateStorage(std::filesystem::path("a.arw"), preferred);
  if (!arw.ok() || arw.value().method != umm::StorageDecision::Method::sidecar ||
      !formats_are(arw.value(), {"XMP"})) {
    return fail("ARW sidecar_recommended");
  }

  const auto raf = umm::evaluateStorage(std::filesystem::path("a.raf"), preferred);
  if (!raf.ok() || raf.value().method != umm::StorageDecision::Method::sidecar ||
      !formats_are(raf.value(), {"XMP"})) {
    return fail("RAF preferred sidecar_recommended");
  }
  umm::WriteOptions raf_embedded;
  raf_embedded.policy = umm::StoragePolicy::embedded_only;
  raf_embedded.backend = "exiv2";
  const auto raf_emb =
      umm::evaluateStorage(std::filesystem::path("a.raf"), raf_embedded);
  if (raf_emb.ok() ||
      raf_emb.error().code != umm::ErrorCode::unsupported_capability) {
    return fail("RAF Exiv2 embedded_only");
  }
  const auto rw2 = umm::evaluateStorage(std::filesystem::path("a.rw2"), preferred);
  const auto sr2 = umm::evaluateStorage(std::filesystem::path("a.sr2"), preferred);
  if (!rw2.ok() || rw2.value().method != umm::StorageDecision::Method::sidecar ||
      !sr2.ok() || sr2.value().method != umm::StorageDecision::Method::sidecar) {
    return fail("RW2/SR2 preferred sidecar_recommended");
  }

  umm::WriteOptions mp4_et;
  mp4_et.policy = umm::StoragePolicy::preferred;
  mp4_et.backend = "exiftool";
  const auto mp4 =
      umm::evaluateStorage(std::filesystem::path("a.mp4"), mp4_et);
  if (!mp4.ok() || mp4.value().method != umm::StorageDecision::Method::embedded ||
      mp4.value().backend != "exiftool" ||
      !formats_are(mp4.value(), {"XMP", "QuickTime"})) {
    return fail("MP4 ExifTool preferred XMP+QuickTime");
  }
  const auto mov =
      umm::evaluateStorage(std::filesystem::path("a.mov"), mp4_et);
  if (!mov.ok() || mov.value().method != umm::StorageDecision::Method::embedded ||
      mov.value().backend != "exiftool" ||
      !formats_are(mov.value(), {"XMP", "QuickTime"})) {
    return fail("MOV ExifTool preferred XMP+QuickTime");
  }
  const umm::Backend* exiftool = umm::BackendManager::instance().get("exiftool");
  if (exiftool && exiftool->availability().available) {
    const auto mp4_pref =
        umm::evaluateStorage(std::filesystem::path("a.mp4"), preferred);
    if (!mp4_pref.ok() || mp4_pref.value().backend != "exiftool" ||
        !formats_are(mp4_pref.value(), {"XMP", "QuickTime"})) {
      return fail("MP4 default preferred selects ExifTool");
    }
  }
  umm::WriteOptions mp4_exiv2;
  mp4_exiv2.policy = umm::StoragePolicy::preferred;
  mp4_exiv2.backend = "exiv2";
  const auto mp4_exiv2_decision =
      umm::evaluateStorage(std::filesystem::path("a.mp4"), mp4_exiv2);
  if (mp4_exiv2_decision.ok() ||
      mp4_exiv2_decision.error().code != umm::ErrorCode::unsupported_capability) {
    return fail("MP4 Exiv2 preferred unsupported_capability");
  }
  umm::WriteOptions mp4_sidecar;
  mp4_sidecar.policy = umm::StoragePolicy::sidecar_only;
  const auto mp4_sc =
      umm::evaluateStorage(std::filesystem::path("a.mp4"), mp4_sidecar);
  if (!mp4_sc.ok() ||
      mp4_sc.value().method != umm::StorageDecision::Method::sidecar ||
      !formats_are(mp4_sc.value(), {"XMP"})) {
    return fail("MP4 sidecar_only");
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
