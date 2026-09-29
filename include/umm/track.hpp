// GPS track import and correlation (concept.md §16, §29; sessions 25–26).
// Location write-back uses umm::write and exif.gps.position — no track-specific
// write path.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include "umm/metadata.hpp"
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

// --- Correlation (session 26) ------------------------------------------------
//
// Timezone policy (track times are always UTC):
//   Media capture times come from the reconciled Phase 1 date property
//   (iptc.photo.dateCreated, else iptc.video.dateCreated). A DateTime with
//   utc_offset_minutes is converted to UTC. A naive timestamp (offset absent)
//   is refused (invalid_value) unless MatchOptions::naive_utc_offset_minutes
//   is set — libumm does not assume naive EXIF/IIM/QuickTime times are UTC
//   (docs/reconciliation-policy.md). Passing 0 treats the naive time as UTC.
//   camera_clock_offset_seconds is then added to that UTC instant to obtain
//   the track-search instant (positive = camera clock behind GPS).
//
// Window and gaps:
//   The search instant must lie in [start_time, end_time] inclusive; outside
//   is invalid_value (including empty tracks). Date-only or time-less capture
//   values are invalid_value (hour and minute required).
//   interpolate=true (default): linear lat/lon (and altitude/accuracy when
//   both brackets have them) between the last sample at t <= search and the
//   first sample at t >= search. An exact sample hit is not interpolated.
//   interpolate=false: the nearest sample (earlier index on a tie).
//   max_time_gap_seconds, when set, refuses interpolation whose bracketing
//   span exceeds the gap, and refuses nearest/exact when |search − sample|
//   exceeds the gap. Unset means no extra limit beyond the track window.
//   Duplicate timestamps keep file order: exact hits use the first sample;
//   the left bracket of an open interval is the last sample with t <= search.

struct MatchOptions {
  // Added to the UTC capture instant before matching. Unset = 0.
  std::optional<std::int64_t> camera_clock_offset_seconds;
  std::optional<std::int64_t> max_time_gap_seconds;
  bool interpolate{true};
  std::optional<int> naive_utc_offset_minutes;

  bool operator==(const MatchOptions&) const = default;
};

enum class TrackMatchKind {
  exact,         // search instant equals a sample
  interpolated,  // linear between bracketing samples
  nearest,       // interpolate == false; closest sample
};

struct TrackMatch {
  GpsCoordinate position;  // gps_time is the search instant (UTC)
  TrackMatchKind kind{};
  // search instant minus the nearer bracketing sample (0 on an exact hit).
  std::int64_t time_delta_ns{};
  TrackPoint before;
  std::optional<TrackPoint> after;

  bool operator==(const TrackMatch&) const = default;
};

Result<TrackMatch> matchTrack(const Metadata& metadata, const Track& track,
                              MatchOptions options = {});

Result<TrackMatch> matchTrack(const std::filesystem::path& media,
                              const Track& track, MatchOptions options = {});

}  // namespace umm
