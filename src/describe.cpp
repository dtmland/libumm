#include "umm/umm.hpp"

#include "cast_rules.hpp"
#include "core/cast.hpp"
#include "cross_media_accessors.hpp"
#include "property_registry.hpp"

#include <utility>

namespace umm {
namespace {

std::string citation_for(const PropertyDef& def) {
  std::string out(def.standard);
  if (!def.standard_version.empty()) {
    out.push_back(' ');
    out.append(def.standard_version);
  }
  return out;
}

std::string local_name(std::string_view id) {
  const auto dot = id.rfind('.');
  if (dot == std::string_view::npos) {
    return std::string(id);
  }
  return std::string(id.substr(dot + 1));
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

std::string_view struct_name_for(std::string_view property_id) {
  for (const internal::PropertyStructDef& row : internal::kPropertyStructs) {
    if (row.property_id == property_id) {
      return row.struct_name;
    }
  }
  return {};
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

std::string partner_of(const internal::CastRuleDef& rule, std::string_view id) {
  const bool source_hit = rule.source_key == id;
  const bool target_hit = rule.target_key == id;
  if (source_hit && !target_hit) {
    std::string out(rule.target_key);
    if (!rule.target_field.empty()) {
      out.push_back('.');
      out.append(rule.target_field);
    }
    return out;
  }
  if (target_hit && !source_hit) {
    std::string out(rule.source_key);
    if (!rule.source_field.empty()) {
      out.push_back('.');
      out.append(rule.source_field);
    }
    return out;
  }
  if (source_hit && target_hit) {
    if (!rule.target_field.empty()) {
      return std::string(rule.target_field);
    }
    if (!rule.source_field.empty()) {
      return std::string(rule.source_field);
    }
  }
  return {};
}

bool rule_touches(const internal::CastRuleDef& rule, std::string_view id) {
  return rule.source_key == id || rule.target_key == id;
}

int default_rank(std::string_view family) {
  if (family == "xmp" || family == "struct_field") {
    return 0;
  }
  if (family == "iim") {
    return 1;
  }
  if (family == "exif") {
    return 2;
  }
  if (family == "quicktime") {
    return 3;
  }
  if (family == "ebucore") {
    return 4;
  }
  return 5;
}

bool starts_with(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

void push_representation(std::vector<RepresentationMap>& out, std::string family,
                         std::string_view key, std::string path,
                         const std::string& citation, int rank) {
  if (key.empty()) {
    return;
  }
  RepresentationMap row;
  row.family = std::move(family);
  row.key = std::string(key);
  row.path = std::move(path);
  row.citation = citation;
  row.read_rank = rank;
  row.write_target = true;
  out.push_back(std::move(row));
}

std::vector<StructFieldMap> struct_fields_for(std::string_view property_id) {
  std::vector<StructFieldMap> out;
  const std::string_view type = struct_name_for(property_id);
  if (type.empty()) {
    return out;
  }
  const std::string_view prefix = struct_prefix(property_id);
  const std::string local = local_name(property_id);
  for (const internal::StructFieldRepresentation& field :
       internal::kStructFieldRepresentations) {
    if (field.struct_name != type) {
      continue;
    }
    if (!prefix.empty() && !starts_with(field.id, prefix)) {
      continue;
    }
    if (!field.struct_property.empty() && field.struct_property != local) {
      continue;
    }
    StructFieldMap row;
    row.id = std::string(field.id);
    row.name = local_name(field.id);
    row.struct_name = std::string(field.struct_name);
    row.xmp_property = std::string(field.xmp_property);
    row.et_tag = std::string(field.et_tag);
    row.exif_tag = std::string(field.exif_tag);
    out.push_back(std::move(row));
  }
  return out;
}

std::vector<RepresentationMap> representations_for(const PropertyDef& def) {
  std::vector<RepresentationMap> out;
  const std::string cite = citation_for(def);
  push_representation(out, "xmp", def.representations.xmp_property,
                      std::string(def.representations.xmp_namespace), cite,
                      default_rank("xmp"));
  push_representation(out, "iim", def.representations.iim_dataset, {}, cite,
                      default_rank("iim"));
  push_representation(out, "exif", def.representations.exif_tag, {}, cite,
                      default_rank("exif"));
  push_representation(out, "quicktime", def.representations.quicktime_key, {},
                      cite, default_rank("quicktime"));
  push_representation(out, "ebucore", def.representations.ebucore, {}, cite,
                      default_rank("ebucore"));

  const std::string_view type = struct_name_for(def.id);
  if (type.empty()) {
    return out;
  }
  const std::string_view prefix = struct_prefix(def.id);
  const std::string local = local_name(def.id);
  for (const internal::StructFieldRepresentation& field :
       internal::kStructFieldRepresentations) {
    if (field.struct_name != type) {
      continue;
    }
    if (!prefix.empty() && !starts_with(field.id, prefix)) {
      continue;
    }
    if (!field.struct_property.empty() && field.struct_property != local) {
      continue;
    }
    const bool gps_overlay = !field.exif_tag.empty() && !field.struct_property.empty();
    if (!field.xmp_property.empty()) {
      const bool xmp_exif = starts_with(field.xmp_property, "exif:");
      const int rank = gps_overlay ? (xmp_exif ? 1 : 2) : default_rank("struct_field");
      push_representation(out, "struct_field", field.xmp_property, std::string(field.id),
                          cite, rank);
    }
    if (!field.exif_tag.empty()) {
      const int rank = gps_overlay ? 0 : default_rank("exif");
      push_representation(out, "exif", field.exif_tag, std::string(field.id), cite,
                          rank);
    }
  }
  return out;
}

std::vector<CastLinkMap> casts_for(std::string_view property_id) {
  std::vector<CastLinkMap> out;
  for (const internal::CastRuleDef& rule : internal::kCastRules) {
    if (!rule_touches(rule, property_id)) {
      continue;
    }
    CastLinkMap row;
    row.group = std::string(rule.group);
    row.direction = parse_direction(rule.direction);
    row.partner = partner_of(rule, property_id);
    row.heuristic = std::string(rule.heuristic);
    row.citation = std::string(rule.citation);
    out.push_back(std::move(row));
  }
  return out;
}

const internal::CrossMediaAccessorDef* accessor_for(std::string_view query) {
  for (const internal::CrossMediaAccessorDef& acc : internal::kCrossMediaAccessors) {
    if (acc.concept_name == query) {
      return &acc;
    }
    for (std::size_t i = 0; i < acc.photo_id_count; ++i) {
      if (acc.photo_ids[i] == query) {
        return &acc;
      }
    }
    for (std::size_t i = 0; i < acc.video_id_count; ++i) {
      if (acc.video_ids[i] == query) {
        return &acc;
      }
    }
  }
  return nullptr;
}

std::vector<std::string> ids_for_query(std::string_view query) {
  if (registry().find(query)) {
    return {std::string(query)};
  }
  const internal::CrossMediaAccessorDef* acc = accessor_for(query);
  if (!acc || acc->concept_name != query) {
    return {};
  }
  std::vector<std::string> ids;
  for (std::size_t i = 0; i < acc->photo_id_count; ++i) {
    if (!acc->photo_ids[i].empty()) {
      ids.emplace_back(acc->photo_ids[i]);
    }
  }
  for (std::size_t i = 0; i < acc->video_id_count; ++i) {
    if (!acc->video_ids[i].empty()) {
      ids.emplace_back(acc->video_ids[i]);
    }
  }
  return ids;
}

std::string other_id_for(const internal::CrossMediaAccessorDef& acc,
                         std::string_view property_id) {
  for (std::size_t i = 0; i < acc.photo_id_count; ++i) {
    if (acc.photo_ids[i] == property_id) {
      return acc.video_id_count > 0 ? std::string(acc.video_ids[0]) : std::string();
    }
  }
  for (std::size_t i = 0; i < acc.video_id_count; ++i) {
    if (acc.video_ids[i] == property_id) {
      return acc.photo_id_count > 0 ? std::string(acc.photo_ids[0]) : std::string();
    }
  }
  return {};
}

PropertyLayers layers_for(std::string_view property_id) {
  PropertyLayers layers{};
  layers.id = std::string(property_id);
  if (const auto def = registry().find(property_id)) {
    layers.definition = *def;
  }
  layers.struct_fields = struct_fields_for(property_id);
  layers.representations = representations_for(layers.definition);
  layers.casts = casts_for(property_id);
  return layers;
}

PropertyDescription describe_id(std::string_view property_id) {
  PropertyDescription out;
  out.layers = layers_for(property_id);
  const internal::CrossMediaAccessorDef* acc = accessor_for(property_id);
  if (!acc) {
    return out;
  }
  const std::string other = other_id_for(*acc, property_id);
  if (other.empty() || other == property_id) {
    CrossMediaMap link;
    link.accessor = std::string(acc->concept_name);
    link.tier = acc->tier;
    out.cross_media = std::move(link);
    return out;
  }
  CrossMediaMap link;
  link.accessor = std::string(acc->concept_name);
  link.tier = acc->tier;
  link.other = layers_for(other);
  out.cross_media = std::move(link);
  return out;
}

bool source_matches(std::string_view entry_key, std::string_view source) {
  if (entry_key == source) {
    return true;
  }
  if (source.empty() || entry_key.size() <= source.size()) {
    return false;
  }
  if (!starts_with(entry_key, source)) {
    return false;
  }
  const char next = entry_key[source.size()];
  return next == '[' || next == '/';
}

std::string family_of_entry(const BaseEntry& entry) {
  if (entry.key.family == "Xmp" || starts_with(entry.key.key, "Xmp.")) {
    return "xmp";
  }
  if (entry.key.family == "Iptc" || starts_with(entry.key.key, "Iptc.")) {
    return "iim";
  }
  if (entry.key.family == "Exif" || starts_with(entry.key.key, "Exif.")) {
    return "exif";
  }
  if (entry.key.family == "QuickTime" || starts_with(entry.key.key, "QuickTime.")) {
    return "quicktime";
  }
  return {};
}

void fill_layers(PropertyLayers& layers, const Metadata& metadata,
                 const std::vector<CastCandidate>& candidates) {
  if (const auto value = metadata.get(layers.id)) {
    layers.value = *value;
    for (const SourceRef& source : value->sources) {
      for (const BaseEntry& entry : metadata.dumpAll()) {
        if (source_matches(entry.key.key, source.base_key)) {
          layers.consumed.push_back(entry);
        }
      }
    }
  }
  for (RepresentationMap& rep : layers.representations) {
    for (const BaseEntry& entry : layers.consumed) {
      const std::string family = family_of_entry(entry);
      if (family.empty()) {
        continue;
      }
      if (rep.family == family ||
          (rep.family == "struct_field" && family == "xmp")) {
        if (!rep.value) {
          rep.value = entry.value;
        }
      }
    }
  }
  for (const CastCandidate& candidate : candidates) {
    bool touches = false;
    for (const CastLinkMap& link : layers.casts) {
      if (link.group == candidate.group && link.direction == candidate.direction) {
        touches = true;
        break;
      }
    }
    if (!touches) {
      continue;
    }
    layers.cast_groups.push_back(candidate);
    for (CastLinkMap& link : layers.casts) {
      if (link.group == candidate.group && link.direction == candidate.direction) {
        link.status = candidate.status;
        link.source_preview = candidate.source_preview;
        link.target_preview = candidate.target_preview;
      }
    }
  }
}

void fill_description(PropertyDescription& description, const Metadata& metadata,
                      const std::vector<CastCandidate>& candidates) {
  fill_layers(description.layers, metadata, candidates);
  if (description.cross_media) {
    fill_layers(description.cross_media->other, metadata, candidates);
  }
}

Result<PropertyMap> build_map(std::string_view query) {
  const std::vector<std::string> ids = ids_for_query(query);
  if (ids.empty()) {
    return Error{ErrorCode::unknown_property,
                 "unknown property: " + std::string(query), "", ""};
  }
  PropertyMap map;
  map.query = std::string(query);
  for (const std::string& id : ids) {
    map.properties.push_back(describe_id(id));
  }
  return map;
}

std::vector<CastCandidate> all_cast_candidates(const Metadata& metadata,
                                               std::string_view file_type,
                                               const Capabilities* caps) {
  CastOptions options;
  std::vector<CastCandidate> out;
  for (const CastDirection direction :
       {CastDirection::up, CastDirection::down, CastDirection::side}) {
    auto rows = internal::evaluate_casts(metadata, direction, options, file_type,
                                         caps);
    out.insert(out.end(), rows.begin(), rows.end());
  }
  return out;
}

}  // namespace

Result<PropertyMap> describe(std::string_view property_id) {
  return build_map(property_id);
}

Result<PropertyMap> describe(std::string_view property_id,
                             const std::filesystem::path& media) {
  Result<PropertyMap> mapped = build_map(property_id);
  if (!mapped.ok()) {
    return mapped.error();
  }
  Result<Metadata> read_result = read(media);
  if (!read_result.ok()) {
    return read_result.error();
  }
  Metadata metadata = std::move(read_result).value();
  std::optional<Capabilities> caps;
  std::string file_type;
  if (const auto discovered = capabilities(media); discovered.ok()) {
    caps = discovered.value();
    file_type = caps->file_type;
  }
  const Capabilities* caps_ptr = caps ? &*caps : nullptr;
  const auto candidates = all_cast_candidates(metadata, file_type, caps_ptr);
  PropertyMap map = std::move(mapped).value();
  for (PropertyDescription& description : map.properties) {
    fill_description(description, metadata, candidates);
  }
  return map;
}

}  // namespace umm
