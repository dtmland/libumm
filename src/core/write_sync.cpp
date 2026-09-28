#include "core/write_sync.hpp"

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

namespace umm::internal {
namespace {

constexpr std::string_view kCreator = "iptc.photo.creator";
constexpr std::string_view kDescription = "iptc.photo.description";
constexpr std::string_view kHeadline = "iptc.photo.headline";
constexpr std::string_view kDateCreated = "iptc.photo.dateCreated";
constexpr std::string_view kCopyright = "iptc.photo.copyrightNotice";
constexpr std::string_view kCredit = "iptc.photo.creditLine";
constexpr std::string_view kKeywords = "iptc.photo.keywords";
constexpr std::string_view kRating = "iptc.photo.imageRating";
constexpr std::string_view kLocation = "iptc.photo.locationCreated";
constexpr std::string_view kGps = "exif.gps.position";

void add(RawChanges& changes, std::string family, std::string key,
         std::string value, std::string type_hint = {}) {
  if (value.empty()) {
    return;
  }
  RawEntry entry;
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

void sync_creator(RawChanges& changes, const Value& value) {
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

void sync_lang(RawChanges& changes, const Value& value, std::string_view xmp,
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

void sync_text_pair(RawChanges& changes, const Value& value, std::string_view xmp,
                    std::string_view iim) {
  const auto* text = std::get_if<std::string>(&value.data);
  if (!text) {
    return;
  }
  add(changes, "Xmp", std::string(xmp), *text);
  add(changes, "Iptc", std::string(iim), *text);
}

void sync_keywords(RawChanges& changes, const Value& value) {
  const auto* words = std::get_if<std::vector<std::string>>(&value.data);
  if (!words || words->empty()) {
    return;
  }
  for (const std::string& word : *words) {
    add(changes, "Xmp", "Xmp.dc.subject", word, "XmpBag");
    add(changes, "Iptc", "Iptc.Application2.Keywords", word);
  }
}

void sync_date(RawChanges& changes, const Value& value) {
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

void sync_rating(RawChanges& changes, const Value& value) {
  const auto* rating = std::get_if<double>(&value.data);
  if (!rating) {
    return;
  }
  add(changes, "Xmp", "Xmp.xmp.Rating", format_real(*rating));
}

void sync_gps(RawChanges& changes, const Value& value) {
  const auto* gps = std::get_if<GpsCoordinate>(&value.data);
  if (!gps) {
    return;
  }
  const char lat_ref = gps->latitude < 0 ? 'S' : 'N';
  const char lon_ref = gps->longitude < 0 ? 'W' : 'E';
  add(changes, "Exif", "Exif.GPSInfo.GPSLatitude",
      format_real(std::fabs(gps->latitude)), "decimal");
  add(changes, "Exif", "Exif.GPSInfo.GPSLatitudeRef", std::string(1, lat_ref));
  add(changes, "Exif", "Exif.GPSInfo.GPSLongitude",
      format_real(std::fabs(gps->longitude)), "decimal");
  add(changes, "Exif", "Exif.GPSInfo.GPSLongitudeRef", std::string(1, lon_ref));
  add(changes, "Xmp", "Xmp.exif.GPSLatitude",
      format_real(std::fabs(gps->latitude)) + lat_ref);
  add(changes, "Xmp", "Xmp.exif.GPSLongitude",
      format_real(std::fabs(gps->longitude)) + lon_ref);
  if (gps->altitude_meters) {
    const double alt = *gps->altitude_meters;
    add(changes, "Exif", "Exif.GPSInfo.GPSAltitude", format_real(std::fabs(alt)),
        "decimal");
    add(changes, "Exif", "Exif.GPSInfo.GPSAltitudeRef", alt < 0 ? "1" : "0");
    add(changes, "Xmp", "Xmp.exif.GPSAltitude", format_real(alt));
  }
}

void sync_location(RawChanges& changes, const Value& value) {
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

bool writes_iptc_application(const RawChanges& changes) {
  for (const RawEntry& entry : changes.upserts) {
    if (entry.key.key.rfind("Iptc.Application2.", 0) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

RawChanges write_sync_xmp(const Metadata& metadata) {
  RawChanges all = write_sync(metadata);
  RawChanges xmp;
  for (RawEntry& entry : all.upserts) {
    if (entry.key.family == "Xmp" || entry.key.key.rfind("Xmp.", 0) == 0) {
      xmp.upserts.push_back(std::move(entry));
    }
  }
  for (RawKey& key : all.removals) {
    if (key.family == "Xmp" || key.key.rfind("Xmp.", 0) == 0) {
      xmp.removals.push_back(std::move(key));
    }
  }
  return xmp;
}

RawChanges write_sync(const Metadata& metadata) {
  RawChanges changes;
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
    }
  }
  if (writes_iptc_application(changes)) {
    add(changes, "Iptc", "Iptc.Envelope.CharacterSet", "UTF8");
  }
  return changes;
}

}  // namespace umm::internal
