# Session 08 — Core semantic model

Stage 3 · Estimated 45 min

## Goal

Implement the core value/result/provenance types declared in the design-draft headers: the
vocabulary every later session builds on. Pure logic — no backends, no I/O.

## Prerequisites

Session 07 merged (registry types exist).

## Deliverables

Promote these design-draft headers to real (banner removed, header-first rule) and implement:

- `include/umm/result.hpp` — `umm::Error` (code enum + message + backend origin) and
  `umm::Result<T>` (expected-style; no exceptions across the API, decision M1). Error code
  groups: `io`, `format`, `backend_unavailable`, `unsupported_capability`, `conflict`,
  `internal`.
- `include/umm/value.hpp` — `umm::Value` variant covering the registry datatype vocabulary from
  session 06: text, lang-alt (language→text map), text list, integer, rational, real, boolean,
  date-time (with the partial-precision semantics EXIF/IPTC need: date-only, unknown-offset),
  structure (named fields), GPS coordinate. Equality + to-string for tests.
- `include/umm/provenance.hpp` — `PropertyValue`: value + `sources` (list of raw source
  identifiers like `Exif.Image.DateTime`, backend name) + `resolution`
  (`single | equivalent | reconciled | conflict`) + preferred-source marker (concept.md §17).
- `include/umm/metadata.hpp` — `umm::Metadata`: map from registry property id →
  `PropertyValue`, typed getters/setters for the Phase 1 property set (creator, description,
  headline, dateCreated, copyrightNotice, creditLine, keywords, rating, location/GPS), plus
  generic `get(property_id)` / `set(property_id, Value)`. Raw-metadata escape hatch type
  declarations (`RawKey`, `RawEntry`) per concept.md §18 (implementation of raw I/O comes with
  backends).
- `src/` implementations + `tests/unit/` coverage: value equality/conversion edge cases
  (lang-alt default language, partial dates), Result propagation, Metadata set/get round-trip,
  provenance defaulting.

## Steps

1. Reconcile the draft headers against sessions 06–07 realities; adjust drafts first, commit the
   header change with rationale, then implement.
2. Implement + unit test; presets loop green; push.

## Acceptance criteria

- All Phase 1 typed accessors compile and round-trip through `Metadata` in unit tests.
- No exceptions thrown across public functions (tests exercise error paths via `Result`).
- No backend headers included anywhere in `include/umm/`.

## Cut line

Structure-valued IPTC Extension location types may land as declarations + TODO tests if time runs
short — GPS coordinate must not be cut (Phase 1 fixture `gps.jpg` depends on it).

## Out of scope

Reading/writing files; reconciliation logic (session 12) — provenance types only.

## References

concept.md §9, §17, §18; analysis decisions M1, M7.
