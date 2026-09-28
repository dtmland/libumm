#!/usr/bin/env python3
"""Offline contract tests for the VMH registry importer (session 20)."""

from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
IMPORTER = REPO_ROOT / "tools" / "registry" / "import_vmh.py"
SCHEMA_MD = REPO_ROOT / "registry" / "schema.md"
SOURCE_DIR = REPO_ROOT / "registry" / "sources" / "vmh"
SOURCE_MD = SOURCE_DIR / "SOURCE.md"
REGISTRY_JSON = REPO_ROOT / "registry" / "iptc-video" / "iptc-video.json"
PHOTO_JSON = REPO_ROOT / "registry" / "iptc-photo" / "iptc-photo.json"
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
    "boolean",
    "lang-alt",
    "struct",
}
CARDINALITIES = {"one", "many"}
SCHEMAS = {
    "Administrative",
    "Descriptive",
    "Rights",
    "Technical",
    "Time marker",
}


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


class TestVmhRegistry(unittest.TestCase):
    def test_required_files_exist(self) -> None:
        for path in (IMPORTER, SCHEMA_MD, SOURCE_MD, REGISTRY_JSON, GITATTRIBUTES):
            self.assertTrue(path.is_file(), f"missing {path}")
        meta = {}
        for line in SOURCE_MD.read_text(encoding="utf-8").splitlines():
            if ":" in line and not line.startswith("#"):
                key, _, value = line.partition(":")
                if key.isidentifier() or all(c.isalnum() or c == "_" for c in key):
                    meta[key.strip()] = value.strip()
        for key in (
            "schema_filename",
            "properties_filename",
            "mapping_filename",
            "schema_sha256",
            "properties_sha256",
            "mapping_sha256",
        ):
            self.assertIn(key, meta)
            if key.endswith("_filename"):
                self.assertTrue((SOURCE_DIR / meta[key]).is_file(), meta[key])

    def test_committed_registry_matches_importer(self) -> None:
        committed = REGISTRY_JSON.read_bytes()
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "iptc-video.json"
            result = run_importer(SOURCE_DIR, output)
            self.assertEqual(result.returncode, 0, result.stderr)
            generated = output.read_bytes()
        self.assertEqual(generated, committed)
        self.assertTrue(committed.endswith(b"\n"))
        self.assertNotIn(b"\r\n", committed)

    def test_gitattributes_pins_vmh_html_to_lf(self) -> None:
        text = GITATTRIBUTES.read_text(encoding="utf-8")
        lines = [
            line.strip()
            for line in text.splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        ]
        self.assertTrue(
            any(
                "registry/sources/**/*.html" in line and "eol=lf" in line
                for line in lines
            ),
            ".gitattributes must pin VMH HTML sources to LF",
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

    def test_source_checksums_match_vendored_files(self) -> None:
        meta = {}
        for line in SOURCE_MD.read_text(encoding="utf-8").splitlines():
            if line.startswith(("schema_", "properties_", "mapping_")):
                key, _, value = line.partition(":")
                meta[key.strip()] = value.strip()
        pairs = (
            ("schema_filename", "schema_sha256"),
            ("properties_filename", "properties_sha256"),
            ("mapping_filename", "mapping_sha256"),
        )
        for filename_key, hash_key in pairs:
            path = SOURCE_DIR / meta[filename_key]
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            self.assertEqual(digest, meta[hash_key])

    def test_counts_match_json_schema(self) -> None:
        meta_filename = None
        for line in SOURCE_MD.read_text(encoding="utf-8").splitlines():
            if line.startswith("schema_filename:"):
                meta_filename = line.split(":", 1)[1].strip()
        schema = load_json(SOURCE_DIR / meta_filename)
        schema_props = schema["items"]["properties"]["photoVideoMetadataIPTC"][
            "properties"
        ]
        schema_names = {str(key).replace("\n", "").strip() for key in schema_props}
        registry = load_json(REGISTRY_JSON)
        counts = registry["counts"]
        self.assertEqual(counts["properties"], len(schema_names))
        self.assertEqual(len(registry["properties"]), counts["properties"])
        self.assertEqual(len(registry["structs"]), counts["structs"])
        self.assertGreater(counts["structs"], 0)
        self.assertEqual(
            counts["administrative"]
            + counts["descriptive"]
            + counts["rights"]
            + counts["technical"]
            + counts["time_marker"],
            counts["properties"],
        )

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
                set(representations),
                {"xmp", "iptc_iim", "exif", "quicktime", "ebucore"},
            )
            self.assertIsNone(representations["iptc_iim"])
            self.assertIsNone(representations["exif"])
            ids.append(record["id"])
            self.assertTrue(record["id"].startswith("iptc.video."))
        self.assertEqual(len(ids), len(set(ids)))
        self.assertEqual(ids, sorted(ids))

        struct_ids = [item["id"] for item in registry["structs"]]
        self.assertEqual(len(struct_ids), len(set(struct_ids)))
        for struct in registry["structs"]:
            self.assertEqual(struct["standard"], registry["standard"])
            self.assertEqual(struct["standard_version"], registry["standard_version"])
            self.assertEqual(struct["source"], source)
            self.assertTrue(struct["fields"])
            self.assertTrue(struct["id"].startswith("iptc.video.struct."))

    def test_domains_are_not_merged(self) -> None:
        photo = {item["id"] for item in load_json(PHOTO_JSON)["properties"]}
        video = {item["id"] for item in load_json(REGISTRY_JSON)["properties"]}
        self.assertTrue(all(item.startswith("iptc.photo.") for item in photo))
        self.assertTrue(all(item.startswith("iptc.video.") for item in video))
        self.assertFalse(photo & video)
        self.assertIn("iptc.photo.creator", photo)
        self.assertIn("iptc.video.creator", video)
        self.assertIn("Domain rule (photo vs video)", SCHEMA_MD.read_text(encoding="utf-8"))

    def test_spotlight_properties_keep_vmh_semantics(self) -> None:
        registry = load_json(REGISTRY_JSON)
        by_id = {item["id"]: item for item in registry["properties"]}

        date_created = by_id["iptc.video.dateCreated"]
        self.assertEqual(date_created["standard_property_name"], "Date Created")
        self.assertEqual(date_created["schema"], "Administrative")
        self.assertEqual(date_created["datatype"], "date-time")
        self.assertEqual(
            date_created["representations"]["xmp"]["property"],
            "photoshop:DateCreated",
        )
        self.assertEqual(
            date_created["representations"]["quicktime"]["key"],
            "com.apple.quicktime.creationdate",
        )
        self.assertEqual(date_created["representations"]["ebucore"]["path"], "date/created")

        description = by_id["iptc.video.description"]
        self.assertEqual(description["datatype"], "lang-alt")
        self.assertEqual(
            description["representations"]["xmp"]["property"],
            "dc:description",
        )
        self.assertEqual(
            description["representations"]["quicktime"]["key"],
            "com.apple.quicktime.description",
        )

        creator = by_id["iptc.video.creator"]
        self.assertEqual(creator["schema"], "Rights")
        self.assertEqual(creator["datatype"], "struct")
        self.assertIn("quicktime", creator["representations"])
        self.assertIn(
            "com.apple.quicktime.artist",
            creator["representations"]["quicktime"]["key"],
        )

        location_shot = by_id["iptc.video.locationShot"]
        self.assertEqual(location_shot["datatype"], "struct")
        self.assertEqual(location_shot["struct_type"], "Location")
        self.assertEqual(location_shot["cardinality"], "many")

    def test_importer_fails_on_checksum_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "sources"
            shutil.copytree(SOURCE_DIR, src)
            schema_path = src / "iptc-vmhub-1.7-schema.json"
            schema_path.write_bytes(schema_path.read_bytes() + b"\n")
            output = Path(tmp) / "out.json"
            result = run_importer(src, output)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("checksum", result.stderr)
            self.assertFalse(output.exists())

    def test_importer_fails_on_unknown_datatype(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "sources"
            shutil.copytree(SOURCE_DIR, src)
            props_path = src / "IPTC-VideoMetadataHub-props-Rec_1.7.html"
            html = props_path.read_text(encoding="utf-8")
            html = html.replace("string//", "widget//", 1)
            props_path.write_text(html, encoding="utf-8", newline="\n")
            digest = hashlib.sha256(props_path.read_bytes()).hexdigest()
            source_md = src / "SOURCE.md"
            text = source_md.read_text(encoding="utf-8")
            text = text.replace(
                "properties_sha256: 031e8bb9a769e77451dedef919db8b958fcb6152c3e61ec0f850f7a75df54f06",
                f"properties_sha256: {digest}",
            )
            source_md.write_text(text, encoding="utf-8", newline="\n")
            output = Path(tmp) / "out.json"
            result = run_importer(src, output)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("type", result.stderr.lower())
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
