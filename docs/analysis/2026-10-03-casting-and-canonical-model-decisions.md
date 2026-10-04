# Casting and the canonical model — decision record — 2026-10-03

Status: **accepted**. The maintainer answered OQ1–OQ8 on 2026-10-04 (§6). C18 (terminology)
and C19 (real-device evidence) were added the same day. Design only; nothing here is
implemented yet. The remaining confirmations before session docs are written are in §8.

This record follows
[2026-10-03-canonical-properties-and-location-review.md](2026-10-03-canonical-properties-and-location-review.md),
which holds the analysis, the evidence, and the outcome of decisions C1–C6 (its §8). This
record continues the **C** series with **C7–C19**. It supersedes C2 and C4c and modifies
C4b and C6.

It is an explicit departure from parts of the original plan:

- [concept.md](concept.md) "Domain C — EXIF / camera technical metadata" planned EXIF as a
  canonical domain of its own. That plan is dropped (C17).
- The Phase 1 well-known id `exif.gps.position`, introduced before an EXIF registry
  existed, is removed (C8).

Statements checked against a file cite it.

This record keeps the *why*. The implementation sessions move the *what* into the
audience-specific docs as each feature lands; §5.2 says which session updates which doc.
Until then, the user, sysadmin, and developer docs describe today's behavior and are not
edited ahead of the code.

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
ask libumm to show every layer for a property (C12b) or every base entry (C13, C18).

## 2. The layer model

| Layer | What it is | Who defines it | Behavior |
|---|---|---|---|
| L0 Base | Every backend entry in a file (`Exif.Photo.DateTimeOriginal`, `QuickTime.GPSCoordinates`, …) | The file | Visible through `dumpAll()` / `dumpUnmapped()` (C13) |
| L1 Representation | A base key that a **standard** says stores a canonical property | IPTC TR, VMH mappings, IPTC Mapping Guidelines, MWG (S4b) | **Automatic.** Reconciled on every read, written on every write |
| L2 Cast | A libumm-curated link between a non-canonical key and a canonical property (up/down), or between two canonical properties (side) | libumm `registry/casts/` (C9), each rule cited | **Opt-in.** Previewed on read; applied by `umm::cast` or a write option |
| L3 Cross-media | A short name that spans a photo id and a video id (`creator`, `locationCreated`, `rating`, …) | `registry/mappings/cross-media-accessors.json` | Naming only. It never decides what is read (C5) |

---

## 3. Decisions

### C7 — Representation versus cast

**Decision.** A base key is a **representation** of a canonical property when one of these
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
| IFD0 `DateTimeOriginal` (`Exif.Image.DateTimeOriginal`, TIFF/EP tag 0x9003 as written by DNG) | photo `dateCreated` | MWG `EXIF:DateTimeOriginal` (ExifTool family-0 EXIF group includes IFD0) | Representation. **New**: today only the ExifIFD copy is read (`src/core/reconcile.cpp:1390-1415`); see C19 |
| EXIF GPS IFD, top-level `Xmp.exif.GPS*` | photo `locationCreated[0].gps*` | Mapping Guidelines overlay rows; Location **Created** confirmed by the maintainer (OQ1) | Representation (C8) |
| IIM 2:90/2:95/2:101/2:92, `photoshop:City/State/Country`, `Iptc4xmpCore:Location` | `cityLegacy` etc. (their own ids) | IPTC TR | Representation of the **legacy ids**. The link to `locationShownInTheImage[0]` is a side cast (C11, OQ2) |
| QuickTime Keys `com.apple.quicktime.creationdate` | video `dateCreated` | VMH Apple QT column | Representation |
| QuickTime Keys `title`, `description`, `keywords`, `artist`/`author`/`director`, `copyright`, `publisher`, `year`, `genre`, `rating.user` | the VMH-mapped video ids | VMH Apple QT column | Representation |
| QuickTime Keys `location.name` with `location.role` = 0 or absent | video `locationShot[0].name` | VMH ("See location structure + role=0") | Representation |
| QuickTime movie-header `CreateDate` (UTC by the QuickTime spec) | video `dateCreated` | none | **Cast** (up only, `approximate`, C19). Today it is a rank 2 read fallback |
| QuickTime `GPSCoordinates` (Keys `location.ISO6709` or UserData `©xyz`) | video `locationShot[0].gps*` | none; VMH has no QuickTime cell for the Location GPS rows | **Cast** (up and down) |

Behavior changes:

- **Movie-header-only video date.** Many Android phones and action cameras (the GoPro
  sample in C19) write only the movie-header `CreateDate`, not Keys `creationdate`. Under C7
  such a video reads with no `dateCreated` until upcast. The read-time preview (C12) shows
  the candidate, so the value is not hidden. Accepted (OQ5).
- **Video GPS.** Video GPS is no longer a representation. Reading needs an upcast; writing
  downcasts by default (OQ3). Accepted (OQ5).
- **ExifTool group information.** libumm's ExifTool key translation folds `Keys`,
  `UserData`, and `ItemList` into one `QuickTime.` family
  (`src/backends/exiftool/keys.cpp:85-86`, `:196-202`). Keys `location.ISO6709` and UserData
  `©xyz` are then indistinguishable. Rules that treat them differently (priority, role
  handling) need the group kept in the base key. This is an implementation prerequisite.

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
**OQ1 answer:** the practical IPTC/MWG mapping treats EXIF GPS as the photo's capture
location, i.e. Location **Created**. Location Shown is a separate, explicit subject-location
field. The overlay gains a `struct_property: locationCreated` qualifier so codegen can emit
the struct-field representation (today it skips struct-field rows,
`tools/registry/generate_cpp.py:186-189`).

**Video.** QuickTime GPS is a cast group (`capturePosition`, C9) to and from
`iptc.video.locationShot[0]` GPS.

**What is lost.** `GpsCoordinate::gps_time` (EXIF `GPSDateStamp` + `GPSTimeStamp`, UTC) has
no field in the Location struct. It remains visible as base entries.

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

- A **cast rule** links one **source** (base key, or canonical id + field path) to one
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
| H3 | **Single → non-empty list** | camera GPS → `locationCreated` that already has one city-only entry | **Merge into `[0]` field by field.** Fill empty fields; equal fields → `equal`; different fields → `needs_force`. **Never append** a new entry | Appending would claim a second location exists. Merge only (OQ4); `append` is not offered |
| H4 | **List ↔ list, same concept** | `personShownInTheImage` ↔ `personShownInTheImageWithDetails[*].name`; `creator` ↔ `imageCreator[*].name` | **Union by display value**: add missing entries, never remove; keep target order | Both lists describe the same set, so adding a missing person is not inventing one. Extra target entries report `partial`, not a conflict |
| H5 | **Joined string ↔ list** | Windows `XPKeywords` `"a;b"`; QuickTime/IIM keyword strings | Split/join on the rule's declared separator; trim; drop empties | Bags compare as sets; ordered lists compare in order |
| H6 | **Hierarchical ↔ flat** | Lightroom `lr:hierarchicalSubject` `"Places\|France\|Paris"` → `keywords` | Leaf term (`Paris`) only, then H4 union | Partial. Lightroom's "export containing keywords" option also adds parents. Defer the rule if real files disagree |
| H7 | **lang-alt ↔ plain string** | `title` → QuickTime title | Use `x-default`, else the only language, else `ambiguous` | Upcast writes the string as `x-default` |
| H8 | **Struct ↔ scalar field** | `recordingDevice.modelName` ← EXIF `Model` | Field-level merge into the struct (as H3) | |
| H9 | **One source → several fields (fan-out)** | QuickTime ISO 6709 `+48.8577+002.2950+035.000/` → lat, lon, alt | Parse into the group; missing altitude is not a conflict | |
| H10 | **Several sources → one field (fan-in)** | lat + `GPSLatitudeRef` → signed decimal | Combine per rule | Altitude ref: 1 negates the value |
| H11 | **Units and encodings** | GPS rational DMS / XMP `DDD,MM.mmmmmmH` / decimal; video duration seconds ↔ VMH `VideoTime`; EXIF orientation 1–8 ↔ rotation degrees | Convert to the canonical datatype; compare with the reconcile tolerances (1e-5°, 0.5 m) | No silent rounding beyond the target format's precision; report when precision is lost |
| H12 | **Date precision and offsets** | EXIF `DateTimeOriginal` without `OffsetTimeOriginal`; movie-header `CreateDate` | **Never invent an offset.** Keep "no offset" as no offset. Movie-header times upcast **without** an offset: the QuickTime spec says UTC, but many cameras write local time (C19 GoPro sample; ExifTool's `QuickTimeUTC` option exists for this reason). Equality uses the existing prefix rule; fractional seconds compare at the shorter precision (C19 Pixel JPEG: `.3897` vs `389696`) | Downcast writes `OffsetTimeOriginal` when the canonical value has an offset |
| H13 | **Split ↔ combined** | IIM date 2:55 + time 2:60 ↔ one date-time | Combine; a missing time stays date-only | Already a representation for photo `dateCreated`; listed so side/down casts reuse the codec |
| H14 | **Length limits and charsets** | IIM City 32 chars, Country 64 (MWG notes); IIM non-UTF-8 | Truncate only on explicit `force`; otherwise `needs_force` with the reason | Report every truncation |
| H15 | **Code ↔ controlled-vocabulary term** | `subjectCodeLegacy` `04000000` ↔ `cvTermAboutImage {cvId: http://cv.iptc.org/newscodes/subjectcode/, cvTermId: …/04000000}`; `sceneCode`; `intellectualGenreLegacy` ↔ `genre` | Only when the code matches the vocabulary pattern; free text → `ambiguous` | Defer from the first rule set |
| H16 | **Semantic near-miss** | EXIF `CreateDate` (digitized) → `dateCreated`; EXIF `ImageDescription` → `description` when it holds a camera model name | Never automatic. Mark the rule `approximate`; apply only with an explicit option | Scans: digitized ≠ created. Some cameras write junk such as `"OLYMPUS DIGITAL CAMERA"` into `ImageDescription` |
| H17 | **Sources disagree** | EXIF GPS ≠ QuickTime `©xyz` | First non-empty source in the group's priority order; report the disagreement | Same as reconcile policy |
| H18 | **Role or qualifier filters** | QuickTime `location.role` 0/absent → `locationShot`; 1 → VMH says Location Shown "(?? 1 the right role value)", ExifTool says "Real Location" | Map role 0/absent only; leave role 1 and 2 unmapped | VMH flags its own uncertainty |
| H19 | **Paired parallel lists** | `nameOfOrganisationFeaturedInTheImage` + `codeOfOrganisationFeaturedInTheImage` | Zip by index only when both lists have equal length; otherwise `ambiguous` | Candidate only; no target uses it yet |
| H20 | **Lost fields** | `GpsCoordinate::gps_time`; EXIF `GPSImgDirection`, `GPSSpeed`, `GPSHPositioningError` (C19 HEIC) | Report as `partial` in the cast report; the fields stay in `dumpUnmapped()` | |
| H21 | **Capture-time sanity check** | C19 Pixel DNG: `DateTimeOriginal` 20:47:28 is UTC, but IIM `TimeCreated` claims `-07:00`; the GPS fix time is 20:47:22Z | **Deferred.** A later read option may *warn* when capture time plus offset is far from the GPS UTC time. Never auto-correct | Prefix-rule reconcile cannot see this error |

### C11 — Side casting

**Decision.** A **side cast** links two canonical properties. It uses the same rule schema,
engine, statuses, and heuristics as up and down casts; only `direction: side` and the
canonical-id endpoints differ. One engine means one code path to test and one way to
describe every link in the property map.

Side casts are opt-in, like all casts. Candidates (photo unless noted):

| Pair | Heuristic | Default |
|---|---|---|
| `cityLegacy`, `provinceOrStateLegacy`, `countryLegacy`, `countryCodeLegacy`, `sublocationLegacy` ↔ `locationShownInTheImage[0]` (`city`, `provinceState`, `countryName`, `countryCode`, `sublocation`) | H1/H3/H14 | First rule set (OQ2) |
| `personShownInTheImage` ↔ `personShownInTheImageWithDetails[*].name` | H4 | First rule set |
| `creator` ↔ `imageCreator[*].name` (PLUS) | H4 | First rule set |
| `intellectualGenreLegacy` ↔ `genre` | H15 | Deferred |
| `subjectCodeLegacy`, `sceneCode` ↔ `cvTermAboutImage` | H15 | Deferred |
| `nameOfOrganisation…` ↔ `codeOfOrganisation…` | H19 | Candidate only |

**Why the legacy fields pair with Location Shown (OQ2).** Two reasons, both recorded:

1. **MWG.** ExifTool's MWG module maps IIM City/Province-State/Country/Sub-location and
   their `photoshop:`/`Iptc4xmpCore:` XMP twins to `LocationShown*`
   (`MWG.pm` 13.59, lines 300–398). S4b makes MWG the compatibility reference.
2. **How the fields are filled.** Location Shown, like the legacy fields, is typically set
   afterwards by a person, a captioning tool, or computer vision/AI, describing *what the
   picture shows*. It is not necessarily where the creator stood. Location Created is a
   capture-time fact, which is why device GPS belongs there (OQ1).

This changes today's behavior: libumm currently reads and writes the legacy fields as
`iptc.photo.locationCreated` (`src/core/reconcile.cpp:1425-1460`,
`src/core/write_sync.cpp:557-570`). Under C4b + C11 that special path is removed.
Release-notes entry required.

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
  separate from `propertyIds()` so a preview never looks like stored data. **Default off**
  in the library; the CLI turns it on for `umm read` hints (OQ6).
- `WriteOptions::downcast`, either a list of groups or "defaults". **Default:** the
  `capturePosition` downcast is on for video writes, because players and photo apps read
  QuickTime GPS and not XMP (OQ3). Other downcast groups default off.
- CLI: `umm cast up|down|side [--dry-run] [--force] [--group G…] FILE`. `umm read` prints a
  one-line hint when candidates exist ("3 upcast candidates; run `umm cast up --dry-run`").

**C12b — Property map ("show me the full gamut").** For any canonical property, libumm
returns a structured description of every layer it can traverse:

1. **Definition and representations (L1).** Registry definition (datatype, cardinality,
   struct fields), then every standard-defined base key per family (XMP, IIM, EXIF,
   QuickTime, struct-field paths) with its source citation, read precedence, and write
   targets.
2. **Casts (L2).** Every up, down, and side rule touching the property: partner, group,
   direction, heuristic, citation.
3. **Cross-media (L3).** The accessor name, tier, and the other domain's id with its own
   layers 1–2.

With a file, the same call also fills each layer with the values found, the status of each
cast group, and which base entries were consumed.

- libumm: `umm::describe(property_id)` and `umm::describe(property_id, path)` return a
  `PropertyMap` value. It is built only from generated registry, overlay, cast, and accessor
  data, never from hand-written lists.
- CLI: `umm map PROPERTY [FILE] [--layers representations,casts,cross-media] [--json]`
  prints it. A cross-media name (`locationCreated`) expands to both domain ids.
- The same data generates the property reference docs (C14), so docs and `umm map` cannot
  drift.

Naming `describe`/`PropertyMap`/`umm map` is accepted (OQ7).

### C13 — `dumpAll()` and `dumpUnmapped()`

**Decision.** Replace `Metadata::unmapped()` with:

- `dumpAll()`: every base entry the backends read (what `unmapped()` returns today);
- `dumpUnmapped()`: only base entries that no canonical property consumed as a
  representation. Cast *sources* stay in this view, because they are not canonical, and are
  flagged as cast sources.

The single-key lookup `unmapped(const UnmappedKey&)` becomes `dumpValue(const BaseKey&)`.
CLI commands: `umm dumpall` and `umm dumpunmapped`. The C++ names follow the existing
camelCase method style. The backend vocabulary types are renamed to `Base*` in the same
pre-release window (C18, OQ8). Release-notes entry required.

After C13 and C18, the word **unmapped** has exactly one meaning: a base entry that no
canonical property consumed. It no longer names the whole backend vocabulary.

### C14 — Documentation structure and generated names

**C14a — Formal property reference.** The single large table in `docs/user/guide.md` is
replaced by a generated, API-reference-style markdown set:

1. **Index page.** Cross-media properties first (Tier 1–3, with photo and video ids), then
   the remaining canonical properties per domain, then common non-canonical keys that take
   part in casts.
2. **One anchored section per canonical property**, on one generated page per domain
   (`docs/user/properties/photo.md`, `docs/user/properties/video.md`), linked from the
   index. One page per property would mean 171 files; per-domain pages keep links stable
   and diffs reviewable. Each holds the
   definition, every representation with its standard citation (for example
   "`DateTimeOriginal` — EXIF, the moment the photo was taken; not `DateTime`, which is the
   modify date"), struct fields (Location GPS, `Iptc4xmpExt`/`Iptc4xmpCore` namespaces),
   cast rules with heuristics, and the cross-media accessor. This is the C12b data rendered
   as text.
3. **Common non-canonical keys** (`docs/user/properties/base-keys.md`). Only keys that appear in a cast rule, plus a short curated
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

- write canonical → read base;
- write base → read canonical;
- cast up/down/side dry run and apply.

Real-device tag layouts come from [docs/sample-output.txt](../sample-output.txt) (C19).
Fixtures that reproduce those layouts follow the M6 fixture rules (see OQ-R1 in §8). Claims
in the docs that are not covered by a test are marked as such.

### C16 — Time fields

Canonical targets and their sources. Names follow MWG/ExifTool.

| Time concept | Base keys | Canonical target | Class |
|---|---|---|---|
| Photo taken ("DateTimeOriginal") | EXIF `DateTimeOriginal` (ExifIFD, or IFD0 in DNG) + `SubSecTimeOriginal` + `OffsetTimeOriginal`; IIM 2:55 + 2:60; `photoshop:DateCreated`; `exif:DateTimeOriginal` | photo `dateCreated` | Representation |
| Digitized ("CreateDate") | EXIF `CreateDate` (`DateTimeDigitized`) + `SubSecTimeDigitized` + `OffsetTimeDigitized`; IIM 2:62 + 2:63; `xmp:CreateDate` | none on photo | Non-canonical. Optional `approximate` upcast to `dateCreated` (H16) |
| Modified ("ModifyDate") | EXIF `ModifyDate` (`Exif.Image.DateTime`) + `OffsetTime`; `xmp:ModifyDate` | video `dateModified` (`xmp:ModifyDate` is its XMP representation); none on photo | Representation on video; non-canonical on photo |
| Video created | Keys `com.apple.quicktime.creationdate` (local time + offset); `photoshop:DateCreated` | video `dateCreated` | Representation |
| Video file/movie created | movie-header `CreateDate`; `TrackCreateDate`; `MediaCreateDate` | video `dateCreated` | Cast (up only, `approximate`), no offset assumed (H12, C19) |
| Video file/movie modified | movie-header `ModifyDate`; `TrackModifyDate`; `MediaModifyDate` | video `dateModified` | Cast (up only) |
| Metadata last edited | `xmp:MetadataDate`; `Iptc4xmpExt:metadataLastEdited` | video `metadataEditDate` (XMP `Iptc4xmpExt:metadataLastEdited`) | `xmp:MetadataDate` (XMP: "the date and time that any metadata for this resource was last changed") → up/down cast, later set |
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

### C18 — Say "base metadata", never "raw metadata"

**Problem.** Earlier records, the reconciliation policy, and the C-series drafts called the
entries a backend reads from a file "raw" (raw key, raw entry, raw value). In a media
library that word already has a fixed meaning: **RAW** is a family of camera image formats
(DNG, CR2, CR3, NEF, ARW, RW2, …), and libumm supports many of them
(`docs/supported-types.md`). "Read the raw metadata of a RAW file" is ambiguous, and
"raw" was beginning to appear in API names.

**Decision.** The metadata entries as stored in a file, in their native family and key
syntax (EXIF, IPTC IIM, XMP, QuickTime, …), before libumm maps them to canonical
properties, are **base metadata**. One entry is a **base entry**; its address is a
**base key**; L0 in the layer model is the **base layer**.

| Term | Meaning |
|---|---|
| base metadata / base entry / base key | What a backend read or will write, in its native vocabulary (`Exif.Photo.DateTimeOriginal`, `QuickTime.GPSCoordinates`) |
| unmapped | A base entry that no canonical property consumed as a representation (C13). Not a synonym for "base" |
| RAW | Only the camera image format family. Always upper case in prose |

**Scope.**

- *Living* docs (`docs/user/`, `docs/developer/`, `docs/reconciliation-policy.md`,
  `docs/umm-cli-concept.md`), headers, and code use "base". The C-series records are already
  updated. Dated records before 2026-10-03 keep their wording as history.
- Public API renames, in the same pre-release window as C13 (OQ8):
  `UnmappedKey` → `BaseKey`, `UnmappedEntry` → `BaseEntry`, `UnmappedDocument` →
  `BaseDocument`, `UnmappedChanges` → `BaseChanges`, `Backend::readUnmapped` /
  `writeUnmapped` → `readBase` / `writeBase`, `Metadata::assignUnmapped` → `assignBase`,
  `SourceRef::raw_key` → `base_key`. Internal helpers follow (`xmp_raw_key` →
  `xmp_base_key`, and so on). Release-notes entry required.
- Unrelated technical meanings are untouched: RAW formats, the `tests/corpus/raw/` folder,
  `raw.githubusercontent.com` URLs, C++ raw string literals, and tooling variables that hold
  undecoded JSON.
- **Prevention.** Add the rule to `.github/copilot-instructions.md` (repository conventions)
  and to the standing constraints in `docs/developer/implementation-history.md`, so
  contributors and agents do not reintroduce the term.

### C19 — Evidence from real-device files

The maintainer supplied `exiftool -G1 -a -s` output (ExifTool 12.76) for five real files in
[docs/sample-output.txt](../sample-output.txt). Findings and their effect on the decisions:

| File | Observation | Effect |
|---|---|---|
| iPhone X `IMG_3851.MOV` (iOS 12.4.1) | Keys `GPSCoordinates` with altitude; Keys `Make`/`Model`/`Software`; Keys `CreationDate` `2019:09:05 14:23:07-04:00`; movie-header `CreateDate` `2026:10:04 04:35:02` | `capturePosition` upcast fills lat, lon, **and** alt (H9). `recordingDevice` upcast from Keys `make`/`model`. Keys `CreationDate` is the `dateCreated` representation. The movie header is **seven years later** than capture: it records when the file was re-exported, not when it was shot. So the `videoCreated` upcast is `approximate` and is never chosen when a representation exists |
| iPhone 16 Pro `IMG_3853.HEIC` | EXIF `DateTimeOriginal` + `SubSecTimeOriginal` + `OffsetTimeOriginal`; full GPS IFD including `GPSImgDirection`, `GPSSpeed`, `GPSHPositioningError`; `LensModel`; XMP only in Apple auxiliary namespaces | `dateCreated` reads with sub-seconds and offset. Location Created gets lat/lon/alt only; the other GPS tags stay in `dumpUnmapped()` (H20). IPTC Photo has no device property, so `Make`/`Model`/`LensModel` stay non-canonical on photos (C17). HEIC writes stay ExifTool-only |
| Pixel XL `android1.dng` | `DateTimeOriginal` in **IFD0** (DNG/TIFF-EP), not ExifIFD; value `20:47:28` matches UTC (GPS fix `20:47:22Z` in the companion JPEG, whose local time is `13:47:28`), yet IIM `TimeCreated` (probably added by Photo Mechanic; `XMP-photomech` is present) claims `-07:00`; `xmp:Rating` 0 | Add IFD0 `DateTimeOriginal` as a `dateCreated` representation (C7). The 7-hour error is invisible to reconcile; a GPS-time cross-check is deferred (H21). This file is itself a RAW file, the case C18 avoids confusing |
| Pixel XL `android1.jpg` | EXIF GPS IFD **and** top-level `XMP-exif` GPS (differences ≤ 0.03″, under 1e-5°); `XMP-exif:GPSDateTime`; `photoshop:DateCreated` `…28.3897` vs EXIF `SubSecTimeOriginal` `389696`; `xmp:CreateDate` | Both GPS encodings are representations and reconcile as `equal` within tolerance. Fractional seconds compare at the shorter precision (H12). `xmp:CreateDate` is the digitized time, not `dateCreated` (C16) |
| GoPro HERO12 `gopro.mp4` | No Keys, no XMP; movie-header `CreateDate` `2016:01:07` (camera clock never set; the model shipped in 2023); `GoPro:Model`, `GoPro:CameraSerialNumber`, `UserData:LensSerialNumber`; GPS only in the `gpmd` timed-metadata track | Confirms the movie header can be wrong *and* local time (H12). `recordingDevice` gains GoPro sources. Per-frame GPS in a timed track is out of scope for casting; a later `importTrack` reader for embedded tracks is a candidate, not part of this plan |

The sample contains personal GPS positions and device serial numbers. See OQ-R1 for how
fixtures are derived from it.

---

## 4. Cast candidate catalog (first pass)

To be confirmed against real files (C15). "Set" is the proposed first rule set.

**Up casts** (non-canonical → canonical):

| Group | Sources (priority) | Target | Set |
|---|---|---|---|
| `capturePosition` (video) | Keys `location.ISO6709` → UserData `©xyz` → EXIF GPS | `locationShot[0].gps*` | first |
| `videoCreated` | movie-header `CreateDate` | video `dateCreated` | first (`approximate`, C19) |
| `videoModified` | movie-header `ModifyDate` | video `dateModified` | first |
| `recordingDevice` | Keys `make`/`model`; EXIF `Make`/`Model`/`SerialNumber`/`LensModel`; GoPro `Model`/`CameraSerialNumber` (C19) | `recordingDevice` fields | first |
| `videoTechnical` | QuickTime `Duration`, `ImageWidth`/`ImageHeight`, `VideoFrameRate`, `CompressorID`/`CompressorName`, `AudioSampleRate`, `AudioChannels`, `Rotation` | VMH technical ids | later (one-way) |
| `metadataDate` | `xmp:MetadataDate` | video `metadataEditDate` | later |
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

## 5. Implementation plan

### 5.1 Proposed sessions

Session numbering continues after 42. Each session updates the `include/umm/` headers first
when the public contract changes, adds a `docs/developer/release-notes.md` entry for every
behavior or API change, and tests every representation and cast rule it touches (C15).

| Session | Scope | Decisions | Depends on |
|---|---|---|---|
| 43 | Terminology and base-entry views: `Unmapped*` → `Base*`, `raw_key` → `base_key`, `unmapped()` → `dumpAll()` / `dumpValue()`, new `dumpUnmapped()` (consumed = union of every reconciled property's `sources`) | C13, C18 | — |
| 44 | Read coverage: reconcile every registry id; Tier 1 `rating`; drop `Exif.Image.DateTime` from `dateCreated`; add IFD0 `DateTimeOriginal` | C5, C3c, C16, C19 | 43 |
| 45 | Registry import: keep TR `etTag`/`XMPid` for struct fields; overlay `struct_property` qualifier; generated ExifTool struct aliases | C14b, C8 (overlay) | — |
| 46 | Photo Location structs: full `Location` read/write incl. GPS fields and the GPS string↔number codec; legacy ids own their keys; remove the legacy → `locationCreated` path | C4a, C4b | 44, 45 |
| 47 | Cast engine: `registry/casts/` schema, importer, codegen; `umm::cast`, `CastReport`, statuses, `ReadOptions::report_casts`, `WriteOptions::downcast`; first rule set (§4 "first", C11 legacy → Location Shown, people/creator side casts); C7 reclassification of movie-header dates and QuickTime GPS; QuickTime group kept in base keys | C7, C9–C12a, C19 | 46 |
| 48 | GPS: photo EXIF GPS as `locationCreated[0]` representation; video `capturePosition` default downcast; remove `exif.gps.position`, `gps()`, `setGps()`, `Datatype::gps_coordinate`; geotag write-back to Location | C8 | 47 |
| 49 | Property map: `umm::describe()` / `PropertyMap` from generated registry, overlay, cast, and accessor data | C12b | 47 |
| 50 | Generated property reference (`docs/user/properties/`) with byte-for-byte contract test; user-guide restructure | C14a, C3a | 48, 49 |
| 51 | Verification: real-device layout fixtures from C19, backend matrix for every representation and cast rule, audit of doc claims against tests | C15 | 48 |

### 5.2 Documentation each session must update

The user, sysadmin, and developer docs describe shipped behavior, so they change in the
session that ships the behavior, not before.

| Doc | Content to add or change | Session |
|---|---|---|
| `.github/copilot-instructions.md` | "Say base metadata, never raw metadata; RAW means the image format" | 43 |
| `docs/developer/implementation-history.md` | Standing constraints: C18 terminology; C7 representation-versus-cast rule; no EXIF canonical domain (C17); C-series in "Where the details live" | 43 (terms), 47 (rule) |
| `docs/user/guide.md` | "Unmapped metadata" → base metadata, `dumpAll()` / `dumpUnmapped()` | 43 |
| `docs/user/guide.md` | Intro: canonical = IPTC Photo + VMH; EXIF/IIM/QuickTime are representations. Remove "most common properties" and `exif.gps.position`. Tier 1 `rating` | 44 (intro, rating), 48 (GPS) |
| `docs/user/guide.md` | Location and GPS section: Location struct fields incl. GPS, `Iptc4xmpExt` / `Iptc4xmpCore`, Created versus Shown, legacy fields and their side cast | 46, 47 |
| `docs/user/guide.md` | Time fields (C16): taken versus digitized versus modified; movie-header caveats | 44, 47 |
| `docs/user/guide.md` | Casting: up/down/side, statuses, dry run, defaults, heuristics a user can see | 47 |
| `docs/user/guide.md` + `docs/user/properties/` | Property map; generated per-property reference replaces the "Full property reference" tables | 49, 50 |
| `docs/developer/canonical-model.md` (new) | Layer model, terminology, representation rule, cast-rule schema, heuristics catalog H1–H21, how to add a rule | 47 |
| `docs/reconciliation-policy.md` | Base terminology (43); per-id precedence for all registry ids (44); `locationCreated` write claim and legacy fields (46); GPS section rewritten as Location GPS (48) | 43–48 |
| `registry/schema.md` | `etTag`/`XMPid` fields; overlay qualifier; `registry/casts/` schema | 45, 47 |
| `docs/test-media-plan.md` | Real-device layout fixtures (OQ-R1) | 51 |
| `docs/umm-cli-concept.md` | `dumpall` / `dumpunmapped` (43); `cast`, `read` hints (47); GPS examples move to `locationCreated` (48); `map` (49); `--use BASEKEY` (43) | 43–49 |
| `docs/sysadmin/install.md` | No change expected: no new build options, dependencies, or deployment steps. A session that adds any updates this doc | — |
| `docs/README.md` | New pages (properties reference, canonical model) | 47, 50 |

## 6. Answered questions

All answered by the maintainer on 2026-10-04.

| # | Question | Answer |
|---|---|---|
| OQ1 | Do the IPTC Mapping Guidelines attach EXIF GPS to Location **Created** (photo)? | **Yes.** EXIF GPS is the photo's capture location (Location Created). Location Shown is a separate, explicit subject-location field |
| OQ2 | Legacy city/state/country/sublocation side-cast partner? | **Location Shown**, following MWG, and because legacy fields and Location Shown are typically set afterwards by a person/CV/AI, not at capture (C11) |
| OQ3 | Downcast `capturePosition` to QuickTime by default on video writes? | **Yes** |
| OQ4 | Single → non-empty list: merge only, or also `append`? | **Merge only** for now |
| OQ5 | Accept the C7 behavior changes (movie-header date and video GPS become casts)? | **Accepted** |
| OQ6 | `ReadOptions::report_casts` default? | **Off** in the library; the CLI turns it on |
| OQ7 | Names `umm::describe` / `PropertyMap` / `umm map`? | **Accepted** |
| OQ8 | Rename the backend vocabulary types? | **Yes, to `Base*`, not `Raw*`**: "raw" collides with RAW image formats (C18) |

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
- [docs/sample-output.txt](../sample-output.txt): real-device ExifTool output (C19).

## 8. Remaining confirmations before session docs

These are small and each has a recommendation. Session docs 43–51 can be written once they
are confirmed or overridden.

| # | Question | Recommendation |
|---|---|---|
| OQ-R1 | Fixtures for the C19 layouts. M6 allows generated in-repo fixtures (Tier A) or checksummed public downloads (Tier B). The original files are large (up to 97 MB) and contain personal GPS positions and serial numbers. | Generate Tier A fixtures that copy the **tag layouts** with synthetic values; do not commit the originals. Also redact the coordinates and serial numbers in `docs/sample-output.txt`, or move it out of the docs tree |
| OQ-R2 | Movie-header `CreateDate` upcast is `approximate` (C19). `umm cast up` then applies it only with `include_approximate`, while the read preview still lists it. | Accept. The iPhone sample is seven years off and the GoPro sample's clock was never set |
| OQ-R3 | Session breakdown §5.1 and documentation ownership §5.2. | Accept, or reorder. 43 and 45 have no dependencies and can run in parallel |
