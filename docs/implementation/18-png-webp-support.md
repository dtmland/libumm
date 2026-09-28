# Session 18 — PNG and WebP support (capability-divergent pair)

Stage 6 · Estimated 45–60 min

## Goal

The first capability-divergent formats: PNG (no EXIF in Exiv2; ExifTool can read/write EXIF-in-PNG)
and WebP (no IPTC in Exiv2). These exercise per-category, per-backend gating that TIFF could not
(analysis decision **R6**; supported-types.md §3).

## Prerequisites

Session 17 merged.

## Deliverables

- Fixtures: `png/` and `webp/` subdirectories — per format at minimum `minimal`, `xmp-only`,
  `full-agreeing` (with only the categories the format supports per backend), plus `png/gps.png`
  carrying XMP GPS *and* ExifTool-written EXIF GPS (the divergence case). Generator + MANIFEST
  updates as in session 17.
- Sniffing: PNG (`\x89PNG`) and WebP (`RIFF....WEBP`) magic bytes added to
  `src/capabilities.cpp`; extensions confirmed against `policy.json`.
- Read: both backends where capable; verify the reconciliation output when one backend simply
  cannot see a family (e.g. Exiv2 on PNG EXIF) — the canonical result must still be correct and
  provenance must attribute the family to the backend that produced it.
- Write: capability-driven decisions per format — writing a property whose only embedded home is
  a category the selected backend lacks must either route to the capable backend or fall back to
  sidecar/`unsupported_capability` per policy, never silently drop (write-sync report lists what
  was written).
- Capability probes extended to PNG and WebP for both backends, locking in the divergence rows.

## Steps

1. Fixtures + sniffing; offline contract tests green.
2. Read tests including the one-backend-blind cases.
3. Write decision tests (category-divergent routing) + round-trips; probes; push; three-OS green.

## Acceptance criteria

- PNG: EXIF GPS written via ExifTool is readable; Exiv2 path still reads IPTC/XMP; `capabilities()`
  reports the asymmetry exactly as the registry data states.
- WebP: XMP+EXIF round-trip; IPTC correctly reported unsupported for Exiv2; no silent drops.
- Write reports (`WriteReport::written`) enumerate exactly the representations updated.

## Cut line

WebP may defer to a follow-up session if PNG's divergent-write routing takes the session budget;
PNG may not be cut.

## Out of scope

RAW (session 19); ICC/thumbnail categories beyond what existing tests cover.

## References

Analysis 2026-09-28 finding R6; supported-types.md §1–§3 (PNG/WebP rows);
registry/capabilities/schema.md derivation rules.
