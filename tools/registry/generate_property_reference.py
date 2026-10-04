#!/usr/bin/env python3
"""Generate docs/user/properties/ from the same data as umm::describe (C14a)."""

from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import generate_cpp as codegen  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT_DIR = REPO_ROOT / "docs" / "user" / "properties"

EXIFTOOL_TAGNAMES = "https://exiftool.org/TagNames/"
EXIFTOOL_EXIF = "https://exiftool.org/TagNames/EXIF.html"
EXIFTOOL_QUICKTIME = "https://exiftool.org/TagNames/QuickTime.html"

NAMESPACE_NOTES = {
    "http://iptc.org/std/Iptc4xmpExt/2008-02-29/": (
        "`Iptc4xmpExt` is the IPTC Photo Metadata Extension XMP namespace "
        "(`http://iptc.org/std/Iptc4xmpExt/2008-02-29/`), not a different standard."
    ),
    "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/": (
        "`Iptc4xmpCore` is the IPTC Photo Metadata Core XMP namespace "
        "(`http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/`)."
    ),
}

# Plain-language notes for well-known representation tokens (C14a). Not IPTC text.
REPRESENTATION_NOTES: tuple[tuple[str, str], ...] = (
    (
        "DateTimeOriginal",
        "`DateTimeOriginal`: when the photo was taken; not `DateTime`, the modify date.",
    ),
    (
        "SubSecTimeOriginal",
        "Companion sub-second and offset tags travel with `DateTimeOriginal`.",
    ),
    (
        "photoshop:DateCreated",
        "XMP Date Created (`photoshop:DateCreated`); capture time, not file modify.",
    ),
    (
        "com.apple.quicktime.creationdate",
        "QuickTime Keys CreationDate (local time with offset) is the automatic "
        "video `dateCreated` representation. Movie-header `CreateDate` is a cast, "
        "not a representation.",
    ),
    (
        "GPS:GPSLatitude",
        "EXIF GPS IFD is a representation of photo `locationCreated[0]` GPS "
        "(EXIF > XMP-exif > struct).",
    ),
    (
        "GPS:GPSLongitude",
        "EXIF GPS IFD is a representation of photo `locationCreated[0]` GPS "
        "(EXIF > XMP-exif > struct).",
    ),
    (
        "GPS:GPSAltitudeRef",
        "`gpsAltitudeRef` is 0 (above WGS 84) or 1 (below).",
    ),
    (
        "GPS:GPSAltitude",
        "EXIF GPS altitude is a representation of photo `locationCreated[0]` GPS.",
    ),
    (
        "exif:GPSLatitude",
        "Top-level XMP-exif GPS; photo `locationCreated[0]` GPS representation "
        "ranked after the EXIF IFD.",
    ),
    (
        "exif:GPSLongitude",
        "Top-level XMP-exif GPS; photo `locationCreated[0]` GPS representation "
        "ranked after the EXIF IFD.",
    ),
    (
        "exif:GPSAltitudeRef",
        "Top-level XMP-exif altitude reference; ranked after the EXIF IFD.",
    ),
    (
        "exif:GPSAltitude",
        "Top-level XMP-exif GPS altitude; photo `locationCreated[0]` GPS "
        "representation ranked after the EXIF IFD.",
    ),
)


def token_in_key(token: str, key: str) -> bool:
    start = 0
    while True:
        found = key.find(token, start)
        if found < 0:
            return False
        end = found + len(token)
        after = key[end : end + 1]
        if after in ("", "+", ":", ".", ",", " "):
            return True
        start = found + 1

CURATED_CAMERA_KEYS: tuple[tuple[str, str], ...] = (
    (
        "Exif.Image.Make",
        "Camera manufacturer. Intentionally non-canonical (C17); no EXIF property domain.",
    ),
    (
        "Exif.Image.Model",
        "Camera model. Intentionally non-canonical (C17).",
    ),
    (
        "Exif.Photo.ExposureTime",
        "Shutter speed. Camera exposure; not a canonical IPTC property.",
    ),
    (
        "Exif.Photo.FNumber",
        "Aperture. Camera exposure; not a canonical IPTC property.",
    ),
    (
        "Exif.Photo.ISOSpeedRatings",
        "ISO sensitivity. Camera exposure; not a canonical IPTC property.",
    ),
    (
        "Exif.Photo.PhotographicSensitivity",
        "ISO sensitivity (Exif 2.3). Camera exposure; not a canonical IPTC property.",
    ),
    (
        "Exif.Photo.FocalLength",
        "Lens focal length. Intentionally non-canonical.",
    ),
    (
        "Exif.Photo.LensMake",
        "Lens manufacturer. Intentionally non-canonical.",
    ),
    (
        "Exif.Photo.LensModel",
        "Lens model. Intentionally non-canonical.",
    ),
)

HEURISTIC_LABELS = {
    "H1": "List → single: first entry",
    "H2": "Single → empty list: create `[0]`",
    "H3": "Single → non-empty list: merge into `[0]`; never append",
    "H4": "List ↔ list: union by display value",
    "H5": "Joined string ↔ list",
    "H6": "Hierarchical ↔ flat (later)",
    "H7": "lang-alt ↔ string via `x-default`",
    "H8": "Struct ↔ scalar: field-level merge",
    "H9": "Fan-out: parse one encoding into several fields",
    "H10": "Fan-in: combine per rule (GPS refs)",
    "H11": "Units / encodings; 1e-5° / 0.5 m",
    "H12": "Dates / offsets: never invent an offset",
    "H13": "Split ↔ combined date + time",
    "H14": "Length limits: truncate only with `force`",
    "H15": "Code ↔ CV term (not in first set)",
    "H16": "Semantic near-miss: `approximate`",
    "H17": "Sources disagree: first non-empty in group priority",
    "H18": "Role filter: Keys location when role is 0 or absent",
    "H19": "Paired parallel lists (not implemented)",
    "H20": "Lost fields stay unmapped",
    "H21": "Capture-time sanity (deferred)",
}

FAMILY_RANK = {
    "xmp": 0,
    "struct_field": 0,
    "iim": 1,
    "exif": 2,
    "quicktime": 3,
    "ebucore": 4,
}


class GenerateError(RuntimeError):
    """Property-reference generator failed closed."""


def md_cell(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", " ").replace("\r", "")


def local_name(property_id: str) -> str:
    return property_id.rsplit(".", 1)[-1]


def domain_page(property_id: str) -> str:
    if property_id.startswith("iptc.photo."):
        return "photo.md"
    if property_id.startswith("iptc.video."):
        return "video.md"
    raise GenerateError(f"unrecognized domain for {property_id!r}")


def property_link(property_id: str, *, label: str | None = None) -> str:
    text = label if label is not None else f"`{property_id}`"
    return f"[{text}]({domain_page(property_id)}#{property_id})"


def representation_field(record: dict[str, Any], *path: str) -> str:
    value: Any = record.get("representations")
    for key in path:
        if not isinstance(value, dict):
            return ""
        value = value.get(key)
    if value is None:
        return ""
    if not isinstance(value, str):
        return ""
    return value


def citation_for(record: dict[str, Any]) -> str:
    standard = record.get("standard") or ""
    version = record.get("standard_version") or ""
    if standard and version:
        return f"{standard} {version}"
    return standard


def default_rank(family: str) -> int:
    return FAMILY_RANK.get(family, 5)


def representation_note(family: str, key: str, property_id: str) -> str:
    notes: list[str] = []
    seen: set[str] = set()
    for token, note in REPRESENTATION_NOTES:
        if token_in_key(token, key) and note not in seen:
            if token.startswith("GPS:") and "locationCreated" not in property_id:
                continue
            if token.startswith("exif:GPS") and "locationCreated" not in property_id:
                continue
            notes.append(note)
            seen.add(note)
    if family == "xmp":
        pass
    return " ".join(notes)


def namespace_notes_for(records: list[dict[str, Any]]) -> list[str]:
    found: list[str] = []
    seen: set[str] = set()
    for record in records:
        ns = representation_field(record, "xmp", "namespace")
        note = NAMESPACE_NOTES.get(ns)
        if note and ns not in seen:
            seen.add(ns)
            found.append(note)
        xmp_prop = representation_field(record, "xmp", "property")
        if xmp_prop.startswith("Iptc4xmpExt:") and "ext" not in seen:
            note = NAMESPACE_NOTES["http://iptc.org/std/Iptc4xmpExt/2008-02-29/"]
            seen.add("ext")
            if note not in found:
                found.append(note)
        if xmp_prop.startswith("Iptc4xmpCore:") and "core" not in seen:
            note = NAMESPACE_NOTES["http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/"]
            seen.add("core")
            if note not in found:
                found.append(note)
    return found


def type_label(record: dict[str, Any]) -> str:
    datatype = record["datatype"]
    cardinality = record["cardinality"]
    struct_type = record.get("struct_type")
    parts = [datatype]
    if struct_type:
        parts.append(f"struct `{struct_type}`")
    if cardinality == "many":
        parts.append("multi")
    return ", ".join(parts)


def endpoint_label(kind: str, key: str, field: str, index: int) -> str:
    if kind == "base_key":
        return key
    suffix = ""
    if kind == "property_field":
        if index >= 0:
            suffix += f"[{index}]"
        if field:
            suffix += f".{field}"
    elif field:
        suffix += f".{field}"
    return f"{key}{suffix}"


def partner_of(rule: dict[str, Any], property_id: str) -> str:
    source_hit = rule["source_key"] == property_id
    target_hit = rule["target_key"] == property_id
    if source_hit and not target_hit:
        return endpoint_label(
            rule["target_kind"],
            rule["target_key"],
            rule["target_field"],
            rule["target_index"],
        )
    if target_hit and not source_hit:
        return endpoint_label(
            rule["source_kind"],
            rule["source_key"],
            rule["source_field"],
            rule["source_index"],
        )
    if source_hit and target_hit:
        if rule["target_field"]:
            return rule["target_field"]
        if rule["source_field"]:
            return rule["source_field"]
    return ""


def link_endpoint(label: str, properties_by_id: dict[str, Any]) -> str:
    if not label.startswith("iptc."):
        return f"`{label}`"
    candidate = label.split("[", 1)[0]
    parts = candidate.split(".")
    if len(parts) >= 3:
        property_id = ".".join(parts[:3])
        if property_id in properties_by_id:
            rest = label[len(property_id) :]
            linked = property_link(property_id)
            if rest:
                return f"{linked}`{rest}`"
            return linked
    return f"`{label}`"


def push_representation(
    out: list[dict[str, Any]],
    family: str,
    key: str,
    path: str,
    citation: str,
    rank: int,
    property_id: str,
) -> None:
    if not key:
        return
    out.append(
        {
            "family": family,
            "key": key,
            "path": path,
            "citation": citation,
            "read_rank": rank,
            "note": representation_note(family, key, property_id),
        }
    )


def representations_for(
    record: dict[str, Any],
    overlay_exif: dict[str, str],
    struct_fields: list[dict[str, str]],
    property_structs: dict[str, str],
) -> list[dict[str, Any]]:
    property_id = record["id"]
    cite = citation_for(record)
    out: list[dict[str, Any]] = []
    xmp_prop = representation_field(record, "xmp", "property")
    xmp_ns = representation_field(record, "xmp", "namespace")
    push_representation(out, "xmp", xmp_prop, xmp_ns, cite, default_rank("xmp"), property_id)
    iim = representation_field(record, "iptc_iim", "dataset")
    iim_name = representation_field(record, "iptc_iim", "name")
    push_representation(out, "iim", iim, iim_name, cite, default_rank("iim"), property_id)
    exif_tag = representation_field(record, "exif", "tag") or overlay_exif.get(property_id, "")
    push_representation(out, "exif", exif_tag, "", cite, default_rank("exif"), property_id)
    qt = representation_field(record, "quicktime", "key")
    push_representation(out, "quicktime", qt, "", cite, default_rank("quicktime"), property_id)
    ebu = representation_field(record, "ebucore", "path")
    push_representation(out, "ebucore", ebu, "", cite, default_rank("ebucore"), property_id)

    struct_name = property_structs.get(property_id)
    if not struct_name:
        return out
    prefix = ""
    if property_id.startswith("iptc.photo."):
        prefix = "iptc.photo.struct."
    elif property_id.startswith("iptc.video."):
        prefix = "iptc.video.struct."
    local = local_name(property_id)
    for field in struct_fields:
        if field["struct_name"] != struct_name:
            continue
        if prefix and not field["id"].startswith(prefix):
            continue
        if field["struct_property"] and field["struct_property"] != local:
            continue
        gps_overlay = bool(field["exif_tag"] and field["struct_property"])
        if field["xmp_property"]:
            xmp_exif = field["xmp_property"].startswith("exif:")
            rank = (1 if xmp_exif else 2) if gps_overlay else default_rank("struct_field")
            push_representation(
                out,
                "struct_field",
                field["xmp_property"],
                field["id"],
                cite,
                rank,
                property_id,
            )
        if field["exif_tag"]:
            rank = 0 if gps_overlay else default_rank("exif")
            push_representation(
                out, "exif", field["exif_tag"], field["id"], cite, rank, property_id
            )
    return out


def struct_fields_for(
    property_id: str,
    struct_fields: list[dict[str, str]],
    property_structs: dict[str, str],
    structs_by_name: dict[tuple[str, str], dict[str, Any]],
) -> list[dict[str, Any]]:
    struct_name = property_structs.get(property_id)
    if not struct_name:
        return []
    prefix = ""
    domain = ""
    if property_id.startswith("iptc.photo."):
        prefix = "iptc.photo.struct."
        domain = "photo"
    elif property_id.startswith("iptc.video."):
        prefix = "iptc.video.struct."
        domain = "video"
    local = local_name(property_id)
    struct_def = structs_by_name.get((domain, struct_name), {})
    fields_by_id = {
        field["id"]: field for field in struct_def.get("fields", []) if isinstance(field, dict)
    }
    rows: list[dict[str, Any]] = []
    for field in struct_fields:
        if field["struct_name"] != struct_name:
            continue
        if prefix and not field["id"].startswith(prefix):
            continue
        if field["struct_property"] and field["struct_property"] != local:
            continue
        src = fields_by_id.get(field["id"], {})
        rows.append(
            {
                "id": field["id"],
                "name": src.get("standard_property_name") or local_name(field["id"]),
                "definition": src.get("definition") or "",
                "xmp_property": field["xmp_property"],
                "et_tag": field["et_tag"],
                "exif_tag": field["exif_tag"],
            }
        )
    return rows


def casts_for(
    property_id: str,
    rules: list[dict[str, Any]],
    groups: dict[tuple[str, str], dict[str, Any]],
) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for rule in rules:
        if rule["source_key"] != property_id and rule["target_key"] != property_id:
            continue
        group = groups.get((rule["group"], rule["direction"]), {})
        flags: list[str] = []
        if group.get("approximate"):
            flags.append("approximate")
        if group.get("partial"):
            flags.append("partial")
        if group.get("one_way"):
            flags.append("one-way")
        rows.append(
            {
                "group": rule["group"],
                "direction": rule["direction"],
                "partner": partner_of(rule, property_id),
                "heuristic": rule["heuristic"],
                "citation": rule["citation"],
                "flags": ", ".join(flags),
                "heuristic_label": HEURISTIC_LABELS.get(rule["heuristic"], ""),
            }
        )
    return rows


def accessors_for_id(
    property_id: str, accessors: list[dict[str, Any]]
) -> list[dict[str, Any]]:
    hits: list[dict[str, Any]] = []
    for row in accessors:
        if property_id in row["photo_ids"] or property_id in row["video_ids"]:
            hits.append(row)
    return hits


def other_ids(row: dict[str, Any], property_id: str) -> list[str]:
    if property_id in row["photo_ids"]:
        return list(row["video_ids"])
    if property_id in row["video_ids"]:
        return list(row["photo_ids"])
    return []


def generated_banner() -> list[str]:
    return [
        "> **GENERATED** from the IPTC registries, `registry/mappings/`, and "
        "`registry/casts/` by `tools/registry/generate_property_reference.py`.",
        "> Do not edit by hand. Decision **C14a**: this is the `umm::describe` / "
        "`PropertyMap` data rendered as text. Definitions are copied from the "
        "imported Technical References.",
        "",
    ]


def load_model(
    registry_dirs: list[Path],
    overlay_path: Path,
    cross_media_path: Path,
    casts_dir: Path,
) -> dict[str, Any]:
    files = codegen.registry_files(registry_dirs)
    overlay = codegen.load_overlay(overlay_path)
    registries: list[dict[str, Any]] = []
    properties: list[dict[str, Any]] = []
    known_ids: dict[str, str] = {}
    seen_ids: set[str] = set()
    structs_by_name: dict[tuple[str, str], dict[str, Any]] = {}

    for path in files:
        registry = codegen.load_json(path)
        if "properties" not in registry or "structs" not in registry:
            raise GenerateError(f"{path}: missing properties/structs")
        registries.append(registry)
        known_ids.update(codegen.collect_known_ids(registry))
        domain = "photo" if "iptc-photo" in path.as_posix() else "video"
        if registry["standard"].startswith("IPTC Photo"):
            domain = "photo"
        elif "Video" in registry["standard"]:
            domain = "video"
        for struct in registry.get("structs", []):
            structs_by_name[(domain, struct["name"])] = struct
        for record in registry["properties"]:
            property_id = record["id"]
            if property_id in seen_ids:
                raise GenerateError(f"duplicate property id {property_id!r}")
            seen_ids.add(property_id)
            properties.append(record)

    overlay_exif, field_overlay = codegen.apply_overlay(
        properties, known_ids, overlay, overlay_path
    )
    properties_by_id = {record["id"]: record for record in properties}
    cross_media = codegen.load_cross_media(cross_media_path, properties_by_id)
    properties.sort(key=lambda item: item["id"])
    struct_fields = codegen.collect_struct_field_rows(registries, field_overlay)
    property_structs = {
        row["id"]: row["struct_name"]
        for row in codegen.collect_property_structs(registries)
    }
    groups_list, rules, _cast_files = codegen.load_casts(casts_dir)
    groups = {(row["id"], row["direction"]): row for row in groups_list}
    return {
        "registries": registries,
        "properties": properties,
        "properties_by_id": properties_by_id,
        "overlay_exif": overlay_exif,
        "struct_fields": struct_fields,
        "property_structs": property_structs,
        "structs_by_name": structs_by_name,
        "accessors": cross_media["accessors"],
        "groups": groups,
        "rules": rules,
    }


def render_property_section(record: dict[str, Any], model: dict[str, Any]) -> list[str]:
    property_id = record["id"]
    lines = [
        f'<a id="{property_id}"></a>',
        "",
        f"## `{property_id}`",
        "",
        f"**{record['standard_property_name']}** — {type_label(record)}",
        "",
        record["definition"].strip() or "_No definition in the imported Technical Reference._",
        "",
        f"- Schema: {record.get('schema') or '—'}",
        f"- Datatype: `{record['datatype']}`",
        f"- Cardinality: `{record['cardinality']}`",
        f"- Standard: {citation_for(record)}",
    ]
    if record.get("struct_type"):
        lines.append(f"- Struct: `{record['struct_type']}`")
    notes = (record.get("mapping_notes") or "").strip()
    if notes:
        lines.append(f"- Mapping notes: {notes}")
    lines.append("")

    ns_notes = namespace_notes_for([record])
    fields = struct_fields_for(
        property_id,
        model["struct_fields"],
        model["property_structs"],
        model["structs_by_name"],
    )
    if fields:
        ns_notes.extend(namespace_notes_for(fields_as_records(fields)))
    # Dedupe namespace notes while preserving order.
    deduped_ns: list[str] = []
    seen_ns: set[str] = set()
    for note in ns_notes:
        if note not in seen_ns:
            seen_ns.add(note)
            deduped_ns.append(note)
    for note in deduped_ns:
        lines.append(note)
        lines.append("")

    reps = representations_for(
        record,
        model["overlay_exif"],
        model["struct_fields"],
        model["property_structs"],
    )
    lines.append("### Representations")
    lines.append("")
    if not reps:
        lines.append("_No XMP, IIM, EXIF, QuickTime, or EBUCore representation in the registry._")
        lines.append("")
    else:
        lines.append("| Family | Key | Path | Read rank | Citation | Note |")
        lines.append("|---|---|---|---|---|---|")
        for row in reps:
            lines.append(
                "| "
                + " | ".join(
                    [
                        md_cell(row["family"]),
                        md_cell(f"`{row['key']}`"),
                        md_cell(f"`{row['path']}`" if row["path"] else ""),
                        md_cell(str(row["read_rank"])),
                        md_cell(row["citation"]),
                        md_cell(row["note"]),
                    ]
                )
                + " |"
            )
        lines.append("")

    if fields:
        lines.append("### Struct fields")
        lines.append("")
        lines.append("| Field | Name | XMP | ExifTool | EXIF |")
        lines.append("|---|---|---|---|---|")
        for field in fields:
            lines.append(
                "| "
                + " | ".join(
                    [
                        md_cell(f"`{local_name(field['id'])}`"),
                        md_cell(field["name"]),
                        md_cell(f"`{field['xmp_property']}`" if field["xmp_property"] else ""),
                        md_cell(f"`{field['et_tag']}`" if field["et_tag"] else ""),
                        md_cell(f"`{field['exif_tag']}`" if field["exif_tag"] else ""),
                    ]
                )
                + " |"
            )
        lines.append("")
        gps_fields = [field for field in fields if local_name(field["id"]).startswith("gps")]
        if gps_fields and "locationCreated" in property_id:
            lines.append(
                "Location GPS fields are numbers (decimal degrees, WGS 84; west/south "
                "negative). On photos, camera EXIF GPS and top-level XMP-exif GPS are "
                "representations of `locationCreated[0]` GPS."
            )
            lines.append("")
        if gps_fields and "locationShot" in property_id:
            lines.append(
                "Video Location Shot GPS lives on the IPTC Location struct. QuickTime "
                "GPS is a `capturePosition` cast, not a `locationShot` representation."
            )
            lines.append("")

    cast_rows = casts_for(property_id, model["rules"], model["groups"])
    lines.append("### Cast rules")
    lines.append("")
    if not cast_rows:
        lines.append("_No cast rules touch this property._")
        lines.append("")
    else:
        lines.append("| Group | Direction | Partner | Heuristic | Flags | Citation |")
        lines.append("|---|---|---|---|---|---|")
        for row in cast_rows:
            heuristic = row["heuristic"]
            if row["heuristic_label"]:
                heuristic = f"{row['heuristic']} — {row['heuristic_label']}"
            lines.append(
                "| "
                + " | ".join(
                    [
                        md_cell(f"`{row['group']}`"),
                        md_cell(row["direction"]),
                        md_cell(f"`{row['partner']}`" if row["partner"] else ""),
                        md_cell(heuristic),
                        md_cell(row["flags"]),
                        md_cell(row["citation"]),
                    ]
                )
                + " |"
            )
        lines.append("")
        lines.append(
            "Casts are opt-in (`umm::cast`). See "
            "[canonical model heuristics](../../developer/canonical-model.md)."
        )
        lines.append("")

    accs = accessors_for_id(property_id, model["accessors"])
    lines.append("### Accessor")
    lines.append("")
    if not accs:
        lines.append(
            "No cross-media accessor. Use `Metadata::get` / `set` with this registry id."
        )
        lines.append("")
    else:
        for acc in accs:
            deferred = " (deferred)" if acc["deferred"] else ""
            others = other_ids(acc, property_id)
            other_links = ", ".join(property_link(item) for item in others) or "—"
            extra = acc["notes"].strip()
            lines.append(
                f"- `Metadata::{acc['concept']}()` — Tier {acc['tier']} "
                f"`{acc['transposition']}`{deferred}. Other domain: {other_links}."
            )
            if extra:
                lines.append(f"  {extra}")
        lines.append("")
    return lines


def fields_as_records(fields: list[dict[str, Any]]) -> list[dict[str, Any]]:
    records = []
    for field in fields:
        records.append(
            {
                "representations": {
                    "xmp": {
                        "property": field.get("xmp_property") or "",
                        "namespace": "",
                    }
                }
            }
        )
    return records


def render_domain_page(domain: str, model: dict[str, Any]) -> str:
    prefix = f"iptc.{domain}."
    records = [row for row in model["properties"] if row["id"].startswith(prefix)]
    if domain == "photo":
        title = "Photo properties — IPTC Photo Metadata 2025.1"
        intro = (
            "One section per canonical photo registry id. Definitions are copied "
            "from the IPTC Photo Metadata Technical Reference 2025.1."
        )
    else:
        title = "Video properties — IPTC Video Metadata Hub 1.7"
        intro = (
            "One section per canonical video registry id. Definitions are copied "
            "from the IPTC Video Metadata Hub 1.7 recommendation."
        )
    lines = [
        f"# {title}",
        "",
        *generated_banner(),
        intro,
        "",
        f"{len(records)} properties. Index: [property reference](README.md).",
        "",
    ]
    by_schema: dict[str, list[dict[str, Any]]] = defaultdict(list)
    schema_order: list[str] = []
    for record in records:
        schema = record.get("schema") or "Other"
        if schema not in schema_order:
            schema_order.append(schema)
        by_schema[schema].append(record)
    for schema in schema_order:
        lines.append(f"## {schema}")
        lines.append("")
        for record in by_schema[schema]:
            lines.append(f"- {property_link(record['id'], label='`' + record['id'] + '`')} — {record['standard_property_name']}")
        lines.append("")
    for schema in schema_order:
        for record in by_schema[schema]:
            lines.extend(render_property_section(record, model))
    return "\n".join(lines).rstrip() + "\n"


def mapped_ids(accessors: list[dict[str, Any]]) -> set[str]:
    ids: set[str] = set()
    for row in accessors:
        ids.update(row["photo_ids"])
        ids.update(row["video_ids"])
    return ids


def render_index(model: dict[str, Any]) -> str:
    accessors = model["accessors"]
    mapped = mapped_ids(accessors)
    remaining_photo = [
        row
        for row in model["properties"]
        if row["id"].startswith("iptc.photo.") and row["id"] not in mapped
    ]
    remaining_video = [
        row
        for row in model["properties"]
        if row["id"].startswith("iptc.video.") and row["id"] not in mapped
    ]
    lines = [
        "# Property reference",
        "",
        *generated_banner(),
        "Canonical properties are the IPTC Photo Metadata 2025.1 and IPTC Video "
        "Metadata Hub 1.7 registry ids. EXIF, IPTC IIM, and QuickTime tags are "
        "**representations** of those properties. Other links are opt-in **casts**.",
        "",
        "Same data as `umm::describe` (C12b / C14a). Cross-media names first, then "
        "the remaining canonical ids, then non-canonical keys that take part in casts.",
        "",
        "- [Photo properties](photo.md)",
        "- [Video properties](video.md)",
        "- [Base keys](base-keys.md) (cast sources and frequent camera keys)",
        "",
        "## Cross-media properties",
        "",
        "Short names spanning a photo id and a video id "
        "(`registry/mappings/cross-media-accessors.json`). Naming only; they never "
        "decide what is read.",
        "",
    ]
    for tier in (1, 2, 3):
        rows = [row for row in accessors if row["tier"] == tier]
        if not rows:
            continue
        heading = {
            1: "Tier 1 — identical shape",
            2: "Tier 2 — transposing",
            3: "Tier 3 — renamed concepts",
        }[tier]
        lines.append(f"### {heading}")
        lines.append("")
        lines.append("| Accessor | Photo | Video | Transpose | Notes |")
        lines.append("|---|---|---|---|---|")
        for row in rows:
            photo = ", ".join(property_link(item) for item in row["photo_ids"])
            video = ", ".join(property_link(item) for item in row["video_ids"])
            notes = row["notes"].strip()
            if row["deferred"]:
                notes = (notes + " " if notes else "") + "Deferred."
            lines.append(
                "| "
                + " | ".join(
                    [
                        md_cell(f"`{row['concept']}`"),
                        md_cell(photo),
                        md_cell(video),
                        md_cell(f"`{row['transposition']}`"),
                        md_cell(notes),
                    ]
                )
                + " |"
            )
        lines.append("")

    lines.append("## Remaining photo properties")
    lines.append("")
    lines.append(
        "Canonical photo ids that are not a cross-media accessor endpoint. "
        "Use `get` / `set` with the registry id."
    )
    lines.append("")
    lines.append("| Property id | Name | Type |")
    lines.append("|---|---|---|")
    for record in remaining_photo:
        lines.append(
            "| "
            + " | ".join(
                [
                    md_cell(property_link(record["id"])),
                    md_cell(record["standard_property_name"]),
                    md_cell(type_label(record)),
                ]
            )
            + " |"
        )
    lines.append("")

    lines.append("## Remaining video properties")
    lines.append("")
    lines.append(
        "Canonical video ids that are not a cross-media accessor endpoint. "
        "Use `get` / `set` with the registry id."
    )
    lines.append("")
    lines.append("| Property id | Name | Type |")
    lines.append("|---|---|---|")
    for record in remaining_video:
        lines.append(
            "| "
            + " | ".join(
                [
                    md_cell(property_link(record["id"])),
                    md_cell(record["standard_property_name"]),
                    md_cell(type_label(record)),
                ]
            )
            + " |"
        )
    lines.append("")

    cast_keys = sorted(cast_source_keys(model["rules"]))
    lines.append("## Non-canonical keys that take part in casts")
    lines.append("")
    lines.append(
        "These base keys are not canonical properties. They appear as cast sources "
        "or as frequent camera tags that stay non-canonical. Details: "
        "[base keys](base-keys.md)."
    )
    lines.append("")
    for key in cast_keys:
        lines.append(f"- [`{key}`](base-keys.md#{key})")
    lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def cast_source_keys(rules: list[dict[str, Any]]) -> set[str]:
    keys: set[str] = set()
    for rule in rules:
        if rule["source_kind"] == "base_key":
            keys.add(rule["source_key"])
        if rule["target_kind"] == "base_key":
            keys.add(rule["target_key"])
    return keys


def rules_for_base_key(key: str, rules: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [
        rule
        for rule in rules
        if (rule["source_kind"] == "base_key" and rule["source_key"] == key)
        or (rule["target_kind"] == "base_key" and rule["target_key"] == key)
    ]


def render_base_keys(model: dict[str, Any]) -> str:
    rules = model["rules"]
    groups = model["groups"]
    cast_keys = sorted(cast_source_keys(rules))
    curated = {key: note for key, note in CURATED_CAMERA_KEYS}
    lines = [
        "# Base keys",
        "",
        *generated_banner(),
        "File-stored tags that are **not** canonical properties. Cast sources are "
        "opt-in links (C7). A short curated list of frequent camera keys stays "
        "non-canonical: libumm does not create an EXIF domain (C17).",
        "",
        "Listing every ExifTool tag would run to thousands of rows. See "
        f"[ExifTool tag names]({EXIFTOOL_TAGNAMES}), "
        f"[EXIF tags]({EXIFTOOL_EXIF}), and "
        f"[QuickTime tags]({EXIFTOOL_QUICKTIME}).",
        "",
        "Index: [property reference](README.md).",
        "",
        "## Cast sources",
        "",
    ]
    for key in cast_keys:
        lines.append(f'<a id="{key}"></a>')
        lines.append("")
        lines.append(f"## `{key}`")
        lines.append("")
        if key in curated:
            lines.append(curated[key])
            lines.append("")
        lines.append("| Group | Direction | Canonical partner | Heuristic | Citation |")
        lines.append("|---|---|---|---|---|")
        for rule in rules_for_base_key(key, rules):
            if rule["source_kind"] == "base_key" and rule["source_key"] == key:
                partner = endpoint_label(
                    rule["target_kind"],
                    rule["target_key"],
                    rule["target_field"],
                    rule["target_index"],
                )
            else:
                partner = endpoint_label(
                    rule["source_kind"],
                    rule["source_key"],
                    rule["source_field"],
                    rule["source_index"],
                )
            partner_cell = link_endpoint(partner, model["properties_by_id"])
            heuristic = rule["heuristic"]
            label = HEURISTIC_LABELS.get(heuristic, "")
            if label:
                heuristic = f"{heuristic} — {label}"
            group = groups.get((rule["group"], rule["direction"]), {})
            extra = []
            if group.get("approximate"):
                extra.append("approximate")
            flag = f" ({', '.join(extra)})" if extra else ""
            lines.append(
                "| "
                + " | ".join(
                    [
                        md_cell(f"`{rule['group']}`{flag}"),
                        md_cell(rule["direction"]),
                        md_cell(partner_cell),
                        md_cell(heuristic),
                        md_cell(rule["citation"]),
                    ]
                )
                + " |"
            )
        lines.append("")

    lines.append("## Frequent camera keys")
    lines.append("")
    lines.append(
        "These tags are common in camera files and stay non-canonical. They are "
        "visible through `dumpAll()` / `dumpUnmapped()` unless a cast consumes them."
    )
    lines.append("")
    for key, note in CURATED_CAMERA_KEYS:
        if key in cast_keys:
            lines.append(
                f"- [`{key}`](#{key}) — {note} Also a cast source (see above)."
            )
            continue
        lines.append(f'<a id="{key}"></a>')
        lines.append("")
        lines.append(f"### `{key}`")
        lines.append("")
        lines.append(note)
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def generate(
    registry_dirs: list[Path],
    overlay_path: Path,
    cross_media_path: Path,
    casts_dir: Path,
    output_dir: Path,
) -> dict[str, Path]:
    model = load_model(registry_dirs, overlay_path, cross_media_path, casts_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    paths = {
        "README.md": output_dir / "README.md",
        "photo.md": output_dir / "photo.md",
        "video.md": output_dir / "video.md",
        "base-keys.md": output_dir / "base-keys.md",
    }
    codegen.write_text(paths["README.md"], render_index(model))
    codegen.write_text(paths["photo.md"], render_domain_page("photo", model))
    codegen.write_text(paths["video.md"], render_domain_page("video", model))
    codegen.write_text(paths["base-keys.md"], render_base_keys(model))
    return paths


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--registry-dir",
        type=Path,
        action="append",
        dest="registry_dirs",
        help="Directory of registry JSON files (repeatable)",
    )
    parser.add_argument(
        "--overlay",
        type=Path,
        default=codegen.DEFAULT_OVERLAY,
        help="Curated IPTC EXIF overlay JSON",
    )
    parser.add_argument(
        "--cross-media",
        type=Path,
        default=codegen.DEFAULT_CROSS_MEDIA,
        help="Cross-media accessor map JSON",
    )
    parser.add_argument(
        "--casts-dir",
        type=Path,
        default=codegen.DEFAULT_CASTS_DIR,
        help="Directory of curated cast-rule JSON files",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help="Directory for generated markdown",
    )
    args = parser.parse_args(argv)
    try:
        registry_dirs = args.registry_dirs or list(codegen.DEFAULT_REGISTRY_DIRS)
        generate(
            [path.resolve() for path in registry_dirs],
            args.overlay.resolve(),
            args.cross_media.resolve(),
            args.casts_dir.resolve(),
            args.output_dir.resolve(),
        )
    except (
        GenerateError,
        codegen.CodegenError,
        OSError,
        KeyError,
        TypeError,
        json.JSONDecodeError,
    ) as exc:
        print(f"generate_property_reference: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
