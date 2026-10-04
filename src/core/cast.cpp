#include "core/cast.hpp"

#include "cast_rules.hpp"
#include "core/media_domain.hpp"
#include "core/xmp_codec.hpp"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

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

bool base_key_matches(std::string_view entry, std::string_view want) {
  if (entry == want) {
    return true;
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
      id == "recordingDevice") {
    return domain == MediaDomain::video;
  }
  if (id == "locationShownLegacy" || id == "personShown" ||
      id == "creatorImageCreator") {
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
  const auto* list_prop = metadata.get("iptc.video.locationShot");
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
  const auto* existing = metadata.get("iptc.video.recordingDevice");
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
  if (have && display_text(make_value(merged)) == display_text(*existing)) {
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
  for (const CastCandidate& candidate : candidates) {
    const CastGroupDef* group =
        find_group(candidate.group,
                   candidate.direction == CastDirection::down
                       ? "down"
                       : (candidate.direction == CastDirection::side ? "side"
                                                                     : "up"));
    if (!group) {
      continue;
    }
    if (group->approximate && !options.include_approximate) {
      continue;
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
