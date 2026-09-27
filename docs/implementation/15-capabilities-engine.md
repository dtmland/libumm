# Session 15 — Capabilities engine and generated supported-types

Stage 5 · Estimated 45–60 min

## Goal

`capabilities(media)` answered from machine-readable capability data — per backend, per file type,
per metadata category, with **GPS and named place as separate location capabilities** — and
`supported-types.md` regenerated from that data with CI drift checks (decision M2).

## Prerequisites

Sessions 10–14 merged (both backends operational; registry tooling exists).

## Deliverables

- `registry/capabilities/` — machine-readable capability data transcribing
  [supported-types.md](../../supported-types.md) §1–§4:
  per (backend × file type): read/write/create per category (EXIF, IPTC-IIM, XMP, ICC, thumbnail)
  **plus location split** per §3: `gps_exif`, `named_place_iptc`, `xmp_location`,
  `container_gps`, `geotiff` — each read/write. Include the Exiv2 BMFF-build conditionality and
  the identify-only marker. Sources + retrieval dates recorded like the property registry.
- `tools/registry/generate_supported_types.py` — regenerates `supported-types.md` from the data
  (same narrative section order; tables generated). Committed-generated with a cleanliness
  contract test (same pattern as session 07).
- Capability engine: `umm::capabilities(path_or_type) -> Result<Capabilities>` implementing
  concept.md §14 — per-category read/write, location per encoding, sidecar recommendation,
  preferred backend (derivation rules from supported-types.md §3 "Derive …" paragraphs encoded as
  data or documented logic, not scattered ifs).
- **Backend probe drift test (Tier A):** for the types the built backends actually support in CI
  today (JPEG, XMP sidecar), probe the *pinned* backends against the capability data (e.g.
  attempt category read/write on fixtures; compare `exiv2 --version --verbose` feature flags /
  ExifTool `-listwf` membership for JPEG/XMP) so a pin bump that changes capability fails a test.
  Wider-type probes join as Stage 6 adds fixtures per type.
- Promote `include/umm/capabilities.hpp` from draft; `WriteOptions`/policy engine consult the
  capability engine (replace any session-14 hardcoding).

## Steps

1. Design the capability data schema; transcribe §1–§4 (largest chunk — consider generating the
   first cut from the markdown tables with a throwaway parse, then hand-verify).
2. Generator for supported-types.md; diff against the hand-written original until equivalent;
   commit generated version (markdown gains a "generated from registry/capabilities" banner).
3. Capability engine + JPEG/XMP probe tests; push; three-OS green.

## Acceptance criteria

- `capabilities()` for JPEG reports exactly what the fixture-verified reality is, per backend,
  with GPS vs named place separated (supported-types.md §6.9).
- Editing capability data regenerates supported-types.md deterministically; stale generated file
  fails CI.
- Pin-bump capability drift for JPEG/XMP is test-detectable.

## Cut line

Transcription of §4 ExifTool-only exotic types (fonts/archives/etc.) may land as a
`coverage: partial` data file completed in Stage 6 — Exiv2 types and all location tables may not
be cut.

## Out of scope

New file-type support; Tier B corpus.

## References

concept.md §14; supported-types.md (all); analysis decision M2.
