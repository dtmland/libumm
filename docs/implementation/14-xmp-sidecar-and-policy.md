# Session 14 — XMP sidecar and storage policy (minimal)

Stage 4 · Estimated 45 min

## Goal

Complete Phase 1 scope (decision S3): XMP sidecars as first-class metadata carriers, plus the
minimal storage-policy engine that decides embedded vs sidecar (concept.md §12).

## Prerequisites

Sessions 12–13 merged.

## Deliverables

- Asset pairing: `media.jpg` + `media.xmp` recognized as one asset (concept.md §28); discovery
  rule (same stem, `.xmp` extension, case rules per OS) documented in the header.
- Sidecar read: sidecar raw entries merged into reconciliation as an additional source with a
  documented precedence rule vs embedded XMP (policy doc updated — sidecar-newer vs
  embedded-newer is exactly the conflict class the fixtures encode).
- Sidecar write: Exiv2 (native XMP sidecar support) and ExifTool (`-o file.xmp`, create-capable)
  both able to produce/update a sidecar through the same atomic temp-rename path.
- `StoragePolicy` minimal engine per concept.md §12: `Preferred | EmbeddedOnly | SidecarOnly |
  SidecarRequired`; evaluation returns `StorageDecision { method, formats, backend }`; for JPEG
  the interesting outcomes are Embedded(XMP+EXIF+IPTC) and Sidecar(XMP).
- Public API promotion: `umm::open(path)`/asset handle if the drafts specify it, `write(...,
  policy)` overload; header-first as always.
- Tests:
  - `sidecar/paired.*` fixtures: read merges; conflict variant classified per policy.
  - `sidecar/orphan.xmp`: readable standalone.
  - Write with `SidecarOnly` leaves the JPEG byte-identical (reuses M3 test 1 machinery).
  - Policy engine unit tests: decision table for JPEG × each policy value.

## Steps

1. Update reconciliation-policy.md with sidecar precedence rules.
2. Pairing + sidecar read merge; sidecar write; policy engine.
3. Fixture + unit tests both backends; push; three-OS green (path-encoding sidecar pairing on
   Windows is the thing to watch).

## Acceptance criteria

- Phase 1 complete: JPEG + XMP sidecar read/write/round-trip via both backends with
  reconciliation, provenance, write safety, and policy-driven storage decisions.
- Sidecar-vs-embedded conflicts surface with full provenance, never silently merged.

## Cut line

`SidecarRequired` semantics can defer to Stage 8 (synchronization) if needed; `Preferred` and
`SidecarOnly` may not.

## Out of scope

`detectConflict()`/`merge()`/`synchronize()` bulk operations (Stage 8); other formats.

## References

concept.md §11, §12, §28; analysis decision S3; supported-types.md §2 (XMP sidecar row).
