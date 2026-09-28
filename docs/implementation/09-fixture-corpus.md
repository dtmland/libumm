# Session 09 — Fixture generator and Phase 1 corpus

Stage 3 · Estimated 45 min

## Goal

The Tier A JPEG + XMP-sidecar fixture corpus from
[docs/test-media-plan.md](../test-media-plan.md) §2.2, generated deterministically with the pinned
backends, committed with a manifest.

## Prerequisites

Sessions 04–05 merged (pinned ExifTool + Exiv2 available as generation tools).

## Deliverables

- `tests/fixtures/generator/generate.py` — stdlib Python invoking the **pinned** ExifTool (and
  where useful the built Exiv2 `exiv2` tool) to produce every fixture in test-media-plan §2.2:
  minimal, exif-only, iptc-only, xmp-only, full-agreeing, full-conflicting, gps, unicode,
  makernote, unknown-tags, sidecar pair + orphan, non-ASCII filename, truncated-corrupt.
  - Base image: 16×16 gray JPEG produced by the generator (ImageMagick if present, else a
    checked-in byte-exact base) — one base, many metadata variants.
  - All embedded timestamps fixed constants; documented per-fixture tag sets inside the script.
- `tests/fixtures/MANIFEST.md` — per file: generator command, SHA-256, size, purpose (mirrors
  test-media-plan §2.2 table).
- `tests/build/test_fixtures.py` — manifest matches files on disk (names + SHA-256); every file
  ≤ 100 KB; total corpus < 1 MB; the non-ASCII filename exists and is correctly encoded in git.
- CMake: fixture directory path passed to tests via a compile definition / CTest property
  (Windows path safety, build-plan §12).

## Steps

1. Write generator; produce corpus locally with pinned tools; verify sizes.
2. Write MANIFEST.md (generator can emit it); add contract test.
3. Spot-check with `exiftool -j` that full-conflicting really conflicts (different dates per
   block) and unicode survives.
4. Commit fixtures + push; CI contract test green on all OSes (watch the Windows unicode
   filename job carefully).

## Acceptance criteria

- Corpus regenerable: rerunning the generator yields metadata-equivalent files (contract test
  compares parsed metadata, not necessarily bytes).
- All test-media-plan §2.2 fixtures present, manifest-verified, < 1 MB total.
- Unicode filename fixture round-trips checkout on Windows CI.
- Unicode *values* are written through an ExifTool UTF-8 argfile (`-charset utf8 -@`)
  so Windows Perl argv does not replace non-ASCII characters with `?`.

## Cut line

`makernote.jpg` may defer to Stage 4 if sourcing a synthetic MakerNote blob is fiddly — record it
as an open item in MANIFEST.md; do not substitute a third-party camera file (decision M6).

## Out of scope

Tier B download corpus (Stage 10); non-JPEG fixtures (Stage 6+).

## References

docs/test-media-plan.md §1–§2; analysis decision M6; build-plan.md §12.
