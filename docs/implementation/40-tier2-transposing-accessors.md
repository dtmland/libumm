# Session 40 — Tier 2 transposing accessors

## Goal

Land the same-name, different-shape accessors, adding the value-transposition layer:
creator, headline, keywords, otherConstraints, digitalSourceType, modelReleaseStatus,
propertyReleaseStatus, copyrightOwner, licensor.

## Depends on

Session 39 (accessor mechanism).

## Scope

**In:**
- A small transposition module (e.g. `src/core/transpose.cpp/.hpp`) with one function
  per transposition kind declared in the generated map:
  - `string ↔ x-default lang-alt` (headline, otherConstraints)
  - `string list ↔ joined x-default lang-alt` (keywords — reuse the exact join/split
    rule the reconcile engine already applies; both sides map to XMP `dc:subject`)
  - `name strings ↔ EntityWRole list` (creator — reuse the session 21 rule:
    `name`-only entities from string sources)
  - `CV URI ↔ CvTerm struct` (digitalSourceType, modelReleaseStatus,
    propertyReleaseStatus — URI becomes the CvTerm's CV id field and back)
  - `struct field subset` (copyrightOwner: shared `name`/`identifiers` carry over)
  - `list ↔ single` (licensor: setter with >1 entry on video returns a documented
    error rather than silently dropping entries; getter wraps the single video value)
- Each transposition documents its lossy directions in the header comment and in
  `docs/reconciliation-policy.md` (new "Accessor transposition" section), sourced from
  the notes column of the session 37 mapping JSON.
- Unit tests per transposition function (both directions, round-trip where lossless,
  explicit expected-loss cases where not).
- Accessor unit tests + JPEG/MP4 round-trip tests for creator, headline, keywords,
  digitalSourceType.

**Out:**
- No changes to how full property IDs behave; transposition happens **only** inside
  convenience accessors.

## Design decisions

- Setters store the **domain-native shape** (transpose on set), so `get(full_id)`,
  provenance, and write-sync see exactly what the standard defines — no shadow storage.
- Getters return the stored domain value untransposed; callers wanting the photo shape
  from a video file use the documented transposition semantics. (Alternative — getters
  normalizing to the photo shape — is rejected: it would hide the standard's own types
  and contradict the "no invented representations" rule.)
- Lossy directions fail loudly where data would be dropped (licensor list→single) and
  are documented where representable (keywords lang-alt merging).

## Work items

1. Transposition module + generated-map wiring.
2. Header updates for the new accessors; implementation.
3. Unit + round-trip tests; policy-doc section.

## Exit criteria

- Round-trips lossless where declared lossless; declared-lossy cases covered by tests
  asserting the documented behavior.
- Existing `setCreator`/`setKeywords`/`setHeadline` photo behavior byte-identical.
