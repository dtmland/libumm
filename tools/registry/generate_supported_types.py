#!/usr/bin/env python3
"""Generate supported-types.md and C++ capability tables from registry/capabilities."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CAP_DIR = REPO_ROOT / "registry" / "capabilities"
DEFAULT_MARKDOWN = REPO_ROOT / "supported-types.md"
DEFAULT_HPP = REPO_ROOT / "src" / "generated" / "capabilities_data.hpp"

ACCESS_RANK = {"none": 0, "read": 1, "read_write": 2, "create": 3}
CAT_KEYS = ("exif", "iptc", "xmp", "comments", "icc", "thumbnail")
LOC_KEYS = ("gps_exif", "named_place", "xmp_location", "container_gps", "geotiff")
ACCESS_CPP = {
    "none": "Access::none",
    "read": "Access::read",
    "read_write": "Access::read_write",
    "create": "Access::create",
}

SECTION2_GROUPS = [
    (
        ["JPEG", "TIFF", "DNG", "JP2", "PSD", "EXV", "ARW", "CR2", "NEF", "ORF", "PEF", "SRW"],
        "Read/Write (see category table)",
        "r/w (EXV also **c**)",
        "Full Exif+IPTC+XMP in Exiv2",
        "**Both** read/write GPS and named place",
    ),
    (
        ["CRW", "DCP"],
        "Read/Write (see category table)",
        "r/w",
        "**No IPTC/XMP in Exiv2**",
        "Exiv2: **GPS only**. ExifTool: broader",
    ),
    (
        ["PNG"],
        "IPTC/XMP/ICC Read/Write; **no Exif**",
        "r/w",
        "ExifTool can read/write PNG Exif; Exiv2’s official table does not",
        "Exiv2: named place + **XMP GPS only** (no EXIF GPS). ExifTool: EXIF GPS too",
    ),
    (
        ["WEBP"],
        "Exif/XMP Read/Write; **no IPTC**",
        "r/w",
        "Exiv2 has no IPTC category for WebP; ExifTool writes WebP as Exif/XMP",
        "Exiv2: GPS + XMP place (**no IPTC**). ExifTool: Exif/XMP",
    ),
    (
        ["EPS"],
        "XMP Read/Write only",
        "r/w",
        "Broader than XMP-only",
        "Exiv2: **XMP location only**",
    ),
    (
        ["XMP"],
        "XMP Read/Write",
        "r/w/**c**",
        "ExifTool can create sidecars from scratch",
        "**Both** XMP GPS + named place; ExifTool can **create**",
    ),
    (
        ["PGF"],
        "**Read/Write**",
        "**r**",
        "Exiv2 is stronger on write",
        "Exiv2 **writes** location; ExifTool **read-only**",
    ),
    (
        ["AVIF", "HEIC", "HEIF", "JXL"],
        "**Read** (BMFF build)",
        "**r/w**",
        "Write requires ExifTool",
        "Exiv2 **read** location; **write via ExifTool**",
    ),
    (
        ["CR3"],
        "**Read** (BMFF build)",
        "**r/w**",
        "Matches the concept-doc CR3 example",
        "Exiv2 **read** location; **write via ExifTool**",
    ),
    (
        ["MRW", "RAF", "RW2", "SR2"],
        "**Read**",
        "**r/w**",
        "Write requires ExifTool",
        "Exiv2 **read** location; **write via ExifTool**",
    ),
    (
        ["GIF"],
        "Identify only",
        "**r/w**",
        "",
        "Exiv2: **none**. ExifTool: **r/w**",
    ),
    (
        ["BMP"],
        "Identify only",
        "**r**",
        "Still no ExifTool write",
        "Exiv2: **none**. ExifTool: **read only**",
    ),
    (
        ["TGA"],
        "Identify only",
        "*not listed*",
        "Exiv2-only recognition",
        "Exiv2: **none**. ExifTool: not listed",
    ),
    (
        ["MOV", "MP4"],
        "Rudimentary read",
        "**r/w**",
        "",
        "Exiv2: **no documented location write**. ExifTool: **r/w** (`GPSCoordinates` / XMP)",
    ),
    (
        ["MKV", "AVI", "WAV", "ASF"],
        "Rudimentary read",
        "**r**",
        "Still no write in either backend",
        "**Read-only** at best; **no location write** in either backend",
    ),
]

SECTION3_GROUPS = [
    (
        ["JPEG", "TIFF", "DNG", "JP2", "PSD", "EXV", "ARW", "CR2", "NEF", "ORF", "PEF", "SRW"],
        "Read/Write",
        "Read/Write (IPTC+XMP)",
        "**Read/Write**",
        "**r/w**",
    ),
    (
        ["PGF"],
        "Read/Write",
        "Read/Write (IPTC+XMP)",
        "**Read/Write**",
        "**r only** — prefer Exiv2 to **write** location",
    ),
    (
        ["PNG"],
        "**-** (no Exif)",
        "Read/Write (IPTC+XMP; XMP may hold GPS)",
        "**Read/Write** (not EXIF GPS)",
        "**r/w** including EXIF GPS",
    ),
    (
        ["WEBP"],
        "Read/Write",
        "Read/Write **XMP only** (no IPTC)",
        "**Read/Write**",
        "**r/w** (Exif/XMP)",
    ),
    (
        ["CRW", "DCP"],
        "Read/Write",
        "**-**",
        "**Read/Write GPS only**",
        "**r/w**",
    ),
    (
        ["EPS"],
        "**-**",
        "Read/Write **XMP only**",
        "**Read/Write**",
        "**r/w**",
    ),
    (
        ["XMP"],
        "**-**",
        "Read/Write **XMP only**",
        "**Read/Write**",
        "**r/w/c**",
    ),
    (
        ["AVIF", "HEIC", "HEIF", "JXL", "CR3"],
        "Read (BMFF build)",
        "Read (IPTC+XMP)",
        "**Read only**",
        "**r/w** — **write location via ExifTool**",
    ),
    (
        ["MRW", "RAF", "RW2", "SR2"],
        "Read",
        "Read (IPTC+XMP)",
        "**Read only**",
        "**r/w** — **write location via ExifTool**",
    ),
    (["BMP"], "**-**", "**-**", "**None**", "**r** only"),
    (["GIF"], "**-**", "**-**", "**None**", "**r/w** — **write location via ExifTool**"),
    (["TGA"], "**-**", "**-**", "**None**", "*not listed*"),
    (
        ["MOV", "MP4"],
        "not documented as GPS",
        "not documented as IPTC/XMP",
        "Rudimentary **read**; **no write**",
        "**r/w** (`GPSCoordinates` / XMP)",
    ),
    (
        ["MKV", "AVI", "WAV", "ASF"],
        "not documented as GPS",
        "not documented as IPTC/XMP",
        "Rudimentary **read**; **no write**",
        "**r** — **no location write**",
    ),
]


class GeneratorError(RuntimeError):
    """Capability data failed closed."""


def load_json(path: Path) -> dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise GeneratorError(f"{path} is not a JSON object")
    return data


def stronger(left: str, right: str) -> str:
    return left if ACCESS_RANK[left] >= ACCESS_RANK[right] else right


def derive_exiv2_location(categories: dict[str, str], *, video: bool) -> dict[str, str]:
    if video:
        return {key: "none" for key in LOC_KEYS}
    return {
        "gps_exif": categories["exif"],
        "named_place": stronger(categories["iptc"], categories["xmp"]),
        "xmp_location": categories["xmp"],
        "container_gps": "none",
        "geotiff": "none",
    }


def require_access(value: str, *, path: str) -> str:
    if value not in ACCESS_RANK:
        raise GeneratorError(f"{path}: unknown access {value!r}")
    return value


def validate_categories(categories: dict[str, Any], *, path: str) -> dict[str, str]:
    if set(categories) != set(CAT_KEYS):
        raise GeneratorError(f"{path}: categories must be {list(CAT_KEYS)}")
    return {key: require_access(categories[key], path=f"{path}.{key}") for key in CAT_KEYS}


def validate_location(location: dict[str, Any], *, path: str) -> dict[str, str]:
    if set(location) != set(LOC_KEYS):
        raise GeneratorError(f"{path}: location must be {list(LOC_KEYS)}")
    return {key: require_access(location[key], path=f"{path}.{key}") for key in LOC_KEYS}


def load_exiv2(path: Path) -> list[dict[str, Any]]:
    data = load_json(path)
    types = data.get("types")
    if not isinstance(types, list) or not types:
        raise GeneratorError(f"{path}: types must be a non-empty array")
    seen: set[str] = set()
    out: list[dict[str, Any]] = []
    for i, raw in enumerate(types):
        name = raw["type"]
        if name in seen:
            raise GeneratorError(f"{path}: duplicate type {name}")
        seen.add(name)
        categories = validate_categories(raw["categories"], path=f"exiv2[{i}].categories")
        location = validate_location(raw["location"], path=f"exiv2[{i}].location")
        video = bool(raw.get("video"))
        expected = derive_exiv2_location(categories, video=video)
        if location != expected:
            raise GeneratorError(
                f"{path}: {name} location {location} != derived {expected}"
            )
        if raw.get("identify_only") and any(categories[key] != "none" for key in CAT_KEYS):
            raise GeneratorError(f"{path}: {name} identify-only has metadata categories")
        out.append(raw)
    return out


def load_exiftool(path: Path) -> dict[str, Any]:
    data = load_json(path)
    for i, raw in enumerate(data.get("overlap_types", [])):
        validate_location(raw["location"], path=f"exiftool.overlap[{i}].location")
        require_access(raw["file"], path=f"exiftool.overlap[{i}].file")
        if "categories" in raw:
            validate_categories(raw["categories"], path=f"exiftool.overlap[{i}].categories")
    for group in data.get("extra_groups", []):
        for i, raw in enumerate(group.get("types", [])):
            validate_location(
                raw["location"],
                path=f"exiftool.extra.{group.get('id')}[{i}].location",
            )
            require_access(raw["file"], path=f"exiftool.extra.{group.get('id')}[{i}].file")
    return data


def by_type(rows: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    return {row["type"]: row for row in rows}


def exiv2_cell(access: str) -> str:
    return {"none": "-", "read": "Read", "read_write": "Read/Write", "create": "Read/Write"}[
        access
    ]


def exiftool_file_cell(access: str, listed: bool) -> str:
    if not listed:
        return "*not listed*"
    return {"none": "-", "read": "r", "read_write": "r/w", "create": "r/w/c"}[access]


def join_types(names: list[str]) -> str:
    if names == ["MOV", "MP4"]:
        return "MOV / MP4"
    if names == ["XMP"]:
        return "XMP sidecar"
    return ", ".join(names)


def triple_table(rows: list[dict[str, Any]]) -> str:
    items = [(row["type"], exiftool_file_cell(row["file"], row.get("listed", True))) for row in rows]
    while len(items) % 3:
        items.append(("", ""))
    lines = [
        "| Type | ExifTool | Type | ExifTool | Type | ExifTool |",
        "|---|---|---|---|---|---|",
    ]
    for i in range(0, len(items), 3):
        cells: list[str] = []
        for name, access in items[i : i + 3]:
            cells.extend([name, access])
        lines.append("| " + " | ".join(cells) + " |")
    return "\n".join(lines)


def cpp_string(value: str) -> str:
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
    )
    return f'"{escaped}"'


def empty_categories() -> dict[str, str]:
    return {key: "none" for key in CAT_KEYS}


def markdown_for(
    exiv2_types: list[dict[str, Any]],
    exiftool: dict[str, Any],
) -> str:
    listed = [row for row in exiv2_types if row.get("listed", True) and not row.get("video")]
    identify = [row["type"] for row in listed if row.get("identify_only")]
    video = [row for row in exiv2_types if row.get("video")]
    overlap = by_type(exiftool["overlap_types"])

    lines: list[str] = []
    add = lines.append

    add("# Backend file-type coverage: Exiv2 and ExifTool")
    add("")
    add(
        "> **GENERATED** from `registry/capabilities/` by "
        "`tools/registry/generate_supported_types.py`."
    )
    add(
        "> Do not edit by hand. Decision **M2**: capability data is machine-readable; "
        "this document is generated output. Tier A CI probes pinned backends against "
        "the data so drift fails a test."
    )
    add("")
    add(
        "This is a snapshot of **file-type** (container) support in the two backends "
        "named in [concept.md](concept.md). It is **not** a list of IPTC/EXIF/XMP "
        "*properties*."
    )
    add("")
    add(
        "**Location is called out separately.** `getLocation()` / `setLocation()` in "
        "[concept.md](concept.md) is not a file-type flag. Coordinates and named place "
        "live in different encodings, and Exiv2 vs ExifTool do not offer the same "
        "location **read** or **write** path for the same type. See "
        "[§3 Location metadata](#3-location-metadata-gps-and-named-place)."
    )
    add("")
    add("Sources (check these for drift):")
    add("")
    add(
        "- Exiv2 FILE TYPES table: [exiv2.md](https://github.com/Exiv2/exiv2/blob/main/exiv2.md)"
    )
    add(
        "- Exiv2 GPS write example (`Exif.GPSInfo.GPSLatitude`): same "
        "[exiv2.md](https://github.com/Exiv2/exiv2/blob/main/exiv2.md)"
    )
    add(
        "- ExifTool file types and meta-information formats: "
        "[ExifTool README](https://github.com/exiftool/exiftool/blob/master/README) "
        "(lists **GPS** and **GeoTIFF** as their own r/w/c formats)"
    )
    add(
        "- ExifTool geotagging (track → GPS tags, including QuickTime `GPSCoordinates`): "
        "[geotag.html](https://github.com/exiftool/exiftool/blob/master/html/geotag.html)"
    )
    add("")
    add(
        "Capability discovery in libumm must be **per backend, per file type, and per "
        "metadata category** — including **location** (GPS coordinates vs IPTC/XMP named "
        "place vs container GPS). A static “this extension is supported” table is not enough."
    )
    add("")
    add("---")
    add("")
    add("## Is Exiv2 a subset of ExifTool?")
    add("")
    add("**Almost for still-image / RAW *file types*, but not strictly — and not for write capability.**")
    add("")
    add("| Claim | Reality |")
    add("|---|---|")
    add(
        "| Exiv2 file types ⊂ ExifTool file types | **Mostly.** ExifTool lists every "
        "Exiv2 still-image/RAW type except **TGA** (Exiv2 only identifies TGA: MIME type "
        "and dimensions). |"
    )
    add(
        "| Exiv2 read/write ⊂ ExifTool read/write | **No.** ExifTool writes many types "
        "Exiv2 can only read. The reverse also happens: Exiv2 **writes PGF**; ExifTool is "
        "**read-only** for PGF. |"
    )
    add(
        "| Same file type ⇒ same metadata categories | **No.** Example: Exiv2’s table has "
        "**no Exif** on PNG and **no IPTC** on WebP. ExifTool lists both containers as "
        "**r/w** (PNG including Exif; WebP via Exif/XMP rather than a separate IPTC column). |"
    )
    add(
        "| Video/audio/documents | Exiv2 has only **rudimentary read** of a few video/RIFF "
        "types. ExifTool covers video, audio, documents, fonts, archives, and more. |"
    )
    add(
        "| Same file type ⇒ same **location** metadata | **No.** Location is several "
        "encodings (EXIF GPS, IPTC/XMP named place, XMP GPS, QuickTime `GPSCoordinates`, "
        "GeoTIFF). Exiv2 has **no** location on identify-only types (BMP/GIF/TGA), "
        "**no EXIF GPS on PNG**, **no IPTC named place on WebP**, and **no video location "
        "write**. ExifTool lists **GPS** as r/w/c and writes location on many types Exiv2 "
        "can only read (CR3, HEIC, AVIF, JXL, RAF, MOV/MP4, …). |"
    )
    add("")
    add(
        "So: treat Exiv2 as the **narrow native C++ image/RAW backend**, and ExifTool as "
        "the **broad compatibility backend**. Prefer ExifTool when write access, format "
        "coverage, or **location write** matters (the [concept.md](concept.md) CR3 example; "
        "PNG EXIF GPS; video GPS)."
    )
    add("")
    add("Legend used below:")
    add("")
    add("- Exiv2: **Read/Write**, **Read**, or **-** (not applicable / not supported for that metadata category)")
    add("- ExifTool: **r** = read, **w** = write, **c** = create (new metadata file from scratch)")
    add(
        "- “Identify only” (Exiv2): format recognized, MIME type assigned, width/height "
        "determined — no Exif/IPTC/XMP — therefore **no location metadata**"
    )
    add(
        "- Location: **any-kind** means at least one of GPS coordinates, IPTC/XMP named "
        "place, XMP GPS, or container GPS can be read or written"
    )
    add("")
    add("---")
    add("")
    add("## 1. Exiv2 supported types")
    add("")
    add(
        "Official Exiv2 table (metadata categories per type). BMFF types (AVIF, CR3, HEIF, "
        "HEIC) are a **build option** (`enable_bmff=1`). Naked JPEG XL *codestreams* do not "
        "contain Exif/IPTC/XMP. Other unlisted TIFF-like RAW files may still read. RAF extra "
        "internal metadata is only partially supported."
    )
    add("")
    add("| Type | Exif | IPTC | XMP | Comments | ICC | Thumbnail |")
    add("|---|---|---|---|---|---|---|")
    for row in listed:
        cat = row["categories"]
        add(
            "| {type} | {exif} | {iptc} | {xmp} | {comments} | {icc} | {thumbnail} |".format(
                type=row["type"],
                exif=exiv2_cell(cat["exif"]),
                iptc=exiv2_cell(cat["iptc"]),
                xmp=exiv2_cell(cat["xmp"]),
                comments=exiv2_cell(cat["comments"]),
                icc=exiv2_cell(cat["icc"]),
                thumbnail=exiv2_cell(cat["thumbnail"]),
            )
        )
    add("")
    add(
        "Identify-only (no metadata categories, **no location**): **"
        + "**, **".join(identify)
        + "**."
    )
    add("")
    add("### Exiv2 video (rudimentary read only)")
    add("")
    add(
        "Exiv2 documents limited **read** of QuickTime, Matroska, and RIFF-based files, "
        "for example:"
    )
    add("")
    add("| Type | Exiv2 | Notes |")
    add("|---|---|---|")
    for row in video:
        kind = row.get("video_kind", "")
        label = row["type"]
        if label == "MOV":
            label = "MOV / MP4"
            if any(other["type"] == "MP4" for other in video):
                add(f"| {label} | Read (rudimentary) | {kind} |")
                continue
        if label == "MP4":
            continue
        add(f"| {label} | Read (rudimentary) | {kind} |")
    add("")
    add(
        "There is **no Exiv2 write path** for these, including **no location write**. "
        "Rudimentary read is not a documented GPS / IPTC / XMP location API."
    )
    add("")
    add("---")
    add("")
    add("## 2. Same types in ExifTool — do not repeat the Exiv2 table")
    add("")
    add(
        "ExifTool covers the Exiv2 still-image/RAW types above (except **TGA**, which it "
        "does not list). The interesting part is **capability**, not the type name."
    )
    add("")
    add("| Type | Exiv2 overall | ExifTool | Do not miss | Location (any kind) |")
    add("|---|---|---|---|---|")
    for names, overall, et, miss, loc in SECTION2_GROUPS:
        for name in names:
            if name not in overlap:
                raise GeneratorError(f"section 2 type missing from ExifTool overlap: {name}")
        add(
            f"| {join_types(names)} | {overall} | {et} | {miss} | {loc} |"
        )
    add("")
    add(
        "ExifTool **create** (`c`) among types Exiv2 also has: **EXV**, **XMP**. "
        "(ExifTool also creates **EXIF**, **ICC**, **MIE**, **DR4**, **VRD** files; those "
        "are listed in section 4.)"
    )
    add("")
    add("---")
    add("")
    add("## 3. Location metadata (GPS and named place)")
    add("")
    add(
        "This is the section to use for `capabilities(media)` **GPS / location** and for "
        "`getLocation()` / `setLocation()`."
    )
    add("")
    add(
        "**Location is not one tag and not one backend flag.** A type can support *some* "
        "location and still fail a specific write. Always split:"
    )
    add("")
    add("| Kind | Typical tags / encodings | What it is |")
    add("|---|---|---|")
    add(
        "| **GPS coordinates** | EXIF GPS IFD (`GPSLatitude` / `GPSLongitude` / altitude / "
        "time); XMP `exif:GPS*`; QuickTime `GPSCoordinates` | Camera/device position |"
    )
    add(
        "| **Named place** | IPTC Core (City, Country, CountryCode, Province/State, "
        "Sublocation); IPTC Extension Location Created / Location Shown; XMP "
        "`photoshop:City`, `Iptc4xmpExt:LocationCreated` / `LocationShown` | "
        "Human-readable location from [IPTC Photo Metadata](https://www.iptc.org/std/photometadata/specification/IPTC-PhotoMetadata-2025.1.html) |"
    )
    add(
        "| **GeoTIFF** | ModelTiepoint / ModelPixelScale / etc. | Raster georeferencing on "
        "TIFF-family files — **not** photo GPS |"
    )
    add("")
    add("### How each backend treats location")
    add("")
    add("| Encoding | Exiv2 | ExifTool |")
    add("|---|---|---|")
    add(
        "| EXIF GPS IFD | Follows the **Exif** column. Documented write path: "
        "`Exif.GPSInfo.*` (example in exiv2.md). **PNG has no Exif** in the official table "
        "→ **no EXIF GPS**. Identify-only types → **none**. | Meta-information format "
        "**GPS r/w/c**. Writable where the container can hold EXIF (including **PNG Exif**, "
        "which Exiv2 does not list). |"
    )
    add(
        "| IPTC named place | Follows the **IPTC** column. **WebP, CRW, DCP, EPS, XMP sidecar** "
        "have no IPTC in Exiv2. | **IPTC r/w/c** on types that hold IPTC/IIM. |"
    )
    add(
        "| XMP GPS + named place | Follows the **XMP** column. This is Exiv2’s location path "
        "on **PNG** (no Exif) and **EPS** / **XMP sidecars** (XMP-only). | **XMP r/w/c**. "
        "Sidecar create (`c`) is ExifTool-only. |"
    )
    add(
        "| QuickTime `GPSCoordinates` | Video is **rudimentary read only** — **not** a "
        "documented location write path. | Written on **writable QuickTime-family files** "
        "(MOV/MP4 and siblings). Geotag writes `GPSCoordinates` into the preferred QuickTime "
        "group. The QuickTime *tag group* in the README is listed read-only; **file** "
        "metadata on MOV/MP4 is still writable. |"
    )
    add(
        "| GeoTIFF | Not a documented Exiv2 category. | **GeoTIFF r/w/c** (TIFF-family). |"
    )
    add(
        "| Track geotagging (GPX/NMEA/KML/…) | Not a backend feature. libumm’s GPS track "
        "engine sits **above** both backends and then writes through the table below. | "
        "ExifTool can interpolate a track and write GPS tags (including video "
        "`GPSCoordinates`). |"
    )
    add("")
    add("**Derive Exiv2 “any location” from §1:**")
    add("")
    add("- **Read** any location if Exif **or** IPTC **or** XMP is Read or Read/Write.")
    add("- **Write** any location if Exif **or** IPTC **or** XMP is Read/Write.")
    add("- **None** if all three are `-` (BMP, GIF, TGA).")
    add(
        "- **GPS write** specifically requires Exif Read/Write (so **not** PNG, EPS, XMP "
        "sidecar, or identify-only)."
    )
    add("- **Named-place write** requires IPTC or XMP Read/Write (so **not** CRW/DCP in Exiv2).")
    add("")
    add(
        "**Derive ExifTool “any location” from the file-type r/w/c flag**, then pick encodings "
        "the container actually holds (EXIF GPS, XMP, IPTC, and/or QuickTime GPS). Read-only "
        "types cannot write location."
    )
    add("")
    add("### Per-type location read / write (Exiv2 types)")
    add("")
    add(
        "Emphasized: **whether each backend can read or write any location**, and which "
        "encodings Exiv2 actually has."
    )
    add("")
    add("| Type | Exiv2 GPS (Exif) | Exiv2 named place | Exiv2 any location | ExifTool any location |")
    add("|---|---|---|---|---|")
    for names, gps, named, any_loc, et in SECTION3_GROUPS:
        add(f"| {join_types(names)} | {gps} | {named} | {any_loc} | {et} |")
    add("")
    add("### ExifTool-only types: location follows file r/w")
    add("")
    add("Do not assume every extra ExifTool type stores GPS. Use the §4 r/w/c flag, then the container:")
    add("")
    add("| Group | Location read | Location write | Notes |")
    add("|---|---|---|---|")
    add(
        "| Extra writable still/RAW (ARQ, ERF, GPR, IIQ, MEF, MOS, NRW, X3F, HDP/WDP, MPO, "
        "PS/PSB, FLIF, 360, INSP, …) | yes | **yes** (ExifTool) | Typical photo GPS/XMP path; "
        "**not in Exiv2** |"
    )
    add(
        "| Extra writable graphics (PBM/PGM/PPM, JNG/MNG, QTIF, …) | sometimes | **if marked w** | "
        "Encoding is container-specific (often comment/XMP, not EXIF GPS) |"
    )
    add(
        "| Extra **read-only** still/RAW (3FR, DCR, K25, KDC, SRF, BMP-class graphics, SVG, "
        "EXR, …) | often | **no** | Read location if present; cannot embed new GPS |"
    )
    add(
        "| QuickTime-family video/audio (**3GP, 3G2, M4A/V, F4A/V, LRV, MQV, GLV, DVB, AAX**, "
        "plus MOV/MP4) | yes | **yes** | ExifTool location write; Exiv2 has no write |"
    )
    add(
        "| Other video (**MKV, WEBM, AVI, WMV, MXF, MPG, …**) | often | **no** | Same story as "
        "MKV/AVI in §2 |"
    )
    add(
        "| Audio except AAX | often | **no** (AAX is the writable exception) | WAV is read-only "
        "in both backends |"
    )
    add(
        "| Documents | PDF/AI/IND: yes | **PDF, AI, IND yes**; Office Open XML **no** | PDF "
        "location is typically **XMP** |"
    )
    add(
        "| Sidecars / metadata-only | EXIF, XMP, MIE, ICC, … | **EXIF/XMP/MIE create** | "
        "Strongest portable location write when the media type cannot embed GPS |"
    )
    add(
        "| Fonts, archives, executables, scientific | sometimes | **no** | Out of scope for "
        "photo/video location |"
    )
    add("")
    add("**Bottom line for location:**")
    add("")
    add(
        "1. **Exiv2 writes location** on types with Exif, IPTC, or XMP **Read/Write** — "
        "strongest on JPEG/TIFF/DNG/common writable RAW, plus PGF, PNG (not EXIF GPS), "
        "WebP (not IPTC)."
    )
    add(
        "2. **ExifTool writes location** on types it marks **w** *when the container holds "
        "EXIF, XMP, IPTC, or QuickTime GPS* — including CR3/HEIC/AVIF/JXL/RAF/GIF/MOV/MP4 "
        "and PNG EXIF GPS. **w** is necessary but not always GPS."
    )
    add(
        "3. **Neither writes location** on MKV/WebM/AVI/WAV/ASF, most audio, and most office "
        "documents."
    )
    add(
        "4. **PGF** is the rare case where **Exiv2 can write location and ExifTool cannot**."
    )
    add("")
    add("---")
    add("")
    add("## 4. Additional ExifTool types")
    add("")
    add(
        "Everything below is **in ExifTool and not in the Exiv2 FILE TYPES table** (video "
        "types already compared above are omitted here except where ExifTool adds *siblings* "
        "such as 3GP/M4V)."
    )
    add("")
    add(
        "Grouped so the overlap with Exiv2 is not repeated. Values are ExifTool r / w / c. "
        "For **location**, treat **w** as “ExifTool may write some location encoding if the "
        "container allows it” and **r** as read-only; see §3 rather than repeating a location "
        "column here."
    )
    add("")
    overlap_names = {row["type"] for row in exiftool["overlap_types"]}
    for group in exiftool["extra_groups"]:
        add(f"### {group['title']}")
        add("")
        types = [row for row in group["types"] if row["type"] not in overlap_names]
        table_types = types
        if group["id"] == "still":
            table_types = [
                row for row in types if row["type"] not in {"HDP", "WDP", "THM"}
            ]
        add(triple_table(table_types))
        add("")
        if group["id"] == "still":
            add(
                "Related TIFF-family still image: **HDP r/w**, **WDP r/w**. **THM r/w** is "
                "JPEG thumbnail naming."
            )
            add("")
        if group["id"] == "video":
            add(
                "**M4A/V**, **F4A/V**, **3GP**, **3G2**, **LRV**, **MQV**, **GLV**, **DVB** "
                "are writable QuickTime-family relatives of MOV/MP4. **MKV / WEBM / MKA / MKS** "
                "remain **read-only** in ExifTool."
            )
            add("")
        if group["id"] == "audio":
            add(
                "Almost all audio is **read-only**. **AAX** is the writable exception in this "
                "list (QuickTime-family). WAV is read-only in both backends."
            )
            add("")
        if group["id"] == "documents":
            add(
                "Writable documents among these: **PDF**, **AI**, **IND**. Office Open XML "
                "(DOCX/XLSX/PPTX) is **read-only**."
            )
            add("")
        if group["id"] == "exotic":
            add(
                "This group is transcribed at file-level r/w only (`coverage: partial`); "
                "per-category probes join in Stage 6."
            )
            add("")
    add("---")
    add("")
    add("## 5. ExifTool meta-information formats")
    add("")
    add("Separate from file types. ExifTool README (r/w/c = read / write / create):")
    add("")
    create_list = ", ".join(
        f"**{name}**" if name in {"GPS", "GeoTIFF"} else name
        for name in exiftool["meta_formats"]["read_write_create"]
    )
    add(f"**Read/write/create:** {create_list}.")
    add("")
    add(
        "Those **GPS** and **GeoTIFF** rows are why ExifTool can advertise location as its "
        "own capability, not merely “the file type is r/w”. GPS here is the EXIF GPS IFD "
        "(ExifTool group `GPS`, family 2 `Location`). GeoTIFF is raster georeferencing, not "
        "`metadata.location` for a photograph."
    )
    add("")
    ro = ", ".join(exiftool["meta_formats"]["read_only_examples"])
    add(f"**Read-only (examples):** {ro}, and more.")
    add("")
    add(
        "QuickTime **file** metadata can be written for MOV/MP4-family files even though the "
        "QuickTime *tag group* is listed read-only in that second table — another reason "
        "libumm should not collapse “file type” and “metadata encoding” into one flag."
    )
    add("")
    add("---")
    add("")
    add("## 6. Implications for libumm")
    add("")
    add(
        "1. **JPEG, TIFF, DNG, and common writable RAW** can use Exiv2 or ExifTool; Exiv2 is "
        "the natural native default. **Location read/write** (EXIF GPS and IPTC/XMP named "
        "place) works in **both** backends on these types."
    )
    add(
        "2. **CR3, HEIC/HEIF, AVIF, JXL, RAF, RW2, MRW, SR2** should advertise **read via "
        "Exiv2 (when built with BMFF) / write via ExifTool** — including **`setLocation()`**."
    )
    add("3. **PGF write** (including location) is an Exiv2-only backend capability.")
    add(
        "4. **PNG Exif / EXIF GPS** is not interchangeable (ExifTool yes; Exiv2 table **no**). "
        "Exiv2 can still write PNG **named place and XMP GPS**. **WebP** has no IPTC "
        "named-place category in Exiv2 (use Exif/XMP). **CRW/DCP** in Exiv2 are **GPS only**."
    )
    add(
        "5. **Video location write** is ExifTool + QuickTime-family only (`GPSCoordinates` / "
        "XMP). MKV/WebM/AVI/WAV/ASF stay **read-only** — **no location write** in either backend."
    )
    add(
        "6. **Audio, office documents, PDF, fonts, archives** are ExifTool-only (PDF is "
        "writable, including XMP location). GIF location write is ExifTool-only. BMP location "
        "is ExifTool **read-only**. TGA has **no** location in Exiv2 and is not listed in ExifTool."
    )
    add(
        "7. Phase 1 in [concept.md](concept.md) (JPEG, TIFF, PNG, WebP, common RAW, XMP "
        "sidecars) is inside Exiv2’s strong **location** set, except **PNG EXIF GPS** if that "
        "encoding is required (use ExifTool)."
    )
    add(
        "8. Phase 3 video (MP4, MOV, M4V, MKV, WebM, MXF) is **ExifTool-primary** for "
        "location; Exiv2 at most supplements read. Phase 5 GPS tracks write through this same "
        "location table — they do not create a new file-type capability."
    )
    add(
        "9. `capabilities(media)` must report **GPS** and **named place** separately per "
        "backend. “Type supported” is not “location writable”."
    )
    return "\n".join(lines) + "\n"


def capability_rows(
    exiv2_types: list[dict[str, Any]],
    exiftool: dict[str, Any],
) -> list[tuple[str, dict[str, Any]]]:
    rows: list[tuple[str, dict[str, Any]]] = []
    for raw in exiv2_types:
        rows.append(("exiv2", raw))
    seen_et: set[str] = set()
    for raw in exiftool["overlap_types"]:
        rows.append(("exiftool", raw))
        seen_et.add(raw["type"])
    for group in exiftool["extra_groups"]:
        for raw in group["types"]:
            if raw["type"] in seen_et:
                continue
            seen_et.add(raw["type"])
            rows.append(("exiftool", raw))
    return rows


def emit_hpp(
    exiv2_types: list[dict[str, Any]],
    exiftool: dict[str, Any],
    policy: dict[str, Any],
) -> str:
    lines = [
        "// GENERATED — do not edit",
        "//",
        "// Generator: tools/registry/generate_supported_types.py",
        "// Sources: registry/capabilities/exiv2.json, exiftool.json, policy.json",
        "",
        "#pragma once",
        "",
        '#include "umm/capabilities.hpp"',
        "",
        "#include <cstddef>",
        "#include <string_view>",
        "",
        "namespace umm::internal {",
        "",
        "struct CapabilityRecord {",
        "  std::string_view file_type;",
        "  std::string_view backend;",
        "  bool listed;",
        "  bool identify_only;",
        "  bool bmff;",
        "  bool video;",
        "  Access exif;",
        "  Access iptc_iim;",
        "  Access xmp;",
        "  Access icc;",
        "  Access thumbnail;",
        "  Access gps_exif;",
        "  Access named_place;",
        "  Access xmp_location;",
        "  Access container_gps;",
        "  Access geotiff;",
        "  std::string_view notes;",
        "};",
        "",
        "struct TypePolicyRecord {",
        "  std::string_view file_type;",
        "  std::string_view preferred_backend;",
        "  bool sidecar_recommended;",
        "};",
        "",
        "struct TypeExtensionRecord {",
        "  std::string_view extension;",
        "  std::string_view file_type;",
        "};",
        "",
        "inline constexpr CapabilityRecord kCapabilityRecords[] = {",
    ]
    for backend, raw in capability_rows(exiv2_types, exiftool):
        cats = raw.get("categories") or empty_categories()
        loc = raw["location"]
        listed = "true" if raw.get("listed", True) else "false"
        identify = "true" if raw.get("identify_only") else "false"
        bmff = "true" if raw.get("bmff") else "false"
        video = "true" if raw.get("video") else "false"
        notes = raw.get("notes") or ""
        lines.append("    {")
        lines.append(f"        {cpp_string(raw['type'])},")
        lines.append(f"        {cpp_string(backend)},")
        lines.append(f"        {listed},")
        lines.append(f"        {identify},")
        lines.append(f"        {bmff},")
        lines.append(f"        {video},")
        lines.append(f"        {ACCESS_CPP[cats['exif']]},")
        lines.append(f"        {ACCESS_CPP[cats['iptc']]},")
        lines.append(f"        {ACCESS_CPP[cats['xmp']]},")
        lines.append(f"        {ACCESS_CPP[cats['icc']]},")
        lines.append(f"        {ACCESS_CPP[cats['thumbnail']]},")
        lines.append(f"        {ACCESS_CPP[loc['gps_exif']]},")
        lines.append(f"        {ACCESS_CPP[loc['named_place']]},")
        lines.append(f"        {ACCESS_CPP[loc['xmp_location']]},")
        lines.append(f"        {ACCESS_CPP[loc['container_gps']]},")
        lines.append(f"        {ACCESS_CPP[loc['geotiff']]},")
        lines.append(f"        {cpp_string(notes)},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("inline constexpr TypePolicyRecord kTypePolicies[] = {")
    for raw in policy["types"]:
        sidecar = "true" if raw["sidecar_recommended"] else "false"
        lines.append("    {")
        lines.append(f"        {cpp_string(raw['type'])},")
        lines.append(f"        {cpp_string(raw['preferred_backend'])},")
        lines.append(f"        {sidecar},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("inline constexpr TypeExtensionRecord kTypeExtensions[] = {")
    seen_ext: set[str] = set()
    for raw in policy["types"]:
        for ext in raw["extensions"]:
            key = ext.lower()
            if key in seen_ext:
                raise GeneratorError(f"duplicate extension {ext}")
            seen_ext.add(key)
            lines.append(
                f"    {{{cpp_string(key)}, {cpp_string(raw['type'])}}},"
            )
    lines.append("};")
    lines.append("")
    n_cap = len(capability_rows(exiv2_types, exiftool))
    n_pol = len(policy["types"])
    n_ext = len(seen_ext)
    lines.append(f"inline constexpr std::size_t kCapabilityRecordCount = {n_cap};")
    lines.append(f"inline constexpr std::size_t kTypePolicyCount = {n_pol};")
    lines.append(f"inline constexpr std::size_t kTypeExtensionCount = {n_ext};")
    lines.append("")
    lines.append("}  // namespace umm::internal")
    lines.append("")
    return "\n".join(lines)


def generate(cap_dir: Path, markdown_path: Path, hpp_path: Path) -> None:
    exiv2_types = load_exiv2(cap_dir / "exiv2.json")
    exiftool = load_exiftool(cap_dir / "exiftool.json")
    policy = load_json(cap_dir / "policy.json")
    markdown_path.write_text(markdown_for(exiv2_types, exiftool), encoding="utf-8", newline="\n")
    hpp_path.parent.mkdir(parents=True, exist_ok=True)
    hpp_path.write_text(emit_hpp(exiv2_types, exiftool, policy), encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capabilities-dir", type=Path, default=DEFAULT_CAP_DIR)
    parser.add_argument("--markdown", type=Path, default=DEFAULT_MARKDOWN)
    parser.add_argument("--output-hpp", type=Path, default=DEFAULT_HPP)
    args = parser.parse_args()
    try:
        generate(args.capabilities_dir, args.markdown, args.output_hpp)
    except GeneratorError as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
