# Cross-Media Convenience Accessors — Phase 2 Proposal (revised)

## Overview

This document proposes expanding Phase 1's photo-only convenience accessors into a
**cross-media layer** that works identically across photos and video (and eventually
audio). The core principle: **a convenience accessor exists only when the concept is
defined for both the IPTC Photo Metadata standard and the IPTC Video Metadata Hub**
(`registry/iptc-photo/` and `registry/iptc-video/`).

This approach:

1. **Fulfills the intent of IPTC standardization** — the standards themselves define
   these properties for multiple media domains, proving they represent universal concepts
2. **Prevents API proliferation** — rather than separate `setPhotoCreator()`,
   `setVideoCreator()`, there is only `setCreator()`
3. **Future-proofs for audio** — when an audio domain is added, the same accessors extend
4. **Maintains architectural clarity** — media-specific properties stay behind full
   property IDs; universal concepts get convenient accessors

## Feasibility assessment (2026-09)

**Verdict: feasible.** Verified against the current codebase:

- **Both registries exist and are code-generated** (sessions 06–07, 20). The exact-name
  overlap between `iptc.photo.*` and `iptc.video.*` is **27 properties**, plus roughly a
  dozen more that share a concept under different names (see the catalog below).
- **Video read/write works** (sessions 20–22): MP4/MOV read is ExifTool-primary, video
  write is ExifTool-only, and `registry/capabilities/` records `xmp: read_write` for
  MP4/MOV — so even VMH properties without a QuickTime key have an XMP write path.
- **Media type detection already exists**: `umm::capabilities(path)` sniffs the container
  and returns `file_type` (e.g. `"MP4"`, `"MOV"`, `"JPEG"`), and
  `docs/reconciliation-policy.md` ("Domain selection") already routes sniffed video types
  to the `iptc.video.*` domain on read. Phase 2 only needs to surface that context on the
  `Metadata` object.
- **GPS is already cross-media**: `exif.gps.position` reconciles from and writes to
  QuickTime `GPSCoordinates` on video (session 22). The original draft wrongly excluded it.

**The two real costs the original draft understated:**

1. **The read/write pipeline, not the accessors, is the bulk of the work.** Today the
   reconciliation engine (`src/core/reconcile.cpp`) and write-sync
   (`src/core/write_sync.cpp`) handle only six `iptc.video.*` properties (title,
   description, creator, copyrightNotice, keywords, dateCreated) plus GPS. Every new
   cross-media accessor whose video property is not in that set needs reconcile + write-sync
   coverage first. `docs/reconciliation-policy.md` explicitly deferred a table-driven
   engine because Phase 1 video was "a closed Phase-1-sized set"; Phase 2 reopens that
   decision (see session 38).
2. **There is no CLI yet.** `docs/umm-cli-concept.md` is a concept document only. The
   original draft's "Update CLI to use cross-media accessors" checklist item is rewritten
   below as "align the CLI concept document"; building the CLI remains out of scope.

**Corrections to the original draft** (each verified against the registries):

| Original claim | Correction |
|---|---|
| `iptc.video.contributor` is "video-specific" | **Wrong.** `contributor` exists in both domains, both as `EntityWRole` struct lists — it is a Tier 1 candidate. |
| GPS excluded because "audio doesn't have GPS" | **Wrong under the photo+video rule.** `exif.gps.position` already works on video via QuickTime `GPSCoordinates`; `gps()`/`setGps()` are cross-media today. |
| `locationCreated` vs `locationShot` have "different roles, different structures" | **Wrong.** Both are `Location` struct lists (video merely lacks `gpsAltitudeRef`) and both mean "where the camera was" — a Tier 3 candidate. |
| `iptc.photo.keywords` (string list) vs `iptc.video.keywords` (lang-alt) | Correct on datatype, but both map to XMP `dc:subject`, so the transposition is well-defined (the reconcile engine already joins bag values, policy doc "keywords"). |
| Only 7 universal candidates | The registries yield **35+** (catalog below). |

## Design Principle: Universal Descriptive Concepts

Convenience accessors exist **only for properties where IPTC defines equivalent semantics
in both the Photo and Video standards.** This is not a libumm invention — the overlap in
the standards' vocabularies is itself the proof that these concepts are universal. libumm
never invents mappings; every accessor below cites the two registry property IDs it
delegates to.

## Cross-Media Accessor Catalog (Phase 2)

Candidates are grouped into three tiers by implementation cost. Datatypes and struct
names below are taken verbatim from `registry/iptc-photo/iptc-photo.json` (2025.1) and
`registry/iptc-video/iptc-video.json` (VMH 1.7).

### Tier 1 — Identical shape (direct pass-through)

Same property name, same datatype and cardinality in both domains. The accessor only
selects the domain-correct property ID.

| Accessor | Photo property | Video property | Shape (both) | Status |
|---|---|---|---|---|
| `title()` / `setTitle` | iptc.photo.title | iptc.video.title | lang-alt / one | new |
| `description()` / `setDescription` | iptc.photo.description | iptc.video.description | lang-alt / one | Phase 1 accessor becomes cross-media |
| `copyrightNotice()` / `setCopyrightNotice` | iptc.photo.copyrightNotice | iptc.video.copyrightNotice | lang-alt / one | Phase 1 accessor becomes cross-media |
| `creditLine()` / `setCreditLine` | iptc.photo.creditLine | iptc.video.creditLine | string / one | Phase 1 accessor becomes cross-media |
| `dateCreated()` / `setDateCreated` | iptc.photo.dateCreated | iptc.video.dateCreated | date-time / one | Phase 1 accessor becomes cross-media |
| `altTextAccessibility()` / `setAltTextAccessibility` | iptc.photo.altTextAccessibility | iptc.video.altTextAccessibility | lang-alt / one | new |
| `extendedDescriptionAccessibility()` / `setExtendedDescriptionAccessibility` | iptc.photo.extendedDescriptionAccessibility | iptc.video.extendedDescriptionAccessibility | lang-alt / one | new |
| `rightsUsageTerms()` / `setRightsUsageTerms` | iptc.photo.rightsUsageTerms | iptc.video.rightsUsageTerms | lang-alt / one | new |
| `sourceSupplyChain()` / `setSourceSupplyChain` | iptc.photo.sourceSupplyChain | iptc.video.sourceSupplyChain | string / one | new |
| `dataMining()` / `setDataMining` | iptc.photo.dataMining | iptc.video.dataMining | uri / one | new |
| `contributor()` / `setContributor` | iptc.photo.contributor | iptc.video.contributor | EntityWRole struct / many | new (draft wrongly excluded) |
| `genre()` / `setGenre` | iptc.photo.genre | iptc.video.genre | CvTerm struct / many | new |
| `embeddedEncodedRightsExpression()` / `set…` | iptc.photo.embeddedEncodedRightsExpression | iptc.video.embeddedEncodedRightsExpression | struct / many | new |
| `linkedEncodedRightsExpression()` / `set…` | iptc.photo.linkedEncodedRightsExpression | iptc.video.linkedEncodedRightsExpression | struct / many | new |
| `aiPromptInformation()` / `set…` | iptc.photo.aiPromptInformation | iptc.video.aiPromptInformation | string / one | new |
| `aiPromptWriterName()` / `set…` | iptc.photo.aiPromptWriterName | iptc.video.aiPromptWriterName | string / one | new |
| `aiSystemUsed()` / `set…` | iptc.photo.aiSystemUsed | iptc.video.aiSystemUsed | string / one | new |
| `aiSystemVersionUsed()` / `set…` | iptc.photo.aiSystemVersionUsed | iptc.video.aiSystemVersionUsed | string / one | new |
| `gps()` / `setGps` | exif.gps.position | exif.gps.position (QuickTime `GPSCoordinates` representation) | gps-coordinate | **already cross-media** (session 22) |

### Tier 2 — Same name, transposed shape

Same property name and semantics; datatype or cardinality differs, so the setter
transposes the caller's value into the domain shape (and the getter reports the stored
domain value). Transposition rules are per-accessor, documented, and lossless where
possible; lossy directions are flagged in the accessor docs.

| Accessor | Photo shape | Video shape | Transposition |
|---|---|---|---|
| `creator()` / `setCreator` | string / many | EntityWRole struct / many | name string ↔ `name` field of EntityWRole (Phase 1 video reconcile already does this) |
| `headline()` / `setHeadline` | string / one | lang-alt / one | string ↔ `x-default` entry |
| `keywords()` / `setKeywords` | string / many | lang-alt / one | both map to XMP `dc:subject`; bag ↔ joined `x-default` (existing reconcile rule) |
| `otherConstraints()` / `setOtherConstraints` | lang-alt / one | string / one | `x-default` entry ↔ string |
| `digitalSourceType()` / `setDigitalSourceType` | uri / one | CvTerm struct / one | IPTC digitalsourcetype CV URI ↔ CvTerm with that CV id |
| `modelReleaseStatus()` / `setModelReleaseStatus` | uri / one | CvTerm struct / one | same CV-URI ↔ CvTerm pattern |
| `propertyReleaseStatus()` / `setPropertyReleaseStatus` | uri / one | CvTerm struct / one | same CV-URI ↔ CvTerm pattern |
| `copyrightOwner()` / `setCopyrightOwner` | CopyrightOwner struct / many | EntityWRole struct / many | shared `name`/`identifiers` fields carry over; role only on video |
| `licensor()` / `setLicensor` | Licensor struct / many | Entity struct / one | shared `name`/`identifiers`; photo list ↔ video single (extra photo entries preserved only under full IDs) |

### Tier 3 — Different names, same concept

IPTC uses different property names but the concept, and usually the structure, match.
The accessor gets a media-neutral name and delegates to the domain property.

| Accessor | Photo property | Video property | Shapes |
|---|---|---|---|
| `locationCreated()` / `setLocationCreated` | iptc.photo.locationCreated | iptc.video.locationShot | Location struct / many (video Location lacks `gpsAltitudeRef`) |
| `locationShown()` / `setLocationShown` | iptc.photo.locationShownInTheImage | iptc.video.locationShown | Location struct / many |
| `personShown()` / `setPersonShown` | iptc.photo.personShownInTheImageWithDetails | iptc.video.personShown | PersonWDetails struct / many (identical struct) |
| `productShown()` / `setProductShown` | iptc.photo.productShownInTheImage | iptc.video.productShown | ProductWGtin ↔ ProductWGTIN struct / many (same fields) |
| `shownEvent()` / `setShownEvent` | iptc.photo.eventName + iptc.photo.eventIdentifier | iptc.video.shownEvent | lang-alt + uri list ↔ Entity struct / many (name+identifier transpose) |
| `registryEntry()` / `setRegistryEntry` | iptc.photo.imageRegistryEntry | iptc.video.registryEntry | RegistryEntry struct / many |
| `assetIdentifier()` / `setAssetIdentifier` | iptc.photo.digitalImageGuid | iptc.video.videoIdentifier | string / one |
| `aboutCvTerms()` / `setAboutCvTerms` | iptc.photo.cvTermAboutImage | iptc.video.cvTermAboutTheContent | CvTerm struct / many |
| `featuredOrganisation()` / `setFeaturedOrganisation` | iptc.photo.nameOfOrganisationFeaturedInTheImage | iptc.video.featuredOrganisation | string / many ↔ Entity struct / many (name transpose) |
| `supplier()` / `setSupplier` | iptc.photo.imageSupplier | iptc.video.supplier | ImageSupplier / many ↔ Entity / one (name+identifiers transpose) |
| `objectShown()` / `setObjectShown` | iptc.photo.artworkOrObjectInTheImage | iptc.video.objectShown | ArtworkOrObject / many ↔ Entity / many — **borderline**: photo struct is far richer; only `title`↔`name` transposes. Include last, or defer if review finds it too lossy. |

### ❌ Media-Specific (No Universal Accessor)

These remain behind full property IDs, with corrected rationale:

- **`iptc.photo.imageRating` vs `iptc.video.rating`** — genuinely different semantics: a
  numeric aesthetic rating vs an MPA-style authority classification (struct). The Phase 1
  photo-only `rating()` accessor stays photo-only and is documented as such.
- **`iptc.photo.jobId` vs `iptc.video.planningReference`** — workflow ID string vs a
  planning-reference struct; concept alignment is too weak.
- **`iptc.photo.modelReleaseId`/`propertyReleaseId` vs
  `iptc.video.modelReleaseDocument`/`propertyReleaseDocument`** — ID vs document link;
  deferred (the *status* pair is covered in Tier 2).
- **Video-only:** dateModified, dateReleased, episode/season/series, transcript,
  dopesheet, workflow properties, and all technical properties (bitrate, codec, frame
  rate, duration, aspect ratios…).
- **Photo-only:** instructions, descriptionWriter, webStatementOfRights, image region,
  legacy location fields, model-age fields, max-avail dimensions, scene/subject codes.

## Implementation Architecture

### Media context

`Metadata` gains an explicit media context (draft "Option A", refined):

- `enum class MediaDomain { photo, video, unknown };` with
  `mediaDomain()` / `setMediaDomain()` on `Metadata`.
- `umm::read()` sets it from the already-sniffed file type (the same domain-selection
  rule the reconcile engine applies today) — no new detection logic is invented.
- **Getters do not require context**: a cross-media getter probes the photo ID, then the
  video ID (a reconciled Metadata only ever populates one domain), so reads work even on
  hand-built objects.
- **Setters require context**: with `unknown` context a cross-media setter defaults to
  the photo property, preserving exact Phase 1 behavior for default-constructed
  `Metadata`. Users writing a fresh object destined for a video file call
  `setMediaDomain(MediaDomain::video)` first (or use full property IDs).

### Registry-driven mapping, not hand-written switches

The concept table (accessor → photo ID + video ID + transposition kind) is emitted by
`tools/registry/generate_cpp.py` from both registry JSON files into `src/generated/`,
keeping the "all property naming flows from the registry" contract and its tests intact.

### Pipeline before accessors

Reconcile (`src/core/reconcile.cpp`) and write-sync (`src/core/write_sync.cpp`) must
cover every video property an accessor touches, using the XMP representations from the
registry (all VMH properties carry one; QuickTime keys exist only for a few). This is
sequenced as its own session (38) before any accessor lands.

## CLI Concept Implications

There is no CLI yet; `docs/umm-cli-concept.md` is aligned so that when the CLI is built,
the same command works for photo and video:

```bash
umm set photo.jpg creator="Jane Doe" keywords="nature,landscape" dateCreated="2025-01-15"
umm set video.mp4 creator="Jane Doe" keywords="nature,landscape" dateCreated="2025-01-15"
```

## Phase Timeline

### Phase 1 (complete)
- ✅ Photo convenience accessors; video via full property IDs; GPS already cross-media.

### Phase 2 (this proposal)
Work is broken into session documents under `docs/implementation/` (numbering continues
from the completed sessions 01–35):

- [x] Session 36 — Media domain context on `Metadata`
- [x] Session 37 — Cross-media accessor map codegen from the registries
- [x] Session 38 — Video reconcile/write-sync generalization for the Phase 2 property set
- [x] Session 39 — Tier 1 pass-through accessors
- [x] Session 40 — Tier 2 transposing accessors
- [x] Session 41 — Tier 3 renamed-concept accessors
- [x] Session 42 — Cross-media verification, user guide, CLI-concept alignment

Shipped catalog: every non-deferred row in `registry/mappings/cross-media-accessors.json`.
**Dropped during implementation:** `objectShown` remains `deferred: true` (ArtworkOrObject ↔
Entity would keep only `title`↔`name`). `rating` stays photo-only; `gps()` is omitted from
the map because `exif.gps.position` is not an IPTC registry id.

### Phase 3+ (future)
- Audio domain when an IPTC audio standard (or agreed mapping) exists; accessors extend
  by adding an `audio` column to the generated map — no API redesign.

## Non-Goals

- No accessor that exists in only one media domain (defeats the purpose).
- No blurring of semantically distinct properties (rating, technical metadata).
- No invented mappings; only what the IPTC standards already define, cited by registry ID.
- No CLI implementation in Phase 2 (concept alignment only).

## Benefits

1. **User simplicity:** one accessor set across media types
2. **Standards alignment:** honors IPTC's intent to define universal descriptive metadata
3. **Future-proof:** audio support requires no API redesign
4. **Consistency:** no more `creator()` for photos but `get("iptc.video.creator")` for video
5. **Correctness:** only properties with true universal semantics get convenient names
6. **Discoverability:** media-specific properties are clearly behind full property IDs

## References

- `include/umm/metadata.hpp` — Phase 1 photo accessors (headers are normative)
- `src/core/property_ids.hpp` — internal property ID constants
- `src/core/reconcile.cpp`, `src/core/write_sync.cpp` — the pipeline sessions 38+ extend
- `docs/reconciliation-policy.md` — domain selection and existing video transpositions
- `registry/iptc-photo/iptc-photo.json` — IPTC Photo Metadata 2025.1 definitions
- `registry/iptc-video/iptc-video.json` — IPTC Video Metadata Hub 1.7 definitions
- `docs/developer/implementation-history.md` — sessions 01–35 summary
- `docs/umm-cli-concept.md` — CLI convenience accessor vision
