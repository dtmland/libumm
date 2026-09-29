#pragma once

#include <string_view>

// Compile-time library version (session 30). These macros are the public
// contract for feature tests. They must match CMake PROJECT_VERSION and
// umm::version(); they do not encode standards versions (concept.md §21).
#define UMM_VERSION_MAJOR 0
#define UMM_VERSION_MINOR 1
#define UMM_VERSION_PATCH 0

#define UMM_VERSION_STRING_XSTR(s) #s
#define UMM_VERSION_STRING_STR(maj, min, pat) \
  UMM_VERSION_STRING_XSTR(maj) "." UMM_VERSION_STRING_XSTR(min) "." UMM_VERSION_STRING_XSTR(pat)
#define UMM_VERSION_STRING \
  UMM_VERSION_STRING_STR(UMM_VERSION_MAJOR, UMM_VERSION_MINOR, UMM_VERSION_PATCH)

namespace umm {

// Library implementation version (independent of supported standards versions;
// see concept.md §21 and registry provenance for standards versions).
std::string_view version() noexcept;

}  // namespace umm
