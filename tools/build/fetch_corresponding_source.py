#!/usr/bin/env python3
"""Download and SHA-256-verify corresponding-source archives (session 34 / P1).

Reads tools/build/corresponding-source.json. Mismatch deletes the bad file and
exits non-zero (fail-closed). Does not fetch ExifTool (never bundled, S1c).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = REPO_ROOT / "tools" / "build" / "corresponding-source.json"
SHA256_LEN = 64


def die(message: str) -> None:
    raise SystemExit(f"fetch_corresponding_source.py: {message}")


def load_manifest(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema") != "libumm.corresponding-source/v1":
        die(f"unexpected schema in {path}")
    components = data.get("components")
    if not isinstance(components, list) or not components:
        die("manifest has no components")
    return data


def archive_name(component: dict) -> str:
    ident = component["id"]
    version = component["version"]
    url = component["url"]
    suffix = ".tar.gz"
    if url.endswith(".tar.xz"):
        suffix = ".tar.xz"
    elif url.endswith(".zip"):
        suffix = ".zip"
    return f"{ident}-{version}{suffix}"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def verify_and_store(data: bytes, expected_sha: str, dest: Path) -> None:
    expected = expected_sha.lower()
    if len(expected) != SHA256_LEN:
        die(f"invalid sha256 for {dest.name}")
    actual = sha256_bytes(data)
    if actual != expected:
        if dest.exists():
            dest.unlink()
        die(
            f"SHA-256 mismatch for {dest.name}: expected {expected}, got {actual}"
        )
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)


def download(url: str, timeout: int) -> bytes:
    request = urllib.request.Request(
        url,
        headers={"User-Agent": "libumm-corresponding-source"},
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read()


def fetch_component(
    component: dict,
    output_dir: Path,
    cache_dir: Path | None,
    timeout: int,
    url_overrides: dict[str, str],
) -> Path:
    ident = component["id"]
    expected = component["sha256"]
    url = url_overrides.get(ident, component["url"])
    dest = output_dir / archive_name(component)
    cache_path = (cache_dir / dest.name) if cache_dir is not None else None
    if cache_path is not None and cache_path.is_file():
        cached = cache_path.read_bytes()
        if sha256_bytes(cached) == expected.lower():
            verify_and_store(cached, expected, dest)
            return dest
        cache_path.unlink()
    data = download(url, timeout)
    if cache_path is not None:
        verify_and_store(data, expected, cache_path)
        shutil.copy2(cache_path, dest)
        return dest
    verify_and_store(data, expected, dest)
    return dest


def parse_overrides(values: list[str]) -> dict[str, str]:
    overrides: dict[str, str] = {}
    for item in values:
        if "=" not in item:
            die(f"override must be id=url, got {item!r}")
        ident, url = item.split("=", 1)
        overrides[ident] = url
    return overrides


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--cache-dir", type=Path, default=None)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument(
        "--override-url",
        action="append",
        default=[],
        help="id=url (tests / local file URIs)",
    )
    args = parser.parse_args(argv)

    manifest = load_manifest(args.manifest)
    overrides = parse_overrides(args.override_url)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    if args.cache_dir is not None:
        args.cache_dir.mkdir(parents=True, exist_ok=True)
    for component in manifest["components"]:
        path = fetch_component(
            component,
            args.output_dir,
            args.cache_dir,
            args.timeout,
            overrides,
        )
        print(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
