# Session 30 — Versioning and API/ABI stability policy

Stage 11 · Estimated 30–45 min

## Goal

Declare the interface contract the project will stick to (analysis 2026-09-29 decision **P4**):
compile-time version macros, a written semver/ABI policy, and drift tests that keep the version
reported by CMake, the headers, and `umm::version()` identical.

## Prerequisites

Session 29 (the ConfigVersion compatibility mode must match the policy written here).

## Deliverables

- `include/umm/version.hpp` gains `UMM_VERSION_MAJOR` / `UMM_VERSION_MINOR` / `UMM_VERSION_PATCH`
  and a `UMM_VERSION_STRING` macro, generated or asserted against `PROJECT_VERSION` at configure
  time (header first, then wiring — the header remains normative).
- `docs/abi-policy.md`, stating: (1) the **source API** follows semver — pre-1.0, minor versions
  may break API and each break is release-noted; from 1.0, breaking changes bump major;
  (2) **C++ ABI stability is not promised** across releases in any linkage mode — public headers
  use std:: types by design (decision M1) and consumers must rebuild against each release; static
  linkage is the supported default consumption mode (M4c); (3) `SOVERSION` policy for the shared
  build: bumps with every minor release pre-1.0, with major from 1.0; (4) the C ABI remains
  deferred-but-not-precluded (M1, reaffirmed P5) and what would trigger revisiting it;
  (5) the standards-version reporting duty stays with `Registry::standards()` (concept.md §21) —
  library semver never encodes standards versions.
- Contract tests: header macros ↔ `PROJECT_VERSION` ↔ `umm::version()` agreement; SOVERSION
  matches the documented rule; `ummConfigVersion.cmake` compatibility mode matches the policy.
- README "versioning" pointer to the policy doc.

## Steps

1. Write `docs/abi-policy.md`; adjust session 29's ConfigVersion mode if it disagrees.
2. Add macros to `version.hpp` + configure-time consistency wiring.
3. Add contract tests; green on all OSes.

## Acceptance criteria

- A consumer can compile-time-gate on `UMM_VERSION_*`.
- Tampering with any one version location fails a test.
- The policy doc answers "is the ABI stable?" and "what does a version bump mean?" without
  reading code.

## Cut line

README polish may defer; macros + policy doc + agreement tests may not.

## Out of scope

Any PIMPL/inline-namespace retrofit (rejected, P4); C ABI design (deferred, P5); release notes
tooling (session 34).

## References

analysis 2026-09-29 §5 (P4, P5); decisions M1, M4c; concept.md §21.
