#include "umm/track.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <fstream>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace umm {
namespace {
namespace chrono = std::chrono;

Error io_error(ErrorCode code, std::string message, std::string detail) {
  return Error{code, std::move(message), "", std::move(detail)};
}

std::string path_utf8(const std::filesystem::path& path) {
  const std::u8string utf8 = path.u8string();
  return {utf8.begin(), utf8.end()};
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
         std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.remove_prefix(1);
  }
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.remove_suffix(1);
  }
  return text;
}

std::string_view local_name(std::string_view qname) {
  const auto pos = qname.rfind(':');
  if (pos == std::string_view::npos) {
    return qname;
  }
  return qname.substr(pos + 1);
}

bool ieq_local(std::string_view qname, std::string_view expected) {
  return ascii_lower(local_name(qname)) == ascii_lower(local_name(expected));
}

std::optional<double> parse_double(std::string_view text) {
  text = trim(text);
  if (text.empty()) {
    return std::nullopt;
  }
  // Apple libc++ does not implement floating-point std::from_chars.
  std::istringstream in{std::string(text)};
  in.imbue(std::locale::classic());
  double value = 0;
  in >> value;
  if (!in) {
    return std::nullopt;
  }
  in >> std::ws;
  if (!in.eof()) {
    return std::nullopt;
  }
  return value;
}

std::optional<int> parse_int(std::string_view text) {
  text = trim(text);
  if (text.empty()) {
    return std::nullopt;
  }
  int value = 0;
  const char* first = text.data();
  const char* last = first + text.size();
  const auto parsed = std::from_chars(first, last, value);
  if (parsed.ec != std::errc{} || parsed.ptr != last) {
    return std::nullopt;
  }
  return value;
}

std::optional<int> parse_digits(std::string_view text, std::size_t width) {
  if (text.size() < width) {
    return std::nullopt;
  }
  return parse_int(text.substr(0, width));
}

using Instant = chrono::sys_time<chrono::nanoseconds>;

std::optional<Instant> to_utc_instant(const DateTime& dt) {
  if (!dt.month || !dt.day || !dt.hour || !dt.minute) {
    return std::nullopt;
  }
  const chrono::year_month_day ymd{chrono::year{dt.year},
                                   chrono::month{static_cast<unsigned>(*dt.month)},
                                   chrono::day{static_cast<unsigned>(*dt.day)}};
  if (!ymd.ok()) {
    return std::nullopt;
  }
  Instant tp = chrono::sys_days{ymd} + chrono::hours{*dt.hour} +
               chrono::minutes{*dt.minute} +
               chrono::seconds{dt.second.value_or(0)} +
               chrono::nanoseconds{dt.subsecond_ns.value_or(0)};
  if (dt.utc_offset_minutes) {
    tp -= chrono::minutes{*dt.utc_offset_minutes};
  }
  return tp;
}

DateTime from_utc_instant(Instant tp) {
  const auto dp = chrono::floor<chrono::days>(tp);
  const chrono::year_month_day ymd{dp};
  auto tod = tp - dp;
  const auto hours = chrono::duration_cast<chrono::hours>(tod);
  tod -= hours;
  const auto minutes = chrono::duration_cast<chrono::minutes>(tod);
  tod -= minutes;
  const auto seconds = chrono::duration_cast<chrono::seconds>(tod);
  tod -= seconds;

  DateTime dt;
  dt.year = static_cast<int>(ymd.year());
  dt.month = static_cast<int>(static_cast<unsigned>(ymd.month()));
  dt.day = static_cast<int>(static_cast<unsigned>(ymd.day()));
  dt.hour = static_cast<int>(hours.count());
  dt.minute = static_cast<int>(minutes.count());
  dt.second = static_cast<int>(seconds.count());
  if (tod.count() != 0) {
    dt.subsecond_ns = static_cast<int>(tod.count());
  }
  dt.utc_offset_minutes = 0;
  return dt;
}

DateTime as_utc(DateTime dt) {
  if (const std::optional<Instant> instant = to_utc_instant(dt)) {
    return from_utc_instant(*instant);
  }
  dt.utc_offset_minutes = 0;
  return dt;
}

std::optional<int> parse_fraction_ns(std::string_view digits) {
  if (digits.empty() || digits.size() > 9) {
    if (digits.empty()) {
      return std::nullopt;
    }
    digits = digits.substr(0, 9);
  }
  std::string padded(digits);
  padded.append(9 - padded.size(), '0');
  return parse_int(padded);
}

std::optional<DateTime> parse_iso8601(std::string_view text) {
  text = trim(text);
  if (text.size() < 19) {
    return std::nullopt;
  }
  const auto year = parse_int(text.substr(0, 4));
  const auto month = parse_int(text.substr(5, 2));
  const auto day = parse_int(text.substr(8, 2));
  if (!year || !month || !day || text[4] != '-' || text[7] != '-' ||
      (text[10] != 'T' && text[10] != 't')) {
    return std::nullopt;
  }
  const auto hour = parse_int(text.substr(11, 2));
  const auto minute = parse_int(text.substr(14, 2));
  const auto second = parse_int(text.substr(17, 2));
  if (!hour || !minute || !second || text[13] != ':' || text[16] != ':') {
    return std::nullopt;
  }

  DateTime dt;
  dt.year = *year;
  dt.month = *month;
  dt.day = *day;
  dt.hour = *hour;
  dt.minute = *minute;
  dt.second = *second;

  std::size_t i = 19;
  if (i < text.size() && text[i] == '.') {
    ++i;
    const std::size_t start = i;
    while (i < text.size() &&
           std::isdigit(static_cast<unsigned char>(text[i])) != 0) {
      ++i;
    }
    if (const auto ns = parse_fraction_ns(text.substr(start, i - start))) {
      dt.subsecond_ns = *ns;
    }
  }

  if (i >= text.size()) {
    dt.utc_offset_minutes = 0;
    return as_utc(dt);
  }
  if (text[i] == 'Z' || text[i] == 'z') {
    if (i + 1 != text.size()) {
      return std::nullopt;
    }
    dt.utc_offset_minutes = 0;
    return as_utc(dt);
  }
  if (text[i] != '+' && text[i] != '-') {
    return std::nullopt;
  }
  const int sign = text[i] == '-' ? -1 : 1;
  ++i;
  const auto oh = parse_digits(text.substr(i), 2);
  if (!oh) {
    return std::nullopt;
  }
  i += 2;
  int om = 0;
  if (i < text.size() && text[i] == ':') {
    ++i;
  }
  if (i + 2 <= text.size()) {
    const auto parsed_om = parse_digits(text.substr(i), 2);
    if (!parsed_om) {
      return std::nullopt;
    }
    om = *parsed_om;
    i += 2;
  }
  if (i != text.size()) {
    return std::nullopt;
  }
  dt.utc_offset_minutes = sign * (*oh * 60 + om);
  return as_utc(dt);
}

void finish_track(Track& track) {
  std::stable_sort(track.points.begin(), track.points.end(),
                   [](const TrackPoint& a, const TrackPoint& b) {
                     const auto ia = to_utc_instant(a.time);
                     const auto ib = to_utc_instant(b.time);
                     if (!ia || !ib) {
                       return false;
                     }
                     return *ia < *ib;
                   });
  if (!track.points.empty()) {
    track.start_time = track.points.front().time;
    track.end_time = track.points.back().time;
  }
}

struct XmlAttr {
  std::string name;
  std::string value;
};

struct XmlToken {
  enum class Kind { start, end, text } kind{Kind::text};
  std::string name;
  std::string text;
  std::vector<XmlAttr> attrs;
  bool self_closing{false};
};

std::string decode_entities(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '&') {
      out.push_back(text[i]);
      continue;
    }
    const auto end = text.find(';', i + 1);
    if (end == std::string_view::npos || end - i > 8) {
      out.push_back(text[i]);
      continue;
    }
    const std::string_view ent = text.substr(i + 1, end - (i + 1));
    if (ent == "amp") {
      out.push_back('&');
    } else if (ent == "lt") {
      out.push_back('<');
    } else if (ent == "gt") {
      out.push_back('>');
    } else if (ent == "quot") {
      out.push_back('"');
    } else if (ent == "apos") {
      out.push_back('\'');
    } else {
      out.append(text.substr(i, end - i + 1));
    }
    i = end;
  }
  return out;
}

bool is_name_start(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
         c == ':';
}

bool is_name_char(char c) {
  return is_name_start(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

std::optional<std::vector<XmlToken>> tokenize_xml(std::string_view xml) {
  std::vector<XmlToken> tokens;
  std::size_t i = 0;
  const std::size_t n = xml.size();
  while (i < n) {
    if (xml[i] != '<') {
      const std::size_t start = i;
      while (i < n && xml[i] != '<') {
        ++i;
      }
      XmlToken token;
      token.kind = XmlToken::Kind::text;
      token.text = decode_entities(xml.substr(start, i - start));
      tokens.push_back(std::move(token));
      continue;
    }
    if (xml.substr(i, 4) == "<!--") {
      const auto end = xml.find("-->", i + 4);
      if (end == std::string_view::npos) {
        return std::nullopt;
      }
      i = end + 3;
      continue;
    }
    if (xml.substr(i, 2) == "<?") {
      const auto end = xml.find("?>", i + 2);
      if (end == std::string_view::npos) {
        return std::nullopt;
      }
      i = end + 2;
      continue;
    }
    if (xml.substr(i, 9) == "<![CDATA[") {
      const auto end = xml.find("]]>", i + 9);
      if (end == std::string_view::npos) {
        return std::nullopt;
      }
      XmlToken token;
      token.kind = XmlToken::Kind::text;
      token.text = std::string(xml.substr(i + 9, end - (i + 9)));
      tokens.push_back(std::move(token));
      i = end + 3;
      continue;
    }
    if (xml.substr(i, 2) == "</") {
      i += 2;
      const std::size_t name_start = i;
      while (i < n && is_name_char(xml[i])) {
        ++i;
      }
      if (i == name_start) {
        return std::nullopt;
      }
      XmlToken token;
      token.kind = XmlToken::Kind::end;
      token.name = std::string(xml.substr(name_start, i - name_start));
      while (i < n && std::isspace(static_cast<unsigned char>(xml[i])) != 0) {
        ++i;
      }
      if (i >= n || xml[i] != '>') {
        return std::nullopt;
      }
      ++i;
      tokens.push_back(std::move(token));
      continue;
    }
    ++i;
    if (i >= n || !is_name_start(xml[i])) {
      return std::nullopt;
    }
    const std::size_t name_start = i;
    while (i < n && is_name_char(xml[i])) {
      ++i;
    }
    XmlToken token;
    token.kind = XmlToken::Kind::start;
    token.name = std::string(xml.substr(name_start, i - name_start));
    while (i < n && xml[i] != '>' && !(xml[i] == '/' && i + 1 < n && xml[i + 1] == '>')) {
      while (i < n && std::isspace(static_cast<unsigned char>(xml[i])) != 0) {
        ++i;
      }
      if (i >= n || xml[i] == '>' ||
          (xml[i] == '/' && i + 1 < n && xml[i + 1] == '>')) {
        break;
      }
      if (!is_name_start(xml[i])) {
        return std::nullopt;
      }
      const std::size_t attr_start = i;
      while (i < n && is_name_char(xml[i])) {
        ++i;
      }
      XmlAttr attr;
      attr.name = std::string(xml.substr(attr_start, i - attr_start));
      while (i < n && std::isspace(static_cast<unsigned char>(xml[i])) != 0) {
        ++i;
      }
      if (i >= n || xml[i] != '=') {
        return std::nullopt;
      }
      ++i;
      while (i < n && std::isspace(static_cast<unsigned char>(xml[i])) != 0) {
        ++i;
      }
      if (i >= n || (xml[i] != '"' && xml[i] != '\'')) {
        return std::nullopt;
      }
      const char quote = xml[i++];
      const std::size_t value_start = i;
      while (i < n && xml[i] != quote) {
        ++i;
      }
      if (i >= n) {
        return std::nullopt;
      }
      attr.value = decode_entities(xml.substr(value_start, i - value_start));
      ++i;
      token.attrs.push_back(std::move(attr));
    }
    if (i < n && xml[i] == '/' && i + 1 < n && xml[i + 1] == '>') {
      token.self_closing = true;
      i += 2;
    } else if (i < n && xml[i] == '>') {
      ++i;
    } else {
      return std::nullopt;
    }
    tokens.push_back(std::move(token));
  }
  return tokens;
}

const XmlAttr* find_attr(const XmlToken& token, std::string_view local) {
  for (const XmlAttr& attr : token.attrs) {
    if (ieq_local(attr.name, local)) {
      return &attr;
    }
  }
  return nullptr;
}

std::string collect_text(const std::vector<XmlToken>& tokens, std::size_t& i,
                         std::string_view end_name) {
  std::string text;
  ++i;
  while (i < tokens.size()) {
    const XmlToken& token = tokens[i];
    if (token.kind == XmlToken::Kind::end && ieq_local(token.name, end_name)) {
      break;
    }
    if (token.kind == XmlToken::Kind::text) {
      text += token.text;
    } else if (token.kind == XmlToken::Kind::start) {
      if (!token.self_closing) {
        text += collect_text(tokens, i, token.name);
      }
    }
    ++i;
  }
  return text;
}

Result<Track> parse_gpx(std::string_view xml) {
  const auto tokens = tokenize_xml(xml);
  if (!tokens) {
    return io_error(ErrorCode::format_corrupt, "malformed GPX XML", "");
  }

  Track track;
  track.format = TrackFormat::gpx;
  int attempts = 0;
  int accepted = 0;
  bool invalid = false;

  for (std::size_t i = 0; i < tokens->size(); ++i) {
    const XmlToken& token = (*tokens)[i];
    if (token.kind != XmlToken::Kind::start || !ieq_local(token.name, "trkpt")) {
      continue;
    }
    ++attempts;
    const XmlAttr* lat_attr = find_attr(token, "lat");
    const XmlAttr* lon_attr = find_attr(token, "lon");
    const auto lat = lat_attr ? parse_double(lat_attr->value) : std::nullopt;
    const auto lon = lon_attr ? parse_double(lon_attr->value) : std::nullopt;
    if (!lat || !lon) {
      invalid = true;
      break;
    }
    TrackPoint point;
    point.latitude = *lat;
    point.longitude = *lon;
    std::optional<DateTime> time;
    if (!token.self_closing) {
      const std::size_t start = i;
      ++i;
      while (i < tokens->size()) {
        const XmlToken& inner = (*tokens)[i];
        if (inner.kind == XmlToken::Kind::end && ieq_local(inner.name, "trkpt")) {
          break;
        }
        if (inner.kind == XmlToken::Kind::start && !inner.self_closing) {
          if (ieq_local(inner.name, "ele")) {
            const std::string text = collect_text(*tokens, i, inner.name);
            point.altitude_meters = parse_double(text);
          } else if (ieq_local(inner.name, "time")) {
            const std::string text = collect_text(*tokens, i, inner.name);
            time = parse_iso8601(text);
          }
        }
        ++i;
      }
      if (i >= tokens->size() && start < tokens->size()) {
        invalid = true;
        break;
      }
    }
    if (!time) {
      continue;
    }
    point.time = *time;
    track.points.push_back(std::move(point));
    ++accepted;
  }

  if (invalid || (attempts > 0 && accepted == 0)) {
    return io_error(ErrorCode::format_corrupt, "malformed GPX track", "");
  }
  finish_track(track);
  return track;
}

Result<Track> parse_kml(std::string_view xml) {
  const auto tokens = tokenize_xml(xml);
  if (!tokens) {
    return io_error(ErrorCode::format_corrupt, "malformed KML XML", "");
  }

  Track track;
  track.format = TrackFormat::kml;
  bool invalid = false;
  int tracks_seen = 0;

  for (std::size_t i = 0; i < tokens->size(); ++i) {
    const XmlToken& token = (*tokens)[i];
    if (token.kind != XmlToken::Kind::start || !ieq_local(token.name, "Track") ||
        token.self_closing) {
      continue;
    }
    ++tracks_seen;
    std::vector<DateTime> whens;
    std::vector<std::string> coords;
    ++i;
    while (i < tokens->size()) {
      const XmlToken& inner = (*tokens)[i];
      if (inner.kind == XmlToken::Kind::end && ieq_local(inner.name, "Track")) {
        break;
      }
      if (inner.kind == XmlToken::Kind::start && !inner.self_closing) {
        if (ieq_local(inner.name, "when")) {
          const auto time = parse_iso8601(collect_text(*tokens, i, inner.name));
          if (!time) {
            invalid = true;
            break;
          }
          whens.push_back(*time);
        } else if (ieq_local(inner.name, "coord")) {
          coords.push_back(collect_text(*tokens, i, inner.name));
        }
      }
      ++i;
    }
    if (invalid) {
      break;
    }
    if (whens.size() != coords.size()) {
      return io_error(ErrorCode::format_corrupt,
                      "KML track when/coord count mismatch", "");
    }
    for (std::size_t p = 0; p < whens.size(); ++p) {
      std::string_view coord = trim(coords[p]);
      std::vector<std::string_view> parts;
      std::size_t start = 0;
      while (start < coord.size()) {
        while (start < coord.size() &&
               std::isspace(static_cast<unsigned char>(coord[start])) != 0) {
          ++start;
        }
        if (start >= coord.size()) {
          break;
        }
        std::size_t end = start;
        while (end < coord.size() &&
               std::isspace(static_cast<unsigned char>(coord[end])) == 0) {
          ++end;
        }
        parts.push_back(coord.substr(start, end - start));
        start = end;
      }
      if (parts.size() < 2) {
        invalid = true;
        break;
      }
      const auto lon = parse_double(parts[0]);
      const auto lat = parse_double(parts[1]);
      if (!lon || !lat) {
        invalid = true;
        break;
      }
      TrackPoint point;
      point.time = whens[p];
      point.latitude = *lat;
      point.longitude = *lon;
      if (parts.size() >= 3) {
        point.altitude_meters = parse_double(parts[2]);
      }
      track.points.push_back(std::move(point));
    }
    if (invalid) {
      break;
    }
  }

  if (invalid || (tracks_seen > 0 && track.points.empty())) {
    return io_error(ErrorCode::format_corrupt, "malformed KML track", "");
  }
  finish_track(track);
  return track;
}

std::vector<std::string_view> split_csv(std::string_view line) {
  std::vector<std::string_view> fields;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= line.size(); ++i) {
    if (i == line.size() || line[i] == ',') {
      fields.push_back(line.substr(start, i - start));
      start = i + 1;
    }
  }
  return fields;
}

bool nmea_checksum_ok(std::string_view sentence) {
  if (sentence.empty() || sentence.front() != '$') {
    return false;
  }
  const auto star = sentence.find('*');
  if (star == std::string_view::npos) {
    return true;
  }
  if (star + 3 > sentence.size()) {
    return false;
  }
  unsigned given = 0;
  const auto parsed = std::from_chars(sentence.data() + star + 1,
                                      sentence.data() + star + 3, given, 16);
  if (parsed.ec != std::errc{}) {
    return false;
  }
  unsigned actual = 0;
  for (std::size_t i = 1; i < star; ++i) {
    actual ^= static_cast<unsigned char>(sentence[i]);
  }
  return actual == given;
}

std::string_view nmea_body(std::string_view sentence) {
  if (sentence.empty() || sentence.front() != '$') {
    return {};
  }
  const auto star = sentence.find('*');
  const auto end = star == std::string_view::npos ? sentence.size() : star;
  return sentence.substr(1, end - 1);
}

std::string nmea_formatter(std::string_view body) {
  const auto comma = body.find(',');
  const std::string_view addr =
      comma == std::string_view::npos ? body : body.substr(0, comma);
  if (addr.size() < 3) {
    return {};
  }
  return ascii_lower(addr.substr(addr.size() - 3));
}

std::optional<double> nmea_degmin(std::string_view field, bool /*longitude*/) {
  field = trim(field);
  const auto dot = field.find('.');
  const std::size_t int_len = dot == std::string_view::npos ? field.size() : dot;
  const std::size_t min_digits = 2;
  if (int_len <= min_digits) {
    return std::nullopt;
  }
  const std::size_t deg_len = int_len - min_digits;
  const auto deg = parse_double(field.substr(0, deg_len));
  const auto minutes = parse_double(field.substr(deg_len));
  if (!deg || !minutes) {
    return std::nullopt;
  }
  return *deg + (*minutes / 60.0);
}

std::optional<DateTime> nmea_time_of_day(std::string_view field) {
  field = trim(field);
  if (field.size() < 6) {
    return std::nullopt;
  }
  const auto hour = parse_digits(field, 2);
  const auto minute = parse_digits(field.substr(2), 2);
  const auto second = parse_digits(field.substr(4), 2);
  if (!hour || !minute || !second) {
    return std::nullopt;
  }
  DateTime dt;
  dt.hour = *hour;
  dt.minute = *minute;
  dt.second = *second;
  if (field.size() > 6 && field[6] == '.') {
    if (const auto ns = parse_fraction_ns(field.substr(7))) {
      dt.subsecond_ns = *ns;
    }
  }
  dt.utc_offset_minutes = 0;
  return dt;
}

std::optional<DateTime> nmea_date_ddmmyy(std::string_view field) {
  field = trim(field);
  if (field.size() < 6) {
    return std::nullopt;
  }
  const auto day = parse_digits(field, 2);
  const auto month = parse_digits(field.substr(2), 2);
  const auto yy = parse_digits(field.substr(4), 2);
  if (!day || !month || !yy) {
    return std::nullopt;
  }
  DateTime dt;
  dt.year = *yy >= 70 ? 1900 + *yy : 2000 + *yy;
  dt.month = *month;
  dt.day = *day;
  return dt;
}

bool apply_date(DateTime& time, const DateTime& date) {
  time.year = date.year;
  time.month = date.month;
  time.day = date.day;
  time.utc_offset_minutes = 0;
  return static_cast<bool>(to_utc_instant(time));
}

Result<Track> parse_nmea(std::string_view text) {
  Track track;
  track.format = TrackFormat::nmea;
  std::optional<DateTime> date;

  std::size_t line_start = 0;
  while (line_start <= text.size()) {
    std::size_t line_end = line_start;
    while (line_end < text.size() && text[line_end] != '\n' &&
           text[line_end] != '\r') {
      ++line_end;
    }
    std::string_view line = trim(text.substr(line_start, line_end - line_start));
    if (line_end < text.size() && text[line_end] == '\r') {
      ++line_end;
    }
    if (line_end < text.size() && text[line_end] == '\n') {
      ++line_end;
    }
    if (line_start == text.size()) {
      break;
    }
    line_start = line_end;
    if (line.empty() || line.front() != '$') {
      continue;
    }
    if (!nmea_checksum_ok(line)) {
      continue;
    }
    const std::string_view body = nmea_body(line);
    const std::string formatter = nmea_formatter(body);
    const auto fields = split_csv(body);
    if (formatter == "zda" && fields.size() >= 5) {
      const auto day = parse_int(fields[2]);
      const auto month = parse_int(fields[3]);
      const auto year = parse_int(fields[4]);
      if (day && month && year) {
        DateTime d;
        d.year = *year;
        d.month = *month;
        d.day = *day;
        date = d;
      }
      continue;
    }
    if (formatter == "rmc" && fields.size() >= 10) {
      if (ascii_lower(trim(fields[2])) != "a") {
        continue;
      }
      auto time = nmea_time_of_day(fields[1]);
      const auto lat = nmea_degmin(fields[3], false);
      const auto lon = nmea_degmin(fields[5], true);
      const auto rmc_date = nmea_date_ddmmyy(fields[9]);
      if (rmc_date) {
        date = rmc_date;
      }
      if (!time || !lat || !lon || !date) {
        continue;
      }
      if (!apply_date(*time, *date)) {
        continue;
      }
      const std::string ns = ascii_lower(trim(fields[4]));
      const std::string ew = ascii_lower(trim(fields[6]));
      TrackPoint point;
      point.time = as_utc(*time);
      point.latitude = *lat;
      point.longitude = *lon;
      if (ns == "s") {
        point.latitude = -point.latitude;
      }
      if (ew == "w") {
        point.longitude = -point.longitude;
      }
      track.points.push_back(std::move(point));
      continue;
    }
    if (formatter == "gga" && fields.size() >= 10) {
      auto time = nmea_time_of_day(fields[1]);
      const auto lat = nmea_degmin(fields[2], false);
      const auto lon = nmea_degmin(fields[4], true);
      if (!time || !lat || !lon || !date) {
        continue;
      }
      if (!apply_date(*time, *date)) {
        continue;
      }
      const std::string ns = ascii_lower(trim(fields[3]));
      const std::string ew = ascii_lower(trim(fields[5]));
      DateTime utc = as_utc(*time);
      double latitude = *lat;
      double longitude = *lon;
      if (ns == "s") {
        latitude = -latitude;
      }
      if (ew == "w") {
        longitude = -longitude;
      }
      const auto alt = parse_double(fields[9]);
      bool merged = false;
      if (!track.points.empty() && track.points.back().time == utc) {
        track.points.back().latitude = latitude;
        track.points.back().longitude = longitude;
        track.points.back().altitude_meters = alt;
        merged = true;
      }
      if (!merged) {
        TrackPoint point;
        point.time = utc;
        point.latitude = latitude;
        point.longitude = longitude;
        point.altitude_meters = alt;
        track.points.push_back(std::move(point));
      }
    }
  }

  if (track.points.empty()) {
    return io_error(ErrorCode::format_corrupt, "no usable NMEA positions", "");
  }
  finish_track(track);
  return track;
}

enum class Detected { unknown, gpx, nmea, kml };

Detected from_extension(const std::filesystem::path& path) {
  const std::string ext = ascii_lower(path_utf8(path.extension()));
  if (ext == ".gpx") {
    return Detected::gpx;
  }
  if (ext == ".kml") {
    return Detected::kml;
  }
  if (ext == ".nmea" || ext == ".nme") {
    return Detected::nmea;
  }
  return Detected::unknown;
}

bool looks_like_nmea(std::string_view text) {
  std::size_t i = 0;
  while (i < text.size()) {
    std::size_t end = i;
    while (end < text.size() && text[end] != '\n' && text[end] != '\r') {
      ++end;
    }
    std::string_view line = trim(text.substr(i, end - i));
    if (line.size() >= 6 && line.front() == '$' &&
        std::isalpha(static_cast<unsigned char>(line[1])) != 0) {
      return true;
    }
    if (end < text.size() && text[end] == '\r') {
      ++end;
    }
    if (end < text.size() && text[end] == '\n') {
      ++end;
    }
    if (end == i) {
      break;
    }
    i = end;
  }
  return false;
}

bool contains_open_local(std::string_view text, std::string_view local) {
  const std::string needle = ascii_lower(local);
  const std::string lower = ascii_lower(text.substr(0, std::min(text.size(), std::size_t{4096})));
  std::size_t pos = 0;
  while ((pos = lower.find('<', pos)) != std::string::npos) {
    ++pos;
    if (pos >= lower.size() || lower[pos] == '/' || lower[pos] == '!' ||
        lower[pos] == '?') {
      continue;
    }
    std::size_t name_end = pos;
    while (name_end < lower.size() && is_name_char(lower[name_end])) {
      ++name_end;
    }
    const std::string_view qname = std::string_view(lower).substr(pos, name_end - pos);
    if (local_name(qname) == needle) {
      return true;
    }
    pos = name_end;
  }
  return false;
}

Detected sniff(std::string_view text) {
  std::string_view view = text;
  if (view.size() >= 3 && static_cast<unsigned char>(view[0]) == 0xEF &&
      static_cast<unsigned char>(view[1]) == 0xBB &&
      static_cast<unsigned char>(view[2]) == 0xBF) {
    view.remove_prefix(3);
  }
  if (contains_open_local(view, "gpx")) {
    return Detected::gpx;
  }
  if (contains_open_local(view, "kml")) {
    return Detected::kml;
  }
  if (looks_like_nmea(view)) {
    return Detected::nmea;
  }
  return Detected::unknown;
}

std::string_view skip_bom(std::string_view text) {
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    text.remove_prefix(3);
  }
  return text;
}

}  // namespace

Result<Track> importTrack(const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return io_error(ErrorCode::io_not_found, "track file not found",
                    path_utf8(path));
  }
  if (!std::filesystem::is_regular_file(path, ec)) {
    return io_error(ErrorCode::io_read_failed, "track path is not a file",
                    path_utf8(path));
  }

  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return io_error(ErrorCode::io_read_failed, "failed to read track file",
                    path_utf8(path));
  }
  const std::string bytes((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
  if (!in && !in.eof()) {
    return io_error(ErrorCode::io_read_failed, "failed to read track file",
                    path_utf8(path));
  }

  const std::string_view payload = skip_bom(bytes);
  Detected detected = sniff(payload);
  if (detected == Detected::unknown) {
    detected = from_extension(path);
  }
  if (detected == Detected::unknown) {
    return io_error(ErrorCode::format_unrecognized, "unrecognized track format",
                    path_utf8(path));
  }
  if (detected == Detected::gpx) {
    return parse_gpx(payload);
  }
  if (detected == Detected::kml) {
    return parse_kml(payload);
  }
  return parse_nmea(payload);
}

}  // namespace umm
