# Session 22 — Video write and location (ExifTool-only)

Stage 7 · Estimated 45–60 min

## Goal

`umm::write` for MP4/MOV through ExifTool — the first single-backend write path — including
QuickTime `GPSCoordinates` location write, with the same write-safety guarantees as stills.

## Prerequisites

Session 21 merged.

## Deliverables

- Write-sync for the video domain: canonical `iptc.video.*` values expand to their registry-mapped
  QuickTime and XMP representations (inverse key translation via the session 21 mappings);
  `WriteReport::written` enumerates them.
- Backend routing: capability data already marks MOV/MP4 write as ExifTool-only; the session 16
  decision logic must select ExifTool without video-specific code. Requesting `backend: "exiv2"`
  explicitly for a video write fails with `unsupported_capability`. If ExifTool is unavailable at
  runtime, the write fails with the existing backend-unavailable error (decision S1b semantics).
- Location write: GPS position written as QuickTime `GPSCoordinates` plus XMP GPS per the policy
  doc; round-trip verified via read-back with ExifTool (and Exiv2 rudimentary read where it can
  see anything).
- Write safety (M3) for video: payload (media track bytes) unchanged on metadata-only writes;
  atomic temp+rename path reused (`mutate_file_atomically` is format-agnostic); non-ASCII values
  and filenames on all three OSes (the Windows UTF-8 argfile discipline already exists in the
  adapter).
- Sidecar interplay: `StoragePolicy::sidecar_only` on video produces an XMP sidecar exactly as
  for stills (no new code expected; test it).

## Steps

1. Header-first check (`umm.hpp` comments already generalized in session 16; amend if video
   exposes gaps).
2. Video write-sync + inverse mappings; dry-run report tests.
3. Round-trip + M3 + policy tests; push; three-OS green.

## Acceptance criteria

- Write-with-ExifTool / read-back round-trip preserves values and provenance shape for the
  registry-mapped video properties, including GPS.
- Media payload bytes are unchanged by metadata-only writes (byte-comparison of track data or
  full-file minus metadata atoms — document the comparison method chosen).
- Stage 7 exit: `capabilities()` answers for MOV/MP4 are probe-verified against real writes.

## Cut line

MOV may defer to MP4-only if the container-atom byte-comparison proves fiddly; MP4 write + GPS +
atomic safety may not.

## Out of scope

Cross-backend video verification (Stage 10); audio formats; MKV/WebM (no write path in either
backend).

## References

Analysis 2026-09-28 finding R2; supported-types.md §3 (MOV/MP4 `r/w GPSCoordinates/XMP` row);
decisions S1b, M3; session 13 write-safety machinery.
