#!/usr/bin/env python3
"""Offline contract tests for registry C++ code generation (session 07)."""

from __future__ import annotations

import json
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
GENERATOR = REPO_ROOT / "tools" / "registry" / "generate_cpp.py"
REGISTRY_DIR = REPO_ROOT / "registry" / "iptc-photo"
REGISTRY_JSON = REGISTRY_DIR / "iptc-photo.json"
VIDEO_REGISTRY_DIR = REPO_ROOT / "registry" / "iptc-video"
VIDEO_REGISTRY_JSON = VIDEO_REGISTRY_DIR / "iptc-video.json"
OVERLAY = REPO_ROOT / "registry" / "mappings" / "iptc-exif-overlay.json"
CROSS_MEDIA = REPO_ROOT / "registry" / "mappings" / "cross-media-accessors.json"
GENERATED_DIR = REPO_ROOT / "src" / "generated"
GENERATED_HPP = GENERATED_DIR / "property_registry.hpp"
GENERATED_CPP = GENERATED_DIR / "property_registry.cpp"
GENERATED_CROSS = GENERATED_DIR / "cross_media_accessors.hpp"
GITATTRIBUTES = REPO_ROOT / ".gitattributes"
CMAKE_REGISTRY = REPO_ROOT / "cmake" / "LibummRegistry.cmake"

XMP_STYLE_STRING = re.compile(r'"[A-Za-z][A-Za-z0-9]*:[A-Za-z][A-Za-z0-9.]*"')
KPROPERTY_COUNT = re.compile(
    r"inline constexpr std::size_t kPropertyCount = (\d+);"
)


def run_generator(
    registry_dirs: Path | list[Path],
    overlay: Path,
    output_dir: Path,
    cross_media: Path | None = None,
) -> subprocess.CompletedProcess[str]:
    if isinstance(registry_dirs, Path):
        registry_dirs = [registry_dirs]
    command = [
        "python3",
        str(GENERATOR),
        "--overlay",
        str(overlay),
        "--output-dir",
        str(output_dir),
    ]
    if cross_media is not None:
        command.extend(["--cross-media", str(cross_media)])
    for registry_dir in registry_dirs:
        command.extend(["--registry-dir", str(registry_dir)])
    return subprocess.run(
        command,
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


class TestCodegen(unittest.TestCase):
    def test_required_files_exist(self) -> None:
        for path in (
            GENERATOR,
            OVERLAY,
            CROSS_MEDIA,
            GENERATED_HPP,
            GENERATED_CPP,
            GENERATED_CROSS,
            CMAKE_REGISTRY,
        ):
            self.assertTrue(path.is_file(), f"missing {path}")

    def test_committed_generated_sources_match_generator(self) -> None:
        committed_hpp = GENERATED_HPP.read_bytes()
        committed_cpp = GENERATED_CPP.read_bytes()
        committed_cross = GENERATED_CROSS.read_bytes()
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)
            result = run_generator([REGISTRY_DIR, VIDEO_REGISTRY_DIR], OVERLAY, output)
            self.assertEqual(result.returncode, 0, result.stderr)
            generated_hpp = (output / GENERATED_HPP.name).read_bytes()
            generated_cpp = (output / GENERATED_CPP.name).read_bytes()
            generated_cross = (output / GENERATED_CROSS.name).read_bytes()
        self.assertEqual(generated_hpp, committed_hpp)
        self.assertEqual(generated_cpp, committed_cpp)
        self.assertEqual(generated_cross, committed_cross)
        self.assertTrue(committed_hpp.endswith(b"\n"))
        self.assertTrue(committed_cpp.endswith(b"\n"))
        self.assertTrue(committed_cross.endswith(b"\n"))
        self.assertNotIn(b"\r\n", committed_hpp)
        self.assertNotIn(b"\r\n", committed_cpp)
        self.assertNotIn(b"\r\n", committed_cross)
        self.assertTrue(committed_hpp.startswith(b"// GENERATED"))
        self.assertTrue(committed_cpp.startswith(b"// GENERATED"))
        self.assertTrue(committed_cross.startswith(b"// GENERATED"))

    def test_generator_is_byte_identical_across_runs(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "a"
            second = Path(tmp) / "b"
            r1 = run_generator([REGISTRY_DIR, VIDEO_REGISTRY_DIR], OVERLAY, first)
            r2 = run_generator([REGISTRY_DIR, VIDEO_REGISTRY_DIR], OVERLAY, second)
            self.assertEqual(r1.returncode, 0, r1.stderr)
            self.assertEqual(r2.returncode, 0, r2.stderr)
            self.assertEqual(
                (first / GENERATED_HPP.name).read_bytes(),
                (second / GENERATED_HPP.name).read_bytes(),
            )
            self.assertEqual(
                (first / GENERATED_CPP.name).read_bytes(),
                (second / GENERATED_CPP.name).read_bytes(),
            )
            self.assertEqual(
                (first / GENERATED_CROSS.name).read_bytes(),
                (second / GENERATED_CROSS.name).read_bytes(),
            )

    def test_property_count_matches_registry_json(self) -> None:
        photo = json.loads(REGISTRY_JSON.read_text(encoding="utf-8"))
        video = json.loads(VIDEO_REGISTRY_JSON.read_text(encoding="utf-8"))
        header = GENERATED_HPP.read_text(encoding="utf-8")
        match = KPROPERTY_COUNT.search(header)
        self.assertIsNotNone(match)
        total = len(photo["properties"]) + len(video["properties"])
        self.assertEqual(int(match.group(1)), total)
        photo_fields = sum(len(item["fields"]) for item in photo["structs"])
        video_fields = sum(len(item["fields"]) for item in video["structs"])
        self.assertEqual(
            header.count('"iptc.photo.'),
            len(photo["properties"]) + photo_fields,
        )
        self.assertEqual(
            header.count('"iptc.video.'),
            len(video["properties"]) + video_fields,
        )
        self.assertIn("iptc.video.dateCreated", header)
        self.assertIn("com.apple.quicktime.creationdate", header)
        self.assertIn("kStructFieldRepresentations", header)
        self.assertIn("kExifToolStructFieldAliases", header)
        self.assertIn("PersonName", header)
        self.assertIn("locationCreated", header)

    def test_editing_registry_changes_generated_entry(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            registry_dir = tmp_path / "iptc-photo"
            shutil.copytree(REGISTRY_DIR, registry_dir)
            registry_path = registry_dir / REGISTRY_JSON.name
            data = json.loads(registry_path.read_text(encoding="utf-8"))
            found = False
            for record in data["properties"]:
                if record["id"] == "iptc.photo.creator":
                    record["representations"]["xmp"]["property"] = "dc:editedCreator"
                    found = True
            self.assertTrue(found)
            registry_path.write_text(
                json.dumps(data, indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            output = tmp_path / "generated"
            result = run_generator(
                [registry_dir, VIDEO_REGISTRY_DIR], OVERLAY, output
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            text = (output / GENERATED_HPP.name).read_text(encoding="utf-8")
            self.assertIn('"dc:editedCreator"', text)
            self.assertNotIn('"dc:creator"', text)

    def test_src_has_no_hand_written_xmp_style_strings(self) -> None:
        src = REPO_ROOT / "src"
        generated = src / "generated"
        offenders: list[str] = []
        for path in src.rglob("*"):
            if not path.is_file():
                continue
            if generated in path.parents or path.parent == generated:
                continue
            if path.suffix.lower() not in {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}:
                continue
            text = path.read_text(encoding="utf-8")
            matches = XMP_STYLE_STRING.findall(text)
            if matches:
                rel = path.relative_to(REPO_ROOT)
                offenders.append(f"{rel}: {matches}")
        self.assertEqual(offenders, [])

    def test_gitattributes_pins_generated_sources_to_lf(self) -> None:
        text = GITATTRIBUTES.read_text(encoding="utf-8")
        lines = [
            line.strip()
            for line in text.splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        ]
        self.assertTrue(
            any("src/generated/**" in line and "eol=lf" in line for line in lines),
            ".gitattributes must pin generated C++ to LF",
        )

    def test_overlay_is_partial_stage4_subset(self) -> None:
        overlay = json.loads(OVERLAY.read_text(encoding="utf-8"))
        self.assertTrue(overlay["partial"])
        ids = [item["id"] for item in overlay["mappings"]]
        self.assertEqual(ids, sorted(ids))
        self.assertIn("iptc.photo.creator", ids)
        self.assertIn("iptc.photo.description", ids)
        self.assertIn("iptc.photo.dateCreated", ids)
        self.assertIn("iptc.photo.copyrightNotice", ids)
        self.assertTrue(any("gpsLatitude" in item for item in ids))
        gps_rows = [
            item
            for item in overlay["mappings"]
            if "struct.Location.gps" in item["id"]
        ]
        self.assertTrue(gps_rows)
        for row in gps_rows:
            self.assertEqual(row["struct_property"], "locationCreated")

    def test_overlay_conflict_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            overlay_path = Path(tmp) / "overlay.json"
            overlay = json.loads(OVERLAY.read_text(encoding="utf-8"))
            for mapping in overlay["mappings"]:
                if mapping["id"] == "iptc.photo.creator":
                    mapping["exif_tag"] = "IFD0:WrongTag"
            overlay_path.write_text(
                json.dumps(overlay, indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            result = run_generator(
                [REGISTRY_DIR, VIDEO_REGISTRY_DIR], overlay_path, Path(tmp) / "out"
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("EXIF conflict", result.stderr)

    def test_overlay_struct_field_requires_qualifier(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            overlay_path = Path(tmp) / "overlay.json"
            overlay = json.loads(OVERLAY.read_text(encoding="utf-8"))
            for mapping in overlay["mappings"]:
                if mapping["id"] == "iptc.photo.struct.Location.gpsLatitude":
                    mapping.pop("struct_property", None)
            overlay_path.write_text(
                json.dumps(overlay, indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            result = run_generator(
                [REGISTRY_DIR, VIDEO_REGISTRY_DIR], overlay_path, Path(tmp) / "out"
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("struct_property", result.stderr)

    def test_cross_media_rows_resolve_against_registries(self) -> None:
        photo = json.loads(REGISTRY_JSON.read_text(encoding="utf-8"))
        video = json.loads(VIDEO_REGISTRY_JSON.read_text(encoding="utf-8"))
        photo_ids = {record["id"]: record for record in photo["properties"]}
        video_ids = {record["id"]: record for record in video["properties"]}
        mapping = json.loads(CROSS_MEDIA.read_text(encoding="utf-8"))
        concepts = [row["concept"] for row in mapping["accessors"]]
        self.assertEqual(len(concepts), len(set(concepts)))
        self.assertIn("title", concepts)
        self.assertIn("shownEvent", concepts)
        self.assertIn("objectShown", concepts)
        header = GENERATED_CROSS.read_text(encoding="utf-8")
        self.assertIn("kCrossMediaAccessorCount", header)
        self.assertIn("iptc.photo.eventName", header)
        self.assertIn("iptc.photo.eventIdentifier", header)
        deferred = False
        for row in mapping["accessors"]:
            self.assertEqual(row["audio_ids"], [])
            self.assertIn(row["tier"], (1, 2, 3))
            for property_id in row["photo_ids"]:
                self.assertIn(property_id, photo_ids)
            for property_id in row["video_ids"]:
                self.assertIn(property_id, video_ids)
            first_photo = photo_ids[row["photo_ids"][0]]
            first_video = video_ids[row["video_ids"][0]]
            self.assertEqual(row["photo_datatype"], first_photo["datatype"])
            self.assertEqual(row["photo_cardinality"], first_photo["cardinality"])
            self.assertEqual(row["video_datatype"], first_video["datatype"])
            self.assertEqual(row["video_cardinality"], first_video["cardinality"])
            if row["concept"] == "objectShown":
                self.assertTrue(row["deferred"])
                deferred = True
            if row["concept"] == "shownEvent":
                self.assertEqual(len(row["photo_ids"]), 2)
                self.assertEqual(row["transposition"], "name_uri_to_entity")
        self.assertTrue(deferred)
        self.assertIn('"title"', header)
        self.assertEqual(header.count("CrossMediaAccessorDef kCrossMediaAccessors"), 1)

    def test_cross_media_unknown_id_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "map.json"
            mapping = json.loads(CROSS_MEDIA.read_text(encoding="utf-8"))
            mapping["accessors"][0]["photo_ids"] = ["iptc.photo.doesNotExist"]
            path.write_text(
                json.dumps(mapping, indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            result = run_generator(
                [REGISTRY_DIR, VIDEO_REGISTRY_DIR],
                OVERLAY,
                Path(tmp) / "out",
                cross_media=path,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unknown id", result.stderr)

    def test_cross_media_wrong_datatype_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "map.json"
            mapping = json.loads(CROSS_MEDIA.read_text(encoding="utf-8"))
            mapping["accessors"][0]["photo_datatype"] = "integer"
            path.write_text(
                json.dumps(mapping, indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            result = run_generator(
                [REGISTRY_DIR, VIDEO_REGISTRY_DIR],
                OVERLAY,
                Path(tmp) / "out",
                cross_media=path,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("datatype/cardinality", result.stderr)


if __name__ == "__main__":
    unittest.main()
