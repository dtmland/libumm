#!/usr/bin/env python3
"""Offline contract tests for version macros, SOVERSION, and ABI policy."""

from __future__ import annotations

import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CMAKE_LISTS = REPO_ROOT / "CMakeLists.txt"
VERSION_HPP = REPO_ROOT / "include" / "umm" / "version.hpp"
VERSION_CPP = REPO_ROOT / "src" / "version.cpp"
INSTALL_CMAKE = REPO_ROOT / "cmake" / "LibummInstall.cmake"
ABI_POLICY = REPO_ROOT / "docs" / "abi-policy.md"
README = REPO_ROOT / "README.md"
TEST_VERSION_CPP = REPO_ROOT / "tests" / "unit" / "test_version.cpp"
CONSUMER_MAIN = REPO_ROOT / "tests" / "consumer" / "main.cpp"

PROJECT_VERSION_RE = re.compile(
    r"project\(\s*libumm\s+VERSION\s+(\d+)\.(\d+)\.(\d+)\b",
    re.MULTILINE,
)
HEADER_MACRO_RE = re.compile(
    r"#define\s+UMM_VERSION_(MAJOR|MINOR|PATCH)\s+(\d+)\b"
)


def cmake_project_version() -> tuple[int, int, int]:
    text = CMAKE_LISTS.read_text(encoding="utf-8")
    match = PROJECT_VERSION_RE.search(text)
    if not match:
        raise AssertionError("CMakeLists.txt is missing project(... VERSION x.y.z)")
    return int(match.group(1)), int(match.group(2)), int(match.group(3))


def header_version() -> tuple[int, int, int]:
    text = VERSION_HPP.read_text(encoding="utf-8")
    found: dict[str, int] = {}
    for match in HEADER_MACRO_RE.finditer(text):
        found[match.group(1)] = int(match.group(2))
    missing = [part for part in ("MAJOR", "MINOR", "PATCH") if part not in found]
    if missing:
        raise AssertionError(f"version.hpp missing UMM_VERSION_* macros: {missing}")
    return found["MAJOR"], found["MINOR"], found["PATCH"]


def expected_soversion(major: int, minor: int) -> str:
    if major == 0:
        return f"{major}.{minor}"
    return str(major)


def expected_compatibility(major: int) -> str:
    return "SameMinorVersion" if major == 0 else "SameMajorVersion"


class TestVersionContract(unittest.TestCase):
    def test_header_macros_match_cmake_project_version(self) -> None:
        cmake = cmake_project_version()
        header = header_version()
        self.assertEqual(
            header,
            cmake,
            "include/umm/version.hpp macros must match project(libumm VERSION)",
        )

    def test_header_defines_version_string_from_components(self) -> None:
        text = VERSION_HPP.read_text(encoding="utf-8")
        self.assertIn("#define UMM_VERSION_STRING", text)
        self.assertIn("UMM_VERSION_STRING_STR", text)
        self.assertIn("UMM_VERSION_MAJOR", text)

    def test_runtime_version_uses_header_macro(self) -> None:
        text = VERSION_CPP.read_text(encoding="utf-8")
        self.assertIn("return UMM_VERSION_STRING;", text)
        self.assertNotIn("#ifndef UMM_VERSION", text)

    def test_cmake_asserts_header_against_project_version(self) -> None:
        text = CMAKE_LISTS.read_text(encoding="utf-8")
        self.assertIn("include/umm/version.hpp", text)
        self.assertIn("UMM_VERSION_MAJOR", text)
        self.assertIn("does not match PROJECT_VERSION", text)

    def test_soversion_follows_abi_policy(self) -> None:
        text = CMAKE_LISTS.read_text(encoding="utf-8")
        major, minor, _patch = cmake_project_version()
        self.assertIn(
            'set(UMM_SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")',
            text,
        )
        self.assertIn('set(UMM_SOVERSION "${PROJECT_VERSION_MAJOR}")', text)
        self.assertIn("PROJECT_VERSION_MAJOR EQUAL 0", text)
        self.assertIn("SOVERSION ${UMM_SOVERSION}", text)
        self.assertNotRegex(text, r"\bSOVERSION\s+0\b")
        self.assertEqual(expected_soversion(major, minor), "0.1")

    def test_package_compatibility_follows_abi_policy(self) -> None:
        cmake = CMAKE_LISTS.read_text(encoding="utf-8")
        install = INSTALL_CMAKE.read_text(encoding="utf-8")
        major, _minor, _patch = cmake_project_version()
        self.assertIn("set(UMM_PACKAGE_COMPATIBILITY SameMinorVersion)", cmake)
        self.assertIn("set(UMM_PACKAGE_COMPATIBILITY SameMajorVersion)", cmake)
        self.assertIn("COMPATIBILITY ${UMM_PACKAGE_COMPATIBILITY}", install)
        self.assertNotRegex(
            install,
            r"COMPATIBILITY\s+SameMajorVersion\b",
        )
        self.assertEqual(expected_compatibility(major), "SameMinorVersion")

    def test_abi_policy_answers_stability_questions(self) -> None:
        self.assertTrue(ABI_POLICY.is_file(), f"missing {ABI_POLICY}")
        text = ABI_POLICY.read_text(encoding="utf-8")
        lowered = text.lower()
        self.assertIn("not promised", lowered)
        self.assertIn("semver", lowered)
        self.assertIn("pre-1.0", lowered)
        self.assertIn("soversion", lowered)
        self.assertIn("c abi", lowered)
        self.assertIn("Registry::standards()", text)
        self.assertIn("M4c", text)
        self.assertIn("SameMinorVersion", text)
        self.assertIn("SameMajorVersion", text)

    def test_readme_points_at_abi_policy(self) -> None:
        text = README.read_text(encoding="utf-8")
        self.assertIn("docs/abi-policy.md", text)

    def test_unit_test_gates_on_macros(self) -> None:
        text = TEST_VERSION_CPP.read_text(encoding="utf-8")
        self.assertIn("UMM_VERSION_MAJOR", text)
        self.assertIn("UMM_VERSION_STRING", text)
        self.assertIn("UMM_CMAKE_VERSION_MAJOR", text)
        self.assertIn("static_assert", text)

    def test_consumer_can_compile_time_gate(self) -> None:
        text = CONSUMER_MAIN.read_text(encoding="utf-8")
        self.assertIn("UMM_VERSION_MAJOR", text)
        self.assertIn("UMM_VERSION_STRING", text)


if __name__ == "__main__":
    unittest.main()
