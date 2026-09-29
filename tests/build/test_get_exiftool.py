#!/usr/bin/env python3
"""Offline contract tests for tools/get-exiftool native scripts (session 33)."""

from __future__ import annotations

import hashlib
import os
import stat
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
BACKENDS_ENV = REPO_ROOT / "tools" / "build" / "backends.env"
INSTALL_SH = REPO_ROOT / "tools" / "get-exiftool" / "install.sh"
INSTALL_PS1 = REPO_ROOT / "tools" / "get-exiftool" / "install.ps1"
README = REPO_ROOT / "README.md"
BACKEND_HPP = REPO_ROOT / "include" / "umm" / "backend.hpp"

PIN_KEYS = (
    "UMM_EXIFTOOL_VERSION=",
    "UMM_EXIFTOOL_SHA256=",
    "UMM_STRAWBERRY_PERL_VERSION=",
)


def parse_backends_env(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip().lstrip("\ufeff")
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value
    return values


def host_is_windows() -> bool:
    return os.name == "nt"


def install_script() -> Path:
    return INSTALL_PS1 if host_is_windows() else INSTALL_SH


def native_args(*, prefix: Path | None = None, pin_file: Path | None = None,
                cache_dir: Path | None = None, check: bool = False) -> list[str]:
    args: list[str] = []
    windows = host_is_windows()
    if prefix is not None:
        args.extend(["-Prefix" if windows else "--prefix", str(prefix)])
    if pin_file is not None:
        args.extend(["-PinFile" if windows else "--pin-file", str(pin_file)])
    if cache_dir is not None:
        args.extend(["-CacheDir" if windows else "--cache-dir", str(cache_dir)])
    if check:
        args.append("-Check" if windows else "--check")
    return args


def run_install(args: list[str], extra_env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    if extra_env:
        env.update(extra_env)
    script = install_script()
    if host_is_windows():
        cmd = [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(script),
            *args,
        ]
    else:
        cmd = ["sh", str(script), *args]
    return subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )


def write_pin(path: Path, *, version: str, sha256: str, url: str,
              strawberry: str = "5.42.3.1") -> None:
    path.write_text(
        "\n".join(
            [
                f"# ExifTool source: {url}",
                f"UMM_EXIFTOOL_VERSION={version}",
                f"UMM_EXIFTOOL_SHA256={sha256}",
                f"UMM_STRAWBERRY_PERL_VERSION={strawberry}",
                "",
            ]
        ),
        encoding="utf-8",
    )


def make_archive(dest: Path, version: str, payload: bytes = b"#!/usr/bin/env perl\n# fake\n") -> str:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / f"exiftool-{version}"
        root.mkdir()
        (root / "exiftool").write_bytes(payload)
        lib = root / "lib"
        lib.mkdir()
        (lib / "placeholder").write_text("ok\n", encoding="utf-8")
        with tarfile.open(dest, "w:gz") as tar:
            tar.add(root, arcname=f"exiftool-{version}")
    return hashlib.sha256(dest.read_bytes()).hexdigest()


class TestGetExiftool(unittest.TestCase):
    def test_scripts_exist(self) -> None:
        self.assertTrue(INSTALL_SH.is_file(), f"missing {INSTALL_SH}")
        self.assertTrue(INSTALL_PS1.is_file(), f"missing {INSTALL_PS1}")
        if not host_is_windows():
            mode = INSTALL_SH.stat().st_mode
            self.assertTrue(stat.S_IXUSR & mode, "install.sh should be executable")

    def test_scripts_have_no_python_fallback(self) -> None:
        for path in (INSTALL_SH, INSTALL_PS1):
            text = path.read_text(encoding="utf-8").lower()
            self.assertNotIn("python", text, f"{path.name} must not fall back to Python")
            self.assertNotIn("pins.sh", text, f"{path.name} must parse backends.env itself")

    def test_windows_script_hashes_without_get_filehash(self) -> None:
        text = INSTALL_PS1.read_text(encoding="utf-8")
        self.assertNotIn(
            "Get-FileHash",
            text,
            "install.ps1 must not depend on Get-FileHash (missing on some hosts)",
        )
        self.assertIn("System.Security.Cryptography.SHA256", text)

    def test_scripts_do_not_duplicate_pin_literals(self) -> None:
        pins = parse_backends_env(BACKENDS_ENV)
        forbidden = (
            pins["UMM_EXIFTOOL_VERSION"],
            pins["UMM_EXIFTOOL_SHA256"],
            pins["UMM_STRAWBERRY_PERL_VERSION"],
        )
        for path in (INSTALL_SH, INSTALL_PS1):
            text = path.read_text(encoding="utf-8")
            for pin in forbidden:
                self.assertNotIn(
                    pin,
                    text,
                    f"{path.name} must not hardcode pin {pin}",
                )
            for key in PIN_KEYS:
                self.assertIn(
                    key.rstrip("="),
                    text,
                    f"{path.name} must parse {key.rstrip('=')}",
                )

    def test_scripts_never_claim_to_modify_path(self) -> None:
        for path in (INSTALL_SH, INSTALL_PS1):
            text = path.read_text(encoding="utf-8")
            self.assertIn("does not modify PATH", text)
            self.assertNotIn("setx ", text.lower())
            self.assertNotIn("GITHUB_PATH", text)

    def test_readme_and_discovery_docs(self) -> None:
        readme = README.read_text(encoding="utf-8")
        self.assertIn("tools/get-exiftool/install.sh", readme)
        self.assertIn("tools/get-exiftool/install.ps1", readme)
        self.assertIn("UMM_EXIFTOOL", readme)
        self.assertIn("prefix and cache", readme.lower())
        backend = BACKEND_HPP.read_text(encoding="utf-8")
        self.assertIn("tools/get-exiftool/", backend)
        self.assertIn("prefix and cache", backend.lower())

    def test_pin_parse_rejects_missing_version(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            pin = tmp_path / "backends.env"
            pin.write_text(
                "UMM_EXIFTOOL_SHA256=" + ("ab" * 32) + "\n",
                encoding="utf-8",
            )
            result = run_install(
                native_args(
                    prefix=tmp_path / "prefix",
                    pin_file=pin,
                    cache_dir=tmp_path / "cache",
                    check=True,
                )
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("UMM_EXIFTOOL_VERSION", result.stderr)

    def test_pin_parse_rejects_bad_sha(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            pin = tmp_path / "backends.env"
            write_pin(
                pin,
                version="9.9",
                sha256="not-a-sha",
                url="https://example.invalid/exiftool-9.9.tar.gz",
            )
            result = run_install(
                native_args(
                    prefix=tmp_path / "prefix",
                    pin_file=pin,
                    cache_dir=tmp_path / "cache",
                    check=True,
                )
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("UMM_EXIFTOOL_SHA256", result.stderr)

    def test_check_fails_when_not_installed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            archive = tmp_path / "good.tar.gz"
            version = "9.9"
            sha = make_archive(archive, version)
            pin = tmp_path / "backends.env"
            write_pin(
                pin,
                version=version,
                sha256=sha,
                url="https://example.invalid/exiftool-9.9.tar.gz",
            )
            result = run_install(
                native_args(
                    prefix=tmp_path / "prefix",
                    pin_file=pin,
                    cache_dir=tmp_path / "cache",
                    check=True,
                )
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("not found", result.stderr.lower())

    def test_checksum_fail_closed_on_tampered_cache(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            cache = tmp_path / "cache"
            cache.mkdir()
            version = "9.9"
            good = tmp_path / "good.tar.gz"
            sha = make_archive(good, version)
            tampered = cache / f"exiftool-{version}.tar.gz"
            tampered.write_bytes(good.read_bytes() + b"\x00")
            pin = tmp_path / "backends.env"
            write_pin(
                pin,
                version=version,
                sha256=sha,
                url="https://example.invalid/exiftool-9.9.tar.gz",
            )
            prefix = tmp_path / "prefix"
            result = run_install(
                native_args(prefix=prefix, pin_file=pin, cache_dir=cache)
            )
            self.assertNotEqual(result.returncode, 0, result.stderr)
            self.assertIn("checksum mismatch", result.stderr.lower())
            self.assertFalse(tampered.exists(), "tampered archive must be deleted")
            self.assertFalse(
                (prefix / "exiftool").exists(),
                "prefix must not be populated after checksum failure",
            )

    def test_install_from_cache_prefix_layout_and_check(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            cache = tmp_path / "cache"
            cache.mkdir()
            version = "9.9"
            archive = cache / f"exiftool-{version}.tar.gz"
            sha = make_archive(archive, version, b"#!/usr/bin/env perl\nprint qq(ok);\n")
            pin = tmp_path / "backends.env"
            write_pin(
                pin,
                version=version,
                sha256=sha,
                url="https://example.invalid/exiftool-9.9.tar.gz",
            )
            prefix = tmp_path / "prefix"
            result = run_install(
                native_args(prefix=prefix, pin_file=pin, cache_dir=cache)
            )
            if host_is_windows() and result.returncode != 0 and "Perl was not found" in (
                result.stderr or ""
            ):
                # Layout must still be written before the Perl guidance exit.
                self.assertTrue((prefix / "exiftool").is_file(), result.stderr)
            else:
                self.assertEqual(result.returncode, 0, result.stderr)
            script = prefix / "exiftool"
            self.assertTrue(script.is_file(), result.stderr)
            self.assertTrue((prefix / "lib" / "placeholder").is_file())
            combined = (result.stdout or "") + (result.stderr or "")
            self.assertIn("UMM_EXIFTOOL", combined)
            self.assertIn(str(script), combined)
            self.assertIn("ExifToolConfig.exiftool_script", combined)

            checked = run_install(
                native_args(
                    prefix=prefix,
                    pin_file=pin,
                    cache_dir=cache,
                    check=True,
                )
            )
            if host_is_windows() and "Perl was not found" in (checked.stderr or ""):
                self.assertNotEqual(checked.returncode, 0)
            else:
                self.assertEqual(checked.returncode, 0, checked.stderr)
                self.assertIn(version, checked.stdout)


if __name__ == "__main__":
    unittest.main()
