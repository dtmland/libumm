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
tables. A generated property reference (session 50) will render the same data.

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

## Full property reference

The tables below list every canonical property. The registry JSON in `registry/` is the
source of truth for definitions, cardinality, and standard mappings; use
`umm::Registry::property(id)` for the full definition at runtime.

### Photo properties — IPTC Photo Metadata 2025.1 (66 properties)

**Core 1.5**

| Property id | Name | Type |
|---|---|---|
| `iptc.photo.altTextAccessibility` | Alt Text (Accessibility) | lang-alt |
| `iptc.photo.cityLegacy` | City (legacy) | string |
| `iptc.photo.copyrightNotice` | Copyright Notice | lang-alt |
| `iptc.photo.countryCodeLegacy` | Country Code (legacy) | string |
| `iptc.photo.countryLegacy` | Country (legacy) | string |
| `iptc.photo.creator` | Creator | string, multi |
| `iptc.photo.creatorsContactInfo` | Creator's Contact Info | struct `CreatorContactInfo` |
| `iptc.photo.creatorsJobtitle` | Creator's jobtitle | string |
| `iptc.photo.creditLine` | Credit Line | string |
| `iptc.photo.dateCreated` | Date Created | date-time |
| `iptc.photo.description` | Description | lang-alt |
| `iptc.photo.descriptionWriter` | Description Writer | string |
| `iptc.photo.extendedDescriptionAccessibility` | Extended Description (Accessibility) | lang-alt |
| `iptc.photo.headline` | Headline | string |
| `iptc.photo.instructions` | Instructions | string |
| `iptc.photo.intellectualGenreLegacy` | Intellectual Genre (legacy) | string |
| `iptc.photo.jobId` | Job Id | string |
| `iptc.photo.keywords` | Keywords | string, multi |
| `iptc.photo.provinceOrStateLegacy` | Province or State (legacy) | string |
| `iptc.photo.rightsUsageTerms` | Rights Usage Terms | lang-alt |
| `iptc.photo.sceneCode` | Scene Code | string, multi |
| `iptc.photo.sourceSupplyChain` | Source (Supply Chain) | string |
| `iptc.photo.subjectCodeLegacy` | Subject Code (legacy) | string, multi |
| `iptc.photo.sublocationLegacy` | Sublocation (legacy) | string |
| `iptc.photo.title` | Title | lang-alt |

**Extension 1.9**

| Property id | Name | Type |
|---|---|---|
| `iptc.photo.additionalModelInformation` | Additional Model Information | string |
| `iptc.photo.aiPromptInformation` | AI Prompt Information | string |
| `iptc.photo.aiPromptWriterName` | AI Prompt Writer Name | string |
| `iptc.photo.aiSystemUsed` | AI System Used | string |
| `iptc.photo.aiSystemVersionUsed` | AI System Version Used | string |
| `iptc.photo.artworkOrObjectInTheImage` | Artwork or Object in the Image | struct `ArtworkOrObject`, multi |
| `iptc.photo.codeOfOrganisationFeaturedInTheImage` | Code of Organisation Featured in the Image | string, multi |
| `iptc.photo.contributor` | Contributor | struct `EntityWRole`, multi |
| `iptc.photo.copyrightOwner` | Copyright Owner | struct `CopyrightOwner`, multi |
| `iptc.photo.cvTermAboutImage` | CV-Term About Image | struct `CvTerm`, multi |
| `iptc.photo.dataMining` | Data Mining | uri |
| `iptc.photo.digitalImageGuid` | Digital Image GUID | string |
| `iptc.photo.digitalSourceType` | Digital Source Type | uri |
| `iptc.photo.embeddedEncodedRightsExpression` | Embedded Encoded Rights Expression | struct `EmbdEncRightsExpr`, multi |
| `iptc.photo.eventIdentifier` | Event Identifier | uri, multi |
| `iptc.photo.eventName` | Event Name | lang-alt |
| `iptc.photo.genre` | Genre | struct `CvTerm`, multi |
| `iptc.photo.imageCreator` | Image Creator | struct `ImageCreator`, multi |
| `iptc.photo.imageRating` | Image Rating | number |
| `iptc.photo.imageRegion` | Image Region | struct `ImageRegion`, multi |
| `iptc.photo.imageRegistryEntry` | Image Registry Entry | struct `RegistryEntry`, multi |
| `iptc.photo.imageSupplier` | Image Supplier | struct `ImageSupplier`, multi |
| `iptc.photo.imageSupplierImageId` | Image Supplier Image ID | string |
| `iptc.photo.licensor` | Licensor | struct `Licensor`, multi |
| `iptc.photo.linkedEncodedRightsExpression` | Linked  Encoded Rights Expression | struct `LinkedEncRightsExpr`, multi |
| `iptc.photo.locationCreated` | Location Created | struct `Location`, multi |
| `iptc.photo.locationShownInTheImage` | Location Shown in the Image | struct `Location`, multi |
| `iptc.photo.maxAvailHeight` | Max Avail Height | integer |
| `iptc.photo.maxAvailWidth` | Max Avail Width | integer |
| `iptc.photo.minorModelAgeDisclosure` | Minor Model Age Disclosure | uri |
| `iptc.photo.modelAge` | Model Age | integer, multi |
| `iptc.photo.modelReleaseId` | Model Release Id | string, multi |
| `iptc.photo.modelReleaseStatus` | Model Release Status | uri |
| `iptc.photo.nameOfOrganisationFeaturedInTheImage` | Name of Organisation Featured in the Image | string, multi |
| `iptc.photo.otherConstraints` | Other Constraints | lang-alt |
| `iptc.photo.personShownInTheImage` | Person Shown in the Image | string, multi |
| `iptc.photo.personShownInTheImageWithDetails` | Person Shown in the Image with Details | struct `PersonWDetails`, multi |
| `iptc.photo.productShownInTheImage` | Product Shown in the Image | struct `ProductWGtin`, multi |
| `iptc.photo.propertyReleaseId` | Property Release Id | string, multi |
| `iptc.photo.propertyReleaseStatus` | Property Release Status | uri |
| `iptc.photo.webStatementOfRights` | Web Statement of Rights | uri |

### Video properties — IPTC Video Metadata Hub 1.7 (105 properties)

**Descriptive**

| Property id | Name | Type |
|---|---|---|
| `iptc.video.altTextAccessibility` | Alt Text (Accessibility) | lang-alt |
| `iptc.video.cvTermAboutTheContent` | CV Term About the Content | struct `CvTerm`, multi |
| `iptc.video.dataDisplayedOnScreen` | Data Displayed on Screen | struct `TextWRegionDelimiter`, multi |
| `iptc.video.description` | Description | lang-alt |
| `iptc.video.dopesheet` | Dopesheet | lang-alt |
| `iptc.video.dopesheetLink` | Dopesheet Link | struct `QualifiedLink`, multi |
| `iptc.video.extendedDescriptionAccessibility` | Extended Description (Accessibility) | lang-alt |
| `iptc.video.featuredOrganisation` | Featured Organisation | struct `Entity`, multi |
| `iptc.video.genre` | Genre | struct `CvTerm`, multi |
| `iptc.video.headline` | Headline | lang-alt |
| `iptc.video.keywords` | Keywords | lang-alt |
| `iptc.video.language` | Language | string |
| `iptc.video.locationShot` | Location Shot | struct `Location`, multi |
| `iptc.video.locationShown` | Location Shown | struct `Location`, multi |
| `iptc.video.objectShown` | Object Shown | struct `Entity`, multi |
| `iptc.video.personHeard` | Person Heard | struct `Entity`, multi |
| `iptc.video.personShown` | Person Shown | struct `PersonWDetails`, multi |
| `iptc.video.productShown` | Product Shown | struct `ProductWGTIN`, multi |
| `iptc.video.shotType` | Shot Type | struct `Entity`, multi |
| `iptc.video.shownEvent` | Shown Event | struct `Entity`, multi |
| `iptc.video.snapshotLink` | Snapshot Link | struct `LinkedImage`, multi |
| `iptc.video.timedTextLink` | Timed Text Link | struct `QualifiedLinkWithLanguage`, multi |
| `iptc.video.title` | Title | lang-alt |
| `iptc.video.transcript` | Transcript | lang-alt |
| `iptc.video.transcriptLink` | Transcript Link | struct `QualifiedLink`, multi |
| `iptc.video.visualColour` | Visual Colour | string |

**Administrative**

| Property id | Name | Type |
|---|---|---|
| `iptc.video.aiPromptInformation` | AI Prompt Information | string |
| `iptc.video.aiPromptWriterName` | AI Prompt Writer Name | string |
| `iptc.video.aiSystemUsed` | AI System Used | string |
| `iptc.video.aiSystemVersionUsed` | AI System Version Used | string |
| `iptc.video.circaDateCreated` | Circa Date Created | string |
| `iptc.video.contentWarning` | Content Warning | struct `CvTerm`, multi |
| `iptc.video.dateCreated` | Date Created | date-time |
| `iptc.video.dateModified` | Date Modified | date-time |
| `iptc.video.dateReleased` | Date Released | date-time |
| `iptc.video.digitalSourceType` | Digital Source Type | struct `CvTerm` |
| `iptc.video.episode` | Episode | struct `EpisodeSeason` |
| `iptc.video.externalMetadataUrl` | External Metadata URL | uri, multi |
| `iptc.video.feedIdentifier` | Feed Identifier | string |
| `iptc.video.metadataAuthority` | Metadata Authority | struct `Entity` |
| `iptc.video.metadataEditDate` | Metadata Edit Date | date-time |
| `iptc.video.metadataEditor` | Metadata Editor | struct `Entity` |
| `iptc.video.parentVideoIdentifier` | Parent Video Identifier | string |
| `iptc.video.planningReference` | Planning Reference | struct `EntityWRole`, multi |
| `iptc.video.publicationEvent` | Publication Event | struct `PublicationEvent`, multi |
| `iptc.video.rating` | Rating | struct `Rating`, multi |
| `iptc.video.readyForRelease` | Ready for Release | boolean |
| `iptc.video.recordingDevice` | Recording Device | struct `Device` |
| `iptc.video.registryEntry` | Registry Entry | struct `RegistryEntry`, multi |
| `iptc.video.reviewRating` | Review Rating | struct `Rating`, multi |
| `iptc.video.season` | Season | struct `EpisodeSeason` |
| `iptc.video.series` | Series | struct `Series` |
| `iptc.video.storylineIdentifier` | Storyline Identifier | string, multi |
| `iptc.video.stylePeriod` | Style Period | string |
| `iptc.video.temporalCoverage` | Temporal Coverage | struct `TemporalCoverage` |
| `iptc.video.videoIdentifier` | Video Identifier | string |
| `iptc.video.videoRendition` | Video Rendition | string |
| `iptc.video.videoVersion` | Video Version | string |
| `iptc.video.workflowRating` | Workflow Rating | number |
| `iptc.video.workflowTag` | Workflow Tag | struct `CvTerm` |

**Rights**

| Property id | Name | Type |
|---|---|---|
| `iptc.video.contributor` | Contributor | struct `EntityWRole`, multi |
| `iptc.video.copyrightNotice` | Copyright Notice | lang-alt |
| `iptc.video.copyrightOwner` | Copyright Owner | struct `EntityWRole`, multi |
| `iptc.video.copyrightYear` | Copyright Year | number |
| `iptc.video.creator` | Creator | struct `EntityWRole`, multi |
| `iptc.video.creditLine` | Credit Line | string |
| `iptc.video.dataMining` | Data Mining | uri |
| `iptc.video.embeddedEncodedRightsExpression` | Embedded Encoded Rights Expression | struct `EmbdEncRightsExpr`, multi |
| `iptc.video.licensor` | Licensor | struct `Entity` |
| `iptc.video.linkedEncodedRightsExpression` | Linked Encoded Rights Expression | struct `LinkedEncRightsExpr`, multi |
| `iptc.video.modelReleaseDocument` | Model Release Document | string, multi |
| `iptc.video.modelReleaseStatus` | Model Release Status | struct `CvTerm` |
| `iptc.video.otherConstraints` | Other Constraints | string |
| `iptc.video.propertyReleaseDocument` | Property Release Document | string, multi |
| `iptc.video.propertyReleaseStatus` | Property Release Status | struct `CvTerm` |
| `iptc.video.rightsUsageTerms` | Rights Usage Terms | lang-alt |
| `iptc.video.sourceSupplyChain` | Source (Supply Chain) | string |
| `iptc.video.supplier` | Supplier | struct `Entity` |

**Time marker**

| Property id | Name | Type |
|---|---|---|
| `iptc.video.markers` | Markers | string |

**Technical**

| Property id | Name | Type |
|---|---|---|
| `iptc.video.audioBitrate` | Audio Bitrate | number |
| `iptc.video.audioBitrateType` | Audio Bitrate Type | string |
| `iptc.video.audioBitsPerSample` | Audio Bits per Sample | number |
| `iptc.video.audioChannelLayout` | Audio Channel Layout | string |
| `iptc.video.audioChannels` | Audio Channels | number |
| `iptc.video.audioCoding` | Audio Coding | struct `Entity` |
| `iptc.video.audioSampleRate` | Audio Sample Rate | number |
| `iptc.video.displayAspectRatio` | Display Aspect Ratio | string |
| `iptc.video.editorialDuration` | Editorial Duration | struct `VideoTime`, multi |
| `iptc.video.editorialDurationEnd` | Editorial Duration End | struct `VideoTime`, multi |
| `iptc.video.editorialDurationStart` | Editorial Duration Start | struct `VideoTime`, multi |
| `iptc.video.fileBitrate` | File Bitrate | number |
| `iptc.video.fileDuration` | File Duration | struct `VideoTime` |
| `iptc.video.fileFormat` | File Format | struct `Entity` |
| `iptc.video.frameSize` | Frame Size | struct `FrameSize` |
| `iptc.video.mediaType` | Media Type | string |
| `iptc.video.orientation` | Orientation | number |
| `iptc.video.signalAspectRatio` | Signal Aspect Ratio | string |
| `iptc.video.signalFormat` | Signal Format | string |
| `iptc.video.streamReady` | Stream-ready | string |
| `iptc.video.videoBitrate` | Video Bitrate | number |
| `iptc.video.videoBitrateType` | Video Bitrate Type | string |
| `iptc.video.videoCoding` | Video Coding | struct `Entity` |
| `iptc.video.videoFrameRate` | Video Frame Rate | number |
| `iptc.video.videoProfile` | Video Profile | string |
| `iptc.video.videoStreamsCount` | Video Streams Count | number |

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
