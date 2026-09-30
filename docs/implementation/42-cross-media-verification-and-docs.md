# Session 42 — Cross-media verification and documentation

## Goal

Close Phase 2: systematic cross-media verification of the full accessor catalog, user
documentation, and CLI-concept alignment.

## Depends on

Sessions 39–41 (all accessors landed).

## Scope

**In:**
- **Catalog completeness test:** an offline contract test asserting that every
  non-deferred row of `registry/mappings/cross-media-accessors.json` has a declared
  accessor (parsed from `include/umm/metadata.hpp`) — the analysis doc, the map, and
  the header cannot drift apart silently.
- **Cross-media matrix test:** for each accessor, set on a photo-domain and a
  video-domain `Metadata`, write, re-read, and compare via the accessor — one
  parameterized suite driven by the generated table, run on JPEG + MP4 + MOV in CI
  (extends the session 28 verification-suite pattern; add divergence-ledger entries for
  any backend asymmetries found).
- **Sidecar interaction check:** video XMP sidecars (session 14 pairing) with the new
  properties — reconcile precedence tests for embedded-vs-sidecar on MP4.
- **Docs:**
  - `docs/user/guide.md`: new "Cross-media accessors" section with the tier tables,
    domain rules (`unknown` → photo), transposition/lossiness notes, and the corrected
    rating/GPS story.
  - `docs/umm-cli-concept.md`: align the concept examples with the accessor set
    (universal `umm set file key=value` semantics), explicitly noting the CLI itself
    remains unimplemented.
  - `docs/developer/implementation-history.md`: append a Phase 2 stage row when the
    phase completes.
  - `docs/analysis/phase-2-video-convenience-accessors.md`: check off the session list;
    record any catalog rows dropped during implementation with rationale.
- Release-notes entry covering the `Metadata` layout change (session 36) per
  `docs/abi-policy.md`.

**Out:**
- CLI implementation.
- Audio domain work.

## Work items

1. Contract test (map ↔ header completeness).
2. Table-driven cross-media matrix round-trip suite + divergence ledger updates.
3. Sidecar precedence tests for video.
4. Documentation updates listed above.

## Exit criteria

- Matrix suite green on all three CI OSes with both backends installed.
- Contract test fails when a map row lacks an accessor (verified by mutation).
- Docs updated; analysis doc reflects the final shipped catalog.
