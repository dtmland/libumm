# Session 05 — Exiv2 acquisition and smoke test

Stage 1 · Estimated 45 min (Windows source build is the risk; rely on caching)

## Goal

Checksum-pinned Exiv2 built from source via FetchContent on all three OSes and linked into a smoke
test that calls a real Exiv2 API.

## Prerequisites

Sessions 01–04 merged.

## Deliverables

- `cmake/LibummExiv2.cmake`:
  - Version/SHA from `tools/build/backends.env` (same parsing helper as session 04 — factor it
    into `cmake/LibummPins.cmake` if not already).
  - `FetchContent_Declare` of the Exiv2 release source archive with `URL_HASH`; **decision M4a:
    pinned source on all OSes**; cache under `.cache/exiv2/`.
  - Exiv2 build options: `EXIV2_ENABLE_BMFF=ON` (CR3/HEIC/AVIF read matters to the plan),
    samples/tests/docs OFF, static lib to match M4c.
  - Link `umm` target → `exiv2lib` **privately** (Exiv2 types must not leak into the public API);
    `UMM_REQUIRE_EXIV2=ON` ⇒ missing/failed Exiv2 fails configure.
  - Append Exiv2 version + BMFF flag to `backends-acquired.txt`.
- Smoke test `tests/backend/test_exiv2_smoke.cpp`: calls `Exiv2::versionString()` (via a tiny
  internal `umm` shim so the test itself doesn't include Exiv2 headers), asserts equals pin.
- ci.yml: `-DUMM_REQUIRE_EXIV2=ON` everywhere; ccache/action cache tuned so the Windows Exiv2
  build is compiled once per pin bump.
- Contract test: workflow passes the flag; no Exiv2 version literals outside backends.env.

## Steps

1. Write LibummExiv2.cmake; local configure/build (expect the longest step here).
2. Shim + smoke test; ctest green.
3. Push; verify all three OSes; check cache hit on a re-run.

## Acceptance criteria

- `ctest -R exiv2_smoke` green on Linux, Windows (MSVC), macOS arm64.
- Second CI run hits the backend cache (visible in step timing).
- Public headers of `umm` do not include any `exiv2/*` header (checked by a contract test grep).

## Cut line

If the Windows source build exceeds runner tolerance even cached, open the sanctioned M4a fallback
(pinned prebuilt for Windows only) as a **new decision entry** in the analysis doc — do not decide
it ad hoc inside the session.

## Out of scope

Any read/write of media (session 10); BMFF write (never — Exiv2 is read-only there).

## References

build-plan.md §9.1, §14 step 6; analysis decisions M4a, M4c; supported-types.md §1 (BMFF note).
