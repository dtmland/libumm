#include "umm/version.hpp"

#ifndef UMM_VERSION
#error "UMM_VERSION must be defined by the build"
#endif

namespace umm {

std::string_view version() noexcept {
  return UMM_VERSION;
}

}  // namespace umm
