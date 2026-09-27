# Session 12 — Mapping and reconciliation engine

Stage 4 · Estimated 45–60 min

## Goal

Raw entries → canonical `umm::Metadata` with provenance: the written reconciliation policy
(decision S4a) implemented and tested for the Phase 1 property set on JPEG.

## Prerequisites

Sessions 07, 08, 10, 11 merged.

## Deliverables

- `docs/reconciliation-policy.md` — the normative policy (written **first**, in the session):
  - Per Phase-1 property: the raw sources (from the registry representations), read precedence,
    equivalence rules (e.g. EXIF date + offset tag vs XMP date with offset; IIM
    date+time pair vs single XMP value; UTF-8 vs IIM charset), and the write-synchronization
    rule (which representations are written/updated together — session 13 consumes this).
  - Conflict classification: `single` (one source), `equivalent` (agree after normalization),
    `reconciled` (disagree, policy picks; record preferred + all sources), `conflict`
    (disagreement the policy refuses to auto-resolve — surfaced to the application).
  - Sources of the rules: IPTC Photo Metadata mapping guidance, **MWG guidance as frozen input**
    with ExifTool's MWG module as the compatibility reference (decision S4b) — each rule cites
    its origin.
- `src/core/reconcile.cpp` (+ internal header): consumes `RawDocument`(s), uses the generated
  registry representations to group raw entries per property, normalizes values into
  `umm::Value`, applies policy, emits `Metadata` with full provenance (concept.md §15, §17).
- Public entry point (promote relevant part of `include/umm/umm.hpp` draft):
  `umm::read(path, ReadOptions) -> Result<Metadata>` — options select backend
  (default: first available per manager order) and strictness (conflicts as errors vs recorded).
- Tests:
  - `full-agreeing.jpg` → every property `equivalent`, values correct.
  - `full-conflicting.jpg` → dates classified `reconciled`/`conflict` exactly per policy; sources
    listed; preferred matches policy.
  - Single-block fixtures → `single`.
  - `unicode.jpg` → normalization does not mangle UTF-8.
  - Cross-backend: same canonical result from Exiv2 and ExifTool raw documents (tolerances
    documented where the backends genuinely differ).

## Steps

1. Write the policy doc (bulk of the session's thinking).
2. Implement grouping/normalization/classification.
3. Wire `umm::read`; parameterized fixture tests over both backends; push.

## Acceptance criteria

- Policy doc exists, every implemented rule cites it, every documented Phase-1 rule has a test.
- Both backends produce identical canonical `Metadata` for all agreeing fixtures.
- No property value ever silently drops a source: provenance lists all raw origins.

## Cut line

Keywords/rating rules can defer to a follow-up if dates/creator/description/copyright/GPS/location
are complete — dates are the heart of the problem and must not be cut.

## Out of scope

Write synchronization (session 13 implements the policy's write rules); sidecar merging
(session 14).

## References

concept.md §15, §17; analysis decisions S4a, S4b; IPTC Mapping Guidelines; MWG Guidance (frozen).
