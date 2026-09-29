# Session 32 — Shared-linkage Exiv2 option

Stage 11 · Estimated 45–60 min

## Goal

Add `UMM_EXIV2_SHARED` (default OFF): build/link the Exiv2 backend against a shared `exiv2`
library — either the FetchContent build produced as a shared lib or a system/consumer-provided
Exiv2 via `find_package(exiv2)` — for packaging and substitutability (analysis 2026-09-29
decision **P2**). Explicitly *not* a licensing device: the option's help text and docs must state
that GPL governs distribution of the combined work in either linkage mode.

## Prerequisites

Session 29 (install/export exists; this session extends it). Session 31 recommended (notices
wording is settled).

## Deliverables

- `UMM_EXIV2_SHARED` option in `cmake/LibummExiv2.cmake`: when ON, prefer
  `find_package(exiv2 CONFIG)` (system/consumer Exiv2, version floor = the pinned minor), else
  FetchContent with `BUILD_SHARED_LIBS=ON` for the Exiv2 subtree only. The existing
  install-rule shielding and the zlib/expat handling (see cmake comments) must keep working in
  both modes.
- Install/runtime handling for the shared mode: the shared `exiv2` library is either an external
  runtime requirement (system mode — recorded as a dependency of the exported target) or
  installed alongside (FetchContent-shared mode) with correct RPATH/`install_name`/DLL placement
  per OS.
- Consumer smoke test (session 29's) parameterized to run in the shared mode.
- One CI leg (Linux is sufficient) exercising `UMM_EXIV2_SHARED=ON`; contract tests updated.
- Docs: a short build-options section noting the option, the version-floor rule, and the P2
  licensing statement.

## Steps

1. Implement the option + system-Exiv2 path on Linux; verify Unicode-path behavior still passes
   (the MemIo workaround in the backend must not regress against a system Exiv2).
2. FetchContent-shared mode + install/runtime placement on all three OSes.
3. CI leg + contract tests; green.

## Acceptance criteria

- Static default build is byte-for-byte unaffected when the option is OFF.
- Shared mode passes the full test suite and the consumer smoke test on Linux CI.
- Option documentation carries the GPL-still-applies statement.

## Cut line

Windows/macOS shared-mode *install* verification may defer to session 34's release build; the
Linux shared leg and the no-regression-when-OFF guarantee may not.

## Out of scope

An Exiv2-less "core" build (P1 option 3, unscheduled); changing the pinned Exiv2 version.

## References

analysis 2026-09-29 §4.2 (P2); decisions M4a, M4c; cmake/LibummExiv2.cmake install-shielding
comments; src/backends/exiv2/ Unicode-path handling.
