#include "core/selective.hpp"

#include "core/xmp_codec.hpp"
#include "exiftool/keys.hpp"
#include "property_registry.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace umm::internal {
namespace {

void add_unique(std::vector<std::string>& out, std::string tag) {
  if (tag.empty() || tag.find('\n') != std::string::npos ||
      tag.find('\r') != std::string::npos) {
    return;
  }
  for (const std::string& existing : out) {
    if (existing == tag) {
      return;
    }
  }
  out.push_back(std::move(tag));
}

void add_exiftool_tag(std::vector<std::string>& out, std::string tag) {
  if (tag.empty()) {
    return;
  }
  const auto colon = tag.rfind(':');
  if (colon != std::string::npos && colon + 1 < tag.size()) {
    add_unique(out, tag.substr(colon + 1));
  }
  add_unique(out, std::move(tag));
}

void add_base_key(std::vector<std::string>& out, std::string_view base_key) {
  if (base_key.empty()) {
    return;
  }
  const auto tag = exiftool_tag_for_base_key(base_key);
  if (!tag) {
    return;
  }
  add_exiftool_tag(out, *tag);
  // gps_group falls back to the composite position when the latitude key
  // is the EXIF GPS IFD and the split lat/lon text does not parse.
  constexpr std::string_view kGpsLat = "Exif.GPSInfo.GPSLatitude";
  if (base_key == kGpsLat) {
    add_unique(out, "GPSPosition");
  }
}

void add_exif_tokens(std::vector<std::string>& out, std::string_view tokens) {
  std::string_view rest = tokens;
  while (!rest.empty()) {
    const auto plus = rest.find('+');
    const std::string_view item =
        plus == std::string_view::npos ? rest : rest.substr(0, plus);
    if (!item.empty()) {
      add_base_key(out, exif_base_key(item));
    }
    if (plus == std::string_view::npos) {
      break;
    }
    rest.remove_prefix(plus + 1);
  }
}

void add_xmp_tokens(std::vector<std::string>& out, std::string_view property) {
  for (const std::string& key : xmp_base_keys(property)) {
    add_base_key(out, key);
  }
}

std::string_view struct_prefix(std::string_view property_id) {
  constexpr std::string_view kPhoto = "iptc.photo.";
  constexpr std::string_view kVideo = "iptc.video.";
  if (property_id.size() >= kPhoto.size() &&
      property_id.substr(0, kPhoto.size()) == kPhoto) {
    return "iptc.photo.struct.";
  }
  if (property_id.size() >= kVideo.size() &&
      property_id.substr(0, kVideo.size()) == kVideo) {
    return "iptc.video.struct.";
  }
  return {};
}

std::string local_name(std::string_view id) {
  const auto dot = id.rfind('.');
  if (dot == std::string_view::npos) {
    return std::string(id);
  }
  return std::string(id.substr(dot + 1));
}

bool starts_with(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

void add_struct_fields(std::vector<std::string>& out, std::string_view property_id) {
  std::string_view type;
  for (const PropertyStructDef& row : kPropertyStructs) {
    if (row.property_id == property_id) {
      type = row.struct_name;
      break;
    }
  }
  if (type.empty()) {
    return;
  }
  const std::string_view prefix = struct_prefix(property_id);
  const std::string local = local_name(property_id);
  for (const StructFieldRepresentation& field : kStructFieldRepresentations) {
    if (field.struct_name != type) {
      continue;
    }
    if (!prefix.empty() && !starts_with(field.id, prefix)) {
      continue;
    }
    if (!field.struct_property.empty() && field.struct_property != local) {
      continue;
    }
    add_xmp_tokens(out, field.xmp_property);
    add_exif_tokens(out, field.exif_tag);
    if (!field.et_tag.empty()) {
      add_unique(out, std::string(field.et_tag));
    }
  }
}

void add_property(std::vector<std::string>& out, std::string_view property_id) {
  const auto def = registry().find(property_id);
  if (!def) {
    return;
  }
  const Representations& rep = def->representations;
  add_xmp_tokens(out, rep.xmp_property);
  add_exif_tokens(out, rep.exif_tag);
  if (!rep.iim_dataset.empty()) {
    add_base_key(out, iim_base_key(rep.iim_dataset));
    for (std::string_view companion : iim_companion_datasets(rep.iim_dataset)) {
      add_base_key(out, iim_base_key(companion));
    }
  }
  for (const std::string& key : quicktime_base_keys(rep.quicktime_key)) {
    add_base_key(out, key);
  }
  add_struct_fields(out, property_id);
}

}  // namespace

std::vector<std::string> exiftool_tags_for_properties(
    const std::vector<std::string>& property_ids) {
  std::vector<std::string> tags;
  for (const std::string& id : property_ids) {
    add_property(tags, id);
  }
  return tags;
}

}  // namespace umm::internal
