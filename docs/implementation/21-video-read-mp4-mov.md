# Session 21 — MP4/MOV read (ExifTool-primary)

Stage 7 · Estimated 45–60 min

## Goal

Read MP4/MOV metadata into the canonical model: video fixtures, QuickTime key translation, and
reconciliation for the `iptc.video.*` domain. ExifTool is the primary backend; Exiv2's
rudimentary video read is a supplement only (analysis decision **R2**).

## Prerequisites

Session 20 merged.

## Deliverables

- **ffmpeg as a pinned generator/CI provisioning dependency** (analysis decision R5): version
  pinned in the pins mechanism, provisioned in CI package steps for all three OSes, covered by a
  build-contract test — generator-time only; committed fixtures keep test-time independence.
- Fixtures: `video/` subdirectory — `minimal.mp4`, `minimal.mov`, `full.mp4` (QuickTime keys +
  XMP written by the pinned ExifTool), `gps.mp4` (`QuickTime:GPSCoordinates` + XMP GPS),
  `conflicting.mp4` (QuickTime vs XMP date disagreement); tiny (`-f lavfi -i color= -t 0.1`),
  within the <5 MB video budget from test-media-plan §5; MANIFEST updated.
- Key translation: QuickTime group mappings added to the central table in
  `src/backends/exiftool/keys.cpp` for the tags the Phase's VMH properties need (creation date,
  title/description-class keys, `GPSCoordinates`, duration-class technical keys), replacing the
  current unmapped `ExifTool.QuickTime.*` fallback for those tags; everything unmapped remains
  raw-accessible (concept.md §18).
- Reconciliation: `iptc.video.*` properties reconciled from QuickTime-container and XMP sources;
  precedence rules added to `docs/reconciliation-policy.md` (video section — XMP vs container
  keys, with QuickTime creation-time timezone caveats documented). **R3 revisit:** keep the
  per-property `if` dispatch (recorded in the policy doc). A second domain did not warrant a
  table-driven engine; video is a closed Phase-1-sized set.
- Exiv2 supplement: where the Exiv2 build reads video keys, they join reconciliation as
  additional sources; absence is fine (capabilities already say `none`).
- Capability probe extended to MP4/MOV (ExifTool r/w rows; Exiv2 none/rudimentary rows).

## Steps

1. ffmpeg pin + provisioning + contract test.
2. Fixtures + MANIFEST; offline tests green.
3. Key mappings + reconciliation + policy doc; read tests; probes; push; three-OS green.

## Acceptance criteria

- `umm::read` on `video/full.mp4` returns canonical `iptc.video.*` properties with provenance;
  `conflicting.mp4` classifies per the written policy.
- GPS from `QuickTime:GPSCoordinates` surfaces through the location property path.
- Unmapped QuickTime tags remain accessible via raw access.

## Cut line

The Exiv2 read supplement may defer (capabilities mark it rudimentary); ExifTool read, fixtures,
and the policy-doc video section may not.

## Out of scope

Video write (session 22); MKV/WebM/AVI (no location write in either backend — post-Stage-10
follow-up if demanded); frame/track-level technical metadata beyond registry-mapped properties.

## References

Analysis 2026-09-28 findings R2, R3, R5; supported-types.md §1 (video), §3 (MOV/MP4 location
rows); docs/test-media-plan.md §2.3, §5; concept.md §8, §27.
