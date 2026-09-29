# Session 35 — BMFF enablement (HEIC/HEIF/AVIF/CR3/JXL)

Stage 12 · Estimated 45–60 min

## Goal

Bring the BMFF family from "compiled but unverified" to first-class supported types, closing
decision **R6**'s "planned after Stage 10". Exiv2 is already built with `EXIV2_ENABLE_BMFF=ON`
(session 05); Exiv2 is read-only for BMFF, so this follows the session 19 pattern: Exiv2 read +
ExifTool write, with `sidecar_recommended`/capability data driving write decisions per type. Also
absorbs the session 28 cut-line depth (round-trip stability + Tier B integration in the
comparison suite).

## Prerequisites

Stage 10 complete (Tier B fetcher, cross-backend suite). Independent of Stage 11; may run in
parallel with sessions 29–34.

## Deliverables

- Capability verification: `registry/capabilities/` rows for HEIC/HEIF/AVIF/CR3/JXL audited
  against the pinned backends (Exiv2 0.28.9 BMFF read; ExifTool 13.59 write coverage —
  ExifTool writes XMP/EXIF in HEIC/AVIF/CR3; JXL per its README status); the supported-types
  generator and drift probes extended so a pinned-backend mismatch fails a test (M2).
- Sniffing: `sniff_type` extended to detect the BMFF container (`ftyp` box) and classify by major
  brand (heic/heix/avif/crx /jxl signature variants), with unit tests; extension fallback remains
  for unrecognized brands.
- Fixtures: HEIC/AVIF where generatable small (ExifTool cannot create BMFF from scratch; evaluate
  ffmpeg — already pinned since session 21 — for tiny HEIC/AVIF stills; document due diligence).
  CR3 and any non-generatable types go to the Tier B manifest (real camera / CC0 samples,
  checksummed, per M6/R4 pattern).
- Read tests both backends where capable; write tests through the capability-driven path
  (expected outcome: ExifTool embedded writes for HEIC/AVIF/CR3; sidecar recommendation honored
  where capability data says so); M3 payload-preservation check on at least one BMFF write.
- Cross-backend suite: BMFF pairs added; new one-directional ledger entries (Exiv2 read-only)
  with reasons; round-trip stability check extended to run over Tier B samples in the Tier B job
  (session 28 cut-line closure).

## Steps

1. Capability audit + sniffing + unit tests.
2. Fixtures/Tier B manifest entries (verify licenses and stable URLs before pinning).
3. Read/write/cross-backend tests + ledger entries; Tier A matrix and Tier B job green.

## Acceptance criteria

- A HEIC file with XMP/EXIF reads identically-classified metadata through both backends where
  both are capable, and location writes land via ExifTool readable by Exiv2.
- Every BMFF capability claim is either exercised by a test or covered by a ledger entry with a
  reason.
- The session 28 round-trip stability check runs over the Tier B corpus in the Tier B job.

## Cut line

JXL and CR3 depth may defer to Tier B follow-up entries; HEIC/AVIF read + sniffing + capability
drift probes may not.

## Out of scope

Video-in-HEIF sequences; Exiv2 BMFF write (upstream does not support it); wide format expansion
beyond BMFF (unscheduled pending owner confirmation, analysis 2026-09-29 §6.3).

## References

analysis 2026-09-28 R6; analysis 2026-09-29 §6.3 (P7); cmake/LibummExiv2.cmake BMFF flag;
sessions 19, 27, 28; decisions M2, M3, M6.
