// ============================================================================
// DESIGN DRAFT — NOT BUILT, NOT TESTED.
// Normative statement of API shape per docs/analysis decision M7.
// Promoted to a real header by docs/implementation/08-core-semantic-model.md.
//
// The Value vocabulary mirrors the registry datatype vocabulary defined in
// docs/implementation/06-registry-importer.md (which in turn follows the IPTC
// Technical Reference). Values are standards-shaped, not backend-shaped.
// ============================================================================
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace umm {

// XMP LangAlt: language tag (BCP 47, "x-default" allowed) -> text.
using LangAlt = std::map<std::string, std::string>;

// EXIF-style rational.
struct Rational {
  std::int64_t numerator{};
  std::int64_t denominator{1};
};

// Date-time with the partial precision metadata standards actually need:
// EXIF may lack a UTC offset; IPTC IIM splits date and time; XMP allows
// date-only values. Absent components are represented, never guessed.
struct DateTime {
  int year{};                       // required
  std::optional<int> month;         // 1-12
  std::optional<int> day;           // 1-31
  std::optional<int> hour;          // 0-23; presence implies minute
  std::optional<int> minute;
  std::optional<int> second;
  std::optional<int> subsecond_ns;
  std::optional<int> utc_offset_minutes;  // absent = unknown offset, NOT UTC
};

// GPS coordinate (decision-relevant: GPS coordinates and named place are
// SEPARATE capabilities and separate value shapes; see supported-types.md §3).
struct GpsCoordinate {
  double latitude{};    // decimal degrees, WGS 84
  double longitude{};
  std::optional<double> altitude_meters;  // negative = below sea level
  std::optional<DateTime> gps_time;
};

struct Value;  // forward declaration for Structure

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
