#include "exiv2_shim.hpp"

#include <cstdio>
#include <string>
#include <string_view>

#ifndef UMM_TEST_EXIV2_VERSION
#error "UMM_TEST_EXIV2_VERSION must be defined by the build"
#endif

int main() {
  const std::string observed = umm::internal::exiv2_version();
  constexpr std::string_view expected = UMM_TEST_EXIV2_VERSION;

  if (observed.empty()) {
    std::fprintf(stderr, "umm::internal::exiv2_version() is empty\n");
    return 1;
  }

  if (observed != expected) {
    std::fprintf(stderr,
                 "umm::internal::exiv2_version() is \"%s\", expected \"%.*s\"\n",
                 observed.c_str(),
                 static_cast<int>(expected.size()), expected.data());
    return 1;
  }

  return 0;
}
