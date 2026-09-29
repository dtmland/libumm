#include <umm/umm.hpp>

#include <cstdio>
#include <string>
#include <string_view>

#if !defined(UMM_VERSION_MAJOR) || !defined(UMM_VERSION_STRING)
#error "installed umm/version.hpp must provide UMM_VERSION_* macros"
#endif

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: umm_consumer FILE\n");
    return 2;
  }

  const std::string_view observed = umm::version();
  if (observed.empty()) {
    std::fprintf(stderr, "umm::version() is empty\n");
    return 1;
  }

  const auto result = umm::read(argv[1]);
  if (!result) {
    const umm::Error& error = result.error();
    std::fprintf(stderr, "umm::read failed (%s): %s\n",
                 error.backend.c_str(), error.message.c_str());
    return 1;
  }

  std::fprintf(stdout, "%s\n", std::string(observed).c_str());
  return 0;
}
