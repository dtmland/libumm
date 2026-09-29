#!/usr/bin/env python3
"""Offline contract tests for the session 34 release pipeline."""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_workflow import LATEST_RUNNERS, PINNED_RUNNERS, content_lines  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[2]
RELEASE_WORKFLOW = REPO_ROOT / ".github" / "workflows" / "release.yml"
PACKAGE_RELEASE = REPO_ROOT / "tools" / "build" / "package_release.py"
FETCH_SOURCE = REPO_ROOT / "tools" / "build" / "fetch_corresponding_source.py"
GENERATE_NOTES = REPO_ROOT / "tools" / "build" / "generate_release_notes.py"
CHECKLIST = REPO_ROOT / "docs" / "release-checklist.md"
MANIFEST = REPO_ROOT / "tools" / "build" / "corresponding-source.json"
TRACK_MATCH = REPO_ROOT / "tests" / "backend" / "test_track_match.cpp"
PROGRESS = REPO_ROOT / "docs" / "implementation-progress.md"


def release_text() -> str:
    return RELEASE_WORKFLOW.read_text(encoding="utf-8")


def run_tool(args: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["python3", *args],
        cwd=cwd or REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


class TestReleaseWorkflow(unittest.TestCase):
    def test_workflow_file_exists_and_parses(self) -> None:
        self.assertTrue(RELEASE_WORKFLOW.is_file(), f"missing {RELEASE_WORKFLOW}")
        lines = content_lines(release_text())
        self.assertGreaterEqual(len(lines), 8)
        top_keys = [
            line.split(":", 1)[0].strip()
            for line in lines
            if len(line) == len(line.lstrip()) and ":" in line
        ]
        for key in ("on", "permissions", "jobs"):
            self.assertIn(key, top_keys)

    def test_triggers_tags_and_dispatch(self) -> None:
        text = release_text()
        self.assertIn("workflow_dispatch", text)
        self.assertIn("v*.*.*", text)
        self.assertIn("tags:", text)

    def test_pinned_runners_and_presets(self) -> None:
        text = release_text()
        for label in PINNED_RUNNERS:
            self.assertIn(label, text)
        for label in LATEST_RUNNERS:
            self.assertNotIn(label, text)
        self.assertIn("sh tools/build/pins.sh", text)
        self.assertIn("cmake --preset default", text)
        self.assertRegex(text, r"fail-fast:\s*false")
        self.assertIn("cmake --build --preset default", text)
        self.assertIn("ctest --preset default", text)
        self.assertIn("-DUMM_REQUIRE_EXIV2=ON", text)
        self.assertIn("-DUMM_REQUIRE_EXIFTOOL=ON", text)
        self.assertNotIn("UMM_EXIV2_SHARED", text)
        self.assertNotIn("ubuntu-latest", text)

    def test_never_uploads_on_failure(self) -> None:
        text = release_text()
        self.assertNotIn("if: always()", text)
        self.assertIn("if-no-files-found: error", text)
        self.assertIn("needs:", text)
        self.assertIn("corresponding-source", text)
        self.assertIn("SHA256SUMS", text)
        self.assertIn("--draft", text)
        test_at = text.index("ctest --preset default")
        package_at = text.index("package_release.py package")
        self.assertLess(test_at, package_at, "tests must run before packaging")
        upload_at = text.index("Upload binary archive")
        self.assertLess(package_at, upload_at)

    def test_attaches_corresponding_source_and_get_exiftool(self) -> None:
        text = release_text()
        self.assertIn("fetch_corresponding_source.py", text)
        self.assertIn("corresponding-source.json", text)
        self.assertIn("package_release.py", text)
        self.assertIn("generate_release_notes.py", text)
        self.assertIn("libumm-${VERSION}-src.tar.gz", text)
        self.assertIn("gh release create", text)
        self.assertIn(
            "github.event_name == 'push' && startsWith(github.ref, 'refs/tags/v')",
            text,
        )

    def test_download_artifact_is_patched(self) -> None:
        # GHSA-cxww-7g56-2vh6: Zip Slip in @actions/download-artifact
        # >=4.0.0,<4.1.3. The floating @v4 tag is treated as 4.0.0 by scanners.
        text = release_text()
        self.assertNotRegex(text, r"download-artifact@v4(?![\d.])")
        pins = re.findall(r"download-artifact@v(\d+)\.(\d+)\.(\d+)", text)
        self.assertTrue(pins, "download-artifact must be pinned to a patch version")
        for major, minor, patch in pins:
            self.assertGreaterEqual(
                (int(major), int(minor), int(patch)),
                (4, 1, 3),
                "download-artifact must be >= 4.1.3",
            )

    def test_helper_scripts_and_checklist_exist(self) -> None:
        for path in (
            PACKAGE_RELEASE,
            FETCH_SOURCE,
            GENERATE_NOTES,
            CHECKLIST,
            MANIFEST,
        ):
            self.assertTrue(path.is_file(), f"missing {path}")
        checklist = CHECKLIST.read_text(encoding="utf-8")
        self.assertIn("S1d", checklist)
        self.assertIn("version.hpp", checklist)
        self.assertIn("vX.Y.Z", checklist)
        self.assertIn("SHA256SUMS", checklist)
        self.assertIn("pin", checklist.lower())
        self.assertIn("workflow_dispatch", checklist)

    def test_video_writeback_covers_mp4_and_mov(self) -> None:
        text = TRACK_MATCH.read_text(encoding="utf-8")
        self.assertIn('test_video(".mp4")', text)
        self.assertIn('test_video(".mov")', text)
        self.assertIn("matchTrack", text)
        self.assertIn("write_gps", text)
        self.assertIn("setGps", text)
        self.assertIn("umm::write", text)

    def test_progress_row_mentions_session_34(self) -> None:
        text = PROGRESS.read_text(encoding="utf-8")
        self.assertIn("34-release-pipeline.md", text)


class TestReleaseTools(unittest.TestCase):
    def test_notes_include_version_standards_pins_and_p1(self) -> None:
        result = run_tool([str(GENERATE_NOTES)])
        self.assertEqual(result.returncode, 0, result.stderr)
        text = result.stdout
        version = run_tool([str(GENERATE_NOTES), "--version-only"])
        self.assertEqual(version.returncode, 0, version.stderr)
        self.assertRegex(version.stdout.strip(), r"^\d+\.\d+\.\d+$")
        self.assertIn(version.stdout.strip(), text)
        self.assertIn("IPTC Photo Metadata 2025.1", text)
        self.assertIn("IPTC Video Metadata Hub 1.7", text)
        self.assertIn("GPL-3.0", text)
        self.assertIn("Apache-2.0", text)
        self.assertIn("S1d", text)
        self.assertIn("never bundled", text)
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        pins = {
            item["id"]: item
            for item in manifest["components"]
        }
        self.assertIn(pins["exiv2"]["version"], text)

    def test_package_archive_contains_required_paths(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            prefix = root / "prefix"
            (prefix / "include" / "umm").mkdir(parents=True)
            (prefix / "lib" / "cmake" / "umm").mkdir(parents=True)
            (prefix / "share" / "doc" / "libumm" / "licenses").mkdir(parents=True)
            (prefix / "include" / "umm" / "umm.hpp").write_text("// umm\n", encoding="utf-8")
            (prefix / "include" / "umm" / "version.hpp").write_text("// ver\n", encoding="utf-8")
            (prefix / "lib" / "libumm.a").write_bytes(b"lib")
            (prefix / "lib" / "cmake" / "umm" / "ummConfig.cmake").write_text(
                "# config\n", encoding="utf-8"
            )
            (prefix / "share" / "doc" / "libumm" / "LICENSE").write_text("L\n", encoding="utf-8")
            (prefix / "share" / "doc" / "libumm" / "NOTICE.md").write_text("N\n", encoding="utf-8")
            (
                prefix / "share" / "doc" / "libumm" / "THIRD-PARTY-NOTICES.md"
            ).write_text("T\n", encoding="utf-8")
            (prefix / "share" / "doc" / "libumm" / "licenses" / "GPL-3.0.txt").write_text(
                "GPL\n", encoding="utf-8"
            )
            out = root / "out"
            result = run_tool(
                [
                    str(PACKAGE_RELEASE),
                    "package",
                    "--install-prefix",
                    str(prefix),
                    "--source-root",
                    str(REPO_ROOT),
                    "--version",
                    "0.1.0",
                    "--os",
                    "ubuntu-24.04",
                    "--output-dir",
                    str(out),
                ]
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            archive = out / "libumm-0.1.0-ubuntu-24.04.tar.gz"
            self.assertTrue(archive.is_file())
            with tarfile.open(archive, "r:gz") as tar:
                names = tar.getnames()
            joined = "\n".join(names)
            for needle in (
                "libumm-0.1.0/include/umm/umm.hpp",
                "libumm-0.1.0/lib/libumm.a",
                "libumm-0.1.0/lib/cmake/umm/ummConfig.cmake",
                "libumm-0.1.0/share/doc/libumm/LICENSE",
                "libumm-0.1.0/share/doc/libumm/NOTICE.md",
                "libumm-0.1.0/share/doc/libumm/THIRD-PARTY-NOTICES.md",
                "libumm-0.1.0/share/doc/libumm/licenses/GPL-3.0.txt",
                "libumm-0.1.0/tools/get-exiftool/install.sh",
                "libumm-0.1.0/tools/get-exiftool/install.ps1",
                "libumm-0.1.0/tools/get-exiftool/backends.env",
                "libumm-0.1.0/docs/abi-policy.md",
                "libumm-0.1.0/README.md",
                "libumm-0.1.0/MANIFEST.txt",
            ):
                self.assertIn(needle, joined)
            missing = run_tool(
                [
                    str(PACKAGE_RELEASE),
                    "package",
                    "--install-prefix",
                    str(root / "empty"),
                    "--source-root",
                    str(REPO_ROOT),
                    "--version",
                    "0.1.0",
                    "--os",
                    "ubuntu-24.04",
                    "--output-dir",
                    str(out),
                ]
            )
            self.assertNotEqual(missing.returncode, 0)

    def test_sha256sums_covers_every_asset(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            (directory / "a.tar.gz").write_bytes(b"aaa")
            (directory / "b.tar.gz").write_bytes(b"bbb")
            output = directory / "SHA256SUMS"
            result = run_tool(
                [
                    str(PACKAGE_RELEASE),
                    "sha256sums",
                    "--dir",
                    str(directory),
                    "--output",
                    str(output),
                ]
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            text = output.read_text(encoding="utf-8")
            self.assertIn("a.tar.gz", text)
            self.assertIn("b.tar.gz", text)
            self.assertNotRegex(text, r"  SHA256SUMS$")
            lines = [line for line in text.splitlines() if line]
            self.assertEqual(len(lines), 2)
            for line in lines:
                digest, name = line.split("  ", 1)
                actual = hashlib.sha256((directory / name).read_bytes()).hexdigest()
                self.assertEqual(digest, actual)

    def test_fetch_fail_closed_on_checksum_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = b"corresponding-source-fixture\n"
            archive = root / "good.tar.gz"
            archive.write_bytes(payload)
            digest = hashlib.sha256(payload).hexdigest()
            wrong = "0" * 64
            manifest = {
                "schema": "libumm.corresponding-source/v1",
                "decision": "P1",
                "components": [
                    {
                        "id": "exiv2",
                        "name": "Exiv2",
                        "version": "0.0.0",
                        "url": archive.resolve().as_uri(),
                        "sha256": wrong,
                    }
                ],
            }
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest) + "\n", encoding="utf-8")
            out = root / "out"
            out.mkdir()
            dest = out / "exiv2-0.0.0.tar.gz"
            dest.write_bytes(b"stale")
            bad = run_tool(
                [
                    str(FETCH_SOURCE),
                    "--manifest",
                    str(manifest_path),
                    "--output-dir",
                    str(out),
                ]
            )
            self.assertNotEqual(bad.returncode, 0, bad.stdout)
            self.assertIn("SHA-256 mismatch", bad.stderr)
            self.assertFalse(dest.exists())

            manifest["components"][0]["sha256"] = digest
            manifest_path.write_text(json.dumps(manifest) + "\n", encoding="utf-8")
            good = run_tool(
                [
                    str(FETCH_SOURCE),
                    "--manifest",
                    str(manifest_path),
                    "--output-dir",
                    str(out),
                ]
            )
            self.assertEqual(good.returncode, 0, good.stderr)
            self.assertEqual(dest.read_bytes(), payload)


if __name__ == "__main__":
    unittest.main()
