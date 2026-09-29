# Session 04 — ExifTool acquisition and smoke test

Stage 1 · Estimated 30–45 min

## Goal

Checksum-pinned ExifTool acquired at configure time on all three OSes, executed via a discovered
Perl, proven by a smoke test (`exiftool -ver` equals the pin).

## Prerequisites

Sessions 01–03 merged (pins exist; CI matrix green; Strawberry Perl step already in ci.yml).

## Deliverables

- `cmake/LibummExifTool.cmake`:
  - Reads `UMM_EXIFTOOL_VERSION` / `UMM_EXIFTOOL_SHA256` from `tools/build/backends.env`
    (parse the file from CMake — pins must not be duplicated in CMake code).
  - `FetchContent_Declare` of the ExifTool source archive
    (`https://exiftool.org/Image-ExifTool-<ver>.tar.gz` or the GitHub mirror) with `URL_HASH
    SHA256=...`; download into `.cache/exiftool/` so the CI cache applies.
  - `find_package(Perl REQUIRED)` when `UMM_REQUIRE_EXIFTOOL=ON`; otherwise optional with a
    clear status message.
  - Exposes `UMM_EXIFTOOL_SCRIPT` (path to the `exiftool` script inside the archive) and
    `UMM_PERL_EXECUTABLE`; writes both plus the version into
    `build/default/backends-acquired.txt` (append; create file this session).
  - `UMM_REQUIRE_EXIFTOOL=ON` + acquisition failure ⇒ **configure fails** (fail closed).
- Smoke test `tests/backend/test_exiftool_smoke.cmake`-driven CTest (or a tiny C++ test spawning
  the process): runs `<perl> <script> -ver`, asserts output matches the pinned version exactly.
  Test receives paths via compile definitions / CTest properties `UMM_TEST_EXIFTOOL_SCRIPT`,
  `UMM_TEST_EXIFTOOL_PERL` (build-plan §9.2).
- ci.yml: set `-DUMM_REQUIRE_EXIFTOOL=ON` on all matrix jobs; a step prints
  `backends-acquired.txt`. macOS: verify image Perl suffices, else pin a brew step.
- Contract tests updated: workflow passes the require flag; `LibummExifTool.cmake` references no
  literal version string (greps for digits — pins only from backends.env).

## Steps

1. Write the CMake module; configure/build/test locally (Linux or macOS dev machine).
2. Wire smoke test; run ctest.
3. Update ci.yml + contract tests; push; all three OS green with the smoke test listed.

## Acceptance criteria

- `ctest -R exiftool_smoke` passes on all three CI jobs.
- Breaking the SHA in backends.env fails configure (verified locally, then reverted).
- `backends-acquired.txt` shows ExifTool version + interpreter path in CI logs on every OS.

## Cut line

macOS brew-Perl fallback can be deferred if the runner image Perl passes the smoke test.

## Out of scope

The stay_open adapter (session 11); any metadata operations; end-user acquisition of ExifTool
outside CMake (session 33, decisions P3/P9 — native scripts, not this FetchContent path).

## References

build-plan.md §9.2, §14 step 5; analysis decisions S1a (deferred), S1c, M4b.
