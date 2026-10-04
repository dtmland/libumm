# Session 46 — Full photo Location structs

## Goal

Read and write photo `locationCreated` and `locationShownInTheImage` as full IPTC
`Location` structures, including the GPS fields (C4a). The legacy city/state/country fields
stop standing in for `locationCreated` (C4b).

## Depends on

Session 44 (legacy ids reconcile under their own ids), session 45 (generated struct-field
names).

## Governing decisions

Review record C4a, C4b, C4d; decision record C11 (behavior change).

## Scope

**In:**
- Route photo `locationCreated` and `locationShownInTheImage` through the table-driven
  struct path video uses: `Iptc4xmpExt:LocationCreated` / `LocationShown` with every TR
  field (`name`, `identifiers`, `sublocation`, `city`, `provinceState`, `countryName`,
  `countryCode`, `worldRegion`, `gpsLatitude`, `gpsLongitude`, `gpsAltitude`,
  `gpsAltitudeRef`).
- **GPS codec** for struct fields: XMP `DDD,MM.mmmmmmH` and decimal strings ↔ numbers, using
  the reconcile tolerances (1e-5°, 0.5 m). Altitude ref 1 negates the value (H10).
- **Remove** the legacy special paths: reading IIM/`photoshop:` city/state/country as
  `locationCreated` (`src/core/reconcile.cpp`, about lines 1232–1304 and 1425–1460) and
  writing them from it (`src/core/write_sync.cpp` `sync_location`, about lines 557–570).
- Tests: round-trip every Location field including GPS on JPEG, TIFF, PNG, WebP, DNG, and
  AVIF where capabilities allow; a file with only legacy fields reads with
  `cityLegacy` etc. and no `locationCreated`.

**Out:**
- The legacy ↔ Location Shown side cast (session 47).
- EXIF GPS as a representation of `locationCreated[0]` (session 48).

## Documentation

- `docs/user/guide.md`: a "Location and GPS" section: Location struct fields including GPS;
  `Iptc4xmpCore` (IPTC Core, the older namespace) versus `Iptc4xmpExt` (IPTC Extension);
  Location Created (where the camera was) versus Location Shown (what the picture shows);
  the legacy fields as their own properties.
- `docs/reconciliation-policy.md`: rewrite the `locationCreated` section (write claim, legacy
  fields).
- `docs/developer/release-notes.md`: legacy fields no longer read or written as
  `locationCreated`.

## Work items

1. Struct path wiring for the two photo Location properties.
2. GPS string ↔ number codec with tests.
3. Removal of the legacy path.
4. Tests and documentation.

## Exit criteria

- Full Location struct round-trip with both backends where they claim the capability; any
  backend gap is recorded in `tests/verification/ledger.json`.
- No code path maps legacy fields onto `locationCreated`.
