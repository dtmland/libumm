# libumm property registry record

This is the written definition of the machine-readable registry produced from
standards Technical References (concept.md §20; decisions **S2**, **M5**). Session
06 imports IPTC Photo Metadata only.

The registry does not invent property semantics. Each record copies identifiers,
definitions, datatypes, cardinality, and representations from the source
standard and records provenance so "which standards do you implement?" is
answered from the data alone.

## Files

| Path | Role |
| --- | --- |
| `registry/sources/` | Vendored Technical Reference input plus `SOURCE.md` |
| `registry/iptc-photo/iptc-photo.json` | Importer output (generated-but-committed) |
| `tools/registry/import_iptc.py` | Stdlib-only importer |

Output JSON is UTF-8, LF newlines, 2-space indent, a trailing newline, and
stable key/array ordering. Re-running the importer must be byte-identical.

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
| `representations` | object | XMP / IPTC IIM / EXIF (null when absent) |
| `mapping_notes` | string | IPTC user notes; empty when the TR has none |
| `source` | object | Same shape as envelope `source` |

`id` is `iptc.photo.` plus a camelCase token derived from
`standard_property_name` (punctuation stripped, words concatenated). The TR
object key is not used as the id so names like Creator become
`iptc.photo.creator`.

## Representations

| Field | Type | Meaning |
| --- | --- | --- |
| `xmp` | object or null | `namespace` (URI) + `property` (prefixed name, e.g. `dc:creator`) |
| `iptc_iim` | object or null | `dataset` (e.g. `2:80`); `name` when the TR provides `IIMname` |
| `exif` | object or null | `tag` from the TR (`etEXIF`, else `EXIFid`) |

XMP namespace URIs are the established IPTC/Adobe/PLUS namespaces already used
by the Photo Metadata Standard. The importer fails closed on an unknown XMP
prefix rather than inventing a URI.

EXIF columns from the IPTC Photo Metadata **Mapping Guidelines** (HTML, not
machine-readable) are out of scope here and may land as a curated overlay in
session 07. EXIF values present in the Technical Reference itself are imported.

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
