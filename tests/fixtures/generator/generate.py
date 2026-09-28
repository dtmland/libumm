#!/usr/bin/env python3
"""Generate the Tier A JPEG + XMP-sidecar fixture corpus (session 09).

Uses the pinned ExifTool (and a checked-in 16x16 gray JPEG, or ImageMagick if
the base is missing) so every committed fixture is synthetic (decision M6).
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
GENERATOR_DIR = Path(__file__).resolve().parent
DEFAULT_OUTPUT_DIR = GENERATOR_DIR.parent
BASE_JPEG_NAME = "base-16x16-gray.jpg"
CONFIG_NAME = "exiftool.config"
UNICODE_FILENAME = "übüng ünïcode.jpg"

# Fixed payload timestamps (not filesystem times).
DATE_AGREE = "2020:01:02 03:04:05"
DATE_EXIF = "2020:01:01 00:00:00"
DATE_IPTC = "2020:02:02"
TIME_IPTC = "00:00:00"
DATE_XMP = "2020:03:03T00:00:00"
DATE_EMBEDDED = "2020:01:01 00:00:00"
DATE_SIDECAR = "2021:02:02T00:00:00"

GPS_LAT = "37.7749"
GPS_LON = "122.4194"

# Truncated JPEG keeps SOI + APP0 so it is recognizably JPEG-but-corrupt.
TRUNCATE_BYTES = 64

# test-media-plan.md §2.2 — purpose text is copied into MANIFEST.md.
PURPOSES = {
    "jpeg/minimal.jpg": "No metadata at all — read returns empty, write starts from scratch",
    "jpeg/exif-only.jpg": "EXIF without IPTC/XMP",
    "jpeg/iptc-only.jpg": "IPTC-IIM without EXIF/XMP",
    "jpeg/xmp-only.jpg": "XMP without EXIF/IPTC",
    "jpeg/full-agreeing.jpg": (
        "All three blocks, values agree — reconciliation \"equivalent\" path"
    ),
    "jpeg/full-conflicting.jpg": (
        "Dates/creator deliberately differ per block — conflict detection"
    ),
    "jpeg/gps.jpg": "EXIF GPS IFD + XMP GPS + IPTC named place",
    "jpeg/unicode.jpg": (
        "Non-ASCII values (UTF-8 XMP, IPTC charset marker) in creator/description"
    ),
    "jpeg/makernote.jpg": "A vendor MakerNote blob — write-preservation test input",
    "jpeg/unknown-tags.jpg": (
        "Unregistered/vendor XMP namespace + unknown EXIF tags — raw access + preservation"
    ),
    "sidecar/paired.jpg": (
        "Embedded + sidecar as one asset; sidecar/embedded conflict variant (embedded side)"
    ),
    "sidecar/paired.xmp": (
        "Embedded + sidecar as one asset; sidecar/embedded conflict variant (sidecar side)"
    ),
    "sidecar/orphan.xmp": "Sidecar with no media file",
    f"naming/{UNICODE_FILENAME}": "Non-ASCII filename (Windows path encoding)",
    "corrupt/truncated.jpg": "Error-path input",
}


class GeneratorError(RuntimeError):
    """Fixture generation failed closed."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_constant_gray_jpeg(path: Path, width: int = 16, height: int = 16) -> None:
    """Write a baseline 8-bit grayscale JPEG filled with mid-gray (128)."""
    if width % 8 or height % 8:
        raise GeneratorError("JPEG dimensions must be multiples of 8")

    # Minimal Huffman tables: DC category 0 and AC EOB each get a 1-bit code.
    dc_bits = [1] + [0] * 15
    dc_vals = [0]
    ac_bits = [1] + [0] * 15
    ac_vals = [0x00]

    def marker(code: int, payload: bytes = b"") -> bytes:
        if not payload:
            return bytes([0xFF, code])
        length = len(payload) + 2
        return bytes([0xFF, code, length >> 8, length & 0xFF]) + payload

    def huff_table(is_ac: bool, ident: int, bits: list[int], vals: list[int]) -> bytes:
        return marker(
            0xC4,
            bytes([((0x10 if is_ac else 0) | ident)]) + bytes(bits) + bytes(vals),
        )

    class BitWriter:
        def __init__(self) -> None:
            self.buf = bytearray()
            self.acc = 0
            self.nbits = 0

        def write_bit(self, bit: int) -> None:
            self.acc = (self.acc << 1) | (bit & 1)
            self.nbits += 1
            if self.nbits == 8:
                self.buf.append(self.acc)
                if self.acc == 0xFF:
                    self.buf.append(0x00)
                self.acc = 0
                self.nbits = 0

        def flush(self) -> None:
            while self.nbits:
                self.write_bit(1)

    writer = BitWriter()
    blocks = (width // 8) * (height // 8)
    for _ in range(blocks):
        writer.write_bit(0)  # DC category 0
        writer.write_bit(0)  # AC EOB
    writer.flush()

    app0 = b"JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00"
    dqt = marker(0xDB, bytes([0x00]) + bytes([1] * 64))
    sof0 = marker(
        0xC0,
        bytes(
            [
                8,
                height >> 8,
                height & 0xFF,
                width >> 8,
                width & 0xFF,
                1,
                1,
                0x11,
                0,
            ]
        ),
    )
    sos = marker(0xDA, bytes([1, 1, 0x00, 0, 63, 0]))
    path.write_bytes(
        bytes([0xFF, 0xD8])
        + marker(0xE0, app0)
        + dqt
        + sof0
        + huff_table(False, 0, dc_bits, dc_vals)
        + huff_table(True, 0, ac_bits, ac_vals)
        + sos
        + bytes(writer.buf)
        + bytes([0xFF, 0xD9])
    )


def ensure_base_jpeg(base_path: Path) -> Path:
    if base_path.is_file():
        return base_path
    magick = shutil.which("magick")
    convert = shutil.which("convert")
    tool = magick or convert
    if tool:
        subprocess.run(
            [tool, "-size", "16x16", "xc:gray", str(base_path)],
            check=True,
        )
        if not base_path.is_file():
            raise GeneratorError(f"ImageMagick did not write {base_path}")
        return base_path
    write_constant_gray_jpeg(base_path)
    return base_path


def find_perl(explicit: str | None) -> str:
    if explicit:
        return explicit
    env = os.environ.get("UMM_PERL_EXECUTABLE", "").strip()
    if env:
        return env
    found = shutil.which("perl")
    if found:
        return found
    raise GeneratorError("perl not found (set --perl or UMM_PERL_EXECUTABLE)")


def find_exiftool(explicit: str | None, repo_root: Path) -> str:
    if explicit:
        return explicit
    env = os.environ.get("UMM_EXIFTOOL_SCRIPT", "").strip()
    if env:
        return env
    cached = repo_root / ".cache" / "exiftool" / "src" / "exiftool"
    if cached.is_file():
        return str(cached)
    raise GeneratorError(
        "pinned ExifTool not found (set --exiftool or UMM_EXIFTOOL_SCRIPT)"
    )


class ExifTool:
    def __init__(self, perl: str, script: str, config: Path) -> None:
        self.perl = perl
        self.script = script
        self.config = config
        if not Path(script).is_file():
            raise GeneratorError(f"ExifTool script not found: {script}")
        if not config.is_file():
            raise GeneratorError(f"ExifTool config not found: {config}")

    def prefix(self) -> list[str]:
        return [
            self.perl,
            self.script,
            "-config",
            str(self.config),
            "-charset",
            "filename=UTF8",
            "-charset",
            "IPTC=UTF8",
        ]

    def run(self, args: list[str]) -> None:
        cmd = self.prefix() + args
        result = subprocess.run(cmd, capture_output=True, check=False)
        if result.returncode != 0:
            stderr = result.stderr.decode("utf-8", errors="replace")
            stdout = result.stdout.decode("utf-8", errors="replace")
            raise GeneratorError(
                f"exiftool failed ({result.returncode}): {' '.join(args)}\n"
                f"{stdout}\n{stderr}"
            )


def logical_command(args: list[str]) -> str:
    return "exiftool " + " ".join(args)


def rel_args(args: list[str], output_dir: Path) -> list[str]:
    recorded: list[str] = []
    for arg in args:
        path = Path(arg)
        if path.is_absolute():
            try:
                recorded.append(path.relative_to(output_dir).as_posix())
                continue
            except ValueError:
                recorded.append(path.name)
                continue
        recorded.append(arg)
    return recorded


def copy_base(base: Path, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(base, dest)


def strip_all(tool: ExifTool, dest: Path) -> list[str]:
    args = ["-overwrite_original", "-all=", str(dest)]
    tool.run(args)
    return args


def write_tags(tool: ExifTool, dest: Path, tags: list[tuple[str, str]]) -> list[str]:
    args = ["-overwrite_original"]
    for name, value in tags:
        args.append(f"-{name}={value}")
    args.append(str(dest))
    tool.run(args)
    return args


def export_xmp(tool: ExifTool, source: Path, dest: Path) -> list[str]:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists():
        dest.unlink()
    args = ["-o", str(dest), "-XMP:all", str(source)]
    tool.run(args)
    if dest.exists():
        text = dest.read_text(encoding="utf-8")
        dest.write_text(text.replace("\r\n", "\n").replace("\r", "\n"), encoding="utf-8")
    return args


def make_jpeg(
    tool: ExifTool,
    base: Path,
    dest: Path,
    tags: list[tuple[str, str]],
) -> list[str]:
    output_dir = dest.parent.parent
    copy_base(base, dest)
    commands = [logical_command(rel_args(strip_all(tool, dest), output_dir))]
    if tags:
        commands.append(
            logical_command(rel_args(write_tags(tool, dest, tags), output_dir))
        )
    return commands


def write_manifest(
    output_dir: Path,
    entries: list[tuple[str, list[str]]],
    open_items: list[str],
) -> None:
    lines = [
        "# Fixture corpus manifest",
        "",
        "Tier A JPEG + XMP-sidecar corpus from [docs/test-media-plan.md](../../../docs/test-media-plan.md) §2.2.",
        "Generated by `tests/fixtures/generator/generate.py` with the pinned ExifTool.",
        "Do not edit by hand.",
        "",
        "Each committed file is generator-produced (decision **M6**); no third-party media.",
        "",
        "## Open items",
        "",
    ]
    if open_items:
        for item in open_items:
            lines.append(f"- {item}")
    else:
        lines.append("- (none)")
    lines.extend(["", "## Files", ""])
    for relpath, commands in entries:
        path = output_dir / relpath
        digest = sha256_file(path)
        size = path.stat().st_size
        purpose = PURPOSES[relpath]
        command = " && ".join(commands)
        lines.extend(
            [
                f"### `{relpath}`",
                "",
                f"- **SHA-256:** `{digest}`",
                f"- **Size:** {size} bytes",
                f"- **Purpose:** {purpose}",
                f"- **Command:** `{command}`",
                "",
            ]
        )
    (output_dir / "MANIFEST.md").write_text("\n".join(lines), encoding="utf-8")


def generate(output_dir: Path, tool: ExifTool, base: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    entries: list[tuple[str, list[str]]] = []

    def add(relpath: str, commands: list[str]) -> None:
        entries.append((relpath, commands))

    add(
        "jpeg/minimal.jpg",
        make_jpeg(tool, base, output_dir / "jpeg/minimal.jpg", []),
    )

    add(
        "jpeg/exif-only.jpg",
        make_jpeg(
            tool,
            base,
            output_dir / "jpeg/exif-only.jpg",
            [
                ("EXIF:Artist", "EXIF Artist"),
                ("EXIF:Copyright", "EXIF Copyright"),
                ("EXIF:DateTimeOriginal", DATE_AGREE),
                ("EXIF:CreateDate", DATE_AGREE),
                ("EXIF:ModifyDate", DATE_AGREE),
            ],
        ),
    )

    add(
        "jpeg/iptc-only.jpg",
        make_jpeg(
            tool,
            base,
            output_dir / "jpeg/iptc-only.jpg",
            [
                ("IPTC:CodedCharacterSet", "UTF8"),
                ("IPTC:By-line", "IPTC Byline"),
                ("IPTC:Caption-Abstract", "IPTC caption"),
                ("IPTC:ObjectName", "IPTC headline"),
                ("IPTC:CopyrightNotice", "IPTC copyright"),
                ("IPTC:DateCreated", DATE_AGREE[:10]),
                ("IPTC:TimeCreated", DATE_AGREE[11:]),
            ],
        ),
    )

    add(
        "jpeg/xmp-only.jpg",
        make_jpeg(
            tool,
            base,
            output_dir / "jpeg/xmp-only.jpg",
            [
                ("XMP-dc:Creator", "XMP Creator"),
                ("XMP-dc:Description", "XMP description"),
                ("XMP-dc:Title", "XMP title"),
                ("XMP-dc:Rights", "XMP copyright"),
                ("XMP-photoshop:DateCreated", DATE_AGREE.replace(" ", "T")),
            ],
        ),
    )

    add(
        "jpeg/full-agreeing.jpg",
        make_jpeg(
            tool,
            base,
            output_dir / "jpeg/full-agreeing.jpg",
            [
                ("EXIF:Artist", "Agreeing Creator"),
                ("EXIF:Copyright", "Agreeing Copyright"),
                ("EXIF:DateTimeOriginal", DATE_AGREE),
                ("EXIF:CreateDate", DATE_AGREE),
                ("EXIF:ModifyDate", DATE_AGREE),
                ("IPTC:CodedCharacterSet", "UTF8"),
                ("IPTC:By-line", "Agreeing Creator"),
                ("IPTC:Caption-Abstract", "Agreeing description"),
                ("IPTC:ObjectName", "Agreeing headline"),
                ("IPTC:CopyrightNotice", "Agreeing Copyright"),
                ("IPTC:Keywords", "alpha"),
                ("IPTC:Keywords", "beta"),
                ("IPTC:City", "Agreeing City"),
                ("IPTC:DateCreated", DATE_AGREE[:10]),
                ("IPTC:TimeCreated", DATE_AGREE[11:]),
                ("XMP-dc:Creator", "Agreeing Creator"),
                ("XMP-dc:Description", "Agreeing description"),
                ("XMP-dc:Title", "Agreeing headline"),
                ("XMP-dc:Rights", "Agreeing Copyright"),
                ("XMP-dc:Subject", "alpha"),
                ("XMP-dc:Subject", "beta"),
                ("XMP-photoshop:City", "Agreeing City"),
                ("XMP-photoshop:DateCreated", DATE_AGREE.replace(" ", "T")),
            ],
        ),
    )

    add(
        "jpeg/full-conflicting.jpg",
        make_jpeg(
            tool,
            base,
            output_dir / "jpeg/full-conflicting.jpg",
            [
                ("EXIF:Artist", "EXIF Creator"),
                ("EXIF:DateTimeOriginal", DATE_EXIF),
                ("EXIF:CreateDate", DATE_EXIF),
                ("EXIF:ModifyDate", DATE_EXIF),
                ("IPTC:CodedCharacterSet", "UTF8"),
                ("IPTC:By-line", "IPTC Creator"),
                ("IPTC:DateCreated", DATE_IPTC),
                ("IPTC:TimeCreated", TIME_IPTC),
                ("XMP-dc:Creator", "XMP Creator"),
                ("XMP-photoshop:DateCreated", DATE_XMP),
            ],
        ),
    )

    add(
        "jpeg/gps.jpg",
        make_jpeg(
            tool,
            base,
            output_dir / "jpeg/gps.jpg",
            [
                ("EXIF:GPSLatitude", GPS_LAT),
                ("EXIF:GPSLatitudeRef", "N"),
                ("EXIF:GPSLongitude", GPS_LON),
                ("EXIF:GPSLongitudeRef", "W"),
                ("EXIF:GPSAltitude", "10"),
                ("EXIF:GPSAltitudeRef", "Above Sea Level"),
                ("XMP-exif:GPSLatitude", f"{GPS_LAT}N"),
                ("XMP-exif:GPSLongitude", f"{GPS_LON}W"),
                ("IPTC:CodedCharacterSet", "UTF8"),
                ("IPTC:City", "San Francisco"),
                ("IPTC:Province-State", "California"),
                ("IPTC:Country-PrimaryLocationName", "United States"),
            ],
        ),
    )

    add(
        "jpeg/unicode.jpg",
        make_jpeg(
            tool,
            base,
            output_dir / "jpeg/unicode.jpg",
            [
                ("XMP-dc:Creator", "Jürgen Müller"),
                ("XMP-dc:Description", "café — 日本語"),
                ("IPTC:CodedCharacterSet", "UTF8"),
                ("IPTC:By-line", "Jürgen Müller"),
                ("IPTC:Caption-Abstract", "café — 日本語"),
            ],
        ),
    )

    stale_makernote = output_dir / "jpeg/makernote.jpg"
    if stale_makernote.exists():
        stale_makernote.unlink()

    add(
        "jpeg/unknown-tags.jpg",
        make_jpeg(
            tool,
            base,
            output_dir / "jpeg/unknown-tags.jpg",
            [
                ("XMP-libummtest:UnknownWidget", "vendor-widget"),
                ("EXIF:LibummUnknownExif", "vendor-exif"),
            ],
        ),
    )

    paired_jpg = output_dir / "sidecar/paired.jpg"
    add(
        "sidecar/paired.jpg",
        make_jpeg(
            tool,
            base,
            paired_jpg,
            [
                ("EXIF:Artist", "Embedded Creator"),
                ("EXIF:DateTimeOriginal", DATE_EMBEDDED),
                ("XMP-dc:Creator", "Embedded Creator"),
                ("XMP-photoshop:DateCreated", DATE_EMBEDDED.replace(" ", "T")),
            ],
        ),
    )

    sidecar_src = output_dir / "sidecar/_sidecar_src.jpg"
    make_jpeg(
        tool,
        base,
        sidecar_src,
        [
            ("XMP-dc:Creator", "Sidecar Creator"),
            ("XMP-photoshop:DateCreated", DATE_SIDECAR),
        ],
    )
    paired_xmp = output_dir / "sidecar/paired.xmp"
    add(
        "sidecar/paired.xmp",
        [
            "exiftool -overwrite_original "
            f"-XMP-dc:Creator=Sidecar Creator "
            f"-XMP-photoshop:DateCreated={DATE_SIDECAR} "
            "then exiftool -o sidecar/paired.xmp -XMP:all"
        ],
    )
    export_xmp(tool, sidecar_src, paired_xmp)
    sidecar_src.unlink()

    orphan_src = output_dir / "sidecar/_orphan_src.jpg"
    make_jpeg(
        tool,
        base,
        orphan_src,
        [
            ("XMP-dc:Creator", "Orphan Creator"),
            ("XMP-dc:Description", "Orphan sidecar"),
        ],
    )
    orphan_xmp = output_dir / "sidecar/orphan.xmp"
    add(
        "sidecar/orphan.xmp",
        [
            "exiftool -overwrite_original "
            "-XMP-dc:Creator=Orphan Creator "
            "-XMP-dc:Description=Orphan sidecar "
            "then exiftool -o sidecar/orphan.xmp -XMP:all"
        ],
    )
    export_xmp(tool, orphan_src, orphan_xmp)
    orphan_src.unlink()

    unicode_dir = output_dir / "naming"
    unicode_dir.mkdir(parents=True, exist_ok=True)
    unicode_dest = unicode_dir / UNICODE_FILENAME
    shutil.copyfile(output_dir / "jpeg/unicode.jpg", unicode_dest)
    add(
        f"naming/{UNICODE_FILENAME}",
        [f"copy jpeg/unicode.jpg naming/{UNICODE_FILENAME}"],
    )

    truncated = output_dir / "corrupt/truncated.jpg"
    truncated.parent.mkdir(parents=True, exist_ok=True)
    data = (output_dir / "jpeg/minimal.jpg").read_bytes()[:TRUNCATE_BYTES]
    truncated.write_bytes(data)
    add(
        "corrupt/truncated.jpg",
        [f"copy jpeg/minimal.jpg and keep first {TRUNCATE_BYTES} bytes"],
    )

    write_manifest(
        output_dir,
        entries,
        open_items=[
            "`jpeg/makernote.jpg` deferred to Stage 4: ExifTool cannot create a "
            "vendor MakerNote structure from scratch without a camera-authored "
            "template (decision M6 forbids committing a third-party camera file).",
        ],
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--exiftool", default=None, help="Pinned ExifTool script")
    parser.add_argument("--perl", default=None, help="Perl interpreter")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    repo_root = args.repo_root.resolve()
    output_dir = args.output_dir
    if not output_dir.is_absolute():
        output_dir = (Path.cwd() / output_dir).resolve()
    try:
        perl = find_perl(args.perl)
        script = find_exiftool(args.exiftool, repo_root)
        base = ensure_base_jpeg(GENERATOR_DIR / BASE_JPEG_NAME)
        tool = ExifTool(perl, script, GENERATOR_DIR / CONFIG_NAME)
        generate(output_dir, tool, base)
    except (GeneratorError, subprocess.CalledProcessError) as exc:
        print(f"generate.py: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
