#!/usr/bin/env python3
"""Checksum-pinned Tier B corpus fetcher (session 27, decision M6).

Downloads samples from tests/corpus/manifest.json into .cache/corpus/.
Fail-closed on size or SHA-256 mismatch. Idempotent when the cache already
matches. Offline-safe: --offline never opens a network URL; missing samples
are a visible skip unless --require is set.
"""

from __future__ import annotations

import argparse
import hashlib
import http.client
import json
import re
import sys
import tempfile
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = REPO_ROOT / "tests" / "corpus" / "manifest.json"
DEFAULT_DEST = REPO_ROOT / ".cache" / "corpus"

ID_RE = re.compile(r"^[a-z0-9-]+$")
PATH_RE = re.compile(r"^[A-Za-z0-9._/-]+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
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

SKIP_PREFIX = "SKIP: "


class FetchError(Exception):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def load_manifest(path: Path) -> list[dict[str, Any]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise FetchError(f"{path} root must be an object")
    if data.get("version") != 1:
        raise FetchError(f"{path} version must be 1")
    samples = data.get("samples")
    if not isinstance(samples, list) or not samples:
        raise FetchError(f"{path} samples must be a non-empty array")
    seen: set[str] = set()
    seen_paths: set[str] = set()
    for index, sample in enumerate(samples):
        if not isinstance(sample, dict):
            raise FetchError(f"{path} samples[{index}] must be an object")
        missing = [field for field in REQUIRED_FIELDS if field not in sample]
        if missing:
            raise FetchError(f"{path} samples[{index}] missing {missing}")
        sample_id = sample["id"]
        relpath = sample["path"]
        sha = sample["sha256"]
        size = sample["bytes"]
        url = sample["url"]
        if not isinstance(sample_id, str) or not ID_RE.fullmatch(sample_id):
            raise FetchError(f"invalid sample id: {sample_id!r}")
        if sample_id in seen:
            raise FetchError(f"duplicate sample id: {sample_id}")
        seen.add(sample_id)
        if not isinstance(relpath, str) or not PATH_RE.fullmatch(relpath):
            raise FetchError(f"invalid sample path: {relpath!r}")
        if relpath.startswith("/") or ".." in Path(relpath).parts:
            raise FetchError(f"sample path must be relative without ..: {relpath}")
        if relpath in seen_paths:
            raise FetchError(f"duplicate sample path: {relpath}")
        seen_paths.add(relpath)
        if not isinstance(sha, str) or not SHA256_RE.fullmatch(sha):
            raise FetchError(f"invalid sha256 for {sample_id}")
        if not isinstance(size, int) or size <= 0:
            raise FetchError(f"invalid bytes for {sample_id}")
        if not isinstance(url, str) or not url:
            raise FetchError(f"invalid url for {sample_id}")
        if not (
            url.startswith("https://")
            or url.startswith("http://127.0.0.1")
            or url.startswith("http://localhost")
            or url.startswith("file:")
        ):
            raise FetchError(f"url must be https or local test URL: {url}")
    return samples


def matches_pin(path: Path, sha: str, size: int) -> bool:
    if not path.is_file():
        return False
    if path.stat().st_size != size:
        return False
    return sha256_file(path) == sha


def unlink_if_exists(path: Path) -> None:
    try:
        path.unlink()
    except FileNotFoundError:
        return


def download(url: str, timeout: float) -> bytes:
    request = urllib.request.Request(
        url,
        headers={"User-Agent": "libumm-corpus-fetcher/1"},
    )
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return response.read()
    except http.client.IncompleteRead as exc:
        raise FetchError(f"truncated download: {url}: {exc}") from exc
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        raise FetchError(f"download failed: {url}: {exc}") from exc


def store_verified(dest: Path, data: bytes, sha: str, size: int, sample_id: str) -> None:
    if len(data) != size:
        raise FetchError(
            f"{sample_id}: truncated or size mismatch: got {len(data)} bytes, expected {size}"
        )
    digest = sha256_bytes(data)
    if digest != sha:
        raise FetchError(
            f"{sample_id}: checksum mismatch: got {digest}, expected {sha}"
        )
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            dir=dest.parent, prefix=dest.name + ".", suffix=".part", delete=False
        ) as handle:
            tmp_path = Path(handle.name)
            handle.write(data)
        tmp_path.replace(dest)
    except Exception:
        if tmp_path is not None:
            unlink_if_exists(tmp_path)
        unlink_if_exists(dest)
        raise


def fetch_sample(
    sample: dict[str, Any],
    dest_root: Path,
    *,
    offline: bool,
    timeout: float,
) -> str:
    sample_id = sample["id"]
    dest = dest_root / sample["path"]
    sha = sample["sha256"]
    size = sample["bytes"]
    if matches_pin(dest, sha, size):
        return f"cached {sample_id}"
    if dest.exists():
        unlink_if_exists(dest)
    if offline:
        raise FetchError(f"{sample_id}: missing from cache (offline)")
    data = download(sample["url"], timeout)
    try:
        store_verified(dest, data, sha, size, sample_id)
    except FetchError:
        unlink_if_exists(dest)
        raise
    return f"fetched {sample_id}"


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--dest", type=Path, default=DEFAULT_DEST)
    parser.add_argument(
        "--offline",
        action="store_true",
        help="Do not download; use only files already in dest",
    )
    parser.add_argument(
        "--require",
        action="store_true",
        help="Fail if any sample is missing (Tier B job); otherwise skip",
    )
    parser.add_argument("--timeout", type=float, default=60.0)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    manifest = args.manifest
    if not manifest.is_file():
        print(f"error: missing manifest {manifest}", file=sys.stderr)
        return 1
    try:
        samples = load_manifest(manifest)
    except (OSError, json.JSONDecodeError, FetchError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    dest_root = args.dest
    dest_root.mkdir(parents=True, exist_ok=True)
    skipped: list[str] = []
    for sample in samples:
        try:
            message = fetch_sample(
                sample,
                dest_root,
                offline=args.offline,
                timeout=args.timeout,
            )
        except FetchError as exc:
            text = str(exc)
            offline_miss = args.offline and "missing from cache" in text
            if (not args.require) and offline_miss:
                skipped.append(sample["id"])
                print(f"{SKIP_PREFIX}{text}", file=sys.stderr)
                continue
            print(f"error: {exc}", file=sys.stderr)
            return 1
        print(message)
    if skipped and not args.require:
        print(
            f"{SKIP_PREFIX}Tier B corpus not complete ({len(skipped)} missing)",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
