#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "umm/metadata.hpp"

namespace umm::internal {

// Translate an ExifTool -G1 JSON field (Group1:Tag) into the Exiv2-syntax
// raw vocabulary. nullopt means the field is not stored metadata (File,
// ExifTool, Composite, SourceFile, Error/Warning).
std::optional<RawKey> map_exiftool_tag(std::string_view json_key);

}  // namespace umm::internal
