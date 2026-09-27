# Session 10 — Backend adapter contract and Exiv2 read

Stage 4 · Estimated 45 min

## Goal

The backend interface (the contract both backends implement) plus the first real read path:
Exiv2 reading raw EXIF/IPTC/XMP entries from JPEG fixtures. Raw entries only — reconciliation is
session 12.

## Prerequisites

Sessions 05, 08, 09 merged.

## Deliverables

- Promote `include/umm/backend.hpp` (design draft → real): the adapter contract per analysis S1a/b:
  - `BackendId`, `availability()` (present/absent + reason — backends optional at runtime,
    decision S1b), `readRaw(path) -> Result<RawDocument>`,
    `writeRaw(path, RawChanges) -> Result<void>` (declared; implemented session 13),
    `typeCapabilities(MediaType)` (declared; implemented session 15).
  - `RawDocument`: ordered list of `RawEntry { family (Exif/Iptc/Xmp), key, value bytes/text,
    type hint }` — the family/key naming follows Exiv2 key syntax (`Exif.Image.Artist`,
    `Iptc.Application2.City`, `Xmp.dc.creator`) as the neutral raw vocabulary; the ExifTool
    adapter maps into it (documented in the header).
  - Error mapping rules, timeout semantics (relevant to session 11), thread-safety statement
    (each backend instance single-threaded; manager may pool).
- `src/backends/exiv2/` — Exiv2 backend implementing `availability()` + `readRaw()` for JPEG:
  iterate exifData/iptcData/xmpData into `RawEntry`s; Exiv2 exceptions caught at the boundary and
  mapped to `umm::Error` (M1: exceptions never escape).
- `BackendManager` minimal: register/enumerate backends, fetch by id.
- Tests (`tests/backend/test_exiv2_read.cpp`) against fixtures: exif-only yields only Exif keys;
  full-agreeing yields all three families with expected values (creator, dates, GPS from
  `gps.jpg`); unicode fixture values match UTF-8 expectations; truncated fixture returns
  `format` error, not a crash.

## Steps

1. Finalize backend.hpp draft (header first).
2. Implement Exiv2 readRaw + availability; map exceptions.
3. Tests against the fixture corpus; presets loop; push; three-OS green.

## Acceptance criteria

- All listed fixture read tests green on the three CI jobs.
- No Exiv2 type or header appears in any public header.
- Error paths return typed `umm::Error` (asserted for truncated.jpg).

## Cut line

GPS rational-to-decimal conversion may stay raw (string/rational passthrough) — conversion
belongs to the mapping layer in session 12 anyway.

## Out of scope

ExifTool adapter (session 11); semantic mapping/reconciliation (session 12); write (session 13).

## References

concept.md §13, §18; analysis decisions S1a, S1b, M1; supported-types.md §1.
