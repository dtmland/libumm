#include "umm/value.hpp"

#include <chrono>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>

namespace umm {
namespace {

std::string quote(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

std::string pad2(int value) {
  const int abs_value = value < 0 ? -value : value;
  std::string digits = std::to_string(abs_value);
  if (digits.size() < 2) {
    digits.insert(digits.begin(), 2 - digits.size(), '0');
  }
  if (value < 0) {
    digits.insert(digits.begin(), '-');
  }
  return digits;
}

std::string dateTimeToString(const DateTime& dt) {
  std::string out = std::to_string(dt.year);
  if (!dt.month) {
    return out;
  }
  out += '-';
  out += pad2(*dt.month);
  if (!dt.day) {
    return out;
  }
  out += '-';
  out += pad2(*dt.day);
  if (!dt.hour) {
    return out;
  }
  out += 'T';
  out += pad2(*dt.hour);
  out += ':';
  out += pad2(dt.minute.value_or(0));
  if (dt.second) {
    out += ':';
    out += pad2(*dt.second);
    if (dt.subsecond_ns) {
      std::string ns = std::to_string(*dt.subsecond_ns);
      if (ns.size() < 9) {
        ns.insert(ns.begin(), 9 - ns.size(), '0');
      }
      out += '.';
      out += ns;
    }
  }
  if (dt.utc_offset_minutes) {
    int off = *dt.utc_offset_minutes;
    if (off == 0) {
      out += 'Z';
    } else {
      const char sign = off < 0 ? '-' : '+';
      if (off < 0) {
        off = -off;
      }
      out += sign;
      out += pad2(off / 60);
      out += ':';
      out += pad2(off % 60);
    }
  }
  return out;
}

std::string structureToString(const Structure& fields) {
  std::string out = "{";
  bool first = true;
  for (const auto& [name, value] : fields) {
    if (!first) {
      out += ", ";
    }
    first = false;
    out += name;
    out += ':';
    out += value.toString();
  }
  out += '}';
  return out;
}

}  // namespace

namespace {

constexpr int kMinYear = 1;
constexpr int kMaxYear = 9999;
constexpr int kMaxOffsetMinutes = 18 * 60;

Error invalid_datetime(std::string detail) {
  return Error{ErrorCode::invalid_value, "invalid date-time", "",
               std::move(detail)};
}

bool add_i64(std::int64_t left, std::int64_t right, std::int64_t& out) {
  if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) {
    return false;
  }
  if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right) {
    return false;
  }
  out = left + right;
  return true;
}

bool in_offset_range(int minutes) {
  return minutes >= -kMaxOffsetMinutes && minutes <= kMaxOffsetMinutes;
}

// Finest component present. nullopt means a coarser precision, not zero.
// A finer component without its coarser siblings is invalid.
enum class DatePrecision { year, year_month, date, minute, second, subsecond };

Result<DatePrecision> classify(const DateTime& value) {
  if (value.year < kMinYear || value.year > kMaxYear) {
    return invalid_datetime("year out of range");
  }
  if (value.utc_offset_minutes && !in_offset_range(*value.utc_offset_minutes)) {
    return invalid_datetime("offset outside ±18:00");
  }
  if (value.subsecond_ns &&
      (*value.subsecond_ns < 0 || *value.subsecond_ns > 999'999'999)) {
    return invalid_datetime("sub-second out of range");
  }

  const bool month = value.month.has_value();
  const bool day = value.day.has_value();
  const bool hour = value.hour.has_value();
  const bool minute = value.minute.has_value();
  const bool second = value.second.has_value();
  const bool subsecond = value.subsecond_ns.has_value();

  if (!month) {
    if (day || hour || minute || second || subsecond) {
      return invalid_datetime("partial date-time is missing a coarser field");
    }
    return DatePrecision::year;
  }
  if (*value.month < 1 || *value.month > 12) {
    return invalid_datetime("month out of range");
  }
  if (!day) {
    if (hour || minute || second || subsecond) {
      return invalid_datetime("partial date-time is missing a coarser field");
    }
    return DatePrecision::year_month;
  }

  const auto ymd = std::chrono::year{value.year} /
                   std::chrono::month{static_cast<unsigned>(*value.month)} /
                   std::chrono::day{static_cast<unsigned>(*value.day)};
  if (!ymd.ok()) {
    return invalid_datetime("day is not valid in this month");
  }
  if (!hour) {
    if (minute || second || subsecond) {
      return invalid_datetime("partial date-time is missing a coarser field");
    }
    return DatePrecision::date;
  }
  if (*value.hour < 0 || *value.hour > 23) {
    return invalid_datetime("hour out of range");
  }
  if (!minute) {
    return invalid_datetime("hour without minute");
  }
  if (*value.minute < 0 || *value.minute > 59) {
    return invalid_datetime("minute out of range");
  }
  if (!second) {
    if (subsecond) {
      return invalid_datetime("sub-second without seconds");
    }
    return DatePrecision::minute;
  }
  if (*value.second < 0 || *value.second > 59) {
    return invalid_datetime("second out of range");
  }
  if (!subsecond) {
    return DatePrecision::second;
  }
  return DatePrecision::subsecond;
}

Result<DateTime> apply_day_shift(const DateTime& value, std::int64_t day_delta) {
  const auto ymd = std::chrono::year{value.year} /
                   std::chrono::month{static_cast<unsigned>(*value.month)} /
                   std::chrono::day{static_cast<unsigned>(*value.day)};
  const auto base = std::chrono::sys_days{ymd};
  const auto min_day = std::chrono::sys_days{
      std::chrono::year{kMinYear} / std::chrono::January / std::chrono::day{1}};
  const auto max_day = std::chrono::sys_days{
      std::chrono::year{kMaxYear} / std::chrono::December / std::chrono::day{31}};
  std::int64_t shifted = 0;
  if (!add_i64(base.time_since_epoch().count(), day_delta, shifted) ||
      shifted < min_day.time_since_epoch().count() ||
      shifted > max_day.time_since_epoch().count()) {
    return invalid_datetime("shifted date is out of range");
  }
  const std::chrono::year_month_day out{
      std::chrono::sys_days{std::chrono::days{shifted}}};
  DateTime result = value;
  result.year = static_cast<int>(out.year());
  result.month = static_cast<int>(static_cast<unsigned>(out.month()));
  result.day = static_cast<int>(static_cast<unsigned>(out.day()));
  return result;
}

Result<DateTime> apply_time_shift(const DateTime& value, std::int64_t delta_seconds,
                                  bool keep_seconds) {
  std::int64_t seconds_of_day =
      static_cast<std::int64_t>(*value.hour) * 3600 +
      static_cast<std::int64_t>(*value.minute) * 60;
  if (keep_seconds) {
    seconds_of_day += *value.second;
  }
  std::int64_t total = 0;
  if (!add_i64(seconds_of_day, delta_seconds, total)) {
    return invalid_datetime("shifted time is out of range");
  }
  constexpr std::int64_t kDay = 24 * 60 * 60;
  std::int64_t day_delta = total / kDay;
  std::int64_t tod = total % kDay;
  if (tod < 0) {
    tod += kDay;
    if (!add_i64(day_delta, -1, day_delta)) {
      return invalid_datetime("shifted time is out of range");
    }
  }
  Result<DateTime> dated = apply_day_shift(value, day_delta);
  if (!dated.ok()) {
    return dated.error();
  }
  DateTime result = std::move(dated).value();
  result.hour = static_cast<int>(tod / 3600);
  result.minute = static_cast<int>((tod % 3600) / 60);
  if (keep_seconds) {
    result.second = static_cast<int>(tod % 60);
  }
  return result;
}

}  // namespace

Result<DateTime> shift(const DateTime& value, std::chrono::seconds delta) {
  Result<DatePrecision> kind = classify(value);
  if (!kind.ok()) {
    return kind.error();
  }
  const std::int64_t seconds = delta.count();
  switch (kind.value()) {
    case DatePrecision::year:
    case DatePrecision::year_month:
      if (seconds != 0) {
        return invalid_datetime(
            "year and year-month values reject a non-zero shift");
      }
      return value;
    case DatePrecision::date:
      if (seconds % 86400 != 0) {
        return invalid_datetime("date-only shift must be a whole number of days");
      }
      return apply_day_shift(value, seconds / 86400);
    case DatePrecision::minute:
      if (seconds % 60 != 0) {
        return invalid_datetime(
            "minute-precision shift must be a whole number of minutes");
      }
      return apply_time_shift(value, seconds, false);
    case DatePrecision::second:
    case DatePrecision::subsecond:
      return apply_time_shift(value, seconds, true);
  }
  return invalid_datetime("unrecognized date-time precision");
}

Result<DateTime> withOffset(const DateTime& value, int minutes, OffsetMode mode) {
  if (!in_offset_range(minutes)) {
    return invalid_datetime("offset outside ±18:00");
  }
  Result<DatePrecision> kind = classify(value);
  if (!kind.ok()) {
    return kind.error();
  }
  if (mode == OffsetMode::reinterpret) {
    DateTime result = value;
    result.utc_offset_minutes = minutes;
    return result;
  }
  if (!value.utc_offset_minutes) {
    return invalid_datetime("cannot convert a value with an unknown offset");
  }
  if (kind.value() == DatePrecision::year ||
      kind.value() == DatePrecision::year_month ||
      kind.value() == DatePrecision::date) {
    return invalid_datetime("cannot convert a value with no time component");
  }
  const std::int64_t delta_minutes =
      static_cast<std::int64_t>(minutes) - *value.utc_offset_minutes;
  Result<DateTime> moved = shift(value, std::chrono::seconds{delta_minutes * 60});
  if (!moved.ok()) {
    return moved.error();
  }
  DateTime result = std::move(moved).value();
  result.utc_offset_minutes = minutes;
  return result;
}

bool Value::operator==(const Value& other) const { return data == other.data; }

std::string Value::toString() const {
  return std::visit(
      [](const auto& alt) -> std::string {
        using T = std::decay_t<decltype(alt)>;
        if constexpr (std::is_same_v<T, std::string>) {
          return quote(alt);
        } else if constexpr (std::is_same_v<T, LangAlt>) {
          std::string out = "{";
          bool first = true;
          for (const auto& [lang, text] : alt) {
            if (!first) {
              out += ", ";
            }
            first = false;
            out += lang;
            out += ':';
            out += quote(text);
          }
          out += '}';
          return out;
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
          std::string out = "[";
          bool first = true;
          for (const std::string& item : alt) {
            if (!first) {
              out += ", ";
            }
            first = false;
            out += quote(item);
          }
          out += ']';
          return out;
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          return std::to_string(alt);
        } else if constexpr (std::is_same_v<T, double>) {
          std::ostringstream os;
          os << alt;
          return os.str();
        } else if constexpr (std::is_same_v<T, bool>) {
          return alt ? "true" : "false";
        } else if constexpr (std::is_same_v<T, Rational>) {
          return std::to_string(alt.numerator) + "/" +
                 std::to_string(alt.denominator);
        } else if constexpr (std::is_same_v<T, DateTime>) {
          return dateTimeToString(alt);
        } else if constexpr (std::is_same_v<T, GpsCoordinate>) {
          std::ostringstream os;
          os << alt.latitude << ',' << alt.longitude;
          if (alt.altitude_meters) {
            os << ',' << *alt.altitude_meters << 'm';
          }
          if (alt.gps_time) {
            os << '@' << dateTimeToString(*alt.gps_time);
          }
          return os.str();
        } else if constexpr (std::is_same_v<T, Structure>) {
          return structureToString(alt);
        } else if constexpr (std::is_same_v<T, std::vector<Structure>>) {
          std::string out = "[";
          bool first = true;
          for (const Structure& item : alt) {
            if (!first) {
              out += ", ";
            }
            first = false;
            out += structureToString(item);
          }
          out += ']';
          return out;
        }
      },
      data);
}

}  // namespace umm
