#!/usr/bin/env python3
"""Offline contract tests for the generated property reference (C14a)."""

from __future__ import annotations

import json
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
GENERATOR = REPO_ROOT / "tools" / "registry" / "generate_property_reference.py"
REGISTRY_PHOTO = REPO_ROOT / "registry" / "iptc-photo"
REGISTRY_VIDEO = REPO_ROOT / "registry" / "iptc-video"
OVERLAY = REPO_ROOT / "registry" / "mappings" / "iptc-exif-overlay.json"
CROSS_MEDIA = REPO_ROOT / "registry" / "mappings" / "cross-media-accessors.json"
CASTS_DIR = REPO_ROOT / "registry" / "casts"
OUTPUT_DIR = REPO_ROOT / "docs" / "user" / "properties"
GUIDE = REPO_ROOT / "docs" / "user" / "guide.md"
GITATTRIBUTES = REPO_ROOT / ".gitattributes"
CMAKE_REGISTRY = REPO_ROOT / "cmake" / "LibummRegistry.cmake"
PAGES = ("README.md", "photo.md", "video.md", "base-keys.md")
ANCHOR_RE = re.compile(r'<a id="([^"]+)"></a>')
MD_LINK_RE = re.compile(r"\[[^\]]*\]\(([^)]+)\)")


def run_generator(
    output_dir: Path,
    *,
    photo_dir: Path = REGISTRY_PHOTO,
    video_dir: Path = REGISTRY_VIDEO,
    overlay: Path = OVERLAY,
    cross_media: Path = CROSS_MEDIA,
    casts_dir: Path = CASTS_DIR,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            "python3",
            str(GENERATOR),
            "--registry-dir",
            str(photo_dir),
            "--registry-dir",
            str(video_dir),
            "--overlay",
            str(overlay),
            "--cross-media",
            str(cross_media),
            "--casts-dir",
            str(casts_dir),
            "--output-dir",
            str(output_dir),
        ],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def load_registry_ids() -> list[str]:
    ids: list[str] = []
    for path in (REGISTRY_PHOTO / "iptc-photo.json", REGISTRY_VIDEO / "iptc-video.json"):
        data = json.loads(path.read_text(encoding="utf-8"))
        ids.extend(record["id"] for record in data["properties"])
    return ids


class TestPropertyReference(unittest.TestCase):
    def test_required_files_exist(self) -> None:
        for path in (
            GENERATOR,
            OUTPUT_DIR / "README.md",
            OUTPUT_DIR / "photo.md",
            OUTPUT_DIR / "video.md",
            OUTPUT_DIR / "base-keys.md",
            CMAKE_REGISTRY,
        ):
            self.assertTrue(path.is_file(), f"missing {path}")

    def test_committed_pages_match_generator(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)
            result = run_generator(output)
            self.assertEqual(result.returncode, 0, result.stderr)
            for name in PAGES:
                committed = (OUTPUT_DIR / name).read_bytes()
                generated = (output / name).read_bytes()
                self.assertEqual(generated, committed, name)
                self.assertTrue(committed.endswith(b"\n"), name)
                self.assertNotIn(b"\r\n", committed, name)
                self.assertIn(b"GENERATED", committed.splitlines()[2])

    def test_generator_is_byte_identical_across_runs(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "a"
            second = Path(tmp) / "b"
            r1 = run_generator(first)
            r2 = run_generator(second)
            self.assertEqual(r1.returncode, 0, r1.stderr)
            self.assertEqual(r2.returncode, 0, r2.stderr)
            for name in PAGES:
                self.assertEqual((first / name).read_bytes(), (second / name).read_bytes())

    def test_hand_edit_fails_byte_compare(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)
            result = run_generator(output)
            self.assertEqual(result.returncode, 0, result.stderr)
            mutated = (output / "README.md").read_text(encoding="utf-8") + "hand-edit\n"
            self.assertNotEqual(
                mutated.encode("utf-8"),
                (OUTPUT_DIR / "README.md").read_bytes(),
            )

    def test_editing_registry_changes_generated_markdown(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            photo_dir = root / "iptc-photo"
            photo_dir.mkdir()
            data = json.loads(
                (REGISTRY_PHOTO / "iptc-photo.json").read_text(encoding="utf-8")
            )
            found = False
            for record in data["properties"]:
                if record["id"] == "iptc.photo.title":
                    record["definition"] = "edited-property-definition-token"
                    found = True
            self.assertTrue(found)
            (photo_dir / "iptc-photo.json").write_text(
                json.dumps(data, indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            result = run_generator(root / "out", photo_dir=photo_dir)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = (root / "out" / "photo.md").read_text(encoding="utf-8")
            self.assertIn("edited-property-definition-token", text)

    def test_every_registry_id_has_exactly_one_anchor(self) -> None:
        photo = (OUTPUT_DIR / "photo.md").read_text(encoding="utf-8")
        video = (OUTPUT_DIR / "video.md").read_text(encoding="utf-8")
        combined = photo + video
        anchors = ANCHOR_RE.findall(combined)
        ids = load_registry_ids()
        self.assertEqual(len(anchors), len(set(anchors)))
        self.assertEqual(set(anchors), set(ids))
        for property_id in ids:
            self.assertEqual(combined.count(f'<a id="{property_id}"></a>'), 1)

    def test_guide_property_links_resolve(self) -> None:
        guide = GUIDE.read_text(encoding="utf-8")
        self.assertNotIn("most common properties", guide.lower())
        self.assertNotIn("## Full property reference", guide)
        self.assertIn("properties/README.md", guide)
        photo = (OUTPUT_DIR / "photo.md").read_text(encoding="utf-8")
        video = (OUTPUT_DIR / "video.md").read_text(encoding="utf-8")
        base = (OUTPUT_DIR / "base-keys.md").read_text(encoding="utf-8")
        pages = {
            "README.md": (OUTPUT_DIR / "README.md").read_text(encoding="utf-8"),
            "photo.md": photo,
            "video.md": video,
            "base-keys.md": base,
        }
        for target in MD_LINK_RE.findall(guide):
            if "properties/" not in target:
                continue
            path_part, _, fragment = target.partition("#")
            rel = path_part.split("properties/", 1)[1]
            dest = OUTPUT_DIR / rel
            self.assertTrue(dest.is_file(), f"guide link missing file: {target}")
            if fragment:
                text = dest.read_text(encoding="utf-8")
                self.assertIn(
                    f'<a id="{fragment}"></a>',
                    text,
                    f"guide link missing anchor: {target}",
                )
        index = pages["README.md"]
        for target in MD_LINK_RE.findall(index):
            if target.startswith("http"):
                continue
            path_part, _, fragment = target.partition("#")
            dest = (OUTPUT_DIR / path_part).resolve()
            self.assertTrue(dest.is_file(), f"index link missing file: {target}")
            if fragment:
                text = dest.read_text(encoding="utf-8")
                self.assertIn(
                    f'<a id="{fragment}"></a>',
                    text,
                    f"index link missing anchor: {target}",
                )

    def test_datetimeoriginal_note_and_cast_source_keys(self) -> None:
        photo = (OUTPUT_DIR / "photo.md").read_text(encoding="utf-8")
        self.assertIn("DateTimeOriginal", photo)
        self.assertIn("when the photo was taken", photo)
        self.assertIn("not `DateTime`", photo)
        base = (OUTPUT_DIR / "base-keys.md").read_text(encoding="utf-8")
        self.assertIn("QuickTime.CreateDate", base)
        self.assertIn("Exif.Image.Make", base)
        self.assertIn(EXIFTOOL_TAGNAMES_SNIPPET, base)

    def test_gitattributes_and_cmake_wire_the_generator(self) -> None:
        attrs = GITATTRIBUTES.read_text(encoding="utf-8")
        self.assertIn("docs/user/properties/**/*.md", attrs)
        self.assertIn("eol=lf", attrs)
        cmake = CMAKE_REGISTRY.read_text(encoding="utf-8")
        self.assertIn("generate_property_reference.py", cmake)
        self.assertIn("umm_property_reference_codegen", cmake)


EXIFTOOL_TAGNAMES_SNIPPET = "https://exiftool.org/TagNames/"


if __name__ == "__main__":
    unittest.main()
