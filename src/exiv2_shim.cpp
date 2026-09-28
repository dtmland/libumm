#include "exiv2_shim.hpp"

#include <exiv2/exiv2.hpp>

namespace umm::internal {

std::string exiv2_version() {
  return Exiv2::versionString();
}

}  // namespace umm::internal
