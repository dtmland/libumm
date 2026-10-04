#!/usr/bin/env python3
"""Offline layout and CMake preset contract tests."""

from __future__ import annotations

import json
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
PRESETS_PATH = REPO_ROOT / "CMakePresets.json"
PINS_SH = REPO_ROOT / "tools" / "build" / "pins.sh"
GET_EXIFTOOL_SH = REPO_ROOT / "tools" / "get-exiftool" / "install.sh"
GET_EXIFTOOL_PS1 = REPO_ROOT / "tools" / "get-exiftool" / "install.ps1"
LINUX_PACKAGES = REPO_ROOT / "tools" / "build" / "linux-packages.txt"
LIBUMM_PINS_CMAKE = REPO_ROOT / "cmake" / "LibummPins.cmake"
LIBUMM_EXIFTOOL_CMAKE = REPO_ROOT / "cmake" / "LibummExifTool.cmake"
LIBUMM_EXIV2_CMAKE = REPO_ROOT / "cmake" / "LibummExiv2.cmake"
LIBUMM_REGISTRY_CMAKE = REPO_ROOT / "cmake" / "LibummRegistry.cmake"
LIBUMM_CORPUS_CMAKE = REPO_ROOT / "cmake" / "LibummCorpus.cmake"
LIBUMM_INSTALL_CMAKE = REPO_ROOT / "cmake" / "LibummInstall.cmake"
RELEASE_WORKFLOW = REPO_ROOT / ".github" / "workflows" / "release.yml"
PACKAGE_RELEASE = REPO_ROOT / "tools" / "build" / "package_release.py"
FETCH_CORRESPONDING_SOURCE = (
    REPO_ROOT / "tools" / "build" / "fetch_corresponding_source.py"
)
GENERATE_RELEASE_NOTES = REPO_ROOT / "tools" / "build" / "generate_release_notes.py"
RELEASE_CHECKLIST = REPO_ROOT / "docs" / "release-checklist.md"
TEST_RELEASE = REPO_ROOT / "tests" / "build" / "test_release.py"
TEST_TRACK_MATCH = REPO_ROOT / "tests" / "backend" / "test_track_match.cpp"
THIRD_PARTY_NOTICES = REPO_ROOT / "THIRD-PARTY-NOTICES.md"
NOTICE_MD = REPO_ROOT / "NOTICE.md"
CORRESPONDING_SOURCE = (
    REPO_ROOT / "tools" / "build" / "corresponding-source.json"
)
CORRESPONDING_SOURCE_GENERATOR = (
    REPO_ROOT / "tools" / "build" / "generate_corresponding_source.py"
)
LICENSES_DIR = REPO_ROOT / "licenses"
ABI_POLICY = REPO_ROOT / "docs" / "abi-policy.md"
VERSION_HPP = REPO_ROOT / "include" / "umm" / "version.hpp"
UMM_CONFIG_IN = REPO_ROOT / "cmake" / "ummConfig.cmake.in"
CONSUMER_CMAKE = REPO_ROOT / "tests" / "consumer" / "CMakeLists.txt"
CONSUMER_MAIN = REPO_ROOT / "tests" / "consumer" / "main.cpp"
CONSUMER_INSTALL_TEST = REPO_ROOT / "tests" / "build" / "test_consumer_install.cmake"
CORPUS_FETCHER = REPO_ROOT / "tools" / "corpus" / "fetch.py"
CORPUS_MANIFEST = REPO_ROOT / "tests" / "corpus" / "manifest.json"
CORPUS_SCHEMA = REPO_ROOT / "tests" / "corpus" / "schema.md"
VERIFICATION_LEDGER = REPO_ROOT / "tests" / "verification" / "ledger.json"
VERIFICATION_SCHEMA = REPO_ROOT / "tests" / "verification" / "schema.md"
TEST_CROSS_BACKEND = REPO_ROOT / "tests" / "backend" / "test_cross_backend.cpp"
CROSS_BACKEND_HPP = REPO_ROOT / "tests" / "backend" / "cross_backend.hpp"
EXIFTOOL_SMOKE = REPO_ROOT / "tests" / "backend" / "test_exiftool_smoke.cmake"
EXIV2_SMOKE = REPO_ROOT / "tests" / "backend" / "test_exiv2_smoke.cpp"
EXIV2_READ = REPO_ROOT / "tests" / "backend" / "test_exiv2_read.cpp"
EXIFTOOL_READ = REPO_ROOT / "tests" / "backend" / "test_exiftool_read.cpp"
EXIFTOOL_KEYS = REPO_ROOT / "tests" / "backend" / "test_exiftool_keys.cpp"
READ_BASE_CHECKS = REPO_ROOT / "tests" / "backend" / "read_base_checks.hpp"
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
TEST_CAPABILITIES = REPO_ROOT / "tests" / "unit" / "test_capabilities.cpp"
TEST_CAPABILITIES_PROBE = (
    REPO_ROOT / "tests" / "backend" / "test_capabilities_probe.cpp"
)
CAPABILITIES_GENERATOR = (
    REPO_ROOT / "tools" / "registry" / "generate_supported_types.py"
)
PROPERTY_REF_GENERATOR = (
    REPO_ROOT / "tools" / "registry" / "generate_property_reference.py"
)
CAPABILITIES_DIR = REPO_ROOT / "registry" / "capabilities"
PUBLIC_INCLUDE = REPO_ROOT / "include"


class TestLayout(unittest.TestCase):
    def test_required_files_exist(self) -> None:
        for path in (
            PRESETS_PATH,
            PINS_SH,
            GET_EXIFTOOL_SH,
            GET_EXIFTOOL_PS1,
            LINUX_PACKAGES,
            LIBUMM_PINS_CMAKE,
            LIBUMM_EXIFTOOL_CMAKE,
            LIBUMM_EXIV2_CMAKE,
            LIBUMM_REGISTRY_CMAKE,
            LIBUMM_CORPUS_CMAKE,
            LIBUMM_INSTALL_CMAKE,
            RELEASE_WORKFLOW,
            PACKAGE_RELEASE,
            FETCH_CORRESPONDING_SOURCE,
            GENERATE_RELEASE_NOTES,
            RELEASE_CHECKLIST,
            TEST_RELEASE,
            TEST_TRACK_MATCH,
            THIRD_PARTY_NOTICES,
            NOTICE_MD,
            CORRESPONDING_SOURCE,
            CORRESPONDING_SOURCE_GENERATOR,
            LICENSES_DIR / "GPL-2.0.txt",
            LICENSES_DIR / "GPL-3.0.txt",
            LICENSES_DIR / "Expat.txt",
            LICENSES_DIR / "Zlib.txt",
            ABI_POLICY,
            VERSION_HPP,
            UMM_CONFIG_IN,
            CONSUMER_CMAKE,
            CONSUMER_MAIN,
            CONSUMER_INSTALL_TEST,
            CORPUS_FETCHER,
            CORPUS_MANIFEST,
            CORPUS_SCHEMA,
            VERIFICATION_LEDGER,
            VERIFICATION_SCHEMA,
            TEST_CROSS_BACKEND,
            CROSS_BACKEND_HPP,
            EXIFTOOL_SMOKE,
            EXIV2_SMOKE,
            EXIV2_READ,
            EXIFTOOL_READ,
            EXIFTOOL_KEYS,
            READ_BASE_CHECKS,
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
            TEST_CAPABILITIES,
            TEST_CAPABILITIES_PROBE,
            CAPABILITIES_GENERATOR,
            PROPERTY_REF_GENERATOR,
            CAPABILITIES_DIR / "schema.md",
            CAPABILITIES_DIR / "exiv2.json",
            CAPABILITIES_DIR / "exiftool.json",
            CAPABILITIES_DIR / "policy.json",
        ):
            self.assertTrue(path.is_file(), f"missing required file: {path}")

    def test_exiv2_expat_shim_exports_include_dirs(self) -> None:
        text = LIBUMM_EXIV2_CMAKE.read_text(encoding="utf-8")
        self.assertIn("EXIV2_ENABLE_XMP ON", text)
        self.assertIn("EXIV2_ENABLE_PNG ON", text)
        # Exiv2 0.28 xmpsdk compiles ExpatAdapter.cpp with EXPAT_INCLUDE_DIRS.
        self.assertRegex(text, r'set\(EXPAT_INCLUDE_DIRS ')
        self.assertRegex(text, r'set\(EXPAT_INCLUDE_DIR ')

    def test_install_export_uses_imported_private_archives(self) -> None:
        # Session 29: static install ships Exiv2 archives as IMPORTED deps of
        # umm::umm. FetchContent targets stay out of the export set.
        install = LIBUMM_INSTALL_CMAKE.read_text(encoding="utf-8")
        config_in = UMM_CONFIG_IN.read_text(encoding="utf-8")
        exiv2 = LIBUMM_EXIV2_CMAKE.read_text(encoding="utf-8")
        consumer = CONSUMER_CMAKE.read_text(encoding="utf-8")
        self.assertIn("IMPORTED STATIC", install)
        self.assertIn("COMPATIBILITY ${UMM_PACKAGE_COMPATIBILITY}", install)
        self.assertIn("CMAKE_INSTALL_LIBDIR}/umm", install)
        self.assertIn("$<BUILD_INTERFACE:exiv2lib>", exiv2)
        self.assertNotIn("find_package(exiv2", config_in)
        self.assertIn("find_dependency(exiv2 CONFIG)", config_in)
        self.assertIn("_umm_need_exiv2", config_in)
        self.assertIn("UMM_PRIVATE_EXIV2_KIND", config_in)
        self.assertIn("SHARED IMPORTED", config_in)
        self.assertIn("does not locate Exiv2 as a CMake package", install)
        self.assertIn("find_package(umm 0.1 CONFIG REQUIRED)", consumer)
        self.assertIn("umm::umm", consumer)
        self.assertIn("CMAKE_SKIP_INSTALL_RULES", exiv2)
        self.assertIn("find_dependency(Iconv)", config_in)
        self.assertIn("NOT TARGET Iconv::Iconv", config_in)
        self.assertIn("IMPORTED_GLOBAL", config_in)
        self.assertIn('OR _umm_item STREQUAL "iconv"', install)
        self.assertIn("APPLE AND UMM_INSTALL_BUNDLE_EXIV2", install)
        self.assertIn("$<BUILD_INTERFACE:${_umm_iface_item}>", install)
        self.assertIn("UMM_EXIV2_SHARED", install)
        self.assertIn("UMM_INSTALL_SHARED_EXIV2", install)
        self.assertIn("UMM_INSTALL_FIND_EXIV2", install)

    def test_exiv2_shared_option_prefers_system_then_fetchcontent(self) -> None:
        # Session 32 / P2: shared Exiv2 is packaging/substitutability, not a
        # GPL escape. Default OFF must keep FetchContent static.
        exiv2 = LIBUMM_EXIV2_CMAKE.read_text(encoding="utf-8")
        readme = (REPO_ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("option(UMM_EXIV2_SHARED", exiv2)
        self.assertIn("GPL still governs combined-work distribution", exiv2)
        self.assertIn("find_package(exiv2 CONFIG QUIET)", exiv2)
        self.assertIn("UMM_EXIV2_MIN_VERSION", exiv2)
        self.assertIn("if(UMM_EXIV2_SHARED)", exiv2)
        self.assertIn("set(BUILD_SHARED_LIBS ON)", exiv2)
        self.assertIn("set(BUILD_SHARED_LIBS OFF)", exiv2)
        self.assertIn("CMAKE_SKIP_INSTALL_RULES", exiv2)
        self.assertIn("$<BUILD_INTERFACE:exiv2lib>", exiv2)
        self.assertIn("UMM_EXIV2_SHARED", readme)
        self.assertIn("GPL", readme)
        self.assertIn("pinned", readme.lower())

    def test_exiv2_fetched_deps_skip_install_rules(self) -> None:
        # zlib 1.3.x install(TARGETS) has no EXPORT. Putting zlibstatic in
        # exiv2Targets then fails generate: INTERFACE_INCLUDE_DIRECTORIES
        # is prefixed in the source/build directory. Skip install rules for
        # FetchContent deps instead; libumm links exiv2lib privately.
        text = LIBUMM_EXIV2_CMAKE.read_text(encoding="utf-8")
        self.assertIn("CMAKE_SKIP_INSTALL_RULES", text)
        self.assertNotIn(
            "install(TARGETS zlibstatic EXPORT exiv2Targets)", text
        )
        self.assertIn("Zlib not found on system; fetched for Exiv2 PNG", text)
        # Parent cmake_install.cmake still includes FetchContent subdir
        # install scripts that skip never wrote; stub them.
        self.assertIn("umm_stub_skipped_install_script", text)
        self.assertIn("cmake_install.cmake", text)

    def test_exiv2_zlib_shim_includes_generated_zconf(self) -> None:
        # zlib CMake generates zconf.h in BINARY_DIR. Exiv2 0.28 compiles
        # pngchunk_int.cpp with ZLIB_INCLUDE_DIR only, so the shim must list
        # both the source tree (zlib.h) and the build tree (zconf.h).
        text = LIBUMM_EXIV2_CMAKE.read_text(encoding="utf-8")
        self.assertRegex(
            text,
            r'set\(ZLIB_INCLUDE_DIR "\$\{_umm_zlib_include_dir\}" '
            r'"\$\{_umm_zlib_binary_dir\}"\)',
        )
        self.assertRegex(
            text,
            r'set\(ZLIB_INCLUDE_DIRS "\$\{_umm_zlib_include_dir\}" '
            r'"\$\{_umm_zlib_binary_dir\}"\)',
        )

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
