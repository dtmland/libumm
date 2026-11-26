# Session 76 — Read mappings and sidecars for BMP, MKV, AVI, WAV, ASF

## Goal

Give BMP, MKV, AVI, WAV, and ASF canonical reads and sidecar-based editing (U6).

## Depends on

Session 64 (Exiv2 video decision, OQ-F2).

## Governing decisions

F15 (U6). C7 (representations versus casts), S2, M2.

## Scope

**In:**
- `registry/capabilities/`: per-type read access for each backend (ExifTool reads; Exiv2
  per the session 64 decision), no embedded write, `sidecar_recommended = true`.
  Regenerate `docs/supported-types.md`.
- Media domain: BMP photo; AVI, MKV, ASF, and WAV video (`src/core/media_domain.hpp`).
  WAV uses the VMH audio fields (OQ-F3). The domain does not enable embedded writes: no
  backend writes WAV, so WAV edits stay sidecar-only like the others.
- Representations only where a standard defines them (embedded XMP in RIFF `_PMX` and ASF
  XMP). RIFF `INFO` and Matroska tags are cast sources; add cast rules only if a cited
  mapping exists, otherwise leave them in `dumpUnmapped()`.
- Fixtures: tiny generated files per type (ffmpeg is already installed in CI) with a
  sidecar; MANIFEST.
- Tests: read, `write` with `preferred` produces a sidecar, `embedded_only` fails with
  `unsupported_capability` and `propertyAccess` reports `container_read_only`.

**Out:**
- Embedded writes (neither backend supports them).

## Documentation

- `docs/user/guide.md`: editing read-only containers through sidecars.

## Exit criteria

- Each type reads, and edits through a sidecar, on all three CI OSes.
