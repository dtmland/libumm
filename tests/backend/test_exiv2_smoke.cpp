#include "exiv2_shim.hpp"

#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>

#ifndef UMM_TEST_EXIV2_VERSION
#error "UMM_TEST_EXIV2_VERSION must be defined by the build"
#endif

#ifndef UMM_TEST_EXIV2_MIN_VERSION
#error "UMM_TEST_EXIV2_MIN_VERSION must be defined by the build"
#endif

namespace {

int version_part(std::string_view text, std::size_t* index) {
  int value = 0;
  bool any = false;
  while (*index < text.size()) {
    const char c = text[*index];
    if (c < '0' || c > '9') {
      break;
    }
    any = true;
    value = value * 10 + (c - '0');
    ++*index;
  }
  return any ? value : -1;
}

bool version_at_least(std::string_view observed, std::string_view floor) {
  std::size_t oi = 0;
  std::size_t fi = 0;
  for (int i = 0; i < 3; ++i) {
    const int ov = version_part(observed, &oi);
    const int fv = version_part(floor, &fi);
    const int lhs = ov < 0 ? 0 : ov;
    const int rhs = fv < 0 ? 0 : fv;
    if (lhs > rhs) {
      return true;
    }
    if (lhs < rhs) {
      return false;
    }
    if (oi < observed.size() && observed[oi] == '.') {
      ++oi;
    }
    if (fi < floor.size() && floor[fi] == '.') {
      ++fi;
    }
  }
  return true;
}

}  // namespace

int main() {
  const std::string observed = umm::internal::exiv2_version();
  constexpr std::string_view expected = UMM_TEST_EXIV2_VERSION;
  constexpr std::string_view floor = UMM_TEST_EXIV2_MIN_VERSION;

  if (observed.empty()) {
    std::fprintf(stderr, "umm::internal::exiv2_version() is empty\n");
    return 1;
  }

#if defined(UMM_TEST_EXIV2_SYSTEM)
  if (!version_at_least(observed, floor)) {
    std::fprintf(stderr,
                 "umm::internal::exiv2_version() is \"%s\", below floor \"%.*s\"\n",
                 observed.c_str(),
                 static_cast<int>(floor.size()), floor.data());
    return 1;
  }
#else
  if (observed != expected) {
    std::fprintf(stderr,
                 "umm::internal::exiv2_version() is \"%s\", expected \"%.*s\"\n",
                 observed.c_str(),
                 static_cast<int>(expected.size()), expected.data());
    return 1;
  }
#endif

  return 0;
}
