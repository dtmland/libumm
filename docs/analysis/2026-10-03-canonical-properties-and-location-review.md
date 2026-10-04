# Canonical properties, "most common properties", and location/GPS review — 2026-10-03

Status: **reviewed — outcomes recorded in §8; follow-up questions answered 2026-10-04**. Metadata as stored in the file is called *base* metadata here (C18). Analysis and design only. Nothing here has
been implemented. The maintainer accepted, modified, or superseded each **C*n*** decision;
§8 records the outcome. The casting design that replaces C2 and C4b/C4c is in the
follow-up decision record
[2026-10-03-casting-and-canonical-model-decisions.md](2026-10-03-casting-and-canonical-model-decisions.md).
Where §1–§6 below disagree with §8 or the follow-up record, the later text wins.
The notation follows [2026-09-27-plan-review-and-decisions.md](2026-09-27-plan-review-and-decisions.md).
New findings use the **C** prefix ("canonical").

This review answers a set of questions from a user reading [docs/user/guide.md](../user/guide.md):

1. Which properties are canonical? The guide names EXIF as a source, but the full
   property reference has no EXIF properties.
2. Does the guide's "most common properties" list correspond to anything in the code? Is
   it still worth keeping now that cross-media accessors exist?
3. Why does `umm::read` return `exif.gps.position`, which isn't in the canonical list?
   What decides which properties `read` returns at all?
4. Do `locationCreated` / `iptc.photo.locationCreated` / `iptc.video.locationShot` hold
   GPS coordinates? What is `Iptc4xmpExt`? Why do the docs steer users to
   `exif.gps.position` and not to these structures?

Every statement below was checked against the cited file. Statements that could not be
checked offline are marked **(verify)**.

---

## 1. Short answers

| Question | Answer |
|---|---|
| What is canonical today? | In practice, the 66 `iptc.photo.*` and 105 `iptc.video.*` registry ids, **plus one id libumm created itself, `exif.gps.position`**. That id is not in any registry: `Registry::find("exif.gps.position")` returns nothing, and the code special-cases it. No other EXIF property is canonical. |
| Are EXIF properties "embedded inside" IPTC properties? | **Partly true.** In most cases EXIF appears in libumm only as an alternative *representation* (another place the same value is stored) of an IPTC property. Examples: `IFD0:Artist` for `iptc.photo.creator`, and `DateTimeOriginal` for `iptc.photo.dateCreated`. Separately, the IPTC `Location` structure literally nests the EXIF XMP fields `exif:GPSLatitude`, `exif:GPSLongitude`, `exif:GPSAltitude` (and `exif:GPSAltitudeRef` on photo). So EXIF GPS does live inside IPTC location properties. |
| Is "most common properties" a code concept? | **Not by that name, but the set is real in code.** It matches the hand-written Phase 1 id list in `src/core/property_ids.hpp`, which is labeled "Canonical Phase 1 property ids", plus `title`. Those ids still have **hand-written special-case read and write paths** that bypass the table-driven engine. Your theory is right: the list is the original Phase 1 accessor set from before cross-media accessors. |
| Why does `read` return `exif.gps.position`? | `umm::read` reconciles exactly this set: the ids in the cross-media accessor map, plus `iptc.photo.imageRating` (photo only), plus `exif.gps.position`. GPS is added by hand. It is a reconciled value, **not** an unmapped entry. It only looks unmapped because it has no registry entry and doesn't appear in the full property reference. |
| Does `read` return every canonical property? | **No.** On photos it reconciles 40 of the 66 photo ids. On video it reconciles 38 of the 105 video ids. Every other id in the "Full property reference" can be **written** with `set()` but never comes back from `read` as a property. It appears only as base entries in `unmapped()`. |
| Do `locationCreated` / `locationShot` hold GPS? | **Yes, per the standards.** Both use the IPTC `Location` structure, which has GPS latitude, longitude, and altitude fields (plus altitude reference on photo). The registry imports these fields. **But the photo pipeline drops them.** Reading photo `locationCreated` keeps only `city` / `provinceState` / `countryName`. Writing it emits only the legacy Photoshop and IIM city, state, and country fields. Coordinates passed to `setLocationCreated` on a photo are silently lost. |
| What is `Iptc4xmpExt`? | Not a different or older standard. It is the **XMP namespace prefix for the IPTC Photo Metadata *Extension* schema** (`http://iptc.org/std/Iptc4xmpExt/2008-02-29/`; `Iptc4xmpCore` is the Core schema). libumm already uses it: `iptc.photo.locationCreated` is stored in XMP as `Iptc4xmpExt:LocationCreated`, and so is `iptc.video.locationShot`. |
| Is the search result's `Iptc4xmpExt:LocationCreated/Iptc4xmpExt:GPSLatitude` correct? | **The structure is right; the field namespace is wrong.** Both standards put the GPS fields in the **EXIF** namespace inside the structure: `Iptc4xmpExt:LocationCreated/exif:GPSLatitude`. The `Iptc4xmpExt:GPSLatitude` form probably comes from tools that show struct fields under the `XMP-iptcExt` group (ExifTool shows them as `LocationCreatedGPSLatitude`) **(verify)**. Video Metadata Hub does reuse the photo Extension namespace, so `Iptc4xmpExt:LocationCreated` (not `LocationShot`) is the correct XMP property for VMH Location Shot. |

---

## 2. Evidence

### 2.1 What "canonical" means in the code

- The guide's opening line names three sources, "IPTC Photo Metadata, IPTC Video Metadata
  Hub, EXIF" (`docs/user/guide.md:4-6`). Its id list adds `exif.gps.position` as a
  "well-known GPS coordinate value (EXIF domain)" (`docs/user/guide.md:44-46`). The "Full
  property reference" says it lists "every canonical property" but has only photo and video
  tables (`docs/user/guide.md:161-163`, `:165`, `:243`).
- The registry holds **only** IPTC Photo 2025.1 (66 properties, 18 structs) and VMH 1.7
  (105 properties, 25 structs) (`registry/iptc-photo/iptc-photo.json` and
  `registry/iptc-video/iptc-video.json` `counts`). There is no `registry/exif/`.
- `exif.gps.position` is a hard-coded constant (`src/core/property_ids.hpp:18`). It gets
  special handling because the registry can't answer for it:
  `Metadata::set` type-checks it by hand (`src/metadata.cpp:20-27`), and the cross-media map
  leaves it out because "that id is not in the IPTC registries"
  (`registry/mappings/cross-media-accessors.json:6`).
- `Datatype::gps_coordinate` exists in the public registry API (`include/umm/registry.hpp:24`),
  but no `PropertyDef` uses it.
- The deferral is recorded but was never closed: "Well-known Phase 1 id until an
  EXIF-domain registry exists (session 08)" (`docs/reconciliation-policy.md:199`;
  `include/umm/metadata.hpp:2-4`; `docs/developer/implementation-history.md:40`).
- EXIF otherwise appears only as a **representation** of IPTC properties. Five photo
  properties carry an `exif` tag: `creator`, `description`, `copyrightNotice`,
  `dateCreated`, and `digitalImageGuid`. No video property does. The curated overlay
  `registry/mappings/iptc-exif-overlay.json` also maps
  `iptc.photo.struct.Location.gpsLatitude/gpsLongitude/gpsAltitude/gpsAltitudeRef` to the
  EXIF GPS IFD tags. **Codegen skips those struct-field rows on purpose**: "Struct-field GPS
  tags are recorded for Stage 4/6; they are not top-level PropertyDef rows"
  (`tools/registry/generate_cpp.py:186-189`). So the IPTC→EXIF GPS bridge exists in data but
  is not used.

**Conclusion:** your theory holds. In practice, canonical means "registry id". EXIF
contributes no canonical vocabulary of its own except the one GPS id libumm created. The
guide's "(… EXIF)" wording and its "every canonical property" table contradict each other,
and the code reflects the same gap (a special-cased id outside the registry).

### 2.2 Where "most common properties" came from

The table at `docs/user/guide.md:51-71` lists creator, title, headline, description,
keywords, dateCreated, copyrightNotice, creditLine, imageRating, `exif.gps.position`, and
locationCreated. Compare `src/core/property_ids.hpp:7-18`:

```
// Canonical Phase 1 property ids. One definition for reconcile, write-sync,
// and Metadata accessors (review R3).
kCreator, kDescription, kHeadline, kDateCreated, kCopyright, kCredit,
kKeywords, kRating, kLocation, kGps
```

That is the same set, minus `title`. The table is the Phase 1 accessor set. It traces back
to the concept doc's `getDescription()/getCreator()/getCaptureTime()/getLocation()/setRating()`
pitch (`docs/analysis/concept.md:1228-1234`).

How the set still shows up in code:

- **Write path.** `write_sync` checks the Phase 1 ids first, with hand-written writers:
  `sync_rating`, `sync_gps`, and `sync_location`. Only after that does it fall through to
  the table-driven `sync_video_generic` (`src/core/write_sync.cpp:607-641`).
- **Read path.** `kLocation` has its own reconcile branch (`src/core/reconcile.cpp:1425-1460`),
  and `kGps` has `collect_gps` (`:1563-1597`). `kRating` and `kGps` are appended to the
  photo read list by hand (`src/core/xmp_codec.cpp:621-634`).
- **The table itself is stale.** It says `title` has only `get`/`set`, but `title()` /
  `setTitle()` exist (`include/umm/metadata.hpp:70,112`).

Of the 11 rows, 9 are cross-media accessors. The other two are:

- **`iptc.photo.imageRating`.** Kept photo-only because Phase 2 compared it with
  `iptc.video.rating` (an MPA-style authority classification struct)
  (`docs/analysis/phase-2-video-convenience-accessors.md:146-148`). **The review missed a
  better match.** `iptc.video.workflowRating` is a `number` mapped to the **same XMP
  property**, `xmp:Rating`, as `iptc.photo.imageRating`. VMH defines it as "An aesthetic
  rating given to the video by its creator or editor". Today `setRating` on a video
  writes `Xmp.xmp.Rating` (`src/core/write_sync.cpp:299-305`), but `read` on a video never
  reconciles `workflowRating`, because it isn't in the map.
- **`exif.gps.position`.** It is cross-media in behavior, since video reads and writes
  QuickTime `GPSCoordinates` (`src/core/reconcile.cpp:1570-1588`,
  `src/core/write_sync.cpp:307-335`). It is left out of the map only because it has no
  registry id. The guide's cross-media section says so (`docs/user/guide.md:83-85`).

### 2.3 What `umm::read` returns, and why

`internal::reconcile` stores **every base entry** with `assignUnmapped`, then reconciles
these lists (`src/core/reconcile.cpp:1897-1921`):

- **photo:** every non-deferred photo id in the cross-media map, plus
  `iptc.photo.imageRating`, plus `exif.gps.position` (`src/core/xmp_codec.cpp:621-634`);
- **video:** every non-deferred video id in the map, plus `exif.gps.position`
  (`src/core/xmp_codec.cpp:607-619`).

So **the cross-media accessor map decides what `read` returns**. That was a sequencing
choice in Phase 2: the pipeline was widened only for ids that accessors needed
(`docs/analysis/2026-09-30-table-driven-video-pipeline.md:10-11`). Ids that `read` never
returns as properties:

- **Photo (26):** `additionalModelInformation`, `artworkOrObjectInTheImage`, `cityLegacy`,
  `codeOfOrganisationFeaturedInTheImage`, `countryCodeLegacy`, `countryLegacy`,
  `creatorsContactInfo`, `creatorsJobtitle`, `descriptionWriter`, `imageCreator`,
  `imageRegion`, `imageSupplierImageId`, `instructions`, `intellectualGenreLegacy`, `jobId`,
  `maxAvailHeight`, `maxAvailWidth`, `minorModelAgeDisclosure`, `modelAge`,
  `modelReleaseId`, `personShownInTheImage`, `propertyReleaseId`, `provinceOrStateLegacy`,
  `sceneCode`, `subjectCodeLegacy`, `sublocationLegacy`, `webStatementOfRights`.
- **Video (67):** every VMH id outside the map, including `workflowRating`,
  `dateModified`, the episode/series fields, and all technical properties.

This creates two asymmetries:

1. `set("iptc.photo.instructions", …)` validates against the registry and writes through
   `sync_video_generic` (`src/core/write_sync.cpp:638-641`). A following `read` does not
   return `iptc.photo.instructions`.
2. `Metadata::unmapped()` is documented as "every unmapped entry a read found"
   (`docs/user/guide.md:380-382`), but it actually holds **all** base entries, mapped and
   unmapped (`src/core/reconcile.cpp:1905`, `src/metadata.cpp:653-655`). This may be why
   `exif.gps.position` *looked* unmapped: its base `Exif.GPSInfo.*` keys also appear in
   `unmapped()`.

Note on "the read command": the `umm` CLI is still a concept
(`docs/umm-cli-concept.md:3-7`). Its `umm read` is specified as "dump all canonical
metadata", backed by `umm::read` (`docs/umm-cli-concept.md:32,45`). A CLI built on today's
library would therefore show exactly the reconciled set above. That is "canonical only"
already, with `exif.gps.position` as the one non-registry id, but it is not "all
canonical".

### 2.4 Location and GPS in the standards and in libumm

**Standards (vendored sources):**

- IPTC Photo TR 2025.1 `Location` struct fields: `city`, `countryCode`, `countryName`,
  `gpsAltitude`, `gpsAltitudeRef`, `gpsLatitude`, `gpsLongitude`, `identifiers`
  (`Iptc4xmpExt:LocationId`), `name` (`Iptc4xmpExt:LocationName`), `provinceState`,
  `sublocation`, `worldRegion`. The four GPS fields use the XMP namespace
  `http://ns.adobe.com/exif/1.0/` (`exif:GPSLatitude` …). Source:
  `registry/sources/iptc-pmd-techreference_2025.1.json:2034-2052`.
- VMH 1.7 `Location` struct has the same fields **without** `gpsAltitudeRef`, also as
  `exif:GPS*`. `name`, `sublocation`, `city`, `provinceState`, `countryName`, and
  `worldRegion` are lang-alt (`registry/iptc-video/iptc-video.json`; VMH props/AppleQT HTML
  in `registry/sources/vmh/`).
- VMH Location Shot → XMP `Iptc4xmpExt:LocationCreated`; Location Shown →
  `Iptc4xmpExt:LocationShown`. The AppleQT mapping uses "location structure +
  `com.apple.quicktime.location.role=0`" for Shot and `…location.name` for the name. It
  lists **no QuickTime key for the GPS fields**, so linking QuickTime `GPSCoordinates` to
  Location Shot is an inference, not a VMH mapping.
- IPTC TR `mapping_notes` on the legacy fields (`cityLegacy` etc.): the legacy fields have
  "blurred semantics", and Location Created / Location Shown are "two more concise
  properties".

**libumm today:**

| Path | Photo (`iptc.photo.locationCreated`) | Video (`iptc.video.locationShot`) |
|---|---|---|
| Read | `structured_location` reads `Iptc4xmpExt:LocationCreated`, but `put_location_field` keeps only `city`, `provinceState`, `countryName`. GPS, `name`, `sublocation`, `countryCode`, `worldRegion`, and `identifiers` are **dropped**. It falls back to the legacy `photoshop:City/State/Country` and IIM `2:90/2:95/2:101` (`src/core/reconcile.cpp:1232-1246`, `:1425-1460`). | Generic struct-list decode keeps whatever field names the backend emits, so GPS fields survive as backend-named keys (`src/core/reconcile.cpp:1543-1549`). |
| Write | `sync_location` writes **only** `photoshop:City/State/Country` + IIM City/ProvinceState/CountryName. It never writes `Iptc4xmpExt:LocationCreated` (`src/core/write_sync.cpp:557-570`). | `sync_video_generic` writes `Iptc4xmpExt:LocationCreated` as ExifTool `{Field=value}` structs using the caller's field names (`src/core/write_sync.cpp:476-492`). There is no Location alias table, so `name`→`LocationName` and `identifiers`→`LocationId` are not translated. Whether ExifTool accepts the lowercase IPTC logical names is **(verify)**. Tests use only `City` / `city` (`tests/backend/test_write.cpp:1106-1108`, `tests/backend/test_cross_media.cpp:108-109`). |
| Docs | The guide calls it "Named place where the image was created" (`docs/user/guide.md:68`) and admits "Photo `locationCreated` still writes the Phase 1 named-place path (photoshop/IIM city)" (`:152`). | Listed as Tier 3, with `gpsAltitudeRef` dropped (`docs/user/guide.md:98,141`). |

Other inconsistencies:

- `docs/reconciliation-policy.md:230-232` says write-sync emits "structured LocationCreated
  when the writer can". **The code never does this on photo.**
- Photo `locationCreated` writes the **same base keys** as three other canonical ids:
  `iptc.photo.cityLegacy` (`photoshop:City` / IIM 2:90), `provinceOrStateLegacy`, and
  `countryLegacy`. Setting both a legacy id and `locationCreated` yields two writers for one
  key.
- The XMP spec encodes `exif:GPSLatitude` / `exif:GPSLongitude` as a GPSCoordinate *string*
  (`DDD,MM.mmk`), but the IPTC TR lists the struct fields as `number`. A real GPS-in-Location
  path needs a defined conversion **(verify against the XMP spec part 2)**.
- Struct field naming isn't standardized. The photo path stores IPTC logical names
  (`city`); the video path stores whatever ExifTool emits (`City`, `PersonName`). Tests
  use both.

**Why the docs point to `exif.gps.position` rather than Location GPS.** That choice was
correct for the common case. Cameras and phones write the **EXIF GPS IFD** (photos) or
QuickTime `GPSCoordinates` (video). They almost never write coordinates inside
`Iptc4xmpExt:LocationCreated`. So `gps()` reads what files actually contain. But the guide
never explains that IPTC Location structures can carry coordinates too, or how the two
relate. And the photo pipeline cannot round-trip them at all.

---

## 3. Findings and proposed decisions

### Finding C1 — "Canonical" is defined in three inconsistent places

The guide prose (IPTC + VMH + EXIF), the guide reference ("every canonical property",
IPTC + VMH only), and the code (registry ids + one hard-coded id) disagree.

- **C1 — Definition of canonical.** Proposed: **a canonical property is exactly a registry
  id**, i.e. something `umm::Registry::find` returns. Anything `Metadata` accepts must be
  findable there, with its source standard. The guide's reference section must list
  every canonical id, so the GPS id must either join the registry (C2) or stop being
  called canonical.

### Finding C2 — `exif.gps.position` is canonical by fiat and outside the registry

| Option | Description | Assessment |
|---|---|---|
| 1. Keep it as a documented pseudo-id | Leave the code as is; fix the docs to call it "libumm well-known composite, not a registry id". | Cheapest. Leaves the C1 rule broken and keeps special cases in `metadata.cpp`, reconcile, write-sync, and the map. |
| 2. EXIF-domain registry entry | Add a small, source-cited `registry/exif/` (EXIF 2.32 / CIPA DC-008 GPS IFD tags 0x0001–0x0006; ExifTool/XMP-exif representations). Generate `exif.gps.position` (datatype `gps_coordinate`) from it, so `Registry::find` answers and the special cases collapse into table rows. | Closes the session 08 deferral. There's no machine-readable EXIF TR, so curation would follow the `iptc-exif-overlay.json` precedent: hand-curated, cited, `partial`. The composite (lat+lon+alt) is a representation choice, not a new meaning. Every field comes from EXIF. |
| 3. Fold GPS into IPTC Location | Drop `exif.gps.position`. Treat EXIF GPS IFD / XMP `exif:GPS*` / QuickTime `GPSCoordinates` as representations of `locationCreated[0].gps*` / `locationShot[0].gps*`, using the overlay rows that already exist. | Most standards-pure. But Location Created is a multi-valued, name-centric struct. One file can have a named place without coordinates, coordinates without a place, or several places. Merging them changes reconcile semantics and breaks `gps()` / `setGps()` / `umm geotag` users. |

**Proposed C2: option 2**, plus an explicit, documented *relationship* to Location GPS
(C4). Option 3 is not recommended. It would merge two concepts that IPTC itself keeps
apart: camera position as recorded by the device, and editorial location structures.

### Finding C3 — "Most common properties" is a Phase 1 remnant

The cross-media accessor tables supersede it. It contains one stale row (`title`), one
photo-only row (`rating`), and one non-registry row (`gps`). The underlying Phase 1 set
still exists in code as special-case branches.

- **C3a — Docs.** Proposed: **remove the "most common properties" section.** Replace it
  with (a) a pointer to the cross-media accessor tables, and (b) a short "Location:
  coordinates vs places" section (C4). Update `docs/README.md`, which advertises "most
  common first". Every accessor already appears in a Tier table or in the GPS/rating
  notes, so nothing is lost.
- **C3b — Code.** There is no "most common" type or table to delete. `property_ids.hpp`
  should eventually shrink to ids that truly need special handling. Each remaining
  special case should name the reconciliation-policy section that justifies it:
  - `dateCreated` (EXIF/IIM composites)
  - `keywords` (bag/IIM repeat)
  - `creator` (IIM/EXIF joins)
  - GPS (multi-tag composite)

  `rating` and `locationCreated` are candidates to move onto the table-driven path (C4,
  C5). Proposed as follow-up, not urgent.
- **C3c — Rating.** Re-open the Phase 2 rating decision. `iptc.photo.imageRating` and
  `iptc.video.workflowRating` are both `number` and both map to `xmp:Rating`. Their
  definitions line up: the IPTC user guide calls Image Rating a user/supplier rating, and
  VMH calls workflowRating "aesthetic rating by creator or editor". Proposed: make
  `rating` a **Tier 1 cross-media accessor (imageRating ↔ workflowRating)** and add a map
  row. `iptc.video.rating` and `reviewRating` stay video-only. If maintainers reject the
  semantic match, `rating()` stays photo-only, and C3a still removes it from any "common"
  list.

### Finding C4 — Location structures support GPS in the standards, not in libumm photo paths

Photo `locationCreated` loses everything except city, state, and country. It reads and
writes the legacy fields under the Extension property's id. The policy doc overstates
write support.

- **C4a — Full Location struct on photo.** Proposed: reconcile and write
  `iptc.photo.locationCreated` / `locationShownInTheImage` as full `Location` structs on
  `Iptc4xmpExt:LocationCreated/LocationShown`, with every TR field including
  `exif:GPS*`. This goes through the same table-driven struct path video uses, plus a
  `Location` field-alias table (`name`↔`LocationName`, `identifiers`↔`LocationId`,
  `gps*`↔`GPS*`) and the GPSCoordinate string↔number conversion.
- **C4b — Legacy fields.** Proposed: stop treating `photoshop:City/State/Country` and IIM
  `2:90/2:95/2:101` as representations of `locationCreated`. They are already canonical in
  their own right (`cityLegacy`, `provinceOrStateLegacy`, `countryLegacy`,
  `sublocationLegacy`, `countryCodeLegacy`). Keep them reconcilable under those ids (C5).
  Back-filling `locationCreated` from legacy fields (or the reverse) should be an explicit,
  documented policy choice, not the default. IPTC itself says the legacy semantics are
  "blurred". **Decision needed:** whether to keep a read-only legacy fallback for
  `locationCreated` for Phase 1 compatibility.
- **C4c — GPS ↔ Location relationship.** Proposed: keep `exif.gps.position` and
  `locationCreated[*].gps*` as **separate canonical values with no automatic
  cross-filling**. Document that:
  - EXIF GPS IFD = device position;
  - Location Created GPS = an editorial statement of where the image was created;
  - the IPTC Mapping Guidelines relate the two (overlay rows exist).

  A later, opt-in helper (for example in `umm geotag`) could copy one into the other.
  **(verify)** the exact Mapping Guidelines wording before citing it in user docs. The
  overlay records the struct-field ↔ GPS-tag rows, but does not say whether they belong to
  Location Created or Location Shown.
- **C4d — Docs.** Document the GPS fields explicitly next to the GPS accessor in the guide:
  "IPTC Location structures can also carry coordinates (`gpsLatitude`, `gpsLongitude`,
  `gpsAltitude`; photo also `gpsAltitudeRef`; XMP `Iptc4xmpExt:LocationCreated/exif:GPSLatitude`)".
  Add a struct-field reference for `Location`, and a glossary entry for `Iptc4xmpCore` /
  `Iptc4xmpExt`. This should land together with C4a, so the docs don't promise behavior
  that doesn't exist yet.

### Finding C5 — `read` coverage is coupled to the accessor map

`read` returns 40/66 photo and 38/105 video canonical ids. Every other id can be written but
does not read back.

- **C5 — Read surface.** Proposed: `umm::read` reconciles **every registry id for the file's
  domain**, plus `exif.gps.position` (or its C2 registry row). Use the table-driven
  engine that already handles video and photo Tier 1–3 shapes. Special cases are limited
  to the C3b list. The accessor map then decides **only** which ids get short names. It
  no longer decides what `read` returns.
- **On "should read return only canonical fields, with emphasis on cross-media
  accessors?"** `umm::read` already returns only canonical ids as properties. Base data
  stays separate in `unmapped()`. Keep that split. Proposed emphasis lives in
  *presentation* only. When built, the CLI's `umm read` would print every reconciled
  property and show the accessor name next to ids that have one (`creator  iptc.photo.creator`),
  with optional grouping (cross-media first, then domain-only). `read` should not drop
  domain-only ids such as `instructions` or `workflowRating`. Hiding standard data defeats
  "write metadata once, use it everywhere", and `umm get` already covers name-based
  lookup.

### Finding C6 — `unmapped()` holds all base entries, not just unmapped ones

The guide and header describe `unmapped()` as unmapped-only. In the code it is the full base
document.

- **C6 — Options:** (1) filter out base keys that any reconciled group consumed; or (2) keep
  the full dump and rename/redocument it as the base view. Proposed: **(1)** once C5 lands,
  because then "consumed" is well defined for every registry id. Until then, at minimum fix
  the guide wording. This is an observable behavior change, so it needs a
  `docs/developer/release-notes.md` entry.

---

## 4. Decision summary

The "Outcome" column was added after maintainer review. §8 gives the details.

| ID | Decision | Proposed outcome | Outcome |
|---|---|---|---|
| C1 | Definition of canonical | Canonical = registry id; the guide reference lists all of them | **Accepted** |
| C2 | `exif.gps.position` status | Option 2: curated, cited EXIF-domain registry entry (`gps_coordinate`); option 3 rejected | **Superseded.** The id is removed (pre-release). GPS becomes a representation of Location GPS or a cast source. See C8 in the follow-up record |
| C3a | "Most common properties" docs | Remove; point to cross-media tables + new location section | **Accepted** |
| C3b | Phase 1 special-case ids in code | Shrink to policy-justified composites; follow-up | **Accepted.** The target is no id-specific branches; composite codecs become registry-driven |
| C3c | Rating | Re-open: Tier 1 `imageRating` ↔ `workflowRating` (both `xmp:Rating`), pending semantic sign-off | **Accepted.** Tier 1 `rating` accessor |
| C4a | Photo Location structs | Full `Location` struct incl. GPS on `Iptc4xmpExt:*`, with field aliases + GPS string↔number | **Accepted.** Prerequisite for casting |
| C4b | Legacy city/state/country | Reconcile under their own legacy ids; legacy fallback for `locationCreated` needs a decision | **Modified.** Legacy ids own their keys; any link to a Location struct is a *side cast* (C11). The partner is Location**Shown**, following MWG (§8.4; decided as OQ2 in the follow-up record) |
| C4c | GPS ↔ Location GPS | Separate values, no implicit cross-fill; documented relationship; optional helper later | **Superseded** by the representation-vs-cast rule (C7) and GPS decision (C8) |
| C4d | Location docs | Document Location GPS fields, `Iptc4xmpExt` / `Iptc4xmpCore`, alongside C4a | **Accepted**; folded into the generated property reference (C14) |
| C5 | `read` coverage | Reconcile every registry id for the domain; accessor map controls names only | **Accepted**, without the `exif.gps.position` addition |
| C6 | `unmapped()` semantics | Filter consumed base keys after C5; fix the docs now | **Modified.** Replace with `dumpAll()` and `dumpUnmapped()` (C13); the backend vocabulary becomes `Base*` and "raw" is not used (C18) |

## 5. Suggested sequencing (once accepted)

> Superseded by the sequencing in the follow-up decision record (§ "Sequencing").
> Kept for history.

No session docs are written yet. If the decisions are accepted, a natural order is:

1. **Docs-only correction.** Guide sections "Canonical property names", "most common", and
   `unmapped()`; reconciliation-policy §locationCreated write claim. No behavior change.
2. **C5 read coverage**, using the existing table-driven engine. This turns the remaining
   photo and video ids into reconciled properties, including the legacy location ids (C4b).
3. **C4a/C4b photo Location structs**, plus field-alias and GPS-string handling. The
   `locationCreated` legacy write/fallback changes here.
4. **C2 EXIF-domain registry row for GPS.** Collapse the `kGps` special cases into
   generated data.
5. **C3c rating map row** (if accepted), then **C6 unmapped filtering**. Both get
   release-notes entries for the behavior change.
6. **C4d user docs** for Location GPS, after step 3 ships.

Each step must update the normative headers in `include/umm/` first where the public
contract changes (`metadata.hpp` comments about rating, GPS, and location; `registry.hpp`
if C2 adds a standard to `Registry::standards()`).

## 6. Open questions for the maintainer

> Answered. Q1: superseded (the GPS id is removed, C8). Q2: yes (C3c). Q3: replaced by
> the side-cast question in the follow-up record (OQ2). Q4: yes (C5).

1. C2: is a hand-curated, cited, `partial` EXIF registry acceptable under S2
   ("registry-first, never invent definitions"), given there is no machine-readable EXIF TR?
2. C3c: do you accept `iptc.photo.imageRating` ↔ `iptc.video.workflowRating` as the same
   concept?
3. C4b: should `locationCreated` keep a **read-only** fallback to legacy
   city/state/country for files that have only legacy fields? IPTC's own guidance is that
   the legacy fields are ambiguous between "created" and "shown".
4. C5: is the larger reconciled set (and more conflict reports) acceptable for existing
   callers?

## 7. References

- `docs/user/guide.md` — canonical names, most-common table, cross-media tiers, full reference, unmapped
- `include/umm/metadata.hpp`, `include/umm/registry.hpp` — normative API
- `src/core/property_ids.hpp`, `src/metadata.cpp`, `src/core/xmp_codec.cpp`,
  `src/core/reconcile.cpp`, `src/core/write_sync.cpp` — current behavior
- `registry/iptc-photo/iptc-photo.json`, `registry/iptc-video/iptc-video.json`,
  `registry/mappings/cross-media-accessors.json`, `registry/mappings/iptc-exif-overlay.json`
- `registry/sources/iptc-pmd-techreference_2025.1.json`, `registry/sources/vmh/` — vendored standards
- `tools/registry/generate_cpp.py` — overlay handling
- `docs/reconciliation-policy.md`, `docs/analysis/phase-2-video-convenience-accessors.md`,
  `docs/analysis/2026-09-30-table-driven-video-pipeline.md`, `docs/umm-cli-concept.md`,
  `docs/supported-types.md` §3

---

## 8. Review outcome (added after maintainer review)

This section records the discussion after the first draft. It covers a second, independent
review, the questions raised while comparing the two, and the maintainer's answers. The
decisions that follow from it are numbered **C7–C17** in
[2026-10-03-casting-and-canonical-model-decisions.md](2026-10-03-casting-and-canonical-model-decisions.md).

### 8.1 Insights from the alternate review

A second analysis of the same questions exists on branch
`copilot/analysis-document-canonical-fields-again` (same file name, no decision ids). It
reaches the same conclusions on C1, C3, and C5. It adds five points this review missed:

| # | Point | Outcome |
|---|---|---|
| 1 | `exif.gps.position` is a stable public id with tests and docs. Silently removing it is an API break. | libumm is pre-release, so the id **is removed**. Headers are updated first and `docs/developer/release-notes.md` gets an entry (C8). |
| 2 | A video can have several shot locations, so one device position cannot map onto "the" location. | List↔single rule: **use the first entry** (C10). |
| 3 | `GpsCoordinate.gps_time` (UTC from `GPSDateStamp`/`GPSTimeStamp`) has no field in the Location struct and would be lost. | Accepted as a loss. Base GPS time stays visible in `dumpAll()`/`dumpUnmapped()`. The wider time-field picture is in C16. |
| 4 | [concept.md](concept.md) "Domain C — EXIF / camera technical metadata" planned EXIF as its own canonical domain. | The maintainer chose to **depart from the original plan explicitly** (C17). |
| 5 | Round-trip claims for Location GPS must be tested against real backends, not inferred from the standard. | Accepted: "test all the things" (C15). |

This review adds one more point that neither draft made:

- **ExifTool field names are already in the vendored standard.** The IPTC TR source
  (`registry/sources/iptc-pmd-techreference_2025.1.json`) carries an `etTag` (ExifTool tag
  name) for structure fields, 99 entries in total, for example `CiAdrCity`. The importer
  drops them. Today `alias_exiftool_struct_fields` in `src/core/write_sync.cpp:346` is a
  hand-written list that translates IPTC field names into ExifTool's. Keeping `etTag` in
  the registry would let the codegen produce that list. This fits the project direction:
  backend names become **generated data from the standard's own file**, the same as the
  XMP names already are (S2 registry-first; the codegen contract in
  `tests/build/test_codegen.py`). Recorded as C14 (part b).

### 8.2 Questions raised and answers

| Q | Question | Answer |
|---|---|---|
| Q1 | Where is the line between a *representation* (automatic) and a *cast* (opt-in)? (a) A key is a representation when a standard defines it; anything libumm adds is a cast. (b) Only IPTC/VMH XMP keys are representations; every EXIF/IIM/QuickTime key is a cast. | **(a).** Representation always wins over casting when a standard defines the link. (b) would make every camera JPEG read as empty until upcast, because `dateCreated` would no longer come from `DateTimeOriginal`. See C7 for consequences. |
| Q2 | Are the date rows right? | Drop `Exif.Image.DateTime` as a `dateCreated` candidate; it is EXIF *ModifyDate*. `xmp:CreateDate` is the *digitized* date. Docs must carry this level of detail, in a more formal API-reference layout (C14). |
| Q3 | Is a cast a single key or a bundle? | A **cast group**: an atomic bundle such as `capturePosition` (lat, lon, alt, refs) with prioritized sources (C9). |
| Q4 | What does "empty" and "equal" mean for a cast target? | Field-level emptiness; equality within the reconcile tolerances (1e-5°, 0.5 m). For a list target, use the first entry (C10). |
| Q5 | Which statuses does a cast report? | can cast, equal, needs force, source empty, target not storable (C9). |
| Q6 | Where does casting logic live? | In libumm. The CLI wraps it (C12). |
| Q7 | What happens to `gps()`/`setGps()` and geotag? | Option (a): remove them. `GpsCoordinate` stays as the value type for tracks and casts (C8). |
| Q8 | Should some casts link two canonical properties? | Yes, called **side casting**. It shares the same engine as up/down (C11). |
| Q9 | What are casts called in data and code? | "Cast rules" in `registry/casts/*.json`, `CastRule`, "cast group". Curated, cited, marked `partial` (C9). |
| Q10 | What replaces `unmapped()`? | `dumpAll()` (every base entry) and `dumpUnmapped()` (only base entries no canonical property consumed). The CLI uses `dumpall` / `dumpunmapped` (C13). |
| Q11 | Is C4a (full photo Location struct) a prerequisite for casting? | Yes. |

### 8.3 Resolution of the "other inconsistencies"

1. `docs/reconciliation-policy.md` §`iptc.photo.locationCreated` says write-sync emits
   "structured LocationCreated when the writer can". The code writes only the legacy fields
   (`src/core/write_sync.cpp:557-570`). Fix the policy text when C4a lands. If C4a is
   delayed, fix the text first.
2. The legacy city/state/country/sublocation fields are canonical in their own right
   (`cityLegacy`, `provinceOrStateLegacy`, `countryLegacy`, `sublocationLegacy`,
   `countryCodeLegacy`). They own their keys. Any link from them to a Location struct is a
   side cast, not a representation (C11). See §8.4 for the default partner.
3. XMP stores GPS as a string. ExifTool 13.59 confirms the `Location` struct GPS fields are
   in the `exif` namespace and are written as `DDD,MM.mmmmmmH` strings (`XMP2.pl`
   `%sLocationDetails`; `XMP.pm` `%latConv` uses `ToDMS`). The canonical value is a decimal
   number. The codec writes the XMP string form and reads both forms.
4. IPTC logical field names (`gpsLatitude`, `city`, …) are canonical. Backend names
   (`GPSLatitude`, `LocationCreatedCity`, …) are derived data, generated from the TR's
   `XMPid`/`etTag` (C14b).

### 8.4 New finding: MWG treats the legacy fields as Location *Shown*

Decision S4b adopted the Metadata Working Group (MWG) guidance as frozen input and ExifTool's
MWG module as the compatibility reference. ExifTool 13.59 `lib/Image/ExifTool/MWG.pm` maps
the legacy fields as follows (composite tags `City`, `State`, `Country`, `Location`):

| MWG composite | IIM | XMP legacy | IPTC Extension |
|---|---|---|---|
| City | `IPTC:City` (2:90) | `XMP-photoshop:City` | `XMP-iptcExt:LocationShownCity` |
| State | `IPTC:Province-State` (2:95) | `XMP-photoshop:State` | `XMP-iptcExt:LocationShownProvinceState` |
| Country | `IPTC:Country-PrimaryLocationName` (2:101) | `XMP-photoshop:Country` | `XMP-iptcExt:LocationShownCountryName` |
| Location | `IPTC:Sub-location` (2:92) | `XMP-iptcCore:Location` | `XMP-iptcExt:LocationShownSublocation` |

libumm currently does the opposite. It reads and writes these legacy fields as
`iptc.photo.locationCreated` (`src/core/reconcile.cpp:1425-1460`,
`src/core/write_sync.cpp:557-570`). A web-search summary claimed "legacy = created"; the
actual ExifTool source contradicts it. IPTC itself calls the legacy semantics "blurred".
The maintainer decided this on 2026-10-04: Location Shown (decision record OQ2, C11).

### 8.5 Clarifications given to the maintainer

- **Multiple locations.** `locationCreated` (photo) and `locationShot` (video) are *lists*
  of Location structs. An edited video made from several clips can list one shot location
  per clip, and a photo may list more than one. A camera GPS fix is a single point and can
  fill only one entry. The rule: target entry `[0]`; create it if the list is empty; on
  downcast, read entry `[0]`.
- **"The catch" in Q1.** Under rule (a), whatever a standard defines is a representation.
  The IPTC overlay (`registry/mappings/iptc-exif-overlay.json`) records IPTC Mapping
  Guidelines rows linking the Location struct GPS fields to the EXIF GPS IFD tags. So on
  photos, EXIF GPS is a *representation* of `locationCreated[0].gps*` (pending the
  Created-vs-Shown wording check). It is not a cast. The maintainer prefers representation
  over casting, so photo GPS becomes automatic. Video is different: see C8.
