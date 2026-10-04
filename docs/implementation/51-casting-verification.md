# Session 51 — Verification with real-device layouts

## Goal

Prove every representation and cast rule against each backend that claims it (C15), using
Tier A fixtures that reproduce the tag layouts from the real-device sample (C19, OQ-R1),
and confirm that docs claim nothing the tests do not cover.

## Depends on

Session 48. Session 50 should land first so the doc audit covers the generated reference.

## Governing decisions

Decision record C15, C19, OQ-R1 (§8).

## Scope

**In:**
- **Layout fixtures** (Tier A, generated with `tests/fixtures/generator/`, synthetic values,
  tiny files) reproducing [docs/sample-output.txt](../sample-output.txt):
  - iPhone-style MOV: Keys `GPSCoordinates` with altitude, Keys `CreationDate` with offset,
    Keys `Make`/`Model`, movie-header `CreateDate` years later than the Keys date;
  - iPhone-style HEIC: EXIF `DateTimeOriginal` + sub-seconds + offset; GPS IFD with
    `GPSImgDirection`, `GPSSpeed`, `GPSHPositioningError` (BMFF write limits apply; use the
    existing HEIC Tier B pattern if a tiny HEIC cannot be generated);
  - Pixel-style DNG: IFD0 `DateTimeOriginal`, IIM `TimeCreated` with an offset;
  - Pixel-style JPEG: EXIF GPS and top-level `exif:GPS*` differing within tolerance;
    `photoshop:DateCreated` with 4-digit fraction versus EXIF 6-digit sub-seconds;
  - GoPro-style MP4: no Keys, no XMP, wrong movie-header date, GoPro `Model` and serial.
  Add them to `tests/fixtures/MANIFEST.md`. The original media files are not committed.
- **Matrix suite:** for each representation and cast rule, write canonical → read base,
  write base → read canonical, cast dry run and apply, on each backend that claims the
  capability. Extend the session 28 verification pattern; record gaps in
  `tests/verification/ledger.json`.
- **Doc audit:** every behavior claim in `docs/user/guide.md`, the generated reference, and
  `docs/reconciliation-policy.md` maps to a test, or is marked as untested.

**Out:**
- H21 capture-time sanity check (deferred).
- Later rule sets (§4 "later"); each future set brings its own tests under the same matrix.

## Documentation

- `docs/test-media-plan.md`: real-device layout fixtures and the redaction rule.
- `docs/developer/implementation-history.md`: add the stage row for sessions 43–51.
- Decision record: check off §5.1.

## Exit criteria

- Matrix suite green on all three CI OSes with both backends installed.
- Each C19 layout reads as the decision record predicts (for example the iPhone-style MOV
  `dateCreated` comes from Keys, not the movie header).
