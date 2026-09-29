#!/usr/bin/env python3
"""Offline contract tests for capability data and supported-types generation."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
GENERATOR = REPO_ROOT / "tools" / "registry" / "generate_supported_types.py"
CAP_DIR = REPO_ROOT / "registry" / "capabilities"
EXIV2_JSON = CAP_DIR / "exiv2.json"
EXIFTOOL_JSON = CAP_DIR / "exiftool.json"
POLICY_JSON = CAP_DIR / "policy.json"
SCHEMA = CAP_DIR / "schema.md"
SOURCE = CAP_DIR / "SOURCE.md"
MARKDOWN = REPO_ROOT / "docs" / "supported-types.md"
GENERATED_HPP = REPO_ROOT / "src" / "generated" / "capabilities_data.hpp"
GITATTRIBUTES = REPO_ROOT / ".gitattributes"
CMAKE_REGISTRY = REPO_ROOT / "cmake" / "LibummRegistry.cmake"
LIBUMM_EXIV2 = REPO_ROOT / "cmake" / "LibummExiv2.cmake"


def run_generator(
    cap_dir: Path, markdown: Path, hpp: Path
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            "python3",
            str(GENERATOR),
            "--capabilities-dir",
            str(cap_dir),
            "--markdown",
            str(markdown),
            "--output-hpp",
            str(hpp),
        ],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


class TestCapabilitiesCodegen(unittest.TestCase):
    def test_required_files_exist(self) -> None:
        for path in (
            GENERATOR,
            EXIV2_JSON,
            EXIFTOOL_JSON,
            POLICY_JSON,
            SCHEMA,
            SOURCE,
            MARKDOWN,
            GENERATED_HPP,
            CMAKE_REGISTRY,
        ):
            self.assertTrue(path.is_file(), f"missing {path}")

    def test_committed_generated_files_match_generator(self) -> None:
        committed_md = MARKDOWN.read_bytes()
        committed_hpp = GENERATED_HPP.read_bytes()
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)
            markdown = output / "supported-types.md"
            hpp = output / "capabilities_data.hpp"
            result = run_generator(CAP_DIR, markdown, hpp)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(markdown.read_bytes(), committed_md)
            self.assertEqual(hpp.read_bytes(), committed_hpp)
        self.assertTrue(committed_md.endswith(b"\n"))
        self.assertTrue(committed_hpp.endswith(b"\n"))
        self.assertNotIn(b"\r\n", committed_md)
        self.assertNotIn(b"\r\n", committed_hpp)
        self.assertTrue(committed_md.startswith(b"# Backend file-type coverage"))
        self.assertIn(b"GENERATED", committed_md.splitlines()[2])
        self.assertTrue(committed_hpp.startswith(b"// GENERATED"))

    def test_generator_is_byte_identical_across_runs(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "a"
            second = Path(tmp) / "b"
            first.mkdir()
            second.mkdir()
            r1 = run_generator(CAP_DIR, first / "supported-types.md", first / "cap.hpp")
            r2 = run_generator(CAP_DIR, second / "supported-types.md", second / "cap.hpp")
            self.assertEqual(r1.returncode, 0, r1.stderr)
            self.assertEqual(r2.returncode, 0, r2.stderr)
            self.assertEqual(
                (first / "supported-types.md").read_bytes(),
                (second / "supported-types.md").read_bytes(),
            )
            self.assertEqual(
                (first / "cap.hpp").read_bytes(),
                (second / "cap.hpp").read_bytes(),
            )

    def test_editing_capability_data_changes_generated_markdown(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            cap_dir = Path(tmp) / "capabilities"
            cap_dir.mkdir()
            for name in ("exiv2.json", "exiftool.json", "policy.json"):
                (cap_dir / name).write_bytes((CAP_DIR / name).read_bytes())
            data = json.loads((cap_dir / "exiv2.json").read_text(encoding="utf-8"))
            found = False
            for row in data["types"]:
                if row["type"] == "JPEG":
                    row["notes"] = "edited-capability-note"
                    found = True
            self.assertTrue(found)
            (cap_dir / "exiv2.json").write_text(
                json.dumps(data, indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            result = run_generator(
                cap_dir, Path(tmp) / "supported-types.md", Path(tmp) / "cap.hpp"
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(
                "edited-capability-note",
                (Path(tmp) / "cap.hpp").read_text(encoding="utf-8"),
            )

    def test_jpeg_and_xmp_are_fixture_types(self) -> None:
        exiv2 = json.loads(EXIV2_JSON.read_text(encoding="utf-8"))
        types = {row["type"] for row in exiv2["types"]}
        self.assertIn("JPEG", types)
        self.assertIn("XMP", types)
        jpeg = next(row for row in exiv2["types"] if row["type"] == "JPEG")
        self.assertEqual(jpeg["categories"]["exif"], "read_write")
        self.assertEqual(jpeg["location"]["gps_exif"], "read_write")
        self.assertEqual(jpeg["location"]["named_place"], "read_write")
        xmp = next(row for row in exiv2["types"] if row["type"] == "XMP")
        self.assertEqual(xmp["location"]["gps_exif"], "none")
        self.assertEqual(xmp["categories"]["xmp"], "read_write")

    def test_exiftool_extra_exotic_group_is_partial(self) -> None:
        data = json.loads(EXIFTOOL_JSON.read_text(encoding="utf-8"))
        self.assertEqual(data["coverage"], "partial")
        exotic = next(group for group in data["extra_groups"] if group["id"] == "exotic")
        self.assertEqual(exotic["coverage"], "partial")
        self.assertIn("GPS", data["meta_formats"]["read_write_create"])
        self.assertIn("GeoTIFF", data["meta_formats"]["read_write_create"])

    def test_gitattributes_pins_capability_json_to_lf(self) -> None:
        text = GITATTRIBUTES.read_text(encoding="utf-8")
        lines = [
            line.strip()
            for line in text.splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        ]
        self.assertTrue(
            any("registry/**/*.json" in line and "eol=lf" in line for line in lines),
            ".gitattributes must pin capability JSON to LF",
        )
        self.assertTrue(
            any("supported-types.md" in line and "eol=lf" in line for line in lines),
            ".gitattributes must pin generated supported-types.md to LF",
        )

    def test_exiv2_bmff_enabled_in_cmake(self) -> None:
        text = LIBUMM_EXIV2.read_text(encoding="utf-8")
        self.assertIn("EXIV2_ENABLE_BMFF ON", text)

    def test_exiftool_listwf_contains_jpeg_and_xmp(self) -> None:
        script = os.environ.get("UMM_EXIFTOOL_SCRIPT")
        perl = os.environ.get("UMM_PERL_EXECUTABLE")
        if not script or not perl:
            self.skipTest("ExifTool not configured")
        result = subprocess.run(
            [perl, script, "-listwf"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        text = result.stdout.upper()
        self.assertRegex(text, r"\bJPEG\b")
        self.assertRegex(text, r"\bXMP\b")


if __name__ == "__main__":
    unittest.main()
