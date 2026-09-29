#include "umm/track.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <variant>

#ifndef UMM_FIXTURES_DIR
#error "UMM_FIXTURES_DIR must be defined by the build"
#endif

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

std::filesystem::path fixtures_dir() {
  const char* raw = UMM_FIXTURES_DIR;
  return std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t*>(raw)));
}

std::filesystem::path tracks_dir() { return fixtures_dir() / "tracks"; }

void write_all(const std::filesystem::path& path, std::string_view text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

bool near(double actual, double expected) {
  return std::fabs(actual - expected) < 1e-5;
}

bool is_utc(const umm::DateTime& dt) {
  return dt.utc_offset_minutes && *dt.utc_offset_minutes == 0 && dt.year == 2020 &&
         dt.month && *dt.month == 1 && dt.day && *dt.day == 2 && dt.hour &&
         dt.minute && dt.second;
}

bool time_hms(const umm::DateTime& dt, int hour, int minute, int second) {
  return is_utc(dt) && *dt.hour == hour && *dt.minute == minute &&
         *dt.second == second;
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

umm::DateTime naive_hms(int hour, int minute, int second) {
  umm::DateTime dt = utc_hms(hour, minute, second);
  dt.utc_offset_minutes.reset();
  return dt;
}

umm::Metadata with_photo_date(umm::DateTime dt) {
  umm::Metadata md;
  if (!md.setDateCreated(dt).ok()) {
    std::fprintf(stderr, "setDateCreated failed\n");
  }
  return md;
}

bool gps_near(const umm::GpsCoordinate& gps, double lat, double lon,
              std::optional<double> alt = std::nullopt) {
  if (!near(gps.latitude, lat) || !near(gps.longitude, lon)) {
    return false;
  }
  if (alt) {
    return gps.altitude_meters && near(*gps.altitude_meters, *alt);
  }
  return true;
}

}  // namespace

int main() {
  const std::filesystem::path missing =
      tracks_dir() / "does-not-exist.gpx";
  const auto missing_result = umm::importTrack(missing);
  if (missing_result.ok() ||
      missing_result.error().code != umm::ErrorCode::io_not_found) {
    return fail("missing file");
  }

  const auto gpx = umm::importTrack(tracks_dir() / "straight.gpx");
  if (!gpx.ok()) {
    std::fprintf(stderr, "straight.gpx: %s\n", gpx.error().message.c_str());
    return fail("straight.gpx");
  }
  if (gpx.value().format != umm::TrackFormat::gpx ||
      gpx.value().points.size() != 3) {
    return fail("straight.gpx point count");
  }
  const umm::TrackPoint& a = gpx.value().points[0];
  const umm::TrackPoint& b = gpx.value().points[1];
  const umm::TrackPoint& c = gpx.value().points[2];
  if (!near(a.latitude, 37.7749) || !near(a.longitude, -122.4194) ||
      !a.altitude_meters || !near(*a.altitude_meters, 10) ||
      !time_hms(a.time, 3, 4, 5)) {
    return fail("straight.gpx first point");
  }
  if (!near(b.latitude, 37.7759) || !time_hms(b.time, 3, 5, 5) ||
      !b.altitude_meters || !near(*b.altitude_meters, 12)) {
    return fail("straight.gpx second point");
  }
  if (!near(c.latitude, 37.7769) || !time_hms(c.time, 3, 6, 5)) {
    return fail("straight.gpx third point");
  }
  if (!gpx.value().start_time || !gpx.value().end_time ||
      *gpx.value().start_time != a.time || *gpx.value().end_time != c.time) {
    return fail("straight.gpx time range");
  }

  const auto nmea = umm::importTrack(tracks_dir() / "nmea.nmea");
  if (!nmea.ok()) {
    std::fprintf(stderr, "nmea.nmea: %s\n", nmea.error().message.c_str());
    return fail("nmea.nmea");
  }
  if (nmea.value().format != umm::TrackFormat::nmea ||
      nmea.value().points.size() != 3) {
    return fail("nmea.nmea point count");
  }
  const umm::TrackPoint& na = nmea.value().points[0];
  if (!near(na.latitude, 37.7749) || !near(na.longitude, -122.4194) ||
      !na.altitude_meters || !near(*na.altitude_meters, 10) ||
      !time_hms(na.time, 3, 4, 5)) {
    std::fprintf(stderr, "nmea first lat=%.8f lon=%.8f alt=%s\n", na.latitude,
                 na.longitude,
                 na.altitude_meters ? std::to_string(*na.altitude_meters).c_str()
                                    : "(none)");
    return fail("nmea.nmea first point");
  }
  if (!near(nmea.value().points[2].latitude, 37.7769) ||
      !nmea.value().points[2].altitude_meters ||
      !near(*nmea.value().points[2].altitude_meters, 14)) {
    return fail("nmea.nmea last point");
  }

  const auto kml = umm::importTrack(tracks_dir() / "straight.kml");
  if (!kml.ok()) {
    std::fprintf(stderr, "straight.kml: %s\n", kml.error().message.c_str());
    return fail("straight.kml");
  }
  if (kml.value().format != umm::TrackFormat::kml ||
      kml.value().points.size() != 3 ||
      !near(kml.value().points[0].longitude, -122.4194) ||
      !near(kml.value().points[0].latitude, 37.7749) ||
      !kml.value().points[0].altitude_meters ||
      !near(*kml.value().points[0].altitude_meters, 10) ||
      !time_hms(kml.value().points[0].time, 3, 4, 5)) {
    return fail("straight.kml points");
  }

  const auto gaps = umm::importTrack(tracks_dir() / "gaps.gpx");
  if (!gaps.ok() || gaps.value().points.size() != 4) {
    return fail("gaps.gpx");
  }
  if (!time_hms(gaps.value().points[1].time, 3, 5, 5) ||
      !time_hms(gaps.value().points[2].time, 3, 5, 5) ||
      !near(gaps.value().points[1].latitude, 37.7759) ||
      !near(gaps.value().points[2].latitude, 37.7800) ||
      !time_hms(gaps.value().points[3].time, 4, 5, 5)) {
    return fail("gaps.gpx duplicate time order");
  }

  const auto bad_gpx = umm::importTrack(tracks_dir() / "malformed.gpx");
  if (bad_gpx.ok() || bad_gpx.error().code != umm::ErrorCode::format_corrupt) {
    return fail("malformed.gpx");
  }
  const auto bad_nmea = umm::importTrack(tracks_dir() / "malformed.nmea");
  if (bad_nmea.ok() ||
      bad_nmea.error().code != umm::ErrorCode::format_corrupt) {
    return fail("malformed.nmea");
  }

  const auto jpeg = umm::importTrack(fixtures_dir() / "jpeg" / "minimal.jpg");
  if (jpeg.ok() || jpeg.error().code != umm::ErrorCode::format_unrecognized) {
    return fail("jpeg is not a track");
  }

  const std::filesystem::path tmp =
      std::filesystem::temp_directory_path() / "umm-track-import";
  std::filesystem::create_directories(tmp);

  const std::filesystem::path offset = tmp / "offset.gpx";
  write_all(offset,
            "<?xml version=\"1.0\"?>\n"
            "<gpx><trk><trkseg>\n"
            "<trkpt lat=\"10\" lon=\"20\">"
            "<time>2020-01-02T04:04:05+01:00</time>"
            "</trkpt>\n"
            "</trkseg></trk></gpx>\n");
  const auto offset_result = umm::importTrack(offset);
  if (!offset_result.ok() || offset_result.value().points.size() != 1 ||
      !time_hms(offset_result.value().points[0].time, 3, 4, 5)) {
    return fail("GPX offset converted to UTC");
  }

  const std::filesystem::path unordered = tmp / "unordered.gpx";
  write_all(unordered,
            "<?xml version=\"1.0\"?>\n"
            "<gpx><trk><trkseg>\n"
            "<trkpt lat=\"1\" lon=\"1\"><time>2020-01-02T03:06:05Z</time></trkpt>\n"
            "<trkpt lat=\"2\" lon=\"2\"><time>2020-01-02T03:04:05Z</time></trkpt>\n"
            "<trkpt lat=\"3\" lon=\"3\"><time>2020-01-02T03:05:05Z</time></trkpt>\n"
            "</trkseg></trk></gpx>\n");
  const auto ordered = umm::importTrack(unordered);
  if (!ordered.ok() || ordered.value().points.size() != 3 ||
      !near(ordered.value().points[0].latitude, 2) ||
      !near(ordered.value().points[1].latitude, 3) ||
      !near(ordered.value().points[2].latitude, 1)) {
    return fail("GPX points sorted by time");
  }

  const std::filesystem::path sniffed = tmp / "track.txt";
  write_all(sniffed,
            "$GPRMC,030405.00,A,3746.494000,N,12225.164000,W,0.0,0.0,020120,,,A*45\n");
  const auto sniffed_result = umm::importTrack(sniffed);
  if (!sniffed_result.ok() ||
      sniffed_result.value().format != umm::TrackFormat::nmea ||
      sniffed_result.value().points.size() != 1) {
    return fail("NMEA sniffed from .txt");
  }

  const std::filesystem::path empty_gpx = tmp / "empty.gpx";
  write_all(empty_gpx, "<?xml version=\"1.0\"?><gpx version=\"1.1\"></gpx>\n");
  const auto empty = umm::importTrack(empty_gpx);
  if (!empty.ok() || !empty.value().points.empty() || empty.value().start_time) {
    return fail("empty GPX");
  }

  const umm::Track straight = gpx.value();
  umm::MatchOptions as_utc;
  as_utc.naive_utc_offset_minutes = 0;

  const auto exact =
      umm::matchTrack(with_photo_date(utc_hms(3, 4, 5)), straight);
  if (!exact.ok() || exact.value().kind != umm::TrackMatchKind::exact ||
      exact.value().time_delta_ns != 0 || exact.value().after ||
      !gps_near(exact.value().position, 37.7749, -122.4194, 10) ||
      !exact.value().position.gps_time ||
      !time_hms(*exact.value().position.gps_time, 3, 4, 5)) {
    return fail("exact-point match");
  }

  const auto last =
      umm::matchTrack(with_photo_date(utc_hms(3, 6, 5)), straight);
  if (!last.ok() || last.value().kind != umm::TrackMatchKind::exact ||
      !gps_near(last.value().position, 37.7769, -122.4174, 14)) {
    return fail("exact last point");
  }

  const auto interpolated =
      umm::matchTrack(with_photo_date(utc_hms(3, 4, 35)), straight);
  if (!interpolated.ok() ||
      interpolated.value().kind != umm::TrackMatchKind::interpolated ||
      !interpolated.value().after ||
      !gps_near(interpolated.value().position, 37.7754, -122.4189, 11) ||
      interpolated.value().time_delta_ns != 30'000'000'000) {
    std::fprintf(stderr, "interp ok=%d kind=%d lat=%.8f lon=%.8f alt=%s dt=%lld\n",
                 interpolated.ok() ? 1 : 0,
                 interpolated.ok() ? static_cast<int>(interpolated.value().kind)
                                   : -1,
                 interpolated.ok() ? interpolated.value().position.latitude : 0,
                 interpolated.ok() ? interpolated.value().position.longitude : 0,
                 interpolated.ok() && interpolated.value().position.altitude_meters
                     ? std::to_string(*interpolated.value().position.altitude_meters)
                           .c_str()
                     : "(none)",
                 interpolated.ok()
                     ? static_cast<long long>(interpolated.value().time_delta_ns)
                     : 0);
    return fail("interpolated match");
  }

  umm::MatchOptions nearest_opts;
  nearest_opts.interpolate = false;
  const auto nearest =
      umm::matchTrack(with_photo_date(utc_hms(3, 4, 35)), straight, nearest_opts);
  if (!nearest.ok() || nearest.value().kind != umm::TrackMatchKind::nearest ||
      nearest.value().after ||
      !gps_near(nearest.value().position, 37.7749, -122.4194, 10) ||
      nearest.value().time_delta_ns != 30'000'000'000) {
    return fail("nearest-point match");
  }

  umm::MatchOptions offset_opts;
  offset_opts.camera_clock_offset_seconds = -60;
  const auto offset_match =
      umm::matchTrack(with_photo_date(utc_hms(3, 5, 5)), straight, offset_opts);
  if (!offset_match.ok() || offset_match.value().kind != umm::TrackMatchKind::exact ||
      !gps_near(offset_match.value().position, 37.7749, -122.4194, 10)) {
    return fail("camera clock offset");
  }

  const auto naive_refused =
      umm::matchTrack(with_photo_date(naive_hms(3, 4, 5)), straight);
  if (naive_refused.ok() ||
      naive_refused.error().code != umm::ErrorCode::invalid_value) {
    return fail("naive timestamp refused");
  }
  const auto naive_ok =
      umm::matchTrack(with_photo_date(naive_hms(3, 4, 5)), straight, as_utc);
  if (!naive_ok.ok() || naive_ok.value().kind != umm::TrackMatchKind::exact) {
    return fail("naive timestamp with explicit UTC offset");
  }

  umm::MatchOptions naive_plus_hour;
  naive_plus_hour.naive_utc_offset_minutes = 60;
  const auto naive_local =
      umm::matchTrack(with_photo_date(naive_hms(4, 4, 5)), straight,
                      naive_plus_hour);
  if (!naive_local.ok() || naive_local.value().kind != umm::TrackMatchKind::exact ||
      !gps_near(naive_local.value().position, 37.7749, -122.4194, 10)) {
    return fail("naive timestamp with +01:00");
  }

  const auto before =
      umm::matchTrack(with_photo_date(utc_hms(3, 4, 4)), straight);
  const auto after =
      umm::matchTrack(with_photo_date(utc_hms(3, 6, 6)), straight);
  if (before.ok() || before.error().code != umm::ErrorCode::invalid_value ||
      after.ok() || after.error().code != umm::ErrorCode::invalid_value) {
    return fail("out-of-window refusal");
  }

  const auto missing = umm::matchTrack(umm::Metadata{}, straight);
  if (missing.ok() || missing.error().code != umm::ErrorCode::invalid_value) {
    return fail("missing capture timestamp");
  }

  umm::DateTime date_only;
  date_only.year = 2020;
  date_only.month = 1;
  date_only.day = 2;
  date_only.utc_offset_minutes = 0;
  const auto partial =
      umm::matchTrack(with_photo_date(date_only), straight);
  if (partial.ok() || partial.error().code != umm::ErrorCode::invalid_value) {
    return fail("date-only capture");
  }

  const auto empty_match = umm::matchTrack(with_photo_date(utc_hms(3, 4, 5)),
                                           empty.value());
  if (empty_match.ok() ||
      empty_match.error().code != umm::ErrorCode::invalid_value) {
    return fail("empty track");
  }

  const umm::Track gappy = gaps.value();
  umm::MatchOptions tight_gap;
  tight_gap.max_time_gap_seconds = 600;
  const auto gap_refused =
      umm::matchTrack(with_photo_date(utc_hms(3, 35, 5)), gappy, tight_gap);
  if (gap_refused.ok() ||
      gap_refused.error().code != umm::ErrorCode::invalid_value) {
    return fail("gap exceeds max_time_gap");
  }
  const auto gap_ok =
      umm::matchTrack(with_photo_date(utc_hms(3, 35, 5)), gappy);
  if (!gap_ok.ok() || gap_ok.value().kind != umm::TrackMatchKind::interpolated) {
    return fail("gap interpolates without max_time_gap");
  }

  const auto dup_exact =
      umm::matchTrack(with_photo_date(utc_hms(3, 5, 5)), gappy);
  if (!dup_exact.ok() || dup_exact.value().kind != umm::TrackMatchKind::exact ||
      !gps_near(dup_exact.value().position, 37.7759, -122.4184)) {
    return fail("duplicate timestamp uses first sample");
  }

  umm::Metadata video;
  umm::Value video_date;
  video_date.data = utc_hms(3, 4, 5);
  if (!video.set("iptc.video.dateCreated", video_date).ok()) {
    return fail("set video dateCreated");
  }
  const auto video_match = umm::matchTrack(video, straight);
  if (!video_match.ok() || video_match.value().kind != umm::TrackMatchKind::exact ||
      !gps_near(video_match.value().position, 37.7749, -122.4194, 10)) {
    return fail("video dateCreated match");
  }

  return 0;
}
