# Session 41 — Tier 3 renamed-concept accessors

## Goal

Land accessors whose photo and video property **names differ** but whose concept
matches: locationCreated (↔ locationShot), locationShown, personShown, productShown,
shownEvent, registryEntry, assetIdentifier, aboutCvTerms, featuredOrganisation,
supplier, and (if the session 37 review un-defers it) objectShown.

## Depends on

Session 40 (transposition module — several Tier 3 rows also transpose).

## Scope

**In:**
- Re-point the existing `locationCreated()`/`setLocationCreated()` accessors through the
  cross-media mechanism (photo `iptc.photo.locationCreated`, video
  `iptc.video.locationShot`). Location structs are field-compatible; the video side
  drops `gpsAltitudeRef` on set (documented, mirrors the registry).
- New accessors, all struct-list shapes unless noted:
  - `locationShown()` — Location both sides.
  - `personShown()` — PersonWDetails both sides (photo maps the *WithDetails* property;
    the legacy string-list `personShownInTheImage` stays behind its full ID).
  - `productShown()` — ProductWGtin/ProductWGTIN (same fields).
  - `shownEvent()` — the one **fan-out** row: photo `eventName` (lang-alt) +
    `eventIdentifier` (uri list) ↔ video `shownEvent` (Entity list). Setter takes
    name+identifiers; on photo it writes both properties, on video one Entity. Getter
    assembles the same shape from either side.
  - `registryEntry()` — RegistryEntry both sides.
  - `assetIdentifier()` — string/one both sides (digitalImageGuid ↔ videoIdentifier).
  - `aboutCvTerms()` — CvTerm both sides.
  - `featuredOrganisation()` — string list ↔ Entity list (name transpose).
  - `supplier()` — ImageSupplier list ↔ Entity single (name/identifier subset +
    list↔single rule from session 40).
  - `objectShown()` — only if un-deferred; ArtworkOrObject↔Entity maps `title`↔`name`
    only, with the loss documented.
- Unit tests for every accessor on both domains; round-trip tests for locationCreated,
  personShown, shownEvent, assetIdentifier on JPEG and MP4.

**Out:**
- No removal or aliasing of the photo-specific full IDs; everything stays reachable via
  `get`/`set` with full property IDs.
- Naming inventions: accessor names must be traceable to one of the two standards'
  property names (e.g. `assetIdentifier` — the neutral term used by the VMH definition
  text — is chosen over inventing a new vocabulary).

## Design decisions

- The `shownEvent` fan-out is the only accessor touching two photo properties; it is
  driven by the map's photo-side list (session 37 schema), not special-cased in code.
- Getter probe order for fan-out: photo pair first, then video, mirroring the
  single-property probe rule.

## Work items

1. Header updates (normative) + implementations through the generated map.
2. Fan-out support in the accessor mechanism (bounded: photo-side list).
3. Unit + round-trip tests; MANIFEST/fixture additions if session 38 fixtures lack any
   of these properties.

## Exit criteria

- All Tier 3 accessors verified on both domains; fan-out round-trips on JPEG (both
  photo properties written) and MP4 (single Entity).
- Phase 1 `locationCreated` photo behavior unchanged.
