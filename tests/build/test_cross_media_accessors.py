#!/usr/bin/env python3
"""Offline contract: every non-deferred cross-media map row has a header accessor."""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
MAP = REPO_ROOT / "registry" / "mappings" / "cross-media-accessors.json"
HEADER = REPO_ROOT / "include" / "umm" / "metadata.hpp"
CMAKE = REPO_ROOT / "CMakeLists.txt"

GETTER_RE = re.compile(
    r"std::optional<PropertyValue>\s+([A-Za-z_][A-Za-z0-9_]*)\(\)\s+const;"
)


def map_concepts(*, include_deferred: bool) -> list[str]:
    data = json.loads(MAP.read_text(encoding="utf-8"))
    names: list[str] = []
    for row in data["accessors"]:
        if row.get("deferred") and not include_deferred:
            continue
        names.append(row["concept"])
    return names


def header_getters(text: str) -> set[str]:
    return set(GETTER_RE.findall(text))


def missing_accessors(concepts: list[str], getters: set[str]) -> list[str]:
    return [name for name in concepts if name not in getters]


class TestCrossMediaAccessors(unittest.TestCase):
    def test_map_and_header_exist(self) -> None:
        self.assertTrue(MAP.is_file(), f"missing {MAP}")
        self.assertTrue(HEADER.is_file(), f"missing {HEADER}")

    def test_non_deferred_map_rows_have_header_accessors(self) -> None:
        getters = header_getters(HEADER.read_text(encoding="utf-8"))
        missing = missing_accessors(map_concepts(include_deferred=False), getters)
        self.assertEqual(
            missing,
            [],
            "map row(s) lack a declared accessor in include/umm/metadata.hpp",
        )

    def test_deferred_object_shown_is_not_required(self) -> None:
        getters = header_getters(HEADER.read_text(encoding="utf-8"))
        self.assertIn("objectShown", map_concepts(include_deferred=True))
        self.assertNotIn("objectShown", getters)

    def test_mutation_detects_missing_accessor(self) -> None:
        getters = header_getters(HEADER.read_text(encoding="utf-8"))
        missing = missing_accessors(["notARealConcept"], getters)
        self.assertEqual(missing, ["notARealConcept"])

    def test_cmake_registers_matrix_test(self) -> None:
        text = CMAKE.read_text(encoding="utf-8")
        self.assertIn("test_cross_media", text)
        self.assertIn("tests/backend/test_cross_media.cpp", text)


if __name__ == "__main__":
    unittest.main()
