// Value vocabulary mirrors the registry datatype set from session 06
// (docs/implementation/06-registry-importer.md) plus boolean, rational, and
// GPS shapes reserved in session 07. Values are standards-shaped, not
// backend-shaped. Value is a plain value type with no shared mutable state.
#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "umm/result.hpp"

namespace umm {

// XMP LangAlt: language tag (BCP 47, "x-default" allowed) -> text.
using LangAlt = std::map<std::string, std::string>;

// EXIF-style rational. Equality is exact (numerator/denominator), not reduced.
struct Rational {
  std::int64_t numerator{};
  std::int64_t denominator{1};

  bool operator==(const Rational&) const = default;
};

// Date-time with the partial precision metadata standards actually need:
// EXIF may lack a UTC offset; IPTC IIM splits date and time; XMP allows
// date-only values. Absent components are represented, never guessed.
struct DateTime {
  int year{};                              // required
  std::optional<int> month;                // 1-12
  std::optional<int> day;                  // 1-31
  std::optional<int> hour;                 // 0-23; presence implies minute
  std::optional<int> minute;
  std::optional<int> second;
  std::optional<int> subsecond_ns;
  std::optional<int> utc_offset_minutes;  // absent = unknown offset, NOT UTC

  bool operator==(const DateTime&) const = default;
};

// Decision F6. Backend-free and always thread-safe: no shared state, no
// backend, no time-zone database, no leap seconds. Arithmetic is proleptic
// Gregorian via std::chrono calendar types. An absent utc_offset_minutes is
// unknown, never UTC (H12); shift never invents one.
//
// Precision is the finest component present. A shift must be representable
// at that precision, else invalid_value:
// - year, or year and month: only a zero shift
// - date (no time): a whole number of days
// - minute (hour and minute, no seconds): a whole number of minutes
// - second or sub-second: any whole seconds; sub-seconds are unchanged
// Input fields must already be valid (calendar date, 0-23/0-59, sub-second
// nanoseconds, offset within ±18:00) or the call returns invalid_value.
enum class OffsetMode {
  reinterpret,  // same wall-clock fields, new offset label
  convert,      // same instant; wall clock moves by the offset delta
};

Result<DateTime> shift(const DateTime& value, std::chrono::seconds delta);

// `minutes` must be within ±18:00 (XMP / ISO 8601). convert fails with
// invalid_value when the offset is unknown or the value has no time
// component. reinterpret assigns `minutes` without moving the wall clock,
// including when the input offset was unknown — that assignment is explicit,
// not a guess that unknown means UTC.
Result<DateTime> withOffset(const DateTime& value, int minutes, OffsetMode mode);

// GPS coordinate (GPS coordinates and named place are SEPARATE capabilities
// and separate value shapes; see supported-types.md §3).
struct GpsCoordinate {
  double latitude{};   // decimal degrees, WGS 84
  double longitude{};
  std::optional<double> altitude_meters;  // negative = below sea level
  std::optional<DateTime> gps_time;

  bool operator==(const GpsCoordinate&) const = default;
};

struct Value;

// Structured value (e.g. IPTC Extension LocationCreated / LocationShown).
using Structure = std::map<std::string, Value>;

struct Value {
  std::variant<
      std::string,               // text
      LangAlt,                   // language alternatives
      std::vector<std::string>,  // text list (bag/seq)
      std::int64_t,              // integer
      double,                    // real
      bool,                      // boolean
      Rational,                  // rational
      DateTime,                  // date-time (partial precision)
      GpsCoordinate,             // GPS position
      Structure,                 // named-field structure
      std::vector<Structure>>    // list of structures
      data;

  bool operator==(const Value&) const;
  std::string toString() const;  // diagnostic form, for tests/logs only
};

}  // namespace umm
