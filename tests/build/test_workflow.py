#!/usr/bin/env python3
"""Offline contract tests for .github/workflows/ci.yml (no PyYAML)."""

from __future__ import annotations

import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
WORKFLOW_PATH = REPO_ROOT / ".github" / "workflows" / "ci.yml"

PINNED_RUNNERS = ("ubuntu-24.04", "windows-2025", "macos-15")
LATEST_RUNNERS = ("ubuntu-latest", "windows-latest", "macos-latest")


def workflow_text() -> str:
    return WORKFLOW_PATH.read_text(encoding="utf-8")


def content_lines(text: str) -> list[str]:
    """Return non-empty, comment-stripped lines; reject tab indentation."""
    lines: list[str] = []
    for raw in text.splitlines():
        if "\t" in raw:
            raise AssertionError("workflow YAML must not use tab indentation")
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            continue
        # Keep inline comments only when they are full-line; values may contain '#'.
        lines.append(raw.rstrip())
    return lines


class TestWorkflow(unittest.TestCase):
    def test_workflow_file_exists(self) -> None:
        self.assertTrue(WORKFLOW_PATH.is_file(), f"missing {WORKFLOW_PATH}")

    def test_workflow_yaml_line_based_parse(self) -> None:
        text = workflow_text()
        self.assertTrue(text.strip(), "workflow is empty")
        lines = content_lines(text)
        self.assertGreaterEqual(len(lines), 8, "workflow has too few content lines")

        top_keys: list[str] = []
        for line in lines:
            if len(line) == len(line.lstrip()) and ":" in line:
                top_keys.append(line.split(":", 1)[0].strip())

        for key in ("on", "permissions", "jobs"):
            self.assertIn(key, top_keys, f"missing top-level key: {key}")

        in_block = False
        block_indent = 0
        for line in lines:
            indent = len(line) - len(line.lstrip())
            stripped = line.lstrip()
            if in_block:
                if indent > block_indent:
                    continue
                in_block = False
            if re.search(r":\s*[|>][+-]?\s*$", stripped):
                in_block = True
                block_indent = indent
                continue
            if stripped.startswith("-") or ":" in stripped:
                continue
            raise AssertionError(f"unrecognized workflow line: {line!r}")

    def test_references_pins_and_linux_packages(self) -> None:
        text = workflow_text()
        self.assertRegex(text, r"sh tools/build/pins\.sh")
        self.assertIn("tools/build/linux-packages.txt", text)
        self.assertIn("brew install ffmpeg", text)
        self.assertIn("choco install ffmpeg", text)

    def test_linux_packages_include_ffmpeg(self) -> None:
        packages = (
            (REPO_ROOT / "tools" / "build" / "linux-packages.txt")
            .read_text(encoding="utf-8")
            .splitlines()
        )
        names = [
            line.strip()
            for line in packages
            if line.strip() and not line.lstrip().startswith("#")
        ]
        self.assertIn("ffmpeg", names)

    def test_pinned_runner_labels(self) -> None:
        text = workflow_text()
        for label in PINNED_RUNNERS:
            self.assertIn(label, text)
        for label in LATEST_RUNNERS:
            self.assertNotIn(label, text)

    def test_fail_fast_false(self) -> None:
        text = workflow_text()
        self.assertRegex(text, r"fail-fast:\s*false")

    def test_permissions_block(self) -> None:
        text = workflow_text()
        self.assertRegex(text, r"(?m)^permissions:\s*$")
        self.assertRegex(text, r"(?m)^\s+contents:\s*read\s*$")

    def test_job_and_dispatch_contract(self) -> None:
        text = workflow_text()
        self.assertIn("build-and-test", text)
        self.assertIn("workflow_dispatch", text)
        self.assertIn("failing_selftest", text)
        self.assertIn("UMM_ENABLE_FAILING_SELFTEST", text)
        self.assertIn("python3 -m unittest discover -s tests/build -v", text)
        self.assertIn("-DUMM_REQUIRE_EXIFTOOL=ON", text)
        self.assertIn("-DUMM_REQUIRE_EXIV2=ON", text)
        self.assertIn("backends-acquired.txt", text)
        self.assertIn("hendrikmuhs/ccache-action", text)
        self.assertIn("tier_b", text)
        self.assertIn("UMM_TIER_B=ON", text)
        self.assertIn(".cache/corpus", text)
        self.assertIn("tests/corpus/manifest.json", text)
        self.assertRegex(text, r"hashFiles\('tests/corpus/manifest\.json'\)")
        self.assertIn("tier-b", text)
        self.assertIn(
            "cmake --preset default -DUMM_REQUIRE_EXIV2=ON -DUMM_REQUIRE_EXIFTOOL=ON ${{ inputs.failing_selftest && '-DUMM_ENABLE_FAILING_SELFTEST=ON' || '' }}",
            text,
        )
        self.assertNotIn(
            "-DUMM_TIER_B=ON ${{ inputs.failing_selftest",
            text,
        )
        self.assertIn("Install and consumer smoke", text)
        self.assertIn("consumer_install", text)


if __name__ == "__main__":
    unittest.main()
