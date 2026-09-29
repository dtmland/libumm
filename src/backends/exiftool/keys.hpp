#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "umm/metadata.hpp"

namespace umm::internal {

// Translate an ExifTool -G1 JSON field (Group1:Tag) into the Exiv2-syntax
// raw vocabulary. nullopt means the field is not stored metadata (File,
// ExifTool, Composite, SourceFile, Error/Warning).
std::optional<UnmappedKey> map_exiftool_tag(std::string_view json_key);

// Inverse of map_exiftool_tag for write commands. Indexed/struct suffixes
// are stripped. nullopt means the raw key cannot be expressed as a tag.
std::optional<std::string> exiftool_tag_for_unmapped_key(std::string_view raw_key);

}  // namespace umm::internal
