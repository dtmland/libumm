# Capability table sources

Vendored as JSON under `registry/capabilities/` so `supported-types.md` and
the capability engine stay offline and drift-checked (decision **M2**).

The upstream tables remain the source of truth for *file-type* coverage.
libumm transcribes them; it does not define new container support.

document: Exiv2 FILE TYPES table
version: snapshot for libumm session 15 (Exiv2 pin in tools/build/backends.env)
url: https://github.com/Exiv2/exiv2/blob/main/exiv2.md
retrieval_date: 2026-09-27

document: Exiv2 GPS write example (Exif.GPSInfo.GPSLatitude)
version: same exiv2.md
url: https://github.com/Exiv2/exiv2/blob/main/exiv2.md
retrieval_date: 2026-09-27

document: ExifTool file types and meta-information formats
version: snapshot for libumm session 15 (ExifTool pin in tools/build/backends.env)
url: https://github.com/exiftool/exiftool/blob/master/README
retrieval_date: 2026-09-27

document: ExifTool geotagging (track → GPS tags, including QuickTime GPSCoordinates)
version: geotag.html
url: https://github.com/exiftool/exiftool/blob/master/html/geotag.html
retrieval_date: 2026-09-27

notes: BMFF types (AVIF, CR3, HEIF, HEIC) are an Exiv2 build option
(enable_bmff=1). libumm acquires Exiv2 with EXIV2_ENABLE_BMFF ON
(cmake/LibummExiv2.cmake). Naked JPEG XL codestreams do not contain
Exif/IPTC/XMP. Exiv2 video is rudimentary read only.
