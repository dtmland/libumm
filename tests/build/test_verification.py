#!/usr/bin/env python3
"""Offline contract tests for the session 28 divergence ledger."""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
LEDGER = REPO_ROOT / "tests" / "verification" / "ledger.json"
SCHEMA = REPO_ROOT / "tests" / "verification" / "schema.md"
CMAKE = REPO_ROOT / "CMakeLists.txt"
GITATTRIBUTES = REPO_ROOT / ".gitattributes"
WORKFLOW = REPO_ROOT / ".github" / "workflows" / "ci.yml"

ID_RE = re.compile(r"^[a-z0-9-]+$")
KINDS = {"representation", "capability", "one-directional"}
BACKENDS = {"", "exiv2", "exiftool"}


class TestVerificationLedger(unittest.TestCase):
    def test_files_exist(self) -> None:
        self.assertTrue(LEDGER.is_file(), f"missing {LEDGER}")
        self.assertTrue(SCHEMA.is_file(), f"missing {SCHEMA}")

    def test_json_is_lf_utf8(self) -> None:
        raw = LEDGER.read_bytes()
        self.assertFalse(raw.startswith(b"\xef\xbb\xbf"), "ledger has a BOM")
        self.assertNotIn(b"\r\n", raw, "ledger must use LF newlines")
        raw.decode("utf-8")
        self.assertTrue(raw.endswith(b"\n"), "ledger must end with a newline")

    def test_ledger_schema(self) -> None:
        data = json.loads(LEDGER.read_text(encoding="utf-8"))
        self.assertEqual(data["version"], 1)
        entries = data["entries"]
        self.assertIsInstance(entries, list)
        ids: list[str] = []
        for entry in entries:
            self.assertIsInstance(entry, dict)
            for key in (
                "id",
                "file_types",
                "category",
                "property_id",
                "write_backend",
                "read_backend",
                "kind",
                "reason",
            ):
                self.assertIn(key, entry, f"missing {key}")
            self.assertRegex(entry["id"], ID_RE)
            self.assertIsInstance(entry["file_types"], list)
            self.assertIn(entry["kind"], KINDS)
            self.assertTrue(entry["reason"].strip(), f"{entry['id']} has no reason")
            self.assertIn(entry["write_backend"], BACKENDS)
            self.assertIn(entry["read_backend"], BACKENDS)
            ids.append(entry["id"])
        self.assertEqual(len(ids), len(set(ids)), "duplicate ledger ids")

    def test_cmake_registers_default_matrix_test(self) -> None:
        text = CMAKE.read_text(encoding="utf-8")
        self.assertIn("test_cross_backend", text)
        self.assertIn("UMM_VERIFICATION_LEDGER", text)
        self.assertIn("tests/backend/test_cross_backend.cpp", text)
        self.assertIn("test_cross_media", text)
        self.assertIn("tests/backend/test_cross_media.cpp", text)

    def test_gitattributes_pins_lf(self) -> None:
        text = GITATTRIBUTES.read_text(encoding="utf-8")
        self.assertIn("tests/verification/**/*.json text eol=lf", text)

    def test_default_workflow_runs_ctest_without_tier_b(self) -> None:
        text = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("build-and-test", text)
        self.assertIn(
            "cmake --preset default -DUMM_REQUIRE_EXIV2=ON -DUMM_REQUIRE_EXIFTOOL=ON ${{ inputs.failing_selftest && '-DUMM_ENABLE_FAILING_SELFTEST=ON' || '' }}",
            text,
        )
        self.assertIn("ctest --preset default", text)


if __name__ == "__main__":
    unittest.main()
