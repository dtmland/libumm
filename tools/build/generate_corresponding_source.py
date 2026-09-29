#!/usr/bin/env python3
"""Generate corresponding-source.json from backend and Exiv2 FetchContent pins.

Session 31 / decision P1: the pin files are the single source of truth. This
script copies URL and SHA-256 values out of tools/build/backends.env (Exiv2)
and cmake/LibummExiv2.cmake (Expat, zlib fallbacks). Session 34 attaches the
listed archives to releases.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_ENV = REPO_ROOT / "tools" / "build" / "backends.env"
DEFAULT_EXIV2_CMAKE = REPO_ROOT / "cmake" / "LibummExiv2.cmake"
DEFAULT_OUTPUT = REPO_ROOT / "tools" / "build" / "corresponding-source.json"

KEY_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")
SHA256_RE = re.compile(r"^[0-9a-fA-F]{64}$")
EXIV2_URL_RE = re.compile(
    r'set\(_umm_exiv2_url\s+"([^"]+)"\)',
)
FETCH_URL_RE = re.compile(
    r"FetchContent_Declare\(\s*(umm_expat|umm_zlib)\s+"
    r'URL\s+"([^"]+)"\s+'
    r"URL_HASH\s+SHA256=([0-9a-fA-F]{64})",
    re.DOTALL,
)
ARCHIVE_VERSION_RE = re.compile(
    r"(?:expat|zlib)-([0-9]+\.[0-9]+(?:\.[0-9]+)?)\.tar\.gz$"
)


def parse_backends_env(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip().lstrip("\ufeff")
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise SystemExit(f"malformed line in {path}: {raw_line!r}")
        key, value = line.split("=", 1)
        if not KEY_RE.fullmatch(key):
            raise SystemExit(f"invalid key in {path}: {key!r}")
        if key in values:
            raise SystemExit(f"duplicate key in {path}: {key}")
        values[key] = value
    return values


def substitute_cmake(template: str, pins: dict[str, str]) -> str:
    def replace(match: re.Match[str]) -> str:
        key = match.group(1)
        if key not in pins:
            raise SystemExit(f"Exiv2 URL template references unknown pin {key}")
        return pins[key]

    return re.sub(r"\$\{([A-Z][A-Z0-9_]*)\}", replace, template)


def version_from_archive_url(url: str) -> str:
    name = url.rsplit("/", 1)[-1]
    match = ARCHIVE_VERSION_RE.search(name)
    if not match:
        raise SystemExit(f"cannot parse version from archive URL: {url}")
    return match.group(1)


def collect_components(pins: dict[str, str], cmake_text: str) -> list[dict[str, object]]:
    exiv2_version = pins.get("UMM_EXIV2_VERSION", "")
    exiv2_sha = pins.get("UMM_EXIV2_SHA256", "")
    if not exiv2_version or not SHA256_RE.fullmatch(exiv2_sha):
        raise SystemExit("backends.env is missing a valid UMM_EXIV2_VERSION/SHA256")

    url_match = EXIV2_URL_RE.search(cmake_text)
    if not url_match:
        raise SystemExit("cmake/LibummExiv2.cmake is missing _umm_exiv2_url")
    exiv2_url = substitute_cmake(url_match.group(1), pins)

    fetched = {match.group(1): match for match in FETCH_URL_RE.finditer(cmake_text)}
    if "umm_expat" not in fetched or "umm_zlib" not in fetched:
        raise SystemExit("cmake/LibummExiv2.cmake is missing umm_expat/umm_zlib pins")

    expat_url = fetched["umm_expat"].group(2)
    expat_sha = fetched["umm_expat"].group(3)
    zlib_url = fetched["umm_zlib"].group(2)
    zlib_sha = fetched["umm_zlib"].group(3)

    return [
        {
            "id": "exiv2",
            "name": "Exiv2",
            "version": exiv2_version,
            "license": "GPL-2.0-or-later",
            "url": exiv2_url,
            "sha256": exiv2_sha.lower(),
            "statically_absorbed": True,
            "pin_source": "tools/build/backends.env",
            "license_files": ["licenses/GPL-2.0.txt", "licenses/GPL-3.0.txt"],
        },
        {
            "id": "expat",
            "name": "Expat",
            "version": version_from_archive_url(expat_url),
            "license": "MIT",
            "url": expat_url,
            "sha256": expat_sha.lower(),
            "statically_absorbed": True,
            "pin_source": "cmake/LibummExiv2.cmake",
            "license_files": ["licenses/Expat.txt"],
        },
        {
            "id": "zlib",
            "name": "zlib",
            "version": version_from_archive_url(zlib_url),
            "license": "Zlib",
            "url": zlib_url,
            "sha256": zlib_sha.lower(),
            "statically_absorbed": True,
            "pin_source": "cmake/LibummExiv2.cmake",
            "license_files": ["licenses/Zlib.txt"],
        },
    ]


def build_manifest(env_path: Path, cmake_path: Path) -> dict[str, object]:
    pins = parse_backends_env(env_path)
    cmake_text = cmake_path.read_text(encoding="utf-8")
    return {
        "schema": "libumm.corresponding-source/v1",
        "decision": "P1",
        "note": (
            "Pinned source archives to attach as GPLv3 corresponding source "
            "for binaries that include the Exiv2 backend. Values are copied "
            "from backends.env and LibummExiv2.cmake; do not edit by hand."
        ),
        "components": collect_components(pins, cmake_text),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--env", type=Path, default=DEFAULT_ENV)
    parser.add_argument("--exiv2-cmake", type=Path, default=DEFAULT_EXIV2_CMAKE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args(argv)

    manifest = build_manifest(args.env, args.exiv2_cmake)
    text = json.dumps(manifest, indent=2, sort_keys=False) + "\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
