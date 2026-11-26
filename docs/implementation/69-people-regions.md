# Session 69 — People regions read cast and guidance

## Goal

Read person names from MWG and Microsoft face regions, and document how to write regions
(U13).

## Depends on

Session 62. Small; can share a sitting with session 68.

## Governing decisions

F13. C7 (no MWG ↔ IPTC region representation link).

## Scope

**In:**
- `registry/casts/regions-people.json`: MWG `mwg-rs:Regions` entries with `Type = Face`
  → `Name`; Microsoft `MPRI` people names → `personShownInTheImage` (names only, union).
  Up only.
- Fixtures: JPEG with MWG regions and one with Microsoft `MP:RegionInfo` (generator,
  MANIFEST).
- Tests: names appear through `personShown()` with `upcast`; existing names win.

**Out:**
- Writing MWG regions or keeping them in sync with `iptc.photo.imageRegion` (F13).
- Video regions (not planned; VMH has no equivalent).

## Documentation

- `docs/user/guide.md`: "Face tagging": write regions through
  `iptc.photo.imageRegion` with role `Face` / person ids; MWG regions are read-only cast
  sources; video regions not planned.

## Exit criteria

- Both fixtures produce the expected names on both backends where readable.
