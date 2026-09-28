#include <cstdio>

int main() {
  std::fprintf(stderr,
               "UMM_FAILING_SELFTEST: deliberately failing because "
               "UMM_ENABLE_FAILING_SELFTEST=ON\n");
  return 1;
}
