#include "umm/track.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

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

  return 0;
}
