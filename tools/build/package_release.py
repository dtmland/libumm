#!/usr/bin/env python3
"""Assemble a per-OS libumm release archive from an install prefix (session 34).

The prefix is produced by `cmake --install` (headers, library, CMake package,
LICENSE/NOTICE/THIRD-PARTY-NOTICES, licenses/). This script copies that tree,
adds tools/get-exiftool/ (scripts + backends.env pin data), docs/abi-policy.md,
and README.md, then writes a .tar.gz plus a path manifest.
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import sys
import tarfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

REQUIRED_PREFIX_FILES = (
    "include/umm/umm.hpp",
    "include/umm/version.hpp",
)

REQUIRED_PREFIX_NAMES = (
    "ummConfig.cmake",
    "LICENSE",
    "NOTICE.md",
    "THIRD-PARTY-NOTICES.md",
    "GPL-3.0.txt",
)

LIBRARY_NAMES = (
    "libumm.a",
    "libumm.lib",
    "umm.lib",
    "libumm.dylib",
    "umm.dll",
)

EXTRA_FILES = (
    ("tools/get-exiftool/install.sh", "tools/get-exiftool/install.sh"),
    ("tools/get-exiftool/install.ps1", "tools/get-exiftool/install.ps1"),
    ("tools/build/backends.env", "tools/get-exiftool/backends.env"),
    ("docs/abi-policy.md", "docs/abi-policy.md"),
    ("README.md", "README.md"),
)


def die(message: str) -> None:
    raise SystemExit(f"package_release.py: {message}")


def iter_files(root: Path) -> list[Path]:
    files = [path for path in root.rglob("*") if path.is_file()]
    files.sort()
    return files


def require_prefix(prefix: Path) -> None:
    if not prefix.is_dir():
        die(f"install prefix is not a directory: {prefix}")
    for rel in REQUIRED_PREFIX_FILES:
        path = prefix / rel
        if not path.is_file():
            die(f"install prefix missing {rel}")
    names = {path.name for path in iter_files(prefix)}
    missing = [name for name in REQUIRED_PREFIX_NAMES if name not in names]
    if missing:
        die(f"install prefix missing {', '.join(missing)}")
    if not any(name in names for name in LIBRARY_NAMES) and not any(
        name.startswith("libumm.so") for name in names
    ):
        die("install prefix missing libumm library archive")


def copy_prefix(prefix: Path, staging: Path) -> None:
    shutil.copytree(prefix, staging, dirs_exist_ok=True, symlinks=False)


def copy_extras(source_root: Path, staging: Path) -> None:
    for src_rel, dest_rel in EXTRA_FILES:
        src = source_root / src_rel
        if not src.is_file():
            die(f"missing extra file: {src_rel}")
        dest = staging / dest_rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)


def write_manifest(staging: Path, archive_root_name: str) -> None:
    lines = []
    for path in iter_files(staging):
        rel = path.relative_to(staging).as_posix()
        lines.append(f"{archive_root_name}/{rel}")
    (staging / "MANIFEST.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def make_archive(staging: Path, archive_root_name: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(dest, "w:gz") as tar:
        tar.add(staging, arcname=archive_root_name)


def package(
    prefix: Path,
    source_root: Path,
    version: str,
    os_id: str,
    output_dir: Path,
) -> Path:
    require_prefix(prefix)
    archive_root_name = f"libumm-{version}"
    archive_name = f"libumm-{version}-{os_id}.tar.gz"
    output_dir.mkdir(parents=True, exist_ok=True)
    staging_parent = output_dir / f".staging-{os_id}"
    if staging_parent.exists():
        shutil.rmtree(staging_parent)
    staging = staging_parent / archive_root_name
    staging.mkdir(parents=True)
    try:
        copy_prefix(prefix, staging)
        copy_extras(source_root, staging)
        write_manifest(staging, archive_root_name)
        archive = output_dir / archive_name
        make_archive(staging, archive_root_name, archive)
        manifest_copy = output_dir / f"{archive_name}.manifest.txt"
        shutil.copy2(staging / "MANIFEST.txt", manifest_copy)
    finally:
        shutil.rmtree(staging_parent)
    return archive


def write_sha256sums(directory: Path, output: Path) -> None:
    files = [
        path
        for path in directory.iterdir()
        if path.is_file() and path.name != output.name and not path.name.endswith(".manifest.txt")
    ]
    files.sort(key=lambda path: path.name)
    if not files:
        die(f"no files to checksum in {directory}")
    lines = []
    for path in files:
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        lines.append(f"{digest}  {path.name}")
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    pkg = sub.add_parser("package", help="Build a per-OS archive from an install prefix")
    pkg.add_argument("--install-prefix", type=Path, required=True)
    pkg.add_argument("--source-root", type=Path, default=REPO_ROOT)
    pkg.add_argument("--version", required=True)
    pkg.add_argument("--os", required=True, dest="os_id")
    pkg.add_argument("--output-dir", type=Path, required=True)

    sums = sub.add_parser("sha256sums", help="Write SHA256SUMS for every file in a directory")
    sums.add_argument("--dir", type=Path, required=True)
    sums.add_argument("--output", type=Path, required=True)

    args = parser.parse_args(argv)
    if args.command == "package":
        archive = package(
            args.install_prefix,
            args.source_root,
            args.version,
            args.os_id,
            args.output_dir,
        )
        print(archive)
        return 0
    write_sha256sums(args.dir, args.output)
    print(args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
