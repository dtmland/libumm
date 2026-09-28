#!/usr/bin/env python3
"""Offline contract tests for the Tier A fixture corpus (session 09)."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unicodedata
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FIXTURES = REPO_ROOT / "tests" / "fixtures"
GENERATOR = FIXTURES / "generator" / "generate.py"
BASE_JPEG = FIXTURES / "generator" / "base-16x16-gray.jpg"
BASE_TIFF = FIXTURES / "generator" / "base-16x16-gray.tif"
CONFIG = FIXTURES / "generator" / "exiftool.config"
MANIFEST = FIXTURES / "MANIFEST.md"
GITATTRIBUTES = REPO_ROOT / ".gitattributes"

UNICODE_FILENAME = "übüng ünïcode.jpg"
MAX_FILE_BYTES = 100 * 1024
MAX_CORPUS_BYTES = 1024 * 1024
TRUNCATE_BYTES = 64

# docs/test-media-plan.md §2.2
SECTION_22 = (
    "jpeg/minimal.jpg",
    "jpeg/exif-only.jpg",
    "jpeg/iptc-only.jpg",
    "jpeg/xmp-only.jpg",
    "jpeg/full-agreeing.jpg",
    "jpeg/full-conflicting.jpg",
    "jpeg/gps.jpg",
    "jpeg/unicode.jpg",
    "jpeg/makernote.jpg",
    "jpeg/unknown-tags.jpg",
    "sidecar/paired.jpg",
    "sidecar/paired.xmp",
    "sidecar/orphan.xmp",
    f"naming/{UNICODE_FILENAME}",
    "corrupt/truncated.jpg",
)

# docs/implementation/17-tiff-support.md — JPEG matrix pattern for TIFF.
TIFF_FILES = (
    "tiff/minimal.tif",
    "tiff/exif-only.tif",
    "tiff/iptc-only.tif",
    "tiff/xmp-only.tif",
    "tiff/full-agreeing.tif",
    "tiff/full-conflicting.tif",
    "tiff/gps.tif",
    "tiff/unicode.tif",
)

ENTRY_RE = re.compile(
    r"^### `([^`]+)`\n"
    r"\n"
    r"- \*\*SHA-256:\*\* `([0-9a-f]{64})`\n"
    r"- \*\*Size:\*\* (\d+) bytes\n"
    r"- \*\*Purpose:\*\* (.+)\n"
    r"- \*\*Command:\*\* `(.+)`",
    re.MULTILINE,
)

IGNORE_JSON_PREFIXES = ("ExifTool:", "System:", "Composite:", "SourceFile")


def nfc(text: str) -> str:
    return unicodedata.normalize("NFC", text)


def parse_manifest(text: str) -> dict[str, dict[str, str]]:
    entries: dict[str, dict[str, str]] = {}
    for match in ENTRY_RE.finditer(text):
        relpath, digest, size, purpose, command = match.groups()
        entries[relpath] = {
            "sha256": digest,
            "size": size,
            "purpose": purpose,
            "command": command,
        }
    return entries


def parse_open_items(text: str) -> list[str]:
    match = re.search(
        r"## Open items\n\n(.*)\n## Files\n",
        text,
        re.DOTALL,
    )
    if not match:
        raise AssertionError("MANIFEST.md missing Open items / Files sections")
    items: list[str] = []
    for line in match.group(1).splitlines():
        stripped = line.strip()
        if stripped.startswith("- ") and stripped != "- (none)":
            items.append(stripped[2:])
    return items


def deferred_paths(open_items: list[str]) -> set[str]:
    found: set[str] = set()
    for item in open_items:
        for relpath in SECTION_22:
            if f"`{relpath}`" in item:
                found.add(relpath)
    return found


def discover_exiftool() -> tuple[str, str] | None:
    script = os.environ.get("UMM_EXIFTOOL_SCRIPT", "").strip()
    perl = os.environ.get("UMM_PERL_EXECUTABLE", "").strip()
    if not script:
        cached = REPO_ROOT / ".cache" / "exiftool" / "src" / "exiftool"
        if cached.is_file():
            script = str(cached)
    if not perl:
        found = shutil.which("perl")
        perl = found or ""
    if script and perl and Path(script).is_file():
        return perl, script
    return None


def load_generator():
    spec = importlib.util.spec_from_file_location("umm_fixture_generate", GENERATOR)
    if spec is None or spec.loader is None:
        raise AssertionError(f"cannot load generator {GENERATOR}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def exiftool_json(perl: str, script: str, path: Path) -> dict:
    # Same Windows constraint as the generator: Perl argv uses the system ACP, so
    # Unicode filenames must go through a UTF-8 argfile after `-charset utf8`.
    args = [
        "-charset",
        "filename=UTF8",
        "-charset",
        "IPTC=UTF8",
        "-j",
        "-G1",
        "-a",
        "-s",
        "-n",
        str(path),
    ]
    fd, name = tempfile.mkstemp(prefix="umm-exiftool-", suffix=".args")
    os.close(fd)
    argfile = Path(name)
    try:
        argfile.write_text("\n".join(args) + "\n", encoding="utf-8", newline="\n")
        result = subprocess.run(
            [
                perl,
                script,
                "-config",
                str(CONFIG),
                "-charset",
                "utf8",
                "-@",
                str(argfile),
            ],
            capture_output=True,
            check=False,
        )
    finally:
        argfile.unlink(missing_ok=True)
    stderr = result.stderr.decode("utf-8", errors="replace")
    stdout = result.stdout.decode("utf-8", errors="replace")
    if result.returncode != 0:
        raise AssertionError(f"exiftool failed on {path}: {stderr}")
    if not stdout.strip():
        raise AssertionError(f"exiftool produced empty JSON for {path}: {stderr}")
    payload = json.loads(stdout)
    if not payload:
        return {}
    return payload[0]


def comparable_metadata(record: dict) -> dict:
    return {
        key: value
        for key, value in record.items()
        if not any(key == prefix or key.startswith(prefix) for prefix in IGNORE_JSON_PREFIXES)
        and not key.startswith("File:")
    }


class TestFixtureCorpus(unittest.TestCase):
    def test_required_generator_files_exist(self) -> None:
        for path in (GENERATOR, BASE_JPEG, BASE_TIFF, CONFIG, MANIFEST, GITATTRIBUTES):
            self.assertTrue(path.is_file(), f"missing {path}")

    def test_generator_passes_unicode_via_utf8_argfile(self) -> None:
        text = GENERATOR.read_text(encoding="utf-8")
        self.assertIn('"-charset"', text)
        self.assertIn('"utf8"', text)
        self.assertIn('"-@"', text)

    def test_exiftool_json_passes_path_via_utf8_argfile(self) -> None:
        captured: dict[str, object] = {}

        def fake_run(cmd, **_kwargs):
            argfile = Path(cmd[-1])
            captured["cmd"] = cmd
            captured["text"] = argfile.read_text(encoding="utf-8")

            class Result:
                returncode = 0
                stdout = b'[{"SourceFile":"x","XMP-dc:Creator":"ok"}]'
                stderr = b""

            return Result()

        original = subprocess.run
        try:
            subprocess.run = fake_run
            record = exiftool_json(
                "perl", str(GENERATOR), FIXTURES / "naming" / UNICODE_FILENAME
            )
        finally:
            subprocess.run = original
        cmd = captured["cmd"]
        self.assertIsInstance(cmd, list)
        self.assertIn("-charset", cmd)
        self.assertIn("utf8", cmd)
        self.assertEqual(cmd[-2], "-@")
        self.assertIn(UNICODE_FILENAME, str(captured["text"]))
        self.assertNotIn(UNICODE_FILENAME, " ".join(str(part) for part in cmd[:-1]))
        self.assertEqual(record.get("XMP-dc:Creator"), "ok")

    def test_exiftool_run_writes_unicode_argfile(self) -> None:
        gen = load_generator()
        captured: dict[str, object] = {}

        def fake_run(cmd, **_kwargs):
            argfile = Path(cmd[-1])
            captured["cmd"] = cmd
            captured["text"] = argfile.read_text(encoding="utf-8")

            class Result:
                returncode = 0
                stdout = b""
                stderr = b""

            return Result()

        original = gen.subprocess.run
        tool = gen.ExifTool(sys.executable, str(GENERATOR), CONFIG)
        try:
            gen.subprocess.run = fake_run
            tool.run(
                [
                    "-overwrite_original",
                    "-XMP-dc:Creator=Jürgen Müller",
                    "-XMP-dc:Description=café — 日本語",
                    "unicode.jpg",
                ]
            )
        finally:
            gen.subprocess.run = original
        cmd = captured["cmd"]
        self.assertIsInstance(cmd, list)
        self.assertIn("-charset", cmd)
        self.assertIn("utf8", cmd)
        self.assertEqual(cmd[-2], "-@")
        text = str(captured["text"])
        self.assertIn("Jürgen Müller", text)
        self.assertIn("日本語", text)

    def test_gitattributes_pins_fixture_encodings(self) -> None:
        text = GITATTRIBUTES.read_text(encoding="utf-8")
        lines = [
            line.strip()
            for line in text.splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        ]
        self.assertTrue(
            any("tests/fixtures/**/*.jpg" in line and "binary" in line for line in lines),
            ".gitattributes must mark JPEG fixtures as binary",
        )
        self.assertTrue(
            any("tests/fixtures/**/*.tif" in line and "binary" in line for line in lines),
            ".gitattributes must mark TIFF fixtures as binary",
        )
        self.assertTrue(
            any(
                "tests/fixtures/**/*.xmp" in line and "eol=lf" in line
                for line in lines
            ),
            ".gitattributes must pin XMP sidecars to LF",
        )

    def test_section_22_is_present_or_open(self) -> None:
        text = MANIFEST.read_text(encoding="utf-8")
        entries = parse_manifest(text)
        open_items = parse_open_items(text)
        deferred = deferred_paths(open_items)
        self.assertTrue(entries, "MANIFEST.md has no file entries")
        for relpath in SECTION_22:
            path = FIXTURES / relpath
            if relpath in deferred:
                self.assertFalse(path.exists(), f"deferred {relpath} should not be committed")
                continue
            self.assertIn(relpath, entries, f"{relpath} missing from MANIFEST.md")
            self.assertTrue(path.is_file(), f"missing fixture {relpath}")

    def test_tiff_matrix_is_present(self) -> None:
        text = MANIFEST.read_text(encoding="utf-8")
        entries = parse_manifest(text)
        for relpath in TIFF_FILES:
            path = FIXTURES / relpath
            self.assertIn(relpath, entries, f"{relpath} missing from MANIFEST.md")
            self.assertTrue(path.is_file(), f"missing fixture {relpath}")
            data = path.read_bytes()
            self.assertTrue(
                (data.startswith(b"II*\x00") or data.startswith(b"MM\x00*")),
                f"{relpath} is not TIFF magic",
            )

    def test_manifest_matches_files_on_disk(self) -> None:
        text = MANIFEST.read_text(encoding="utf-8")
        entries = parse_manifest(text)
        for relpath, info in entries.items():
            path = FIXTURES / relpath
            self.assertTrue(path.is_file(), f"manifest path missing: {relpath}")
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            self.assertEqual(digest, info["sha256"], relpath)
            self.assertEqual(str(path.stat().st_size), info["size"], relpath)
            self.assertTrue(info["purpose"].strip(), relpath)
            self.assertTrue(info["command"].strip(), relpath)
            self.assertNotIn("/home/", info["command"], "command should be repo-relative")

    def test_size_limits(self) -> None:
        text = MANIFEST.read_text(encoding="utf-8")
        entries = parse_manifest(text)
        total = 0
        for relpath in entries:
            size = (FIXTURES / relpath).stat().st_size
            self.assertLessEqual(size, MAX_FILE_BYTES, relpath)
            total += size
        self.assertLess(total, MAX_CORPUS_BYTES)

    def test_unicode_filename_git_encoding(self) -> None:
        naming = FIXTURES / "naming"
        self.assertTrue(naming.is_dir())
        names = [nfc(path.name) for path in naming.iterdir() if path.is_file()]
        self.assertIn(nfc(UNICODE_FILENAME), names)
        result = subprocess.run(
            ["git", "-c", "core.quotepath=false", "ls-files", "-z", "tests/fixtures/naming"],
            cwd=REPO_ROOT,
            capture_output=True,
            check=False,
        )
        if result.returncode != 0:
            self.skipTest("git ls-files unavailable")
        listed = [nfc(item.decode("utf-8")) for item in result.stdout.split(b"\0") if item]
        expected = nfc(f"tests/fixtures/naming/{UNICODE_FILENAME}")
        self.assertIn(expected, listed)

    def test_truncated_fixture_is_short_jpeg_prefix(self) -> None:
        truncated = FIXTURES / "corrupt" / "truncated.jpg"
        data = truncated.read_bytes()
        self.assertEqual(len(data), TRUNCATE_BYTES)
        self.assertTrue(data.startswith(b"\xff\xd8"))


@unittest.skipUnless(discover_exiftool(), "pinned ExifTool not available")
class TestFixtureExifTool(unittest.TestCase):
    def setUp(self) -> None:
        tools = discover_exiftool()
        assert tools is not None
        self.perl, self.script = tools

    def test_full_conflicting_dates_and_creators_differ(self) -> None:
        record = exiftool_json(
            self.perl, self.script, FIXTURES / "jpeg" / "full-conflicting.jpg"
        )
        self.assertEqual(record.get("IFD0:Artist"), "EXIF Creator")
        self.assertEqual(record.get("IPTC:By-line"), "IPTC Creator")
        self.assertEqual(record.get("XMP-dc:Creator"), "XMP Creator")
        exif_date = str(record.get("ExifIFD:DateTimeOriginal", ""))
        iptc_date = str(record.get("IPTC:DateCreated", ""))
        xmp_date = str(record.get("XMP-photoshop:DateCreated", ""))
        self.assertTrue(exif_date)
        self.assertTrue(iptc_date)
        self.assertTrue(xmp_date)
        self.assertNotEqual(exif_date[:10].replace(":", "-"), iptc_date[:10].replace(":", "-"))
        self.assertNotIn(iptc_date[:10], xmp_date)
        self.assertNotIn("2020:01:01", xmp_date)

    def test_unicode_filename_is_readable(self) -> None:
        record = exiftool_json(
            self.perl, self.script, FIXTURES / "naming" / UNICODE_FILENAME
        )
        self.assertEqual(record.get("XMP-dc:Creator"), "Jürgen Müller")
        self.assertIn("日本語", str(record.get("XMP-dc:Description", "")))

    def test_unicode_values_survive(self) -> None:
        record = exiftool_json(self.perl, self.script, FIXTURES / "jpeg" / "unicode.jpg")
        self.assertEqual(record.get("XMP-dc:Creator"), "Jürgen Müller")
        self.assertIn("café", str(record.get("XMP-dc:Description", "")))
        self.assertIn("日本語", str(record.get("XMP-dc:Description", "")))
        self.assertEqual(record.get("IPTC:By-line"), "Jürgen Müller")

    def test_tiff_full_conflicting_dates_and_creators_differ(self) -> None:
        record = exiftool_json(
            self.perl, self.script, FIXTURES / "tiff" / "full-conflicting.tif"
        )
        self.assertEqual(record.get("IFD0:Artist"), "EXIF Creator")
        self.assertEqual(record.get("IPTC:By-line"), "IPTC Creator")
        self.assertEqual(record.get("XMP-dc:Creator"), "XMP Creator")
        exif_date = str(record.get("ExifIFD:DateTimeOriginal", ""))
        iptc_date = str(record.get("IPTC:DateCreated", ""))
        xmp_date = str(record.get("XMP-photoshop:DateCreated", ""))
        self.assertTrue(exif_date)
        self.assertTrue(iptc_date)
        self.assertTrue(xmp_date)
        self.assertNotEqual(exif_date[:10].replace(":", "-"), iptc_date[:10].replace(":", "-"))
        self.assertNotIn(iptc_date[:10], xmp_date)
        self.assertNotIn("2020:01:01", xmp_date)

    def test_tiff_unicode_values_survive(self) -> None:
        record = exiftool_json(self.perl, self.script, FIXTURES / "tiff" / "unicode.tif")
        self.assertEqual(record.get("XMP-dc:Creator"), "Jürgen Müller")
        self.assertIn("café", str(record.get("XMP-dc:Description", "")))
        self.assertIn("日本語", str(record.get("XMP-dc:Description", "")))
        self.assertEqual(record.get("IPTC:By-line"), "Jürgen Müller")

    def test_generator_write_tags_preserves_unicode(self) -> None:
        gen = load_generator()
        tool = gen.ExifTool(self.perl, self.script, CONFIG)
        with tempfile.TemporaryDirectory() as tmp:
            dest = Path(tmp) / "unicode.jpg"
            shutil.copyfile(BASE_JPEG, dest)
            gen.write_tags(
                tool,
                dest,
                [
                    ("XMP-dc:Creator", "Jürgen Müller"),
                    ("XMP-dc:Description", "café — 日本語"),
                    ("IPTC:CodedCharacterSet", "UTF8"),
                    ("IPTC:By-line", "Jürgen Müller"),
                    ("IPTC:Caption-Abstract", "café — 日本語"),
                ],
            )
            record = exiftool_json(self.perl, self.script, dest)
        self.assertEqual(record.get("XMP-dc:Creator"), "Jürgen Müller")
        self.assertIn("日本語", str(record.get("XMP-dc:Description", "")))
        self.assertEqual(record.get("IPTC:By-line"), "Jürgen Müller")
        self.assertIn("日本語", str(record.get("IPTC:Caption-Abstract", "")))

    def test_sidecar_conflicts_with_embedded(self) -> None:
        embedded = exiftool_json(
            self.perl, self.script, FIXTURES / "sidecar" / "paired.jpg"
        )
        sidecar = exiftool_json(
            self.perl, self.script, FIXTURES / "sidecar" / "paired.xmp"
        )
        self.assertEqual(embedded.get("XMP-dc:Creator"), "Embedded Creator")
        self.assertEqual(sidecar.get("XMP-dc:Creator"), "Sidecar Creator")

    def test_regen_is_metadata_equivalent(self) -> None:
        text = MANIFEST.read_text(encoding="utf-8")
        entries = parse_manifest(text)
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "fixtures"
            result = subprocess.run(
                [
                    sys.executable,
                    str(GENERATOR),
                    "--repo-root",
                    str(REPO_ROOT),
                    "--output-dir",
                    str(out),
                    "--exiftool",
                    self.script,
                    "--perl",
                    self.perl,
                ],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            for relpath in entries:
                committed = FIXTURES / relpath
                generated = out / relpath
                self.assertTrue(generated.is_file(), relpath)
                if relpath == "corrupt/truncated.jpg":
                    self.assertEqual(committed.read_bytes(), generated.read_bytes())
                    continue
                left = comparable_metadata(
                    exiftool_json(self.perl, self.script, committed)
                )
                right = comparable_metadata(
                    exiftool_json(self.perl, self.script, generated)
                )
                self.assertEqual(left, right, relpath)


if __name__ == "__main__":
    unittest.main()
