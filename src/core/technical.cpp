#include "core/technical.hpp"

#include "technical_sources.hpp"

#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace umm::internal {
namespace {

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

BaseKey key_from_text(std::string_view full) {
  BaseKey key;
  key.key = std::string(full);
  const auto dot = full.find('.');
  key.family = dot == std::string_view::npos ? std::string(full)
                                             : std::string(full.substr(0, dot));
  return key;
}

const BaseEntry* find_entry(const std::vector<BaseEntry>& entries,
                            std::string_view key) {
  for (const BaseEntry& entry : entries) {
    if (entry.key.key == key) {
      return &entry;
    }
  }
  return nullptr;
}

bool want_field(const std::vector<std::string>* properties,
                std::string_view pseudo_id) {
  if (!properties) {
    return true;
  }
  for (const std::string& id : *properties) {
    if (id == pseudo_id) {
      return true;
    }
  }
  return false;
}

bool parse_number(std::string_view text, double& out) {
  text = trim(text);
  if (text.empty()) {
    return false;
  }
  const std::string copy(text);
  char* end = nullptr;
  out = std::strtod(copy.c_str(), &end);
  return end != copy.c_str() && end != nullptr && *end == '\0';
}

bool near_int(double value, int& out) {
  if (!std::isfinite(value)) {
    return false;
  }
  const auto rounded = std::lround(value);
  if (std::fabs(value - static_cast<double>(rounded)) > 1e-4) {
    return false;
  }
  out = static_cast<int>(rounded);
  return true;
}

bool parse_positive_int(std::string_view text, int& out) {
  double value = 0;
  if (!parse_number(text, value) || !near_int(value, out) || out <= 0) {
    return false;
  }
  return true;
}

bool orientation_from_exif(int exif, Orientation& out) {
  switch (exif) {
    case 1:
      out = Orientation{1, 0, false};
      return true;
    case 2:
      out = Orientation{2, 0, true};
      return true;
    case 3:
      out = Orientation{3, 180, false};
      return true;
    case 4:
      out = Orientation{4, 180, true};
      return true;
    case 5:
      out = Orientation{5, 90, true};
      return true;
    case 6:
      out = Orientation{6, 90, false};
      return true;
    case 7:
      out = Orientation{7, 270, true};
      return true;
    case 8:
      out = Orientation{8, 270, false};
      return true;
    default:
      return false;
  }
}

int exif_from_degrees(double degrees) {
  int turns = static_cast<int>(std::lround(degrees)) % 360;
  if (turns < 0) {
    turns += 360;
  }
  if (std::fabs(degrees - std::lround(degrees)) > 0.5) {
    return 0;
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

bool parse_rotation_degrees(std::string_view text, double& degrees) {
  text = trim(text);
  std::size_t index = 0;
  if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
    index = 1;
  }
  const std::size_t start = index;
  while (index < text.size() &&
         ((text[index] >= '0' && text[index] <= '9') || text[index] == '.')) {
    ++index;
  }
  if (index == start) {
    return false;
  }
  return parse_number(text.substr(0, index), degrees);
}

bool parse_matrix(std::string_view text, double values[9]) {
  std::size_t count = 0;
  std::size_t index = 0;
  text = trim(text);
  while (index < text.size() && count < 9) {
    while (index < text.size() && (text[index] == ' ' || text[index] == ',')) {
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
    if (!digit || !parse_number(text.substr(start, index - start), values[count])) {
      return false;
    }
    ++count;
  }
  return count == 9;
}

bool orientation_from_matrix(std::string_view text, Orientation& out) {
  double values[9];
  if (!parse_matrix(text, values)) {
    return false;
  }
  const double a = values[0];
  const double b = values[1];
  const double d = values[3];
  const double e = values[4];
  const double determinant = a * e - b * d;
  if (std::fabs(determinant) <= 0.5) {
    return false;
  }
  const double degrees = std::atan2(b, a) * 180.0 / 3.14159265358979323846;
  out.degrees = degrees;
  out.mirror = determinant < 0;
  out.exif = out.mirror ? 0 : exif_from_degrees(degrees);
  return true;
}

bool orientation_from_rotation_text(std::string_view text, Orientation& out) {
  double degrees = 0;
  if (!parse_rotation_degrees(text, degrees)) {
    return false;
  }
  out.degrees = degrees;
  out.mirror = false;
  out.exif = exif_from_degrees(degrees);
  return out.exif != 0 || std::fabs(degrees) > 1e-6;
}

std::optional<std::string> nonempty_text(std::string_view text) {
  text = trim(text);
  if (text.empty()) {
    return std::nullopt;
  }
  return std::string(text);
}

const BaseEntry* source_entry(const BaseDocument& embedded,
                              const TechnicalSourceDef& row) {
  if (row.kind == "intrinsic") {
    return find_entry(embedded.intrinsics, row.key);
  }
  return find_entry(embedded.entries, row.key);
}

bool companion_ready(const BaseDocument& embedded, const TechnicalSourceDef& row,
                     int& width, int& height) {
  const BaseEntry* primary = source_entry(embedded, row);
  if (!primary || row.companion.empty()) {
    return false;
  }
  const BaseEntry* companion =
      row.kind == "intrinsic" ? find_entry(embedded.intrinsics, row.companion)
                              : find_entry(embedded.entries, row.companion);
  if (!companion) {
    return false;
  }
  return parse_positive_int(primary->value, width) &&
         parse_positive_int(companion->value, height);
}

BaseKey source_of(const PropertyValue& property) {
  std::string key = property.preferred_source;
  if (key.empty() && !property.sources.empty()) {
    key = property.sources.front().base_key;
  }
  if (key.empty()) {
    return {};
  }
  return key_from_text(key);
}

std::optional<std::string> struct_text(const PropertyValue& property,
                                       std::string_view field) {
  const auto* structure = std::get_if<Structure>(&property.value.data);
  if (!structure) {
    return std::nullopt;
  }
  const auto it = structure->find(std::string(field));
  if (it == structure->end()) {
    return std::nullopt;
  }
  if (const auto* text = std::get_if<std::string>(&it->second.data)) {
    return nonempty_text(*text);
  }
  return std::nullopt;
}

std::optional<int> value_positive_int(const Value& value) {
  if (const auto* number = std::get_if<double>(&value.data)) {
    int out = 0;
    if (near_int(*number, out) && out > 0) {
      return out;
    }
  }
  if (const auto* text = std::get_if<std::string>(&value.data)) {
    int out = 0;
    if (parse_positive_int(*text, out)) {
      return out;
    }
  }
  return std::nullopt;
}

void fill_strings(TechnicalInfo& info, const BaseDocument& embedded,
                  std::string_view domain,
                  const std::vector<std::string>* properties) {
  for (const TechnicalSourceDef& row : kTechnicalSources) {
    if (row.domain != domain || row.kind == "intrinsic" || !row.companion.empty()) {
      continue;
    }
    if (!want_field(properties, row.pseudo_id)) {
      continue;
    }
    const bool make = row.field == "cameraMake";
    const bool model = row.field == "cameraModel";
    const bool lens = row.field == "lensModel";
    if (!make && !model && !lens) {
      continue;
    }
    if ((make && info.cameraMake) || (model && info.cameraModel) ||
        (lens && info.lensModel)) {
      continue;
    }
    const BaseEntry* entry = source_entry(embedded, row);
    if (!entry) {
      continue;
    }
    const auto text = nonempty_text(entry->value);
    if (!text) {
      continue;
    }
    TechnicalValue<std::string> value{*text, entry->key};
    if (make) {
      info.cameraMake = std::move(value);
    } else if (model) {
      info.cameraModel = std::move(value);
    } else {
      info.lensModel = std::move(value);
    }
  }
}

void fill_orientation(TechnicalInfo& info, const BaseDocument& embedded,
                      std::string_view domain,
                      const std::vector<std::string>* properties) {
  if (!want_field(properties, "technical.orientation") || info.orientation) {
    return;
  }
  for (const TechnicalSourceDef& row : kTechnicalSources) {
    if (row.domain != domain || row.field != "orientation") {
      continue;
    }
    const BaseEntry* entry = source_entry(embedded, row);
    if (!entry) {
      continue;
    }
    Orientation parsed;
    bool ok = false;
    if (domain == "photo") {
      int exif = 0;
      ok = parse_positive_int(entry->value, exif) &&
           orientation_from_exif(exif, parsed);
    } else if (std::string_view(row.key).find("MatrixStructure") !=
               std::string_view::npos) {
      ok = orientation_from_matrix(entry->value, parsed);
    } else {
      ok = orientation_from_rotation_text(entry->value, parsed);
    }
    if (!ok) {
      continue;
    }
    info.orientation = TechnicalValue<Orientation>{parsed, entry->key};
    return;
  }
}

void fill_pixel_size(TechnicalInfo& info, const BaseDocument& embedded,
                     std::string_view domain,
                     const std::vector<std::string>* properties) {
  if (!want_field(properties, "technical.pixelSize") || info.pixelSize) {
    return;
  }
  for (const TechnicalSourceDef& row : kTechnicalSources) {
    if (row.domain != domain || row.field != "pixelSize") {
      continue;
    }
    int width = 0;
    int height = 0;
    if (!companion_ready(embedded, row, width, height)) {
      continue;
    }
    const BaseEntry* entry = source_entry(embedded, row);
    if (!entry) {
      continue;
    }
    info.pixelSize =
        TechnicalValue<PixelSize>{PixelSize{width, height}, entry->key};
    return;
  }
}

void fill_video_canonical(TechnicalInfo& info, const Metadata& metadata,
                          const std::vector<std::string>* properties) {
  if (const auto device = metadata.get("iptc.video.recordingDevice")) {
    const BaseKey source = source_of(*device);
    if (!source.key.empty()) {
      auto take = [&](std::optional<TechnicalValue<std::string>>& slot,
                      std::string_view pseudo, std::string_view field) {
        if (slot || !want_field(properties, pseudo)) {
          return;
        }
        if (const auto text = struct_text(*device, field)) {
          slot = TechnicalValue<std::string>{*text, source};
        }
      };
      take(info.cameraMake, "technical.cameraMake", "manufacturer");
      take(info.cameraModel, "technical.cameraModel", "modelName");
      take(info.lensModel, "technical.lensModel", "attLensDescription");
    }
  }
  if (!info.pixelSize && want_field(properties, "technical.pixelSize")) {
    if (const auto frame = metadata.get("iptc.video.frameSize")) {
      const auto* structure = std::get_if<Structure>(&frame->value.data);
      const BaseKey source = source_of(*frame);
      if (structure && !source.key.empty()) {
        const auto width_it = structure->find("widthPixels");
        const auto height_it = structure->find("heightPixels");
        if (width_it != structure->end() && height_it != structure->end()) {
          const auto width = value_positive_int(width_it->second);
          const auto height = value_positive_int(height_it->second);
          if (width && height) {
            info.pixelSize =
                TechnicalValue<PixelSize>{PixelSize{*width, *height}, source};
          }
        }
      }
    }
  }
  if (!info.orientation && want_field(properties, "technical.orientation")) {
    if (const auto turned = metadata.get("iptc.video.orientation")) {
      const BaseKey source = source_of(*turned);
      const auto number = value_positive_int(turned->value);
      Orientation parsed;
      if (number && !source.key.empty() && orientation_from_exif(*number, parsed)) {
        info.orientation = TechnicalValue<Orientation>{parsed, source};
      }
    }
  }
}

}  // namespace

void fill_technical(Metadata& metadata, const BaseDocument& embedded,
                    const std::vector<std::string>* properties) {
  const std::string_view domain =
      metadata.mediaDomain() == MediaDomain::video ? "video" : "photo";
  TechnicalInfo info;
  fill_strings(info, embedded, domain, properties);
  fill_orientation(info, embedded, domain, properties);
  fill_pixel_size(info, embedded, domain, properties);
  if (domain == "video") {
    fill_video_canonical(info, metadata, properties);
  }
  metadata.assignTechnical(std::move(info));
}

}  // namespace umm::internal
