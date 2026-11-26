# Session 77 — v0.3.0 verification, doc audit, and release

## Goal

Close feature request 1: every acceptance criterion maps to a test, the docs claim nothing
untested, and the history and release notes are current.

## Depends on

Sessions 60–76 that the maintainer chose to ship. Session 59 (v0.2.0) is done.

## Governing decisions

F1–F16. C15 (prove claims per backend).

## Scope

**In:**
- Walk each U1–U14 acceptance criterion in
  [docs/feature-request-1.md](../feature-request-1.md) and map it to a test or a recorded
  gap (`tests/verification/ledger.json`, `doc-audit.json`).
- Doc audit of everything sessions 60–76 changed in `docs/user/`, `docs/sysadmin/`,
  `docs/reconciliation-policy.md`, and the generated references.
- `docs/developer/implementation-history.md`: stage row for sessions 60–77 and new
  standing constraints (F12 legacy write-sync rule).
- `docs/developer/release-notes.md` and version bump to 0.3.0 (`include/umm/version.hpp`, `CMakeLists.txt`,
  `SOVERSION` per `docs/abi-policy.md`).
- `docs/feature-request-1.md`: complete the status line per request (shipped / partial / not
  planned, with links).
- Decision record: mark each F decision implemented or amended.

**Out:**
- New features.

## Exit criteria

- CI green on all jobs (including `tsan` and `tier-b`); doc-audit test green.
