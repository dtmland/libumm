#!/usr/bin/env python3
"""Offline contract tests for the Tier B corpus fetcher (session 27)."""

from __future__ import annotations

import hashlib
import http.server
import json
import socketserver
import subprocess
import sys
import tempfile
import threading
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FETCHER = REPO_ROOT / "tools" / "corpus" / "fetch.py"
MANIFEST = REPO_ROOT / "tests" / "corpus" / "manifest.json"
SCHEMA = REPO_ROOT / "tests" / "corpus" / "schema.md"
BACKENDS_ENV = REPO_ROOT / "tools" / "build" / "backends.env"

REQUIRED_FIELDS = (
    "id",
    "path",
    "url",
    "sha256",
    "bytes",
    "file_type",
    "license",
    "capability",
)
REQUIRED_IDS = ("jpeg-makernote", "raw-panasonic-rw2")
MEDIA_SUFFIXES = {
    ".jpg",
    ".jpeg",
    ".tif",
    ".tiff",
    ".png",
    ".webp",
    ".dng",
    ".raf",
    ".rw2",
    ".cr2",
    ".arw",
    ".nef",
    ".mp4",
    ".mov",
}


def parse_exiftool_version() -> str:
    for raw in BACKENDS_ENV.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if line.startswith("UMM_EXIFTOOL_VERSION="):
            return line.split("=", 1)[1]
    raise AssertionError("UMM_EXIFTOOL_VERSION missing from backends.env")


def write_json(path: Path, payload: dict) -> None:
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def sample(
    *,
    sample_id: str,
    relpath: str,
    url: str,
    data: bytes,
    sha: str | None = None,
    size: int | None = None,
) -> dict:
    return {
        "id": sample_id,
        "path": relpath,
        "url": url,
        "sha256": sha if sha is not None else hashlib.sha256(data).hexdigest(),
        "bytes": size if size is not None else len(data),
        "file_type": "JPEG",
        "license": "test",
        "capability": "fetcher contract",
    }


def run_fetch(manifest: Path, dest: Path, extra: list[str] | None = None) -> subprocess.CompletedProcess[str]:
    cmd = [
        sys.executable,
        str(FETCHER),
        "--manifest",
        str(manifest),
        "--dest",
        str(dest),
        "--timeout",
        "5",
    ]
    if extra:
        cmd.extend(extra)
    return subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True, check=False)


class TruncatingHandler(http.server.BaseHTTPRequestHandler):
    payload = b"0123456789abcdef"

    def do_GET(self) -> None:  # noqa: N802
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(self.payload)))
        self.end_headers()
        self.wfile.write(self.payload[:4])

    def log_message(self, format: str, *args: object) -> None:  # noqa: A003
        return


class TestCorpusManifest(unittest.TestCase):
    def test_schema_and_manifest_exist(self) -> None:
        self.assertTrue(SCHEMA.is_file(), SCHEMA)
        self.assertTrue(MANIFEST.is_file(), MANIFEST)
        self.assertTrue(FETCHER.is_file(), FETCHER)
        attributes = (REPO_ROOT / ".gitattributes").read_text(encoding="utf-8")
        self.assertIn("tests/corpus/**/*.json text eol=lf", attributes)

    def test_committed_corpus_has_no_media_bytes(self) -> None:
        root = REPO_ROOT / "tests" / "corpus"
        media = [
            path
            for path in root.rglob("*")
            if path.is_file() and path.suffix.lower() in MEDIA_SUFFIXES
        ]
        self.assertEqual(media, [], f"third-party media must not be committed: {media}")

    def test_manifest_pins_required_samples(self) -> None:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual(data["version"], 1)
        samples = data["samples"]
        self.assertIsInstance(samples, list)
        ids = [row["id"] for row in samples]
        for required in REQUIRED_IDS:
            self.assertIn(required, ids)
        pin = parse_exiftool_version()
        for row in samples:
            for field in REQUIRED_FIELDS:
                self.assertIn(field, row)
            self.assertTrue(row["url"].startswith("https://"))
            self.assertIn(f"/exiftool/{pin}/", row["url"])
            self.assertRegex(row["sha256"], r"^[0-9a-f]{64}$")
            self.assertIsInstance(row["bytes"], int)
            self.assertGreater(row["bytes"], 0)
        jpeg = next(row for row in samples if row["id"] == "jpeg-makernote")
        self.assertEqual(jpeg["path"], "jpeg/makernote.jpg")
        self.assertEqual(jpeg["file_type"], "JPEG")
        self.assertIn("MakerNote", jpeg["capability"])
        raw = next(row for row in samples if row["id"] == "raw-panasonic-rw2")
        self.assertEqual(raw["file_type"], "RW2")
        self.assertTrue(raw["path"].endswith(".rw2"))


class TestCorpusFetcher(unittest.TestCase):
    def test_offline_skip_when_cache_missing(self) -> None:
        data = b"hello-offline"
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "hello.bin"
            source.write_bytes(data)
            manifest = root / "manifest.json"
            dest = root / "dest"
            write_json(
                manifest,
                {
                    "version": 1,
                    "samples": [
                        sample(
                            sample_id="hello",
                            relpath="hello.bin",
                            url=source.resolve().as_uri(),
                            data=data,
                        )
                    ],
                },
            )
            result = run_fetch(manifest, dest, ["--offline"])
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("SKIP:", result.stderr)
            self.assertFalse((dest / "hello.bin").exists())

    def test_idempotent_file_uri_fetch(self) -> None:
        data = b"hello-cached"
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "hello.bin"
            source.write_bytes(data)
            manifest = root / "manifest.json"
            dest = root / "dest"
            write_json(
                manifest,
                {
                    "version": 1,
                    "samples": [
                        sample(
                            sample_id="hello",
                            relpath="a/hello.bin",
                            url=source.resolve().as_uri(),
                            data=data,
                        )
                    ],
                },
            )
            first = run_fetch(manifest, dest)
            self.assertEqual(first.returncode, 0, first.stderr)
            self.assertTrue((dest / "a/hello.bin").is_file())
            source.write_bytes(b"changed-source-should-not-matter")
            second = run_fetch(manifest, dest)
            self.assertEqual(second.returncode, 0, second.stderr)
            self.assertIn("cached hello", second.stdout)
            self.assertEqual((dest / "a/hello.bin").read_bytes(), data)

    def test_checksum_mismatch_fails_closed(self) -> None:
        data = b"hello-mismatch"
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "hello.bin"
            source.write_bytes(data)
            manifest = root / "manifest.json"
            dest = root / "dest"
            write_json(
                manifest,
                {
                    "version": 1,
                    "samples": [
                        sample(
                            sample_id="hello",
                            relpath="hello.bin",
                            url=source.resolve().as_uri(),
                            data=data,
                            sha="0" * 64,
                        )
                    ],
                },
            )
            result = run_fetch(manifest, dest, ["--require"])
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("checksum mismatch", result.stderr)
            self.assertFalse((dest / "hello.bin").exists())

    def test_truncated_download_fails_closed(self) -> None:
        payload = TruncatingHandler.payload
        server = socketserver.TCPServer(("127.0.0.1", 0), TruncatingHandler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            host, port = server.server_address
            url = f"http://127.0.0.1:{port}/sample.bin"
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                manifest = root / "manifest.json"
                dest = root / "dest"
                write_json(
                    manifest,
                    {
                        "version": 1,
                        "samples": [
                            sample(
                                sample_id="trunc",
                                relpath="sample.bin",
                                url=url,
                                data=payload,
                            )
                        ],
                    },
                )
                result = run_fetch(manifest, dest, ["--require"])
                self.assertNotEqual(result.returncode, 0)
                self.assertRegex(result.stderr, r"truncated|size mismatch")
                self.assertFalse((dest / "sample.bin").exists())
        finally:
            server.shutdown()
            server.server_close()


if __name__ == "__main__":
    unittest.main()
