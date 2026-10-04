#!/usr/bin/env python3
"""Import the vendored IPTC Photo Metadata Technical Reference into the registry."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE_DIR = REPO_ROOT / "registry" / "sources"
DEFAULT_SOURCE_MD = DEFAULT_SOURCE_DIR / "SOURCE.md"
DEFAULT_OUTPUT = REPO_ROOT / "registry" / "iptc-photo" / "iptc-photo.json"

SOURCE_FIELD_RE = re.compile(r"^([a-z_]+):\s+(.+?)\s*$")
REQUIRED_SOURCE_FIELDS = (
    "document",
    "standard",
    "version",
    "url",
    "retrieval_date",
    "filename",
    "core_schema",
    "extension_schema",
)

SCHEMA_BY_TR = {
    "IptcCore": "core_schema",
    "IptcExt": "extension_schema",
}

# PLUS members appear inside IPTC Extension structures (Licensor, Image Creator, …).
LITERAL_SCHEMAS = {
    "PLUS": "PLUS",
}

XMP_NAMESPACES = {
    "dc": "http://purl.org/dc/elements/1.1/",
    "photoshop": "http://ns.adobe.com/photoshop/1.0/",
    "Iptc4xmpCore": "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
    "Iptc4xmpExt": "http://iptc.org/std/Iptc4xmpExt/2008-02-29/",
    "xmp": "http://ns.adobe.com/xap/1.0/",
    "xmpRights": "http://ns.adobe.com/xap/1.0/rights/",
    "plus": "http://ns.useplus.org/ldf/xmp/1.0/",
    "exif": "http://ns.adobe.com/exif/1.0/",
}

PROPERTY_KEYS = (
    "id",
    "standard",
    "standard_version",
    "schema",
    "standard_property_name",
    "definition",
    "datatype",
    "cardinality",
    "struct_type",
    "representations",
    "mapping_notes",
    "source",
)

STRUCT_KEYS = (
    "id",
    "name",
    "standard",
    "standard_version",
    "source",
    "fields",
)

ENVELOPE_KEYS = (
    "standard",
    "standard_version",
    "source",
    "counts",
    "properties",
    "structs",
)

COUNTS_KEYS = ("properties", "core", "extension", "structs")
SOURCE_KEYS = ("document", "version", "url", "retrieval_date")
REPR_KEYS = ("xmp", "iptc_iim", "exif", "exiftool")

FIELD_EXTRA_KEYS = ("et_tag",)


class ImportError_(RuntimeError):
    """Importer failed closed on unknown or malformed Technical Reference data."""


def load_source_md(path: Path) -> dict[str, str]:
    fields: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        match = SOURCE_FIELD_RE.fullmatch(raw.strip())
        if not match:
            continue
        key, value = match.group(1), match.group(2)
        if key in fields:
            raise ImportError_(f"duplicate source field {key!r} in {path}")
        fields[key] = value
    missing = [key for key in REQUIRED_SOURCE_FIELDS if key not in fields]
    if missing:
        raise ImportError_(f"missing source fields in {path}: {missing}")
    return fields


def source_object(meta: dict[str, str]) -> dict[str, str]:
    return ordered(
        {
            "document": meta["document"],
            "version": meta["version"],
            "url": meta["url"],
            "retrieval_date": meta["retrieval_date"],
        },
        SOURCE_KEYS,
    )


def ordered(data: dict[str, Any], keys: tuple[str, ...]) -> dict[str, Any]:
    unknown = [key for key in data if key not in keys]
    if unknown:
        raise ImportError_(f"unexpected keys {unknown} (allowed {list(keys)})")
    return {key: data[key] for key in keys}


def property_id(name: str) -> str:
    cleaned = name.replace("'", "").replace("’", "")
    parts = re.findall(r"[A-Za-z0-9]+", cleaned)
    if not parts:
        raise ImportError_(f"cannot derive id from property name {name!r}")
    camel = parts[0].lower() + "".join(part.capitalize() for part in parts[1:])
    return f"iptc.photo.{camel}"


def map_cardinality(occurrence: str) -> str:
    if occurrence == "single":
        return "one"
    if occurrence == "multi":
        return "many"
    raise ImportError_(f"unknown propoccurrence {occurrence!r}")


def map_schema(ipmdschema: str, meta: dict[str, str]) -> str:
    if ipmdschema in LITERAL_SCHEMAS:
        return LITERAL_SCHEMAS[ipmdschema]
    field = SCHEMA_BY_TR.get(ipmdschema)
    if field is None:
        raise ImportError_(f"unknown ipmdschema {ipmdschema!r}")
    return meta[field]


def map_datatype(
    datatype: str,
    dataformat: str | None,
    struct_names: set[str],
) -> tuple[str, str | None]:
    if datatype == "any":
        if dataformat:
            raise ImportError_(f"datatype any with unexpected dataformat {dataformat!r}")
        return "any", None
    if datatype == "string":
        if dataformat is None:
            return "string", None
        if dataformat in ("uri", "url"):
            return "uri", None
        if dataformat == "date-time":
            return "date-time", None
        raise ImportError_(f"unknown string dataformat {dataformat!r}")
    if datatype == "number":
        if dataformat is None:
            return "number", None
        if dataformat == "integer":
            return "integer", None
        raise ImportError_(f"unknown number dataformat {dataformat!r}")
    if datatype == "struct":
        if dataformat == "AltLang":
            return "lang-alt", None
        if dataformat in struct_names:
            return "struct", dataformat
        raise ImportError_(f"unknown struct dataformat {dataformat!r}")
    raise ImportError_(f"unknown datatype {datatype!r}")


def xmp_representation(xmp_id: str | None) -> dict[str, str] | None:
    if not xmp_id:
        return None
    if ":" not in xmp_id:
        raise ImportError_(f"XMPid is not prefixed: {xmp_id!r}")
    prefix, _local = xmp_id.split(":", 1)
    namespace = XMP_NAMESPACES.get(prefix)
    if namespace is None:
        raise ImportError_(f"unknown XMP prefix {prefix!r} in {xmp_id!r}")
    return {"namespace": namespace, "property": xmp_id}


def iim_representation(record: dict[str, Any]) -> dict[str, str] | None:
    dataset = record.get("IIMid")
    if not dataset:
        return None
    out: dict[str, str] = {"dataset": str(dataset)}
    name = record.get("IIMname")
    if name:
        out["name"] = str(name)
    return out


def exif_representation(record: dict[str, Any]) -> dict[str, str] | None:
    tag = record.get("etEXIF") or record.get("EXIFid")
    if not tag:
        return None
    return {"tag": str(tag)}


def exiftool_tag_name(record: dict[str, Any]) -> str | None:
    tag = record.get("etTag") or record.get("etXMP")
    if not tag:
        return None
    name = str(tag)
    if ":" in name:
        name = name.rsplit(":", 1)[-1]
    return name or None


def exiftool_representation(record: dict[str, Any]) -> dict[str, str] | None:
    name = exiftool_tag_name(record)
    if not name:
        return None
    return {"tag": name}


def representations(record: dict[str, Any]) -> dict[str, Any]:
    return ordered(
        {
            "xmp": xmp_representation(record.get("XMPid")),
            "iptc_iim": iim_representation(record),
            "exif": exif_representation(record),
            "exiftool": exiftool_representation(record),
        },
        REPR_KEYS,
    )


def convert_member(
    *,
    record_id: str,
    record: dict[str, Any],
    meta: dict[str, str],
    source: dict[str, str],
    struct_names: set[str],
    require_schema: bool,
) -> dict[str, Any]:
    if "datatype" not in record:
        raise ImportError_(f"{record_id}: missing datatype")
    if "propoccurrence" not in record:
        raise ImportError_(f"{record_id}: missing propoccurrence")
    datatype, struct_type = map_datatype(
        str(record["datatype"]),
        str(record["dataformat"]) if "dataformat" in record else None,
        struct_names,
    )
    schema_value = ""
    if require_schema:
        if "ipmdschema" not in record:
            raise ImportError_(f"{record_id}: missing ipmdschema")
        schema_value = map_schema(str(record["ipmdschema"]), meta)
    elif "ipmdschema" in record:
        schema_value = map_schema(str(record["ipmdschema"]), meta)
    payload: dict[str, Any] = {
        "id": record_id,
        "standard": meta["standard"],
        "standard_version": meta["version"],
        "schema": schema_value,
        "standard_property_name": str(record.get("name") or ""),
        "definition": str(record.get("helptext") or ""),
        "datatype": datatype,
        "cardinality": map_cardinality(str(record["propoccurrence"])),
        "struct_type": struct_type,
        "representations": representations(record),
        "mapping_notes": str(record.get("usernotes") or ""),
        "source": source,
    }
    if not require_schema:
        payload["et_tag"] = exiftool_tag_name(record)
        # Struct fields still record schema when the TR provides ipmdschema.
        if not schema_value:
            payload.pop("schema")
            keys = tuple(key for key in PROPERTY_KEYS if key != "schema") + FIELD_EXTRA_KEYS
            return ordered(payload, keys)
        return ordered(payload, PROPERTY_KEYS + FIELD_EXTRA_KEYS)
    return ordered(payload, PROPERTY_KEYS)


def convert_property(
    tr_key: str,
    record: dict[str, Any],
    meta: dict[str, str],
    source: dict[str, str],
    struct_names: set[str],
) -> dict[str, Any]:
    name = str(record.get("name") or "")
    if not name:
        raise ImportError_(f"property {tr_key!r} has no name")
    return convert_member(
        record_id=property_id(name),
        record=record,
        meta=meta,
        source=source,
        struct_names=struct_names,
        require_schema=True,
    )


def is_altlang(struct: Any) -> bool:
    return isinstance(struct, dict) and "Note" in struct and not any(
        isinstance(value, dict) and "datatype" in value for value in struct.values()
    )


def convert_struct(
    name: str,
    struct: dict[str, Any],
    meta: dict[str, str],
    source: dict[str, str],
    struct_names: set[str],
) -> dict[str, Any]:
    fields: list[dict[str, Any]] = []
    for tr_key in sorted(struct):
        member = struct[tr_key]
        if not isinstance(member, dict) or "datatype" not in member:
            raise ImportError_(f"struct {name}.{tr_key} is not a typed field")
        fields.append(
            convert_member(
                record_id=f"iptc.photo.struct.{name}.{tr_key}",
                record=member,
                meta=meta,
                source=source,
                struct_names=struct_names,
                require_schema=False,
            )
        )
    return ordered(
        {
            "id": f"iptc.photo.struct.{name}",
            "name": name,
            "standard": meta["standard"],
            "standard_version": meta["version"],
            "source": source,
            "fields": fields,
        },
        STRUCT_KEYS,
    )


def dump_json(data: dict[str, Any]) -> str:
    return json.dumps(data, indent=2, ensure_ascii=False) + "\n"


def import_registry(tr: dict[str, Any], meta: dict[str, str]) -> dict[str, Any]:
    if "ipmd_top" not in tr or "ipmd_struct" not in tr:
        raise ImportError_("Technical Reference is missing ipmd_top / ipmd_struct")
    ipmd_top = tr["ipmd_top"]
    ipmd_struct = tr["ipmd_struct"]
    if not isinstance(ipmd_top, dict) or not isinstance(ipmd_struct, dict):
        raise ImportError_("ipmd_top and ipmd_struct must be objects")

    struct_names = {name for name, value in ipmd_struct.items() if not is_altlang(value)}
    source = source_object(meta)

    properties = [
        convert_property(key, ipmd_top[key], meta, source, struct_names)
        for key in ipmd_top
    ]
    properties.sort(key=lambda item: item["id"])
    ids = [item["id"] for item in properties]
    if len(ids) != len(set(ids)):
        raise ImportError_("duplicate property ids")

    structs = [
        convert_struct(name, ipmd_struct[name], meta, source, struct_names)
        for name in sorted(struct_names)
    ]

    core = sum(1 for item in properties if item["schema"] == meta["core_schema"])
    extension = sum(1 for item in properties if item["schema"] == meta["extension_schema"])
    if core + extension != len(properties):
        raise ImportError_("property schemas do not partition Core/Extension")

    tr_core = sum(1 for rec in ipmd_top.values() if rec.get("ipmdschema") == "IptcCore")
    tr_ext = sum(1 for rec in ipmd_top.values() if rec.get("ipmdschema") == "IptcExt")
    if core != tr_core or extension != tr_ext or len(properties) != len(ipmd_top):
        raise ImportError_("imported counts do not match the Technical Reference")

    return ordered(
        {
            "standard": meta["standard"],
            "standard_version": meta["version"],
            "source": source,
            "counts": ordered(
                {
                    "properties": len(properties),
                    "core": core,
                    "extension": extension,
                    "structs": len(structs),
                },
                COUNTS_KEYS,
            ),
            "properties": properties,
            "structs": structs,
        },
        ENVELOPE_KEYS,
    )


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def run(source_dir: Path, output: Path) -> None:
    meta = load_source_md(source_dir / "SOURCE.md")
    tr_path = source_dir / meta["filename"]
    tr = json.loads(tr_path.read_text(encoding="utf-8"))
    if not isinstance(tr, dict):
        raise ImportError_(f"{tr_path} is not a JSON object")
    registry = import_registry(tr, meta)
    write_text(output, dump_json(registry))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-dir",
        type=Path,
        default=DEFAULT_SOURCE_DIR,
        help="Directory containing SOURCE.md and the vendored TR JSON",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="Registry JSON output path",
    )
    args = parser.parse_args(argv)
    try:
        run(args.source_dir.resolve(), args.output.resolve())
    except ImportError_ as exc:
        print(f"import_iptc: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
