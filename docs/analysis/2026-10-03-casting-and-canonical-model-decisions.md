# Casting and the canonical model — decision record — 2026-10-03

Status: **accepted in principle; open questions OQ1–OQ8 remain**. Design only. Nothing here
is implemented. Session docs are not written until the open questions are answered.

This record follows
[2026-10-03-canonical-properties-and-location-review.md](2026-10-03-canonical-properties-and-location-review.md),
which holds the analysis, the evidence, and the outcome of decisions C1–C6 (its §8). This
record continues the **C** series with **C7–C17**. It supersedes C2 and C4c and modifies
C4b and C6.

It is an explicit departure from parts of the original plan:

- [concept.md](concept.md) "Domain C — EXIF / camera technical metadata" planned EXIF as a
  canonical domain of its own. That plan is dropped (C17).
- The Phase 1 well-known id `exif.gps.position`, introduced before an EXIF registry
  existed, is removed (C8).

Statements checked against a file cite it. Statements that still need a primary-source
check are marked **(verify)**.

---

## 1. Vision

libumm treats the **canonical** properties as its primary domain. Canonical properties
are every IPTC Photo Metadata 2025.1 and IPTC Video Metadata Hub 1.7 property, meaning
every registry id (C1).

Real files rarely contain only canonical data, so libumm also **bridges old and new
metadata**:

- **Backward compatibility (down casting).** When libumm writes canonical values, it can
  also write older, widely read fields such as the EXIF GPS IFD or QuickTime
  `GPSCoordinates`. Applications that never learned the IPTC fields still see the data.
- **Forward compatibility (up casting).** Media arriving from phones, cameras, and older
  tools often has no canonical fields, only common non-canonical ones. libumm recognizes
  them, **previews** what an upcast would produce, and on request **writes** the canonical
  fields.
- **Simplification (side casting).** Some standards define the same concept twice: legacy
  IIM-era fields next to newer structures. libumm can keep both in step.

libumm is an abstraction layer. By default the user sees canonical properties and, for
the common cases, the short cross-media names. Anyone who wants to look under the hood can
ask libumm to show every layer for a property (C12b) or every raw entry (C13).

## 2. The layer model

| Layer | What it is | Who defines it | Behavior |
|---|---|---|---|
| L0 Raw | Every backend entry in a file (`Exif.Photo.DateTimeOriginal`, `QuickTime.GPSCoordinates`, …) | The file | Visible through `dumpAll()` / `dumpUnmapped()` (C13) |
| L1 Representation | A raw key that a **standard** says stores a canonical property | IPTC TR, VMH mappings, IPTC Mapping Guidelines, MWG (S4b) | **Automatic.** Reconciled on every read, written on every write |
| L2 Cast | A libumm-curated link between a non-canonical key and a canonical property (up/down), or between two canonical properties (side) | libumm `registry/casts/` (C9), each rule cited | **Opt-in.** Previewed on read; applied by `umm::cast` or a write option |
| L3 Cross-media | A short name that spans a photo id and a video id (`creator`, `locationCreated`, `rating`, …) | `registry/mappings/cross-media-accessors.json` | Naming only. It never decides what is read (C5) |

---

## 3. Decisions

### C7 — Representation versus cast

**Decision.** A raw key is a **representation** of a canonical property when one of these
sources defines the link:

1. the IPTC Photo Metadata TR (`registry/sources/iptc-pmd-techreference_2025.1.json`):
   XMP, IIM, and EXIF columns;
2. the VMH 1.7 mapping tables (`registry/sources/vmh/`), including the Apple QuickTime
   column;
3. the IPTC Photo Metadata Mapping Guidelines, as curated in
   `registry/mappings/iptc-exif-overlay.json`;
4. the MWG guidance as implemented by ExifTool's `MWG.pm` (decision S4b);
5. the XMP projection of any of the above EXIF tags (`exif:*` / `exifEX:*` namespaces,
   XMP Specification Part 2).

Every other link is a **cast**. Representation always wins: if a standard defines the
link, libumm does not model it as a cast.

**Why not "only XMP is a representation"?** The alternative (Q1 option (b)) would make every
EXIF, IIM, and QuickTime key a cast. A camera JPEG with only `DateTimeOriginal` and no XMP
would read with no `dateCreated` until the user ran an upcast, so almost every camera file
would look empty. Option (a) keeps today's automatic reads wherever a standard backs them.

**Consequences.** Each existing libumm-curated key must be classified. Keys no standard
backs move from L1 to L2.

| Current key | Canonical id | Source | Class |
|---|---|---|---|
| EXIF `DateTimeOriginal` (+`SubSecTimeOriginal`, `OffsetTimeOriginal`), `Xmp.exif.DateTimeOriginal` | photo `dateCreated` | IPTC TR EXIF column; MWG `DateTimeOriginal` | Representation |
| EXIF GPS IFD, top-level `Xmp.exif.GPS*` | photo `locationCreated[0].gps*` | Mapping Guidelines overlay rows **(verify Created vs Shown)** | Representation (C8) |
| IIM 2:90/2:95/2:101/2:92, `photoshop:City/State/Country`, `Iptc4xmpCore:Location` | `cityLegacy` etc. (their own ids) | IPTC TR | Representation of the **legacy ids**. The link to a Location struct is a side cast (C11) |
| QuickTime Keys `com.apple.quicktime.creationdate` | video `dateCreated` | VMH Apple QT column | Representation |
| QuickTime Keys `title`, `description`, `keywords`, `artist`/`author`/`director`, `copyright`, `publisher`, `year`, `genre`, `rating.user` | the VMH-mapped video ids | VMH Apple QT column | Representation |
| QuickTime Keys `location.name` with `location.role` = 0 or absent | video `locationShot[0].name` | VMH ("See location structure + role=0") | Representation |
| QuickTime movie-header `CreateDate` (UTC) | video `dateCreated` | none | **Cast** (up only). Today it is a rank 2 read fallback |
| QuickTime `GPSCoordinates` (Keys `location.ISO6709` or UserData `©xyz`) | video `locationShot[0].gps*` | none; VMH has no QuickTime cell for the Location GPS rows | **Cast** (up and down) |

Behavior changes:

- **Android video date.** Many Android phones write only the movie-header `CreateDate`, not
  Keys `creationdate`. Under C7 such a video reads with no `dateCreated` until upcast. The
  read-time preview (C12) shows the candidate, so the value is not hidden.
- **Video GPS.** Video GPS is no longer automatic in either direction; see C8 and OQ3.
- **ExifTool group information.** libumm's ExifTool key translation folds `Keys`,
  `UserData`, and `ItemList` into one `QuickTime.` family
  (`src/backends/exiftool/keys.cpp:85-86`, `:196-202`). Keys `location.ISO6709` and UserData
  `©xyz` are then indistinguishable. Rules that treat them differently (priority, role
  handling) need the group kept in the raw key. This is an implementation prerequisite.

### C8 — GPS: remove `exif.gps.position`

**Decision.** Remove the id `exif.gps.position` and every special case for it:

- `kGps` in `src/core/property_ids.hpp`;
- `collect_gps` in `src/core/reconcile.cpp`;
- the GPS branch in `src/core/write_sync.cpp`;
- the read id lists in `src/core/xmp_codec.cpp`;
- `Metadata::gps()` / `setGps()`;
- `Datatype::gps_coordinate` in `include/umm/registry.hpp`.

`GpsCoordinate` stays in `include/umm/value.hpp` as the value type for tracks and for the
`capturePosition` cast group.

**Photo.** EXIF GPS is a representation of `iptc.photo.locationCreated[0]` GPS fields
(C7). Reading a camera JPEG fills `locationCreated[0].gpsLatitude/gpsLongitude/gpsAltitude/
gpsAltitudeRef` automatically. Writing `locationCreated[0]` GPS writes the EXIF GPS IFD,
the top-level `exif:GPS*`, and the struct fields
`Iptc4xmpExt:LocationCreated/exif:GPSLatitude` etc. This depends on C4a.

The overlay rows link struct-field GPS ↔ EXIF GPS but do not say *which* Location property.
A web-search summary says the Mapping Guidelines attach EXIF GPS to Location Created. This
must be checked against the Guidelines text before implementation (OQ1).

**Video.** QuickTime GPS is a cast group (`capturePosition`, C9) to and from
`iptc.video.locationShot[0]` GPS.

**What is lost.** `GpsCoordinate::gps_time` (EXIF `GPSDateStamp` + `GPSTimeStamp`, UTC) has
no field in the Location struct. It remains visible as raw entries.

**Geotag (`include/umm/track.hpp`).** Track correlation still produces a `GpsCoordinate`.
Write-back sets `locationCreated[0]` GPS on photo (representation, so EXIF GPS follows
automatically) and `locationShot[0]` GPS on video, then downcasts `capturePosition` when
requested. The `track.hpp` header comment changes accordingly.

**Process.** Update headers first (`metadata.hpp`, `registry.hpp`, `track.hpp`, `umm.hpp`
comments that mention `exif.gps.position` / container GPS). Add an entry to
`docs/developer/release-notes.md`. Update `docs/reconciliation-policy.md` §GPS. Remove the
"Phase 1 properties" note from docs.

### C9 — Cast rules, cast groups, and statuses

**Decision.** Casts are data, not code: JSON files in `registry/casts/`, imported and
generated like the other registry mappings.

- A **cast rule** links one **source** (raw key, or canonical id + field path) to one
  **target**, with a direction (`up`, `down`, `side`), a conversion (C10), and a citation
  (MWG section, ExifTool tag doc, vendor spec, or a libumm rationale). Each rule is marked
  `partial` / curated, like the overlays.
- A **cast group** is an atomic bundle of rules that must move together, for example
  `capturePosition` = latitude + longitude + altitude + refs. A group has a
  prioritized source list. For `capturePosition` on video: Keys `location.ISO6709`, then
  UserData `©xyz`, then EXIF GPS if present.
- Some groups are **one-way**. Container technical facts (duration, frame size) can be
  upcast but never downcast, because libumm does not rewrite container structure.

**Statuses** reported per group:

| Status | Meaning |
|---|---|
| `can_cast` | Source has data, target is empty (field-level, C10) |
| `equal` | Target already holds the same value (within tolerance) |
| `needs_force` | Target holds a different value; applying requires `force` |
| `source_empty` | No source in the priority list has data |
| `target_not_storable` | The file type or backend cannot store the target (capabilities) |
| `ambiguous` | The conversion needs a choice the rule cannot make (C10), for example a lang-alt with several languages and no `x-default` |

`ambiguous` is new since the Q5 discussion. Without it, cases that cannot pick a value
would have to be reported as `needs_force`, which suggests that force would help when it
would not.

### C10 — Conversion heuristics catalog

The casting conundrum is: what to do when the two ends do not have the same shape. These
are the cases found, and the default for each. Every heuristic is named in the cast rule,
so the property map (C12b) and the docs can show it.

| # | Case | Example | Default | Notes |
|---|---|---|---|---|
| H1 | **List → single** | `locationShot` (list) → QuickTime `GPSCoordinates` | Take the **first entry** | Maintainer decision. For locations, the first entry is where tagging started. For lists where "first" has no meaning (keywords), see H4 |
| H2 | **Single → empty list** | camera GPS → empty `locationCreated` | Create entry `[0]` | |
| H3 | **Single → non-empty list** | camera GPS → `locationCreated` that already has one city-only entry | **Merge into `[0]` field by field.** Fill empty fields; equal fields → `equal`; different fields → `needs_force`. **Never append** a new entry by default | Appending would claim a second location exists. An explicit `append` option can come later (OQ4) |
| H4 | **List ↔ list, same concept** | `personShownInTheImage` ↔ `personShownInTheImageWithDetails[*].name`; `creator` ↔ `imageCreator[*].name` | **Union by display value**: add missing entries, never remove; keep target order | Both lists describe the same set, so adding a missing person is not inventing one. Extra target entries report `partial`, not a conflict |
| H5 | **Joined string ↔ list** | Windows `XPKeywords` `"a;b"`; QuickTime/IIM keyword strings | Split/join on the rule's declared separator; trim; drop empties | Bags compare as sets; ordered lists compare in order |
| H6 | **Hierarchical ↔ flat** | Lightroom `lr:hierarchicalSubject` `"Places\|France\|Paris"` → `keywords` | Leaf term (`Paris`) only, then H4 union | Partial. Lightroom's "export containing keywords" option also adds parents. Defer the rule if real files disagree |
| H7 | **lang-alt ↔ plain string** | `title` → QuickTime title | Use `x-default`, else the only language, else `ambiguous` | Upcast writes the string as `x-default` |
| H8 | **Struct ↔ scalar field** | `recordingDevice.modelName` ← EXIF `Model` | Field-level merge into the struct (as H3) | |
| H9 | **One source → several fields (fan-out)** | QuickTime ISO 6709 `+48.8577+002.2950+035.000/` → lat, lon, alt | Parse into the group; missing altitude is not a conflict | |
| H10 | **Several sources → one field (fan-in)** | lat + `GPSLatitudeRef` → signed decimal | Combine per rule | Altitude ref: 1 negates the value |
| H11 | **Units and encodings** | GPS rational DMS / XMP `DDD,MM.mmmmmmH` / decimal; video duration seconds ↔ VMH `VideoTime`; EXIF orientation 1–8 ↔ rotation degrees | Convert to the canonical datatype; compare with the reconcile tolerances (1e-5°, 0.5 m) | No silent rounding beyond the target format's precision; report when precision is lost |
| H12 | **Date precision and offsets** | EXIF `DateTimeOriginal` without `OffsetTimeOriginal`; movie-header `CreateDate` in UTC | **Never invent an offset.** Keep "no offset" as no offset; UTC sources keep `Z`. Equality uses the existing prefix rule | Downcast writes `OffsetTimeOriginal` when the canonical value has an offset |
| H13 | **Split ↔ combined** | IIM date 2:55 + time 2:60 ↔ one date-time | Combine; a missing time stays date-only | Already a representation for photo `dateCreated`; listed so side/down casts reuse the codec |
| H14 | **Length limits and charsets** | IIM City 32 chars, Country 64 (MWG notes); IIM non-UTF-8 | Truncate only on explicit `force`; otherwise `needs_force` with the reason | Report every truncation |
| H15 | **Code ↔ controlled-vocabulary term** | `subjectCodeLegacy` `04000000` ↔ `cvTermAboutImage {cvId: http://cv.iptc.org/newscodes/subjectcode/, cvTermId: …/04000000}`; `sceneCode`; `intellectualGenreLegacy` ↔ `genre` | Only when the code matches the vocabulary pattern; free text → `ambiguous` | Defer from the first rule set |
| H16 | **Semantic near-miss** | EXIF `CreateDate` (digitized) → `dateCreated`; EXIF `ImageDescription` → `description` when it holds a camera model name | Never automatic. Mark the rule `approximate`; apply only with an explicit option | Scans: digitized ≠ created. Some cameras write junk such as `"OLYMPUS DIGITAL CAMERA"` into `ImageDescription` |
| H17 | **Sources disagree** | EXIF GPS ≠ QuickTime `©xyz` | First non-empty source in the group's priority order; report the disagreement | Same as reconcile policy |
| H18 | **Role or qualifier filters** | QuickTime `location.role` 0/absent → `locationShot`; 1 → VMH says Location Shown "(?? 1 the right role value)", ExifTool says "Real Location" | Map role 0/absent only; leave role 1 and 2 unmapped | VMH flags its own uncertainty |
| H19 | **Paired parallel lists** | `nameOfOrganisationFeaturedInTheImage` + `codeOfOrganisationFeaturedInTheImage` | Zip by index only when both lists have equal length; otherwise `ambiguous` | Candidate only; no target uses it yet |
| H20 | **Lost fields** | `GpsCoordinate::gps_time` | Report as `partial` in the cast report | |

### C11 — Side casting

**Decision.** A **side cast** links two canonical properties. It uses the same rule schema,
engine, statuses, and heuristics as up and down casts; only `direction: side` and the
canonical-id endpoints differ. One engine means one code path to test and one way to
describe every link in the property map.

Side casts are opt-in, like all casts. Candidates (photo unless noted):

| Pair | Heuristic | Default |
|---|---|---|
| `cityLegacy`, `provinceOrStateLegacy`, `countryLegacy`, `countryCodeLegacy`, `sublocationLegacy` ↔ a Location struct `[0]` | H1/H3/H14 | Partner per OQ2 (MWG: Location**Shown**) |
| `personShownInTheImage` ↔ `personShownInTheImageWithDetails[*].name` | H4 | First rule set |
| `creator` ↔ `imageCreator[*].name` (PLUS) | H4 | First rule set |
| `intellectualGenreLegacy` ↔ `genre` | H15 | Deferred |
| `subjectCodeLegacy`, `sceneCode` ↔ `cvTermAboutImage` | H15 | Deferred |
| `nameOfOrganisation…` ↔ `codeOfOrganisation…` | H19 | Candidate only |

**Rejected** as side casts: `title` ↔ `headline` (different editorial roles);
`copyrightYear` ↔ `copyrightNotice` (would mean parsing free text); `locationCreated` ↔
`locationShown` (different meanings; never cast by default); video `dateCreated` ↔
`circaDateCreated` (circa is free text for approximate dates).

### C12 — API, CLI split, and the property map

**C12a — Casting API (libumm owns logic; the CLI formats).** Sketch, to be made normative
in `include/umm/` before implementation:

- `umm::cast(path, CastDirection::up|down|side, CastOptions{dry_run, force, groups,
  include_approximate})` returns `Result<CastReport>`, one row per group with its status
  (C9), source, target, before, after, and heuristic. `dry_run` is the preview.
- `ReadOptions::report_casts` fills `Metadata::castCandidates()`. These rows are kept
  separate from `propertyIds()` so a preview never looks like stored data. Default: OQ6.
- `WriteOptions::downcast`, either a list of groups or "defaults". Default: OQ3.
- CLI: `umm cast up|down|side [--dry-run] [--force] [--group G…] FILE`. `umm read` prints a
  one-line hint when candidates exist ("3 upcast candidates; run `umm cast up --dry-run`").

**C12b — Property map ("show me the full gamut").** For any canonical property, libumm
returns a structured description of every layer it can traverse:

1. **Definition and representations (L1).** Registry definition (datatype, cardinality,
   struct fields), then every standard-defined raw key per family (XMP, IIM, EXIF,
   QuickTime, struct-field paths) with its source citation, read precedence, and write
   targets.
2. **Casts (L2).** Every up, down, and side rule touching the property: partner, group,
   direction, heuristic, citation.
3. **Cross-media (L3).** The accessor name, tier, and the other domain's id with its own
   layers 1–2.

With a file, the same call also fills each layer with the values found, the status of each
cast group, and which raw entries were consumed.

- libumm: `umm::describe(property_id)` and `umm::describe(property_id, path)` return a
  `PropertyMap` value. It is built only from generated registry, overlay, cast, and accessor
  data, never from hand-written lists.
- CLI: `umm map PROPERTY [FILE] [--layers representations,casts,cross-media] [--json]`
  prints it. A cross-media name (`locationCreated`) expands to both domain ids.
- The same data generates the property reference docs (C14), so docs and `umm map` cannot
  drift.

Naming (`describe`/`PropertyMap`/`umm map`) is OQ7.

### C13 — `dumpAll()` and `dumpUnmapped()`

**Decision.** Replace `Metadata::unmapped()` with:

- `dumpAll()`: every raw entry the backends read (what `unmapped()` returns today);
- `dumpUnmapped()`: only raw entries that no canonical property consumed as a
  representation. Cast *sources* stay in this view, because they are not canonical, and are
  flagged as cast sources.

The single-key lookup `unmapped(const UnmappedKey&)` becomes `dumpValue(const UnmappedKey&)`.
CLI commands: `umm dumpall` and `umm dumpunmapped`. The C++ names follow the existing
camelCase method style. The backend vocabulary types (`UnmappedKey`, `UnmappedEntry`,
`UnmappedDocument` in `include/umm/backend.hpp`) keep their names in this change; renaming
them to `Raw*` is OQ8. Release-notes entry required.

### C14 — Documentation structure and generated names

**C14a — Formal property reference.** The single large table in `docs/user/guide.md` is
replaced by a generated, API-reference-style markdown set:

1. **Index page.** Cross-media properties first (Tier 1–3, with photo and video ids), then
   the remaining canonical properties per domain, then common non-canonical keys that take
   part in casts.
2. **One section or page per canonical property**, linked from the index. Each holds the
   definition, every representation with its standard citation (for example
   "`DateTimeOriginal` — EXIF, the moment the photo was taken; not `DateTime`, which is the
   modify date"), struct fields (Location GPS, `Iptc4xmpExt`/`Iptc4xmpCore` namespaces),
   cast rules with heuristics, and the cross-media accessor. This is the C12b data rendered
   as text.
3. **Common non-canonical keys.** Only keys that appear in a cast rule, plus a short curated
   list of frequent camera keys that intentionally stay non-canonical (`Make`, `Model`,
   exposure, lens). Listing every ExifTool tag would run to thousands of rows; link to
   ExifTool's tag documentation instead.

The generator lives next to the existing registry codegen and gets a byte-for-byte
contract test, like `docs/supported-types.md`. "Most common properties" is removed (C3a).

**C14b — Generated backend names.** Keep the TR's `etTag` (and `XMPid`) for structure
fields in the registry import. Generate `alias_exiftool_struct_fields`
(`src/core/write_sync.cpp:346`) and the Location field aliases from it. Hand-written
backend field names are then limited to keys no standard file supplies (VMH structs, cast
sources), each with a citation.

### C15 — Testing

Every representation and cast rule gets a round-trip test against each real backend that
claims the capability:

- write canonical → read raw;
- write raw → read canonical;
- cast up/down/side dry run and apply.

Fixtures come from real phone and camera files where possible. The maintainer is collecting
`exiftool -G1 -a -s` output for this. Claims in the docs that are not covered by a test are
marked as such.

### C16 — Time fields

Canonical targets and their sources. Names follow MWG/ExifTool.

| Time concept | Raw keys | Canonical target | Class |
|---|---|---|---|
| Photo taken ("DateTimeOriginal") | EXIF `DateTimeOriginal` + `SubSecTimeOriginal` + `OffsetTimeOriginal`; IIM 2:55 + 2:60; `photoshop:DateCreated`; `exif:DateTimeOriginal` | photo `dateCreated` | Representation |
| Digitized ("CreateDate") | EXIF `CreateDate` (`DateTimeDigitized`) + `SubSecTimeDigitized` + `OffsetTimeDigitized`; IIM 2:62 + 2:63; `xmp:CreateDate` | none on photo | Non-canonical. Optional `approximate` upcast to `dateCreated` (H16) |
| Modified ("ModifyDate") | EXIF `ModifyDate` (`Exif.Image.DateTime`) + `OffsetTime`; `xmp:ModifyDate` | video `dateModified` (`xmp:ModifyDate` is its XMP representation); none on photo | Representation on video; non-canonical on photo |
| Video created | Keys `com.apple.quicktime.creationdate` (local time + offset); `photoshop:DateCreated` | video `dateCreated` | Representation |
| Video file/movie created | movie-header `CreateDate` (UTC); `TrackCreateDate`; `MediaCreateDate` | video `dateCreated` | Cast (up only), UTC kept (H12) |
| Video file/movie modified | movie-header `ModifyDate`; `TrackModifyDate`; `MediaModifyDate` | video `dateModified` | Cast (up only) |
| Metadata last edited | `xmp:MetadataDate`; `Iptc4xmpExt:metadataLastEdited` | video `metadataEditDate` (XMP `Iptc4xmpExt:metadataLastEdited`) | `xmp:MetadataDate` → cast candidate (verify semantic match) |
| Released | `xmpDM:releaseDate` | video `dateReleased` | Representation |
| GPS fix time | EXIF `GPSDateStamp` + `GPSTimeStamp` (UTC); `exif:GPSTimeStamp` | none | Non-canonical; lost on GPS cast (H20) |
| File system times | OS create/modify | none | Out of scope; not metadata |

`Exif.Image.DateTime` is removed from every `dateCreated` candidate list (Q2).

### C17 — No EXIF canonical domain

**Decision.** libumm does not create an EXIF canonical domain. EXIF and container keys are
either representations of IPTC/VMH properties (C7) or cast sources. Camera technical facts
(make, model, lens, exposure) stay non-canonical and visible in `dumpUnmapped()`, except
where a VMH property exists: `recordingDevice` (Device struct: `manufacturer`, `modelName`,
`serialNumber`, `attLensDescription`) and the VMH technical properties (`fileDuration`,
`frameSize`, `videoFrameRate`, `videoCoding`, `audioSampleRate`, `orientation`). For those,
upcast rules apply. This replaces [concept.md](concept.md) "Domain C" and the stage plan's
"until an EXIF-domain registry exists" notes.

---

## 4. Cast candidate catalog (first pass)

To be confirmed against real files (C15). "Set" is the proposed first rule set.

**Up casts** (non-canonical → canonical):

| Group | Sources (priority) | Target | Set |
|---|---|---|---|
| `capturePosition` (video) | Keys `location.ISO6709` → UserData `©xyz` → EXIF GPS | `locationShot[0].gps*` | first |
| `videoCreated` | movie-header `CreateDate` | video `dateCreated` | first |
| `videoModified` | movie-header `ModifyDate` | video `dateModified` | first |
| `recordingDevice` | Keys `make`/`model`; EXIF `Make`/`Model`/`SerialNumber`/`LensModel` | `recordingDevice` fields | first |
| `videoTechnical` | QuickTime `Duration`, `ImageWidth`/`ImageHeight`, `VideoFrameRate`, `CompressorID`, `AudioSampleRate`, `Rotation` | VMH technical ids | later (one-way) |
| `windowsXP` | `XPTitle`, `XPComment`, `XPKeywords`, `XPAuthor`, `XPSubject` | title, description, keywords, creator | later |
| `exifUserComment` | EXIF `UserComment` | description | later (approximate) |
| `pngText` | PNG `Title`, `Author`, `Description`, `Copyright` | title, creator, description, copyrightNotice | later |
| `quickTimeUserData` | UserData `©nam`, `©ART`, `©day`, `©des` | title, creator, dateCreated, description | later |
| `hierarchicalKeywords` | `lr:hierarchicalSubject` | keywords | later (H6) |
| `regionsPeople` | MWG `mwg-rs:Regions` names; Microsoft `MPRI` people | `personShownInTheImage` | later |

**Down casts** (canonical → non-canonical):

| Group | Source | Targets | Set |
|---|---|---|---|
| `capturePosition` (video) | `locationShot[0].gps*` | Keys `location.ISO6709` (+ UserData `©xyz`) | first |
| `recordingDevice` | `recordingDevice` | Keys `make`/`model` | later |
| `windowsXP` | title, description, keywords, creator | XP tags | later (optional) |

Photo GPS has no down cast: EXIF GPS is a representation and is written automatically (C8).

**Side casts:** see C11.

---

## 5. Sequencing

Each step updates `include/umm/` headers first and adds release notes where behavior changes.

1. **Docs correction.** Remove "most common properties"; fix the guide's `unmapped()`
   wording and the reconciliation-policy Location write claim.
2. **C5 read coverage.** Reconcile every registry id through the table-driven engine.
3. **C4a + C14b.** Full photo Location structs, with generated `etTag` aliases and the GPS
   string↔number codec.
4. **C8.** Photo GPS as Location GPS representation; remove `exif.gps.position`, `gps()`,
   `setGps()`, `Datatype::gps_coordinate`; move geotag write-back.
5. **C3c rating** Tier 1 row; **C13** `dumpAll()`/`dumpUnmapped()`.
6. **C9–C11 engine** with the first rule set; C7 reclassification (movie-header date,
   QuickTime GPS) lands here so nothing disappears before the cast preview exists.
7. **C12** `describe()` and CLI commands; **C14a** generated property reference.
8. Later rule sets from §4, driven by real-file evidence (C15 throughout).

## 6. Open questions

| # | Question | Recommendation |
|---|---|---|
| OQ1 | Do the IPTC Mapping Guidelines attach EXIF GPS to Location **Created** (photo)? | Yes, pending a check of the Guidelines text (iptc.org is not reachable from this environment) |
| OQ2 | Legacy city/state/country/sublocation side-cast partner: Location **Shown** (MWG/ExifTool) or Location **Created** (current libumm)? | Follow MWG (Shown), per S4b; or make the partner a per-call choice with Shown as default |
| OQ3 | Should video writes downcast `capturePosition` to QuickTime by default? | Yes, per-group default on. Players and photo apps read QuickTime GPS, not XMP, so without it a geotagged video looks untagged |
| OQ4 | Single → non-empty list: merge into `[0]` (H3) only, or also offer `append`? | Merge only in the first version |
| OQ5 | Confirm the C7 behavior changes (Android movie-header date and video GPS become casts) | Accept; the read preview keeps them visible |
| OQ6 | `ReadOptions::report_casts` default | Off in the library (cost and purity); the CLI turns it on for `umm read` hints |
| OQ7 | Names: `umm::describe`/`PropertyMap`/`umm map` | Accept, or choose another verb (`explain`, `trace`) |
| OQ8 | Rename `UnmappedKey`/`UnmappedEntry`/`UnmappedDocument` to `Raw*` | Yes, in the same pre-release window as C13 |

## 7. References

- ExifTool 13.59 (pinned in `tools/build/backends.env`):
  - `lib/Image/ExifTool/MWG.pm` (composites City/State/Country/Location, lines 300–398);
  - `XMP2.pl` (`%sLocationDetails`: GPS fields in the `exif` namespace);
  - `XMP.pm` (`%latConv`);
  - `QuickTime.pm` (Keys `location.ISO6709`, `location.name`, `location.role`, `make`, `model`).
- `registry/sources/vmh/IPTC-VideoMetadataHub-mapping-AppleQT-Rec_1.7.html`: Location Shot
  "See location structure + com.apple.quicktime.location.role=0"; Location Name ↔
  `com.apple.quicktime.location.name`; no QuickTime cell for the GPS rows.
- `registry/sources/iptc-pmd-techreference_2025.1.json`: `etTag` entries.
- `registry/mappings/iptc-exif-overlay.json`, `registry/mappings/cross-media-accessors.json`.
- `src/backends/exiftool/keys.cpp`, `src/core/reconcile.cpp`, `src/core/write_sync.cpp`,
  `src/core/xmp_codec.cpp`, `src/core/property_ids.hpp`.
- `include/umm/metadata.hpp`, `include/umm/value.hpp`, `include/umm/registry.hpp`,
  `include/umm/track.hpp`, `include/umm/umm.hpp`.
- [concept.md](concept.md) Domain C; [2026-09-27-plan-review-and-decisions.md](2026-09-27-plan-review-and-decisions.md) (S2, S4b).
