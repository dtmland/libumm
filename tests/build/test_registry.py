#!/usr/bin/env python3
"""Offline contract tests for the IPTC registry importer (session 06)."""

from __future__ import annotations

import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
IMPORTER = REPO_ROOT / "tools" / "registry" / "import_iptc.py"
SCHEMA_MD = REPO_ROOT / "registry" / "schema.md"
SOURCE_DIR = REPO_ROOT / "registry" / "sources"
SOURCE_MD = SOURCE_DIR / "SOURCE.md"
REGISTRY_JSON = REPO_ROOT / "registry" / "iptc-photo" / "iptc-photo.json"
GITATTRIBUTES = REPO_ROOT / ".gitattributes"

PROPERTY_REQUIRED = (
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
DATATYPES = {
    "string",
    "uri",
    "date-time",
    "number",
    "integer",
    "lang-alt",
    "struct",
    "any",
}
CARDINALITIES = {"one", "many"}
SCHEMAS = {"Core 1.5", "Extension 1.9"}


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def run_importer(source_dir: Path, output: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            "python3",
            str(IMPORTER),
            "--source-dir",
            str(source_dir),
            "--output",
            str(output),
        ],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


class TestRegistry(unittest.TestCase):
    def test_required_files_exist(self) -> None:
        for path in (IMPORTER, SCHEMA_MD, SOURCE_MD, REGISTRY_JSON, GITATTRIBUTES):
            self.assertTrue(path.is_file(), f"missing {path}")
        filename = None
        for line in SOURCE_MD.read_text(encoding="utf-8").splitlines():
            if line.startswith("filename:"):
                filename = line.split(":", 1)[1].strip()
        self.assertIsNotNone(filename)
        self.assertTrue((SOURCE_DIR / filename).is_file())

    def test_committed_registry_matches_importer(self) -> None:
        committed = REGISTRY_JSON.read_bytes()
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "iptc-photo.json"
            result = run_importer(SOURCE_DIR, output)
            self.assertEqual(result.returncode, 0, result.stderr)
            generated = output.read_bytes()
        self.assertEqual(generated, committed)
        self.assertTrue(committed.endswith(b"\n"))
        self.assertNotIn(b"\r\n", committed)

    def test_gitattributes_pins_registry_json_to_lf(self) -> None:
        text = GITATTRIBUTES.read_text(encoding="utf-8")
        lines = [
            line.strip()
            for line in text.splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        ]
        self.assertTrue(
            any(
                "registry/**/*.json" in line and "eol=lf" in line
                for line in lines
            ),
            ".gitattributes must pin registry JSON to LF",
        )

    def test_importer_is_byte_identical_across_runs(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "a.json"
            second = Path(tmp) / "b.json"
            r1 = run_importer(SOURCE_DIR, first)
            r2 = run_importer(SOURCE_DIR, second)
            self.assertEqual(r1.returncode, 0, r1.stderr)
            self.assertEqual(r2.returncode, 0, r2.stderr)
            self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_counts_match_technical_reference(self) -> None:
        meta_filename = None
        for line in SOURCE_MD.read_text(encoding="utf-8").splitlines():
            if line.startswith("filename:"):
                meta_filename = line.split(":", 1)[1].strip()
        tr = load_json(SOURCE_DIR / meta_filename)
        top = tr["ipmd_top"]
        tr_core = sum(1 for rec in top.values() if rec["ipmdschema"] == "IptcCore")
        tr_ext = sum(1 for rec in top.values() if rec["ipmdschema"] == "IptcExt")
        registry = load_json(REGISTRY_JSON)
        counts = registry["counts"]
        self.assertEqual(counts["properties"], len(top))
        self.assertEqual(counts["core"], tr_core)
        self.assertEqual(counts["extension"], tr_ext)
        self.assertEqual(counts["core"] + counts["extension"], counts["properties"])
        self.assertEqual(len(registry["properties"]), counts["properties"])
        self.assertEqual(len(registry["structs"]), counts["structs"])
        self.assertGreater(counts["structs"], 0)

    def test_property_records_have_provenance_and_unique_ids(self) -> None:
        registry = load_json(REGISTRY_JSON)
        source = registry["source"]
        ids: list[str] = []
        for record in registry["properties"]:
            for key in PROPERTY_REQUIRED:
                self.assertIn(key, record, f"missing {key} on {record.get('id')}")
            self.assertEqual(record["standard"], registry["standard"])
            self.assertEqual(record["standard_version"], registry["standard_version"])
            self.assertEqual(record["source"], source)
            self.assertIn(record["datatype"], DATATYPES)
            self.assertIn(record["cardinality"], CARDINALITIES)
            self.assertIn(record["schema"], SCHEMAS)
            if record["datatype"] == "struct":
                self.assertIsInstance(record["struct_type"], str)
                self.assertTrue(record["struct_type"])
            else:
                self.assertIsNone(record["struct_type"])
            representations = record["representations"]
            self.assertEqual(
                set(representations), {"xmp", "iptc_iim", "exif", "exiftool"}
            )
            ids.append(record["id"])
            self.assertTrue(record["id"].startswith("iptc.photo."))
        self.assertEqual(len(ids), len(set(ids)))
        self.assertEqual(ids, sorted(ids))

        struct_ids = [item["id"] for item in registry["structs"]]
        self.assertEqual(len(struct_ids), len(set(struct_ids)))
        for struct in registry["structs"]:
            self.assertEqual(struct["standard"], registry["standard"])
            self.assertEqual(struct["standard_version"], registry["standard_version"])
            self.assertEqual(struct["source"], source)
            self.assertTrue(struct["fields"])

    def test_spotlight_properties_keep_tr_semantics(self) -> None:
        registry = load_json(REGISTRY_JSON)
        by_id = {item["id"]: item for item in registry["properties"]}
        structs = {item["name"]: item for item in registry["structs"]}

        creator = by_id["iptc.photo.creator"]
        self.assertEqual(creator["standard_property_name"], "Creator")
        self.assertEqual(creator["schema"], "Core 1.5")
        self.assertEqual(creator["datatype"], "string")
        self.assertEqual(creator["cardinality"], "many")
        self.assertEqual(creator["representations"]["xmp"]["property"], "dc:creator")
        self.assertEqual(creator["representations"]["iptc_iim"]["dataset"], "2:80")
        self.assertEqual(
            creator["representations"]["xmp"]["namespace"],
            "http://purl.org/dc/elements/1.1/",
        )

        description = by_id["iptc.photo.description"]
        self.assertEqual(description["datatype"], "lang-alt")
        self.assertEqual(description["cardinality"], "one")
        self.assertEqual(
            description["representations"]["xmp"]["property"],
            "dc:description",
        )

        date_created = by_id["iptc.photo.dateCreated"]
        self.assertEqual(date_created["datatype"], "date-time")
        self.assertEqual(
            date_created["representations"]["xmp"]["property"],
            "photoshop:DateCreated",
        )

        location_created = by_id["iptc.photo.locationCreated"]
        self.assertEqual(location_created["datatype"], "struct")
        self.assertEqual(location_created["struct_type"], "Location")
        self.assertEqual(location_created["cardinality"], "many")
        self.assertEqual(location_created["schema"], "Extension 1.9")

        location_shown = by_id["iptc.photo.locationShownInTheImage"]
        self.assertEqual(location_shown["datatype"], "struct")
        self.assertEqual(location_shown["struct_type"], "Location")
        self.assertEqual(location_shown["cardinality"], "many")

        location = structs["Location"]
        field_names = {field["id"].rsplit(".", 1)[-1] for field in location["fields"]}
        self.assertIn("city", field_names)
        self.assertIn("gpsLatitude", field_names)
        self.assertNotIn("AltLang", structs)
        person = structs["PersonWDetails"]
        name_field = next(
            field for field in person["fields"] if field["id"].endswith(".name")
        )
        self.assertEqual(name_field["et_tag"], "PersonName")
        self.assertEqual(name_field["representations"]["exiftool"]["tag"], "PersonName")

    def test_importer_fails_on_unknown_datatype(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "sources"
            shutil.copytree(SOURCE_DIR, src)
            tr_path = src / "iptc-pmd-techreference_2025.1.json"
            tr = load_json(tr_path)
            tr["ipmd_top"]["creatorNames"]["datatype"] = "widget"
            tr_path.write_text(json.dumps(tr), encoding="utf-8")
            output = Path(tmp) / "out.json"
            result = run_importer(src, output)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("datatype", result.stderr)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
