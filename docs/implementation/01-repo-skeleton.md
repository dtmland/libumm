# Session 01 — Repository skeleton

Stage 0 · Estimated 30–45 min

## Goal

A buildable, testable empty library on the local machine: CMake + presets + empty `umm` target +
one trivial passing test + the optional deliberately failing self-test + license files.

## Prerequisites

None (first implementation session).

## Deliverables

- `LICENSE` — Apache-2.0 text (decision S1d; owner confirms before first release).
- `NOTICE.md` — licensing notes: Exiv2 is GPL-2.0+ (statically distributing the Exiv2 backend
  makes the combined work GPL-governed); ExifTool is invoked out-of-process only and never bundled.
- `.gitignore` — `build/`, `.cache/`, editor junk.
- `CMakeLists.txt` (root):
  - `cmake_minimum_required(VERSION 3.24)`, project `libumm`, `CXX` only, C++20 required,
    no compiler extensions.
  - Options: `UMM_BUILD_TESTS` (default `ON`), `UMM_ENABLE_FAILING_SELFTEST` (default `OFF`),
    placeholders `UMM_REQUIRE_EXIV2` / `UMM_REQUIRE_EXIFTOOL` (default `OFF`, no effect yet).
  - `BUILD_SHARED_LIBS` default OFF (static default, decision M4c).
  - Library target `umm` (alias `umm::umm`) with `include/` as public include dir and a single
    placeholder source `src/version.cpp` implementing `umm::version()` returning the project
    version string. **Do not compile the design-draft headers' declarations yet** — only
    `include/umm/version.hpp` graduates from draft status this session.
  - `enable_testing()`; add `tests/unit/`.
- `CMakePresets.json`: configure `default` (Ninja, RelWithDebInfo, tests on) and `debug`
  (inherits, Debug); build presets matching; test preset `default` with
  `"noTestsAction": "error"` and output-on-failure.
- `tests/unit/test_version.cpp` — asserts `umm::version()` is non-empty and matches the CMake
  project version (pass it via compile definition). Use CTest with plain asserts or doctest
  single-header **only if** vendoring a single header is acceptable; otherwise plain `assert` +
  `return` codes. Do not add a test framework dependency this session.
- `tests/unit/test_failing_selftest.cpp` — compiled and registered only when
  `UMM_ENABLE_FAILING_SELFTEST=ON`; always fails with a recognizable message.
- `include/umm/version.hpp` — remove the DESIGN DRAFT banner (it becomes real this session).

## Steps

1. Add LICENSE, NOTICE.md, .gitignore.
2. Write root CMakeLists.txt and CMakePresets.json exactly per build-plan.md §10.
3. Add `src/version.cpp`, promote `include/umm/version.hpp`.
4. Add the two tests; wire with `add_test`.
5. `cmake --preset default && cmake --build --preset default && ctest --preset default` — green.
6. Configure once with `-DUMM_ENABLE_FAILING_SELFTEST=ON` and confirm ctest goes red, then back.

## Acceptance criteria

- Fresh clone → three preset commands → 1 test passes.
- Failing self-test flag produces exactly one failing test.
- No network access needed to configure/build (backends not wired yet).
- Design-draft headers other than `version.hpp` are untouched and still not compiled.

## Cut line

If time runs out: drop the failing self-test to session 02; never drop the passing test.

## Out of scope

Backends, pins, CI, any metadata API implementation.

## References

build-plan.md §8, §10, §14 step 2; analysis decisions S1d, M4c, M7.
