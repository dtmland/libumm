# Session 48 — GPS as Location GPS; remove `exif.gps.position`

## Goal

Make photo EXIF GPS a representation of `iptc.photo.locationCreated[0]` GPS fields, make
video QuickTime GPS reachable only through the `capturePosition` cast, and remove the Phase 1
id `exif.gps.position` with all of its special cases (C8).

## Depends on

Session 47 (cast engine and the `capturePosition` group). Uses the overlay qualifier from
session 45 and the Location GPS codec from session 46.

## Governing decisions

Decision record C8, C3a (docs), C17.

## Scope

**In:**
- **Headers first:** remove `Metadata::gps()` and `setGps()` (`include/umm/metadata.hpp`),
  `Datatype::gps_coordinate` (`include/umm/registry.hpp`), and the "well-known Phase 1"
  wording. `GpsCoordinate` stays in `include/umm/value.hpp`. Update `include/umm/track.hpp`
  (write-back target) and the `umm.hpp` comments about container GPS.
- **Photo:** reading fills `locationCreated[0].gpsLatitude/gpsLongitude/gpsAltitude/
  gpsAltitudeRef` from the EXIF GPS IFD and top-level `exif:GPS*`, with the struct fields as
  further representations (EXIF > XMP top-level > struct, as in today's GPS precedence).
  Writing `locationCreated[0]` GPS writes all three. H3 merge rules apply when
  `locationCreated[0]` already has a place name.
- **Video:** `locationShot[0]` GPS; `capturePosition` downcast on by default for video
  writes.
- **Remove:** `kGps` (`src/core/property_ids.hpp`), `collect_gps` (`src/core/reconcile.cpp`),
  the GPS branch in `src/core/write_sync.cpp`, the `kGps` entries in the read id lists,
  `Datatype::gps_coordinate` handling in `src/metadata.cpp`, and the note in
  `registry/mappings/cross-media-accessors.json`.
- **Geotag:** track write-back sets `locationCreated[0]` GPS (photo) or `locationShot[0]`
  GPS (video).
- Tests: camera-style JPEG/HEIC/DNG GPS reads into `locationCreated[0]`; EXIF and XMP
  `exif:GPS*` within tolerance reconcile as `equal` (C19 Pixel case); a city-only
  `locationCreated[0]` merges GPS without a new entry; `GPSImgDirection` etc. stay in
  `dumpUnmapped()`; geotag tests updated.

**Out:**
- GPS from timed tracks inside video files (the GoPro `gpmd` case) stays out of scope.

## Documentation

- `docs/user/guide.md`: remove "The most common properties" (C3a) and every
  `exif.gps.position` / `gps()` mention; the Location and GPS section explains that camera
  GPS appears as `locationCreated[0]` GPS on photos and needs an upcast on video.
- `docs/reconciliation-policy.md`: replace both `exif.gps.position` sections with Location
  GPS precedence.
- `docs/umm-cli-concept.md`: GPS and geotag examples use `locationCreated`.
- `docs/README.md`: drop the "most common first" description of the guide.
- `docs/developer/release-notes.md`: removed API (`gps()`, `setGps()`,
  `Datatype::gps_coordinate`, `exif.gps.position`) and the migration path.

## Work items

1. Header edits and removals.
2. Photo representation wiring; video cast wiring.
3. Geotag write-back.
4. Tests and documentation.

## Exit criteria

- No `exif.gps.position` string remains in `include/`, `src/`, `registry/`, or living docs.
- Geotag round-trips on JPEG and MP4.
- Full test suites green on all three CI OSes.
