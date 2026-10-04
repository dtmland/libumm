#include "read_base_checks.hpp"
#include "umm/umm.hpp"

#include <filesystem>
#include <string>
#include <variant>
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

std::filesystem::path work_dir() {
  const auto dir = std::filesystem::temp_directory_path() / "umm-test-cast";
  std::filesystem::create_directories(dir);
  return dir;
}

std::filesystem::path copy_fixture(const std::filesystem::path& source,
                                   const std::string& name) {
  const auto dest = work_dir() / name;
  std::filesystem::copy_file(
      source, dest, std::filesystem::copy_options::overwrite_existing);
  return dest;
}

const umm::CastCandidate* find_group(const std::vector<umm::CastCandidate>& list,
                                     std::string_view group) {
  for (const umm::CastCandidate& candidate : list) {
    if (candidate.group == group) {
      return &candidate;
    }
  }
  return nullptr;
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  umm::Backend* backend = umm::BackendManager::instance().get("exiftool");
  if (!backend || !backend->availability().available) {
    return 0;
  }

  umm::ReadOptions read_options;
  read_options.backend = "exiftool";
  read_options.report_casts = true;
  const auto preview =
      umm::read(raw_stem("video", "minimal", ".mp4"), read_options);
  if (!preview.ok()) {
    std::fprintf(stderr, "minimal report_casts failed: %s\n",
                 preview.error().message.c_str());
    return 1;
  }
  if (preview.value().get("iptc.video.dateCreated")) {
    return fail("movie-header-only MP4 must not have dateCreated");
  }
  const auto* created =
      find_group(preview.value().castCandidates(), "videoCreated");
  if (!created) {
    return fail("movie-header-only MP4 missing videoCreated candidate");
  }

  const auto jpeg = copy_fixture(raw_jpeg("gps.jpg"), "cast-side.jpg");
  umm::CastOptions side;
  side.dry_run = true;
  const auto side_preview =
      umm::cast(jpeg, umm::CastDirection::side, side);
  if (!side_preview.ok()) {
    std::fprintf(stderr, "side dry-run failed: %s\n",
                 side_preview.error().message.c_str());
    return 1;
  }
  if (!find_group(side_preview.value().candidates, "locationShownLegacy")) {
    return fail("gps.jpg should preview locationShownLegacy");
  }
  side.dry_run = false;
  const auto side_applied =
      umm::cast(jpeg, umm::CastDirection::side, side);
  if (!side_applied.ok()) {
    std::fprintf(stderr, "side apply failed: %s\n",
                 side_applied.error().message.c_str());
    return 1;
  }
  const auto shown =
      side_applied.value().metadata.get("iptc.photo.locationShownInTheImage");
  const auto* items =
      shown ? std::get_if<std::vector<umm::Structure>>(&shown->value.data)
            : nullptr;
  if (!items || items->empty()) {
    return fail("side apply did not fill locationShownInTheImage");
  }

  const auto mp4 =
      copy_fixture(raw_stem("video", "minimal", ".mp4"), "cast-down.mp4");
  umm::Metadata metadata;
  metadata.setMediaDomain(umm::MediaDomain::video);
  umm::Structure loc;
  loc.emplace("gpsLatitude", umm::Value{37.7749});
  loc.emplace("gpsLongitude", umm::Value{-122.4194});
  if (!metadata
           .set("iptc.video.locationShot",
                umm::Value{std::vector<umm::Structure>{loc}})
           .ok()) {
    return fail("set locationShot");
  }
  umm::WriteOptions write_options;
  write_options.backend = "exiftool";
  const auto written = umm::write(mp4, metadata, write_options);
  if (!written.ok()) {
    std::fprintf(stderr, "default downcast write failed: %s (%s)\n",
                 written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }
  bool saw_iso = false;
  for (const umm::BaseKey& key : written.value().written) {
    if (key.key.find("location.ISO6709") != std::string::npos ||
        key.key.find("GPSCoordinates") != std::string::npos) {
      saw_iso = true;
    }
  }
  if (!saw_iso) {
    return fail("default video write downcast missing QuickTime GPS");
  }

  umm::CastOptions up;
  up.dry_run = true;
  up.include_approximate = true;
  const auto up_preview = umm::cast(mp4, umm::CastDirection::up, up);
  if (!up_preview.ok()) {
    std::fprintf(stderr, "up dry-run failed: %s\n",
                 up_preview.error().message.c_str());
    return 1;
  }
  if (!find_group(up_preview.value().candidates, "capturePosition")) {
    return fail("upcast should preview capturePosition after downcast write");
  }
  return 0;
}
