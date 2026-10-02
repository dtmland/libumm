#include "read_unmapped_checks.hpp"
#include "umm/umm.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
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
  const auto dir = std::filesystem::temp_directory_path() / "umm-track-match";
  std::filesystem::create_directories(dir);
  return dir;
}

std::filesystem::path copy_named(const std::filesystem::path& source,
                                 const std::string& name) {
  const auto dest = work_dir() / name;
  std::filesystem::copy_file(
      source, dest, std::filesystem::copy_options::overwrite_existing);
  return dest;
}

bool near(double actual, double expected) {
  return std::fabs(actual - expected) < 1e-4;
}

umm::DateTime utc_hms(int hour, int minute, int second) {
  umm::DateTime dt;
  dt.year = 2020;
  dt.month = 1;
  dt.day = 2;
  dt.hour = hour;
  dt.minute = minute;
  dt.second = second;
  dt.utc_offset_minutes = 0;
  return dt;
}

int write_gps(const std::filesystem::path& file, const umm::TrackMatch& match,
              const umm::WriteOptions& wopts, const umm::ReadOptions& ropts) {
  auto loaded = umm::read(file, ropts);
  if (!loaded.ok()) {
    std::fprintf(stderr, "read before gps write: %s\n",
                 loaded.error().message.c_str());
    return 1;
  }
  umm::Metadata metadata = loaded.value();
  if (!metadata.setGps(match.position).ok()) {
    return fail("setGps");
  }
  const auto written = umm::write(file, metadata, wopts);
  if (!written.ok()) {
    std::fprintf(stderr, "gps write failed: %s (%s)\n",
                 written.error().message.c_str(), written.error().detail.c_str());
    return 1;
  }
  const auto round = umm::read(file, ropts);
  if (!round.ok()) {
    std::fprintf(stderr, "read after gps write: %s\n",
                 round.error().message.c_str());
    return 1;
  }
  const auto gps = round.value().gps();
  const auto* coord =
      gps ? std::get_if<umm::GpsCoordinate>(&gps->value.data) : nullptr;
  if (!coord || !near(coord->latitude, match.position.latitude) ||
      !near(coord->longitude, match.position.longitude)) {
    return fail("gps read-back");
  }
  return 0;
}

int test_jpeg(const std::string& backend) {
  const auto track = umm::importTrack(raw_fixtures_dir() / "tracks" / "straight.gpx");
  if (!track.ok()) {
    return fail("import straight.gpx");
  }

  umm::WriteOptions wopts;
  wopts.backend = backend;
  umm::ReadOptions ropts;
  ropts.backend = backend;

  umm::MatchOptions naive_utc;
  naive_utc.naive_utc_offset_minutes = 0;

  const auto exact_file =
      copy_named(raw_jpeg("exif-only.jpg"), backend + "-track-exact.jpg");
  const auto refused = umm::matchTrack(exact_file, track.value());
  if (refused.ok() || refused.error().code != umm::ErrorCode::invalid_value) {
    return fail("jpeg naive timestamp refused");
  }
  const auto exact = umm::matchTrack(exact_file, track.value(), naive_utc);
  if (!exact.ok() || exact.value().kind != umm::TrackMatchKind::exact ||
      !near(exact.value().position.latitude, 37.7749) ||
      !near(exact.value().position.longitude, -122.4194)) {
    if (!exact.ok()) {
      std::fprintf(stderr, "jpeg exact match failed: %s\n",
                   exact.error().message.c_str());
    }
    return fail("jpeg exact match");
  }
  if (const int rc = write_gps(exact_file, exact.value(), wopts, ropts);
      rc != 0) {
    return rc;
  }

  const auto interp_file =
      copy_named(raw_jpeg("minimal.jpg"), backend + "-track-interp.jpg");
  umm::Metadata dated;
  if (!dated.setDateCreated(utc_hms(3, 4, 35)).ok()) {
    return fail("setDateCreated interpolated jpeg");
  }
  const auto dated_write = umm::write(interp_file, dated, wopts);
  if (!dated_write.ok()) {
    std::fprintf(stderr, "jpeg date write failed: %s\n",
                 dated_write.error().message.c_str());
    return 1;
  }
  const auto interpolated =
      umm::matchTrack(interp_file, track.value(), naive_utc);
  if (!interpolated.ok() ||
      interpolated.value().kind != umm::TrackMatchKind::interpolated ||
      !near(interpolated.value().position.latitude, 37.7754) ||
      !near(interpolated.value().position.longitude, -122.4189)) {
    if (!interpolated.ok()) {
      std::fprintf(stderr, "jpeg interp match failed: %s\n",
                   interpolated.error().message.c_str());
    }
    return fail("jpeg interpolated match");
  }
  return write_gps(interp_file, interpolated.value(), wopts, ropts);
}

int test_video(const char* ext) {
  umm::Backend* backend = umm::BackendManager::instance().get("exiftool");
  if (!backend || !backend->availability().available) {
    return 0;
  }
  const auto track = umm::importTrack(raw_fixtures_dir() / "tracks" / "straight.gpx");
  if (!track.ok()) {
    return fail("import straight.gpx for video");
  }

  umm::WriteOptions wopts;
  wopts.backend = "exiftool";
  umm::ReadOptions ropts;
  ropts.backend = "exiftool";

  const std::string dest = std::string("track-exact") + ext;
  const auto file = copy_named(raw_stem("video", "minimal", ext), dest);
  umm::Metadata dated;
  umm::Value when;
  when.data = utc_hms(3, 4, 5);
  if (!dated.set("iptc.video.dateCreated", when).ok()) {
    return fail("set video dateCreated");
  }
  const auto dated_write = umm::write(file, dated, wopts);
  if (!dated_write.ok()) {
    std::fprintf(stderr, "video %s date write failed: %s (%s)\n", ext,
                 dated_write.error().message.c_str(),
                 dated_write.error().detail.c_str());
    return 1;
  }
  umm::MatchOptions naive_utc;
  naive_utc.naive_utc_offset_minutes = 0;
  // Path overload pins ExifTool for MP4/MOV (preferred_backend), matching
  // default umm::read.
  // Session 34: MOV write-back closes the session 26 cut line.
  const auto matched = umm::matchTrack(file, track.value(), naive_utc);
  if (!matched.ok() || matched.value().kind != umm::TrackMatchKind::exact ||
      !near(matched.value().position.latitude, 37.7749) ||
      !near(matched.value().position.longitude, -122.4194)) {
    if (!matched.ok()) {
      std::fprintf(stderr, "video %s match failed: %s\n", ext,
                   matched.error().message.c_str());
    }
    return fail("video exact match");
  }
  return write_gps(file, matched.value(), wopts, ropts);
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  umm::BackendManager& manager = umm::BackendManager::instance();
  std::vector<std::string> available;
  for (const std::string& id : {"exiv2", "exiftool"}) {
    umm::Backend* backend = manager.get(id);
    if (backend && backend->availability().available) {
      available.push_back(id);
    }
  }
  if (available.empty()) {
    return fail("no backend available for test_track_match");
  }
  for (const std::string& id : available) {
    if (const int rc = test_jpeg(id); rc != 0) {
      std::fprintf(stderr, "backend %s jpeg track match failed\n", id.c_str());
      return rc;
    }
  }
  if (const int rc = test_video(".mp4"); rc != 0) {
    return rc;
  }
  return test_video(".mov");
}
