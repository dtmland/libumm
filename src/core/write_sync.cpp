#include "core/write_sync.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "core/property_ids.hpp"
#include "core/xmp_codec.hpp"
#include "umm/registry.hpp"

namespace umm::internal {
namespace {

void add(UnmappedChanges& changes, std::string family, std::string key,
         std::string value, std::string type_hint = {}) {
  if (value.empty()) {
    return;
  }
  UnmappedEntry entry;
  entry.key.family = std::move(family);
  entry.key.key = std::move(key);
  entry.type_hint = std::move(type_hint);
  entry.value = std::move(value);
  changes.upserts.push_back(std::move(entry));
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

std::string join_names(const std::vector<std::string>& names,
                       std::string_view sep) {
  std::string out;
  for (const std::string& name : names) {
    if (name.empty()) {
      continue;
    }
    if (!out.empty()) {
      out.append(sep);
    }
    out += name;
  }
  return out;
}

std::string lang_plain(const LangAlt& alt) {
  const auto it = alt.find("x-default");
  if (it != alt.end()) {
    return it->second;
  }
  if (alt.size() == 1) {
    return alt.begin()->second;
  }
  return {};
}

std::string format_real(double value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

std::string format_gps_number(double value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(7) << value;
  std::string text = out.str();
  while (text.size() > 1 && text.find('.') != std::string::npos &&
         text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
  return text;
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
    std::string frac = pad4(0) + pad4(0);
    frac = std::to_string(ns);
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

std::string format_exif_datetime(const DateTime& dt) {
  if (!dt.month || !dt.day) {
    return {};
  }
  std::string out = pad4(dt.year);
  out += ':';
  out += pad2(*dt.month);
  out += ':';
  out += pad2(*dt.day);
  out += ' ';
  out += pad2(dt.hour.value_or(0));
  out += ':';
  out += pad2(dt.minute.value_or(0));
  out += ':';
  out += pad2(dt.second.value_or(0));
  return out;
}

std::string format_iim_date(const DateTime& dt) {
  if (!dt.month || !dt.day) {
    return {};
  }
  return pad4(dt.year) + pad2(*dt.month) + pad2(*dt.day);
}

std::string format_iim_time(const DateTime& dt) {
  if (!dt.hour || !dt.minute) {
    return {};
  }
  std::string out = pad2(*dt.hour) + pad2(*dt.minute) + pad2(dt.second.value_or(0));
  if (dt.utc_offset_minutes) {
    int minutes = *dt.utc_offset_minutes;
    const char sign = minutes < 0 ? '-' : '+';
    if (minutes < 0) {
      minutes = -minutes;
    }
    out += sign;
    out += pad2(minutes / 60);
    out += pad2(minutes % 60);
  }
  return out;
}

std::string format_subsec(const DateTime& dt) {
  if (!dt.subsecond_ns) {
    return {};
  }
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
  return frac;
}

std::string format_exif_offset(const DateTime& dt) {
  if (!dt.utc_offset_minutes) {
    return {};
  }
  int minutes = *dt.utc_offset_minutes;
  const char sign = minutes < 0 ? '-' : '+';
  if (minutes < 0) {
    minutes = -minutes;
  }
  std::string out;
  out += sign;
  out += pad2(minutes / 60);
  out += ':';
  out += pad2(minutes % 60);
  return out;
}

std::string structure_text(const Structure& fields, std::string_view name) {
  const auto it = fields.find(std::string(name));
  if (it == fields.end()) {
    return {};
  }
  if (const auto* text = std::get_if<std::string>(&it->second.data)) {
    return *text;
  }
  return {};
}

void sync_creator(UnmappedChanges& changes, const Value& value) {
  const auto* names = std::get_if<std::vector<std::string>>(&value.data);
  if (!names || names->empty()) {
    return;
  }
  for (const std::string& name : *names) {
    add(changes, "Xmp", "Xmp.dc.creator", name, "XmpSeq");
    add(changes, "Iptc", "Iptc.Application2.Byline", name);
  }
  add(changes, "Exif", "Exif.Image.Artist", join_names(*names, "; "));
}

void sync_lang(UnmappedChanges& changes, const Value& value, std::string_view xmp,
               std::string_view iim, std::string_view exif) {
  const auto* alt = std::get_if<LangAlt>(&value.data);
  if (!alt) {
    return;
  }
  const std::string plain = lang_plain(*alt);
  const auto xd = alt->find("x-default");
  const std::string xmp_text =
      xd != alt->end() ? xd->second : (alt->size() == 1 ? alt->begin()->second : plain);
  add(changes, "Xmp", std::string(xmp), xmp_text, "LangAlt");
  add(changes, "Iptc", std::string(iim), plain);
  add(changes, "Exif", std::string(exif), plain);
}

void sync_text_pair(UnmappedChanges& changes, const Value& value, std::string_view xmp,
                    std::string_view iim) {
  const auto* text = std::get_if<std::string>(&value.data);
  if (!text) {
    return;
  }
  add(changes, "Xmp", std::string(xmp), *text);
  add(changes, "Iptc", std::string(iim), *text);
}

void sync_keywords(UnmappedChanges& changes, const Value& value) {
  const auto* words = std::get_if<std::vector<std::string>>(&value.data);
  if (!words || words->empty()) {
    return;
  }
  for (const std::string& word : *words) {
    add(changes, "Xmp", "Xmp.dc.subject", word, "XmpBag");
    add(changes, "Iptc", "Iptc.Application2.Keywords", word);
  }
}

void sync_date(UnmappedChanges& changes, const Value& value) {
  const auto* dt = std::get_if<DateTime>(&value.data);
  if (!dt) {
    return;
  }
  add(changes, "Xmp", "Xmp.photoshop.DateCreated", format_xmp_datetime(*dt));
  add(changes, "Exif", "Exif.Photo.DateTimeOriginal", format_exif_datetime(*dt));
  add(changes, "Exif", "Exif.Photo.SubSecTimeOriginal", format_subsec(*dt));
  add(changes, "Exif", "Exif.Photo.OffsetTimeOriginal", format_exif_offset(*dt));
  add(changes, "Iptc", "Iptc.Application2.DateCreated", format_iim_date(*dt));
  add(changes, "Iptc", "Iptc.Application2.TimeCreated", format_iim_time(*dt));
}

void sync_rating(UnmappedChanges& changes, const Value& value) {
  const auto* rating = std::get_if<double>(&value.data);
  if (!rating) {
    return;
  }
  add(changes, "Xmp", "Xmp.xmp.Rating", format_real(*rating));
}

void sync_gps(UnmappedChanges& changes, const Value& value) {
  const auto* gps = std::get_if<GpsCoordinate>(&value.data);
  if (!gps) {
    return;
  }
  const char lat_ref = gps->latitude < 0 ? 'S' : 'N';
  const char lon_ref = gps->longitude < 0 ? 'W' : 'E';
  add(changes, "Exif", "Exif.GPSInfo.GPSLatitude",
      format_gps_number(std::fabs(gps->latitude)), "decimal");
  add(changes, "Exif", "Exif.GPSInfo.GPSLatitudeRef", std::string(1, lat_ref));
  add(changes, "Exif", "Exif.GPSInfo.GPSLongitude",
      format_gps_number(std::fabs(gps->longitude)), "decimal");
  add(changes, "Exif", "Exif.GPSInfo.GPSLongitudeRef", std::string(1, lon_ref));
  add(changes, "Xmp", "Xmp.exif.GPSLatitude",
      format_gps_number(std::fabs(gps->latitude)) + lat_ref);
  add(changes, "Xmp", "Xmp.exif.GPSLongitude",
      format_gps_number(std::fabs(gps->longitude)) + lon_ref);
  std::string qt = format_gps_number(gps->latitude) + ", " +
                   format_gps_number(gps->longitude);
  if (gps->altitude_meters) {
    const double alt = *gps->altitude_meters;
    add(changes, "Exif", "Exif.GPSInfo.GPSAltitude",
        format_gps_number(std::fabs(alt)), "decimal");
    add(changes, "Exif", "Exif.GPSInfo.GPSAltitudeRef", alt < 0 ? "1" : "0");
    add(changes, "Xmp", "Xmp.exif.GPSAltitude", format_gps_number(alt));
    qt += ", " + format_gps_number(alt);
  }
  add(changes, "QuickTime", "QuickTime.GPSCoordinates", std::move(qt));
}

std::string structure_lang_or_text(const Structure& fields,
                                   std::string_view name) {
  const auto it = fields.find(std::string(name));
  if (it == fields.end()) {
    return {};
  }
  if (const auto* text = std::get_if<std::string>(&it->second.data)) {
    return *text;
  }
  if (const auto* alt = std::get_if<LangAlt>(&it->second.data)) {
    return lang_plain(*alt);
  }
  return {};
}

void sync_video_generic(UnmappedChanges& changes, std::string_view property_id,
                        const Value& value) {
  const auto def = registry().find(property_id);
  if (!def) {
    return;
  }
  const auto xmp_keys = xmp_raw_keys(def->representations.xmp_property);
  const auto qt_keys = quicktime_raw_keys(def->representations.quicktime_key);
  if (xmp_keys.empty()) {
    return;
  }
  const std::string& xmp = xmp_keys.front();
  auto add_qt_plain = [&](const std::string& text) {
    if (qt_keys.empty() || text.empty()) {
      return;
    }
    add(changes, "QuickTime", qt_keys.front(), text);
  };
  if (def->datatype == Datatype::lang_alt) {
    const auto* alt = std::get_if<LangAlt>(&value.data);
    if (!alt) {
      return;
    }
    const std::string plain = lang_plain(*alt);
    const auto xd = alt->find("x-default");
    const std::string xmp_text =
        xd != alt->end() ? xd->second
                         : (alt->size() == 1 ? alt->begin()->second : plain);
    add(changes, "Xmp", xmp, xmp_text, "LangAlt");
    add_qt_plain(plain);
    return;
  }
  if (def->datatype == Datatype::date_time) {
    const auto* dt = std::get_if<DateTime>(&value.data);
    if (!dt) {
      return;
    }
    const std::string iso = format_xmp_datetime(*dt);
    add(changes, "Xmp", xmp, iso);
    add_qt_plain(iso);
    return;
  }
  if (def->datatype == Datatype::text || def->datatype == Datatype::text_list) {
    if (const auto* text = std::get_if<std::string>(&value.data)) {
      add(changes, "Xmp", xmp, *text);
      add_qt_plain(*text);
      return;
    }
    if (const auto* list = std::get_if<std::vector<std::string>>(&value.data)) {
      for (const std::string& item : *list) {
        add(changes, "Xmp", xmp, item);
      }
      add_qt_plain(join_names(*list, ", "));
    }
    return;
  }
  if (def->datatype == Datatype::structure) {
    const auto* fields = std::get_if<Structure>(&value.data);
    if (!fields || fields->empty()) {
      return;
    }
    if (structure_is_uri_like(*fields)) {
      add(changes, "Xmp", xmp, uri_from_structure(*fields));
    } else {
      add(changes, "Xmp", xmp, encode_exiftool_struct(*fields), "struct");
    }
    return;
  }
  if (def->datatype == Datatype::structure_list) {
    const auto* list = std::get_if<std::vector<Structure>>(&value.data);
    if (!list || list->empty()) {
      return;
    }
    if (xmp_keys.size() > 1) {
      for (const Structure& item : *list) {
        add(changes, "Xmp", xmp, structure_display_name(item));
      }
      return;
    }
    for (const Structure& item : *list) {
      add(changes, "Xmp", xmp, encode_exiftool_struct(item), "struct");
    }
  }
}

void sync_video_creator(UnmappedChanges& changes, const Value& value) {
  const auto* list = std::get_if<std::vector<Structure>>(&value.data);
  if (!list || list->empty()) {
    return;
  }
  std::vector<std::string> names;
  for (const Structure& entity : *list) {
    std::string name = structure_display_name(entity);
    if (name.empty()) {
      name = structure_lang_or_text(entity, "name");
    }
    if (!name.empty()) {
      names.push_back(name);
    }
  }
  if (names.empty()) {
    return;
  }
  for (const std::string& name : names) {
    add(changes, "Xmp", "Xmp.dc.creator", name, "XmpSeq");
  }
  add(changes, "QuickTime", "QuickTime.Artist", join_names(names, "; "));
}

void sync_video_keywords(UnmappedChanges& changes, const Value& value) {
  const auto* alt = std::get_if<LangAlt>(&value.data);
  if (!alt) {
    return;
  }
  const std::string plain = lang_plain(*alt);
  if (plain.empty()) {
    return;
  }
  add(changes, "QuickTime", "QuickTime.Keywords", plain);
  std::string_view rest = plain;
  while (!rest.empty()) {
    const auto comma = rest.find(',');
    std::string_view item =
        comma == std::string_view::npos ? rest : rest.substr(0, comma);
    while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) {
      item.remove_prefix(1);
    }
    while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) {
      item.remove_suffix(1);
    }
    add(changes, "Xmp", "Xmp.dc.subject", std::string(item), "XmpBag");
    if (comma == std::string_view::npos) {
      break;
    }
    rest.remove_prefix(comma + 1);
  }
}

void sync_video_date(UnmappedChanges& changes, const Value& value) {
  const auto* dt = std::get_if<DateTime>(&value.data);
  if (!dt) {
    return;
  }
  const std::string iso = format_xmp_datetime(*dt);
  add(changes, "Xmp", "Xmp.photoshop.DateCreated", iso);
  add(changes, "QuickTime", "QuickTime.CreationDate", iso);
}

void sync_location(UnmappedChanges& changes, const Value& value) {
  const auto* list = std::get_if<std::vector<Structure>>(&value.data);
  if (!list || list->empty()) {
    return;
  }
  const Structure& fields = list->front();
  const std::string city = structure_text(fields, "city");
  const std::string state = structure_text(fields, "provinceState");
  const std::string country = structure_text(fields, "countryName");
  add(changes, "Xmp", "Xmp.photoshop.City", city);
  add(changes, "Xmp", "Xmp.photoshop.State", state);
  add(changes, "Xmp", "Xmp.photoshop.Country", country);
  add(changes, "Iptc", "Iptc.Application2.City", city);
  add(changes, "Iptc", "Iptc.Application2.ProvinceState", state);
  add(changes, "Iptc", "Iptc.Application2.CountryName", country);
}

bool writes_iptc_application(const UnmappedChanges& changes) {
  for (const UnmappedEntry& entry : changes.upserts) {
    if (entry.key.key.rfind("Iptc.Application2.", 0) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

UnmappedChanges write_sync_xmp(const Metadata& metadata) {
  UnmappedChanges all = write_sync(metadata);
  UnmappedChanges xmp;
  for (UnmappedEntry& entry : all.upserts) {
    if (entry.key.family == "Xmp" || entry.key.key.rfind("Xmp.", 0) == 0) {
      xmp.upserts.push_back(std::move(entry));
    }
  }
  for (UnmappedKey& key : all.removals) {
    if (key.family == "Xmp" || key.key.rfind("Xmp.", 0) == 0) {
      xmp.removals.push_back(std::move(key));
    }
  }
  return xmp;
}

UnmappedChanges write_sync(const Metadata& metadata) {
  UnmappedChanges changes;
  for (const std::string& id : metadata.propertyIds()) {
    const auto property = metadata.get(id);
    if (!property) {
      continue;
    }
    if (id == kCreator) {
      sync_creator(changes, property->value);
    } else if (id == kDescription) {
      sync_lang(changes, property->value, "Xmp.dc.description",
                "Iptc.Application2.Caption", "Exif.Image.ImageDescription");
    } else if (id == kCopyright) {
      sync_lang(changes, property->value, "Xmp.dc.rights",
                "Iptc.Application2.Copyright", "Exif.Image.Copyright");
    } else if (id == kHeadline) {
      sync_text_pair(changes, property->value, "Xmp.photoshop.Headline",
                     "Iptc.Application2.Headline");
    } else if (id == kCredit) {
      sync_text_pair(changes, property->value, "Xmp.photoshop.Credit",
                     "Iptc.Application2.Credit");
    } else if (id == kKeywords) {
      sync_keywords(changes, property->value);
    } else if (id == kDateCreated) {
      sync_date(changes, property->value);
    } else if (id == kRating) {
      sync_rating(changes, property->value);
    } else if (id == kGps) {
      sync_gps(changes, property->value);
    } else if (id == kLocation) {
      sync_location(changes, property->value);
    } else if (id == kVideoCreator) {
      sync_video_creator(changes, property->value);
    } else if (id == kVideoKeywords) {
      sync_video_keywords(changes, property->value);
    } else if (id == kVideoDateCreated) {
      sync_video_date(changes, property->value);
    } else if (id.rfind("iptc.video.", 0) == 0 ||
               id.rfind("iptc.photo.", 0) == 0) {
      sync_video_generic(changes, id, property->value);
    }
  }
  if (writes_iptc_application(changes)) {
    add(changes, "Iptc", "Iptc.Envelope.CharacterSet", "UTF8");
  }
  return changes;
}

}  // namespace umm::internal
