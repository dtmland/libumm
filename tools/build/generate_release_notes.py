#!/usr/bin/env python3
"""Generate draft GitHub release notes from in-repo pins and registry (session 34)."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
VERSION_HPP = REPO_ROOT / "include" / "umm" / "version.hpp"
BACKENDS_ENV = REPO_ROOT / "tools" / "build" / "backends.env"
PHOTO_REGISTRY = REPO_ROOT / "registry" / "iptc-photo" / "iptc-photo.json"
VIDEO_REGISTRY = REPO_ROOT / "registry" / "iptc-video" / "iptc-video.json"

HEADER_MACRO_RE = re.compile(r"#define\s+UMM_VERSION_(MAJOR|MINOR|PATCH)\s+(\d+)\b")
KEY_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")


def die(message: str) -> None:
    raise SystemExit(f"generate_release_notes.py: {message}")


def library_version(path: Path = VERSION_HPP) -> str:
    text = path.read_text(encoding="utf-8")
    found: dict[str, str] = {}
    for match in HEADER_MACRO_RE.finditer(text):
        found[match.group(1)] = match.group(2)
    missing = [part for part in ("MAJOR", "MINOR", "PATCH") if part not in found]
    if missing:
        die(f"version.hpp missing UMM_VERSION_* macros: {missing}")
    return f"{found['MAJOR']}.{found['MINOR']}.{found['PATCH']}"


def parse_backends_env(path: Path = BACKENDS_ENV) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip().lstrip("\ufeff")
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            die(f"malformed line in {path}: {raw_line!r}")
        key, value = line.split("=", 1)
        if not KEY_RE.fullmatch(key):
            die(f"invalid key in {path}: {key!r}")
        values[key] = value
    return values


def standard_line(path: Path) -> str:
    data = json.loads(path.read_text(encoding="utf-8"))
    name = data.get("standard")
    version = data.get("standard_version")
    if not name or not version:
        die(f"{path} is missing standard/standard_version")
    return f"- {name} {version}"


def render_notes() -> str:
    version = library_version()
    pins = parse_backends_env()
    standards = "\n".join(
        (standard_line(PHOTO_REGISTRY), standard_line(VIDEO_REGISTRY))
    )
    return f"""# libumm {version}

## Licensing (decision P1)

Binary artifacts that include the Exiv2 backend are conveyed under **GPL-3.0**.
libumm's own source remains **Apache-2.0** (available in the source archive and
the git repository). Dynamic/shared linkage does not change that analysis.

Project source licensing (decision **S1d**, Apache-2.0) is still provisional
until owner confirmation; see `docs/release-checklist.md`.

ExifTool is located at runtime and is **never bundled** (decisions S1a, S1c).
Use `tools/get-exiftool/` in the binary archive to acquire the pinned ExifTool.

## Standards

Reported by `umm::Registry::standards()` (library semver does not encode these):

{standards}

## Backend pins

- Exiv2 {pins['UMM_EXIV2_VERSION']} (statically linked; corresponding source attached)
- ExifTool {pins['UMM_EXIFTOOL_VERSION']} (not redistributed)
- Strawberry Perl {pins['UMM_STRAWBERRY_PERL_VERSION']} (Windows CI/reference only)

Corresponding source for Exiv2, Expat, and zlib is attached as the pinned
upstream tarballs. `SHA256SUMS` covers every release asset.
"""


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument(
        "--version-only",
        action="store_true",
        help="Print the header version triple and exit",
    )
    args = parser.parse_args(argv)
    if args.version_only:
        print(library_version())
        return 0
    text = render_notes()
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8", newline="\n")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
