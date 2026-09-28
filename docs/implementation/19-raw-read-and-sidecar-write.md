# Session 19 — RAW read and the sidecar-write pattern

Stage 6 · Estimated 45–60 min

## Goal

Common RAW support as a *pattern*, not a per-vendor slog: DNG as the writable-RAW representative
(embedded read/write both backends), and the read-only-RAW pattern where `sidecar_recommended`
routes writes to the XMP sidecar path built in session 14 (analysis decisions **R4**, **R6**).

## Prerequisites

Session 18 merged.

## Deliverables

- Fixtures: `raw/minimal.dng` and `raw/full-agreeing.dng` generated with the pinned ExifTool
  (DNG is TIFF-based and ExifTool-writable); MANIFEST updated. **No proprietary RAW is committed**
  (M6): RAF/RW2/SR2-class verification is explicitly deferred to Tier B (session 27) and recorded
  as such in the MANIFEST deferral list alongside `jpeg/makernote.jpg`.
- Sniffing: DNG discrimination from plain TIFF (extension-first is acceptable; document the rule);
  `policy.json` extension rows confirmed.
- DNG read/write/round-trip through both backends, reusing the session 17 test shape; write-safety
  (M3) payload-unchanged test included — this is the highest-value format for byte-preservation
  guarantees.
- Read-only-RAW pattern: for a type whose capability row is read-only with
  `sidecar_recommended: true`, `StoragePolicy::preferred` must resolve to a sidecar decision and
  `embedded_only` must fail with `unsupported_capability`. Unit-tested against capability records
  (a real proprietary-RAW file is not required to test the *decision*; Tier B later verifies
  against real files).
- Policy doc touch-up: `docs/reconciliation-policy.md` sidecar section notes RAW-preferred-sidecar
  reads (sidecar XMP is same-tier as embedded XMP, unchanged from session 14).

## Steps

1. DNG fixtures + sniffing; contract tests green.
2. DNG read/write/round-trip + M3 preservation test.
3. Read-only-RAW decision tests; MANIFEST deferral notes; push; three-OS green.

## Acceptance criteria

- DNG round-trips with byte-identical image payload on metadata-only writes.
- `preferred` policy on a read-only-RAW capability row produces a sidecar write decision without
  any format-specific code (capability data alone drives it).
- Stage 6 exit: JPEG, TIFF, PNG, WebP, DNG all pass the capability probe drift tests.

## Cut line

The reconciliation-policy doc touch-up may defer; the read-only-RAW decision tests may not (they
are the pattern this session exists to prove).

## Out of scope

Proprietary RAW files (Tier B, session 27); BMFF RAW (CR3 — post-Stage-10 with the other BMFF
types); MakerNote synthesis.

## References

Analysis 2026-09-28 findings R4, R6; supported-types.md §1, §3 (RAW rows);
docs/test-media-plan.md §2.3; session 14 sidecar machinery.
