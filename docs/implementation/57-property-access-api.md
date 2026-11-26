# Session 57 — Per-property access API and reason codes

## Goal

Tell an editor whether each canonical property can be written to a given file under a
policy with the backends available now, and why not (U4).

## Depends on

None.

## Governing decisions

F5. M2 (capability-driven, never silent), concept.md §12 (storage policy).

## Scope

**In:**
- **Header first** (`include/umm/capabilities.hpp`): `AccessReason` (with `to_string`),
  `PropertyAccess`, `propertyAccess(path, id, StoragePolicy)`,
  `propertyAccessAll(path, StoragePolicy)`. Document that reason strings are stable within
  a minor version and new reasons are appended.
- Implementation reuses the `write(..., dry_run = true)` path: storage evaluation
  (`evaluateStorage`) plus the write-sync mapping (`src/core/write_sync.cpp`) for a
  synthetic single-property change. Factor a shared internal function if needed; do not
  add a second mapping table.
- Read-only access: report `read` with `property_read_only` for one-way cast targets and
  (after session 65) `technical.*`; report `none` with `no_mapping_for_type` when the type
  has no representation of the property.
- Unit tests for each reason, including `backend_unavailable` with ExifTool absent (reuse
  `tests/backend/test_exiftool_unavailable.cpp` setup) and each `StoragePolicy`.

**Out:**
- The full type × property × policy agreement matrix (session 58).

## Documentation

- `docs/user/guide.md`: "Enabling editor controls" with the reason table.
- `docs/developer/release-notes.md`: new API.

## Exit criteria

- Every `AccessReason` value is produced by at least one test.
- `propertyAccessAll` returns one row per registry id for JPEG, MP4, and an XMP sidecar.
