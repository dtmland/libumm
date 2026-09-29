#include "umm/version.hpp"

namespace umm {

std::string_view version() noexcept {
  return UMM_VERSION_STRING;
}

}  // namespace umm
