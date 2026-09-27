# Session 06 — Standards registry: data model and IPTC importer

Stage 2 · Estimated 45 min · **Registry-first (decision S2)**

## Goal

A machine-readable property registry populated from the IPTC Photo Metadata **Technical Reference**
(JSON/YAML), with provenance (standard + version) recorded per property (decision M5). No C++ yet.

## Prerequisites

Sessions 01–03 merged (04–05 not required — this session is pure data + Python).

## Deliverables

- `registry/schema.md` — written definition of the registry record, matching concept.md §20:
  `id` (e.g. `iptc.photo.creator`), `standard`, `standard_version`, `schema` (Core/Extension),
  `standard_property_name`, `definition`, `datatype`, `cardinality`, representations
  (`xmp` namespace+property, `iptc_iim` dataset, `exif` tag where IPTC mapping guidance provides
  one), `mapping_notes`, `source` (document + version + retrieval date).
- `registry/iptc-photo/` — the imported output, one JSON file (or one per schema) produced by the
  importer. Committed (generated-but-committed, like fixtures) so builds are offline.
- `tools/registry/import_iptc.py` — stdlib-only Python:
  - Input: a **vendored copy** of the IPTC Technical Reference JSON under
    `registry/sources/` (record its version + URL + retrieval date in a `SOURCE.md`; IPTC TR is
    published for exactly this purpose).
  - Output: registry JSON conforming to `schema.md`; deterministic ordering; fails on unknown
    datatypes rather than guessing.
- `tests/build/test_registry.py` — registry JSON validates against the documented shape; every
  record has standard/version/source; ids unique; re-running the importer is byte-identical
  (determinism).

## Steps

1. Download the IPTC TR JSON manually, vendor under `registry/sources/` with SOURCE.md.
2. Write schema.md — resolve datatype/cardinality vocabulary now (string, lang-alt, struct,
   date, etc. as used by the TR).
3. Write importer; generate; eyeball `creator`, `description`, `dateCreated`, location structs
   (Location Created / Location Shown — these are structured, keep them structured).
4. Write tests; run contract suite.

## Acceptance criteria

- Registry covers all IPTC Core 1.5 + Extension 1.9 properties from TR 2025.1 (count asserted in
  the test against the TR's own count).
- `python3 tools/registry/import_iptc.py` is reproducible byte-for-byte.
- Provenance queryable: standard versions recoverable from the data alone.

## Cut line

EXIF-mapping columns (from the IPTC Mapping Guidelines, which are HTML not machine-readable) may
land as a curated overlay file in session 07 instead of this one.

## Out of scope

Video Metadata Hub (Stage 7); code generation (session 07); any C++.

## References

concept.md §5, §9, §20, §21; analysis decisions S2, M5.
