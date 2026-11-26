# Session 65 — Technical view: photo and video values

## Goal

Fill `Metadata::technical()` for JPEG, HEIC, the main RAW formats, and MP4/MOV, with a
source key for each value and a defined absent state (U2 acceptance).

## Depends on

Sessions 63 (video technical values) and 64 (contract and source table).

## Governing decisions

F11, F10.

## Scope

**In:**
- Adapters: Exiv2 fills `intrinsics` from `Image::pixelWidth()` / `pixelHeight()`;
  ExifTool requests only the intrinsic `File:` tags the source table lists.
- Core fills the view from the generated source table after reconcile. Video fields come
  from canonical `recordingDevice` and the `videoTechnical` values (session 63), applied
  in memory for the view even when `ReadOptions::upcast` does not request them.
- Orientation: EXIF `Orientation` 1–8 for photos; QuickTime rotation degrees for video.
  Pixel size: intrinsic first, then EXIF `PixelXDimension`/`PixelYDimension`.
- Tests: JPEG, DNG, and MP4/MOV Tier A fixtures; HEIC and CR3 in the Tier B job; both
  backends; absent fields are `std::nullopt`; each value's source key is the expected one.

**Out:**
- Exposure, focal length, and other technical facts (not requested; stay in
  `dumpUnmapped()`).

## Documentation

- `docs/user/guide.md`: "Technical information" with the source table summary.
- `tests/verification/doc-audit.json`: claims mapped to tests.

## Exit criteria

- Typed values for the listed types on both backends where the backend can read them;
  gaps recorded in `tests/verification/ledger.json`.
