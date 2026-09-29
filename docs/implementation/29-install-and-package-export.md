# Session 29 — Install rules and CMake package export

Stage 11 · Estimated 45–60 min

## Goal

Make libumm consumable as an installed package: `cmake --install` produces a working prefix and a
downstream project can `find_package(umm CONFIG)` and link `umm::umm`. This is the foundation of
every release artifact (analysis 2026-09-29 §7) and of the umm CLI's installed-package mode
(docs/umm-cli-concept.md §3.2).

## Prerequisites

Stage 10 complete. No public-header changes expected (install only ships what exists).

## Deliverables

- `install()` rules for the `umm` target (library + `include/umm/` headers), namespaced export
  `umm::umm`, and generated `ummConfig.cmake` + `ummConfigVersion.cmake`
  (`write_basic_package_version_file`, SameMajorVersion pre-1.0 caveat per session 30 policy).
- Correct handling of the static-default build: the installed static `libumm` must carry its
  private Exiv2/Expat/zlib objects or archives such that a consumer links successfully without
  finding Exiv2 itself (verify on all three OSes; document the chosen mechanism — object
  library merge vs installing the dependency archives as IMPORTED deps of the export).
- The existing `CMAKE_SKIP_INSTALL_RULES` shielding for FetchContent'd deps stays intact —
  installing libumm must not install Exiv2/Expat/zlib headers or CMake files into the prefix.
  Stub the skipped subdirectory `cmake_install.cmake` files so the parent install script can
  include them without installing those packages.
- Consumer smoke test: a tiny out-of-tree CMake project (under `tests/consumer/`) that
  `find_package(umm CONFIG REQUIRED)` against a scratch install prefix, compiles a call to
  `umm::version()` and one `umm::read` of a Tier A fixture, run as a CTest step (configure +
  build + run via `cmake -E`/`execute_process`, same pattern as existing build-contract tests).
- CI: an install + consumer-smoke step added to the existing matrix jobs (no new workflow);
  workflow contract tests extended.

## Steps

1. Author install/export rules; resolve the static-transitive-deps question first on Linux, then
   verify Windows (MSVC .lib naming) and macOS.
2. Add the consumer smoke project and CTest wiring.
3. Extend CI + contract tests; all three OSes green.

## Acceptance criteria

- `cmake --install` into a scratch prefix, then the consumer project configures, builds, links,
  and runs `umm::read` on all three OSes using only the prefix.
- No Exiv2/Expat/zlib headers or CMake package files appear in the install prefix.
- `find_package(umm 0.1 CONFIG)` version-checks correctly.

## Cut line

The shared-library (`BUILD_SHARED_LIBS=ON`) install verification may defer to session 32; the
static install + consumer smoke test may not.

## Out of scope

Versioning/ABI policy documents (session 30); notices in the install tree (session 31); CPack or
archive creation (session 34).

## References

analysis 2026-09-29 §5, §7, §9 item 2; build-plan.md §8 (target naming `umm::umm`); decision M4c.
