#pragma once

#include <string>

namespace umm::internal {

// Runtime Exiv2 version string from Exiv2::versionString(). Not part of the public API.
std::string exiv2_version();

}  // namespace umm::internal
