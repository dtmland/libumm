#pragma once

#include <string>
#include <vector>

namespace umm::internal {

// ExifTool -TAG arguments derived from registry representations of the
// listed ids (generated tables plus the existing base-key converters).
// Empty when no token converts. Does not validate ids.
std::vector<std::string> exiftool_tags_for_properties(
    const std::vector<std::string>& property_ids);

}  // namespace umm::internal
