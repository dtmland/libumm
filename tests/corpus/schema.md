# Tier B corpus manifest

Checksum-pinned third-party samples fetched at test time (test-media-plan §3;
session 27; decisions **M6**, **R4**). Files are never committed; the fetcher
writes them under `.cache/corpus/`.

JSON is UTF-8, LF newlines, 2-space indent, a trailing newline, and stable
key/array ordering.

## File

`tests/corpus/manifest.json`

## Root

| Field | Type | Meaning |
| --- | --- | --- |
| `version` | integer | Manifest schema version (`1`) |
| `samples` | array | Ordered sample records |

## Sample record

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | string | Stable identifier (`[a-z0-9-]+`) |
| `path` | string | POSIX path relative to `.cache/corpus/` (no `..`) |
| `url` | string | `https://` download URL pinned to a tag or commit |
| `sha256` | string | Lowercase hex SHA-256 of the exact bytes |
| `bytes` | integer | Exact byte size; mismatch is a fetch failure |
| `file_type` | string | Capability type name (`JPEG`, `RW2`, `MOV`, …) |
| `license` | string | License note for the *referenced* upstream sample |
| `capability` | string | The test capability this sample exists to verify |

Sources are referenced in place from the pinned ExifTool (or Exiv2) tree, or
CC0 camera samples. The fetcher is fail-closed: size or checksum mismatch
deletes the bad file and exits non-zero. When Tier B is not requested, tests
skip with a visible status instead of downloading.
