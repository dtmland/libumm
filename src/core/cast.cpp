#include "core/cast.hpp"

#include "cast_rules.hpp"
#include "core/media_domain.hpp"
#include "core/xmp_codec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace umm::internal {
namespace {

constexpr double kGpsDegEps = 1e-5;
constexpr double kGpsAltEps = 0.5;

Value make_value(auto payload) {
  Value value;
  value.data = std::move(payload);
  return value;
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

std::string ascii_lower(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return out;
}

bool qt_group(std::string_view group) {
  return group == "Keys" || group == "UserData" || group == "ItemList";
}

std::string_view qt_tag(std::string_view key) {
  constexpr std::string_view kPrefix = "QuickTime.";
  if (key.rfind(kPrefix, 0) != 0) {
    return key;
  }
  std::string_view rest = key.substr(kPrefix.size());
  const auto dot = rest.find('.');
  if (dot != std::string_view::npos && qt_group(rest.substr(0, dot))) {
    return rest.substr(dot + 1);
  }
  return rest;
}

bool explicit_container_group(std::string_view key) {
  return key.find(".UserData.") != std::string_view::npos ||
         key.find(".Keys.") != std::string_view::npos ||
         key.find(".ItemList.") != std::string_view::npos;
}

bool base_key_matches(std::string_view entry, std::string_view want) {
  if (entry == want) {
    return true;
  }
  // UserData Title must not satisfy a Keys/ItemList rule, or the reverse.
  if (explicit_container_group(entry) || explicit_container_group(want)) {
    return false;
  }
  if (entry.rfind("QuickTime.", 0) == 0 && want.rfind("QuickTime.", 0) == 0) {
    return qt_tag(entry) == qt_tag(want);
  }
  return false;
}

const BaseEntry* find_base(const Metadata& metadata, std::string_view key) {
  for (const BaseEntry& entry : metadata.dumpAll()) {
    if (base_key_matches(entry.key.key, key) && !trim(entry.value).empty()) {
      return &entry;
    }
  }
  return nullptr;
}

bool parse_double(std::string_view text, double& out) {
  text = trim(text);
  if (text.empty()) {
    return false;
  }
  std::string copy(text);
  char* end = nullptr;
  const double value = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str()) {
    return false;
  }
  out = value;
  return true;
}

bool parse_iso6709(std::string_view text, GpsCoordinate& gps) {
  text = trim(text);
  if (!text.empty() && text.back() == '/') {
    text.remove_suffix(1);
  }
  if (text.size() < 4 || (text.front() != '+' && text.front() != '-')) {
    return false;
  }
  std::size_t second = std::string_view::npos;
  for (std::size_t i = 1; i < text.size(); ++i) {
    if (text[i] == '+' || text[i] == '-') {
      second = i;
      break;
    }
  }
  if (second == std::string_view::npos) {
    return false;
  }
  std::size_t third = std::string_view::npos;
  for (std::size_t i = second + 1; i < text.size(); ++i) {
    if (text[i] == '+' || text[i] == '-') {
      third = i;
      break;
    }
  }
  double lat = 0;
  double lon = 0;
  if (!parse_double(text.substr(0, second), lat) ||
      !parse_double(text.substr(second, third == std::string_view::npos
                                          ? std::string_view::npos
                                          : third - second),
                    lon)) {
    return false;
  }
  gps.latitude = lat;
  gps.longitude = lon;
  if (third != std::string_view::npos) {
    double alt = 0;
    if (parse_double(text.substr(third), alt)) {
      gps.altitude_meters = alt;
    }
  }
  return true;
}

bool parse_comma_gps(std::string_view text, GpsCoordinate& gps) {
  std::string s(trim(text));
  std::vector<std::string> parts;
  std::string part;
  auto flush = [&] {
    const auto item = std::string(trim(part));
    if (!item.empty()) {
      parts.push_back(item);
    }
    part.clear();
  };
  for (char c : s) {
    if (c == ',') {
      flush();
    } else {
      part.push_back(c);
    }
  }
  flush();
  if (parts.size() < 2) {
    return false;
  }
  const auto lat = parse_gps_coord(parts[0]);
  const auto lon = parse_gps_coord(parts[1]);
  if (!lat || !lon) {
    return false;
  }
  gps.latitude = *lat;
  gps.longitude = *lon;
  if (parts.size() >= 3) {
    double alt = 0;
    std::string alt_text = parts[2];
    const auto space = alt_text.find_first_of(" \t");
    if (space != std::string::npos) {
      alt_text.resize(space);
    }
    if (parse_double(alt_text, alt)) {
      gps.altitude_meters = alt;
    }
  }
  return true;
}

bool parse_position(std::string_view text, GpsCoordinate& gps) {
  gps = {};
  if (parse_iso6709(text, gps)) {
    return true;
  }
  gps = {};
  return parse_comma_gps(text, gps);
}

std::string format_iso6709(const GpsCoordinate& gps) {
  std::ostringstream out;
  out << std::showpos << std::fixed << std::setprecision(4) << gps.latitude
      << gps.longitude;
  if (gps.altitude_meters) {
    out << std::setprecision(2) << *gps.altitude_meters;
  }
  out << '/';
  return out.str();
}

bool gps_equal(const GpsCoordinate& a, const GpsCoordinate& b) {
  if (std::fabs(a.latitude - b.latitude) > kGpsDegEps ||
      std::fabs(a.longitude - b.longitude) > kGpsDegEps) {
    return false;
  }
  if (a.altitude_meters && b.altitude_meters) {
    return std::fabs(*a.altitude_meters - *b.altitude_meters) <= kGpsAltEps;
  }
  return true;
}

int parse_i32(std::string_view text, int& out) {
  text = trim(text);
  if (text.empty()) {
    return 0;
  }
  std::string copy(text);
  char* end = nullptr;
  const long value = std::strtol(copy.c_str(), &end, 10);
  if (end == copy.c_str()) {
    return 0;
  }
  out = static_cast<int>(value);
  return static_cast<int>(end - copy.c_str());
}

bool parse_datetime_text(std::string_view text, DateTime& out) {
  text = trim(text);
  if (text.size() < 4) {
    return false;
  }
  DateTime dt;
  int year = 0;
  if (!parse_i32(text.substr(0, 4), year)) {
    return false;
  }
  dt.year = year;
  std::string_view rest = text.substr(4);
  auto take_sep_num = [&](std::optional<int>& slot) {
    if (rest.size() < 3) {
      return false;
    }
    if (rest.front() != ':' && rest.front() != '-') {
      return false;
    }
    int value = 0;
    if (!parse_i32(rest.substr(1, 2), value)) {
      return false;
    }
    slot = value;
    rest.remove_prefix(3);
    return true;
  };
  (void)take_sep_num(dt.month);
  (void)take_sep_num(dt.day);
  rest = trim(rest);
  if (!rest.empty() && (rest.front() == 'T' || rest.front() == 't' ||
                        rest.front() == ' ')) {
    rest.remove_prefix(1);
    rest = trim(rest);
  }
  if (rest.size() >= 2) {
    int hour = 0;
    if (parse_i32(rest.substr(0, 2), hour)) {
      dt.hour = hour;
      rest.remove_prefix(2);
      std::optional<int> minute;
      if (take_sep_num(minute)) {
        dt.minute = minute;
        std::optional<int> second;
        if (take_sep_num(second)) {
          dt.second = second;
        }
      }
    }
  }
  out = dt;
  return true;
}

bool datetime_equal(const DateTime& a, const DateTime& b) {
  if (a.year != b.year) {
    return false;
  }
  if (a.month && b.month && a.month != b.month) {
    return false;
  }
  if (a.day && b.day && a.day != b.day) {
    return false;
  }
  if (a.hour && b.hour && a.hour != b.hour) {
    return false;
  }
  if (a.minute && b.minute && a.minute != b.minute) {
    return false;
  }
  if (a.second && b.second && a.second != b.second) {
    return false;
  }
  return true;
}

std::string display_text(const Value& value) {
  if (const auto* text = std::get_if<std::string>(&value.data)) {
    return *text;
  }
  if (const auto* alt = std::get_if<LangAlt>(&value.data)) {
    const auto it = alt->find("x-default");
    if (it != alt->end()) {
      return it->second;
    }
    if (!alt->empty()) {
      return alt->begin()->second;
    }
  }
  if (const auto* list = std::get_if<std::vector<std::string>>(&value.data)) {
    std::string out;
    for (std::size_t i = 0; i < list->size(); ++i) {
      if (i != 0) {
        out += "; ";
      }
      out += (*list)[i];
    }
    return out;
  }
  return value.toString();
}

std::optional<double> as_double(const Value& value) {
  if (const auto* real = std::get_if<double>(&value.data)) {
    return *real;
  }
  if (const auto* integer = std::get_if<std::int64_t>(&value.data)) {
    return static_cast<double>(*integer);
  }
  if (const auto* text = std::get_if<std::string>(&value.data)) {
    double out = 0;
    if (parse_double(*text, out)) {
      return out;
    }
  }
  return std::nullopt;
}

std::optional<GpsCoordinate> gps_from_location(const Structure& fields) {
  const auto lat = fields.find("gpsLatitude");
  const auto lon = fields.find("gpsLongitude");
  if (lat == fields.end() || lon == fields.end()) {
    return std::nullopt;
  }
  const auto lat_v = as_double(lat->second);
  const auto lon_v = as_double(lon->second);
  if (!lat_v || !lon_v) {
    return std::nullopt;
  }
  GpsCoordinate gps;
  gps.latitude = *lat_v;
  gps.longitude = *lon_v;
  if (const auto alt = fields.find("gpsAltitude"); alt != fields.end()) {
    if (const auto alt_v = as_double(alt->second)) {
      gps.altitude_meters = *alt_v;
    }
  }
  return gps;
}

void put_gps(Structure& fields, const GpsCoordinate& gps) {
  fields.insert_or_assign("gpsLatitude", make_value(gps.latitude));
  fields.insert_or_assign("gpsLongitude", make_value(gps.longitude));
  if (gps.altitude_meters) {
    fields.insert_or_assign("gpsAltitude", make_value(*gps.altitude_meters));
  }
}

std::optional<GpsCoordinate> location_shot_gps(const Metadata& metadata) {
  const auto property = metadata.get("iptc.video.locationShot");
  if (!property) {
    return std::nullopt;
  }
  const auto* list = std::get_if<std::vector<Structure>>(&property->value.data);
  if (!list || list->empty()) {
    return std::nullopt;
  }
  return gps_from_location(list->front());
}

std::optional<GpsCoordinate> read_exif_gps(const Metadata& metadata) {
  const auto* lat = find_base(metadata, "Exif.GPSInfo.GPSLatitude");
  const auto* lon = find_base(metadata, "Exif.GPSInfo.GPSLongitude");
  if (!lat || !lon) {
    return std::nullopt;
  }
  auto lat_v = parse_gps_coord(lat->value);
  auto lon_v = parse_gps_coord(lon->value);
  if (!lat_v || !lon_v) {
    return std::nullopt;
  }
  if (const auto* lat_ref = find_base(metadata, "Exif.GPSInfo.GPSLatitudeRef")) {
    const auto ref = ascii_lower(lat_ref->value);
    if (!ref.empty() && (ref.front() == 's' || ref.front() == 'S')) {
      *lat_v = -std::fabs(*lat_v);
    } else if (!ref.empty() && (ref.front() == 'n' || ref.front() == 'N')) {
      *lat_v = std::fabs(*lat_v);
    }
  }
  if (const auto* lon_ref = find_base(metadata, "Exif.GPSInfo.GPSLongitudeRef")) {
    const auto ref = ascii_lower(lon_ref->value);
    if (!ref.empty() && (ref.front() == 'w' || ref.front() == 'W')) {
      *lon_v = -std::fabs(*lon_v);
    } else if (!ref.empty() && (ref.front() == 'e' || ref.front() == 'E')) {
      *lon_v = std::fabs(*lon_v);
    }
  }
  GpsCoordinate gps;
  gps.latitude = *lat_v;
  gps.longitude = *lon_v;
  if (const auto* alt = find_base(metadata, "Exif.GPSInfo.GPSAltitude")) {
    double meters = 0;
    if (parse_double(alt->value, meters)) {
      if (const auto* ref = find_base(metadata, "Exif.GPSInfo.GPSAltitudeRef")) {
        if (trim(ref->value) == "1") {
          meters = -std::fabs(meters);
        }
      }
      gps.altitude_meters = meters;
    }
  }
  return gps;
}

int location_role(const Metadata& metadata) {
  const auto* entry = find_base(metadata, "QuickTime.Keys.location.role");
  if (!entry) {
    return 0;
  }
  int role = 0;
  if (!parse_i32(entry->value, role)) {
    return 0;
  }
  return role;
}

bool container_gps_writable(const Capabilities* caps) {
  if (!caps) {
    return true;
  }
  for (const BackendCapability& backend : caps->backends) {
    if (!backend.available) {
      continue;
    }
    if (backend.location.container_gps == Access::read_write ||
        backend.location.container_gps == Access::create) {
      return true;
    }
  }
  return false;
}

bool group_selected(const CastOptions& options, std::string_view id) {
  if (options.groups.empty()) {
    return true;
  }
  for (const std::string& group : options.groups) {
    if (group == id) {
      return true;
    }
  }
  return false;
}

bool group_applies(std::string_view id, MediaDomain domain) {
  if (id == "capturePosition" || id == "videoCreated" || id == "videoModified" ||
      id == "recordingDevice" || id == "videoTechnical" ||
      id == "quickTimeUserData") {
    return domain == MediaDomain::video;
  }
  if (id == "locationShownLegacy" || id == "personShown" ||
      id == "creatorImageCreator" || id == "windowsXP" ||
      id == "exifUserComment" || id == "pngText") {
    return domain != MediaDomain::video;
  }
  return true;
}

CastDirection parse_direction(std::string_view text) {
  if (text == "down") {
    return CastDirection::down;
  }
  if (text == "side") {
    return CastDirection::side;
  }
  return CastDirection::up;
}

const CastGroupDef* find_group(std::string_view id, std::string_view direction) {
  for (const CastGroupDef& group : kCastGroups) {
    if (group.id == id && group.direction == direction) {
      return &group;
    }
  }
  return nullptr;
}

CastCandidate make_candidate(const CastGroupDef& group, CastStatus status,
                             std::string source, std::string target,
                             std::string source_preview,
                             std::string target_preview,
                             std::vector<std::string> notes) {
  CastCandidate candidate;
  candidate.group = std::string(group.id);
  candidate.direction = parse_direction(group.direction);
  candidate.status = status;
  candidate.source_id = std::move(source);
  candidate.target_id = std::move(target);
  candidate.source_preview = std::move(source_preview);
  candidate.target_preview = std::move(target_preview);
  candidate.notes = std::move(notes);
  if (group.approximate) {
    candidate.notes.push_back("approximate");
  }
  if (group.partial) {
    candidate.notes.push_back("partial");
  }
  return candidate;
}

CastCandidate capture_position_up(const Metadata& metadata,
                                  const CastGroupDef& group) {
  std::vector<std::string> notes;
  const int role = location_role(metadata);
  std::optional<GpsCoordinate> chosen;
  std::string source_id;
  std::string source_preview;
  auto consider = [&](std::string_view key, std::string_view label,
                      const std::optional<GpsCoordinate>& gps) {
    if (!gps) {
      return;
    }
    if (!chosen) {
      chosen = gps;
      source_id = std::string(key);
      source_preview = label;
      return;
    }
    if (!gps_equal(*chosen, *gps)) {
      notes.emplace_back("H17 source disagreement");
    }
  };
  if (role == 0) {
    if (const auto* keys = find_base(metadata, "QuickTime.Keys.location.ISO6709")) {
      GpsCoordinate gps;
      if (parse_position(keys->value, gps)) {
        consider(keys->key.key, keys->value, gps);
      }
    }
  } else {
    notes.emplace_back("H18 skipped Keys location for role != 0");
  }
  if (const auto* user = find_base(metadata, "QuickTime.UserData.GPSCoordinates")) {
    GpsCoordinate gps;
    if (parse_position(user->value, gps)) {
      consider(user->key.key, user->value, gps);
    }
  }
  if (const auto exif = read_exif_gps(metadata)) {
    consider("Exif.GPSInfo.GPSLatitude", "exif-gps", exif);
  }
  if (find_base(metadata, "Exif.GPSInfo.GPSDateStamp") ||
      find_base(metadata, "Exif.GPSInfo.GPSTimeStamp") ||
      find_base(metadata, "Exif.GPSInfo.GPSImgDirection") ||
      find_base(metadata, "Exif.GPSInfo.GPSSpeed")) {
    notes.emplace_back("H20 extra GPS tags stay unmapped");
  }
  const auto target = location_shot_gps(metadata);
  std::string target_preview = target ? format_iso6709(*target) : "";
  if (!chosen) {
    return make_candidate(group, CastStatus::source_empty, source_id,
                          "iptc.video.locationShot", source_preview,
                          target_preview, std::move(notes));
  }
  if (!target) {
    return make_candidate(group, CastStatus::can_cast, source_id,
                          "iptc.video.locationShot", source_preview,
                          target_preview, std::move(notes));
  }
  if (gps_equal(*chosen, *target)) {
    return make_candidate(group, CastStatus::equal, source_id,
                          "iptc.video.locationShot", source_preview,
                          target_preview, std::move(notes));
  }
  const auto list_prop = metadata.get("iptc.video.locationShot");
  const auto* list =
      list_prop ? std::get_if<std::vector<Structure>>(&list_prop->value.data)
                : nullptr;
  if (list && !list->empty()) {
    const bool has_lat = list->front().find("gpsLatitude") != list->front().end();
    if (has_lat) {
      notes.emplace_back("H3 merge never appends");
      return make_candidate(group, CastStatus::needs_force, source_id,
                            "iptc.video.locationShot", source_preview,
                            target_preview, std::move(notes));
    }
  }
  return make_candidate(group, CastStatus::can_cast, source_id,
                        "iptc.video.locationShot", source_preview,
                        target_preview, std::move(notes));
}

CastCandidate capture_position_down(const Metadata& metadata,
                                    const CastGroupDef& group,
                                    const Capabilities* caps) {
  const auto gps = location_shot_gps(metadata);
  const auto* existing =
      find_base(metadata, "QuickTime.Keys.location.ISO6709");
  const auto* existing_user =
      find_base(metadata, "QuickTime.UserData.GPSCoordinates");
  std::string target_preview;
  if (existing) {
    target_preview = existing->value;
  } else if (existing_user) {
    target_preview = existing_user->value;
  }
  if (!gps) {
    return make_candidate(group, CastStatus::source_empty,
                          "iptc.video.locationShot",
                          "QuickTime.Keys.location.ISO6709", "", target_preview,
                          {});
  }
  if (!container_gps_writable(caps)) {
    return make_candidate(group, CastStatus::target_not_storable,
                          "iptc.video.locationShot",
                          "QuickTime.Keys.location.ISO6709",
                          format_iso6709(*gps), target_preview, {});
  }
  GpsCoordinate have;
  if (existing && parse_position(existing->value, have) &&
      gps_equal(*gps, have)) {
    return make_candidate(group, CastStatus::equal, "iptc.video.locationShot",
                          "QuickTime.Keys.location.ISO6709",
                          format_iso6709(*gps), target_preview, {});
  }
  if (existing && parse_position(existing->value, have) &&
      !gps_equal(*gps, have)) {
    return make_candidate(group, CastStatus::needs_force,
                          "iptc.video.locationShot",
                          "QuickTime.Keys.location.ISO6709",
                          format_iso6709(*gps), target_preview,
                          {"existing QuickTime GPS differs"});
  }
  return make_candidate(group, CastStatus::can_cast, "iptc.video.locationShot",
                        "QuickTime.Keys.location.ISO6709", format_iso6709(*gps),
                        target_preview, {});
}

CastCandidate date_up(const Metadata& metadata, const CastGroupDef& group,
                      std::string_view source_key, std::string_view target_id) {
  const auto* source = find_base(metadata, source_key);
  const auto target = metadata.get(target_id);
  std::string target_preview = target ? display_text(target->value) : "";
  if (!source) {
    return make_candidate(group, CastStatus::source_empty, std::string(source_key),
                          std::string(target_id), "", target_preview, {});
  }
  DateTime parsed;
  if (!parse_datetime_text(source->value, parsed)) {
    return make_candidate(group, CastStatus::ambiguous, std::string(source_key),
                          std::string(target_id), source->value, target_preview,
                          {"unparsed date"});
  }
  if (target) {
    if (const auto* have = std::get_if<DateTime>(&target->value.data)) {
      if (datetime_equal(parsed, *have)) {
        return make_candidate(group, CastStatus::equal, std::string(source_key),
                              std::string(target_id), source->value,
                              target_preview, {});
      }
      return make_candidate(group, CastStatus::needs_force, std::string(source_key),
                            std::string(target_id), source->value, target_preview,
                            {});
    }
  }
  return make_candidate(group, CastStatus::can_cast, std::string(source_key),
                        std::string(target_id), source->value, target_preview,
                        {});
}

std::string struct_field_text(const Structure& fields, std::string_view name) {
  const auto it = fields.find(std::string(name));
  if (it == fields.end()) {
    return {};
  }
  return display_text(it->second);
}

CastCandidate recording_device_up(const Metadata& metadata,
                                  const CastGroupDef& group) {
  const auto existing = metadata.get("iptc.video.recordingDevice");
  const Structure* have =
      existing ? std::get_if<Structure>(&existing->value.data) : nullptr;
  Structure merged = have ? *have : Structure{};
  bool any = false;
  bool conflict = false;
  std::string source_preview;
  auto merge_field = [&](std::string_view key, std::string_view field) {
    const auto* entry = find_base(metadata, key);
    if (!entry) {
      return;
    }
    any = true;
    if (!source_preview.empty()) {
      source_preview += "; ";
    }
    source_preview += std::string(field) + "=" + entry->value;
    const std::string current = struct_field_text(merged, field);
    if (current.empty()) {
      merged.insert_or_assign(std::string(field), make_value(entry->value));
      return;
    }
    if (current != entry->value) {
      conflict = true;
    }
  };
  merge_field("QuickTime.Keys.Make", "manufacturer");
  merge_field("QuickTime.Keys.Model", "modelName");
  merge_field("Exif.Image.Make", "manufacturer");
  merge_field("Exif.Image.Model", "modelName");
  merge_field("Exif.Photo.BodySerialNumber", "serialNumber");
  merge_field("Exif.Photo.LensModel", "attLensDescription");
  merge_field("ExifTool.GoPro.Model", "modelName");
  merge_field("ExifTool.GoPro.CameraSerialNumber", "serialNumber");
  std::string target_preview = existing ? display_text(existing->value) : "";
  if (!any) {
    return make_candidate(group, CastStatus::source_empty, "recordingDevice",
                          "iptc.video.recordingDevice", "", target_preview, {});
  }
  if (conflict) {
    return make_candidate(group, CastStatus::needs_force, "recordingDevice",
                          "iptc.video.recordingDevice", source_preview,
                          target_preview, {"H8 field conflict"});
  }
  if (have && display_text(make_value(merged)) == display_text(existing->value)) {
    return make_candidate(group, CastStatus::equal, "recordingDevice",
                          "iptc.video.recordingDevice", source_preview,
                          target_preview, {});
  }
  return make_candidate(group, CastStatus::can_cast, "recordingDevice",
                        "iptc.video.recordingDevice", source_preview,
                        target_preview, {});
}

std::string location_field(const Metadata& metadata, std::string_view field) {
  const auto property = metadata.get("iptc.photo.locationShownInTheImage");
  if (!property) {
    return {};
  }
  const auto* list = std::get_if<std::vector<Structure>>(&property->value.data);
  if (!list || list->empty()) {
    return {};
  }
  return struct_field_text(list->front(), field);
}

std::string legacy_text(const Metadata& metadata, std::string_view id) {
  const auto property = metadata.get(id);
  if (!property) {
    return {};
  }
  return display_text(property->value);
}

CastCandidate location_shown_legacy(const Metadata& metadata,
                                    const CastGroupDef& group) {
  struct Pair {
    std::string_view legacy;
    std::string_view field;
    std::size_t limit;
  };
  const Pair pairs[] = {
      {"iptc.photo.cityLegacy", "city", 32},
      {"iptc.photo.provinceOrStateLegacy", "provinceState", 32},
      {"iptc.photo.countryLegacy", "countryName", 64},
      {"iptc.photo.countryCodeLegacy", "countryCode", 3},
      {"iptc.photo.sublocationLegacy", "sublocation", 32},
  };
  bool any_source = false;
  bool can = false;
  bool conflict = false;
  bool needs_trunc = false;
  std::string source_preview;
  std::string target_preview;
  for (const Pair& pair : pairs) {
    const std::string legacy = legacy_text(metadata, pair.legacy);
    const std::string shown = location_field(metadata, pair.field);
    if (!legacy.empty() || !shown.empty()) {
      any_source = true;
    }
    if (!source_preview.empty()) {
      source_preview += "; ";
    }
    source_preview += std::string(pair.field) + "=" + legacy;
    if (!target_preview.empty()) {
      target_preview += "; ";
    }
    target_preview += std::string(pair.field) + "=" + shown;
    if (!legacy.empty() && shown.empty()) {
      can = true;
    } else if (legacy.empty() && !shown.empty()) {
      can = true;
      if (shown.size() > pair.limit) {
        needs_trunc = true;
      }
    } else if (!legacy.empty() && !shown.empty() && legacy != shown) {
      conflict = true;
    }
  }
  if (!any_source) {
    return make_candidate(group, CastStatus::source_empty, "legacy location",
                          "iptc.photo.locationShownInTheImage", "", "", {});
  }
  if (conflict || needs_trunc) {
    std::vector<std::string> notes;
    if (needs_trunc) {
      notes.emplace_back("H14 truncation needs force");
    }
    if (conflict) {
      notes.emplace_back("H3 merge never appends");
    }
    return make_candidate(group, CastStatus::needs_force, "legacy location",
                          "iptc.photo.locationShownInTheImage", source_preview,
                          target_preview, std::move(notes));
  }
  if (!can) {
    return make_candidate(group, CastStatus::equal, "legacy location",
                          "iptc.photo.locationShownInTheImage", source_preview,
                          target_preview, {});
  }
  return make_candidate(group, CastStatus::can_cast, "legacy location",
                        "iptc.photo.locationShownInTheImage", source_preview,
                        target_preview, {});
}

std::vector<std::string> as_name_list(const Value& value) {
  if (const auto* list = std::get_if<std::vector<std::string>>(&value.data)) {
    return *list;
  }
  if (const auto* text = std::get_if<std::string>(&value.data)) {
    if (!text->empty()) {
      return {*text};
    }
  }
  return {};
}

std::string person_name(const Structure& fields) {
  std::string name = struct_field_text(fields, "name");
  if (name.empty()) {
    name = struct_field_text(fields, "PersonName");
  }
  return name;
}

std::string creator_name(const Structure& fields) {
  std::string name = struct_field_text(fields, "imageCreatorName");
  if (name.empty()) {
    name = struct_field_text(fields, "name");
  }
  return name;
}

bool contains_name(const std::vector<std::string>& names,
                   std::string_view name) {
  for (const std::string& item : names) {
    if (item == name) {
      return true;
    }
  }
  return false;
}

CastCandidate list_side(const Metadata& metadata, const CastGroupDef& group,
                        std::string_view left_id, std::string_view right_id,
                        bool right_is_person) {
  const auto left = metadata.get(left_id);
  const auto right = metadata.get(right_id);
  std::vector<std::string> left_names = left ? as_name_list(left->value) : std::vector<std::string>{};
  std::vector<std::string> right_names;
  if (right) {
    if (const auto* list = std::get_if<std::vector<Structure>>(&right->value.data)) {
      for (const Structure& item : *list) {
        const std::string name =
            right_is_person ? person_name(item) : creator_name(item);
        if (!name.empty()) {
          right_names.push_back(name);
        }
      }
    }
  }
  bool missing = false;
  for (const std::string& name : left_names) {
    if (!contains_name(right_names, name)) {
      missing = true;
    }
  }
  for (const std::string& name : right_names) {
    if (!contains_name(left_names, name)) {
      missing = true;
    }
  }
  std::string source_preview;
  for (std::size_t i = 0; i < left_names.size(); ++i) {
    if (i != 0) {
      source_preview += "; ";
    }
    source_preview += left_names[i];
  }
  std::string target_preview;
  for (std::size_t i = 0; i < right_names.size(); ++i) {
    if (i != 0) {
      target_preview += "; ";
    }
    target_preview += right_names[i];
  }
  if (left_names.empty() && right_names.empty()) {
    return make_candidate(group, CastStatus::source_empty, std::string(left_id),
                          std::string(right_id), "", "", {});
  }
  if (!missing) {
    return make_candidate(group, CastStatus::equal, std::string(left_id),
                          std::string(right_id), source_preview, target_preview,
                          {});
  }
  return make_candidate(group, CastStatus::can_cast, std::string(left_id),
                        std::string(right_id), source_preview, target_preview,
                        {});
}

std::string format_decimal(double value) {
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out << std::setprecision(6) << value;
  std::string text = out.str();
  while (text.size() > 1 && text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
  if (text.empty() || text == "-") {
    return "0";
  }
  return text;
}

std::string trimmed_copy(std::string_view text) {
  return std::string(trim(text));
}

bool duration_seconds(std::string_view text, bool milliseconds, double& seconds) {
  std::string_view body = trim(text);
  if (body.size() > 2 && body.substr(body.size() - 2) == " s") {
    body = trim(body.substr(0, body.size() - 2));
  }
  double number = 0;
  if (!parse_double(body, number)) {
    return false;
  }
  seconds = milliseconds ? number / 1000.0 : number;
  return seconds >= 0;
}

Value video_time_value(double seconds) {
  Structure fields;
  fields.insert_or_assign("timeFormat", make_value(std::string("seconds")));
  fields.insert_or_assign("timeValue", make_value(format_decimal(seconds)));
  return make_value(std::move(fields));
}

bool parse_channels(std::string_view text, double& out) {
  if (parse_double(trim(text), out)) {
    return out > 0;
  }
  std::string lower = trimmed_copy(text);
  for (char& ch : lower) {
    if (ch >= 'A' && ch <= 'Z') {
      ch = static_cast<char>(ch - 'A' + 'a');
    }
  }
  if (lower.find("stereo") != std::string::npos) {
    out = 2;
    return true;
  }
  if (lower.find("mono") != std::string::npos) {
    out = 1;
    return true;
  }
  return false;
}

bool parse_rotation_degrees(std::string_view text, double& degrees) {
  text = trim(text);
  std::size_t index = 0;
  if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
    index = 1;
  }
  const std::size_t start = 0;
  while (index < text.size() &&
         ((text[index] >= '0' && text[index] <= '9') || text[index] == '.')) {
    ++index;
  }
  if (index == start || (text[0] == '-' || text[0] == '+') && index == 1) {
    return false;
  }
  return parse_double(text.substr(0, index), degrees);
}

int orientation_from_degrees(double degrees) {
  int turns = static_cast<int>(std::lround(degrees)) % 360;
  if (turns < 0) {
    turns += 360;
  }
  if (turns == 0) {
    return 1;
  }
  if (turns == 90) {
    return 6;
  }
  if (turns == 180) {
    return 3;
  }
  if (turns == 270) {
    return 8;
  }
  return 0;
}

bool parse_matrix_orientation(std::string_view text, int& orientation) {
  double values[9];
  std::size_t count = 0;
  std::size_t index = 0;
  text = trim(text);
  while (index < text.size() && count < 9) {
    while (index < text.size() && text[index] == ' ') {
      ++index;
    }
    if (index >= text.size()) {
      break;
    }
    const std::size_t start = index;
    if (text[index] == '-' || text[index] == '+') {
      ++index;
    }
    bool digit = false;
    while (index < text.size() &&
           ((text[index] >= '0' && text[index] <= '9') || text[index] == '.')) {
      digit = true;
      ++index;
    }
    if (!digit || !parse_double(text.substr(start, index - start), values[count])) {
      return false;
    }
    ++count;
  }
  if (count != 9) {
    return false;
  }
  const double a = values[0];
  const double b = values[1];
  const double d = values[3];
  const double e = values[4];
  if ((a * e - b * d) <= 0.5) {
    return false;
  }
  const double degrees = std::atan2(b, a) * 180.0 / 3.14159265358979323846;
  orientation = orientation_from_degrees(degrees);
  return orientation != 0;
}

const CastRuleDef* rule_by_id(const CastGroupDef& group, std::string_view id) {
  for (const CastRuleDef& rule : kCastRules) {
    if (rule.group == group.id && rule.direction == group.direction &&
        rule.id == id) {
      return &rule;
    }
  }
  return nullptr;
}

struct FieldHit {
  std::string field;
  std::string source_key;
  std::string preview;
  Value value;
};

bool field_filled(const std::vector<FieldHit>& hits, std::string_view field) {
  for (const FieldHit& hit : hits) {
    if (hit.field == field) {
      return true;
    }
  }
  return false;
}

std::optional<FieldHit> interpret_technical_rule(const Metadata& metadata,
                                                 const CastRuleDef& rule) {
  const BaseEntry* entry = find_base(metadata, rule.source_key);
  if (!entry) {
    return std::nullopt;
  }
  const std::string target(rule.target_key);
  FieldHit hit;
  hit.field = std::string(rule.target_field);
  hit.source_key = std::string(rule.source_key);
  hit.preview = trimmed_copy(entry->value);
  if (target == "iptc.video.fileDuration") {
    double seconds = 0;
    if (!duration_seconds(entry->value, rule.source_key == "Xmp.video.Duration",
                          seconds)) {
      return std::nullopt;
    }
    hit.value = video_time_value(seconds);
    return hit;
  }
  if (target == "iptc.video.frameSize") {
    double pixels = 0;
    if (!parse_double(trim(entry->value), pixels) || pixels <= 0) {
      return std::nullopt;
    }
    hit.value = make_value(pixels);
    return hit;
  }
  if (target == "iptc.video.videoFrameRate" ||
      target == "iptc.video.audioSampleRate") {
    double number = 0;
    if (!parse_double(trim(entry->value), number) || number <= 0) {
      return std::nullopt;
    }
    hit.value = make_value(number);
    return hit;
  }
  if (target == "iptc.video.audioChannels") {
    double number = 0;
    if (!parse_channels(entry->value, number)) {
      return std::nullopt;
    }
    hit.value = make_value(number);
    return hit;
  }
  if (target == "iptc.video.videoCoding") {
    const std::string text = trimmed_copy(entry->value);
    if (text.empty()) {
      return std::nullopt;
    }
    if (hit.field == "identifiers") {
      hit.value = make_value(std::vector<std::string>{text});
      return hit;
    }
    if (hit.field == "name") {
      hit.value = make_value(LangAlt{{"x-default", text}});
      return hit;
    }
    return std::nullopt;
  }
  if (target == "iptc.video.orientation") {
    int orientation = 0;
    if (rule.source_key.find("MatrixStructure") != std::string_view::npos) {
      if (!parse_matrix_orientation(entry->value, orientation)) {
        return std::nullopt;
      }
    } else {
      double degrees = 0;
      if (!parse_rotation_degrees(entry->value, degrees)) {
        return std::nullopt;
      }
      orientation = orientation_from_degrees(degrees);
      if (orientation == 0) {
        return std::nullopt;
      }
    }
    hit.value = make_value(static_cast<double>(orientation));
    return hit;
  }
  return std::nullopt;
}

struct TechBuilt {
  std::string target_id;
  std::string source_id;
  std::string source_preview;
  Value value;
};

struct TargetAcc {
  std::string target;
  std::vector<FieldHit> hits;
};

bool target_complete(std::string_view target, const std::vector<FieldHit>& hits) {
  if (hits.empty()) {
    return false;
  }
  if (target == "iptc.video.frameSize") {
    return field_filled(hits, "widthPixels") && field_filled(hits, "heightPixels");
  }
  return true;
}

Value assemble_target(std::string_view target, const std::vector<FieldHit>& hits) {
  if (target == "iptc.video.frameSize") {
    Structure fields;
    for (const FieldHit& hit : hits) {
      fields.insert_or_assign(hit.field, hit.value);
    }
    fields.insert_or_assign("unitPixels", make_value(std::string("pixels")));
    return make_value(std::move(fields));
  }
  if (target == "iptc.video.videoCoding") {
    Structure fields;
    for (const FieldHit& hit : hits) {
      fields.insert_or_assign(hit.field, hit.value);
    }
    return make_value(std::move(fields));
  }
  return hits.front().value;
}

std::vector<TechBuilt> build_video_technical(const Metadata& metadata,
                                             const CastGroupDef& group) {
  std::vector<TargetAcc> accs;
  for (std::size_t i = 0; i < group.source_priority_count; ++i) {
    const CastRuleDef* rule = rule_by_id(group, group.source_priority[i]);
    if (!rule) {
      continue;
    }
    TargetAcc* acc = nullptr;
    for (TargetAcc& item : accs) {
      if (item.target == rule->target_key) {
        acc = &item;
        break;
      }
    }
    if (!acc) {
      accs.push_back(TargetAcc{std::string(rule->target_key), {}});
      acc = &accs.back();
    }
    if (field_filled(acc->hits, rule->target_field)) {
      continue;
    }
    if (std::optional<FieldHit> hit = interpret_technical_rule(metadata, *rule)) {
      acc->hits.push_back(std::move(*hit));
    }
  }
  std::vector<TechBuilt> built;
  for (const TargetAcc& acc : accs) {
    if (!target_complete(acc.target, acc.hits)) {
      continue;
    }
    TechBuilt item;
    item.target_id = acc.target;
    item.source_id = acc.hits.front().source_key;
    for (std::size_t i = 0; i < acc.hits.size(); ++i) {
      if (i != 0) {
        item.source_preview += "; ";
      }
      item.source_preview += acc.hits[i].preview;
    }
    item.value = assemble_target(acc.target, acc.hits);
    built.push_back(std::move(item));
  }
  return built;
}

CastStatus technical_status(const Metadata& metadata, const TechBuilt& built) {
  const auto existing = metadata.get(built.target_id);
  if (!existing) {
    return CastStatus::can_cast;
  }
  if (display_text(existing->value) == display_text(built.value)) {
    return CastStatus::equal;
  }
  return CastStatus::needs_force;
}

std::vector<CastCandidate> evaluate_video_technical(const Metadata& metadata,
                                                    const CastGroupDef& group) {
  std::vector<CastCandidate> out;
  for (const TechBuilt& built : build_video_technical(metadata, group)) {
    out.push_back(make_candidate(group, technical_status(metadata, built),
                                 built.source_id, built.target_id,
                                 built.source_preview, display_text(built.value),
                                 {}));
  }
  return out;
}

void apply_video_technical(Metadata& metadata, const CastCandidate& candidate,
                           bool force) {
  if (candidate.status != CastStatus::can_cast &&
      !(force && candidate.status == CastStatus::needs_force)) {
    return;
  }
  const CastGroupDef* group = find_group("videoTechnical", "up");
  if (!group) {
    return;
  }
  for (const TechBuilt& built : build_video_technical(metadata, *group)) {
    if (built.target_id == candidate.target_id) {
      (void)metadata.set(built.target_id, built.value);
      return;
    }
  }
}

bool is_text_group(std::string_view id) {
  return id == "windowsXP" || id == "exifUserComment" || id == "pngText" ||
         id == "quickTimeUserData";
}

void append_utf8(std::string& out, std::uint32_t codepoint) {
  if (codepoint < 0x80) {
    out.push_back(static_cast<char>(codepoint));
    return;
  }
  if (codepoint < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    return;
  }
  if (codepoint < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
}

std::optional<std::vector<unsigned char>> parse_byte_list(std::string_view text) {
  text = trim(text);
  if (text.empty()) {
    return std::nullopt;
  }
  std::vector<unsigned char> bytes;
  std::size_t index = 0;
  while (index < text.size()) {
    while (index < text.size() && text[index] == ' ') {
      ++index;
    }
    if (index >= text.size()) {
      break;
    }
    if (text[index] < '0' || text[index] > '9') {
      return std::nullopt;
    }
    int value = 0;
    int digits = 0;
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
      value = value * 10 + (text[index] - '0');
      if (value > 255 || digits == 3) {
        return std::nullopt;
      }
      ++digits;
      ++index;
    }
    bytes.push_back(static_cast<unsigned char>(value));
  }
  if (bytes.size() < 4 || (bytes.size() % 2) != 0) {
    return std::nullopt;
  }
  return bytes;
}

std::optional<std::string> utf8_from_ucs2(const std::vector<unsigned char>& bytes) {
  if (bytes.size() % 2 != 0) {
    return std::nullopt;
  }
  std::string out;
  std::size_t index = 0;
  if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
    index = 2;
  }
  for (; index + 1 < bytes.size(); index += 2) {
    const std::uint32_t codepoint =
        bytes[index] | (static_cast<std::uint32_t>(bytes[index + 1]) << 8);
    if (codepoint == 0) {
      break;
    }
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
      return std::nullopt;
    }
    append_utf8(out, codepoint);
  }
  if (out.empty()) {
    return std::nullopt;
  }
  return out;
}

std::optional<std::string> latin1_utf8(const std::vector<unsigned char>& bytes) {
  std::string out;
  for (const unsigned char byte : bytes) {
    if (byte == 0) {
      break;
    }
    append_utf8(out, byte);
  }
  if (out.empty()) {
    return std::nullopt;
  }
  return out;
}

bool take_charset_prefix(std::string& body, std::string& charset) {
  constexpr std::string_view kPrefix = "charset=";
  if (body.size() < kPrefix.size() ||
      ascii_lower(std::string_view(body).substr(0, kPrefix.size())) != kPrefix) {
    return false;
  }
  std::string_view rest = std::string_view(body).substr(kPrefix.size());
  if (!rest.empty() && rest.front() == '"') {
    rest.remove_prefix(1);
    const auto end = rest.find('"');
    if (end == std::string_view::npos) {
      return false;
    }
    charset = ascii_lower(rest.substr(0, end));
    rest.remove_prefix(end + 1);
  } else {
    const auto space = rest.find(' ');
    if (space == std::string_view::npos) {
      charset = ascii_lower(rest);
      rest = {};
    } else {
      charset = ascii_lower(rest.substr(0, space));
      rest.remove_prefix(space + 1);
    }
  }
  if (!rest.empty() && rest.front() == ' ') {
    rest.remove_prefix(1);
  }
  body = std::string(rest);
  return true;
}

bool take_eight_byte_charset(std::string& body, std::string& charset) {
  if (body.size() < 8) {
    return false;
  }
  for (std::size_t i = 0; i < 8; ++i) {
    const unsigned char byte = static_cast<unsigned char>(body[i]);
    if (byte > 127) {
      return false;
    }
  }
  std::string id = body.substr(0, 8);
  while (!id.empty() && (id.back() == '\0' || id.back() == ' ')) {
    id.pop_back();
  }
  const std::string lower = ascii_lower(id);
  if (lower != "ascii" && lower != "unicode" && lower != "jis" &&
      lower != "undefined") {
    return false;
  }
  charset = lower;
  body = body.substr(8);
  return true;
}

std::optional<std::string> decode_legacy_text(std::string_view raw, bool user_comment,
                                              bool& ambiguous) {
  ambiguous = false;
  std::string body(trim(raw));
  std::string charset;
  if (user_comment) {
    if (!take_charset_prefix(body, charset)) {
      (void)take_eight_byte_charset(body, charset);
    }
    charset = ascii_lower(charset);
    if (charset == "jis") {
      ambiguous = true;
      return std::nullopt;
    }
  }
  if (const auto bytes = parse_byte_list(body)) {
    if (!user_comment || charset == "unicode") {
      return utf8_from_ucs2(*bytes);
    }
    return latin1_utf8(*bytes);
  }
  if (body.find('\0') != std::string::npos) {
    const std::vector<unsigned char> bytes(body.begin(), body.end());
    if (!user_comment || charset == "unicode") {
      return utf8_from_ucs2(bytes);
    }
    return latin1_utf8(bytes);
  }
  body = std::string(trim(body));
  if (body.empty()) {
    return std::nullopt;
  }
  return body;
}

void attach_offset(std::string_view text, DateTime& dt) {
  text = trim(text);
  if (text.empty()) {
    return;
  }
  if (text.back() == 'Z' || text.back() == 'z') {
    if (text.size() > 1 && text[text.size() - 2] >= '0' &&
        text[text.size() - 2] <= '9') {
      dt.utc_offset_minutes = 0;
    }
    return;
  }
  const auto sign = text.find_last_of("+-");
  if (sign == std::string_view::npos || sign == 0) {
    return;
  }
  if (text[sign - 1] < '0' || text[sign - 1] > '9') {
    return;
  }
  const std::string_view tail = text.substr(sign + 1);
  int hour = 0;
  int minute = 0;
  if (tail.size() == 5 && tail[2] == ':') {
    if (parse_i32(tail.substr(0, 2), hour) != 2 ||
        parse_i32(tail.substr(3, 2), minute) != 2) {
      return;
    }
  } else if (tail.size() == 4) {
    if (parse_i32(tail.substr(0, 2), hour) != 2 ||
        parse_i32(tail.substr(2, 2), minute) != 2) {
      return;
    }
  } else {
    return;
  }
  if (hour > 18 || minute > 59) {
    return;
  }
  const int total = (text[sign] == '-' ? -1 : 1) * (hour * 60 + minute);
  if (total < -18 * 60 || total > 18 * 60) {
    return;
  }
  dt.utc_offset_minutes = total;
}

bool parse_text_date(std::string_view text, DateTime& out) {
  if (!parse_datetime_text(text, out)) {
    return false;
  }
  attach_offset(text, out);
  return true;
}

std::vector<std::string> split_semicolon(std::string_view text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= text.size()) {
    const auto pos = text.find(';', start);
    const std::string part = std::string(
        trim(pos == std::string_view::npos ? text.substr(start)
                                           : text.substr(start, pos - start)));
    if (!part.empty()) {
      out.push_back(part);
    }
    if (pos == std::string_view::npos) {
      break;
    }
    start = pos + 1;
  }
  return out;
}

enum class TextShape { lang_alt, string_list, entity_list, date };

TextShape text_shape(std::string_view target) {
  if (target == "iptc.video.dateCreated") {
    return TextShape::date;
  }
  if (target == "iptc.video.creator") {
    return TextShape::entity_list;
  }
  if (target == "iptc.photo.keywords" || target == "iptc.photo.creator") {
    return TextShape::string_list;
  }
  return TextShape::lang_alt;
}

Value lang_alt_value(const std::string& text) {
  return make_value(LangAlt{{"x-default", text}});
}

Value string_list_value(const std::vector<std::string>& names) {
  return make_value(names);
}

Value entity_list_value(const std::vector<std::string>& names) {
  std::vector<Structure> entities;
  for (const std::string& name : names) {
    Structure entity;
    entity.emplace("name", lang_alt_value(name));
    entities.push_back(std::move(entity));
  }
  return make_value(std::move(entities));
}

std::vector<std::string> entity_names(const Value& value) {
  const auto* list = std::get_if<std::vector<Structure>>(&value.data);
  if (!list) {
    return {};
  }
  std::vector<std::string> names;
  for (const Structure& item : *list) {
    const auto it = item.find("name");
    if (it == item.end()) {
      continue;
    }
    const std::string name = display_text(it->second);
    if (!name.empty()) {
      names.push_back(name);
    }
  }
  return names;
}

bool same_string_set(std::vector<std::string> left, std::vector<std::string> right) {
  std::sort(left.begin(), left.end());
  std::sort(right.begin(), right.end());
  return left == right;
}

bool text_dates_equal(const DateTime& left, const DateTime& right) {
  if (!datetime_equal(left, right)) {
    return false;
  }
  return left.utc_offset_minutes == right.utc_offset_minutes;
}

bool values_equal(TextShape shape, const Value& existing, const Value& built) {
  if (shape == TextShape::date) {
    const auto* left = std::get_if<DateTime>(&existing.data);
    const auto* right = std::get_if<DateTime>(&built.data);
    return left && right && text_dates_equal(*left, *right);
  }
  if (shape == TextShape::string_list) {
    return same_string_set(as_name_list(existing), as_name_list(built));
  }
  if (shape == TextShape::entity_list) {
    return same_string_set(entity_names(existing), entity_names(built));
  }
  return display_text(existing) == display_text(built);
}

struct TextBuilt {
  std::string target_id;
  std::string source_id;
  std::string source_preview;
  Value value;
  std::vector<std::string> notes;
  bool ambiguous = false;
};

std::vector<TextBuilt> build_text_group(const Metadata& metadata,
                                        const CastGroupDef& group) {
  struct Acc {
    std::string target;
    std::optional<TextBuilt> built;
    bool saw_source = false;
    bool ambiguous = false;
  };
  std::vector<Acc> accs;
  const bool user_comment = group.id == "exifUserComment";
  const bool legacy = group.id == "windowsXP" || user_comment;
  for (std::size_t i = 0; i < group.source_priority_count; ++i) {
    const CastRuleDef* rule = rule_by_id(group, group.source_priority[i]);
    if (!rule || rule->source_kind != "base_key") {
      continue;
    }
    Acc* acc = nullptr;
    for (Acc& item : accs) {
      if (item.target == rule->target_key) {
        acc = &item;
        break;
      }
    }
    if (!acc) {
      accs.push_back(Acc{std::string(rule->target_key), std::nullopt, false, false});
      acc = &accs.back();
    }
    const BaseEntry* entry = find_base(metadata, rule->source_key);
    if (!entry) {
      continue;
    }
    acc->saw_source = true;
    bool ambiguous = false;
    std::optional<std::string> decoded;
    if (legacy) {
      decoded = decode_legacy_text(entry->value, user_comment, ambiguous);
    } else {
      const std::string plain(trim(entry->value));
      if (!plain.empty()) {
        decoded = plain;
      }
    }
    if (!decoded) {
      if (ambiguous && !acc->built) {
        acc->ambiguous = true;
      }
      continue;
    }
    const TextShape shape = text_shape(rule->target_key);
    Value value;
    std::string preview = *decoded;
    if (shape == TextShape::date) {
      DateTime parsed;
      if (!parse_text_date(*decoded, parsed)) {
        if (!acc->built) {
          acc->ambiguous = true;
        }
        continue;
      }
      value = make_value(parsed);
      preview = display_text(value);
    } else if (shape == TextShape::string_list || shape == TextShape::entity_list) {
      const std::vector<std::string> names = split_semicolon(*decoded);
      if (names.empty()) {
        continue;
      }
      value = shape == TextShape::entity_list ? entity_list_value(names)
                                              : string_list_value(names);
      preview.clear();
      for (std::size_t n = 0; n < names.size(); ++n) {
        if (n != 0) {
          preview += "; ";
        }
        preview += names[n];
      }
    } else {
      value = lang_alt_value(*decoded);
    }
    if (!acc->built) {
      TextBuilt built;
      built.target_id = std::string(rule->target_key);
      built.source_id = std::string(rule->source_key);
      built.source_preview = std::move(preview);
      built.value = std::move(value);
      acc->built = std::move(built);
      acc->ambiguous = false;
      continue;
    }
    if (acc->built->source_preview != preview) {
      bool noted = false;
      for (const std::string& note : acc->built->notes) {
        if (note == "H17") {
          noted = true;
          break;
        }
      }
      if (!noted) {
        acc->built->notes.emplace_back("H17");
      }
    }
  }
  std::vector<TextBuilt> built;
  for (Acc& acc : accs) {
    if (acc.built) {
      built.push_back(std::move(*acc.built));
      continue;
    }
    if (acc.saw_source && acc.ambiguous) {
      TextBuilt item;
      item.target_id = acc.target;
      item.ambiguous = true;
      item.notes.emplace_back("unparsed text");
      built.push_back(std::move(item));
    }
  }
  return built;
}

CastStatus text_status(const Metadata& metadata, const TextBuilt& built) {
  if (built.ambiguous) {
    return CastStatus::ambiguous;
  }
  const auto existing = metadata.get(built.target_id);
  if (!existing) {
    return CastStatus::can_cast;
  }
  if (values_equal(text_shape(built.target_id), existing->value, built.value)) {
    return CastStatus::equal;
  }
  return CastStatus::needs_force;
}

std::vector<CastCandidate> evaluate_text_group(const Metadata& metadata,
                                               const CastGroupDef& group) {
  std::vector<CastCandidate> out;
  for (const TextBuilt& built : build_text_group(metadata, group)) {
    out.push_back(make_candidate(group, text_status(metadata, built),
                                 built.source_id, built.target_id,
                                 built.source_preview, display_text(built.value),
                                 built.notes));
  }
  return out;
}

bool target_has_value(const Metadata& metadata, std::string_view id) {
  const auto property = metadata.get(id);
  if (!property) {
    return false;
  }
  if (text_shape(id) == TextShape::date) {
    return std::holds_alternative<DateTime>(property->value.data);
  }
  if (text_shape(id) == TextShape::entity_list) {
    return !entity_names(property->value).empty();
  }
  if (text_shape(id) == TextShape::string_list) {
    return !as_name_list(property->value).empty();
  }
  return !display_text(property->value).empty();
}

void apply_text_group(Metadata& metadata, const CastCandidate& candidate,
                      bool force) {
  if (candidate.status != CastStatus::can_cast &&
      !(force && candidate.status == CastStatus::needs_force)) {
    return;
  }
  if (!force && target_has_value(metadata, candidate.target_id)) {
    return;
  }
  const CastGroupDef* group = find_group(candidate.group, "up");
  if (!group) {
    return;
  }
  for (const TextBuilt& built : build_text_group(metadata, *group)) {
    if (built.target_id == candidate.target_id && !built.ambiguous) {
      (void)metadata.set(built.target_id, built.value);
      return;
    }
  }
}

CastCandidate evaluate_group(const Metadata& metadata, const CastGroupDef& group,
                             const Capabilities* caps) {
  if (group.id == "capturePosition" && group.direction == "up") {
    return capture_position_up(metadata, group);
  }
  if (group.id == "capturePosition" && group.direction == "down") {
    return capture_position_down(metadata, group, caps);
  }
  if (group.id == "videoCreated") {
    return date_up(metadata, group, "QuickTime.CreateDate",
                   "iptc.video.dateCreated");
  }
  if (group.id == "videoModified") {
    return date_up(metadata, group, "QuickTime.ModifyDate",
                   "iptc.video.dateModified");
  }
  if (group.id == "recordingDevice") {
    return recording_device_up(metadata, group);
  }
  if (group.id == "locationShownLegacy") {
    return location_shown_legacy(metadata, group);
  }
  if (group.id == "personShown") {
    return list_side(metadata, group, "iptc.photo.personShownInTheImage",
                     "iptc.photo.personShownInTheImageWithDetails", true);
  }
  if (group.id == "creatorImageCreator") {
    return list_side(metadata, group, "iptc.photo.creator",
                     "iptc.photo.imageCreator", false);
  }
  return make_candidate(group, CastStatus::source_empty, "", "", "", "", {});
}

void apply_capture_up(Metadata& metadata, const CastCandidate& candidate,
                      bool force) {
  if (candidate.status != CastStatus::can_cast &&
      !(force && candidate.status == CastStatus::needs_force)) {
    return;
  }
  const auto* source = find_base(metadata, candidate.source_id);
  GpsCoordinate gps;
  if (candidate.source_id == "Exif.GPSInfo.GPSLatitude") {
    const auto parsed = read_exif_gps(metadata);
    if (!parsed) {
      return;
    }
    gps = *parsed;
  } else if (!source || !parse_position(source->value, gps)) {
    return;
  }
  std::vector<Structure> list;
  if (const auto property = metadata.get("iptc.video.locationShot")) {
    if (const auto* have =
            std::get_if<std::vector<Structure>>(&property->value.data)) {
      list = *have;
    }
  }
  if (list.empty()) {
    list.emplace_back();
  }
  put_gps(list.front(), gps);
  (void)metadata.set("iptc.video.locationShot", make_value(std::move(list)));
}

void apply_capture_down(const Metadata& metadata, const CastCandidate& candidate,
                        bool force, BaseChanges* extra) {
  if (!extra) {
    return;
  }
  if (candidate.status != CastStatus::can_cast &&
      !(force && candidate.status == CastStatus::needs_force)) {
    return;
  }
  const auto gps = location_shot_gps(metadata);
  if (!gps) {
    return;
  }
  const std::string iso = format_iso6709(*gps);
  extra->upserts.push_back(
      BaseEntry{{"QuickTime", "QuickTime.Keys.location.ISO6709"}, "string", iso});
  extra->upserts.push_back(
      BaseEntry{{"QuickTime", "QuickTime.UserData.GPSCoordinates"}, "string", iso});
}

void apply_date_up(Metadata& metadata, const CastCandidate& candidate,
                   std::string_view target_id, bool force) {
  if (candidate.status != CastStatus::can_cast &&
      !(force && candidate.status == CastStatus::needs_force)) {
    return;
  }
  const auto* source = find_base(metadata, candidate.source_id);
  DateTime parsed;
  if (!source || !parse_datetime_text(source->value, parsed)) {
    return;
  }
  (void)metadata.set(target_id, make_value(parsed));
}

void apply_recording_device(Metadata& metadata, const CastCandidate& candidate,
                            bool force) {
  if (candidate.status != CastStatus::can_cast &&
      !(force && candidate.status == CastStatus::needs_force)) {
    return;
  }
  Structure fields;
  if (const auto existing = metadata.get("iptc.video.recordingDevice")) {
    if (const auto* have = std::get_if<Structure>(&existing->value.data)) {
      fields = *have;
    }
  }
  auto fill = [&](std::string_view key, std::string_view field) {
    const auto* entry = find_base(metadata, key);
    if (!entry) {
      return;
    }
    const std::string current = struct_field_text(fields, field);
    if (current.empty() || force) {
      fields.insert_or_assign(std::string(field), make_value(entry->value));
    }
  };
  fill("QuickTime.Keys.Make", "manufacturer");
  fill("QuickTime.Keys.Model", "modelName");
  fill("Exif.Image.Make", "manufacturer");
  fill("Exif.Image.Model", "modelName");
  fill("Exif.Photo.BodySerialNumber", "serialNumber");
  fill("Exif.Photo.LensModel", "attLensDescription");
  fill("ExifTool.GoPro.Model", "modelName");
  fill("ExifTool.GoPro.CameraSerialNumber", "serialNumber");
  (void)metadata.set("iptc.video.recordingDevice", make_value(std::move(fields)));
}

void apply_location_legacy(Metadata& metadata, const CastCandidate& candidate,
                           bool force) {
  if (candidate.status != CastStatus::can_cast &&
      !(force && candidate.status == CastStatus::needs_force)) {
    return;
  }
  struct Pair {
    std::string_view legacy;
    std::string_view field;
    std::size_t limit;
  };
  const Pair pairs[] = {
      {"iptc.photo.cityLegacy", "city", 32},
      {"iptc.photo.provinceOrStateLegacy", "provinceState", 32},
      {"iptc.photo.countryLegacy", "countryName", 64},
      {"iptc.photo.countryCodeLegacy", "countryCode", 3},
      {"iptc.photo.sublocationLegacy", "sublocation", 32},
  };
  std::vector<Structure> list;
  if (const auto property = metadata.get("iptc.photo.locationShownInTheImage")) {
    if (const auto* have =
            std::get_if<std::vector<Structure>>(&property->value.data)) {
      list = *have;
    }
  }
  if (list.empty()) {
    list.emplace_back();
  }
  for (const Pair& pair : pairs) {
    std::string legacy = legacy_text(metadata, pair.legacy);
    std::string shown = struct_field_text(list.front(), pair.field);
    if (legacy.empty() && shown.empty()) {
      continue;
    }
    if (shown.empty() && !legacy.empty()) {
      list.front().insert_or_assign(std::string(pair.field), make_value(legacy));
      shown = legacy;
    } else if (legacy.empty() && !shown.empty()) {
      std::string write = shown;
      if (write.size() > pair.limit) {
        if (!force) {
          continue;
        }
        write.resize(pair.limit);
      }
      (void)metadata.set(pair.legacy, make_value(write));
    } else if (legacy != shown && force) {
      list.front().insert_or_assign(std::string(pair.field), make_value(legacy));
    }
  }
  (void)metadata.set("iptc.photo.locationShownInTheImage",
                     make_value(std::move(list)));
}

void apply_list_side(Metadata& metadata, const CastCandidate& candidate,
                     std::string_view left_id, std::string_view right_id,
                     bool right_is_person, bool force) {
  (void)force;
  if (candidate.status != CastStatus::can_cast &&
      candidate.status != CastStatus::needs_force) {
    return;
  }
  const auto left = metadata.get(left_id);
  const auto right = metadata.get(right_id);
  std::vector<std::string> names = left ? as_name_list(left->value)
                                        : std::vector<std::string>{};
  std::vector<Structure> details;
  if (right) {
    if (const auto* list =
            std::get_if<std::vector<Structure>>(&right->value.data)) {
      details = *list;
    }
  }
  auto detail_names = [&] {
    std::vector<std::string> out;
    for (const Structure& item : details) {
      const std::string name =
          right_is_person ? person_name(item) : creator_name(item);
      if (!name.empty()) {
        out.push_back(name);
      }
    }
    return out;
  };
  for (const std::string& name : names) {
    if (!contains_name(detail_names(), name)) {
      Structure item;
      if (right_is_person) {
        item.insert_or_assign("name",
                              make_value(LangAlt{{"x-default", name}}));
      } else {
        item.insert_or_assign("imageCreatorName", make_value(name));
      }
      details.push_back(std::move(item));
    }
  }
  for (const std::string& name : detail_names()) {
    if (!contains_name(names, name)) {
      names.push_back(name);
    }
  }
  (void)metadata.set(left_id, make_value(names));
  (void)metadata.set(right_id, make_value(std::move(details)));
}

}  // namespace

bool is_upcast_group(std::string_view id) {
  for (const CastGroupDef& group : kCastGroups) {
    if (group.id == id && group.direction == "up") {
      return true;
    }
  }
  return false;
}

void record_upcast_provenance(Metadata& metadata, const CastCandidate& candidate,
                              std::string_view backend_id) {
  if (candidate.status != CastStatus::can_cast || candidate.target_id.empty()) {
    return;
  }
  auto property = metadata.get(candidate.target_id);
  if (!property) {
    return;
  }
  std::vector<std::string> keys;
  auto add_key = [&](std::string key) {
    if (key.empty()) {
      return;
    }
    for (const std::string& have : keys) {
      if (have == key) {
        return;
      }
    }
    keys.push_back(std::move(key));
  };
  if (candidate.source_id.find('.') != std::string::npos) {
    add_key(candidate.source_id);
  }
  for (const CastRuleDef& rule : kCastRules) {
    if (rule.group != candidate.group || rule.direction != "up" ||
        rule.source_kind != "base_key") {
      continue;
    }
    if (!candidate.target_id.empty() && rule.target_key != candidate.target_id) {
      continue;
    }
    if (find_base(metadata, rule.source_key)) {
      add_key(std::string(rule.source_key));
    }
  }
  for (const std::string& key : keys) {
    bool exists = false;
    for (const SourceRef& have : property->sources) {
      if (have.base_key == key && have.container == "cast:" + candidate.group) {
        exists = true;
        break;
      }
    }
    if (exists) {
      continue;
    }
    SourceRef ref;
    ref.base_key = key;
    ref.backend = std::string(backend_id);
    ref.container = "cast:" + candidate.group;
    property->sources.push_back(std::move(ref));
  }
  if (property->preferred_source.empty() && !keys.empty()) {
    property->preferred_source = keys.front();
  }
  if (property->resolution == Resolution::single && !property->sources.empty()) {
    property->resolution = Resolution::reconciled;
  }
  (void)metadata.set(candidate.target_id, *property);
}

std::vector<CastCandidate> evaluate_casts(const Metadata& metadata,
                                          CastDirection direction,
                                          const CastOptions& options,
                                          std::string_view file_type,
                                          const Capabilities* caps) {
  MediaDomain domain = metadata.mediaDomain();
  if (domain == MediaDomain::unknown) {
    domain = media_domain_from_file_type(file_type);
  }
  const char* dir =
      direction == CastDirection::down
          ? "down"
          : (direction == CastDirection::side ? "side" : "up");
  std::vector<CastCandidate> out;
  for (const CastGroupDef& group : kCastGroups) {
    if (group.direction != dir || !group_selected(options, group.id) ||
        !group_applies(group.id, domain)) {
      continue;
    }
    if (group.id == "videoTechnical") {
      for (CastCandidate& candidate : evaluate_video_technical(metadata, group)) {
        if (candidate.status == CastStatus::source_empty) {
          continue;
        }
        out.push_back(std::move(candidate));
      }
      continue;
    }
    if (is_text_group(group.id)) {
      for (CastCandidate& candidate : evaluate_text_group(metadata, group)) {
        if (candidate.status == CastStatus::source_empty) {
          continue;
        }
        out.push_back(std::move(candidate));
      }
      continue;
    }
    CastCandidate candidate = evaluate_group(metadata, group, caps);
    if (candidate.status == CastStatus::source_empty) {
      continue;
    }
    out.push_back(std::move(candidate));
  }
  return out;
}

void apply_cast_candidates(Metadata& metadata,
                           const std::vector<CastCandidate>& candidates,
                           const CastOptions& options, BaseChanges* extra_base) {
  auto apply_one = [&](const CastCandidate& candidate, bool approximate_pass) {
    const CastGroupDef* group =
        find_group(candidate.group,
                   candidate.direction == CastDirection::down
                       ? "down"
                       : (candidate.direction == CastDirection::side ? "side"
                                                                     : "up"));
    if (!group || group->approximate != approximate_pass) {
      return;
    }
    if (group->approximate && !options.include_approximate) {
      return;
    }
    if (is_text_group(candidate.group)) {
      apply_text_group(metadata, candidate, options.force);
      return;
    }
    if (candidate.group == "capturePosition" &&
        candidate.direction == CastDirection::up) {
      apply_capture_up(metadata, candidate, options.force);
    } else if (candidate.group == "capturePosition" &&
               candidate.direction == CastDirection::down) {
      apply_capture_down(metadata, candidate, options.force, extra_base);
    } else if (candidate.group == "videoCreated") {
      apply_date_up(metadata, candidate, "iptc.video.dateCreated", options.force);
    } else if (candidate.group == "videoModified") {
      apply_date_up(metadata, candidate, "iptc.video.dateModified",
                    options.force);
    } else if (candidate.group == "videoTechnical") {
      apply_video_technical(metadata, candidate, options.force);
    } else if (candidate.group == "recordingDevice") {
      apply_recording_device(metadata, candidate, options.force);
    } else if (candidate.group == "locationShownLegacy") {
      apply_location_legacy(metadata, candidate, options.force);
    } else if (candidate.group == "personShown") {
      apply_list_side(metadata, candidate, "iptc.photo.personShownInTheImage",
                      "iptc.photo.personShownInTheImageWithDetails", true,
                      options.force);
    } else if (candidate.group == "creatorImageCreator") {
      apply_list_side(metadata, candidate, "iptc.photo.creator",
                      "iptc.photo.imageCreator", false, options.force);
    }
  };
  // Non-approximate text fills first so UserComment cannot overwrite XPComment.
  for (const bool approximate_pass : {false, true}) {
    for (const CastCandidate& candidate : candidates) {
      apply_one(candidate, approximate_pass);
    }
  }
}

void flag_cast_sources(Metadata& metadata) {
  std::vector<BaseEntry> all = metadata.dumpAll();
  for (BaseEntry& entry : all) {
    for (const CastRuleDef& rule : kCastRules) {
      if (rule.source_kind == "base_key" &&
          base_key_matches(entry.key.key, rule.source_key)) {
        entry.cast_source = true;
        break;
      }
    }
  }
  metadata.assignBase(std::move(all));
  metadata.recomputeUnmapped();
}

void merge_base_changes(BaseChanges& dest, BaseChanges extra) {
  dest.upserts.insert(dest.upserts.end(), extra.upserts.begin(),
                      extra.upserts.end());
  dest.removals.insert(dest.removals.end(), extra.removals.begin(),
                       extra.removals.end());
}

BaseChanges downcast_write_changes(const Metadata& metadata,
                                   const WriteOptions& options,
                                   std::string_view file_type,
                                   const Capabilities* caps) {
  CastOptions cast_options;
  cast_options.dry_run = true;
  if (options.downcast) {
    cast_options.groups = *options.downcast;
  } else if (media_domain_from_file_type(file_type) == MediaDomain::video ||
             metadata.mediaDomain() == MediaDomain::video) {
    cast_options.groups = {"capturePosition"};
  } else {
    return {};
  }
  if (cast_options.groups.empty()) {
    return {};
  }
  const auto candidates = evaluate_casts(metadata, CastDirection::down,
                                         cast_options, file_type, caps);
  Metadata copy = metadata;
  BaseChanges extra;
  CastOptions apply = cast_options;
  apply.dry_run = false;
  apply_cast_candidates(copy, candidates, apply, &extra);
  return extra;
}

}  // namespace umm::internal
