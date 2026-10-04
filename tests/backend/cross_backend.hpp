#pragma once

#include "exiftool/json.hpp"
#include "read_unmapped_checks.hpp"
#include "umm/umm.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#ifndef UMM_VERIFICATION_LEDGER
#error "UMM_VERIFICATION_LEDGER must be defined by the build"
#endif

namespace xbv {

inline constexpr double kGpsDegreeTol = 1e-5;
inline constexpr double kGpsAltitudeTol = 0.5;
inline constexpr double kRatingTol = 1e-6;

struct LedgerEntry {
  std::string id;
  std::vector<std::string> file_types;
  std::string category;
  std::string property_id;
  std::string write_backend;
  std::string read_backend;
  std::string kind;
  std::string reason;
};

struct Ledger {
  int version{0};
  std::vector<LedgerEntry> entries;
};

inline int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

inline std::string trim(std::string_view text) {
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
  return std::string(text);
}

inline bool can_read(umm::Access access) { return access != umm::Access::none; }

inline bool can_write(umm::Access access) {
  return access == umm::Access::read_write || access == umm::Access::create;
}

inline umm::Access category_access(const umm::BackendCapability& row,
                                   std::string_view category) {
  if (category == "exif") {
    return row.categories.exif;
  }
  if (category == "iptc_iim") {
    return row.categories.iptc_iim;
  }
  if (category == "xmp") {
    return row.categories.xmp;
  }
  if (category == "gps_exif") {
    return row.location.gps_exif;
  }
  if (category == "named_place") {
    return row.location.named_place;
  }
  if (category == "xmp_location") {
    return row.location.xmp_location;
  }
  if (category == "container_gps") {
    return row.location.container_gps;
  }
  return umm::Access::none;
}

inline const umm::BackendCapability* backend_row(const umm::Capabilities& caps,
                                                 std::string_view id) {
  for (const umm::BackendCapability& row : caps.backends) {
    if (row.backend == id) {
      return &row;
    }
  }
  return nullptr;
}

inline std::string family_of_key(std::string_view key) {
  if (key.rfind("Exif.", 0) == 0) {
    return "Exif";
  }
  if (key.rfind("Iptc.", 0) == 0) {
    return "Iptc";
  }
  if (key.rfind("Xmp.", 0) == 0) {
    return "Xmp";
  }
  if (key.rfind("QuickTime.", 0) == 0) {
    return "QuickTime";
  }
  return {};
}

inline bool reader_sees_family(const umm::BackendCapability& row,
                               std::string_view family) {
  if (family == "Exif") {
    return can_read(row.categories.exif) || can_read(row.location.gps_exif);
  }
  if (family == "Iptc") {
    return can_read(row.categories.iptc_iim) ||
           can_read(row.location.named_place);
  }
  if (family == "Xmp") {
    return can_read(row.categories.xmp) || can_read(row.location.xmp_location);
  }
  if (family == "QuickTime") {
    return can_read(row.location.container_gps);
  }
  return false;
}

inline bool opt_int_equal(const std::optional<int>& a,
                          const std::optional<int>& b) {
  if (!a || !b) {
    return true;
  }
  return *a == *b;
}

inline bool datetime_equivalent(const umm::DateTime& a, const umm::DateTime& b) {
  if (a.year != b.year) {
    return false;
  }
  return opt_int_equal(a.month, b.month) && opt_int_equal(a.day, b.day) &&
         opt_int_equal(a.hour, b.hour) && opt_int_equal(a.minute, b.minute) &&
         opt_int_equal(a.second, b.second) &&
         opt_int_equal(a.subsecond_ns, b.subsecond_ns) &&
         opt_int_equal(a.utc_offset_minutes, b.utc_offset_minutes);
}

inline bool gps_equivalent(const umm::GpsCoordinate& a,
                           const umm::GpsCoordinate& b) {
  if (std::fabs(a.latitude - b.latitude) > kGpsDegreeTol ||
      std::fabs(a.longitude - b.longitude) > kGpsDegreeTol) {
    return false;
  }
  if (a.altitude_meters && b.altitude_meters &&
      std::fabs(*a.altitude_meters - *b.altitude_meters) > kGpsAltitudeTol) {
    return false;
  }
  if (a.gps_time && b.gps_time &&
      !datetime_equivalent(*a.gps_time, *b.gps_time)) {
    return false;
  }
  return true;
}

inline std::string lang_plain(const umm::LangAlt& alt) {
  const auto it = alt.find("x-default");
  if (it != alt.end()) {
    return trim(it->second);
  }
  if (alt.size() == 1) {
    return trim(alt.begin()->second);
  }
  return {};
}

inline bool values_equivalent(const umm::Value& expected,
                              const umm::Value& actual);

inline bool structures_equivalent(const umm::Structure& expected,
                                  const umm::Structure& actual) {
  for (const auto& [name, value] : expected) {
    const auto it = actual.find(name);
    if (it == actual.end()) {
      continue;
    }
    if (!values_equivalent(value, it->second)) {
      return false;
    }
  }
  return true;
}

inline bool values_equivalent(const umm::Value& expected,
                              const umm::Value& actual) {
  if (expected.data.index() != actual.data.index()) {
    if (const auto* exp_text = std::get_if<std::string>(&expected.data)) {
      if (const auto* act_lang = std::get_if<umm::LangAlt>(&actual.data)) {
        return trim(*exp_text) == lang_plain(*act_lang);
      }
    }
    if (const auto* exp_lang = std::get_if<umm::LangAlt>(&expected.data)) {
      if (const auto* act_text = std::get_if<std::string>(&actual.data)) {
        return lang_plain(*exp_lang) == trim(*act_text);
      }
    }
    return false;
  }
  if (const auto* text = std::get_if<std::string>(&expected.data)) {
    return trim(*text) == trim(std::get<std::string>(actual.data));
  }
  if (const auto* lang = std::get_if<umm::LangAlt>(&expected.data)) {
    return lang_plain(*lang) == lang_plain(std::get<umm::LangAlt>(actual.data));
  }
  if (const auto* list = std::get_if<std::vector<std::string>>(&expected.data)) {
    const auto& other = std::get<std::vector<std::string>>(actual.data);
    if (list->size() != other.size()) {
      return false;
    }
    for (std::size_t i = 0; i < list->size(); ++i) {
      if (trim((*list)[i]) != trim(other[i])) {
        return false;
      }
    }
    return true;
  }
  if (const auto* integer = std::get_if<std::int64_t>(&expected.data)) {
    return *integer == std::get<std::int64_t>(actual.data);
  }
  if (const auto* real = std::get_if<double>(&expected.data)) {
    return std::fabs(*real - std::get<double>(actual.data)) <= kRatingTol;
  }
  if (const auto* flag = std::get_if<bool>(&expected.data)) {
    return *flag == std::get<bool>(actual.data);
  }
  if (const auto* rat = std::get_if<umm::Rational>(&expected.data)) {
    return *rat == std::get<umm::Rational>(actual.data);
  }
  if (const auto* dt = std::get_if<umm::DateTime>(&expected.data)) {
    return datetime_equivalent(*dt, std::get<umm::DateTime>(actual.data));
  }
  if (const auto* gps = std::get_if<umm::GpsCoordinate>(&expected.data)) {
    return gps_equivalent(*gps, std::get<umm::GpsCoordinate>(actual.data));
  }
  if (const auto* st = std::get_if<umm::Structure>(&expected.data)) {
    return structures_equivalent(*st, std::get<umm::Structure>(actual.data));
  }
  if (const auto* sts =
          std::get_if<std::vector<umm::Structure>>(&expected.data)) {
    const auto& other = std::get<std::vector<umm::Structure>>(actual.data);
    if (sts->empty() || other.empty()) {
      return sts->empty() == other.empty();
    }
    return structures_equivalent(sts->front(), other.front());
  }
  return false;
}

inline bool provenance_ok(const umm::PropertyValue& actual) {
  if (actual.sources.empty()) {
    return false;
  }
  return actual.resolution != umm::Resolution::conflict;
}

inline std::vector<std::string> source_families(
    const umm::PropertyValue& property) {
  std::vector<std::string> families;
  for (const umm::SourceRef& source : property.sources) {
    const std::string family = family_of_key(source.base_key);
    if (family.empty()) {
      continue;
    }
    bool seen = false;
    for (const std::string& existing : families) {
      if (existing == family) {
        seen = true;
        break;
      }
    }
    if (!seen) {
      families.push_back(std::move(family));
    }
  }
  return families;
}

inline bool has_family(const std::vector<std::string>& families,
                       std::string_view family) {
  for (const std::string& item : families) {
    if (item == family) {
      return true;
    }
  }
  return false;
}

inline std::filesystem::path ledger_path() {
  const char* raw = UMM_VERIFICATION_LEDGER;
  return std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t*>(raw)));
}

inline std::string json_text(const umm::internal::JsonValue* value) {
  return value ? value->as_text() : std::string{};
}

inline std::vector<std::string> json_string_array(
    const umm::internal::JsonValue* value) {
  std::vector<std::string> out;
  if (!value || value->kind != umm::internal::JsonValue::Kind::array) {
    return out;
  }
  for (const umm::internal::JsonValue& item : value->array) {
    out.push_back(item.as_text());
  }
  return out;
}

inline umm::Result<Ledger> load_ledger() {
  std::ifstream in(ledger_path(), std::ios::binary);
  if (!in) {
    return umm::Error{umm::ErrorCode::io_not_found,
                      "missing divergence ledger", "",
                      ledger_path().string()};
  }
  const std::string text((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
  std::string error;
  const auto parsed = umm::internal::parse_json(text, &error);
  if (!parsed || parsed->kind != umm::internal::JsonValue::Kind::object) {
    return umm::Error{umm::ErrorCode::format_corrupt,
                      "divergence ledger is not a JSON object", "", error};
  }
  Ledger ledger;
  const umm::internal::JsonValue* version = parsed->field("version");
  if (!version || version->kind != umm::internal::JsonValue::Kind::number) {
    return umm::Error{umm::ErrorCode::format_corrupt,
                      "divergence ledger missing version", "", ""};
  }
  ledger.version = std::stoi(version->text);
  if (ledger.version != 1) {
    return umm::Error{umm::ErrorCode::format_corrupt,
                      "unsupported divergence ledger version", "",
                      version->text};
  }
  const umm::internal::JsonValue* entries = parsed->field("entries");
  if (!entries || entries->kind != umm::internal::JsonValue::Kind::array) {
    return umm::Error{umm::ErrorCode::format_corrupt,
                      "divergence ledger missing entries", "", ""};
  }
  for (const umm::internal::JsonValue& item : entries->array) {
    if (item.kind != umm::internal::JsonValue::Kind::object) {
      return umm::Error{umm::ErrorCode::format_corrupt,
                        "ledger entry is not an object", "", ""};
    }
    LedgerEntry entry;
    entry.id = json_text(item.field("id"));
    entry.file_types = json_string_array(item.field("file_types"));
    entry.category = json_text(item.field("category"));
    entry.property_id = json_text(item.field("property_id"));
    entry.write_backend = json_text(item.field("write_backend"));
    entry.read_backend = json_text(item.field("read_backend"));
    entry.kind = json_text(item.field("kind"));
    entry.reason = json_text(item.field("reason"));
    if (entry.id.empty() || entry.reason.empty()) {
      return umm::Error{umm::ErrorCode::format_corrupt,
                        "ledger entry missing id or reason", "", entry.id};
    }
    if (entry.kind != "representation" && entry.kind != "capability" &&
        entry.kind != "one-directional") {
      return umm::Error{umm::ErrorCode::format_corrupt,
                        "ledger entry has unknown kind", "", entry.kind};
    }
    ledger.entries.push_back(std::move(entry));
  }
  return ledger;
}

inline bool ledger_allows(const Ledger& ledger, std::string_view file_type,
                          std::string_view property_id,
                          std::string_view write_backend,
                          std::string_view read_backend,
                          std::string_view kind) {
  for (const LedgerEntry& entry : ledger.entries) {
    if (entry.kind != kind) {
      continue;
    }
    // Documentation-only rows (no property) never suppress a mismatch.
    if (entry.property_id.empty()) {
      continue;
    }
    if (entry.property_id != property_id) {
      continue;
    }
    if (!entry.file_types.empty()) {
      bool match_type = false;
      for (const std::string& type : entry.file_types) {
        if (type == file_type) {
          match_type = true;
          break;
        }
      }
      if (!match_type) {
        continue;
      }
    }
    if (!entry.write_backend.empty() && entry.write_backend != write_backend) {
      continue;
    }
    if (!entry.read_backend.empty() && entry.read_backend != read_backend) {
      continue;
    }
    return true;
  }
  return false;
}

inline umm::WriteOptions write_opts(const std::string& backend,
                                    umm::StoragePolicy policy) {
  umm::WriteOptions options;
  options.backend = backend;
  options.policy = policy;
  return options;
}

inline umm::ReadOptions read_opts(const std::string& backend) {
  umm::ReadOptions options;
  options.backend = backend;
  return options;
}

inline umm::Metadata stills_payload() {
  umm::Metadata metadata;
  (void)metadata.setCreator({"Cross Backend Creator"});
  umm::LangAlt description;
  description.emplace("x-default", "Cross-backend description");
  (void)metadata.setDescription(description);
  (void)metadata.setHeadline("Cross-backend headline");
  umm::DateTime when;
  when.year = 2020;
  when.month = 1;
  when.day = 2;
  when.hour = 3;
  when.minute = 4;
  when.second = 5;
  (void)metadata.setDateCreated(when);
  umm::LangAlt copyright;
  copyright.emplace("x-default", "Cross-backend copyright");
  (void)metadata.setCopyrightNotice(copyright);
  (void)metadata.setKeywords({"alpha", "beta"});
  (void)metadata.setRating(4.0);
  umm::GpsCoordinate gps;
  gps.latitude = 37.7749;
  gps.longitude = -122.4194;
  gps.altitude_meters = 10.0;
  (void)metadata.setGps(gps);
  umm::Value city;
  city.data = std::string("San Francisco");
  umm::Value state;
  state.data = std::string("CA");
  umm::Value country;
  country.data = std::string("United States");
  umm::Structure location;
  location.emplace("city", std::move(city));
  location.emplace("provinceState", std::move(state));
  location.emplace("countryName", std::move(country));
  (void)metadata.setLocationCreated({std::move(location)});
  return metadata;
}

inline umm::Value lang_value(const std::string& text) {
  umm::Value value;
  value.data = umm::LangAlt{{"x-default", text}};
  return value;
}

inline umm::Value creator_entity(const std::string& name) {
  umm::Value name_value;
  name_value.data = umm::LangAlt{{"x-default", name}};
  umm::Structure entity;
  entity.emplace("name", std::move(name_value));
  umm::Value value;
  value.data = std::vector<umm::Structure>{std::move(entity)};
  return value;
}

inline umm::Metadata video_payload() {
  umm::Metadata metadata;
  (void)metadata.set("iptc.video.title", lang_value("Cross Backend Title"));
  (void)metadata.set("iptc.video.description",
                     lang_value("Cross-backend description"));
  (void)metadata.set("iptc.video.creator", creator_entity("Cross Backend Creator"));
  umm::DateTime when;
  when.year = 2020;
  when.month = 1;
  when.day = 2;
  when.hour = 3;
  when.minute = 4;
  when.second = 5;
  umm::Value date;
  date.data = when;
  (void)metadata.set("iptc.video.dateCreated", date);
  umm::GpsCoordinate gps;
  gps.latitude = 37.7749;
  gps.longitude = -122.4194;
  gps.altitude_meters = 10.0;
  (void)metadata.setGps(gps);
  return metadata;
}

inline std::string compare_property(const Ledger& ledger,
                                    std::string_view file_type,
                                    std::string_view write_backend,
                                    std::string_view read_backend,
                                    std::string_view property_id,
                                    const umm::Value& expected,
                                    const std::optional<umm::PropertyValue>& actual) {
  if (!actual) {
    if (ledger_allows(ledger, file_type, property_id, write_backend,
                      read_backend, "representation")) {
      return {};
    }
    return "missing " + std::string(property_id);
  }
  if (!values_equivalent(expected, actual->value)) {
    if (ledger_allows(ledger, file_type, property_id, write_backend,
                      read_backend, "representation")) {
      return {};
    }
    return "value mismatch " + std::string(property_id) + " expected " +
           expected.toString() + " got " + actual->value.toString();
  }
  if (!provenance_ok(*actual)) {
    if (ledger_allows(ledger, file_type, property_id, write_backend,
                      read_backend, "representation")) {
      return {};
    }
    return "provenance " + std::string(property_id);
  }
  return {};
}

inline std::string compare_written(const Ledger& ledger,
                                   std::string_view file_type,
                                   std::string_view write_backend,
                                   std::string_view read_backend,
                                   const umm::Metadata& expected,
                                   const umm::Metadata& actual) {
  for (const std::string& id : expected.propertyIds()) {
    const auto want = expected.get(id);
    if (!want) {
      continue;
    }
    const std::string issue =
        compare_property(ledger, file_type, write_backend, read_backend, id,
                         want->value, actual.get(id));
    if (!issue.empty()) {
      return issue;
    }
  }
  return {};
}

inline bool families_cover(const umm::WriteReport& report,
                           const umm::Metadata& actual,
                           const umm::BackendCapability& reader) {
  std::vector<std::string> expected;
  for (const umm::BaseKey& key : report.written) {
    const std::string family = family_of_key(key.key);
    if (family.empty() || !reader_sees_family(reader, family)) {
      continue;
    }
    if (!has_family(expected, family)) {
      expected.push_back(family);
    }
  }
  std::vector<std::string> seen;
  for (const std::string& id : actual.propertyIds()) {
    const auto property = actual.get(id);
    if (!property) {
      continue;
    }
    for (const std::string& family : source_families(*property)) {
      if (!has_family(seen, family)) {
        seen.push_back(family);
      }
    }
  }
  for (const std::string& family : expected) {
    if (!has_family(seen, family)) {
      return false;
    }
  }
  return true;
}

}  // namespace xbv
