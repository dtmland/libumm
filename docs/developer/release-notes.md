# Unreleased notes

Pre-1.0 (`0.y.z`): the C++ ABI is not stable. See [docs/abi-policy.md](../abi-policy.md).

## Unreleased

### Session 47 — Cast engine

`umm::cast` evaluates or applies the first rule set (`registry/casts/`).
`ReadOptions::report_casts` fills `Metadata::castCandidates()`.
`WriteOptions::downcast` defaults to `capturePosition` on video.

Movie-header-only videos no longer get `dateCreated` from `QuickTime.CreateDate`
(C7). Enable `report_casts` to see the `videoCreated` candidate; apply with
`include_approximate`. QuickTime GPS is no longer read or write-synced as
`exif.gps.position`; upcast/downcast `capturePosition` instead. QuickTime Keys,
UserData, and ItemList stay in the base key (`QuickTime.Keys.CreationDate`,
`QuickTime.UserData.GPSCoordinates`, …).

### Session 46 — Full photo Location structs

Photo `locationCreated` and `locationShownInTheImage` are full IPTC Location
structures, including GPS fields (`gpsLatitude` / `gpsLongitude` / `gpsAltitude` /
`gpsAltitudeRef`) as numbers. Write-sync emits XMP `LocationCreated` /
`LocationShown`, not photoshop/IIM city. Legacy `cityLegacy` /
`provinceOrStateLegacy` / `countryLegacy` no longer read or write as
`locationCreated` (C4b). Camera EXIF GPS remains `exif.gps.position` until
session 48. A side cast from legacy fields to Location Shown is session 47.

### Session 44 — Full read coverage

`umm::read` reconciles every IPTC Photo or Video registry id for the file's domain, not
only the cross-media accessor map. `dumpUnmapped()` shrinks as those representations are
consumed. `rating()` / `setRating()` are Tier 1 (`iptc.photo.imageRating` ↔
`iptc.video.workflowRating`). Photo `dateCreated` no longer treats `Exif.Image.DateTime`
(ModifyDate) as a candidate; IFD0 `DateTimeOriginal` is accepted (DNG).

### Session 43 — Base metadata and dump views

Public backend vocabulary is renamed from unmapped/raw to **base** (C18). `Metadata::unmapped()`
is replaced by `dumpAll()`, `dumpUnmapped()`, and `dumpValue()` (C13).

| Before | After |
|---|---|
| `UnmappedKey` / `UnmappedEntry` | `BaseKey` / `BaseEntry` |
| `UnmappedDocument` / `UnmappedChanges` | `BaseDocument` / `BaseChanges` |
| `readUnmapped` / `writeUnmapped` | `readBase` / `writeBase` |
| `SourceRef::raw_key` | `SourceRef::base_key` |
| `Metadata::unmapped()` | `Metadata::dumpAll()` |
| `Metadata::unmapped(key)` | `Metadata::dumpValue(key)` |
| — | `Metadata::dumpUnmapped()` |

`dumpAll()` is every base entry in source order. `dumpUnmapped()` is computed during
reconcile: entries whose base key was not consumed as a representation. Session 44
reconciles every domain registry id, so dumpUnmapped shrinks to leftover companions
and tags no canonical property consumed. This is a pre-1.0 source break; rebuild
consumers.

### Session 45 — Struct-field backend names

Photo registry struct fields keep TR `etTag` (`et_tag` plus `representations.exiftool`).
The EXIF overlay GPS rows now name `struct_property: locationCreated`. ExifTool
struct-field and iptcExt XMP tag aliases are generated from those names; ShownEvent
stays a cited hand-written exception because it is not in the Technical Reference.

### Session 36 — `Metadata` layout

`umm::Metadata` gained a `MediaDomain` member (`photo` / `video` / `unknown`) with
`mediaDomain()` / `setMediaDomain()`. That changes the object layout. Consumers must
rebuild against this library version; there is no C++ ABI promise in any linkage mode.
Default `unknown` keeps Phase 1 setter semantics (photo property ids).
