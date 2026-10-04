# Canonical model

libumm’s public vocabulary is the IPTC Photo Metadata 2025.1 and IPTC Video
Metadata Hub 1.7 registry ids. File-stored tags are **base** metadata (C18).
This page is the developer map of layers, representation versus cast (C7), the
cast-rule schema, conversion heuristics, and how to add a rule. Design rationale
stays in
[2026-10-03-casting-and-canonical-model-decisions.md](../analysis/2026-10-03-casting-and-canonical-model-decisions.md).

## Layers (L0–L3)

| Layer | What it is | Who defines it | Behavior |
|---|---|---|---|
| L0 Base | Every backend entry (`Exif.Photo.DateTimeOriginal`, `QuickTime.Keys.CreationDate`, …) | The file | `dumpAll()` / `dumpUnmapped()` |
| L1 Representation | A base key a **standard** says stores a canonical property | IPTC TR, VMH, Mapping Guidelines, MWG, XMP of those EXIF tags | Automatic on every `umm::read` / `umm::write` |
| L2 Cast | A curated link that is not a representation (up/down) or between two canonical ids (side) | `registry/casts/` | Opt-in: `ReadOptions::report_casts`, `umm::cast`, `WriteOptions::downcast` |
| L3 Cross-media | Short names spanning a photo id and a video id | `registry/mappings/cross-media-accessors.json` | Naming only; never decides what is read |

**Base** means metadata as stored in a file (`BaseKey` / `BaseEntry`).
**Unmapped** means a base entry that no canonical property consumed as a
representation. **RAW** means camera image formats only.

## Representation versus cast (C7)

A base key is a representation only when one of these sources defines the link:

1. IPTC Photo Metadata Technical Reference
2. IPTC Video Metadata Hub 1.7 mapping tables (including Apple QuickTime)
3. IPTC Photo Metadata Mapping Guidelines (`registry/mappings/iptc-exif-overlay.json`)
4. MWG as implemented by ExifTool `MWG.pm` (S4b)
5. The XMP projection of those EXIF tags

Every other link is a **cast**. Representation always wins.

Consequences already in the engine:

- Keys `CreationDate` (`QuickTime.Keys.CreationDate`) is video `dateCreated`.
- Movie-header `QuickTime.CreateDate` is **not** `dateCreated`; it is the
  `videoCreated` upcast (`approximate`, C19). Many phones write only the movie
  header.
- QuickTime Keys `location.ISO6709` and UserData `GPSCoordinates` are **not**
  video GPS representations; they are the `capturePosition` group.
- ExifTool Keys / UserData / ItemList stay in the base key
  (`QuickTime.Keys.…`) so those sources are distinguishable.
- There is no EXIF canonical domain (C17). Camera EXIF GPS is a
  representation of `locationCreated[0]` GPS on photos.

## Cast API (C9–C12a)

`umm::cast(path, direction, CastOptions)` returns `Result<CastReport>`.
`CastOptions::dry_run` defaults to true (preview, no write). Apply with
`dry_run = false`, then `umm::write`. `include_approximate` is required to
apply H15/H16-style rules (`videoCreated` in the first set). `force` overwrites
a different target (`needs_force`). `groups` empty means every group for that
direction.

Statuses per group: `can_cast`, `equal`, `needs_force`, `source_empty`
(omitted from reports), `target_not_storable`, `ambiguous`.

`ReadOptions::report_casts` (default off) fills `Metadata::castCandidates()`.
`WriteOptions::downcast`: `nullopt` = library default (`capturePosition` on
video); empty vector = none.

Cast-rule sources are flagged on `dumpAll()` / `dumpUnmapped()`
(`BaseEntry::cast_source`).

## First rule set

| Group | Direction | Notes |
|---|---|---|
| `capturePosition` | up | Keys ISO6709 → UserData GPS → EXIF GPS → `locationShot[0].gps*` (H1–H3, H9–H11, H17, H18, H20) |
| `capturePosition` | down | `locationShot[0]` GPS → Keys `location.ISO6709` and UserData `GPSCoordinates` as ISO 6709 |
| `videoCreated` | up | Movie-header `CreateDate` → `iptc.video.dateCreated`; `approximate` (H12, H16) |
| `videoModified` | up | Movie-header `ModifyDate` → `iptc.video.dateModified` (H12) |
| `recordingDevice` | up | Keys Make/Model, EXIF Make/Model/serial/lens, GoPro tags → Device struct (H8) |
| `locationShownLegacy` | side | Legacy city/state/country/countryCode/sublocation ↔ `locationShownInTheImage[0]` (H3, H14; MWG Location Shown) |
| `personShown` | side | `personShownInTheImage` ↔ details `name` (H4 union) |
| `creatorImageCreator` | side | `creator` ↔ `imageCreator[*].imageCreatorName` (H4 union) |

H3 never appends a second location entry. H15/H16 apply only with
`include_approximate`. H19 and H21 are not implemented.

## Cast-rule schema (`registry/casts/`)

One JSON object per group file. Curated, not IPTC-imported. UTF-8, LF, 2-space
indent, trailing newline. Codegen: `tools/registry/generate_cpp.py --casts-dir`
→ `src/generated/cast_rules.hpp`.

**Group**

| Field | Type | Meaning |
|---|---|---|
| `id` | string | Group id (`capturePosition`, `videoCreated`, …) |
| `direction` | `up` \| `down` \| `side` | Cast direction |
| `partial` | bool | Curated / incomplete |
| `approximate` | bool | Apply only with `include_approximate` |
| `one_way` | bool | No reverse group |
| `citation` | string | Why this is a cast, not a representation |
| `source_priority` | string[] | Rule ids in evaluation order |
| `rules` | object[] | Member rules |

**Rule**

| Field | Type | Meaning |
|---|---|---|
| `id` | string | Unique within the group |
| `source` / `target` | endpoint | See below |
| `heuristic` | `H1`…`H21` | Named conversion |
| `citation` | string | Per-rule evidence |

**Endpoint:** `kind` is `base_key`, `property`, or `property_field`; `key` is
the base key or property id; `field` / `index` optional for struct fields.

## Heuristics H1–H21

| # | Case | Default |
|---|---|---|
| H1 | List → single | First entry |
| H2 | Single → empty list | Create `[0]` |
| H3 | Single → non-empty list | Merge into `[0]`; never append |
| H4 | List ↔ list, same concept | Union by display value |
| H5 | Joined string ↔ list | Split/join on the declared separator |
| H6 | Hierarchical ↔ flat | Leaf term, then H4 (later) |
| H7 | lang-alt ↔ string | `x-default`, else only language, else `ambiguous` |
| H8 | Struct ↔ scalar | Field-level merge (as H3) |
| H9 | Fan-out | Parse one encoding into several fields |
| H10 | Fan-in | Combine per rule (GPS refs) |
| H11 | Units / encodings | Canonical datatype; 1e-5° / 0.5 m |
| H12 | Dates / offsets | Never invent an offset |
| H13 | Split ↔ combined | Date + time; missing time stays date-only |
| H14 | Length limits | Truncate only with `force` |
| H15 | Code ↔ CV term | Pattern match only; else `ambiguous` (not in first set) |
| H16 | Semantic near-miss | `approximate`; `include_approximate` |
| H17 | Sources disagree | First non-empty in group priority; report |
| H18 | Role filter | Keys location only when `location.role` is 0 or absent |
| H19 | Paired parallel lists | Not implemented |
| H20 | Lost fields | Partial; extras stay unmapped |
| H21 | Capture-time sanity | Deferred; never auto-correct |

## How to add a rule

1. Classify the link with C7. If a listed standard already defines it, add a
   representation (registry / overlay), not a cast.
2. Add `registry/casts/<group>.json` with the group/rule schema and a citation.
3. Regenerate `src/generated/cast_rules.hpp` (`generate_cpp.py --casts-dir`).
4. If the conversion is not already handled in `src/core/cast.cpp`
   (`evaluate_group` / apply helpers), add the heuristic there. Do not put
   `ns:prop` strings in `src/` outside `src/generated/`.
5. Add dry-run and apply tests (unit; backend on each backend that claims the
   target). Update this page and the user guide if the group is user-visible.
