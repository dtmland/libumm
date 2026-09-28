#include "umm/version.hpp"

#include <cstdio>
#include <string_view>

#ifndef UMM_PROJECT_VERSION
#error "UMM_PROJECT_VERSION must be defined by the build"
#endif

int main() {
  const std::string_view observed = umm::version();
  constexpr std::string_view expected = UMM_PROJECT_VERSION;

  if (observed.empty()) {
    std::fprintf(stderr, "umm::version() is empty\n");
    return 1;
  }

  if (observed != expected) {
    std::fprintf(stderr,
                 "umm::version() is \"%.*s\", expected \"%.*s\"\n",
                 static_cast<int>(observed.size()), observed.data(),
                 static_cast<int>(expected.size()), expected.data());
    return 1;
  }

  return 0;
}
