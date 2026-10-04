# libumm property registry record

This is the written definition of the machine-readable registry produced from
standards Technical References (concept.md §20; decisions **S2**, **M5**, **R2**).
Session 06 imports IPTC Photo Metadata; session 20 adds the IPTC Video Metadata
Hub as a separate domain.

The registry does not invent property semantics. Each record copies identifiers,
definitions, datatypes, cardinality, and representations from the source
standard and records provenance so "which standards do you implement?" is
answered from the data alone.

## Files

| Path | Role |
| --- | --- |
| `registry/sources/` | Vendored IPTC Photo Technical Reference plus `SOURCE.md` |
| `registry/sources/vmh/` | Vendored IPTC Video Metadata Hub 1.7 artifacts plus `SOURCE.md` |
| `registry/iptc-photo/iptc-photo.json` | Photo importer output (generated-but-committed) |
| `registry/iptc-video/iptc-video.json` | VMH importer output (generated-but-committed) |
| `registry/mappings/iptc-exif-overlay.json` | Curated EXIF mappings from the IPTC Mapping Guidelines (session 07; `partial: true` until Stage 6) |
| `registry/mappings/cross-media-accessors.json` | Hand-curated Phase 2 accessor map (session 37); photo+video ids, tier, transposition |
| `registry/casts/` | Hand-curated cast groups (session 47); not IPTC-imported |
| `tools/registry/import_iptc.py` | Stdlib-only IPTC Photo importer |
| `tools/registry/import_vmh.py` | Stdlib-only IPTC Video Metadata Hub importer |
| `tools/registry/generate_cpp.py` | Stdlib-only C++ table generator |
| `src/generated/` | Committed generated `property_registry.hpp` / `.cpp` |
| `registry/capabilities/` | File-type capability tables (session 15; decision M2) |
| `tools/registry/generate_supported_types.py` | Regenerates `supported-types.md` and `src/generated/capabilities_data.hpp` |

Output JSON is UTF-8, LF newlines, 2-space indent, a trailing newline, and
stable key/array ordering. Re-running the importer must be byte-identical.
`.gitattributes` pins `registry/**/*.json` to LF so Windows checkouts stay
byte-identical to the importer.

## Envelope (`registry/iptc-photo/iptc-photo.json`)

| Field | Type | Meaning |
| --- | --- | --- |
| `standard` | string | Adopted standard name, e.g. `IPTC Photo Metadata` |
| `standard_version` | string | Standard version, e.g. `2025.1` |
| `source` | object | Provenance of the Technical Reference (see Source) |
| `counts` | object | `properties`, `core`, `extension`, `structs` — must match the TR |
| `properties` | array | One record per IPTC Core + Extension top-level property |
| `structs` | array | Structured types used by properties (Location, etc.) |

`AltLang` in the TR is a datatype (`lang-alt`), not a registry struct.

## Property record

Matches concept.md §20.

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | string | Stable library id, e.g. `iptc.photo.creator` |
| `standard` | string | Same as envelope `standard` |
| `standard_version` | string | Same as envelope `standard_version` |
| `schema` | string | `Core 1.5` or `Extension 1.9` |
| `standard_property_name` | string | IPTC property name, e.g. `Creator` |
| `definition` | string | IPTC definition / help text |
| `datatype` | string | Closed vocabulary below |
| `cardinality` | string | `one` or `many` |
| `struct_type` | string or null | TR structure name when `datatype` is `struct` |
| `representations` | object | XMP / IPTC IIM / EXIF / ExifTool (null when absent) |
| `mapping_notes` | string | IPTC user notes; empty when the TR has none |
| `source` | object | Same shape as envelope `source` |

`id` is `iptc.photo.` plus a camelCase token derived from
`standard_property_name` (punctuation stripped, words concatenated). The TR
object key is not used as the id so names like Creator become
`iptc.photo.creator`. Video properties use the same derivation under
`iptc.video.` (see Domain rule).

## Cast groups (`registry/casts/*.json`)

Curated L2 links (C7, C9). One group object per file. Regenerating
`src/generated/cast_rules.hpp` must be byte-identical.

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | string | Group id (`capturePosition`, `videoCreated`, …) |
| `direction` | string | `up`, `down`, or `side` |
| `partial` | bool | Curated / incomplete |
| `approximate` | bool | Apply only with `CastOptions::include_approximate` |
| `one_way` | bool | No reverse group |
| `citation` | string | Why this is a cast |
| `source_priority` | string[] | Rule ids in evaluation order |
| `rules` | array | Member rules |

Each rule has `id`, `source`, `target`, `heuristic` (`H1`–`H21`), and
`citation`. Endpoints are `{kind, key, field?, index?}` with `kind`
`base_key`, `property`, or `property_field`.

## Representations

| Field | Type | Meaning |
| --- | --- | --- |
| `xmp` | object or null | `namespace` (URI) + `property` (prefixed name, e.g. `dc:creator`) |
| `iptc_iim` | object or null | `dataset` (e.g. `2:80`); `name` when the TR provides `IIMname` |
| `exif` | object or null | `tag` from the TR (`etEXIF`, else `EXIFid`) |
| `exiftool` | object or null | Photo: `tag` from TR `etXMP` (properties) or `etTag` (struct fields), local name only |
| `quicktime` | object or null | VMH only: `key` from the Apple QuickTime mapping (e.g. `com.apple.quicktime.creationdate`) |
| `ebucore` | object or null | VMH only: `path` from the EBUCore mapping (e.g. `date/created`) |

Photo records omit `quicktime` and `ebucore` and include `exiftool`. Video records include
`quicktime` / `ebucore` (null when the mapping artifact has no value) and keep
`iptc_iim` / `exif` / `exiftool` null — VMH does not define IIM, EXIF, or ExifTool
`etTag` names.

Struct fields additionally store `et_tag` (TR `etTag`, e.g. `PersonName`) beside
`representations` so codegen can emit ExifTool struct-field aliases (C14b).

XMP namespace URIs are the established IPTC/Adobe/PLUS namespaces already used
by the Photo Metadata Standard. The importer fails closed on an unknown XMP
prefix rather than inventing a URI.

EXIF columns from the IPTC Photo Metadata **Mapping Guidelines** (HTML, not
machine-readable) are a curated overlay (`registry/mappings/iptc-exif-overlay.json`)
merged by `tools/registry/generate_cpp.py`. EXIF values present in the Technical
Reference itself are imported here; the overlay fills gaps and must not disagree
with a TR tag. The overlay is marked `partial: true` until Stage 6 completes it.

## EXIF overlay (`registry/mappings/iptc-exif-overlay.json`)

| Field | Type | Meaning |
| --- | --- | --- |
| `partial` | boolean | `true` while the overlay is a Stage 4 subset |
| `source` | object | Mapping Guidelines document, version, URL, retrieval date, note |
| `mappings` | array | Sorted by `id`; each entry is `id` + `exif_tag`, plus `struct_property` on struct-field rows |

`id` is a registry property id or struct-field id. Overlay tags use the same
ExifTool-style names as the Technical Reference (`IFD0:Artist`, `GPS:GPSLatitude`).
Struct-field GPS rows require `struct_property` (currently `locationCreated`) so
the overlay says which Location property they belong to (C8).

## Cross-media accessor map (`registry/mappings/cross-media-accessors.json`)

Hand-curated decision artifact for Phase 2 convenience accessors. Rows are **not**
inferred from name equality. `tools/registry/generate_cpp.py` validates every id
against the imported registries and that declared datatype/cardinality match,
then emits `src/generated/cross_media_accessors.hpp`.

| Field | Type | Meaning |
| --- | --- | --- |
| `document` | string | Map title |
| `version` | string | Map schema version |
| `source` | object | `document` + `note` citing the Phase 2 analysis record |
| `accessors` | array | One row per catalog concept |

Each accessor row:

| Field | Type | Meaning |
| --- | --- | --- |
| `concept` | string | Accessor name (`title`, `locationCreated`, …) |
| `photo_ids` | array of string | 1 or 2 `iptc.photo.*` ids (`shownEvent` has two) |
| `video_ids` | array of string | 1 or 2 `iptc.video.*` ids |
| `audio_ids` | array | Reserved; must be `[]` until an audio registry exists |
| `tier` | integer | `1` (passthrough), `2` (transpose), or `3` (renamed) |
| `transposition` | string | Closed vocabulary below |
| `photo_datatype` | string | Registry datatype of the **first** photo id |
| `photo_cardinality` | string | Registry cardinality of the first photo id |
| `video_datatype` | string | Registry datatype of the first video id |
| `video_cardinality` | string | Registry cardinality of the first video id |
| `deferred` | boolean | `true` to skip until review accepts the row (`objectShown`) |
| `notes` | string | Lossy directions and rename rationale |

`transposition` vocabulary: `passthrough`, `string_to_lang_alt`,
`string_list_to_lang_alt`, `lang_alt_to_string`, `names_to_entity_list`,
`uri_to_cv_term`, `struct_field_subset`, `list_to_single`, `name_uri_to_entity`.

GPS is a field on `locationCreated` / `locationShot`, not a separate accessor
row. There is no `exif.*` property domain (C17).

## C++ table (`src/generated/`)

Session 07 generates `umm::PropertyDef` rows from `properties` (not structs).
JSON `datatype` + `cardinality` map onto `include/umm/registry.hpp`:

| Registry JSON | `umm::Datatype` |
| --- | --- |
| `string`/`uri` + `one` | `text` |
| `string`/`uri` + `many` | `text_list` |
| `lang-alt` | `lang_alt` |
| `integer` | `integer` |
| `number` | `real` |
| `date-time` | `date_time` |
| `boolean` | `boolean` |
| `struct` + `one` | `structure` |
| `struct` + `many` | `structure_list` |

`boolean` is emitted for VMH boolean properties. `rational` remains reserved
for later value shapes. `Datatype::gps_coordinate` was removed in session 48
(Location GPS fields are numbers). Generated C++ is UTF-8 with LF newlines.
`.gitattributes` pins `src/generated/**` to LF.

`umm::Representations` also carries `quicktime_key` and `ebucore` (empty for
photo rows). Only XMP and QuickTime mappings are used at runtime in Stage 7;
EBUCore is imported as data.

Session 37 also emits `cross_media_accessors.hpp`: `CrossMediaAccessorDef` rows
with both-side `Datatype`/`Cardinality` so accessor code does not re-read the
registry at runtime. The table is unused until session 39.

## Struct record

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | string | `iptc.photo.struct.<TR struct name>` |
| `name` | string | TR structure name, e.g. `Location` |
| `standard` | string | Same as envelope `standard` |
| `standard_version` | string | Same as envelope `standard_version` |
| `source` | object | Same shape as envelope `source` |
| `fields` | array | Member records, ordered by TR key |

Struct **field** records use the property datatype/cardinality/representation
vocabulary, with `id` `iptc.photo.struct.<Struct>.<tr_key>` and
`standard_property_name` from the TR field name. Location Created / Location
Shown remain structured: they reference `Location` rather than being flattened.

The TR ImageRegion wildcard member `$anypmdproperty` (`datatype: any`) is
preserved.

## Source object

| Field | Type | Meaning |
| --- | --- | --- |
| `document` | string | Source document title |
| `version` | string | Technical Reference / standard version |
| `url` | string | Canonical retrieval URL |
| `retrieval_date` | string | ISO date `YYYY-MM-DD` |

Every property and struct record repeats `standard`, `standard_version`, and
`source` so provenance is queryable without the envelope.

## Datatype vocabulary

Closed set. Values are resolved from the TR `datatype` + `dataformat` pair.
Unknown pairs fail the importer.

| Registry `datatype` | Technical Reference |
| --- | --- |
| `string` | `datatype=string` with no `dataformat` |
| `uri` | `datatype=string` and `dataformat` `uri` or `url` |
| `date-time` | `datatype=string` and `dataformat=date-time` |
| `number` | `datatype=number` with no `dataformat` |
| `integer` | `datatype=number` and `dataformat=integer` |
| `lang-alt` | `datatype=struct` and `dataformat=AltLang` |
| `struct` | `datatype=struct` and `dataformat` naming an `ipmd_struct` type |
| `any` | `datatype=any` (TR wildcard member) |

## Cardinality vocabulary

| Registry | Technical Reference `propoccurrence` |
| --- | --- |
| `one` | `single` |
| `many` | `multi` |

Unknown `propoccurrence` values fail the importer.

## Schema vocabulary (IPTC Photo)

| Registry `schema` | TR `ipmdschema` |
| --- | --- |
| `Core 1.5` | `IptcCore` |
| `Extension 1.9` | `IptcExt` |
| `PLUS` | `PLUS` (members of IPTC Extension structures that IPTC adopts from PLUS) |

Unknown `ipmdschema` values fail the importer. Top-level `properties` are only
Core 1.5 + Extension 1.9; envelope counts must equal the TR's own `ipmd_top`
counts for those two schemas.

## Domain rule (photo vs video)

Photo properties stay `iptc.photo.*`. Video-domain semantics come from the IPTC
Video Metadata Hub as `iptc.video.*`. Shared concepts (creator, description,
date created, location, …) are **distinct registry entries** with their
VMH-defined mappings. libumm does not invent a merged super-schema
(concept.md §10).

## Envelope (`registry/iptc-video/iptc-video.json`)

| Field | Type | Meaning |
| --- | --- | --- |
| `standard` | string | `IPTC Video Metadata Hub` |
| `standard_version` | string | Recommendation version, e.g. `1.7` |
| `source` | object | Provenance of the vendored VMH artifacts (see Source) |
| `counts` | object | `properties`, `administrative`, `descriptive`, `rights`, `technical`, `time_marker`, `structs` |
| `properties` | array | One record per VMH top-level property |
| `structs` | array | Property structures from the VMH properties table |

`schema` on video properties is the VMH property group:

| Registry `schema` | VMH property group |
| --- | --- |
| `Administrative` | Administrative fields |
| `Descriptive` | Fields describing audio/visual content |
| `Rights` | Rights fields |
| `Technical` | Technical fields |
| `Time marker` | Time marker |

Video ids are `iptc.video.` plus a camelCase token from the VMH property name.
Struct ids are `iptc.video.struct.<Name>` from the structure header, with field
ids using the PVMD JSON property token when present.

## VMH datatype mapping

Closed set. Values are resolved from the PVMD JSON Data Type column
(`type/format/occurrence`); the HTML Basic Type/Cardinality column is the
fallback when JSON is `NA` or empty. Unknown types fail the importer.

| Registry `datatype` | PVMD JSON Data Type |
| --- | --- |
| `string` | `string//` or `string//enum` |
| `uri` | `string/uri` or `string/url` |
| `date-time` | `string/date-time` |
| `number` | `number//` |
| `integer` | `number/integer` |
| `boolean` | `boolean//` |
| `lang-alt` | `object/AltLang` |
| `struct` | `object/<StructureName>` |

`array` in the third JSON slot is cardinality `many`; otherwise `one`.

## VMH source artifacts

`tools/registry/import_vmh.py` reads:

1. `iptc-vmhub-1.7-schema.json` — property count / PVMD JSON names (checksummed)
2. `IPTC-VideoMetadataHub-props-Rec_1.7.html` — names, definitions, XMP, types, structures
3. `IPTC-VideoMetadataHub-mapping-AppleQT-Rec_1.7.html` — Apple QuickTime keys and EBUCore paths

`SOURCE.md` records version, URLs, retrieval date, and SHA-256 for each file
(decision M5). The importer verifies checksums before parsing.

The VMH 1.7 mapping HTML labels are shifted relative to the master sheet
columns: Apple QuickTime is taken from the column headed `Apple Quicktime`;
EBUCore paths are taken from the labeled `EBUcore` cell when that cell is an
EBUCore path, otherwise from the published `PVMD JSON` mapping cell when it
contains an EBUCore path (`ebuCore…` or a `/`-separated EBUCore location).
XMP on video properties always comes from the properties table, not the
mapping page.
