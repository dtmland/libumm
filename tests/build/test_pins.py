#!/usr/bin/env python3
"""Offline contract tests for tools/build/backends.env and pins.sh."""

from __future__ import annotations

import os
import re
import shutil
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
BACKENDS_ENV = REPO_ROOT / "tools" / "build" / "backends.env"
PINS_SH = REPO_ROOT / "tools" / "build" / "pins.sh"

EXPECTED_KEYS = (
    "UMM_EXIV2_VERSION",
    "UMM_EXIV2_SHA256",
    "UMM_EXIFTOOL_VERSION",
    "UMM_EXIFTOOL_SHA256",
    "UMM_STRAWBERRY_PERL_VERSION",
    "UMM_FFMPEG_VERSION",
)
SHA_KEYS = ("UMM_EXIV2_SHA256", "UMM_EXIFTOOL_SHA256")
VERSION_KEYS = (
    "UMM_EXIV2_VERSION",
    "UMM_EXIFTOOL_VERSION",
    "UMM_STRAWBERRY_PERL_VERSION",
    "UMM_FFMPEG_VERSION",
)
SHA256_RE = re.compile(r"^[0-9a-fA-F]{64}$")
KEY_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")


def parse_backends_env(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    text = path.read_text(encoding="utf-8")
    if "TODO-pin" in text:
        raise AssertionError(f"{path} still contains TODO-pin")
    for raw_line in text.splitlines():
        line = raw_line.strip().lstrip("\ufeff")
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise AssertionError(f"malformed line in {path}: {raw_line!r}")
        key, value = line.split("=", 1)
        if not KEY_RE.fullmatch(key):
            raise AssertionError(f"invalid key in {path}: {key!r}")
        if not value:
            raise AssertionError(f"empty value for {key} in {path}")
        if key in values:
            raise AssertionError(f"duplicate key in {path}: {key}")
        values[key] = value
    return values


def run_pins(env_file: Path, extra_env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    if extra_env:
        env.update(extra_env)
    return subprocess.run(
        ["sh", str(PINS_SH), str(env_file)],
        cwd=REPO_ROOT,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )


class TestPins(unittest.TestCase):
    def test_backends_env_parses_with_complete_keys(self) -> None:
        values = parse_backends_env(BACKENDS_ENV)
        missing = [key for key in EXPECTED_KEYS if key not in values]
        self.assertEqual(missing, [], f"missing keys in {BACKENDS_ENV}")

    def test_versions_non_empty(self) -> None:
        values = parse_backends_env(BACKENDS_ENV)
        for key in VERSION_KEYS:
            self.assertTrue(values[key].strip(), f"{key} must be non-empty")

    def test_sha_format_valid(self) -> None:
        values = parse_backends_env(BACKENDS_ENV)
        for key in SHA_KEYS:
            self.assertRegex(values[key], SHA256_RE, f"{key} must be 64 hex characters")

    def test_pins_sh_accepts_repo_pins(self) -> None:
        result = run_pins(BACKENDS_ENV)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_pins_sh_writes_github_env(self) -> None:
        values = parse_backends_env(BACKENDS_ENV)
        with tempfile.TemporaryDirectory() as tmp:
            github_env = Path(tmp) / "github.env"
            github_env.write_text("", encoding="utf-8")
            result = run_pins(BACKENDS_ENV, extra_env={"GITHUB_ENV": str(github_env)})
            self.assertEqual(result.returncode, 0, result.stderr)
            written = dict(
                line.split("=", 1)
                for line in github_env.read_text(encoding="utf-8").splitlines()
                if line
            )
            for key in EXPECTED_KEYS:
                self.assertEqual(written[key], values[key])

    def test_pins_sh_rejects_malformed_temp_copy(self) -> None:
        values = parse_backends_env(BACKENDS_ENV)
        values["UMM_EXIV2_SHA256"] = "not-a-sha"
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "backends.env"
            bad.write_text(
                "\n".join(f"{key}={value}" for key, value in values.items()) + "\n",
                encoding="utf-8",
            )
            result = run_pins(bad)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("UMM_EXIV2_SHA256", result.stderr)

    def test_pins_sh_rejects_missing_key(self) -> None:
        values = parse_backends_env(BACKENDS_ENV)
        with tempfile.TemporaryDirectory() as tmp:
            incomplete = Path(tmp) / "backends.env"
            lines = [
                f"{key}={value}"
                for key, value in values.items()
                if key != "UMM_EXIFTOOL_VERSION"
            ]
            incomplete.write_text("\n".join(lines) + "\n", encoding="utf-8")
            result = run_pins(incomplete)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("UMM_EXIFTOOL_VERSION", result.stderr)

    def test_pins_sh_is_executable_bit_or_runnable_via_sh(self) -> None:
        self.assertTrue(PINS_SH.is_file())
        mode = PINS_SH.stat().st_mode
        self.assertTrue(stat.S_IXUSR & mode or shutil.which("sh"), "pins.sh should be runnable")

    def test_cmake_has_no_literal_backend_pins(self) -> None:
        values = parse_backends_env(BACKENDS_ENV)
        cmake_dir = REPO_ROOT / "cmake"
        forbidden = (
            ("UMM_EXIFTOOL_VERSION", values["UMM_EXIFTOOL_VERSION"]),
            ("UMM_EXIFTOOL_SHA256", values["UMM_EXIFTOOL_SHA256"]),
            ("UMM_EXIV2_VERSION", values["UMM_EXIV2_VERSION"]),
            ("UMM_EXIV2_SHA256", values["UMM_EXIV2_SHA256"]),
        )
        for path in sorted(cmake_dir.glob("*.cmake")):
            text = path.read_text(encoding="utf-8")
            for key, pin in forbidden:
                self.assertNotIn(
                    pin,
                    text,
                    f"{path.name} must not hardcode {key}",
                )
            self.assertNotRegex(
                text,
                r"(?i)image-exiftool-\d",
                f"{path.name} must not embed a literal ExifTool archive version",
            )


if __name__ == "__main__":
    unittest.main()
