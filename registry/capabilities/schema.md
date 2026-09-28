# libumm capability records

Machine-readable backend coverage transcribed from
[supported-types.md](../../supported-types.md) §1–§4 (concept.md §14;
decision **M2**). Session 15 generates `supported-types.md` and the C++
lookup tables from these files.

libumm does not invent file-type support. Each record copies Exiv2 / ExifTool
published coverage and records provenance so drift is testable.

## Files

| Path | Role |
| --- | --- |
| `registry/capabilities/SOURCE.md` | Upstream tables, URLs, retrieval dates |
| `registry/capabilities/exiv2.json` | Exiv2 still/RAW/video types and categories |
| `registry/capabilities/exiftool.json` | ExifTool overlap types, extra types, meta-formats |
| `registry/capabilities/policy.json` | Preferred backend, sidecar recommendation, extensions |
| `tools/registry/generate_supported_types.py` | Regenerates `supported-types.md` and `src/generated/capabilities_data.hpp` |

JSON is UTF-8, LF newlines, 2-space indent, a trailing newline, and stable
key/array ordering. `.gitattributes` pins `registry/**/*.json` to LF.

## Access vocabulary

Closed set used for category and location fields:

| JSON | Meaning | Exiv2 table | ExifTool table |
| --- | --- | --- | --- |
| `none` | not applicable / not supported | `-` | omitted / not listed |
| `read` | read only | `Read` | `r` |
| `read_write` | read and write existing metadata | `Read/Write` | `r/w` |
| `create` | read, write, and create from scratch | (not used by Exiv2) | `r/w/c` |

## Exiv2 type record (`exiv2.json` → `types`)

| Field | Type | Meaning |
| --- | --- | --- |
| `type` | string | Container name as in Exiv2 FILE TYPES (`JPEG`, `CR3`, `XMP`) |
| `identify_only` | boolean | Recognized with MIME/dimensions only; no metadata categories |
| `bmff` | boolean | Requires Exiv2 `enable_bmff=1` (AVIF, CR3, HEIF, HEIC) |
| `video` | boolean | Rudimentary Exiv2 video/RIFF read (no documented location API) |
| `video_kind` | string | Optional (`QuickTime`, `Matroska`, `RIFF`, …) |
| `listed` | boolean | Present in the official Exiv2 FILE TYPES table (false for video) |
| `categories` | object | `exif`, `iptc`, `xmp`, `comments`, `icc`, `thumbnail` → access |
| `location` | object | `gps_exif`, `named_place`, `xmp_location`, `container_gps`, `geotiff` |
| `notes` | string | Build conditions or table footnotes; empty when none |

Identify-only types must have every category and location field `none`.

**Derive Exiv2 location** (supported-types.md §3; generator verifies stills):

- `gps_exif` follows `categories.exif`
- `xmp_location` follows `categories.xmp`
- `named_place` is the stronger of `categories.iptc` and `categories.xmp`
- `container_gps` and `geotiff` are `none` (not documented Exiv2 categories)
- Video rows are not a documented GPS/IPTC/XMP location API: all location `none`

## ExifTool type record (`exiftool.json`)

Overlap types (also in Exiv2) live in `overlap_types` with the same `type` name.

| Field | Type | Meaning |
| --- | --- | --- |
| `type` | string | ExifTool type name |
| `listed` | boolean | False when ExifTool does not list the type (TGA) |
| `file` | string | File-level access (`none` / `read` / `read_write` / `create`) |
| `categories` | object | Same keys as Exiv2 when known; omitted on extra types |
| `location` | object | Same location keys as Exiv2 |
| `notes` | string | Capability caveats |

`extra_groups` hold ExifTool-only types (supported-types.md §4). Extra groups
may set `coverage: "partial"` when only the file-level r/w/c flag is
transcribed (no per-category columns). Location for extra types follows the
file flag unless overridden.

`meta_formats` lists ExifTool meta-information formats (supported-types.md §5),
including **GPS** and **GeoTIFF** as their own r/w/c rows.

## Policy record (`policy.json` → `types`)

Derivation rules from supported-types.md §3 and §6 encoded as data:

| Field | Type | Meaning |
| --- | --- | --- |
| `type` | string | Must match an Exiv2 and/or ExifTool type name |
| `preferred_backend` | string | `exiv2` or `exiftool` |
| `sidecar_recommended` | boolean | XMP sidecar advisable for writes |
| `extensions` | array of string | Leading-dot ASCII extensions, lowercase |

`umm::capabilities(path)` maps the path extension (and JPEG/XMP magic) through
this table. Unknown types return `ErrorCode::unsupported_type`.

## Generated outputs

- `supported-types.md` — narrative section order preserved; tables generated
- `src/generated/capabilities_data.hpp` — committed C++ lookup tables

Editing JSON and regenerating must be byte-identical across runs. Stale
generated files fail `tests/build/test_capabilities.py`.
