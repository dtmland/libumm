# Cross-backend divergence ledger

Machine-readable expected differences for the session 28 comparison suite
(concept.md §30; `docs/reconciliation-policy.md` Cross-backend identity).

The suite writes with one backend, reads with the other, and compares canonical
`Metadata` using the reconciliation policy's **equivalence** rules. Byte-equality
of files or raw tags is not the goal. Capability data drives skip logic: a pair
is compared only where both backends claim the needed access.

Unexplained divergence fails CI. Accepted divergence lives here so it is
documented rather than silently tolerated.

JSON is UTF-8, LF newlines, 2-space indent, a trailing newline, and stable
key/array ordering. `.gitattributes` pins `tests/verification/**/*.json` to LF.

## File

`tests/verification/ledger.json`

## Root

| Field | Type | Meaning |
| --- | --- | --- |
| `version` | integer | Ledger schema version (`1`) |
| `entries` | array | Ordered accepted-divergence records |

## Entry record

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | string | Stable identifier (`[a-z0-9-]+`) |
| `file_types` | array of string | Capability type names this entry applies to; empty means any suite type |
| `category` | string | Optional category (`exif`, `iptc_iim`, `xmp`, `gps_exif`, `named_place`, `xmp_location`, `container_gps`) |
| `property_id` | string | Optional canonical property id; empty means any property in the case |
| `write_backend` | string | `exiv2`, `exiftool`, or empty (any) |
| `read_backend` | string | `exiv2`, `exiftool`, or empty (any) |
| `kind` | string | `representation`, `capability`, or `one-directional` |
| `reason` | string | Required human-readable explanation; empty reasons are invalid |

### `kind`

| Value | Meaning |
| --- | --- |
| `representation` | Both backends claim access; canonical values or provenance shape differ in a documented, equivalent way |
| `capability` | Informational note for a type×category one backend cannot access (skip logic still comes from capability data) |
| `one-directional` | Write is only valid on one backend (MP4/MOV ExifTool-primary); read the other only where capable |

Capability skips do **not** require a ledger entry. Entries exist so every
*accepted* comparison mismatch has a reason.
