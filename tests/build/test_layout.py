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
LIBUMM_REGISTRY_CMAKE = REPO_ROOT / "cmake" / "LibummRegistry.cmake"
EXIFTOOL_SMOKE = REPO_ROOT / "tests" / "backend" / "test_exiftool_smoke.cmake"
EXIV2_SMOKE = REPO_ROOT / "tests" / "backend" / "test_exiv2_smoke.cpp"
EXIV2_READ = REPO_ROOT / "tests" / "backend" / "test_exiv2_read.cpp"
EXIFTOOL_READ = REPO_ROOT / "tests" / "backend" / "test_exiftool_read.cpp"
EXIFTOOL_KEYS = REPO_ROOT / "tests" / "backend" / "test_exiftool_keys.cpp"
READ_RAW_CHECKS = REPO_ROOT / "tests" / "backend" / "read_raw_checks.hpp"
EXIFTOOL_BACKEND = (
    REPO_ROOT / "src" / "backends" / "exiftool" / "exiftool_backend.cpp"
)
RECONCILE_POLICY = REPO_ROOT / "docs" / "reconciliation-policy.md"
RECONCILE_CPP = REPO_ROOT / "src" / "core" / "reconcile.cpp"
TEST_RECONCILE = REPO_ROOT / "tests" / "unit" / "test_reconcile.cpp"
TEST_READ = REPO_ROOT / "tests" / "backend" / "test_read.cpp"
ATOMIC_WRITE = REPO_ROOT / "src" / "core" / "atomic_write.cpp"
WRITE_SYNC = REPO_ROOT / "src" / "core" / "write_sync.cpp"
WRITE_CPP = REPO_ROOT / "src" / "write.cpp"
TEST_WRITE = REPO_ROOT / "tests" / "backend" / "test_write.cpp"
SIDECAR_CPP = REPO_ROOT / "src" / "core" / "sidecar.cpp"
TEST_STORAGE_POLICY = REPO_ROOT / "tests" / "unit" / "test_storage_policy.cpp"
TEST_SIDECAR = REPO_ROOT / "tests" / "backend" / "test_sidecar.cpp"
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
            LIBUMM_REGISTRY_CMAKE,
            EXIFTOOL_SMOKE,
            EXIV2_SMOKE,
            EXIV2_READ,
            EXIFTOOL_READ,
            EXIFTOOL_KEYS,
            READ_RAW_CHECKS,
            EXIFTOOL_BACKEND,
            RECONCILE_POLICY,
            RECONCILE_CPP,
            TEST_RECONCILE,
            TEST_READ,
            ATOMIC_WRITE,
            WRITE_SYNC,
            WRITE_CPP,
            TEST_WRITE,
            SIDECAR_CPP,
            TEST_STORAGE_POLICY,
            TEST_SIDECAR,
        ):
            self.assertTrue(path.is_file(), f"missing required file: {path}")

    def test_exiv2_expat_shim_exports_include_dirs(self) -> None:
        text = LIBUMM_EXIV2_CMAKE.read_text(encoding="utf-8")
        self.assertIn("EXIV2_ENABLE_XMP ON", text)
        # Exiv2 0.28 xmpsdk compiles ExpatAdapter.cpp with EXPAT_INCLUDE_DIRS.
        self.assertRegex(text, r'set\(EXPAT_INCLUDE_DIRS ')
        self.assertRegex(text, r'set\(EXPAT_INCLUDE_DIR ')

    def test_exiv2_windows_unicode_paths_use_memio(self) -> None:
        # Exiv2 0.28 FileIo::open uses fopen (ACP on Windows). Unicode fixture
        # paths must be read via ifstream + MemIo, not ImageFactory::open(utf8).
        text = (
            REPO_ROOT / "src" / "backends" / "exiv2" / "exiv2_backend.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("#if defined(_WIN32)", text)
        self.assertIn("std::ifstream", text)
        self.assertIn("MemIo", text)
        self.assertIn("ImageFactory::open(std::move(io))", text)
        self.assertIn("ImageFactory::open(path_as_utf8(media))", text)

    def test_write_path_uses_temp_and_not_overwrite_original(self) -> None:
        atomic = ATOMIC_WRITE.read_text(encoding="utf-8")
        self.assertIn("ReplaceFileW", atomic)
        self.assertIn("mutate_file_atomically", atomic)
        exiftool = EXIFTOOL_BACKEND.read_text(encoding="utf-8")
        self.assertNotIn("-overwrite_original", exiftool)
        self.assertIn("-o", exiftool)
        write_cpp = WRITE_CPP.read_text(encoding="utf-8")
        self.assertIn("mutate_file_atomically", write_cpp)
        self.assertIn("write_sync", write_cpp)

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
