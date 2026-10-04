#include "core/xmp_codec.hpp"

#include "core/property_ids.hpp"
#include "exiftool/json.hpp"
#include "property_registry.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <utility>

namespace umm::internal {
namespace {

std::string ascii_lower(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return out;
}

std::string_view trim(std::string_view text) {
  while (!text.empty() &&
         (text.front() == ' ' || text.front() == '\t' || text.front() == '\n' ||
          text.front() == '\r')) {
    text.remove_prefix(1);
  }
  while (!text.empty() &&
         (text.back() == ' ' || text.back() == '\t' || text.back() == '\n' ||
          text.back() == '\r')) {
    text.remove_suffix(1);
  }
  return text;
}

std::vector<std::string> split_ws(std::string_view text) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() &&
           (text[i] == ' ' || text[i] == '\t' || text[i] == '\n')) {
      ++i;
    }
    if (i >= text.size()) {
      break;
    }
    std::size_t j = i;
    while (j < text.size() && text[j] != ' ' && text[j] != '\t' &&
           text[j] != '\n') {
      ++j;
    }
    out.emplace_back(text.substr(i, j - i));
    i = j;
  }
  return out;
}

std::string pad2(int value) {
  std::string out = std::to_string(value);
  if (out.size() < 2) {
    out.insert(out.begin(), 2 - out.size(), '0');
  }
  return out;
}

std::string pad4(int value) {
  std::string out = std::to_string(value);
  if (out.size() < 4) {
    out.insert(out.begin(), 4 - out.size(), '0');
  }
  return out;
}

void json_escape(std::string& out, std::string_view text) {
  out.push_back('"');
  for (unsigned char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          out += "\\u00";
          const char* hex = "0123456789abcdef";
          out.push_back(hex[c >> 4]);
          out.push_back(hex[c & 0x0f]);
        } else {
          out.push_back(static_cast<char>(c));
        }
        break;
    }
  }
  out.push_back('"');
}

void append_json_value(std::string& out, const Value& value);

void append_json_structure(std::string& out, const Structure& fields) {
  out.push_back('{');
  bool first = true;
  for (const auto& [name, field] : fields) {
    if (!first) {
      out.push_back(',');
    }
    first = false;
    json_escape(out, name);
    out.push_back(':');
    append_json_value(out, field);
  }
  out.push_back('}');
}

void append_json_value(std::string& out, const Value& value) {
  if (const auto* text = std::get_if<std::string>(&value.data)) {
    json_escape(out, *text);
    return;
  }
  if (const auto* alt = std::get_if<LangAlt>(&value.data)) {
    json_escape(out, lang_plain_text(*alt));
    return;
  }
  if (const auto* list = std::get_if<std::vector<std::string>>(&value.data)) {
    out.push_back('[');
    for (std::size_t i = 0; i < list->size(); ++i) {
      if (i != 0) {
        out.push_back(',');
      }
      json_escape(out, (*list)[i]);
    }
    out.push_back(']');
    return;
  }
  if (const auto* nested = std::get_if<Structure>(&value.data)) {
    append_json_structure(out, *nested);
    return;
  }
  if (const auto* items = std::get_if<std::vector<Structure>>(&value.data)) {
    out.push_back('[');
    for (std::size_t i = 0; i < items->size(); ++i) {
      if (i != 0) {
        out.push_back(',');
      }
      append_json_structure(out, (*items)[i]);
    }
    out.push_back(']');
    return;
  }
  if (const auto* i = std::get_if<std::int64_t>(&value.data)) {
    out += std::to_string(*i);
    return;
  }
  if (const auto* d = std::get_if<double>(&value.data)) {
    out += std::to_string(*d);
    return;
  }
  if (const auto* b = std::get_if<bool>(&value.data)) {
    out += *b ? "true" : "false";
    return;
  }
  json_escape(out, {});
}

Value json_to_value(const JsonValue& json) {
  Value value;
  switch (json.kind) {
    case JsonValue::Kind::boolean:
      value.data = json.boolean ? std::string("true") : std::string("false");
      break;
    case JsonValue::Kind::number:
    case JsonValue::Kind::string:
      value.data = json.text;
      break;
    case JsonValue::Kind::array: {
      if (!json.array.empty() &&
          json.array.front().kind == JsonValue::Kind::object) {
        std::vector<Structure> items;
        for (const JsonValue& item : json.array) {
          if (item.kind != JsonValue::Kind::object) {
            continue;
          }
          Structure fields;
          for (const auto& [name, nested] : item.object) {
            fields.emplace(name, json_to_value(nested));
          }
          items.push_back(std::move(fields));
        }
        value.data = std::move(items);
      } else {
        std::vector<std::string> items;
        for (const JsonValue& item : json.array) {
          items.push_back(item.as_text());
        }
        value.data = std::move(items);
      }
      break;
    }
    case JsonValue::Kind::object: {
      Structure fields;
      for (const auto& [name, nested] : json.object) {
        fields.emplace(name, json_to_value(nested));
      }
      value.data = std::move(fields);
      break;
    }
    case JsonValue::Kind::null:
      value.data = std::string();
      break;
  }
  return value;
}

std::optional<Structure> structure_from_json_object(const JsonValue& json) {
  if (json.kind != JsonValue::Kind::object) {
    return std::nullopt;
  }
  Structure fields;
  for (const auto& [name, nested] : json.object) {
    fields.emplace(name, json_to_value(nested));
  }
  if (fields.empty()) {
    return std::nullopt;
  }
  return fields;
}

std::optional<Structure> parse_exiftool_braces(std::string_view text) {
  text = trim(text);
  if (text.size() < 2 || text.front() != '{' || text.back() != '}') {
    return std::nullopt;
  }
  if (text.size() >= 3 && text[1] == '"') {
    return std::nullopt;
  }
  Structure fields;
  std::string_view rest = text.substr(1, text.size() - 2);
  while (!rest.empty()) {
    rest = trim(rest);
    if (rest.empty()) {
      break;
    }
    const auto eq = rest.find('=');
    if (eq == std::string_view::npos) {
      return std::nullopt;
    }
    const std::string key(trim(rest.substr(0, eq)));
    rest.remove_prefix(eq + 1);
    rest = trim(rest);
    std::string value;
    if (!rest.empty() && rest.front() == '"') {
      rest.remove_prefix(1);
      while (!rest.empty() && rest.front() != '"') {
        value.push_back(rest.front());
        rest.remove_prefix(1);
      }
      if (!rest.empty() && rest.front() == '"') {
        rest.remove_prefix(1);
      }
    } else {
      std::size_t depth = 0;
      std::size_t i = 0;
      for (; i < rest.size(); ++i) {
        if (rest[i] == '{') {
          ++depth;
        } else if (rest[i] == '}') {
          if (depth == 0) {
            break;
          }
          --depth;
        } else if (rest[i] == ',' && depth == 0) {
          break;
        }
      }
      value = std::string(trim(rest.substr(0, i)));
      rest.remove_prefix(i);
    }
    rest = trim(rest);
    if (!rest.empty() && rest.front() == ',') {
      rest.remove_prefix(1);
    }
    if (!key.empty() && !value.empty()) {
      Value field;
      field.data = value;
      fields.insert_or_assign(key, std::move(field));
    }
  }
  if (fields.empty()) {
    return std::nullopt;
  }
  return fields;
}

bool parse_rational_token(std::string_view token, double& out) {
  if (token.empty()) {
    return false;
  }
  const auto slash = token.find('/');
  if (slash == std::string_view::npos) {
    char* end = nullptr;
    const std::string text(token);
    out = std::strtod(text.c_str(), &end);
    return end != text.c_str() && std::isfinite(out);
  }
  double num = 0;
  double den = 0;
  if (!parse_rational_token(token.substr(0, slash), num) ||
      !parse_rational_token(token.substr(slash + 1), den) || den == 0) {
    return false;
  }
  out = num / den;
  return std::isfinite(out);
}

char gps_hemi_from_token(std::string_view token) {
  const std::string lower = ascii_lower(token);
  if (lower == "n" || lower == "north") {
    return 'N';
  }
  if (lower == "s" || lower == "south") {
    return 'S';
  }
  if (lower == "e" || lower == "east") {
    return 'E';
  }
  if (lower == "w" || lower == "west") {
    return 'W';
  }
  return 0;
}

char take_gps_hemisphere(std::string& text) {
  text = trim(text);
  if (text.empty()) {
    return 0;
  }
  const auto space = text.find_last_of(" \t");
  if (space != std::string::npos) {
    if (const char word = gps_hemi_from_token(text.substr(space + 1))) {
      text.resize(space);
      text = std::string(trim(text));
      return word;
    }
  }
  const char last = static_cast<char>(
      std::toupper(static_cast<unsigned char>(text.back())));
  if (last == 'N' || last == 'S' || last == 'E' || last == 'W') {
    if (text.size() == 1 ||
        (text[text.size() - 2] < 'A' || text[text.size() - 2] > 'z')) {
      text.pop_back();
      text = std::string(trim(text));
      return last;
    }
  }
  return 0;
}

int gps_hemi_sign(char hemi) {
  if (hemi == 'S' || hemi == 'W') {
    return -1;
  }
  return 1;
}

std::optional<double> parse_gps_coord_impl(std::string_view text) {
  std::string s(trim(text));
  const char hemi = take_gps_hemisphere(s);
  if (s.empty()) {
    return std::nullopt;
  }
  std::vector<double> parts;
  std::string token;
  auto flush = [&] {
    if (token.empty()) {
      return;
    }
    double value = 0;
    if (parse_rational_token(token, value)) {
      parts.push_back(value);
    }
    token.clear();
  };
  for (char c : s) {
    if ((c >= '0' && c <= '9') || c == '.' || c == '/' || c == '-' ||
        c == '+') {
      token.push_back(c);
    } else {
      flush();
    }
  }
  flush();
  if (parts.empty()) {
    return std::nullopt;
  }
  double deg = parts[0];
  if (parts.size() >= 2) {
    deg += parts[1] / 60.0;
  }
  if (parts.size() >= 3) {
    deg += parts[2] / 3600.0;
  }
  if (deg < 0) {
    return deg;
  }
  return deg * static_cast<double>(gps_hemi_sign(hemi));
}

std::optional<double> as_gps_number(const Value& value) {
  if (const auto* d = std::get_if<double>(&value.data)) {
    return *d;
  }
  if (const auto* i = std::get_if<std::int64_t>(&value.data)) {
    return static_cast<double>(*i);
  }
  if (const auto* s = std::get_if<std::string>(&value.data)) {
    return parse_gps_coord_impl(*s);
  }
  return std::nullopt;
}

std::optional<double> as_altitude_number(const Value& value) {
  if (const auto* d = std::get_if<double>(&value.data)) {
    return *d;
  }
  if (const auto* i = std::get_if<std::int64_t>(&value.data)) {
    return static_cast<double>(*i);
  }
  if (const auto* s = std::get_if<std::string>(&value.data)) {
    std::string text(trim(*s));
    if (text.empty()) {
      return std::nullopt;
    }
    const std::string lower = ascii_lower(text);
    bool below = lower.find("below") != std::string::npos;
    if (!text.empty() && (text.back() == 'm' || text.back() == 'M')) {
      text.pop_back();
      text = std::string(trim(text));
    }
    std::string numeric;
    for (char c : text) {
      if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' ||
          c == '/') {
        numeric.push_back(c);
      } else if (!numeric.empty()) {
        break;
      }
    }
    double meters = 0;
    if (!parse_rational_token(numeric, meters)) {
      return std::nullopt;
    }
    if (below && meters > 0) {
      meters = -meters;
    }
    return meters;
  }
  return std::nullopt;
}

std::optional<std::int64_t> as_altitude_ref(const Value& value) {
  if (const auto* i = std::get_if<std::int64_t>(&value.data)) {
    return *i == 0 ? 0 : 1;
  }
  if (const auto* d = std::get_if<double>(&value.data)) {
    return *d == 0 ? 0 : 1;
  }
  if (const auto* s = std::get_if<std::string>(&value.data)) {
    const std::string lower = ascii_lower(trim(*s));
    if (lower.empty()) {
      return std::nullopt;
    }
    if (lower == "0" || lower.find("above") != std::string::npos) {
      return 0;
    }
    if (lower == "1" || lower.find("below") != std::string::npos) {
      return 1;
    }
    double n = 0;
    if (parse_rational_token(lower, n)) {
      return n == 0 ? 0 : 1;
    }
  }
  return std::nullopt;
}

std::string_view location_id_suffix(std::string_view id) {
  const auto dot = id.rfind('.');
  if (dot == std::string_view::npos) {
    return id;
  }
  return id.substr(dot + 1);
}

std::string location_canonical_field(std::string_view name) {
  std::string field(name);
  const auto colon = field.rfind(':');
  if (colon != std::string::npos) {
    field = field.substr(colon + 1);
  }
  const std::string lower = ascii_lower(field);
  constexpr std::string_view kCreated = "locationcreated";
  constexpr std::string_view kShown = "locationshown";
  if (lower.size() > kCreated.size() && lower.rfind(kCreated, 0) == 0) {
    field = field.substr(kCreated.size());
  } else if (lower.size() > kShown.size() && lower.rfind(kShown, 0) == 0) {
    field = field.substr(kShown.size());
  }
  const std::string key = ascii_lower(field);
  std::string photo;
  std::string video;
  for (const StructFieldRepresentation& row : kStructFieldRepresentations) {
    if (row.struct_name != "Location") {
      continue;
    }
    const std::string_view suffix = location_id_suffix(row.id);
    bool match = ascii_lower(row.et_tag) == key || ascii_lower(suffix) == key;
    if (!match) {
      const auto ns = row.xmp_property.rfind(':');
      const std::string_view local =
          ns == std::string_view::npos ? row.xmp_property
                                       : row.xmp_property.substr(ns + 1);
      match = ascii_lower(local) == key;
    }
    if (!match) {
      continue;
    }
    if (row.id.rfind("iptc.photo.struct.Location.", 0) == 0 && photo.empty()) {
      photo = std::string(suffix);
    } else if (row.id.rfind("iptc.video.struct.Location.", 0) == 0 &&
               video.empty()) {
      video = std::string(suffix);
    }
  }
  if (!photo.empty()) {
    return photo;
  }
  if (!video.empty()) {
    return video;
  }
  return std::string(name);
}

std::string location_et_tag(std::string_view canonical) {
  const std::string key = ascii_lower(canonical);
  for (const StructFieldRepresentation& row : kStructFieldRepresentations) {
    if (row.struct_name != "Location") {
      continue;
    }
    if (row.id.rfind("iptc.photo.struct.Location.", 0) != 0) {
      continue;
    }
    if (ascii_lower(location_id_suffix(row.id)) == key) {
      return std::string(row.et_tag);
    }
  }
  return std::string(canonical);
}

Value make_double(double n) {
  Value value;
  value.data = n;
  return value;
}

Value make_int(std::int64_t n) {
  Value value;
  value.data = n;
  return value;
}

Value make_text(std::string text) {
  Value value;
  value.data = std::move(text);
  return value;
}

Structure decode_location_fields(const Structure& fields) {
  Structure out;
  for (const auto& [name, value] : fields) {
    out.insert_or_assign(location_canonical_field(name), value);
  }
  if (const auto it = out.find("gpsLatitude"); it != out.end()) {
    if (const auto n = as_gps_number(it->second)) {
      it->second = make_double(*n);
    }
  }
  if (const auto it = out.find("gpsLongitude"); it != out.end()) {
    if (const auto n = as_gps_number(it->second)) {
      it->second = make_double(*n);
    }
  }
  if (const auto it = out.find("gpsAltitude"); it != out.end()) {
    if (const auto n = as_altitude_number(it->second)) {
      it->second = make_double(*n);
    }
  }
  if (const auto it = out.find("gpsAltitudeRef"); it != out.end()) {
    if (const auto n = as_altitude_ref(it->second)) {
      it->second = make_int(*n);
    }
  }
  return out;
}

std::string format_gps_coord_impl(double degrees, bool longitude) {
  if (!std::isfinite(degrees)) {
    return {};
  }
  const char hemi =
      longitude ? (degrees < 0 ? 'W' : 'E') : (degrees < 0 ? 'S' : 'N');
  std::ostringstream oss;
  oss << std::setprecision(10) << std::fabs(degrees) << hemi;
  return oss.str();
}

std::string format_gps_altitude_impl(double meters) {
  std::ostringstream oss;
  oss << std::setprecision(15) << meters;
  return oss.str();
}

Value encode_location_field(std::string_view canonical, const Value& value) {
  if (canonical == "gpsLatitude" || canonical == "gpsLongitude") {
    if (const auto n = as_gps_number(value)) {
      return make_text(
          format_gps_coord_impl(*n, canonical == "gpsLongitude"));
    }
  }
  if (canonical == "gpsAltitude") {
    if (const auto n = as_altitude_number(value)) {
      return make_text(format_gps_altitude_impl(*n));
    }
  }
  if (canonical == "gpsAltitudeRef") {
    if (const auto n = as_altitude_ref(value)) {
      return make_text(std::to_string(*n));
    }
  }
  if (canonical == "identifiers") {
    if (const auto* list = std::get_if<std::vector<std::string>>(&value.data)) {
      if (!list->empty()) {
        return make_text(list->front());
      }
    }
  }
  return value;
}

std::string quote_exiftool_value(std::string_view text) {
  if (text.size() >= 2 && text.front() == '{' && text.back() == '}') {
    return std::string(text);
  }
  bool quote = false;
  for (char c : text) {
    if (c == ',' || c == '"' || c == '=') {
      quote = true;
      break;
    }
  }
  if (!quote) {
    return std::string(text);
  }
  std::string out = "\"";
  out += text;
  out += '"';
  return out;
}

std::string qt_suffix_tag(std::string_view suffix) {
  const std::string lower = ascii_lower(suffix);
  if (lower == "title") {
    return "Title";
  }
  if (lower == "description") {
    return "Description";
  }
  if (lower == "copyright") {
    return "Copyright";
  }
  if (lower == "creationdate") {
    return "CreationDate";
  }
  if (lower == "keywords" || lower == "keyword") {
    return "Keywords";
  }
  if (lower == "artist") {
    return "Artist";
  }
  if (lower == "author") {
    return "Author";
  }
  if (lower == "director") {
    return "Director";
  }
  if (lower == "genre") {
    return "Genre";
  }
  if (lower == "publisher") {
    return "Publisher";
  }
  if (lower == "gpscoordinates" || lower == "location.iso6709") {
    return "GPSCoordinates";
  }
  return {};
}

}  // namespace

std::string xmp_base_key(std::string_view property) {
  const auto colon = property.find(':');
  if (colon == std::string_view::npos) {
    return "Xmp." + std::string(property);
  }
  std::string ns(property.substr(0, colon));
  std::string name(property.substr(colon + 1));
  if (ascii_lower(ns) == "dc") {
    name = ascii_lower(name);
  }
  return "Xmp." + ns + "." + name;
}

std::vector<std::string> xmp_base_keys(std::string_view property) {
  std::vector<std::string> out;
  for (const std::string& token : split_ws(property)) {
    if (token.find(':') == std::string::npos && token.find('.') == std::string::npos) {
      continue;
    }
    out.push_back(xmp_base_key(token));
  }
  return out;
}

std::vector<std::string> quicktime_base_keys(std::string_view registry_key) {
  const std::string_view text = trim(registry_key);
  if (text.empty()) {
    return {};
  }
  const std::string lower = ascii_lower(text);
  if (lower.rfind("see ", 0) == 0 || text.find('=') != std::string_view::npos ||
      text.find('"') != std::string_view::npos) {
    return {};
  }
  std::vector<std::string> out;
  constexpr std::string_view kPrefix = "com.apple.quicktime.";
  for (const std::string& token : split_ws(text)) {
    const std::string token_lower = ascii_lower(token);
    if (token_lower.rfind(std::string(kPrefix), 0) != 0) {
      continue;
    }
    const std::string tag = qt_suffix_tag(token.substr(kPrefix.size()));
    if (tag.empty()) {
      continue;
    }
    out.push_back("QuickTime.Keys." + tag);
  }
  return out;
}

std::string lang_plain_text(const LangAlt& alt) {
  const auto it = alt.find("x-default");
  if (it != alt.end()) {
    return it->second;
  }
  if (alt.size() == 1) {
    return alt.begin()->second;
  }
  return {};
}

std::string format_xmp_datetime(const DateTime& dt) {
  std::string out = pad4(dt.year);
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
  if (!dt.hour || !dt.minute) {
    return out;
  }
  out += 'T';
  out += pad2(*dt.hour);
  out += ':';
  out += pad2(*dt.minute);
  out += ':';
  out += pad2(dt.second.value_or(0));
  if (dt.subsecond_ns) {
    int ns = *dt.subsecond_ns;
    if (ns < 0) {
      ns = 0;
    }
    std::string frac = std::to_string(ns);
    while (frac.size() < 9) {
      frac.insert(frac.begin(), '0');
    }
    while (frac.size() > 1 && frac.back() == '0') {
      frac.pop_back();
    }
    out += '.';
    out += frac;
  }
  if (dt.utc_offset_minutes) {
    int minutes = *dt.utc_offset_minutes;
    if (minutes == 0) {
      out += 'Z';
    } else {
      const char sign = minutes < 0 ? '-' : '+';
      if (minutes < 0) {
        minutes = -minutes;
      }
      out += sign;
      out += pad2(minutes / 60);
      out += ':';
      out += pad2(minutes % 60);
    }
  }
  return out;
}

std::string encode_structure_json(const Structure& fields) {
  std::string out;
  append_json_structure(out, fields);
  return out;
}

std::string encode_exiftool_struct(const Structure& fields) {
  std::string out = "{";
  bool first = true;
  for (const auto& [name, field] : fields) {
    std::string text;
    if (const auto* s = std::get_if<std::string>(&field.data)) {
      text = *s;
    } else if (const auto* alt = std::get_if<LangAlt>(&field.data)) {
      text = lang_plain_text(*alt);
    } else if (const auto* nested = std::get_if<Structure>(&field.data)) {
      text = encode_exiftool_struct(*nested);
    } else if (const auto* i = std::get_if<std::int64_t>(&field.data)) {
      text = std::to_string(*i);
    } else if (const auto* d = std::get_if<double>(&field.data)) {
      std::ostringstream oss;
      oss << std::setprecision(15) << *d;
      text = oss.str();
    } else if (const auto* list =
                   std::get_if<std::vector<std::string>>(&field.data)) {
      if (list->empty()) {
        continue;
      }
      text = list->front();
    } else {
      continue;
    }
    if (text.empty()) {
      continue;
    }
    if (!first) {
      out.push_back(',');
    }
    first = false;
    out += name;
    out.push_back('=');
    out += quote_exiftool_value(text);
  }
  out.push_back('}');
  return out;
}

std::optional<Structure> decode_structure_text(std::string_view text) {
  text = trim(text);
  if (text.empty()) {
    return std::nullopt;
  }
  if (text.front() == '{') {
    std::string error;
    if (const auto json = parse_json(text, &error)) {
      if (auto fields = structure_from_json_object(*json)) {
        return fields;
      }
    }
    return parse_exiftool_braces(text);
  }
  return std::nullopt;
}

std::optional<std::vector<Structure>> decode_structure_list_text(
    std::string_view text) {
  text = trim(text);
  if (text.empty()) {
    return std::nullopt;
  }
  if (text.front() == '[') {
    std::string error;
    const auto json = parse_json(text, &error);
    if (!json || json->kind != JsonValue::Kind::array) {
      return std::nullopt;
    }
    std::vector<Structure> items;
    for (const JsonValue& item : json->array) {
      if (auto fields = structure_from_json_object(item)) {
        items.push_back(std::move(*fields));
      } else if (item.kind == JsonValue::Kind::string && !item.text.empty()) {
        items.push_back(structure_from_uri(item.text));
      }
    }
    if (items.empty()) {
      return std::nullopt;
    }
    return items;
  }
  if (auto one = decode_structure_text(text)) {
    return std::vector<Structure>{std::move(*one)};
  }
  return std::nullopt;
}

bool structure_is_uri_like(const Structure& fields) {
  if (fields.size() != 1) {
    return false;
  }
  return !uri_from_structure(fields).empty();
}

std::string uri_from_structure(const Structure& fields) {
  static constexpr std::string_view kKeys[] = {
      "cvId", "CvId", "cvTermId", "CvTermId", "identifier", "Identifier",
      "uri",  "URI",  "id",      "Id"};
  for (std::string_view name : kKeys) {
    const auto it = fields.find(std::string(name));
    if (it == fields.end()) {
      continue;
    }
    if (const auto* text = std::get_if<std::string>(&it->second.data)) {
      if (!text->empty()) {
        return *text;
      }
    }
  }
  return {};
}

Structure structure_from_uri(std::string_view uri) {
  Structure fields;
  Value value;
  value.data = std::string(uri);
  fields.emplace("cvId", std::move(value));
  return fields;
}

std::string structure_display_name(const Structure& fields) {
  // Lookup order covers IPTC logical field `name` and TR etTag names
  // (PersonName, ProductName, PLUS *Name). Encoding aliases are generated.
  static constexpr std::string_view kKeys[] = {
      "name",
      "Name",
      "PersonName",
      "OrganisationName",
      "CopyrightOwnerName",
      "ImageSupplierName",
      "LicensorName",
      "ProductName"};
  for (std::string_view name : kKeys) {
    const auto it = fields.find(std::string(name));
    if (it == fields.end()) {
      continue;
    }
    if (const auto* text = std::get_if<std::string>(&it->second.data)) {
      if (!text->empty()) {
        return *text;
      }
    }
    if (const auto* alt = std::get_if<LangAlt>(&it->second.data)) {
      const std::string plain = lang_plain_text(*alt);
      if (!plain.empty()) {
        return plain;
      }
    }
  }
  return {};
}

bool is_photo_location_id(std::string_view id) {
  return id == kLocation || id == kLocationShown;
}

std::optional<double> parse_gps_coord(std::string_view text) {
  return parse_gps_coord_impl(text);
}

std::string format_gps_coord(double degrees, bool longitude) {
  return format_gps_coord_impl(degrees, longitude);
}

Structure canonicalize_location_struct(const Structure& fields) {
  return decode_location_fields(fields);
}

Structure encode_location_struct_fields(const Structure& fields) {
  Structure out;
  for (const auto& [name, value] : fields) {
    const std::string canonical = location_canonical_field(name);
    out.insert_or_assign(location_et_tag(canonical),
                         encode_location_field(canonical, value));
  }
  return out;
}

void decode_location_value(Value& value) {
  if (auto* list = std::get_if<std::vector<Structure>>(&value.data)) {
    for (Structure& item : *list) {
      item = decode_location_fields(item);
    }
    return;
  }
  if (auto* fields = std::get_if<Structure>(&value.data)) {
    *fields = decode_location_fields(*fields);
  }
}

std::vector<std::string_view> mapped_video_property_ids() {
  std::vector<std::string_view> ids;
  for (const PropertyDef& def : kProperties) {
    if (def.id.rfind("iptc.video.", 0) == 0) {
      ids.push_back(def.id);
    }
  }
  return ids;
}

std::vector<std::string_view> mapped_photo_property_ids() {
  std::vector<std::string_view> ids;
  for (const PropertyDef& def : kProperties) {
    if (def.id.rfind("iptc.photo.", 0) == 0) {
      ids.push_back(def.id);
    }
  }
  return ids;
}

}  // namespace umm::internal
