# Unreleased notes

Pre-1.0 (`0.y.z`): the C++ ABI is not stable. See [docs/abi-policy.md](../abi-policy.md).

## v0.3.0 (in progress)

Not released. `SOVERSION` stays `0.2` until session 77.

### Session 60 — DateTime shift and offset helpers

`umm::shift` and `umm::withOffset` in `include/umm/value.hpp`. Backend-free.
Precision rules and the ±18:00 offset limit are in the header. An unknown
offset is not treated as UTC.

### Session 61 — Selective reads

`ReadOptions::properties` reconciles only the listed registry ids. Unknown ids
return `unknown_property`. `dumpAll()` and `dumpUnmapped()` are empty on a
selective read. Sidecar pairing is unchanged. ExifTool receives the
registry-derived tag list; Exiv2 ignores the hint. Listed-id values match a
full read.

### Session 62 — `readMany` and ExifTool batching

`umm::readMany` streams per-path results on the calling thread. Per-file
errors go to the callback. Cancellation, deadline, and argument errors are the
return value and stop further callbacks. ExifTool batches up to
`kExifToolReadBatch` (32) paths per `-execute`. Exiv2 loops. `read_bench` is
built with `-DUMM_BUILD_BENCHMARKS=ON` and is not part of CTest.

### Session 64 — Video technical upcasts

`videoTechnical` is a one-way up group for duration, frame size, frame rate,
coding, audio sample rate and channels, and orientation. ExifTool is required.
The Exiv2 0.28.9 video spike did not report rotation and misread a GPS fixture,
so `EXIV2_ENABLE_VIDEO` stays off. Synthetic `Xmp.video.*` / `Xmp.audio.*` keys
are never written.

### Session 66 — Technical view values

`umm::read` fills `Metadata::technical()` for JPEG, HEIC, RAW, and MP4/MOV.
Each value carries its source key. Container pixel size is an intrinsic and is
not in `dumpAll()`. Selective `technical.*` reads request only those sources.
Exposure and focal length stay unmapped. ExifTool IFD0 `ImageHeight` is read
as `Exif.Image.ImageLength`. Bundled Exiv2 does not fill video technical
values (`exiv2-video-technical-view`).

### Session 65 — Technical view contract

`Metadata::technical()` and `TechnicalInfo` are in `include/umm/metadata.hpp`.
The view is empty until values are filled. `BaseDocument::intrinsics` holds
container pixel facts that are not stored metadata. Pseudo-ids `technical.*`
are `property_read_only`.

### Sessions 67–68 — Legacy and container text upcasts

`windowsXP`, `exifUserComment`, `pngText`, and `quickTimeUserData` are up-only.
`XP*` UCS-2LE byte lists and `UserComment` charset prefixes are decoded in the
cast. Keyword and author lists split on `;`. An existing canonical value wins
and does not block other targets. `exifUserComment` is approximate and does
not replace a nonempty description. QuickTime user-data dates keep an offset
only when the source string has one. Exiv2 reads PNG Description only
(`Png.Text.Description`); Title, Author, and Copyright, and QuickTime user
data, stay ExifTool-only (`exiv2-png-text-keywords`,
`exiv2-quicktime-user-data`).

### Session 63 — Read-time upcasts

`ReadOptions::upcast` fills empty canonical targets from named up groups in
memory only. Unknown groups fail with `invalid_value`. A representation always
wins. `needs_force` and `ambiguous` never apply. Naming `videoCreated` applies
that approximate group. Provenance is `cast:<group>`.

### Sessions 67–68 — Legacy and container text upcasts

`windowsXP` and approximate `exifUserComment` fill photo title, description,
keywords, and creator. `pngText` and `quickTimeUserData` fill photo or video
text from PNG keywords and QuickTime user-data atoms. Up only. UCS-2LE byte
lists and `UserComment` charset prefixes are decoded in the cast. A date offset
is recorded only when the source string has one. Exiv2 reads PNG Description
only; bundled Exiv2 does not read QuickTime user data.

### Session 71 — Durable writes

`WriteOptions::durable` and `SyncOptions::durable` (default `false`). POSIX
`fsync` / macOS `F_FULLFSYNC` and Windows `FlushFileBuffers` run only when the
flag is set. Fault points `before_file_sync` and `after_rename_sync` cover the
flush steps.

## v0.2.0

Breaking / relink required. Pre-1.0 `SOVERSION` is `0.2`. `ErrorCode` gained
`cancelled` and `deadline_exceeded` (appended; earlier codes are unchanged).

### Sessions 52–54 — Thread safety and backend pool

Public headers state which calls are safe to run concurrently. ExifTool keeps a
process pool (`ExifToolConfig::max_processes`, default 1). Exiv2 stays serialized
because its XMP toolkit is process-global. `configureExifTool` applies to calls
that start after it returns; in-flight children are retired when they finish.
`tests/backend/test_parallel_read.cpp` and `test_exiftool_pool.cpp` cover the
contract. CI job `tsan (ubuntu-24.04)` configures `-DUMM_SANITIZE=thread`.

### Sessions 55–56 — Cancellation

`ReadOptions`, `WriteOptions`, `SyncOptions`, and `CastOptions` accept a shared
`CancelToken` and an optional deadline. Checkpoints run before backend selection,
between carriers, between read and reconcile, and before the atomic rename. A
write that has already renamed returns success. ExifTool's poll loop wakes within
50 ms, kills that child, and the next call starts clean.

### Sessions 57–58 — Per-property access

`umm::propertyAccess` and `propertyAccessAll` dry-run `umm::write` and report
`AccessReason` (`ok`, `unknown_property`, `backend_unavailable`,
`container_read_only`, `policy_forbids_sidecar`, `policy_requires_sidecar`,
`property_read_only`, `no_mapping_for_type`). Reason strings are stable within
this minor version.

### Session 59 — Verification

User guide sections Threads, Cancellation, and Enabling editor controls. Install
docs cover `UMM_SANITIZE`. U1, U4, and U9 ship in this version. U7's packaging
guide remains session 72.

## Unreleased

### Session 51 — Casting verification

Tier A fixtures reproduce the C19 real-device tag layouts with synthetic values
(`video/iphone-style.mov`, `jpeg/iphone-heic-layout.jpg`, `raw/pixel-style.dng`,
`jpeg/pixel-style.jpg`, `video/gopro-style.mp4`). The HEIC container stays Tier B.
`test_casting_verification` checks those layouts and the first cast-rule set on
each claiming backend. Gaps live in `tests/verification/ledger.json`; doc headings
in the user guide, reconciliation policy, and generated property index map to tests
in `tests/verification/doc-audit.json`.

### Session 50 — Generated property reference

`docs/user/properties/` replaces the hand-maintained full-property tables in the
user guide. Regenerated by `tools/registry/generate_property_reference.py` from
the registry, overlay, casts, and cross-media map (the same data as
`umm::describe`). Do not edit those pages by hand.

### Session 49 — Property map

`umm::describe(property_id)` and `umm::describe(property_id, path)` return
`Result<PropertyMap>`: definition and representations (L1), cast rules (L2),
and the cross-media partner with its own L1–L2 (L3). A cross-media name
(`locationCreated`) expands to both domain ids. File mode fills values,
consumed base entries, and cast-group statuses identical to
`umm::cast(..., dry_run)`. Types live in `umm/describe.hpp`.

### Session 48 — GPS as Location GPS

Removed the Phase 1 well-known id `exif.gps.position`, `Metadata::gps()` /
`setGps()`, and `Datatype::gps_coordinate`. `GpsCoordinate` remains in
`value.hpp` for track matching. Photo camera GPS (EXIF IFD and top-level
XMP-exif) is a representation of `iptc.photo.locationCreated[0]` GPS
(precedence EXIF > XMP-exif > struct). Video QuickTime GPS stays the
`capturePosition` cast, not `locationShot`. Geotag write-back merges GPS
into `locationCreated[0]` (photo) or `locationShot[0]` (video). Rebuild
consumers; this is a pre-1.0 source break.

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
`locationCreated` (C4b). Camera EXIF GPS is session 48 (`locationCreated[0]`).
A side cast from legacy fields to Location Shown is session 47.

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
