#!/usr/bin/env python3
"""Generate C++ property tables from IPTC photo and video registry JSON."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REGISTRY_DIRS = (
    REPO_ROOT / "registry" / "iptc-photo",
    REPO_ROOT / "registry" / "iptc-video",
)
DEFAULT_OVERLAY = REPO_ROOT / "registry" / "mappings" / "iptc-exif-overlay.json"
DEFAULT_CROSS_MEDIA = REPO_ROOT / "registry" / "mappings" / "cross-media-accessors.json"
DEFAULT_OUTPUT_DIR = REPO_ROOT / "src" / "generated"

HEADER_NAME = "property_registry.hpp"
SOURCE_NAME = "property_registry.cpp"
CROSS_MEDIA_HEADER_NAME = "cross_media_accessors.hpp"

OVERLAY_SOURCE_KEYS = (
    "document",
    "version",
    "url",
    "retrieval_date",
    "note",
)
OVERLAY_MAPPING_KEYS = ("id", "exif_tag")
OVERLAY_MAPPING_OPTIONAL = ("struct_property",)
OVERLAY_KEYS = ("partial", "source", "mappings")

CROSS_MEDIA_KEYS = ("document", "version", "source", "accessors")
CROSS_MEDIA_SOURCE_KEYS = ("document", "note")
CROSS_MEDIA_ACCESSOR_KEYS = (
    "concept",
    "photo_ids",
    "video_ids",
    "audio_ids",
    "tier",
    "transposition",
    "photo_datatype",
    "photo_cardinality",
    "video_datatype",
    "video_cardinality",
    "deferred",
    "notes",
)
TRANSPOSITIONS = (
    "passthrough",
    "string_to_lang_alt",
    "string_list_to_lang_alt",
    "lang_alt_to_string",
    "names_to_entity_list",
    "uri_to_cv_term",
    "struct_field_subset",
    "list_to_single",
    "name_uri_to_entity",
)

# JSON datatype + cardinality -> umm::Datatype enumerator (include/umm/registry.hpp).
DATATYPE_ENUM = {
    ("string", "one"): "text",
    ("string", "many"): "text_list",
    ("uri", "one"): "text",
    ("uri", "many"): "text_list",
    ("lang-alt", "one"): "lang_alt",
    ("lang-alt", "many"): "lang_alt",
    ("integer", "one"): "integer",
    ("integer", "many"): "integer",
    ("number", "one"): "real",
    ("number", "many"): "real",
    ("date-time", "one"): "date_time",
    ("date-time", "many"): "date_time",
    ("boolean", "one"): "boolean",
    ("boolean", "many"): "boolean",
    ("struct", "one"): "structure",
    ("struct", "many"): "structure_list",
}

CARDINALITY_ENUM = {
    "one": "one",
    "many": "many",
}


class CodegenError(RuntimeError):
    """Generator failed closed on unknown or conflicting registry data."""


def load_json(path: Path) -> Any:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise CodegenError(f"{path} is not a JSON object")
    return data


def ordered(data: dict[str, Any], keys: tuple[str, ...], *, path: str) -> dict[str, Any]:
    unknown = [key for key in data if key not in keys]
    if unknown:
        raise CodegenError(f"{path}: unexpected keys {unknown} (allowed {list(keys)})")
    missing = [key for key in keys if key not in data]
    if missing:
        raise CodegenError(f"{path}: missing keys {missing}")
    return {key: data[key] for key in keys}


def cpp_string(value: str) -> str:
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )
    return f'"{escaped}"'


def registry_files(registry_dirs: list[Path]) -> list[Path]:
    files: list[Path] = []
    for registry_dir in registry_dirs:
        found = sorted(path for path in registry_dir.glob("*.json") if path.is_file())
        if not found:
            raise CodegenError(f"no registry JSON files in {registry_dir}")
        files.extend(found)
    return files


def collect_known_ids(registry: dict[str, Any]) -> dict[str, str]:
    known: dict[str, str] = {}
    for record in registry["properties"]:
        known[record["id"]] = "property"
    for struct in registry["structs"]:
        known[struct["id"]] = "struct"
        for field in struct["fields"]:
            known[field["id"]] = "field"
    return known


def load_overlay(path: Path) -> dict[str, Any]:
    raw = load_json(path)
    overlay = ordered(raw, OVERLAY_KEYS, path=str(path))
    if overlay["partial"] is not True:
        raise CodegenError(f"{path}: partial must be true until Stage 6 completes the overlay")
    source = overlay["source"]
    if not isinstance(source, dict):
        raise CodegenError(f"{path}: source must be an object")
    overlay["source"] = ordered(source, OVERLAY_SOURCE_KEYS, path=f"{path} source")
    mappings = overlay["mappings"]
    if not isinstance(mappings, list) or not mappings:
        raise CodegenError(f"{path}: mappings must be a non-empty array")
    normalized: list[dict[str, str]] = []
    ids: list[str] = []
    allowed = OVERLAY_MAPPING_KEYS + OVERLAY_MAPPING_OPTIONAL
    for index, item in enumerate(mappings):
        if not isinstance(item, dict):
            raise CodegenError(f"{path}: mappings[{index}] must be an object")
        unknown = [key for key in item if key not in allowed]
        if unknown:
            raise CodegenError(
                f"{path} mappings[{index}]: unexpected keys {unknown} "
                f"(allowed {list(allowed)})"
            )
        missing = [key for key in OVERLAY_MAPPING_KEYS if key not in item]
        if missing:
            raise CodegenError(f"{path} mappings[{index}]: missing keys {missing}")
        mapping = {key: item[key] for key in OVERLAY_MAPPING_KEYS}
        if not mapping["id"] or not mapping["exif_tag"]:
            raise CodegenError(f"{path}: mappings[{index}] has empty id or exif_tag")
        struct_property = item.get("struct_property")
        if struct_property is not None:
            if not isinstance(struct_property, str) or not struct_property:
                raise CodegenError(
                    f"{path}: mappings[{index}] struct_property must be a non-empty string"
                )
            mapping["struct_property"] = struct_property
        ids.append(mapping["id"])
        normalized.append(mapping)
    if len(ids) != len(set(ids)):
        raise CodegenError(f"{path}: duplicate overlay mapping ids")
    if ids != sorted(ids):
        raise CodegenError(f"{path}: overlay mappings must be sorted by id")
    overlay["mappings"] = normalized
    return overlay


def apply_overlay(
    properties: list[dict[str, Any]],
    known_ids: dict[str, str],
    overlay: dict[str, Any],
    overlay_path: Path,
) -> tuple[dict[str, str], dict[str, dict[str, str]]]:
    by_id = {record["id"]: record for record in properties}
    merged: dict[str, str] = {}
    field_overlay: dict[str, dict[str, str]] = {}
    for mapping in overlay["mappings"]:
        property_id = mapping["id"]
        kind = known_ids.get(property_id)
        if kind is None:
            raise CodegenError(f"{overlay_path}: unknown id {property_id!r}")
        tag = mapping["exif_tag"]
        struct_property = mapping.get("struct_property")
        if kind != "property":
            if not struct_property:
                raise CodegenError(
                    f"{overlay_path}: {property_id} is a struct field and requires "
                    "struct_property"
                )
            if struct_property != "locationCreated":
                raise CodegenError(
                    f"{overlay_path}: {property_id} struct_property "
                    f"{struct_property!r} is not locationCreated"
                )
            field_overlay[property_id] = {
                "exif_tag": tag,
                "struct_property": struct_property,
            }
            continue
        if struct_property:
            raise CodegenError(
                f"{overlay_path}: {property_id} is a property; struct_property is "
                "only valid on struct fields"
            )
        existing = by_id[property_id]["representations"].get("exif")
        existing_tag = existing["tag"] if isinstance(existing, dict) else None
        if existing_tag and existing_tag != tag:
            raise CodegenError(
                f"{overlay_path}: EXIF conflict for {property_id}: "
                f"TR {existing_tag!r} vs overlay {tag!r}"
            )
        merged[property_id] = tag
    return merged, field_overlay


def load_cross_media(path: Path, properties_by_id: dict[str, dict[str, Any]]) -> dict[str, Any]:
    raw = load_json(path)
    mapping = ordered(raw, CROSS_MEDIA_KEYS, path=str(path))
    source = mapping["source"]
    if not isinstance(source, dict):
        raise CodegenError(f"{path}: source must be an object")
    mapping["source"] = ordered(source, CROSS_MEDIA_SOURCE_KEYS, path=f"{path} source")
    accessors = mapping["accessors"]
    if not isinstance(accessors, list) or not accessors:
        raise CodegenError(f"{path}: accessors must be a non-empty array")
    normalized: list[dict[str, Any]] = []
    concepts: list[str] = []
    for index, item in enumerate(accessors):
        if not isinstance(item, dict):
            raise CodegenError(f"{path}: accessors[{index}] must be an object")
        row = ordered(item, CROSS_MEDIA_ACCESSOR_KEYS, path=f"{path} accessors[{index}]")
        concept = row["concept"]
        if not concept or not isinstance(concept, str):
            raise CodegenError(f"{path}: accessors[{index}] has empty concept")
        if concept in concepts:
            raise CodegenError(f"{path}: duplicate concept {concept!r}")
        concepts.append(concept)
        if row["tier"] not in (1, 2, 3):
            raise CodegenError(f"{path}: {concept}: tier must be 1, 2, or 3")
        if row["transposition"] not in TRANSPOSITIONS:
            raise CodegenError(f"{path}: {concept}: unknown transposition {row['transposition']!r}")
        if not isinstance(row["deferred"], bool):
            raise CodegenError(f"{path}: {concept}: deferred must be a boolean")
        if not isinstance(row["notes"], str):
            raise CodegenError(f"{path}: {concept}: notes must be a string")
        if not isinstance(row["audio_ids"], list) or row["audio_ids"]:
            raise CodegenError(f"{path}: {concept}: audio_ids is reserved and must be []")
        photo_ids = row["photo_ids"]
        video_ids = row["video_ids"]
        if not isinstance(photo_ids, list) or not photo_ids or len(photo_ids) > 2:
            raise CodegenError(f"{path}: {concept}: photo_ids must be 1 or 2 registry ids")
        if not isinstance(video_ids, list) or not video_ids or len(video_ids) > 2:
            raise CodegenError(f"{path}: {concept}: video_ids must be 1 or 2 registry ids")
        for property_id in photo_ids + video_ids:
            if property_id not in properties_by_id:
                raise CodegenError(f"{path}: {concept}: unknown id {property_id!r}")
        photo = properties_by_id[photo_ids[0]]
        video = properties_by_id[video_ids[0]]
        declared_photo = (row["photo_datatype"], row["photo_cardinality"])
        declared_video = (row["video_datatype"], row["video_cardinality"])
        actual_photo = (photo["datatype"], photo["cardinality"])
        actual_video = (video["datatype"], video["cardinality"])
        if declared_photo != actual_photo:
            raise CodegenError(
                f"{path}: {concept}: photo datatype/cardinality {declared_photo} "
                f"does not match registry {actual_photo}"
            )
        if declared_video != actual_video:
            raise CodegenError(
                f"{path}: {concept}: video datatype/cardinality {declared_video} "
                f"does not match registry {actual_video}"
            )
        validate_transposition(path, row, photo, video, len(photo_ids))
        normalized.append(row)
    mapping["accessors"] = normalized
    return mapping


def validate_transposition(
    path: Path,
    row: dict[str, Any],
    photo: dict[str, Any],
    video: dict[str, Any],
    photo_id_count: int,
) -> None:
    kind = row["transposition"]
    concept = row["concept"]
    p = (photo["datatype"], photo["cardinality"])
    v = (video["datatype"], video["cardinality"])
    expected: dict[str, Any] = {
        "passthrough": p == v,
        "string_to_lang_alt": p == ("string", "one") and v == ("lang-alt", "one"),
        "string_list_to_lang_alt": p == ("string", "many") and v == ("lang-alt", "one"),
        "lang_alt_to_string": p == ("lang-alt", "one") and v == ("string", "one"),
        "names_to_entity_list": p == ("string", "many") and v == ("struct", "many"),
        "uri_to_cv_term": p == ("uri", "one") and v == ("struct", "one"),
        "struct_field_subset": p[0] == "struct" and v[0] == "struct",
        "list_to_single": p[1] == "many" and v[1] == "one",
        "name_uri_to_entity": (
            p == ("lang-alt", "one") and v == ("struct", "many") and photo_id_count == 2
        ),
    }
    if not expected[kind]:
        raise CodegenError(
            f"{path}: {concept}: transposition {kind} is inconsistent with "
            f"photo {p} / video {v}"
        )


def map_datatype(record: dict[str, Any]) -> str:
    key = (record["datatype"], record["cardinality"])
    enumerator = DATATYPE_ENUM.get(key)
    if enumerator is None:
        raise CodegenError(
            f"{record['id']}: unsupported datatype/cardinality {key}"
        )
    return enumerator


def map_cardinality(record: dict[str, Any]) -> str:
    enumerator = CARDINALITY_ENUM.get(record["cardinality"])
    if enumerator is None:
        raise CodegenError(
            f"{record['id']}: unknown cardinality {record['cardinality']!r}"
        )
    return enumerator


def representation_field(record: dict[str, Any], *path: str) -> str:
    value: Any = record["representations"]
    for key in path:
        if not isinstance(value, dict):
            return ""
        value = value.get(key)
    if value is None:
        return ""
    if not isinstance(value, str):
        raise CodegenError(f"{record['id']}: representation {'/'.join(path)} is not a string")
    return value


def banner(
    registry_paths: list[Path],
    overlay_path: Path,
    registries: list[dict[str, Any]],
    cross_media_path: Path | None = None,
) -> str:
    lines = [
        "// GENERATED — do not edit",
        "//",
        f"// Generator: tools/registry/generate_cpp.py",
    ]
    for path, registry in zip(registry_paths, registries):
        try:
            rel = path.resolve().relative_to(REPO_ROOT).as_posix()
        except ValueError:
            rel = path.as_posix()
        lines.append(
            f"// Source registry: {rel} ({registry['standard']} {registry['standard_version']})"
        )
    try:
        overlay_rel = overlay_path.resolve().relative_to(REPO_ROOT).as_posix()
    except ValueError:
        overlay_rel = overlay_path.as_posix()
    lines.append(f"// EXIF overlay: {overlay_rel}")
    if cross_media_path is not None:
        try:
            map_rel = cross_media_path.resolve().relative_to(REPO_ROOT).as_posix()
        except ValueError:
            map_rel = cross_media_path.as_posix()
        lines.append(f"// Cross-media map: {map_rel}")
    return "\n".join(lines) + "\n\n"


def xmp_local_name(property_name: str) -> str:
    if ":" in property_name:
        return property_name.split(":", 1)[1]
    return property_name


def collect_struct_field_rows(
    registries: list[dict[str, Any]],
    field_overlay: dict[str, dict[str, str]],
) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for registry in registries:
        for struct in registry.get("structs", []):
            for field in struct.get("fields", []):
                field_id = field["id"]
                overlay = field_overlay.get(field_id, {})
                xmp = field.get("representations", {}).get("xmp") or {}
                exiftool = field.get("representations", {}).get("exiftool") or {}
                et_tag = field.get("et_tag") or exiftool.get("tag") or ""
                rows.append(
                    {
                        "id": field_id,
                        "struct_name": struct.get("name") or "",
                        "struct_property": overlay.get("struct_property") or "",
                        "xmp_property": xmp.get("property") or "",
                        "et_tag": et_tag,
                        "exif_tag": overlay.get("exif_tag") or "",
                    }
                )
    rows.sort(key=lambda item: item["id"])
    return rows


def collect_exiftool_struct_aliases(
    registries: list[dict[str, Any]],
) -> list[dict[str, str]]:
    aliases: list[dict[str, str]] = []
    seen: set[tuple[str, str, str]] = set()
    for registry in registries:
        structs = {
            struct["name"]: struct.get("fields", [])
            for struct in registry.get("structs", [])
        }
        for record in registry.get("properties", []):
            struct_type = record.get("struct_type")
            if not struct_type or struct_type not in structs:
                continue
            xmp = (record.get("representations") or {}).get("xmp") or {}
            local = xmp_local_name(xmp.get("property") or "")
            if not local:
                continue
            for field in structs[struct_type]:
                exiftool = (field.get("representations") or {}).get("exiftool") or {}
                et_tag = field.get("et_tag") or exiftool.get("tag") or ""
                if not et_tag:
                    continue
                from_field = field["id"].rsplit(".", 1)[-1]
                pairs = [(from_field, et_tag)]
                if (
                    from_field != "name"
                    and from_field.lower().endswith("name")
                    and et_tag.endswith("Name")
                ):
                    pairs.append(("name", et_tag))
                for src, dst in pairs:
                    if src == dst:
                        continue
                    key = (local, src, dst)
                    if key in seen:
                        continue
                    seen.add(key)
                    aliases.append(
                        {
                            "xmp_local": local,
                            "from_field": src,
                            "to_field": dst,
                        }
                    )
    aliases.sort(
        key=lambda item: (item["xmp_local"], item["from_field"], item["to_field"])
    )
    return aliases


def collect_exiftool_xmp_tag_aliases(
    registries: list[dict[str, Any]],
) -> list[dict[str, str]]:
    aliases: list[dict[str, str]] = []
    seen: set[tuple[str, str, str]] = set()
    for registry in registries:
        for record in registry.get("properties", []):
            xmp = (record.get("representations") or {}).get("xmp") or {}
            exiftool = (record.get("representations") or {}).get("exiftool") or {}
            xmp_prop = xmp.get("property") or ""
            et_tag = exiftool.get("tag") or ""
            if ":" not in xmp_prop or not et_tag:
                continue
            prefix, local = xmp_prop.split(":", 1)
            if prefix != "Iptc4xmpExt" or local == et_tag:
                continue
            key = (prefix, local, et_tag)
            if key in seen:
                continue
            seen.add(key)
            aliases.append(
                {
                    "xmp_ns": prefix,
                    "xmp_local": local,
                    "et_tag": et_tag,
                }
            )
    aliases.sort(key=lambda item: (item["xmp_ns"], item["xmp_local"], item["et_tag"]))
    return aliases


def emit_struct_field_row(row: dict[str, str]) -> str:
    return "\n".join(
        [
            "    {",
            f"        {cpp_string(row['id'])},",
            f"        {cpp_string(row['struct_name'])},",
            f"        {cpp_string(row['struct_property'])},",
            f"        {cpp_string(row['xmp_property'])},",
            f"        {cpp_string(row['et_tag'])},",
            f"        {cpp_string(row['exif_tag'])},",
            "    }",
        ]
    )


def emit_struct_alias_row(row: dict[str, str]) -> str:
    return (
        "    {"
        f" {cpp_string(row['xmp_local'])},"
        f" {cpp_string(row['from_field'])},"
        f" {cpp_string(row['to_field'])} "
        "}"
    )


def emit_xmp_tag_alias_row(row: dict[str, str]) -> str:
    return (
        "    {"
        f" {cpp_string(row['xmp_ns'])},"
        f" {cpp_string(row['xmp_local'])},"
        f" {cpp_string(row['et_tag'])} "
        "}"
    )


def emit_property(record: dict[str, Any], exif_tag: str) -> str:
    xmp_ns = representation_field(record, "xmp", "namespace")
    xmp_prop = representation_field(record, "xmp", "property")
    iim = representation_field(record, "iptc_iim", "dataset")
    if not exif_tag:
        exif_tag = representation_field(record, "exif", "tag")
    quicktime = representation_field(record, "quicktime", "key")
    ebucore = representation_field(record, "ebucore", "path")
    datatype = map_datatype(record)
    cardinality = map_cardinality(record)
    return "\n".join(
        [
            "    {",
            f"        {cpp_string(record['id'])},",
            f"        {cpp_string(record['standard'])},",
            f"        {cpp_string(record['standard_version'])},",
            f"        {cpp_string(record['schema'])},",
            f"        {cpp_string(record['standard_property_name'])},",
            f"        Datatype::{datatype},",
            f"        Cardinality::{cardinality},",
            "        {",
            f"            {cpp_string(xmp_ns)},",
            f"            {cpp_string(xmp_prop)},",
            f"            {cpp_string(iim)},",
            f"            {cpp_string(exif_tag)},",
            f"            {cpp_string(quicktime)},",
            f"            {cpp_string(ebucore)},",
            "        },",
            "    }",
        ]
    )


def generate_header(
    *,
    banner_text: str,
    properties: list[dict[str, Any]],
    overlay_exif: dict[str, str],
    standards: list[dict[str, str]],
    struct_fields: list[dict[str, str]],
    struct_aliases: list[dict[str, str]],
    xmp_tag_aliases: list[dict[str, str]],
) -> str:
    rows = [
        emit_property(record, overlay_exif.get(record["id"], ""))
        for record in properties
    ]
    body = ",\n".join(rows)
    standard_rows = []
    for item in standards:
        standard_rows.append(
            "    {\n"
            f"        {cpp_string(item['standard'])},\n"
            f"        {cpp_string(item['version'])},\n"
            f"        {cpp_string(item['source_document'])},\n"
            "    }"
        )
    standards_body = ",\n".join(standard_rows)
    if not struct_fields:
        raise CodegenError("no struct-field representation rows")
    if not struct_aliases:
        raise CodegenError("no ExifTool struct-field aliases")
    if not xmp_tag_aliases:
        raise CodegenError("no ExifTool XMP tag aliases")
    struct_field_rows = ",\n".join(emit_struct_field_row(row) for row in struct_fields)
    struct_alias_rows = ",\n".join(emit_struct_alias_row(row) for row in struct_aliases)
    xmp_alias_rows = ",\n".join(emit_xmp_tag_alias_row(row) for row in xmp_tag_aliases)
    return (
        f"{banner_text}"
        "#pragma once\n"
        "\n"
        '#include "umm/registry.hpp"\n'
        "\n"
        "#include <cstddef>\n"
        "#include <iterator>\n"
        "#include <string_view>\n"
        "\n"
        "namespace umm::internal {\n"
        "\n"
        "struct StructFieldRepresentation {\n"
        "  std::string_view id;\n"
        "  std::string_view struct_name;\n"
        "  std::string_view struct_property;\n"
        "  std::string_view xmp_property;\n"
        "  std::string_view et_tag;\n"
        "  std::string_view exif_tag;\n"
        "};\n"
        "\n"
        "struct ExifToolStructFieldAlias {\n"
        "  std::string_view xmp_local;\n"
        "  std::string_view from_field;\n"
        "  std::string_view to_field;\n"
        "};\n"
        "\n"
        "struct ExifToolXmpTagAlias {\n"
        "  std::string_view xmp_ns;\n"
        "  std::string_view xmp_local;\n"
        "  std::string_view et_tag;\n"
        "};\n"
        "\n"
        f"inline constexpr std::size_t kPropertyCount = {len(properties)};\n"
        f"inline constexpr std::size_t kStandardCount = {len(standards)};\n"
        f"inline constexpr std::size_t kStructFieldCount = {len(struct_fields)};\n"
        f"inline constexpr std::size_t kExifToolStructFieldAliasCount = {len(struct_aliases)};\n"
        f"inline constexpr std::size_t kExifToolXmpTagAliasCount = {len(xmp_tag_aliases)};\n"
        "\n"
        "inline constexpr PropertyDef kProperties[] = {\n"
        f"{body}\n"
        "};\n"
        "\n"
        "inline constexpr Registry::StandardInfo kStandards[] = {\n"
        f"{standards_body}\n"
        "};\n"
        "\n"
        "inline constexpr StructFieldRepresentation kStructFieldRepresentations[] = {\n"
        f"{struct_field_rows}\n"
        "};\n"
        "\n"
        "inline constexpr ExifToolStructFieldAlias kExifToolStructFieldAliases[] = {\n"
        f"{struct_alias_rows}\n"
        "};\n"
        "\n"
        "inline constexpr ExifToolXmpTagAlias kExifToolXmpTagAliases[] = {\n"
        f"{xmp_alias_rows}\n"
        "};\n"
        "\n"
        "static_assert(std::size(kProperties) == kPropertyCount);\n"
        "static_assert(std::size(kStandards) == kStandardCount);\n"
        "static_assert(std::size(kStructFieldRepresentations) == kStructFieldCount);\n"
        "static_assert(std::size(kExifToolStructFieldAliases) == "
        "kExifToolStructFieldAliasCount);\n"
        "static_assert(std::size(kExifToolXmpTagAliases) == kExifToolXmpTagAliasCount);\n"
        "\n"
        "}  // namespace umm::internal\n"
    )


def pad_ids(ids: list[str]) -> tuple[str, str]:
    first = ids[0]
    second = ids[1] if len(ids) > 1 else ""
    return first, second


def emit_cross_media_row(row: dict[str, Any]) -> str:
    photo_a, photo_b = pad_ids(row["photo_ids"])
    video_a, video_b = pad_ids(row["video_ids"])
    photo_dt = DATATYPE_ENUM[(row["photo_datatype"], row["photo_cardinality"])]
    video_dt = DATATYPE_ENUM[(row["video_datatype"], row["video_cardinality"])]
    deferred = "true" if row["deferred"] else "false"
    return "\n".join(
        [
            "    {",
            f"        {cpp_string(row['concept'])},",
            f"        {{{cpp_string(photo_a)}, {cpp_string(photo_b)}}},",
            f"        {len(row['photo_ids'])},",
            f"        {{{cpp_string(video_a)}, {cpp_string(video_b)}}},",
            f"        {len(row['video_ids'])},",
            f"        {row['tier']},",
            f"        CrossMediaTransposition::{row['transposition']},",
            f"        Datatype::{photo_dt},",
            f"        Cardinality::{row['photo_cardinality']},",
            f"        Datatype::{video_dt},",
            f"        Cardinality::{row['video_cardinality']},",
            f"        {deferred},",
            "    }",
        ]
    )


def generate_cross_media_header(*, banner_text: str, accessors: list[dict[str, Any]]) -> str:
    rows = [emit_cross_media_row(row) for row in accessors]
    body = ",\n".join(rows)
    return (
        f"{banner_text}"
        "#pragma once\n"
        "\n"
        '#include "umm/registry.hpp"\n'
        "\n"
        "#include <cstddef>\n"
        "#include <iterator>\n"
        "#include <string_view>\n"
        "\n"
        "namespace umm::internal {\n"
        "\n"
        "enum class CrossMediaTransposition {\n"
        "  passthrough,\n"
        "  string_to_lang_alt,\n"
        "  string_list_to_lang_alt,\n"
        "  lang_alt_to_string,\n"
        "  names_to_entity_list,\n"
        "  uri_to_cv_term,\n"
        "  struct_field_subset,\n"
        "  list_to_single,\n"
        "  name_uri_to_entity,\n"
        "};\n"
        "\n"
        "struct CrossMediaAccessorDef {\n"
        "  std::string_view concept_name;\n"
        "  std::string_view photo_ids[2];\n"
        "  std::size_t photo_id_count;\n"
        "  std::string_view video_ids[2];\n"
        "  std::size_t video_id_count;\n"
        "  int tier;\n"
        "  CrossMediaTransposition transposition;\n"
        "  Datatype photo_datatype;\n"
        "  Cardinality photo_cardinality;\n"
        "  Datatype video_datatype;\n"
        "  Cardinality video_cardinality;\n"
        "  bool deferred;\n"
        "};\n"
        "\n"
        f"inline constexpr std::size_t kCrossMediaAccessorCount = {len(accessors)};\n"
        "\n"
        "inline constexpr CrossMediaAccessorDef kCrossMediaAccessors[] = {\n"
        f"{body}\n"
        "};\n"
        "\n"
        "static_assert(std::size(kCrossMediaAccessors) == kCrossMediaAccessorCount);\n"
        "\n"
        "}  // namespace umm::internal\n"
    )


def generate_source(banner_text: str) -> str:
    return (
        f"{banner_text}"
        '#include "umm/registry.hpp"\n'
        "\n"
        '#include "cross_media_accessors.hpp"\n'
        '#include "property_registry.hpp"\n'
        "\n"
        "#include <iterator>\n"
        "#include <vector>\n"
        "\n"
        "namespace umm {\n"
        "\n"
        "const Registry& Registry::instance() noexcept {\n"
        "  static const Registry registry;\n"
        "  return registry;\n"
        "}\n"
        "\n"
        "const Registry& registry() noexcept {\n"
        "  return Registry::instance();\n"
        "}\n"
        "\n"
        "std::optional<PropertyDef> Registry::find(\n"
        "    std::string_view property_id) const noexcept {\n"
        "  for (const PropertyDef& record : internal::kProperties) {\n"
        "    if (record.id == property_id) {\n"
        "      return record;\n"
        "    }\n"
        "  }\n"
        "  return std::nullopt;\n"
        "}\n"
        "\n"
        "std::vector<PropertyDef> Registry::all() const {\n"
        "  return {std::begin(internal::kProperties), std::end(internal::kProperties)};\n"
        "}\n"
        "\n"
        "std::size_t Registry::size() const noexcept {\n"
        "  return internal::kPropertyCount;\n"
        "}\n"
        "\n"
        "std::vector<Registry::StandardInfo> Registry::standards() const {\n"
        "  return {std::begin(internal::kStandards), std::end(internal::kStandards)};\n"
        "}\n"
        "\n"
        "}  // namespace umm\n"
    )


def generate(
    registry_dirs: list[Path],
    overlay_path: Path,
    output_dir: Path,
    cross_media_path: Path | None = None,
) -> tuple[Path, Path, Path]:
    files = registry_files(registry_dirs)
    overlay = load_overlay(overlay_path)
    registries: list[dict[str, Any]] = []
    properties: list[dict[str, Any]] = []
    standards: list[dict[str, str]] = []
    known_ids: dict[str, str] = {}
    seen_ids: set[str] = set()

    for path in files:
        registry = load_json(path)
        if "properties" not in registry or "structs" not in registry:
            raise CodegenError(f"{path}: missing properties/structs")
        if not isinstance(registry["properties"], list):
            raise CodegenError(f"{path}: properties must be an array")
        registries.append(registry)
        file_ids = collect_known_ids(registry)
        known_ids.update(file_ids)
        for record in registry["properties"]:
            property_id = record["id"]
            if property_id in seen_ids:
                raise CodegenError(f"duplicate property id {property_id!r}")
            seen_ids.add(property_id)
            properties.append(record)
        standards.append(
            {
                "standard": registry["standard"],
                "version": registry["standard_version"],
                "source_document": registry["source"]["document"],
            }
        )

    overlay_exif, field_overlay = apply_overlay(
        properties, known_ids, overlay, overlay_path
    )
    mapping_path = cross_media_path or DEFAULT_CROSS_MEDIA
    properties_by_id = {record["id"]: record for record in properties}
    cross_media = load_cross_media(mapping_path, properties_by_id)

    properties.sort(key=lambda item: item["id"])
    struct_fields = collect_struct_field_rows(registries, field_overlay)
    struct_aliases = collect_exiftool_struct_aliases(registries)
    xmp_tag_aliases = collect_exiftool_xmp_tag_aliases(registries)
    banner_text = banner(files, overlay_path, registries, mapping_path)
    header = generate_header(
        banner_text=banner_text,
        properties=properties,
        overlay_exif=overlay_exif,
        standards=standards,
        struct_fields=struct_fields,
        struct_aliases=struct_aliases,
        xmp_tag_aliases=xmp_tag_aliases,
    )
    source = generate_source(banner_text)
    cross_header = generate_cross_media_header(
        banner_text=banner_text,
        accessors=cross_media["accessors"],
    )
    output_dir.mkdir(parents=True, exist_ok=True)
    header_path = output_dir / HEADER_NAME
    source_path = output_dir / SOURCE_NAME
    cross_path = output_dir / CROSS_MEDIA_HEADER_NAME
    write_text(header_path, header)
    write_text(source_path, source)
    write_text(cross_path, cross_header)
    return header_path, source_path, cross_path


def write_text(path: Path, text: str) -> None:
    if not text.endswith("\n"):
        text += "\n"
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--registry-dir",
        type=Path,
        action="append",
        dest="registry_dirs",
        help="Directory of registry JSON files (repeatable; default: iptc-photo and iptc-video)",
    )
    parser.add_argument(
        "--overlay",
        type=Path,
        default=DEFAULT_OVERLAY,
        help="Curated IPTC EXIF overlay JSON",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help="Directory for generated C++ files",
    )
    parser.add_argument(
        "--cross-media",
        type=Path,
        default=DEFAULT_CROSS_MEDIA,
        help="Hand-curated cross-media accessor map JSON",
    )
    args = parser.parse_args(argv)
    try:
        registry_dirs = args.registry_dirs or list(DEFAULT_REGISTRY_DIRS)
        generate(
            [path.resolve() for path in registry_dirs],
            args.overlay.resolve(),
            args.output_dir.resolve(),
            args.cross_media.resolve(),
        )
    except (CodegenError, OSError, json.JSONDecodeError, KeyError) as exc:
        print(f"generate_cpp: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
