#!/usr/bin/env python3
"""Offline contract tests for session 31 notices and corresponding source."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_pins import parse_backends_env  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[2]
GITATTRIBUTES = REPO_ROOT / ".gitattributes"
NOTICES = REPO_ROOT / "THIRD-PARTY-NOTICES.md"
NOTICE = REPO_ROOT / "NOTICE.md"
LICENSE = REPO_ROOT / "LICENSE"
MANIFEST = REPO_ROOT / "tools" / "build" / "corresponding-source.json"
GENERATOR = REPO_ROOT / "tools" / "build" / "generate_corresponding_source.py"
BACKENDS_ENV = REPO_ROOT / "tools" / "build" / "backends.env"
EXIV2_CMAKE = REPO_ROOT / "cmake" / "LibummExiv2.cmake"
INSTALL_CMAKE = REPO_ROOT / "cmake" / "LibummInstall.cmake"
CONSUMER_INSTALL = REPO_ROOT / "tests" / "build" / "test_consumer_install.cmake"
LICENSES_DIR = REPO_ROOT / "licenses"
LICENSE_FILES = {
    "GPL-2.0.txt": "GNU GENERAL PUBLIC LICENSE",
    "GPL-3.0.txt": "GNU GENERAL PUBLIC LICENSE",
    "Expat.txt": "Permission is hereby granted, free of charge",
    "Zlib.txt": "Jean-loup Gailly",
}
REQUIRED_COMPONENTS = ("exiv2", "expat", "zlib")


def load_manifest() -> dict:
    return json.loads(MANIFEST.read_text(encoding="utf-8"))


class TestNotices(unittest.TestCase):
    def test_required_files_exist(self) -> None:
        for path in (
            NOTICES,
            NOTICE,
            LICENSE,
            MANIFEST,
            GENERATOR,
            LICENSES_DIR,
        ):
            self.assertTrue(path.exists(), f"missing {path}")
        for name in LICENSE_FILES:
            self.assertTrue((LICENSES_DIR / name).is_file(), f"missing licenses/{name}")

    def test_committed_manifest_matches_generator(self) -> None:
        committed = MANIFEST.read_bytes()
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "corresponding-source.json"
            result = subprocess.run(
                [
                    "python3",
                    str(GENERATOR),
                    "--env",
                    str(BACKENDS_ENV),
                    "--exiv2-cmake",
                    str(EXIV2_CMAKE),
                    "--output",
                    str(output),
                ],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            generated = output.read_bytes()
        self.assertEqual(generated, committed)
        self.assertTrue(committed.endswith(b"\n"))
        self.assertNotIn(b"\r", committed)

    def test_manifest_checksums_match_pins(self) -> None:
        pins = parse_backends_env(BACKENDS_ENV)
        cmake = EXIV2_CMAKE.read_text(encoding="utf-8")
        manifest = load_manifest()
        by_id = {item["id"]: item for item in manifest["components"]}
        self.assertEqual(set(by_id), set(REQUIRED_COMPONENTS))

        exiv2 = by_id["exiv2"]
        self.assertEqual(exiv2["version"], pins["UMM_EXIV2_VERSION"])
        self.assertEqual(exiv2["sha256"], pins["UMM_EXIV2_SHA256"].lower())
        self.assertEqual(
            exiv2["url"],
            f"https://github.com/Exiv2/exiv2/archive/refs/tags/v{pins['UMM_EXIV2_VERSION']}.tar.gz",
        )
        self.assertTrue(exiv2["statically_absorbed"])

        for dep_id, needle in (
            ("expat", "umm_expat"),
            ("zlib", "umm_zlib"),
        ):
            item = by_id[dep_id]
            self.assertIn(item["url"], cmake)
            self.assertIn(item["sha256"], cmake.lower())
            self.assertIn(needle, cmake)
            self.assertTrue(item["statically_absorbed"])
            self.assertIn(item["version"], item["url"])

    def test_notices_mention_pinned_components_and_versions(self) -> None:
        text = NOTICES.read_text(encoding="utf-8")
        pins = parse_backends_env(BACKENDS_ENV)
        manifest = load_manifest()
        self.assertIn("GPL-3.0", text)
        self.assertIn("Apache License, Version 2.0", text)
        self.assertIn("never", text.lower())
        self.assertIn("bundled", text.lower())
        self.assertIn("S1a", text)
        self.assertIn("S1c", text)
        self.assertIn(pins["UMM_EXIV2_VERSION"], text)
        for item in manifest["components"]:
            self.assertIn(item["name"], text)
            self.assertIn(item["version"], text)
            self.assertIn(item["license"], text)
            for license_file in item["license_files"]:
                self.assertIn(license_file, text)

    def test_notice_points_at_new_artifacts(self) -> None:
        text = NOTICE.read_text(encoding="utf-8")
        self.assertIn("THIRD-PARTY-NOTICES.md", text)
        self.assertIn("licenses/", text)
        self.assertIn("corresponding-source.json", text)
        self.assertIn("GPL-3.0", text)
        self.assertIn("does not change this analysis", text)

    def test_license_texts_are_non_empty_and_referenced(self) -> None:
        notices = NOTICES.read_text(encoding="utf-8")
        for name, marker in LICENSE_FILES.items():
            path = LICENSES_DIR / name
            text = path.read_text(encoding="utf-8")
            self.assertGreater(len(text.strip()), 200, f"{name} looks empty or abridged")
            self.assertIn(marker, text)
            self.assertIn(f"licenses/{name}", notices)
            self.assertTrue(text.endswith("\n"))
            self.assertNotIn("\r", text)

        gpl2 = (LICENSES_DIR / "GPL-2.0.txt").read_text(encoding="utf-8")
        gpl3 = (LICENSES_DIR / "GPL-3.0.txt").read_text(encoding="utf-8")
        self.assertIn("Version 2, June 1991", gpl2)
        self.assertIn("Version 3, 29 June 2007", gpl3)
        self.assertNotIn("[TODO", gpl2)
        self.assertNotIn("[TODO", gpl3)

    def test_gitattributes_pins_notices_to_lf(self) -> None:
        text = GITATTRIBUTES.read_text(encoding="utf-8")
        self.assertIn("tools/build/corresponding-source.json text eol=lf", text)
        self.assertIn("licenses/**/*.txt text eol=lf", text)

    def test_exiftool_is_not_in_corresponding_source(self) -> None:
        manifest = load_manifest()
        ids = [item["id"] for item in manifest["components"]]
        self.assertNotIn("exiftool", ids)
        notices = NOTICES.read_text(encoding="utf-8")
        self.assertIn("ExifTool", notices)
        self.assertIn("out-of-process", notices.lower())

    def test_install_rules_ship_notices_and_licenses(self) -> None:
        install = INSTALL_CMAKE.read_text(encoding="utf-8")
        self.assertIn("THIRD-PARTY-NOTICES.md", install)
        self.assertIn("NOTICE.md", install)
        self.assertIn("LICENSE", install)
        self.assertIn("corresponding-source.json", install)
        self.assertIn("CMAKE_INSTALL_DOCDIR", install)
        self.assertIn("licenses/", install)
        consumer = CONSUMER_INSTALL.read_text(encoding="utf-8")
        self.assertIn("THIRD-PARTY-NOTICES.md", consumer)
        self.assertIn("GPL-3.0.txt", consumer)


if __name__ == "__main__":
    unittest.main()
