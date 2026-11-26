# Session 64 — Technical view: header, source table, codegen

## Goal

Define the read-only, non-canonical technical view (U2) and its data-driven source table,
without filling values yet.

## Depends on

None.

## Governing decisions

F11. C17 (no EXIF canonical domain), S2 (registry-first), the contract test forbidding
hand-written tag names in `src/` outside `src/generated/`.

## Scope

**In:**
- **Header first** (`include/umm/metadata.hpp`): `TechnicalValue<T>` (value + `BaseKey`
  source), `Orientation` (EXIF 1–8 or rotation degrees + mirror), `PixelSize`,
  `TechnicalInfo` (`cameraMake`, `cameraModel`, `lensModel`, `orientation`, `pixelSize`),
  `Metadata::technical()`. Read-only: no setters; `umm::write` ignores it.
- `BaseDocument::intrinsics` in `backend.hpp` for container header facts that are not
  stored metadata (pixel dimensions); `dumpAll()` stays unchanged.
- `registry/technical/`: schema and JSON source table (priority list per field and media
  domain, with citations: EXIF 2.32/CIPA, ExifTool tag docs). Document the schema in
  `registry/schema.md`.
- Codegen in `tools/registry/` producing `src/generated/technical_sources.hpp`; build
  contract tests in `tests/build/` (reproducible, LF-pinned).
- Pseudo-ids `technical.cameraMake` … registered for `ReadOptions::properties` (F7) and
  reported by `propertyAccess` as `property_read_only`.

**Out:**
- Filling values (session 65).

## Documentation

- `docs/developer/canonical-model.md`: technical view versus canonical properties (C17).
- `docs/developer/release-notes.md`: layout change to `Metadata` and `BaseDocument`.

## Exit criteria

- Generated table byte-for-byte reproducible; existing tests green.
