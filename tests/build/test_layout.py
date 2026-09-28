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


class TestLayout(unittest.TestCase):
    def test_required_files_exist(self) -> None:
        for path in (PRESETS_PATH, PINS_SH, LINUX_PACKAGES):
            self.assertTrue(path.is_file(), f"missing required file: {path}")

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
