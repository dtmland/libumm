# Session 27 — Tier B corpus infrastructure

Stage 10 · Estimated 45–60 min

## Goal

Implement the Tier B checksummed-download corpus from test-media-plan §3: manifest, fetcher,
fail-closed verification, and the optional CI job — and use it to close the two long-standing
deferrals (vendor MakerNote and proprietary RAW; analysis decision **R4**).

## Prerequisites

Stage 9 complete. (Tier B infrastructure itself only depends on the build/test scaffolding.)

## Deliverables

- `tests/corpus/manifest.json` + schema doc: per sample — URL, SHA-256, byte size, file type,
  license note, and the test capability it exists to verify. Initial entries (all fetched, never
  committed): a camera-authored JPEG with a vendor MakerNote (closes the `jpeg/makernote.jpg`
  deferral and activates the conditional M3 preservation test), at least one proprietary RAW
  (e.g. RAF or RW2 class — closes the session 19 deferral), and one real-camera MP4/MOV.
  Sources per test-media-plan §3 (files referenced in place from the exiftool/exiv2 repositories,
  or CC0 samples in a `dtmland` fixtures repository).
- Fetcher: a Python tool under `tools/` (pattern-matched to the existing pinned-download logic
  used for backend acquisition) downloading into `.cache/corpus/`, **fail-closed** on checksum or
  size mismatch, idempotent, offline-safe (clear skip status when the corpus is not requested).
- CMake/CTest wiring: Tier B tests registered behind an opt-in flag (e.g. `UMM_TIER_B=ON`),
  skipped-with-visible-status otherwise; the existing conditional makernote test picks up the
  fetched sample via the fixture-path mechanism.
- CI: an optional workflow job (workflow_dispatch and/or scheduled) running the Tier B suite on
  Linux at minimum, caching `.cache/corpus/` by manifest hash; the default PR matrix is
  unaffected. Workflow contract tests extended.
- MANIFEST/deferral bookkeeping: `tests/fixtures/MANIFEST.md` deferral notes updated to point at
  the Tier B manifest entries that resolve them.

## Steps

1. Manifest schema + initial entries (verify licenses and stable URLs before pinning).
2. Fetcher + fail-closed tests (checksum mismatch, truncated download, offline skip).
3. CTest wiring + CI job + contract tests; push; default matrix green, Tier B job green on demand.

## Acceptance criteria

- Tier B suite runs green on demand with the initial manifest; checksum tampering fails closed.
- The makernote write-preservation test executes (not skipped) in the Tier B job.
- Zero third-party media bytes committed to the repository.

## Cut line

The scheduled-run trigger and the video sample may defer; the fetcher, fail-closed behavior, and
the makernote entry may not.

## Out of scope

The cross-backend comparison suite itself (session 28); large-corpus growth (separate fixtures
repo per test-media-plan §3).

## References

docs/test-media-plan.md §3; analysis 2026-09-28 finding R4; decisions M3, M6;
tests/fixtures/MANIFEST.md deferral list.
