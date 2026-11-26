# Session 73 — Payload digest

## Goal

A stable digest of the media essence that ignores metadata edits, for duplicate detection
(U14).

## Depends on

None. OQ-F4 (in-repo SHA-256) is approved.

## Governing decisions

F15 (U14) and the decision record §6 (payload-hash research). Dependency rule in `.github/copilot-instructions.md` (justify before adding).

## Scope

**In:**
- **Header first:** `include/umm/digest.hpp` with `PayloadDigest { scheme; hex; }` and
  `payloadDigest(path)`; include from `umm.hpp`. Backend-free and thread-safe.
- In-repo SHA-256 (`src/core/sha256.cpp`) with FIPS 180-4 test vectors.
- **Byte-range definition first (≤ 15 min):** read ExifTool's `ImageDataHash`
  implementation for JPEG, PNG, WebP, and MP4/MOV (in `exiftool/exiftool`; ExifTool's
  `Extra.html` describes it). For each type, write down which bytes it hashes. Use
  scheme `exiftool-imagedatahash-sha256` where libumm hashes the same bytes, otherwise
  `umm-payload-v1-sha256` with the reason recorded in this doc.
- Container walkers (bounded, defensive against truncated input; `format_corrupt` on bad
  structure) for JPEG, PNG, WebP, and MP4/MOV (64-bit box sizes), hashing the ranges
  fixed above. Thumbnails and previews are excluded, as in ExifTool. Other types:
  `unsupported_capability`.
- Cross-check test: when `exiftool` is on `PATH`, compare `payloadDigest` with
  `exiftool -api ImageHashType=SHA256 -ImageDataHash` for every Tier A fixture whose type
  uses an `exiftool-*` scheme. Skip (not fail) when ExifTool is absent; the `tier-b` CI
  job has it.
- Tests: for each supported Tier A fixture, digest unchanged after `umm::write` with both
  backends (title, keywords, GPS, sidecar-free) and after an ExifTool metadata-only edit;
  digest changes when one pixel/sample byte changes.

**Out:**
- TIFF, DNG, RAW, HEIC/AVIF (strip/tile and item-location walking): later, per type.
- Reading or writing `XMP-et:OriginalImageHash` (ExifTool's private namespace; F15).
- Comparison API (session 74).

## Documentation

- `docs/user/guide.md`: "Duplicate detection" with the per-type definition and scheme
  versioning rule (a definition change bumps the scheme string).

## Exit criteria

- Digest stable across every libumm write on supported types; changes on essence edits.
- `exiftool-*` scheme digests equal ExifTool's `ImageDataHash` (SHA256) on `tier-b`.
