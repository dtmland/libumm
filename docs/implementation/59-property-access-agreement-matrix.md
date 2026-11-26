# Session 59 — Per-property access agreement matrix

## Goal

Prove that `propertyAccess` agrees with `write(..., dry_run = true)` for every supported
type, property, and policy (U4 acceptance).

## Depends on

Session 58.

## Governing decisions

F5. C15 (prove every claim per backend).

## Scope

**In:**
- `tests/backend/test_property_access_matrix.cpp`: for each routed writable or
  sidecar-capable type in `registry/capabilities/policy.json` with a Tier A fixture, each
  canonical id, each `StoragePolicy`, and each backend configuration (both, Exiv2 only,
  ExifTool only): `propertyAccess(...).access == read_write` exactly when a dry-run write
  of a sample value for that id succeeds and lists at least one written key.
- Sample values per datatype come from one table in the test (reuse existing fixture
  helpers where possible).
- Record any disagreement found as a bug fix in this session, or in
  `tests/verification/ledger.json` with a reason if it is a backend divergence.

**Out:**
- Types without a Tier A fixture (Tier B types run in the existing `tier-b` job only if
  cheap; otherwise list them as untested).

## Documentation

- `tests/verification/doc-audit.json`: map the guide's U4 claims to this test.

## Exit criteria

- Matrix green on all three CI OSes.
