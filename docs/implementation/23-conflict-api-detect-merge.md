# Session 23 — Conflict API: detectConflict and merge

Stage 8 · Estimated 45–60 min

## Goal

Public conflict-handling API over the existing provenance model (concept.md §28): enumerate
conflicts without a full read-everything workflow, and resolve them with provenance preserved.
The Stage 5 review confirmed the data model (`PropertyValue` sources/resolution/preferred_source,
`SourceRef` embedded-vs-sidecar containers) needs **no extension** — this is API surface only.

## Prerequisites

Stage 7 complete (sessions 20–22 merged). (Functionally this stage depends only on Stage 6;
ordering follows the stage map.)

## Deliverables

- Header-first design in `include/umm/umm.hpp` (or a dedicated header if the drafts call for it):
  - `detectConflict(path, options) -> Result<ConflictReport>` — per-property conflict entries:
    property id, candidate values with their `SourceRef`s (family + container), and the policy's
    preferred candidate. Implemented over the existing read pipeline; no second reconciliation
    engine.
  - `merge(...)` — resolve a conflicted property by choosing a candidate (by source) or supplying
    an explicit value; the resulting `Metadata` records the resolution (`reconciled` with
    `preferred_source` set, or a documented equivalent for user-supplied values) instead of
    discarding provenance the way a plain `set()` does today.
- Conflict classes covered: same-family embedded conflicts (session 12), embedded-vs-sidecar
  conflicts (session 14), and video container-vs-XMP conflicts (session 21) — one enumeration
  path for all.
- Tests: `full-conflicting` fixtures (JPEG + the Stage 6/7 equivalents), sidecar conflict pair,
  video conflicting fixture — detect, merge each way, verify the merged metadata's provenance;
  `conflicts_as_errors` read option interplay covered.
- Policy doc: a short "conflict resolution API" section describing how merge choices map onto the
  resolution states.

## Steps

1. Header design + doc section.
2. `detectConflict` over the read pipeline; report tests.
3. `merge` + provenance-preservation tests; push; three-OS green.

## Acceptance criteria

- Every fixture-encoded conflict class is enumerable through one API with full provenance.
- Merging never silently loses the losing candidates (they remain listed as sources).
- No change to reconciliation outcomes for non-conflicted reads (existing tests unmodified).

## Cut line

Video-conflict coverage may defer to session 24 if Stage 7 fixtures complicate it; JPEG and
sidecar conflict coverage may not.

## Out of scope

`synchronize()` and bulk write-back (session 24); interactive/callback resolution schemes.

## References

concept.md §17, §28; analysis 2026-09-28 §2 (provenance readiness) and finding R1's decision
context; sessions 12, 14, 21 conflict fixtures.
