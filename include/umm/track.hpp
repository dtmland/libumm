// GPS track import (concept.md §16, §29; session 25).
// Import only — correlation and location write-back are session 26.
#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "umm/result.hpp"
#include "umm/value.hpp"

namespace umm {

enum class TrackFormat {
  gpx,
  nmea,
  kml,
};

// One sample from a GPX/NMEA/KML track. Times are stored as UTC
// (DateTime::utc_offset_minutes == 0).
//
// Timezone:
//   GPX 1.1 times are UTC; a zone offset in the ISO-8601 value is converted
//   to UTC. A time with no zone designator is treated as UTC (GPX), not as
//   a naive local time.
//   NMEA RMC/ZDA carry the UTC date; GGA/RMC/ZDA times of day are UTC and
//   are combined with the most recent date from RMC or ZDA.
//   KML gx:Track <when> values follow the same ISO-8601-to-UTC rule as GPX.
//
// accuracy_meters is only set when the source format provides a meters
// quantity. HDOP/PDOP are not converted into meters.
struct TrackPoint {
  DateTime time;  // UTC
  double latitude{};
  double longitude{};
  std::optional<double> altitude_meters;
  std::optional<double> accuracy_meters;

  bool operator==(const TrackPoint&) const = default;
};

// Ordered UTC samples from one file. start_time/end_time are the first and
// last point times after stable sort by UTC instant (absent when empty).
struct Track {
  TrackFormat format{};
  std::vector<TrackPoint> points;
  std::optional<DateTime> start_time;
  std::optional<DateTime> end_time;

  bool operator==(const Track&) const = default;
};

// Detect format by extension (.gpx / .nmea / .nme / .kml) and content sniff.
// Content wins when both are present. Missing file -> io_not_found;
// unreadable -> io_read_failed; unknown type -> format_unrecognized;
// parse failure or no usable points in a positioned file -> format_corrupt.
Result<Track> importTrack(const std::filesystem::path& path);

}  // namespace umm
