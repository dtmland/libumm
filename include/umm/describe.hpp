// Property map (C12b): every layer libumm can traverse for a canonical
// property or a cross-media accessor name.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "umm/metadata.hpp"
#include "umm/registry.hpp"
#include "umm/result.hpp"

namespace umm {

// One standard-defined representation (L1) or struct-field path.
struct RepresentationMap {
  std::string family;  // "xmp" | "iim" | "exif" | "quicktime" | "ebucore" | "struct_field"
  std::string key;     // registry token (XMP property, IIM dataset, EXIF tag, …)
  std::string path;    // XMP namespace, companion tags, or struct-field id
  std::string citation;
  int read_rank{0};    // lower is preferred (docs/reconciliation-policy.md)
  bool write_target{true};
  std::optional<std::string> value;  // file mode: matching base text

  bool operator==(const RepresentationMap&) const = default;
};

struct StructFieldMap {
  std::string id;
  std::string name;
  std::string struct_name;
  std::string xmp_property;
  std::string et_tag;
  std::string exif_tag;

  bool operator==(const StructFieldMap&) const = default;
};

// One cast rule that touches the property (L2).
struct CastLinkMap {
  std::string group;
  CastDirection direction = CastDirection::up;
  std::string partner;
  std::string heuristic;
  std::string citation;
  std::optional<CastStatus> status;  // file mode, from the group
  std::string source_preview;
  std::string target_preview;

  bool operator==(const CastLinkMap&) const = default;
};

// L1–L2 for one registry id (no nested L3).
struct PropertyLayers {
  std::string id;
  PropertyDef definition;
  std::vector<StructFieldMap> struct_fields;
  std::vector<RepresentationMap> representations;
  std::vector<CastLinkMap> casts;
  std::optional<PropertyValue> value;  // file mode
  std::vector<CastCandidate> cast_groups;
  std::vector<BaseEntry> consumed;

  bool operator==(const PropertyLayers&) const = default;
};

struct CrossMediaMap {
  std::string accessor;
  int tier{0};
  PropertyLayers other;  // the other domain's L1–L2

  bool operator==(const CrossMediaMap&) const = default;
};

struct PropertyDescription {
  PropertyLayers layers;
  std::optional<CrossMediaMap> cross_media;

  bool operator==(const PropertyDescription&) const = default;
};

struct PropertyMap {
  std::string query;
  std::vector<PropertyDescription> properties;

  bool operator==(const PropertyMap&) const = default;
};

// Registry / accessor lookup only. unknown_property when the query matches
// neither a registry id nor a cross-media concept name.
Result<PropertyMap> describe(std::string_view property_id);

// Same map, filled with values, consumed base entries, and cast-group
// statuses from umm::read + umm::cast dry-run (C12b file mode).
Result<PropertyMap> describe(std::string_view property_id,
                             const std::filesystem::path& media);

}  // namespace umm
