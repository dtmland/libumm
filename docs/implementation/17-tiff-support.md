# Session 17 — TIFF support

Stage 6 · Estimated 45 min

## Goal

First new still format: TIFF read/write/round-trip through both backends. TIFF has full
two-backend category parity (EXIF/IPTC/XMP read-write in both), so this session validates the
session 16 generalization on the easiest real case (analysis decision **R6**).

## Prerequisites

Session 16 merged.

## Deliverables

- Fixtures: `tiff/` subdirectory following the JPEG matrix pattern (test-media-plan §2.2) —
  at minimum `minimal.tif`, `full-agreeing.tif`, `full-conflicting.tif`, `gps.tif`,
  `unicode.tif`; generator additions in `tests/fixtures/generator/generate.py` (PURPOSES +
  generation steps via the pinned ExifTool); MANIFEST regenerated; corpus stays within
  test-media-plan size budgets.
- Type sniffing: TIFF magic bytes already exist in `src/capabilities.cpp` (`looks_like_tiff`);
  confirm extension mapping (`.tif`, `.tiff`) in `registry/capabilities/policy.json` and the
  sniffer agree.
- Read: both backends read the TIFF fixtures into the canonical model (reconciliation unchanged —
  it is source-family-based, not container-based).
- Write: `umm::write` on TIFF through the session 16 capability-driven decision; embedded
  XMP+EXIF+IPTC via preferred backend; sidecar policies work as for JPEG.
- Write-safety (M3) tests reused for TIFF: payload bytes unchanged on metadata-only write;
  atomic-rename path; non-ASCII values.
- Capability probe: extend the Stage 5 probe drift test to TIFF (both backends).

## Steps

1. Generator + fixtures + MANIFEST; contract tests green offline.
2. Read tests (both backends); write + round-trip tests; probe extension.
3. Push; three-OS green.

## Acceptance criteria

- TIFF round-trips (write with each backend, read back with each) with reconciliation and
  provenance identical in shape to JPEG.
- `capabilities()` answers for TIFF are fixture-verified by the probe test.
- No code change needed in the storage-decision path (proves session 16 did its job — if a
  dispatch change is needed, stop and amend session 16's approach first).

## Cut line

`unicode.tif` and the M3 non-ASCII value test may defer one session; minimal/full/gps fixtures
and the round-trip may not.

## Out of scope

PNG/WebP/RAW (sessions 18–19); BMFF types (post-Stage-10, decision R6).

## References

Analysis 2026-09-28 finding R6; supported-types.md §1, §3; docs/test-media-plan.md §2.2.
