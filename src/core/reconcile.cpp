#include "core/reconcile.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/property_ids.hpp"
#include "umm/registry.hpp"

namespace umm::internal {
namespace {

constexpr double kGpsDegEps = 1e-5;
constexpr double kGpsAltEps = 0.5;

Value make_value(auto payload) {
  Value value;
  value.data = std::move(payload);
  return value;
}

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

std::string trimmed(std::string_view text) { return std::string(trim(text)); }

bool key_belongs(std::string_view entry_key, std::string_view base) {
  if (entry_key == base) {
    return true;
  }
  if (entry_key.size() <= base.size()) {
    return false;
  }
  if (entry_key.substr(0, base.size()) != base) {
    return false;
  }
  const char next = entry_key[base.size()];
  return next == '[' || next == '/';
}

std::string_view last_field(std::string_view key) {
  const auto slash = key.rfind('/');
  std::string_view tail =
      slash == std::string_view::npos ? key : key.substr(slash + 1);
  const auto colon = tail.rfind(':');
  if (colon != std::string_view::npos) {
    return tail.substr(colon + 1);
  }
  const auto dot = tail.rfind('.');
  if (dot != std::string_view::npos) {
    return tail.substr(dot + 1);
  }
  const auto bracket = tail.find('[');
  if (bracket != std::string_view::npos) {
    return tail.substr(0, bracket);
  }
  return tail;
}

bool parse_i32(std::string_view text, int& out) {
  text = trim(text);
  if (text.empty()) {
    return false;
  }
  std::size_t idx = 0;
  int sign = 1;
  if (text[0] == '+' || text[0] == '-') {
    sign = text[0] == '-' ? -1 : 1;
    idx = 1;
  }
  if (idx >= text.size()) {
    return false;
  }
  int value = 0;
  bool any = false;
  for (; idx < text.size(); ++idx) {
    const char c = text[idx];
    if (c < '0' || c > '9') {
      return false;
    }
    any = true;
    value = value * 10 + (c - '0');
  }
  if (!any) {
    return false;
  }
  out = value * sign;
  return true;
}

bool parse_double(std::string_view text, double& out) {
  text = trim(text);
  if (text.empty()) {
    return false;
  }
  try {
    std::size_t n = 0;
    const double value = std::stod(std::string(text), &n);
    if (n == 0) {
      return false;
    }
    out = value;
    return true;
  } catch (...) {
    return false;
  }
}

bool parse_rational(std::string_view text, double& out) {
  text = trim(text);
  const auto slash = text.find('/');
  if (slash == std::string_view::npos) {
    return parse_double(text, out);
  }
  double num = 0;
  double den = 0;
  if (!parse_double(text.substr(0, slash), num) ||
      !parse_double(text.substr(slash + 1), den) || den == 0) {
    return false;
  }
  out = num / den;
  return true;
}

int subsec_to_ns(std::string_view digits) {
  digits = trim(digits);
  std::string padded(digits);
  if (padded.size() > 9) {
    padded.resize(9);
  }
  while (padded.size() < 9) {
    padded.push_back('0');
  }
  int ns = 0;
  (void)parse_i32(padded, ns);
  return ns;
}

bool parse_offset(std::string_view text, int& minutes) {
  text = trim(text);
  if (text.empty()) {
    return false;
  }
  if (text == "Z" || text == "z") {
    minutes = 0;
    return true;
  }
  int sign = 1;
  if (text.front() == '+') {
    text.remove_prefix(1);
  } else if (text.front() == '-') {
    sign = -1;
    text.remove_prefix(1);
  } else {
    return false;
  }
  int hour = 0;
  int minute = 0;
  if (text.size() >= 5 && text[2] == ':') {
    if (!parse_i32(text.substr(0, 2), hour) ||
        !parse_i32(text.substr(3, 2), minute)) {
      return false;
    }
  } else if (text.size() >= 4) {
    if (!parse_i32(text.substr(0, 2), hour) ||
        !parse_i32(text.substr(2, 2), minute)) {
      return false;
    }
  } else if (text.size() >= 2) {
    if (!parse_i32(text.substr(0, 2), hour)) {
      return false;
    }
  } else {
    return false;
  }
  minutes = sign * (hour * 60 + minute);
  return true;
}

bool parse_time_only(std::string_view text, DateTime& out) {
  text = trim(text);
  if (text.size() >= 6 && text.find(':') == std::string_view::npos) {
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (!parse_i32(text.substr(0, 2), hour) ||
        !parse_i32(text.substr(2, 2), minute) ||
        !parse_i32(text.substr(4, 2), second)) {
      return false;
    }
    out.hour = hour;
    out.minute = minute;
    out.second = second;
    if (text.size() > 6) {
      int off = 0;
      if (parse_offset(text.substr(6), off)) {
        out.utc_offset_minutes = off;
      }
    }
    return true;
  }
  if (text.size() < 5 || text[2] != ':') {
    return false;
  }
  int hour = 0;
  int minute = 0;
  if (!parse_i32(text.substr(0, 2), hour) ||
      !parse_i32(text.substr(3, 2), minute)) {
    return false;
  }
  out.hour = hour;
  out.minute = minute;
  std::size_t pos = 5;
  if (text.size() > pos && text[pos] == ':') {
    int second = 0;
    if (!parse_i32(text.substr(pos + 1, 2), second)) {
      return false;
    }
    out.second = second;
    pos += 3;
    if (pos < text.size() && text[pos] == '.') {
      std::size_t end = pos + 1;
      while (end < text.size() && text[end] >= '0' && text[end] <= '9') {
        ++end;
      }
      out.subsecond_ns = subsec_to_ns(text.substr(pos + 1, end - pos - 1));
      pos = end;
    }
  }
  if (pos < text.size()) {
    int off = 0;
    if (parse_offset(text.substr(pos), off)) {
      out.utc_offset_minutes = off;
    }
  }
  return true;
}

bool parse_datetime(std::string_view text, DateTime& out) {
  text = trim(text);
  if (text.empty()) {
    return false;
  }
  DateTime dt;
  std::string_view rest = text;
  if (rest.size() >= 8 && rest.find('-') == std::string_view::npos &&
      rest.find(':') == std::string_view::npos && rest[0] >= '0' &&
      rest[0] <= '9') {
    int year = 0;
    int month = 0;
    int day = 0;
    if (!parse_i32(rest.substr(0, 4), year) ||
        !parse_i32(rest.substr(4, 2), month) ||
        !parse_i32(rest.substr(6, 2), day)) {
      return parse_time_only(text, out);
    }
    dt.year = year;
    dt.month = month;
    dt.day = day;
    rest.remove_prefix(8);
  } else {
    if (rest.size() < 4) {
      return parse_time_only(text, out);
    }
    int year = 0;
    if (!parse_i32(rest.substr(0, 4), year)) {
      return parse_time_only(text, out);
    }
    dt.year = year;
    rest.remove_prefix(4);
    if (rest.size() >= 3 && (rest[0] == '-' || rest[0] == ':')) {
      int month = 0;
      if (!parse_i32(rest.substr(1, 2), month)) {
        return false;
      }
      dt.month = month;
      rest.remove_prefix(3);
      if (rest.size() >= 3 && (rest[0] == '-' || rest[0] == ':')) {
        int day = 0;
        if (!parse_i32(rest.substr(1, 2), day)) {
          return false;
        }
        dt.day = day;
        rest.remove_prefix(3);
      }
    }
  }
  rest = trim(rest);
  if (!rest.empty() && (rest.front() == 'T' || rest.front() == 't' ||
                        rest.front() == ' ')) {
    rest.remove_prefix(1);
  }
  rest = trim(rest);
  if (!rest.empty()) {
    DateTime time;
    if (!parse_time_only(rest, time)) {
      return false;
    }
    dt.hour = time.hour;
    dt.minute = time.minute;
    dt.second = time.second;
    dt.subsecond_ns = time.subsecond_ns;
    dt.utc_offset_minutes = time.utc_offset_minutes;
  }
  out = dt;
  return true;
}

void apply_subsec_offset(DateTime& dt, std::string_view subsec,
                         std::string_view offset) {
  if (!trim(subsec).empty()) {
    dt.subsecond_ns = subsec_to_ns(subsec);
  }
  int minutes = 0;
  if (parse_offset(offset, minutes)) {
    dt.utc_offset_minutes = minutes;
  }
}

bool opt_equal(const std::optional<int>& a, const std::optional<int>& b) {
  if (!a || !b) {
    return true;
  }
  return *a == *b;
}

bool datetime_equivalent(const DateTime& a, const DateTime& b) {
  return a.year == b.year && opt_equal(a.month, b.month) &&
         opt_equal(a.day, b.day) && opt_equal(a.hour, b.hour) &&
         opt_equal(a.minute, b.minute) && opt_equal(a.second, b.second) &&
         opt_equal(a.subsecond_ns, b.subsecond_ns) &&
         opt_equal(a.utc_offset_minutes, b.utc_offset_minutes);
}

DateTime datetime_merge(DateTime a, const DateTime& b) {
  if (!a.month) {
    a.month = b.month;
  }
  if (!a.day) {
    a.day = b.day;
  }
  if (!a.hour) {
    a.hour = b.hour;
  }
  if (!a.minute) {
    a.minute = b.minute;
  }
  if (!a.second) {
    a.second = b.second;
  }
  if (!a.subsecond_ns) {
    a.subsecond_ns = b.subsecond_ns;
  }
  if (!a.utc_offset_minutes) {
    a.utc_offset_minutes = b.utc_offset_minutes;
  }
  return a;
}

char hemi_from_token(std::string_view text) {
  const std::string lower = ascii_lower(trim(text));
  if (lower.empty()) {
    return 0;
  }
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

char take_hemisphere(std::string& text) {
  text = trimmed(text);
  if (text.empty()) {
    return 0;
  }
  const auto space = text.find_last_of(" \t");
  if (space != std::string::npos) {
    if (const char word = hemi_from_token(text.substr(space + 1))) {
      text.resize(space);
      text = trimmed(text);
      return word;
    }
  }
  const char last = static_cast<char>(
      std::toupper(static_cast<unsigned char>(text.back())));
  if (last == 'N' || last == 'S' || last == 'E' || last == 'W') {
    if (text.size() == 1 ||
        (text[text.size() - 2] < 'A' || text[text.size() - 2] > 'z')) {
      text.pop_back();
      text = trimmed(text);
      return last;
    }
  }
  return 0;
}

int hemi_sign(char hemi) {
  if (hemi == 'S' || hemi == 'W') {
    return -1;
  }
  return 1;
}

bool parse_coord(std::string_view text, bool longitude, double& out) {
  std::string s(trim(text));
  const char hemi = take_hemisphere(s);
  if (s.empty()) {
    return false;
  }
  std::vector<double> parts;
  std::string token;
  auto flush = [&] {
    if (token.empty()) {
      return;
    }
    double value = 0;
    if (parse_rational(token, value)) {
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
    return false;
  }
  double deg = parts[0];
  if (parts.size() >= 2) {
    deg += parts[1] / 60.0;
  }
  if (parts.size() >= 3) {
    deg += parts[2] / 3600.0;
  }
  if (deg < 0) {
    out = deg;
    return true;
  }
  out = deg * static_cast<double>(hemi_sign(hemi));
  (void)longitude;
  return true;
}

bool parse_altitude(std::string_view text, std::string_view ref,
                    std::optional<double>& out) {
  std::string s(trim(text));
  if (s.empty()) {
    return false;
  }
  if (!s.empty() && (s.back() == 'm' || s.back() == 'M')) {
    s.pop_back();
    s = trimmed(s);
  }
  double meters = 0;
  if (!parse_rational(s, meters)) {
    return false;
  }
  const std::string r = ascii_lower(trim(ref));
  if (r == "1" || r.find("below") != std::string::npos) {
    meters = -meters;
  }
  out = meters;
  return true;
}

bool parse_qt_gps(std::string_view text, GpsCoordinate& gps) {
  std::string s(trim(text));
  if (s.empty()) {
    return false;
  }
  std::vector<std::string> comma_parts;
  std::string part;
  auto flush_part = [&] {
    const std::string item = trimmed(part);
    if (!item.empty()) {
      comma_parts.push_back(item);
    }
    part.clear();
  };
  for (char c : s) {
    if (c == ',') {
      flush_part();
    } else {
      part.push_back(c);
    }
  }
  flush_part();
  if (comma_parts.size() >= 2) {
    if (parse_coord(comma_parts[0], false, gps.latitude) &&
        parse_coord(comma_parts[1], true, gps.longitude)) {
      if (comma_parts.size() >= 3) {
        std::string alt = comma_parts[2];
        const auto space = alt.find_first_of(" \t");
        if (space != std::string::npos) {
          alt.resize(space);
        }
        parse_altitude(alt, comma_parts.size() >= 4 ? comma_parts[3] : "",
                       gps.altitude_meters);
      }
      return true;
    }
  }
  for (char& c : s) {
    if (c == ',') {
      c = ' ';
    }
  }
  std::vector<std::string> tokens;
  std::string token;
  for (char c : s) {
    if (c == ' ' || c == '\t') {
      if (!token.empty()) {
        tokens.push_back(token);
        token.clear();
      }
    } else {
      token.push_back(c);
    }
  }
  if (!token.empty()) {
    tokens.push_back(token);
  }
  if (tokens.size() < 2) {
    return false;
  }
  if (!parse_coord(tokens[0], false, gps.latitude) ||
      !parse_coord(tokens[1], true, gps.longitude)) {
    return false;
  }
  if (tokens.size() >= 3) {
    parse_altitude(tokens[2], "", gps.altitude_meters);
  }
  return true;
}

bool gps_equivalent(const GpsCoordinate& a, const GpsCoordinate& b) {
  if (std::fabs(a.latitude - b.latitude) > kGpsDegEps ||
      std::fabs(a.longitude - b.longitude) > kGpsDegEps) {
    return false;
  }
  if (a.altitude_meters && b.altitude_meters) {
    if (std::fabs(*a.altitude_meters - *b.altitude_meters) > kGpsAltEps) {
      return false;
    }
  }
  if (a.gps_time && b.gps_time &&
      !datetime_equivalent(*a.gps_time, *b.gps_time)) {
    return false;
  }
  return true;
}

GpsCoordinate gps_merge(GpsCoordinate a, const GpsCoordinate& b) {
  if (!a.altitude_meters) {
    a.altitude_meters = b.altitude_meters;
  }
  if (!a.gps_time) {
    a.gps_time = b.gps_time;
  }
  return a;
}

LangAlt parse_lang_alt(std::string_view text) {
  LangAlt alt;
  const std::string_view t = trim(text);
  if (t.size() >= 6 && t.substr(0, 5) == "lang=") {
    auto q1 = t.find('"');
    if (q1 != std::string_view::npos) {
      auto q2 = t.find('"', q1 + 1);
      if (q2 != std::string_view::npos) {
        const std::string lang(t.substr(5, q1 - 5));
        std::string value(trim(t.substr(q2 + 1)));
        std::string lang_id = trimmed(lang);
        if (!lang_id.empty() && lang_id.front() == '"') {
          lang_id.erase(lang_id.begin());
        }
        if (lang_id.empty()) {
          lang_id = "x-default";
        }
        alt.emplace(lang_id, std::move(value));
        return alt;
      }
    }
  }
  if (!t.empty() && t.front() == '{') {
    auto find_lang = [&](std::string_view lang) {
      const std::string needle = "\"" + std::string(lang) + "\"";
      const auto pos = t.find(needle);
      if (pos == std::string_view::npos) {
        return;
      }
      auto colon = t.find(':', pos + needle.size());
      if (colon == std::string_view::npos) {
        return;
      }
      auto q1 = t.find('"', colon);
      if (q1 == std::string_view::npos) {
        return;
      }
      auto q2 = t.find('"', q1 + 1);
      if (q2 == std::string_view::npos) {
        return;
      }
      alt.emplace(std::string(lang), std::string(t.substr(q1 + 1, q2 - q1 - 1)));
    };
    find_lang("x-default");
    if (!alt.empty()) {
      return alt;
    }
  }
  alt.emplace("x-default", std::string(t));
  return alt;
}

bool lang_equivalent(const LangAlt& a, const LangAlt& b) {
  auto xd = [](const LangAlt& alt) -> std::optional<std::string> {
    const auto it = alt.find("x-default");
    if (it != alt.end()) {
      return it->second;
    }
    if (alt.size() == 1) {
      return alt.begin()->second;
    }
    return std::nullopt;
  };
  const auto ax = xd(a);
  const auto bx = xd(b);
  if (ax && bx && trimmed(*ax) != trimmed(*bx)) {
    return false;
  }
  for (const auto& [lang, text] : a) {
    const auto it = b.find(lang);
    if (it != b.end() && trimmed(it->second) != trimmed(text)) {
      return false;
    }
  }
  if (a.size() != b.size() && ax && bx && trimmed(*ax) == trimmed(*bx) &&
      a.size() != 1 && b.size() != 1) {
    return false;
  }
  if (a.size() != b.size()) {
    return false;
  }
  return true;
}

std::vector<std::string> as_names(std::string_view text) {
  const std::string value = trimmed(text);
  if (value.empty()) {
    return {};
  }
  return {value};
}

bool list_equal_ordered(const std::vector<std::string>& a,
                        const std::vector<std::string>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (trimmed(a[i]) != trimmed(b[i])) {
      return false;
    }
  }
  return true;
}

bool list_equal_set(const std::vector<std::string>& a,
                    const std::vector<std::string>& b) {
  std::vector<std::string> left;
  std::vector<std::string> right;
  for (const auto& item : a) {
    left.push_back(trimmed(item));
  }
  for (const auto& item : b) {
    right.push_back(trimmed(item));
  }
  std::sort(left.begin(), left.end());
  std::sort(right.begin(), right.end());
  left.erase(std::unique(left.begin(), left.end()), left.end());
  right.erase(std::unique(right.begin(), right.end()), right.end());
  return left == right;
}

std::string xmp_raw_key(std::string_view property) {
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

std::string exif_raw_key(std::string_view tag) {
  const auto colon = tag.find(':');
  std::string group;
  std::string name(tag);
  if (colon != std::string_view::npos) {
    group = std::string(tag.substr(0, colon));
    name = std::string(tag.substr(colon + 1));
  }
  if (group == "IFD0") {
    return "Exif.Image." + name;
  }
  if (group == "IFD1") {
    return "Exif.Thumbnail." + name;
  }
  if (group == "ExifIFD") {
    return "Exif.Photo." + name;
  }
  if (group == "GPS") {
    return "Exif.GPSInfo." + name;
  }
  if (group == "InteropIFD") {
    return "Exif.Iop." + name;
  }
  return "Exif." + (group.empty() ? name : group + "." + name);
}

std::string iim_raw_key(std::string_view dataset) {
  if (dataset == "2:80") {
    return "Iptc.Application2.Byline";
  }
  if (dataset == "2:120") {
    return "Iptc.Application2.Caption";
  }
  if (dataset == "2:105") {
    return "Iptc.Application2.Headline";
  }
  if (dataset == "2:116") {
    return "Iptc.Application2.Copyright";
  }
  if (dataset == "2:110") {
    return "Iptc.Application2.Credit";
  }
  if (dataset == "2:25") {
    return "Iptc.Application2.Keywords";
  }
  if (dataset == "2:55") {
    return "Iptc.Application2.DateCreated";
  }
  if (dataset == "2:60") {
    return "Iptc.Application2.TimeCreated";
  }
  if (dataset == "2:90") {
    return "Iptc.Application2.City";
  }
  if (dataset == "2:95") {
    return "Iptc.Application2.ProvinceState";
  }
  if (dataset == "2:101") {
    return "Iptc.Application2.CountryName";
  }
  return {};
}

std::vector<const RawEntry*> matching(const RawDocument& document,
                                      std::string_view base) {
  std::vector<const RawEntry*> out;
  for (const RawEntry& entry : document.entries) {
    if (key_belongs(entry.key.key, base)) {
      out.push_back(&entry);
    }
  }
  return out;
}

std::optional<std::string> first_value(const RawDocument& document,
                                       std::string_view base) {
  for (const RawEntry* entry : matching(document, base)) {
    const std::string value = trimmed(entry->value);
    if (!value.empty()) {
      return value;
    }
  }
  return std::nullopt;
}

void add_sources(std::vector<SourceRef>& sources, const RawDocument& document,
                 std::string_view backend, std::string_view base) {
  for (const RawEntry* entry : matching(document, base)) {
    SourceRef ref;
    ref.raw_key = entry->key.key;
    ref.backend = std::string(backend);
    sources.push_back(std::move(ref));
  }
}

struct Group {
  std::string family;
  int rank{0};
  std::string primary_key;
  std::vector<SourceRef> sources;
  Value value;
};

void stamp_container(Group& group, std::string_view container) {
  for (SourceRef& source : group.sources) {
    source.container = std::string(container);
  }
}

void stamp_container(std::vector<Group>& groups, std::size_t from,
                     std::string_view container) {
  for (std::size_t i = from; i < groups.size(); ++i) {
    stamp_container(groups[i], container);
  }
}

bool values_equivalent(std::string_view property_id, const Value& a,
                       const Value& b) {
  if (property_id == kDateCreated || property_id == kVideoDateCreated) {
    const auto* da = std::get_if<DateTime>(&a.data);
    const auto* db = std::get_if<DateTime>(&b.data);
    return da && db && datetime_equivalent(*da, *db);
  }
  if (property_id == kGps) {
    const auto* ga = std::get_if<GpsCoordinate>(&a.data);
    const auto* gb = std::get_if<GpsCoordinate>(&b.data);
    return ga && gb && gps_equivalent(*ga, *gb);
  }
  if (property_id == kKeywords || property_id == kVideoKeywords) {
    const auto* la = std::get_if<std::vector<std::string>>(&a.data);
    const auto* lb = std::get_if<std::vector<std::string>>(&b.data);
    return la && lb && list_equal_set(*la, *lb);
  }
  if (property_id == kCreator) {
    const auto* la = std::get_if<std::vector<std::string>>(&a.data);
    const auto* lb = std::get_if<std::vector<std::string>>(&b.data);
    return la && lb && list_equal_ordered(*la, *lb);
  }
  if (property_id == kVideoCreator) {
    const auto* la = std::get_if<std::vector<Structure>>(&a.data);
    const auto* lb = std::get_if<std::vector<Structure>>(&b.data);
    if (!la || !lb || la->size() != lb->size()) {
      return false;
    }
    for (std::size_t i = 0; i < la->size(); ++i) {
      const auto na = la->at(i).find("name");
      const auto nb = lb->at(i).find("name");
      if (na == la->at(i).end() || nb == lb->at(i).end() ||
          !(na->second == nb->second)) {
        return false;
      }
    }
    return true;
  }
  if (property_id == kDescription || property_id == kCopyright ||
      property_id == kVideoTitle || property_id == kVideoDescription ||
      property_id == kVideoCopyright || property_id == kVideoKeywords) {
    const auto* la = std::get_if<LangAlt>(&a.data);
    const auto* lb = std::get_if<LangAlt>(&b.data);
    return la && lb && lang_equivalent(*la, *lb);
  }
  if (property_id == kLocation) {
    const auto* la = std::get_if<std::vector<Structure>>(&a.data);
    const auto* lb = std::get_if<std::vector<Structure>>(&b.data);
    if (!la || !lb || la->empty() || lb->empty()) {
      return false;
    }
    const Structure& sa = la->front();
    const Structure& sb = lb->front();
    for (const auto& [name, value] : sa) {
      const auto it = sb.find(name);
      if (it != sb.end() && !(it->second == value)) {
        return false;
      }
    }
    return true;
  }
  if (property_id == kRating) {
    const auto* da = std::get_if<double>(&a.data);
    const auto* db = std::get_if<double>(&b.data);
    return da && db && std::fabs(*da - *db) < 1e-9;
  }
  return a == b;
}

Value merge_values(std::string_view property_id, Value a, const Value& b) {
  if (property_id == kDateCreated || property_id == kVideoDateCreated) {
    const auto* da = std::get_if<DateTime>(&a.data);
    const auto* db = std::get_if<DateTime>(&b.data);
    if (da && db) {
      return make_value(datetime_merge(*da, *db));
    }
  }
  if (property_id == kGps) {
    const auto* ga = std::get_if<GpsCoordinate>(&a.data);
    const auto* gb = std::get_if<GpsCoordinate>(&b.data);
    if (ga && gb) {
      return make_value(gps_merge(*ga, *gb));
    }
  }
  if (property_id == kLocation) {
    const auto* la = std::get_if<std::vector<Structure>>(&a.data);
    const auto* lb = std::get_if<std::vector<Structure>>(&b.data);
    if (la && lb && !la->empty() && !lb->empty()) {
      Structure merged = la->front();
      for (const auto& [name, value] : lb->front()) {
        if (!merged.contains(name)) {
          merged.emplace(name, value);
        }
      }
      return make_value(std::vector<Structure>{std::move(merged)});
    }
  }
  return a;
}

ConflictEntry disagreement_entry(std::string_view property_id,
                                 const std::vector<Group>& groups,
                                 std::size_t winner, Resolution resolution) {
  ConflictEntry entry;
  entry.property_id = std::string(property_id);
  entry.preferred_source = groups[winner].primary_key;
  entry.resolution = resolution;
  entry.candidates.reserve(groups.size());
  for (const Group& group : groups) {
    ConflictCandidate candidate;
    candidate.value = group.value;
    candidate.sources = group.sources;
    candidate.family = group.family;
    candidate.primary_key = group.primary_key;
    entry.candidates.push_back(std::move(candidate));
  }
  return entry;
}

void classify(Metadata& metadata, std::string_view property_id,
              std::vector<Group> groups,
              std::vector<ConflictEntry>* disagreements) {
  if (groups.empty()) {
    return;
  }
  PropertyValue property;
  for (const Group& group : groups) {
    property.sources.insert(property.sources.end(), group.sources.begin(),
                            group.sources.end());
  }
  if (groups.size() == 1) {
    property.value = std::move(groups.front().value);
    property.resolution = Resolution::single;
    (void)metadata.set(property_id, std::move(property));
    return;
  }

  bool all_equivalent = true;
  for (std::size_t i = 1; i < groups.size(); ++i) {
    if (!values_equivalent(property_id, groups.front().value, groups[i].value)) {
      all_equivalent = false;
      break;
    }
  }
  if (all_equivalent) {
    Value merged = groups.front().value;
    for (std::size_t i = 1; i < groups.size(); ++i) {
      merged = merge_values(property_id, std::move(merged), groups[i].value);
    }
    property.value = std::move(merged);
    property.resolution = Resolution::equivalent;
    (void)metadata.set(property_id, std::move(property));
    return;
  }

  bool same_family_conflict = false;
  for (std::size_t i = 0; i < groups.size(); ++i) {
    for (std::size_t j = i + 1; j < groups.size(); ++j) {
      if (groups[i].family == groups[j].family &&
          !values_equivalent(property_id, groups[i].value, groups[j].value)) {
        same_family_conflict = true;
      }
    }
  }

  std::size_t winner = 0;
  for (std::size_t i = 1; i < groups.size(); ++i) {
    if (groups[i].rank < groups[winner].rank) {
      winner = i;
    }
  }
  property.value = groups[winner].value;
  property.preferred_source = groups[winner].primary_key;
  property.resolution =
      same_family_conflict ? Resolution::conflict : Resolution::reconciled;
  if (disagreements) {
    disagreements->push_back(disagreement_entry(
        property_id, groups, winner, property.resolution));
  }
  (void)metadata.set(property_id, std::move(property));
}

bool is_xmp_array_type(std::string_view type) {
  return type == "XmpBag" || type == "XmpSeq" || type == "seq";
}

bool is_xmp_array_token(std::string_view value) {
  return value == "XmpBag" || value == "XmpSeq" || value == "XmpAlt";
}

std::vector<std::string> split_joined_list(std::string_view text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= text.size()) {
    const auto pos = text.find(", ", start);
    const std::string_view part =
        pos == std::string_view::npos ? text.substr(start)
                                      : text.substr(start, pos - start);
    const std::string value = trimmed(part);
    if (!value.empty() && !is_xmp_array_token(value)) {
      out.push_back(value);
    }
    if (pos == std::string_view::npos) {
      break;
    }
    start = pos + 2;
  }
  return out;
}

std::vector<std::string> collect_list(const RawDocument& document,
                                      std::string_view base) {
  std::vector<std::string> indexed;
  std::vector<std::string> unindexed;
  std::vector<std::string> from_container;
  for (const RawEntry* entry : matching(document, base)) {
    const std::string value = trimmed(entry->value);
    if (value.empty() || is_xmp_array_token(value)) {
      continue;
    }
    const bool indexed_key =
        entry->key.key.size() > base.size() &&
        entry->key.key[base.size()] == '[';
    if (indexed_key) {
      indexed.push_back(value);
      continue;
    }
    if (is_xmp_array_type(entry->type_hint)) {
      auto parts = split_joined_list(value);
      from_container.insert(from_container.end(), parts.begin(), parts.end());
      continue;
    }
    unindexed.push_back(value);
  }
  if (!indexed.empty()) {
    return indexed;
  }
  if (!from_container.empty()) {
    return from_container;
  }
  return unindexed;
}

std::optional<Group> text_list_group(const RawDocument& document,
                                     std::string_view backend,
                                     std::string_view base,
                                     std::string family, int rank) {
  auto values = collect_list(document, base);
  if (values.empty()) {
    return std::nullopt;
  }
  Group group;
  group.family = std::move(family);
  group.rank = rank;
  group.primary_key = std::string(base);
  add_sources(group.sources, document, backend, base);
  group.value = make_value(std::move(values));
  return group;
}

std::optional<Group> text_group(const RawDocument& document,
                                std::string_view backend, std::string_view base,
                                std::string family, int rank) {
  const auto value = first_value(document, base);
  if (!value) {
    return std::nullopt;
  }
  Group group;
  group.family = std::move(family);
  group.rank = rank;
  group.primary_key = std::string(base);
  add_sources(group.sources, document, backend, base);
  group.value = make_value(*value);
  return group;
}

std::optional<Group> lang_group(const RawDocument& document,
                                std::string_view backend, std::string_view base,
                                std::string family, int rank) {
  LangAlt merged;
  bool any = false;
  for (const RawEntry* entry : matching(document, base)) {
    if (trim(entry->value).empty()) {
      continue;
    }
    any = true;
    LangAlt parsed = parse_lang_alt(entry->value);
    merged.insert(parsed.begin(), parsed.end());
  }
  if (!any) {
    return std::nullopt;
  }
  Group group;
  group.family = std::move(family);
  group.rank = rank;
  group.primary_key = std::string(base);
  add_sources(group.sources, document, backend, base);
  group.value = make_value(std::move(merged));
  return group;
}

std::optional<Group> date_group(const RawDocument& document,
                                std::string_view backend,
                                std::string_view primary,
                                std::string_view extra1,
                                std::string_view extra2, std::string family,
                                int rank) {
  const auto primary_value = first_value(document, primary);
  if (!primary_value) {
    return std::nullopt;
  }
  DateTime dt;
  if (!parse_datetime(*primary_value, dt)) {
    return std::nullopt;
  }
  const auto extra_a = extra1.empty() ? std::nullopt : first_value(document, extra1);
  const auto extra_b = extra2.empty() ? std::nullopt : first_value(document, extra2);
  if (family == "iim" && extra_a) {
    DateTime time;
    if (parse_time_only(*extra_a, time) || parse_datetime(*extra_a, time)) {
      dt.hour = time.hour;
      dt.minute = time.minute;
      dt.second = time.second;
      dt.subsecond_ns = time.subsecond_ns;
      dt.utc_offset_minutes = time.utc_offset_minutes;
    }
  } else {
    apply_subsec_offset(dt, extra_a.value_or(""), extra_b.value_or(""));
  }
  Group group;
  group.family = std::move(family);
  group.rank = rank;
  group.primary_key = std::string(primary);
  add_sources(group.sources, document, backend, primary);
  if (!extra1.empty()) {
    add_sources(group.sources, document, backend, extra1);
  }
  if (!extra2.empty()) {
    add_sources(group.sources, document, backend, extra2);
  }
  group.value = make_value(dt);
  return group;
}

std::optional<Structure> location_from_fields(const RawDocument& document,
                                              std::string_view city,
                                              std::string_view state,
                                              std::string_view country) {
  Structure fields;
  if (const auto value = first_value(document, city)) {
    fields.emplace("city", make_value(*value));
  }
  if (const auto value = first_value(document, state)) {
    fields.emplace("provinceState", make_value(*value));
  }
  if (const auto value = first_value(document, country)) {
    fields.emplace("countryName", make_value(*value));
  }
  if (fields.empty()) {
    return std::nullopt;
  }
  return fields;
}

void put_location_field(Structure& fields, std::string_view name,
                        std::string_view value) {
  const std::string n = ascii_lower(name);
  const std::string v = trimmed(value);
  if (v.empty()) {
    return;
  }
  if (n == "city") {
    fields.insert_or_assign("city", make_value(v));
  } else if (n == "provincestate" || n == "state" || n == "province") {
    fields.insert_or_assign("provinceState", make_value(v));
  } else if (n == "countryname" || n == "country") {
    fields.insert_or_assign("countryName", make_value(v));
  }
}

std::optional<Group> structured_location(const RawDocument& document,
                                         std::string_view backend,
                                         std::string_view base) {
  const auto entries = matching(document, base);
  if (entries.empty()) {
    return std::nullopt;
  }
  Structure fields;
  for (const RawEntry* entry : entries) {
    if (entry->key.key == base ||
        (entry->key.key.size() > base.size() &&
         entry->key.key[base.size()] == '[' &&
         entry->key.key.find('/') == std::string::npos)) {
      const std::string_view text = trim(entry->value);
      if (!text.empty() && text.front() == '{') {
        std::string_view rest = text;
        while (true) {
          auto q1 = rest.find('"');
          if (q1 == std::string_view::npos) {
            break;
          }
          auto q2 = rest.find('"', q1 + 1);
          if (q2 == std::string_view::npos) {
            break;
          }
          const std::string key(rest.substr(q1 + 1, q2 - q1 - 1));
          auto colon = rest.find(':', q2);
          if (colon == std::string_view::npos) {
            break;
          }
          auto v1 = rest.find('"', colon);
          if (v1 == std::string_view::npos) {
            break;
          }
          auto v2 = rest.find('"', v1 + 1);
          if (v2 == std::string_view::npos) {
            break;
          }
          put_location_field(fields, key, rest.substr(v1 + 1, v2 - v1 - 1));
          rest.remove_prefix(v2 + 1);
        }
      }
      continue;
    }
    put_location_field(fields, last_field(entry->key.key), entry->value);
  }
  if (fields.empty()) {
    return std::nullopt;
  }
  Group group;
  group.family = "xmp";
  group.rank = 0;
  group.primary_key = std::string(base);
  add_sources(group.sources, document, backend, base);
  group.value = make_value(std::vector<Structure>{std::move(fields)});
  return group;
}

std::optional<Group> gps_group(const RawDocument& document,
                               std::string_view backend,
                               std::string_view lat_key,
                               std::string_view lat_ref, std::string_view lon_key,
                               std::string_view lon_ref, std::string_view alt_key,
                               std::string_view alt_ref, std::string family,
                               int rank) {
  const auto lat = first_value(document, lat_key);
  const auto lon = first_value(document, lon_key);
  if (!lat || !lon) {
    return std::nullopt;
  }
  GpsCoordinate gps;
  std::string lat_text = *lat;
  std::string lon_text = *lon;
  char lat_h = take_hemisphere(lat_text);
  char lon_h = take_hemisphere(lon_text);
  if (!lat_h) {
    if (const auto ref = first_value(document, lat_ref)) {
      lat_h = hemi_from_token(*ref);
    }
  }
  if (!lon_h) {
    if (const auto ref = first_value(document, lon_ref)) {
      lon_h = hemi_from_token(*ref);
    }
  }
  if (!parse_coord(lat_text, false, gps.latitude) ||
      !parse_coord(lon_text, true, gps.longitude)) {
    return std::nullopt;
  }
  if (gps.latitude >= 0) {
    gps.latitude *= static_cast<double>(hemi_sign(lat_h));
  }
  if (gps.longitude >= 0) {
    gps.longitude *= static_cast<double>(hemi_sign(lon_h));
  }
  if (const auto alt = first_value(document, alt_key)) {
    parse_altitude(*alt, first_value(document, alt_ref).value_or(""),
                   gps.altitude_meters);
  }
  Group group;
  group.family = std::move(family);
  group.rank = rank;
  group.primary_key = std::string(lat_key);
  add_sources(group.sources, document, backend, lat_key);
  add_sources(group.sources, document, backend, lat_ref);
  add_sources(group.sources, document, backend, lon_key);
  add_sources(group.sources, document, backend, lon_ref);
  add_sources(group.sources, document, backend, alt_key);
  add_sources(group.sources, document, backend, alt_ref);
  group.value = make_value(gps);
  return group;
}

void collect_registry_property(std::vector<Group>& groups,
                               const RawDocument& document,
                               std::string_view backend,
                               std::string_view property_id) {
  const auto def = registry().find(property_id);
  if (!def) {
    return;
  }
  const Representations& rep = def->representations;

  auto push = [&](std::optional<Group> group) {
    if (group) {
      groups.push_back(std::move(*group));
    }
  };

  if (property_id == kDateCreated) {
    std::string xmp_ps;
    if (!rep.xmp_property.empty()) {
      xmp_ps = xmp_raw_key(rep.xmp_property);
    }
    std::vector<std::string> exif_keys;
    std::string_view rest = rep.exif_tag;
    while (!rest.empty()) {
      const auto plus = rest.find('+');
      const std::string_view item =
          plus == std::string_view::npos ? rest : rest.substr(0, plus);
      exif_keys.push_back(exif_raw_key(item));
      if (plus == std::string_view::npos) {
        break;
      }
      rest.remove_prefix(plus + 1);
    }
    const std::string iim_date = iim_raw_key(rep.iim_dataset);
    const std::string iim_time = iim_raw_key("2:60");
    if (!xmp_ps.empty()) {
      push(date_group(document, backend, xmp_ps, "", "", "xmp", 0));
    }
    push(date_group(document, backend, "Xmp.exif.DateTimeOriginal", "", "",
                    "xmp", 0));
    if (exif_keys.size() >= 1) {
      const std::string_view subsec =
          exif_keys.size() > 1 ? std::string_view(exif_keys[1])
                               : std::string_view{};
      const std::string_view offset =
          exif_keys.size() > 2 ? std::string_view(exif_keys[2])
                               : std::string_view{};
      push(date_group(document, backend, exif_keys[0], subsec, offset, "exif",
                      1));
    }
    if (!iim_date.empty()) {
      push(date_group(document, backend, iim_date, iim_time, "", "iim", 2));
    }
    return;
  }

  if (property_id == kLocation) {
    if (!rep.xmp_property.empty()) {
      push(structured_location(document, backend, xmp_raw_key(rep.xmp_property)));
    }
    if (auto fields = location_from_fields(document, "Xmp.photoshop.City",
                                           "Xmp.photoshop.State",
                                           "Xmp.photoshop.Country")) {
      Group group;
      group.family = "xmp-legacy";
      group.rank = 1;
      group.primary_key = "Xmp.photoshop.City";
      add_sources(group.sources, document, backend, "Xmp.photoshop.City");
      add_sources(group.sources, document, backend, "Xmp.photoshop.State");
      add_sources(group.sources, document, backend, "Xmp.photoshop.Country");
      group.value = make_value(std::vector<Structure>{std::move(*fields)});
      groups.push_back(std::move(group));
    }
    if (auto fields = location_from_fields(document, "Iptc.Application2.City",
                                           "Iptc.Application2.ProvinceState",
                                           "Iptc.Application2.CountryName")) {
      Group group;
      group.family = "iim";
      group.rank = 2;
      group.primary_key = "Iptc.Application2.City";
      add_sources(group.sources, document, backend, "Iptc.Application2.City");
      add_sources(group.sources, document, backend,
                  "Iptc.Application2.ProvinceState");
      add_sources(group.sources, document, backend,
                  "Iptc.Application2.CountryName");
      group.value = make_value(std::vector<Structure>{std::move(*fields)});
      groups.push_back(std::move(group));
    }
    return;
  }

  const std::string xmp =
      rep.xmp_property.empty() ? std::string() : xmp_raw_key(rep.xmp_property);
  const std::string iim =
      rep.iim_dataset.empty() ? std::string() : iim_raw_key(rep.iim_dataset);
  std::string exif;
  if (!rep.exif_tag.empty()) {
    const auto plus = rep.exif_tag.find('+');
    exif = exif_raw_key(plus == std::string_view::npos ? rep.exif_tag
                                                      : rep.exif_tag.substr(0, plus));
  }

  if (property_id == kCreator || property_id == kKeywords) {
    if (!xmp.empty()) {
      push(text_list_group(document, backend, xmp, "xmp", 0));
    }
    if (!iim.empty()) {
      push(text_list_group(document, backend, iim, "iim", 1));
    }
    if (!exif.empty()) {
      push(text_list_group(document, backend, exif, "exif", 2));
    }
    return;
  }
  if (property_id == kDescription || property_id == kCopyright) {
    if (!xmp.empty()) {
      push(lang_group(document, backend, xmp, "xmp", 0));
    }
    if (!iim.empty()) {
      push(lang_group(document, backend, iim, "iim", 1));
    }
    if (!exif.empty()) {
      push(lang_group(document, backend, exif, "exif", 2));
    }
    return;
  }
  if (property_id == kRating) {
    if (!xmp.empty()) {
      if (const auto text = first_value(document, xmp)) {
        double rating = 0;
        if (parse_double(*text, rating)) {
          Group group;
          group.family = "xmp";
          group.rank = 0;
          group.primary_key = xmp;
          add_sources(group.sources, document, backend, xmp);
          group.value = make_value(rating);
          groups.push_back(std::move(group));
        }
      }
    }
    return;
  }

  if (!xmp.empty()) {
    push(text_group(document, backend, xmp, "xmp", 0));
  }
  if (!iim.empty()) {
    push(text_group(document, backend, iim, "iim", 1));
  }
  if (!exif.empty()) {
    push(text_group(document, backend, exif, "exif", 2));
  }
}

void collect_gps(std::vector<Group>& groups, const RawDocument& document,
                 std::string_view backend, bool video) {
  auto push = [&](std::optional<Group> group) {
    if (group) {
      groups.push_back(std::move(*group));
    }
  };
  if (video) {
    if (const auto text = first_value(document, "QuickTime.GPSCoordinates")) {
      GpsCoordinate gps;
      if (parse_qt_gps(*text, gps)) {
        Group group;
        group.family = "quicktime";
        group.rank = 0;
        group.primary_key = "QuickTime.GPSCoordinates";
        add_sources(group.sources, document, backend,
                    "QuickTime.GPSCoordinates");
        group.value = make_value(gps);
        groups.push_back(std::move(group));
      }
    }
    push(gps_group(document, backend, "Xmp.exif.GPSLatitude",
                   "Xmp.exif.GPSLatitudeRef", "Xmp.exif.GPSLongitude",
                   "Xmp.exif.GPSLongitudeRef", "Xmp.exif.GPSAltitude",
                   "Xmp.exif.GPSAltitudeRef", "xmp", 1));
    return;
  }
  push(gps_group(document, backend, "Exif.GPSInfo.GPSLatitude",
                 "Exif.GPSInfo.GPSLatitudeRef", "Exif.GPSInfo.GPSLongitude",
                 "Exif.GPSInfo.GPSLongitudeRef", "Exif.GPSInfo.GPSAltitude",
                 "Exif.GPSInfo.GPSAltitudeRef", "exif", 0));
  push(gps_group(document, backend, "Xmp.exif.GPSLatitude",
                 "Xmp.exif.GPSLatitudeRef", "Xmp.exif.GPSLongitude",
                 "Xmp.exif.GPSLongitudeRef", "Xmp.exif.GPSAltitude",
                 "Xmp.exif.GPSAltitudeRef", "xmp", 1));
}

void collect_video_property(std::vector<Group>& groups,
                            const RawDocument& document,
                            std::string_view backend,
                            std::string_view property_id) {
  const auto def = registry().find(property_id);
  std::string xmp;
  if (def && !def->representations.xmp_property.empty()) {
    xmp = xmp_raw_key(def->representations.xmp_property);
  }

  auto push = [&](std::optional<Group> group) {
    if (group) {
      groups.push_back(std::move(*group));
    }
  };

  if (property_id == kVideoDateCreated) {
    if (!xmp.empty()) {
      push(date_group(document, backend, xmp, "", "", "xmp", 0));
    }
    push(date_group(document, backend, "QuickTime.CreationDate", "", "",
                    "quicktime", 1));
    push(date_group(document, backend, "QuickTime.CreateDate", "", "",
                    "quicktime-header", 2));
    return;
  }
  if (property_id == kVideoCreator) {
    auto names = collect_list(document, "Xmp.dc.creator");
    std::string xmp_key = "Xmp.dc.creator";
    if (names.empty() && !xmp.empty() && xmp != "Xmp.dc.creator") {
      names = collect_list(document, xmp);
      xmp_key = xmp;
    }
    if (!names.empty()) {
      std::vector<Structure> entities;
      for (const std::string& name : names) {
        Structure entity;
        entity.emplace("name", make_value(LangAlt{{"x-default", name}}));
        entities.push_back(std::move(entity));
      }
      Group group;
      group.family = "xmp";
      group.rank = 0;
      group.primary_key = xmp_key;
      add_sources(group.sources, document, backend, xmp_key);
      group.value = make_value(std::move(entities));
      groups.push_back(std::move(group));
    }
    std::vector<std::string> qt_names;
    for (const char* key : {"QuickTime.Artist", "QuickTime.Author",
                            "QuickTime.Director"}) {
      auto part = collect_list(document, key);
      qt_names.insert(qt_names.end(), part.begin(), part.end());
    }
    if (!qt_names.empty()) {
      std::vector<Structure> entities;
      for (const std::string& name : qt_names) {
        Structure entity;
        entity.emplace("name", make_value(LangAlt{{"x-default", name}}));
        entities.push_back(std::move(entity));
      }
      Group group;
      group.family = "quicktime";
      group.rank = 1;
      group.primary_key = "QuickTime.Artist";
      add_sources(group.sources, document, backend, "QuickTime.Artist");
      add_sources(group.sources, document, backend, "QuickTime.Author");
      add_sources(group.sources, document, backend, "QuickTime.Director");
      group.value = make_value(std::move(entities));
      groups.push_back(std::move(group));
    }
    return;
  }
  if (property_id == kVideoKeywords) {
    auto as_lang = [&](std::string_view base, std::string family,
                       int rank) -> std::optional<Group> {
      auto values = collect_list(document, base);
      if (values.empty()) {
        return std::nullopt;
      }
      std::string joined;
      for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
          joined += ", ";
        }
        joined += values[i];
      }
      Group group;
      group.family = std::move(family);
      group.rank = rank;
      group.primary_key = std::string(base);
      add_sources(group.sources, document, backend, base);
      group.value = make_value(LangAlt{{"x-default", std::move(joined)}});
      return group;
    };
    if (!xmp.empty()) {
      push(as_lang(xmp, "xmp", 0));
    }
    push(as_lang("QuickTime.Keywords", "quicktime", 1));
    return;
  }
  if (property_id == kVideoTitle || property_id == kVideoDescription ||
      property_id == kVideoCopyright) {
    if (!xmp.empty()) {
      push(lang_group(document, backend, xmp, "xmp", 0));
    }
    const char* qt = "QuickTime.Title";
    if (property_id == kVideoDescription) {
      qt = "QuickTime.Description";
    } else if (property_id == kVideoCopyright) {
      qt = "QuickTime.Copyright";
    }
    push(lang_group(document, backend, qt, "quicktime", 1));
  }
}

bool is_video_file_type(std::string_view file_type) {
  const std::string lower = ascii_lower(file_type);
  return lower == "mp4" || lower == "mov";
}

void add_document_groups(std::vector<Group>& groups, const RawDocument& document,
                         std::string_view backend, std::string_view property_id,
                         std::string_view container, bool video) {
  const std::size_t from = groups.size();
  if (property_id == kGps) {
    collect_gps(groups, document, backend, video);
  } else if (video) {
    collect_video_property(groups, document, backend, property_id);
  } else {
    collect_registry_property(groups, document, backend, property_id);
  }
  stamp_container(groups, from, container);
}

void reconcile_property(Metadata& metadata, const RawDocument& embedded,
                        const RawDocument* sidecar, std::string_view backend,
                        std::string_view property_id, bool video,
                        std::vector<ConflictEntry>* disagreements) {
  std::vector<Group> groups;
  add_document_groups(groups, embedded, backend, property_id, "embedded",
                      video);
  if (sidecar) {
    add_document_groups(groups, *sidecar, backend, property_id, "sidecar",
                        video);
  }
  classify(metadata, property_id, std::move(groups), disagreements);
}

}  // namespace

Result<Metadata> reconcile(const RawDocument& document,
                           std::string_view backend_id,
                           const RawDocument* sidecar,
                           std::string_view file_type,
                           std::vector<ConflictEntry>* disagreements) {
  Metadata metadata;
  std::vector<RawEntry> raw = document.entries;
  if (sidecar) {
    raw.insert(raw.end(), sidecar->entries.begin(), sidecar->entries.end());
  }
  metadata.assignRaw(std::move(raw));
  const bool video = is_video_file_type(file_type);
  if (video) {
    reconcile_property(metadata, document, sidecar, backend_id, kVideoTitle,
                       true, disagreements);
    reconcile_property(metadata, document, sidecar, backend_id,
                       kVideoDescription, true, disagreements);
    reconcile_property(metadata, document, sidecar, backend_id, kVideoCreator,
                       true, disagreements);
    reconcile_property(metadata, document, sidecar, backend_id,
                       kVideoDateCreated, true, disagreements);
    reconcile_property(metadata, document, sidecar, backend_id,
                       kVideoCopyright, true, disagreements);
    reconcile_property(metadata, document, sidecar, backend_id, kVideoKeywords,
                       true, disagreements);
    reconcile_property(metadata, document, sidecar, backend_id, kGps, true,
                       disagreements);
    return metadata;
  }
  reconcile_property(metadata, document, sidecar, backend_id, kCreator, false,
                     disagreements);
  reconcile_property(metadata, document, sidecar, backend_id, kDescription,
                     false, disagreements);
  reconcile_property(metadata, document, sidecar, backend_id, kHeadline, false,
                     disagreements);
  reconcile_property(metadata, document, sidecar, backend_id, kDateCreated,
                     false, disagreements);
  reconcile_property(metadata, document, sidecar, backend_id, kCopyright,
                     false, disagreements);
  reconcile_property(metadata, document, sidecar, backend_id, kCredit, false,
                     disagreements);
  reconcile_property(metadata, document, sidecar, backend_id, kKeywords, false,
                     disagreements);
  reconcile_property(metadata, document, sidecar, backend_id, kRating, false,
                     disagreements);
  reconcile_property(metadata, document, sidecar, backend_id, kLocation, false,
                     disagreements);
  reconcile_property(metadata, document, sidecar, backend_id, kGps, false,
                     disagreements);
  return metadata;
}

}  // namespace umm::internal
