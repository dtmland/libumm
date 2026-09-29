#include "umm/version.hpp"

#include <cstdio>
#include <string_view>

#ifndef UMM_PROJECT_VERSION
#error "UMM_PROJECT_VERSION must be defined by the build"
#endif

#ifndef UMM_CMAKE_VERSION_MAJOR
#error "UMM_CMAKE_VERSION_MAJOR must be defined by the build"
#endif
#ifndef UMM_CMAKE_VERSION_MINOR
#error "UMM_CMAKE_VERSION_MINOR must be defined by the build"
#endif
#ifndef UMM_CMAKE_VERSION_PATCH
#error "UMM_CMAKE_VERSION_PATCH must be defined by the build"
#endif

#if !defined(UMM_VERSION_MAJOR) || !defined(UMM_VERSION_MINOR) || \
    !defined(UMM_VERSION_PATCH) || !defined(UMM_VERSION_STRING)
#error "UMM_VERSION_* macros must be defined by umm/version.hpp"
#endif

static_assert(UMM_VERSION_MAJOR == UMM_CMAKE_VERSION_MAJOR,
              "UMM_VERSION_MAJOR disagrees with CMake PROJECT_VERSION");
static_assert(UMM_VERSION_MINOR == UMM_CMAKE_VERSION_MINOR,
              "UMM_VERSION_MINOR disagrees with CMake PROJECT_VERSION");
static_assert(UMM_VERSION_PATCH == UMM_CMAKE_VERSION_PATCH,
              "UMM_VERSION_PATCH disagrees with CMake PROJECT_VERSION");

int main() {
  const std::string_view observed = umm::version();
  constexpr std::string_view expected = UMM_PROJECT_VERSION;
  constexpr std::string_view from_macros = UMM_VERSION_STRING;

  if (observed.empty()) {
    std::fprintf(stderr, "umm::version() is empty\n");
    return 1;
  }

  if (from_macros != expected) {
    std::fprintf(stderr,
                 "UMM_VERSION_STRING is \"%.*s\", expected \"%.*s\"\n",
                 static_cast<int>(from_macros.size()), from_macros.data(),
                 static_cast<int>(expected.size()), expected.data());
    return 1;
  }

  if (observed != expected) {
    std::fprintf(stderr,
                 "umm::version() is \"%.*s\", expected \"%.*s\"\n",
                 static_cast<int>(observed.size()), observed.data(),
                 static_cast<int>(expected.size()), expected.data());
    return 1;
  }

  if (observed != from_macros) {
    std::fprintf(stderr,
                 "umm::version() is \"%.*s\", UMM_VERSION_STRING is \"%.*s\"\n",
                 static_cast<int>(observed.size()), observed.data(),
                 static_cast<int>(from_macros.size()), from_macros.data());
    return 1;
  }

  return 0;
}
