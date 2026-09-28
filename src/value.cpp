#include "umm/value.hpp"

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
