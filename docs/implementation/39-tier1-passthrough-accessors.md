# Session 39 — Tier 1 pass-through accessors

## Goal

Land the cross-media accessor mechanism and all Tier 1 (identical-shape) accessors:
the five Phase 1 accessors become cross-media, and the new identical-shape accessors
are added (title, accessibility pair, rightsUsageTerms, sourceSupplyChain, dataMining,
contributor, genre, both encoded-rights expressions, the four AI properties).

## Depends on

Sessions 36 (media domain), 37 (generated map), 38 (video pipeline coverage).

## Scope

**In:**
- Header first: `include/umm/metadata.hpp` gains the new getter/setter pairs; comments
  on the existing `description()`, `copyrightNotice()`, `creditLine()`, `dateCreated()`
  accessors change from `iptc.photo.X` to "cross-media: iptc.photo.X / iptc.video.X".
- One private resolution helper on `Metadata`:
  - setter: map concept → property ID via generated `CrossMediaAccessorDef` table +
    `mediaDomain()` (`unknown` → photo, preserving Phase 1 semantics).
  - getter: probe photo ID, then video ID.
- Implement all Tier 1 accessors through that helper — pass-through rows need no value
  conversion; datatype validation reuses the existing `matchesDatatype` path.
- `gps()`/`setGps()`: no code change (already cross-media); update header comment only.
- Unit tests: each accessor set/get on photo-domain and video-domain `Metadata`;
  `unknown` domain behaves exactly as Phase 1; getter works with no domain set.
- Round-trip tests: representative Tier 1 accessors (title, contributor, genre,
  dataMining, one AI property) through `umm::write`/`umm::read` on JPEG and MP4 fixtures.

**Out:**
- Tier 2/3 accessors (sessions 40, 41).
- User guide rewrite (session 42); header comments only here.

## Design decisions

- Accessors resolve through the **generated table**, never hand-written ID switches —
  one mechanism for all tiers, so sessions 40–41 only add transposition functions.
- Existing photo-only accessors that stay photo-only (`rating()`, and the
  photo-specific location accessors until session 41 re-points them) are explicitly
  commented as photo-only to keep the API honest.

## Work items

1. Header update (normative) + implementation in `src/metadata.cpp`.
2. Resolution helper + wiring to `src/generated/` table.
3. Unit tests in `tests/unit/test_metadata.cpp`.
4. Backend round-trip tests (JPEG + MP4) beside the existing video read/write tests.

## Exit criteria

- All Phase 1 accessor tests pass unchanged with domain `unknown`/`photo`.
- Every Tier 1 accessor verified on both domains in unit tests; representative set
  verified end-to-end in CI on all three OSes.
