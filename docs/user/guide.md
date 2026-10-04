# libumm user guide

libumm gives applications one standards-based API for reading, writing, and synchronizing
media metadata across photos and video. Canonical properties are the IPTC Photo Metadata
2025.1 and IPTC Video Metadata Hub 1.7 registry ids. EXIF, IPTC IIM, and QuickTime tags
are **representations** of those properties, not a second set of canonical names. The
Exiv2 and ExifTool backends do the low-level work.

## Reading and writing

Everything returns `umm::Result<T>` — no exceptions cross the public API.

```cpp
#include <umm/umm.hpp>

auto meta = umm::read("photo.jpg");            // reconciled canonical metadata
if (meta) {
  auto creator = meta->creator();              // typed accessor
  auto title   = meta->get("iptc.photo.title"); // or generic access by property id
}

umm::Metadata m = *meta;
m.setKeywords({"family", "vacation"});
auto report = umm::write("photo.jpg", m);      // atomic, write-synchronized
```

- `umm::read` reconciles all embedded representations (XMP, IPTC IIM, EXIF, QuickTime) and a
  paired `.xmp` sidecar into one canonical value per property, with provenance and conflict
  detection (see [docs/reconciliation-policy.md](../reconciliation-policy.md)). With an empty
  `ReadOptions::backend`, the type's `preferred_backend` is used when that backend is
  available (ExifTool for video and other Exiv2-weak types, Exiv2 for JPEG); otherwise the
  first available backend. An explicit `backend` is a hard pin with no fallback.
- `umm::write` keeps every synchronized representation up to date and replaces files
  atomically (temp file + rename). `WriteOptions::policy` selects embedded vs sidecar
  storage; `dry_run` previews the decision.
- `umm::detectConflict` / `umm::merge` / `umm::synchronize` handle disagreeing sources and
  keep embedded and sidecar carriers in agreement.
- `umm::capabilities` reports, per backend / file type / metadata category, what can be read
  or written — the data behind [docs/supported-types.md](../supported-types.md).
- `umm::describe` returns the property map (representations, casts, cross-media partner);
  pass a file to fill values and cast statuses. See [Exploring a property](#exploring-a-property).

## Canonical property names

Properties are addressed by stable registry ids:

- `iptc.photo.*` — IPTC Photo Metadata (Core + Extension)
- `iptc.video.*` — IPTC Video Metadata Hub

`umm::read` reconciles every registry id for the file's domain. The registry
(`umm::Registry`) is generated from the vendored IPTC Technical References in `registry/`;
ids, definitions, and XMP/IIM/EXIF/QuickTime mappings are queryable at runtime.
Typed convenience accessors cover the cross-media concepts (and photo `rating`);
generic `get`/`set` by id works for every property.

`dateCreated` is when the scene was captured. Digitized-time (`DateTimeDigitized` /
CreateDate) and file-modify time (`Exif.Image.DateTime` / ModifyDate) are different
moments and are not candidates for `dateCreated`.

## Location and GPS

Photo `locationCreated` and `locationShownInTheImage` are full IPTC **Location**
structures (cardinality many). Field names are the Technical Reference logical names:

`name`, `identifiers`, `sublocation`, `city`, `provinceState`, `countryName`,
`countryCode`, `worldRegion`, `gpsLatitude`, `gpsLongitude`, `gpsAltitude`,
`gpsAltitudeRef`.

GPS fields on the struct are numbers (decimal degrees, WGS 84; west/south negative).
Reads accept XMP `DDD,MM.mmmmmmH`, decimal, and hemisphere-suffixed strings; writes
use decimal with a hemisphere suffix so brace-encoded structs stay comma-safe.
Equivalence uses 1e-5° and 0.5 m. `gpsAltitudeRef` is 0 (above WGS 84) or 1
(below). Camera EXIF GPS IFD and top-level XMP-exif GPS are representations of
`locationCreated[0]` GPS on photos (EXIF > XMP-exif > struct). A city-only
Location Created merges GPS onto the same `[0]` entry. Video QuickTime GPS is
not a `locationShot` representation; upcast `capturePosition` to fill
`locationShot[0]` GPS, and default video writes downcast it back.

**Created versus Shown.** Location Created is where the camera was.
Location Shown is what the picture depicts. They are independent lists.

**`Iptc4xmpCore` versus `Iptc4xmpExt`.** These are XMP namespace prefixes, not
different standards. `Iptc4xmpCore` is IPTC Photo Metadata Core
(`http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/`). `Iptc4xmpExt` is the Extension
schema (`http://iptc.org/std/Iptc4xmpExt/2008-02-29/`). Location Created and
Location Shown live in the Extension namespace.

**Legacy city/state/country.** `iptc.photo.cityLegacy`, `provinceOrStateLegacy`,
`countryLegacy`, `countryCodeLegacy`, and `sublocationLegacy` are their own
canonical properties (photoshop/IIM / Core `Location`). They no longer fill or
write `locationCreated`. Use `umm::cast(..., CastDirection::side)` group
`locationShownLegacy` to copy them to or from `locationShownInTheImage[0]`
(MWG maps those IIM/photoshop fields to Location Shown; both are typically
filled afterwards, unlike capture GPS).

## Casting

Casts are opt-in links that are **not** representations (C7). Preview with
`ReadOptions::report_casts` or `umm::cast` (`dry_run` default true). Apply with
`CastOptions::dry_run = false`. Statuses: `can_cast`, `equal`, `needs_force`
(requires `force`), `target_not_storable`, `ambiguous`. Empty sources are
omitted.

**Up** fills a canonical property from a non-canonical base key.
**Down** writes a non-canonical key from a canonical property.
**Side** keeps two canonical properties in step.

First groups: video `capturePosition` (QuickTime / EXIF GPS ↔
`locationShot[0]` GPS), `videoCreated` / `videoModified` (movie-header dates),
`recordingDevice`; photo `locationShownLegacy`, `personShown`,
`creatorImageCreator`.

Movie-header `CreateDate` is often UTC-without-offset or simply wrong (re-export
or unset camera clock). `videoCreated` is **approximate**: listed in previews,
applied only with `include_approximate`. Keys `CreationDate` remains the
automatic `dateCreated` representation.

`WriteOptions::downcast` defaults to `capturePosition` on video so players that
only read QuickTime GPS still see a written `locationShot` GPS. Pass an empty
vector to write none.

Cast-rule sources appear in `dumpUnmapped()` with `BaseEntry::cast_source`.

## Exploring a property

`umm::describe` returns the full map for a registry id or a cross-media name:
definition and representations (L1), cast rules (L2), and the other domain's
layers (L3). Pass a file to fill values, consumed base entries, and cast-group
statuses (the same statuses as `umm::cast(..., dry_run)`).

```cpp
auto map = umm::describe("locationCreated");
if (map) {
  for (const auto& property : map->properties) {
    // photo locationCreated and video locationShot
    auto with_file = umm::describe(property.layers.id, "photo.jpg");
  }
}

auto creator = umm::describe("iptc.photo.creator");
// creator.properties[0].layers.representations  // XMP, IIM, EXIF
// creator.properties[0].cross_media->other.id   // iptc.video.creator
```

The map is built from the generated registry, overlay, cast, and accessor
tables. The [property reference](properties/README.md) renders the same data.

## Cross-media accessors

Convenience accessors exist only for concepts IPTC defines in **both** Photo Metadata and
the Video Metadata Hub. Setters take a photo-native value and write the domain-correct
property id. Getters probe `iptc.photo.*` then `iptc.video.*` and do not need
`mediaDomain()`.

- `MediaDomain::unknown` (the default) writes the photo id — Phase 1 setter behavior.
- `MediaDomain::photo` / `MediaDomain::video` select that domain on set.
- `umm::read` sets the domain from the sniffed file type (JPEG → photo, MP4/MOV → video).
- Full registry ids stay reachable via `get`/`set`. Camera GPS is
  `locationCreated()` / `locationShot` GPS fields, not a separate property.
- `objectShown` is **deferred**: ArtworkOrObject ↔ Entity would keep only `title`↔`name`.

### Domain and transposition

| Rule | Behavior |
|---|---|
| Unknown domain on set | Photo property ids |
| Getters | Probe photo ids, then video ids |
| Tier 1 | Same shape both sides (pass-through) |
| Tier 2 | Datatype transpose (string ↔ lang-alt, names ↔ Entity, URI ↔ CvTerm, list ↔ single) |
| Tier 3 | Same concept, different property names (`locationCreated` ↔ `locationShot`, and so on) |
| `shownEvent` | Photo `eventName` + `eventIdentifier` ↔ video Entity list; setter takes name + identifiers |
| Lossy transposes | Extra photo fields/entries stay only under full ids (licensor/supplier list→single; location drops `gpsAltitudeRef` on video) |

### Tier 1 — identical shape

| Accessor | Photo id | Video id |
|---|---|---|
| `title` | `iptc.photo.title` | `iptc.video.title` |
| `description` | `iptc.photo.description` | `iptc.video.description` |
| `copyrightNotice` | `iptc.photo.copyrightNotice` | `iptc.video.copyrightNotice` |
| `creditLine` | `iptc.photo.creditLine` | `iptc.video.creditLine` |
| `dateCreated` | `iptc.photo.dateCreated` | `iptc.video.dateCreated` |
| `rating` | `iptc.photo.imageRating` | `iptc.video.workflowRating` |
| `altTextAccessibility` | `iptc.photo.altTextAccessibility` | `iptc.video.altTextAccessibility` |
| `extendedDescriptionAccessibility` | `iptc.photo.extendedDescriptionAccessibility` | `iptc.video.extendedDescriptionAccessibility` |
| `rightsUsageTerms` | `iptc.photo.rightsUsageTerms` | `iptc.video.rightsUsageTerms` |
| `sourceSupplyChain` | `iptc.photo.sourceSupplyChain` | `iptc.video.sourceSupplyChain` |
| `dataMining` | `iptc.photo.dataMining` | `iptc.video.dataMining` |
| `contributor` | `iptc.photo.contributor` | `iptc.video.contributor` |
| `genre` | `iptc.photo.genre` | `iptc.video.genre` |
| `embeddedEncodedRightsExpression` | `iptc.photo.embeddedEncodedRightsExpression` | `iptc.video.embeddedEncodedRightsExpression` |
| `linkedEncodedRightsExpression` | `iptc.photo.linkedEncodedRightsExpression` | `iptc.video.linkedEncodedRightsExpression` |
| `aiPromptInformation` | `iptc.photo.aiPromptInformation` | `iptc.video.aiPromptInformation` |
| `aiPromptWriterName` | `iptc.photo.aiPromptWriterName` | `iptc.video.aiPromptWriterName` |
| `aiSystemUsed` | `iptc.photo.aiSystemUsed` | `iptc.video.aiSystemUsed` |
| `aiSystemVersionUsed` | `iptc.photo.aiSystemVersionUsed` | `iptc.video.aiSystemVersionUsed` |

### Tier 2 — transposing

| Accessor | Photo | Video | Transpose |
|---|---|---|---|
| `creator` | string list | Entity list | names ↔ Entity.name |
| `headline` | string | lang-alt | string ↔ x-default |
| `keywords` | string list | lang-alt | joined x-default |
| `otherConstraints` | lang-alt | string | x-default ↔ string |
| `digitalSourceType` | URI | CvTerm | URI ↔ cvId |
| `modelReleaseStatus` | URI | CvTerm | URI ↔ cvId |
| `propertyReleaseStatus` | URI | CvTerm | URI ↔ cvId |
| `copyrightOwner` | struct list | struct list | name/identifiers subset; role video-only |
| `licensor` | struct list | single Entity | extra photo entries stay on the full id |

### Tier 3 — renamed concepts

| Accessor | Photo id(s) | Video id |
|---|---|---|
| `locationCreated` | `iptc.photo.locationCreated` | `iptc.video.locationShot` |
| `locationShown` | `iptc.photo.locationShownInTheImage` | `iptc.video.locationShown` |
| `personShown` | `iptc.photo.personShownInTheImageWithDetails` | `iptc.video.personShown` |
| `productShown` | `iptc.photo.productShownInTheImage` | `iptc.video.productShown` |
| `shownEvent` | `iptc.photo.eventName` + `iptc.photo.eventIdentifier` | `iptc.video.shownEvent` |
| `registryEntry` | `iptc.photo.imageRegistryEntry` | `iptc.video.registryEntry` |
| `assetIdentifier` | `iptc.photo.digitalImageGuid` | `iptc.video.videoIdentifier` |
| `aboutCvTerms` | `iptc.photo.cvTermAboutImage` | `iptc.video.cvTermAboutTheContent` |
| `featuredOrganisation` | `iptc.photo.nameOfOrganisationFeaturedInTheImage` | `iptc.video.featuredOrganisation` |
| `supplier` | `iptc.photo.imageSupplier` | `iptc.video.supplier` |

Photo `locationCreated` / `locationShown` write the Extension Location structs, not
legacy photoshop/IIM city fields.
`personShown` uses the WithDetails struct list, not the legacy string-list
`iptc.photo.personShownInTheImage`.

The map in `registry/mappings/cross-media-accessors.json` is the source of truth; a
contract test fails if a non-deferred row lacks a header accessor.

## Property reference

Every canonical property, its representations, cast rules, struct fields, and
cross-media accessor is in the generated
[property reference](properties/README.md). Those pages are built from the same
registry, overlay, cast, and accessor tables as `umm::describe` (C14a). Do not
edit them by hand.

- [Index](properties/README.md) — cross-media names, remaining canonical ids, cast-source keys
- [Photo properties](properties/photo.md)
- [Video properties](properties/video.md)
- [Base keys](properties/base-keys.md) — cast sources and frequent camera tags (`Make`, `Model`, exposure, lens)

## Base metadata

Entries as stored in a file are **base metadata** (a base key plus a base entry). Canonical
properties consume some of those entries as representations. An **unmapped** entry is one
that no canonical property consumed. RAW means camera image formats only, not this
vocabulary.

Not everything in a file maps to a canonical property — vendor MakerNotes, custom XMP
namespaces, niche container tags. libumm never invents a fake definition for these. Instead:

- `Metadata::dumpAll()` returns every base entry a read found, in source order, as
  `{family, key, value}` — for example `{"Exif", "Exif.Nikon3.LensType", …}` or
  `{"Xmp", "Xmp.vendor.SomeProperty", …}`. Values are textual; binary blobs are base64.
- `Metadata::dumpUnmapped()` returns only the base entries that no canonical property
  consumed. Until more ids are reconciled, this view still includes keys of properties
  that are not mapped yet.
- `Metadata::dumpValue(key)` looks up a single base entry.
- Writes preserve other base data: `umm::write` mutates only the representations of the
  properties you set and reports every representation it touched in
  `WriteReport::written`.
- Writing *new* base keys through libumm is intentionally not supported. If a property
  matters to your application, the right path is a registry mapping (it may already exist in
  a newer IPTC release), or use a backend tool such as ExifTool directly for one-off
  vendor-specific writes — libumm will still read the result and report it via
  `dumpAll()` / `dumpUnmapped()`.

## When a property will not write

Support is per backend, per file type, and per metadata category. If a write is not possible
for a file type (for example EXIF on PNG via Exiv2, or any embedded write on a read-only RAW
format), `umm::write` returns `unsupported_capability` — or, with
`StoragePolicy::preferred`, routes the data to an XMP sidecar when that is the recommended
storage. Check `umm::capabilities(media)` or
[docs/supported-types.md](../supported-types.md) first when scripting bulk writes.
