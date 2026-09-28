# Session 20 — Video Metadata Hub registry import

Stage 7 · Estimated 45–60 min

## Goal

Registry-first for video, mirroring decision S2 and analysis decision **R2**: vendor the pinned
IPTC Video Metadata Hub machine-readable specification, import it into a `registry/iptc-video/`
domain, and generate the C++ property tables — before any video read/write code exists.

## Prerequisites

Stage 6 complete (sessions 16–19 merged). No video code prerequisites — this session is
data + tooling only.

## Deliverables

- Vendored source: the pinned VMH machine-readable properties (JSON, from IPTC's
  `iptc/video-metadata-hub` repository — the specification artifacts generated from IPTC's master
  sheet) under `registry/sources/` with version, URL, retrieval date, and checksum recorded per
  the M5 provenance pattern (VMH 1.7 unless a newer Recommendation is current at execution time).
- Importer: `tools/registry/import_vmh.py` following the session 06 importer pattern, producing
  `registry/iptc-video/iptc-video.json` conforming to `registry/schema.md` (extend the schema doc
  only where video requires it — e.g. VMH's per-technology mappings to XMP / EBUCore /
  QuickTime keys; document additions in the schema doc).
- Codegen: `tools/registry/generate_cpp.py` extended (or invoked for the new domain) so the
  generated tables in `src/generated/` include `iptc.video.*` properties; codegen-cleanliness
  contract tests extended (same "no hand-written XMP-style strings" rule).
- Domain rule recorded: photo properties stay `iptc.photo.*`; video-domain semantics come from
  VMH as `iptc.video.*`; shared concepts (e.g. creator, description) are distinct registry
  entries with their VMH-defined mappings — libumm does not invent a merged super-schema
  (concept.md §10).
- Contract tests: importer determinism, registry JSON validity, LF pinning per the existing
  `.gitattributes` rule, generated-file cleanliness.

## Steps

1. Vendor + checksum the VMH spec artifacts; SOURCE recordkeeping.
2. Importer + registry JSON; schema doc deltas.
3. Codegen + contract tests; push; three-OS green (offline tests only — no backend involvement).

## Acceptance criteria

- `registry/iptc-video/iptc-video.json` regenerates byte-identically from the vendored source.
- Generated tables expose VMH properties with per-technology mappings queryable from C++.
- Zero video-specific strings hand-written in `src/` outside `src/generated/`.

## Cut line

Full VMH property coverage may land as `partial: true` (like the EXIF overlay) with the core
descriptive/administrative/rights/technical subset complete; the importer and codegen pipeline
may not be cut.

## Out of scope

Reading or writing any video file (sessions 21–22); EBUCore/JSON serialization of VMH (mappings
are imported as data; only XMP + QuickTime mappings get runtime use in this stage).

## References

Analysis 2026-09-28 finding R2; concept.md §8, §10, §27; sessions 06–07 patterns; decision M5.
