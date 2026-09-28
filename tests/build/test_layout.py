#!/usr/bin/env python3
"""Offline layout and CMake preset contract tests."""

from __future__ import annotations

import json
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
PRESETS_PATH = REPO_ROOT / "CMakePresets.json"
PINS_SH = REPO_ROOT / "tools" / "build" / "pins.sh"
LINUX_PACKAGES = REPO_ROOT / "tools" / "build" / "linux-packages.txt"
LIBUMM_PINS_CMAKE = REPO_ROOT / "cmake" / "LibummPins.cmake"
LIBUMM_EXIFTOOL_CMAKE = REPO_ROOT / "cmake" / "LibummExifTool.cmake"
LIBUMM_EXIV2_CMAKE = REPO_ROOT / "cmake" / "LibummExiv2.cmake"
EXIFTOOL_SMOKE = REPO_ROOT / "tests" / "backend" / "test_exiftool_smoke.cmake"
EXIV2_SMOKE = REPO_ROOT / "tests" / "backend" / "test_exiv2_smoke.cpp"
PUBLIC_INCLUDE = REPO_ROOT / "include"


class TestLayout(unittest.TestCase):
    def test_required_files_exist(self) -> None:
        for path in (
            PRESETS_PATH,
            PINS_SH,
            LINUX_PACKAGES,
            LIBUMM_PINS_CMAKE,
            LIBUMM_EXIFTOOL_CMAKE,
            LIBUMM_EXIV2_CMAKE,
            EXIFTOOL_SMOKE,
            EXIV2_SMOKE,
        ):
            self.assertTrue(path.is_file(), f"missing required file: {path}")

    def test_public_headers_do_not_include_exiv2(self) -> None:
        headers = list(PUBLIC_INCLUDE.rglob("*"))
        self.assertTrue(headers, f"no files under {PUBLIC_INCLUDE}")
        for path in headers:
            if path.suffix.lower() not in {".h", ".hh", ".hpp", ".hxx"}:
                continue
            text = path.read_text(encoding="utf-8")
            self.assertNotRegex(
                text,
                r'(?m)^\s*#\s*include\s*[<"]exiv2/',
                f"public header must not include Exiv2: {path.relative_to(REPO_ROOT)}",
            )

    def test_default_presets_and_no_tests_action(self) -> None:
        data = json.loads(PRESETS_PATH.read_text(encoding="utf-8"))
        configure = {preset["name"] for preset in data["configurePresets"]}
        build = {preset["name"] for preset in data["buildPresets"]}
        test_presets = {preset["name"]: preset for preset in data["testPresets"]}
        self.assertIn("default", configure)
        self.assertIn("default", build)
        self.assertIn("default", test_presets)
        self.assertEqual(
            test_presets["default"]["execution"]["noTestsAction"],
            "error",
        )


if __name__ == "__main__":
    unittest.main()
