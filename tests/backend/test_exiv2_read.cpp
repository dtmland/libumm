#include "read_base_checks.hpp"
#include "umm/backend.hpp"

#include <cstdio>
#include <string_view>

#ifndef UMM_TEST_EXIV2_VERSION
#error "UMM_TEST_EXIV2_VERSION must be defined by the build"
#endif

int main() {
  umm::BackendManager& manager = umm::BackendManager::instance();
  const auto ids = manager.backendIds();
  if (ids.empty() || ids.front() != "exiv2") {
    return raw_fail("BackendManager does not register exiv2 first");
  }
  umm::Backend* backend = manager.get("exiv2");
  if (!backend || backend != manager.firstAvailable()) {
    return raw_fail("get(exiv2) / firstAvailable mismatch");
  }

  const umm::BackendAvailability status = backend->availability();
  if (!status.available) {
    std::fprintf(stderr, "exiv2 unavailable: %s\n", status.reason.c_str());
    return 1;
  }
  if (status.version != UMM_TEST_EXIV2_VERSION) {
    std::fprintf(stderr, "exiv2 version \"%s\", expected \"%s\"\n",
                 status.version.c_str(), UMM_TEST_EXIV2_VERSION);
    return 1;
  }

  if (const int rc = check_jpeg_unmapped_reads(*backend, "exiv2"); rc != 0) {
    return rc;
  }
  if (const int rc = check_tiff_unmapped_reads(*backend, "exiv2"); rc != 0) {
    return rc;
  }
  if (const int rc = check_png_unmapped_reads(*backend, "exiv2"); rc != 0) {
    return rc;
  }
  if (const int rc = check_webp_unmapped_reads(*backend, "exiv2"); rc != 0) {
    return rc;
  }
  return check_dng_unmapped_reads(*backend, "exiv2");
}
