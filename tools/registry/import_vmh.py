#!/usr/bin/env python3
"""Import vendored IPTC Video Metadata Hub artifacts into the registry."""

from __future__ import annotations

import argparse
import hashlib
import html as html_lib
import json
import re
import sys
from html.parser import HTMLParser
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE_DIR = REPO_ROOT / "registry" / "sources" / "vmh"
DEFAULT_OUTPUT = REPO_ROOT / "registry" / "iptc-video" / "iptc-video.json"

SOURCE_FIELD_RE = re.compile(r"^([a-z0-9_]+):\s+(.+?)\s*$")
REQUIRED_SOURCE_FIELDS = (
    "document",
    "standard",
    "version",
    "url",
    "retrieval_date",
    "schema_filename",
    "schema_sha256",
    "properties_filename",
    "properties_sha256",
    "mapping_filename",
    "mapping_sha256",
)

XMP_NAMESPACES = {
    "dc": "http://purl.org/dc/elements/1.1/",
    "photoshop": "http://ns.adobe.com/photoshop/1.0/",
    "Iptc4xmpCore": "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/",
    "Iptc4xmpExt": "http://iptc.org/std/Iptc4xmpExt/2008-02-29/",
    "xmp": "http://ns.adobe.com/xap/1.0/",
    "xmpRights": "http://ns.adobe.com/xap/1.0/rights/",
    "xmpDM": "http://ns.adobe.com/xmp/1.0/DynamicMedia/",
    "xmpMM": "http://ns.adobe.com/xap/1.0/mm/",
    "plus": "http://ns.useplus.org/ldf/xmp/1.0/",
    "exif": "http://ns.adobe.com/exif/1.0/",
    "tiff": "http://ns.adobe.com/tiff/1.0/",
    "stDim": "http://ns.adobe.com/xap/1.0/sType/Dimensions#",
}

SCHEMA_BY_GROUP = {
    "Administrative fields": "Administrative",
    "Fields describing audio/visual content": "Descriptive",
    "Rights fields": "Rights",
    "Technical fields": "Technical",
    "Time marker": "Time marker",
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

COUNTS_KEYS = (
    "properties",
    "administrative",
    "descriptive",
    "rights",
    "technical",
    "time_marker",
    "structs",
)
SOURCE_KEYS = ("document", "version", "url", "retrieval_date")
REPR_KEYS = ("xmp", "iptc_iim", "exif", "quicktime", "ebucore")
XMP_KEYS = ("namespace", "property")
QUICKTIME_KEYS = ("key",)
EBUCORE_KEYS = ("path",)

PROP_HEADERS = (
    "Property Group",
    "Property Name",
    "Definition / Semantics",
    "User Notes",
    "Change Notes",
    "Basic Type/Cardinality",
    "XMP Property",
    "XMP Data Type",
    "PVMD JSON Property",
    "PVMD JSON Data Type",
)

MAP_HEADERS = (
    "Property Group",
    "Property Name",
    "Definition / Semantics",
    "EBUcore",
    "XMP",
    "PVMD JSON",
    "Apple Quicktime",
)


class ImportError_(RuntimeError):
    """Importer failed closed on unknown or malformed Video Metadata Hub data."""


class TableParser(HTMLParser):
    """Collect text tables from a VMH specification HTML page."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.tables: list[list[list[str]]] = []
        self._in_table = False
        self._in_cell = False
        self._rows: list[list[str]] = []
        self._row: list[str] = []
        self._cell: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag == "table":
            self._in_table = True
            self._rows = []
        elif tag == "tr" and self._in_table:
            self._row = []
        elif tag in ("td", "th") and self._in_table:
            self._in_cell = True
            self._cell = []
        elif tag == "br" and self._in_cell:
            self._cell.append(" ")

    def handle_endtag(self, tag: str) -> None:
        if tag in ("td", "th") and self._in_table and self._in_cell:
            text = html_lib.unescape(re.sub(r"\s+", " ", "".join(self._cell))).strip()
            self._row.append(text)
            self._in_cell = False
        elif tag == "tr" and self._in_table and self._row:
            self._rows.append(self._row)
        elif tag == "table" and self._in_table:
            self.tables.append(self._rows)
            self._in_table = False

    def handle_data(self, data: str) -> None:
        if self._in_cell:
            self._cell.append(data)


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


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    digest.update(path.read_bytes())
    return digest.hexdigest()


def verify_checksum(path: Path, expected: str) -> None:
    actual = sha256_file(path)
    if actual != expected.lower():
        raise ImportError_(
            f"checksum mismatch for {path.name}: expected {expected}, got {actual}"
        )


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
    cleaned = name.replace("'", "").replace("’", "").replace("´", "")
    parts = re.findall(r"[A-Za-z0-9]+", cleaned)
    if not parts:
        raise ImportError_(f"cannot derive id from property name {name!r}")
    camel = parts[0].lower() + "".join(part.capitalize() for part in parts[1:])
    return f"iptc.video.{camel}"


def struct_id(name: str) -> str:
    cleaned = name.replace("'", "").replace("’", "").replace("´", "")
    parts = re.findall(r"[A-Za-z0-9]+", cleaned)
    if not parts:
        raise ImportError_(f"cannot derive struct id from name {name!r}")
    token = "".join(part.capitalize() for part in parts)
    return f"iptc.video.struct.{token}"


def parse_tables(html_text: str) -> list[list[list[str]]]:
    parser = TableParser()
    parser.feed(html_text)
    parser.close()
    return parser.tables


def table_with_header(
    tables: list[list[list[str]]], header: tuple[str, ...]
) -> list[list[str]]:
    for table in tables:
        if table and tuple(table[0][: len(header)]) == header:
            return table
    raise ImportError_(f"no HTML table with header {list(header)}")


def map_schema(group: str) -> str:
    schema = SCHEMA_BY_GROUP.get(group)
    if schema is None:
        raise ImportError_(f"unknown property group {group!r}")
    return schema


def map_json_type(json_type: str) -> tuple[str, str | None, str] | None:
    token = json_type.strip().replace("\n", "")
    if not token or token == "NA":
        return None
    parts = token.split("/")
    while len(parts) < 3:
        parts.append("")
    kind, fmt, occ = parts[0], parts[1], parts[2]
    cardinality = "many" if occ == "array" else "one"
    if kind == "string":
        if fmt in ("uri", "url"):
            return "uri", None, cardinality
        if fmt == "date-time":
            return "date-time", None, cardinality
        return "string", None, cardinality
    if kind == "number":
        if fmt == "integer":
            return "integer", None, cardinality
        if fmt in ("", "float", "number"):
            return "number", None, cardinality
        raise ImportError_(f"unknown number JSON format {json_type!r}")
    if kind == "boolean":
        return "boolean", None, cardinality
    if kind == "object":
        if fmt == "AltLang":
            return "lang-alt", None, cardinality
        if not fmt:
            raise ImportError_(f"object JSON type missing structure name: {json_type!r}")
        return "struct", fmt, cardinality
    if kind == "AltLang":
        return "lang-alt", None, cardinality
    if kind == "CvTerm":
        # One struct-field row omits the "object/" prefix (CvTerm//array).
        return "struct", "CvTerm", cardinality
    raise ImportError_(f"unknown JSON type {json_type!r}")


def map_html_type(type_card: str) -> tuple[str, str | None, str]:
    text = re.sub(r"\s+", " ", type_card).strip()
    if not text:
        raise ImportError_("empty Basic Type/Cardinality")
    if text == "XMP specific type":
        return "string", None, "one"
    if text == "PLUS CV Term/URI":
        return "uri", None, "one"

    card = "one"
    match = re.search(r"\(([^)]+)\)", text)
    body = text
    if match:
        occ = match.group(1).strip()
        body = (text[: match.start()] + text[match.end() :]).strip()
        if occ in ("0..1", "1"):
            card = "one"
        elif occ in ("0..unbounded", "1..unbounded", "unbounded"):
            card = "many"
        else:
            raise ImportError_(f"unknown cardinality {occ!r} in {type_card!r}")

    body_lower = body.lower()
    if "language tag" in body_lower and "text" in body_lower:
        return "lang-alt", None, card
    if body_lower.startswith("date"):
        return "date-time", None, card
    if body_lower.startswith("boolean"):
        return "boolean", None, card
    if body_lower.startswith("integer") or body_lower.startswith("number/integer"):
        return "integer", None, card
    if body_lower.startswith("number"):
        return "number", None, card
    if body_lower.startswith("url") or body_lower.startswith("uri"):
        return "uri", None, card
    if "text/uri" in body_lower:
        return "uri", None, card
    if " or " in body_lower and "structure" in body_lower:
        names = [part.strip() for part in re.split(r"\s+or\s+", body, flags=re.I)]
        cleaned = [re.sub(r"\s+structure$", "", name, flags=re.I).strip() for name in names]
        return "struct", " or ".join(cleaned), card
    if "structure" in body_lower:
        name = re.sub(r"\s+structure$", "", body, flags=re.I).strip()
        if not name:
            raise ImportError_(f"structure type missing name: {type_card!r}")
        return "struct", name, card
    if body_lower.startswith("text") or body_lower.startswith("language tag"):
        return "string", None, card
    raise ImportError_(f"unknown HTML type {type_card!r}")


def map_datatype(json_type: str, html_type: str) -> tuple[str, str | None, str]:
    mapped = map_json_type(json_type)
    if mapped is not None:
        return mapped
    return map_html_type(html_type)


def xmp_representation(xmp_id: str) -> dict[str, str] | None:
    token = xmp_id.strip()
    if not token or token == "NA":
        return None
    if ":" not in token:
        raise ImportError_(f"XMPid is not prefixed: {token!r}")
    prefix, _local = token.split(":", 1)
    namespace = XMP_NAMESPACES.get(prefix)
    if namespace is None:
        raise ImportError_(f"unknown XMP prefix {prefix!r} in {token!r}")
    return ordered({"namespace": namespace, "property": token}, XMP_KEYS)


def looks_like_ebucore(value: str) -> bool:
    token = value.strip()
    if not token:
        return False
    lowered = token.lower()
    if "ebucore" in lowered:
        return True
    if ":" in token.split()[0]:
        return False
    return "/" in token or token.startswith("@")


def looks_like_quicktime(value: str) -> bool:
    return "quicktime" in value.lower() or value.strip().startswith("com.apple.")


def mapping_index(rows: list[list[str]]) -> dict[str, list[list[str]]]:
    by_name: dict[str, list[list[str]]] = {}
    for row in rows[1:]:
        if len(row) < 7:
            continue
        name = row[1].strip()
        if not name:
            continue
        by_name.setdefault(name, []).append(row)
    return by_name


def pick_mapping_row(
    rows: list[list[str]], group: str
) -> list[str] | None:
    if not rows:
        return None
    group_token = group.split()[0].lower() if group else ""
    for row in rows:
        cell_group = row[0].strip().lower()
        if cell_group and group_token and cell_group.startswith(group_token):
            return row
    for row in rows:
        if row[0].strip():
            return row
    return rows[0]


def quicktime_representation(row: list[str] | None) -> dict[str, str] | None:
    if row is None:
        return None
    value = row[6].strip()
    if not value:
        return None
    return ordered({"key": value}, QUICKTIME_KEYS)


def ebucore_representation(row: list[str] | None) -> dict[str, str] | None:
    if row is None:
        return None
    labeled = row[3].strip()
    json_col = row[5].strip()
    if looks_like_ebucore(labeled):
        return ordered({"path": labeled}, EBUCORE_KEYS)
    if looks_like_ebucore(json_col):
        # VMH 1.7 mapping HTML labels are shifted relative to the sheet
        # columns: EBUCore paths are published under the PVMD JSON header.
        return ordered({"path": json_col}, EBUCORE_KEYS)
    if labeled and not looks_like_quicktime(labeled) and ":" not in labeled:
        return ordered({"path": labeled}, EBUCORE_KEYS)
    return None


def representations(
    *,
    xmp_id: str,
    mapping_row: list[str] | None,
) -> dict[str, Any]:
    return ordered(
        {
            "xmp": xmp_representation(xmp_id),
            "iptc_iim": None,
            "exif": None,
            "quicktime": quicktime_representation(mapping_row),
            "ebucore": ebucore_representation(mapping_row),
        },
        REPR_KEYS,
    )


def schema_property_names(schema: dict[str, Any]) -> set[str]:
    try:
        props = schema["items"]["properties"]["photoVideoMetadataIPTC"]["properties"]
    except (KeyError, TypeError) as exc:
        raise ImportError_("JSON Schema is missing photoVideoMetadataIPTC.properties") from exc
    if not isinstance(props, dict):
        raise ImportError_("JSON Schema properties must be an object")
    names = {str(key).replace("\n", "").strip() for key in props}
    names.discard("")
    return names


def convert_member(
    *,
    record_id: str,
    name: str,
    definition: str,
    notes: str,
    html_type: str,
    json_type: str,
    xmp_id: str,
    schema_value: str,
    meta: dict[str, str],
    source: dict[str, str],
    mapping_row: list[str] | None,
    require_schema: bool,
) -> dict[str, Any]:
    datatype, struct_type, cardinality = map_datatype(json_type, html_type)
    payload: dict[str, Any] = {
        "id": record_id,
        "standard": meta["standard"],
        "standard_version": meta["version"],
        "schema": schema_value,
        "standard_property_name": name,
        "definition": definition,
        "datatype": datatype,
        "cardinality": cardinality,
        "struct_type": struct_type,
        "representations": representations(xmp_id=xmp_id, mapping_row=mapping_row),
        "mapping_notes": notes,
        "source": source,
    }
    if not require_schema:
        payload.pop("schema")
        keys = tuple(key for key in PROPERTY_KEYS if key != "schema")
        return ordered(payload, keys)
    return ordered(payload, PROPERTY_KEYS)


def convert_properties(
    prop_rows: list[list[str]],
    mappings: dict[str, list[list[str]]],
    schema_names: set[str],
    meta: dict[str, str],
    source: dict[str, str],
) -> list[dict[str, Any]]:
    properties: list[dict[str, Any]] = []
    group = ""
    json_names: list[str] = []
    for row in prop_rows[1:]:
        if row and row[0] == "Property Structures (PS)":
            break
        if len(row) == 1 and "Structure" in row[0]:
            break
        if len(row) < 10:
            continue
        if row[0]:
            group = row[0]
        name = row[1].strip()
        if not name:
            raise ImportError_("property row is missing a name")
        json_name = row[8].strip().replace("\n", "")
        json_names.append(json_name)
        mapping_row = pick_mapping_row(mappings.get(name, []), group)
        properties.append(
            convert_member(
                record_id=property_id(name),
                name=name,
                definition=row[2],
                notes=row[3],
                html_type=row[5],
                json_type=row[9],
                xmp_id=row[6],
                schema_value=map_schema(group),
                meta=meta,
                source=source,
                mapping_row=mapping_row,
                require_schema=True,
            )
        )

    if not properties:
        raise ImportError_("no VMH properties found")
    ids = [item["id"] for item in properties]
    if len(ids) != len(set(ids)):
        raise ImportError_("duplicate property ids")

    normalized_json = {name for name in json_names if name}
    missing = sorted(schema_names - normalized_json)
    extra = sorted(normalized_json - schema_names)
    if missing or extra:
        raise ImportError_(
            "HTML PVMD JSON names do not match the JSON Schema: "
            f"missing={missing[:8]} extra={extra[:8]}"
        )
    properties.sort(key=lambda item: item["id"])
    return properties


def convert_structs(
    prop_rows: list[list[str]],
    meta: dict[str, str],
    source: dict[str, str],
) -> list[dict[str, Any]]:
    structs: list[dict[str, Any]] = []
    current_name: str | None = None
    fields: list[dict[str, Any]] = []
    started = False

    def flush() -> None:
        nonlocal current_name, fields
        if current_name is None:
            return
        if not fields:
            raise ImportError_(f"struct {current_name!r} has no fields")
        structs.append(
            ordered(
                {
                    "id": struct_id(current_name),
                    "name": current_name,
                    "standard": meta["standard"],
                    "standard_version": meta["version"],
                    "source": source,
                    "fields": fields,
                },
                STRUCT_KEYS,
            )
        )
        current_name = None
        fields = []

    for row in prop_rows[1:]:
        if row and row[0] == "Property Structures (PS)":
            started = True
            continue
        if not started:
            continue
        if len(row) == 1 and row[0].endswith("Structure"):
            flush()
            current_name = re.sub(r"\s+Structure$", "", row[0]).strip()
            if current_name.startswith("PS "):
                current_name = current_name[3:].strip()
            continue
        if current_name is None or len(row) < 10:
            continue
        field_name = row[1].strip()
        if not field_name:
            raise ImportError_(f"struct {current_name} has an unnamed field")
        tr_key = row[8].strip() or field_name
        tr_key = re.sub(r"[^A-Za-z0-9]+", "", tr_key) or re.sub(
            r"[^A-Za-z0-9]+", "", field_name
        )
        fields.append(
            convert_member(
                record_id=f"{struct_id(current_name)}.{tr_key}",
                name=field_name,
                definition=row[2],
                notes=row[3],
                html_type=row[5],
                json_type=row[9],
                xmp_id=row[6],
                schema_value="",
                meta=meta,
                source=source,
                mapping_row=None,
                require_schema=False,
            )
        )
    flush()
    structs.sort(key=lambda item: item["id"])
    ids = [item["id"] for item in structs]
    if len(ids) != len(set(ids)):
        raise ImportError_("duplicate struct ids")
    return structs


def dump_json(data: dict[str, Any]) -> str:
    return json.dumps(data, indent=2, ensure_ascii=False) + "\n"


def import_registry(
    *,
    schema: dict[str, Any],
    properties_html: str,
    mapping_html: str,
    meta: dict[str, str],
) -> dict[str, Any]:
    source = source_object(meta)
    schema_names = schema_property_names(schema)
    prop_table = table_with_header(parse_tables(properties_html), PROP_HEADERS)
    map_table = table_with_header(parse_tables(mapping_html), MAP_HEADERS)
    mappings = mapping_index(map_table)
    properties = convert_properties(prop_table, mappings, schema_names, meta, source)
    structs = convert_structs(prop_table, meta, source)

    counts = {
        "Administrative": 0,
        "Descriptive": 0,
        "Rights": 0,
        "Technical": 0,
        "Time marker": 0,
    }
    for record in properties:
        schema_name = record["schema"]
        if schema_name not in counts:
            raise ImportError_(f"property schema {schema_name!r} is not counted")
        counts[schema_name] += 1
    if sum(counts.values()) != len(properties):
        raise ImportError_("property schemas do not partition VMH groups")
    if len(properties) != len(schema_names):
        raise ImportError_("imported property count does not match the JSON Schema")

    return ordered(
        {
            "standard": meta["standard"],
            "standard_version": meta["version"],
            "source": source,
            "counts": ordered(
                {
                    "properties": len(properties),
                    "administrative": counts["Administrative"],
                    "descriptive": counts["Descriptive"],
                    "rights": counts["Rights"],
                    "technical": counts["Technical"],
                    "time_marker": counts["Time marker"],
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
    schema_path = source_dir / meta["schema_filename"]
    properties_path = source_dir / meta["properties_filename"]
    mapping_path = source_dir / meta["mapping_filename"]
    for path, key in (
        (schema_path, "schema_sha256"),
        (properties_path, "properties_sha256"),
        (mapping_path, "mapping_sha256"),
    ):
        if not path.is_file():
            raise ImportError_(f"missing vendored artifact {path}")
        verify_checksum(path, meta[key])

    schema = json.loads(schema_path.read_text(encoding="utf-8"))
    if not isinstance(schema, dict):
        raise ImportError_(f"{schema_path} is not a JSON object")
    properties_html = properties_path.read_text(encoding="utf-8")
    mapping_html = mapping_path.read_text(encoding="utf-8")
    registry = import_registry(
        schema=schema,
        properties_html=properties_html,
        mapping_html=mapping_html,
        meta=meta,
    )
    write_text(output, dump_json(registry))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-dir",
        type=Path,
        default=DEFAULT_SOURCE_DIR,
        help="Directory containing SOURCE.md and vendored VMH artifacts",
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
        print(f"import_vmh: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
