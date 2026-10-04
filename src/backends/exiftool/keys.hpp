#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "umm/metadata.hpp"

namespace umm::internal {

// Translate an ExifTool -G1 JSON field (Group1:Tag) into the Exiv2-syntax
// base vocabulary. nullopt means the field is not stored metadata (File,
// ExifTool, Composite, SourceFile, Error/Warning).
std::optional<BaseKey> map_exiftool_tag(std::string_view json_key);

// Inverse of map_exiftool_tag for write commands. Indexed/struct suffixes
// are stripped. nullopt means the base key cannot be expressed as a tag.
std::optional<std::string> exiftool_tag_for_base_key(std::string_view base_key);

// ExifTool assignment operator for a write. PLUS/xmpDM CV tags, URI-like
// XMP values, DigitalSourceType, and *Type/*Mode/*Status/*Ready/*Order
// tags need "#=" so print conversion does not reject the value. Ordinary
// XMP lang-alt/bag/GPS writes must keep "=" so Exiv2 can read the packet.
std::string_view exiftool_assign_operator(std::string_view tag,
                                          std::string_view value);

}  // namespace umm::internal
