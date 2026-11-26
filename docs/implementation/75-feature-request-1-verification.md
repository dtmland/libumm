# Session 75 — Verification, doc audit, and release notes

## Goal

Close feature request 1: every acceptance criterion maps to a test, the docs claim nothing
untested, and the history and release notes are current.

## Depends on

All sessions 52–74 that the maintainer chose to ship.

## Governing decisions

F1–F15. C15 (prove claims per backend).

## Scope

**In:**
- Walk each U1–U14 acceptance criterion in
  [docs/feature-request-1.md](../feature-request-1.md) and map it to a test or a recorded
  gap (`tests/verification/ledger.json`, `doc-audit.json`).
- Doc audit of everything sessions 52–74 changed in `docs/user/`, `docs/sysadmin/`,
  `docs/reconciliation-policy.md`, and the generated references.
- `docs/developer/implementation-history.md`: stage row for sessions 52–75 and new
  standing constraints (thread-safety contract, F12 legacy write-sync rule).
- `docs/developer/release-notes.md` and version bump per OQ-F6 and `docs/abi-policy.md`.
- `docs/feature-request-1.md`: add a status line per request (shipped / partial / not
  planned, with links).
- Decision record: mark F-series accepted or amended.

**Out:**
- New features.

## Exit criteria

- CI green on all jobs (including `tsan` and `tier-b`); doc-audit test green.
