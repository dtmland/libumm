# Session 72 — Payload digest

## Goal

A stable digest of the media essence that ignores metadata edits, for duplicate detection
(U14).

## Depends on

None. Requires OQ-F4 (in-repo SHA-256) to be accepted.

## Governing decisions

F15 (U14). Dependency rule in `.github/copilot-instructions.md` (justify before adding).

## Scope

**In:**
- **Header first:** `include/umm/digest.hpp` with `PayloadDigest { scheme; hex; }` and
  `payloadDigest(path)`; include from `umm.hpp`. Backend-free and thread-safe.
- In-repo SHA-256 (`src/core/sha256.cpp`) with FIPS 180-4 test vectors.
- Container walkers (bounded, defensive against truncated input; `format_corrupt` on bad
  structure): JPEG (skip APPn and COM), PNG (critical chunks + `tRNS`), WebP (image chunks
  only), MP4/MOV (every `mdat` payload in order, 64-bit sizes). Other types:
  `unsupported_capability`.
- Tests: for each supported Tier A fixture, digest unchanged after `umm::write` with both
  backends (title, keywords, GPS, sidecar-free) and after an ExifTool metadata-only edit;
  digest changes when one pixel/sample byte changes.

**Out:**
- TIFF, DNG, RAW, HEIC/AVIF (strip/tile and item-location walking): later, per type.

## Documentation

- `docs/user/guide.md`: "Duplicate detection" with the per-type definition and scheme
  versioning rule (a definition change bumps the scheme string).

## Exit criteria

- Digest stable across every libumm write on supported types; changes on essence edits.
