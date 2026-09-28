#!/usr/bin/env python3
"""Generate C++ property tables from the IPTC photo registry JSON."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REGISTRY_DIR = REPO_ROOT / "registry" / "iptc-photo"
DEFAULT_OVERLAY = REPO_ROOT / "registry" / "mappings" / "iptc-exif-overlay.json"
DEFAULT_OUTPUT_DIR = REPO_ROOT / "src" / "generated"

HEADER_NAME = "property_registry.hpp"
SOURCE_NAME = "property_registry.cpp"

OVERLAY_SOURCE_KEYS = (
    "document",
    "version",
    "url",
    "retrieval_date",
    "note",
)
OVERLAY_MAPPING_KEYS = ("id", "exif_tag")
OVERLAY_KEYS = ("partial", "source", "mappings")

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


def registry_files(registry_dir: Path) -> list[Path]:
    files = sorted(path for path in registry_dir.glob("*.json") if path.is_file())
    if not files:
        raise CodegenError(f"no registry JSON files in {registry_dir}")
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
    for index, item in enumerate(mappings):
        if not isinstance(item, dict):
            raise CodegenError(f"{path}: mappings[{index}] must be an object")
        mapping = ordered(item, OVERLAY_MAPPING_KEYS, path=f"{path} mappings[{index}]")
        if not mapping["id"] or not mapping["exif_tag"]:
            raise CodegenError(f"{path}: mappings[{index}] has empty id or exif_tag")
        ids.append(mapping["id"])
        normalized.append({"id": mapping["id"], "exif_tag": mapping["exif_tag"]})
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
) -> dict[str, str]:
    by_id = {record["id"]: record for record in properties}
    merged: dict[str, str] = {}
    for mapping in overlay["mappings"]:
        property_id = mapping["id"]
        kind = known_ids.get(property_id)
        if kind is None:
            raise CodegenError(f"{overlay_path}: unknown id {property_id!r}")
        tag = mapping["exif_tag"]
        if kind != "property":
            # Struct-field GPS tags are recorded for Stage 4/6; they are not
            # top-level PropertyDef rows.
            continue
        existing = by_id[property_id]["representations"].get("exif")
        existing_tag = existing["tag"] if isinstance(existing, dict) else None
        if existing_tag and existing_tag != tag:
            raise CodegenError(
                f"{overlay_path}: EXIF conflict for {property_id}: "
                f"TR {existing_tag!r} vs overlay {tag!r}"
            )
        merged[property_id] = tag
    return merged


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


def banner(registry_paths: list[Path], overlay_path: Path, registries: list[dict[str, Any]]) -> str:
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
    return "\n".join(lines) + "\n\n"


def emit_property(record: dict[str, Any], exif_tag: str) -> str:
    xmp_ns = representation_field(record, "xmp", "namespace")
    xmp_prop = representation_field(record, "xmp", "property")
    iim = representation_field(record, "iptc_iim", "dataset")
    if not exif_tag:
        exif_tag = representation_field(record, "exif", "tag")
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
    return (
        f"{banner_text}"
        "#pragma once\n"
        "\n"
        '#include "umm/registry.hpp"\n'
        "\n"
        "#include <cstddef>\n"
        "#include <iterator>\n"
        "\n"
        "namespace umm::internal {\n"
        "\n"
        f"inline constexpr std::size_t kPropertyCount = {len(properties)};\n"
        f"inline constexpr std::size_t kStandardCount = {len(standards)};\n"
        "\n"
        "inline constexpr PropertyDef kProperties[] = {\n"
        f"{body}\n"
        "};\n"
        "\n"
        "inline constexpr Registry::StandardInfo kStandards[] = {\n"
        f"{standards_body}\n"
        "};\n"
        "\n"
        "static_assert(std::size(kProperties) == kPropertyCount);\n"
        "static_assert(std::size(kStandards) == kStandardCount);\n"
        "\n"
        "}  // namespace umm::internal\n"
    )


def generate_source(banner_text: str) -> str:
    return (
        f"{banner_text}"
        '#include "umm/registry.hpp"\n'
        "\n"
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
    registry_dir: Path,
    overlay_path: Path,
    output_dir: Path,
) -> tuple[Path, Path]:
    files = registry_files(registry_dir)
    overlay = load_overlay(overlay_path)
    registries: list[dict[str, Any]] = []
    properties: list[dict[str, Any]] = []
    standards: list[dict[str, str]] = []
    known_ids: dict[str, str] = {}
    seen_ids: set[str] = set()
    overlay_exif: dict[str, str] = {}

    for path in files:
        registry = load_json(path)
        if "properties" not in registry or "structs" not in registry:
            raise CodegenError(f"{path}: missing properties/structs")
        if not isinstance(registry["properties"], list):
            raise CodegenError(f"{path}: properties must be an array")
        registries.append(registry)
        file_ids = collect_known_ids(registry)
        known_ids.update(file_ids)
        overlay_exif.update(apply_overlay(registry["properties"], file_ids, overlay, overlay_path))
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

    properties.sort(key=lambda item: item["id"])
    banner_text = banner(files, overlay_path, registries)
    header = generate_header(
        banner_text=banner_text,
        properties=properties,
        overlay_exif=overlay_exif,
        standards=standards,
    )
    source = generate_source(banner_text)
    output_dir.mkdir(parents=True, exist_ok=True)
    header_path = output_dir / HEADER_NAME
    source_path = output_dir / SOURCE_NAME
    write_text(header_path, header)
    write_text(source_path, source)
    return header_path, source_path


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
        default=DEFAULT_REGISTRY_DIR,
        help="Directory of registry JSON files (default: registry/iptc-photo)",
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
    args = parser.parse_args(argv)
    try:
        generate(
            args.registry_dir.resolve(),
            args.overlay.resolve(),
            args.output_dir.resolve(),
        )
    except (CodegenError, OSError, json.JSONDecodeError, KeyError) as exc:
        print(f"generate_cpp: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
