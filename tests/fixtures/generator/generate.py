#!/usr/bin/env python3
"""Generate the Tier A JPEG + TIFF + PNG + WebP + DNG + video + AVIF + XMP-sidecar corpus.

Uses the pinned ExifTool (and checked-in 16x16 gray bases, or ImageMagick if
the JPEG base is missing) so every committed fixture is synthetic (decision M6).
JPEG corpus: session 09 / test-media-plan §2.2. TIFF: session 17.
PNG + WebP: session 18 (capability-divergent pair). DNG: session 19
(writable-RAW representative; proprietary RAW is deferred to Tier B).
MP4/MOV: session 21 (ffmpeg lavfi color + pinned ExifTool metadata).
AVIF: session 35 (committed 16x16 gray still-picture seed + ExifTool metadata).
HEIC/CR3/JXL are not synthesizable here and live in the Tier B corpus.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
GENERATOR_DIR = Path(__file__).resolve().parent
DEFAULT_OUTPUT_DIR = GENERATOR_DIR.parent
BASE_JPEG_NAME = "base-16x16-gray.jpg"
BASE_TIFF_NAME = "base-16x16-gray.tif"
BASE_PNG_NAME = "base-16x16-gray.png"
BASE_WEBP_NAME = "base-16x16-gray.webp"
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

# C19 / docs/sample-output.txt synthetic layouts (OQ-R1 redaction preserved).
IPHONE_KEYS_DATE = "2019:09:05 14:23:07-04:00"
IPHONE_MOVIE_DATE = "2026:10:04 04:35:02"
IPHONE_MOVIE_MODIFY = "2026:10:04 04:35:03"
IPHONE_GPS = "12.58243889, -98.11848333, 104.65"
HEIC_DTO = "2026:09:01 14:44:19"
HEIC_SUBSEC = "685"
HEIC_OFFSET = "-04:00"
HEIC_GPS_LAT = "23.75188333"
HEIC_GPS_LON = "87.10150833"
PIXEL_IFD0_DTO = "2016:10:25 20:47:28"
PIXEL_IIM_DATE = "2016:10:25"
PIXEL_IIM_TIME = "20:47:28-07:00"
PIXEL_JPEG_DTO = "2016:10:25 13:47:28"
PIXEL_JPEG_SUBSEC = "389696"
PIXEL_PS_DATE = "2016:10:25 13:47:28.3897"
PIXEL_EXIF_LAT = "34.935525"
PIXEL_EXIF_LON = "76.08453333"
PIXEL_XMP_LAT = "34,56.131333N"
PIXEL_XMP_LON = "76,5.0715W"
GOPRO_MOVIE_DATE = "2016:01:07 20:05:15"

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
        "Unregistered/vendor XMP namespace + unknown EXIF tags — unmapped access + preservation"
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
    "tiff/minimal.tif": "No metadata at all — read returns empty, write starts from scratch",
    "tiff/exif-only.tif": "EXIF without IPTC/XMP",
    "tiff/iptc-only.tif": "IPTC-IIM without EXIF/XMP",
    "tiff/xmp-only.tif": "XMP without EXIF/IPTC",
    "tiff/full-agreeing.tif": (
        "All three blocks, values agree — reconciliation \"equivalent\" path"
    ),
    "tiff/full-conflicting.tif": (
        "Dates/creator deliberately differ per block — conflict detection"
    ),
    "tiff/gps.tif": "EXIF GPS IFD + XMP GPS + IPTC named place",
    "tiff/unicode.tif": (
        "Non-ASCII values (UTF-8 XMP, IPTC charset marker) in creator/description"
    ),
    "png/minimal.png": "No metadata at all — read returns empty, write starts from scratch",
    "png/xmp-only.png": "XMP without EXIF/IPTC (both backends)",
    "png/full-agreeing.png": (
        "IPTC+XMP agree — categories both backends can see on PNG"
    ),
    "png/gps.png": (
        "XMP GPS + ExifTool-written EXIF GPS (Exiv2 is EXIF-blind on PNG)"
    ),
    "webp/minimal.webp": "No metadata at all — read returns empty, write starts from scratch",
    "webp/xmp-only.webp": "XMP without EXIF/IPTC (both backends)",
    "webp/full-agreeing.webp": (
        "EXIF+XMP agree — categories both backends can see on WebP (no IPTC)"
    ),
    "avif/minimal.avif": "No metadata at all — read returns empty, write starts from scratch",
    "avif/xmp-only.avif": "XMP without EXIF/IPTC (both backends on AVIF)",
    "avif/full-agreeing.avif": (
        "EXIF+XMP agree — categories both backends can see on AVIF"
    ),
    "avif/gps.avif": "EXIF GPS IFD + XMP GPS on a BMFF still",
    "raw/minimal.dng": (
        "No Phase 1 metadata — writable-RAW representative, write starts from scratch"
    ),
    "raw/full-agreeing.dng": (
        "All three blocks, values agree — DNG reconciliation \"equivalent\" path"
    ),
    "video/minimal.mp4": "No metadata at all — read returns empty, write starts from scratch",
    "video/minimal.mov": "No metadata at all — MOV container counterpart of video/minimal.mp4",
    "video/full.mp4": (
        "QuickTime keys + XMP agree — video reconciliation \"equivalent\" path"
    ),
    "video/gps.mp4": "QuickTime GPSCoordinates + XMP GPS",
    "video/conflicting.mp4": (
        "QuickTime vs XMP date disagreement — video conflict/reconcile path"
    ),
    "video/xmp-shapes.mp4": (
        "XMP-only video shapes (text, lang-alt, uri, structure) for session 38"
    ),
    "video/iphone-style.mov": (
        "C19 iPhone-style MOV: Keys GPSCoordinates with altitude, Keys "
        "CreationDate with offset, Keys Make/Model, movie-header CreateDate "
        "years later"
    ),
    "jpeg/iphone-heic-layout.jpg": (
        "C19 iPhone HEIC EXIF layout on JPEG (HEIC container is Tier B): "
        "DateTimeOriginal + sub-seconds + offset; GPS IFD with ImgDirection, "
        "Speed, HPositioningError"
    ),
    "raw/pixel-style.dng": (
        "C19 Pixel-style DNG: IFD0 DateTimeOriginal and IIM TimeCreated with "
        "an offset"
    ),
    "jpeg/pixel-style.jpg": (
        "C19 Pixel-style JPEG: EXIF GPS and top-level exif:GPS* differing "
        "within tolerance; photoshop:DateCreated 4-digit fraction versus EXIF "
        "6-digit sub-seconds"
    ),
    "video/gopro-style.mp4": (
        "C19 GoPro-style MP4: no Keys, no XMP, wrong movie-header date, GoPro "
        "Model and serial when writable"
    ),
    "tracks/straight.gpx": "Straight-line GPX 1.1 track (three timed trkpt samples)",
    "tracks/nmea.nmea": "NMEA RMC+GGA log matching tracks/straight.gpx",
    "tracks/gaps.gpx": "GPX with duplicate timestamps and a one-hour gap",
    "tracks/straight.kml": "KML gx:Track matching tracks/straight.gpx",
    "tracks/malformed.gpx": "GPX with a non-numeric lat — format_corrupt",
    "tracks/malformed.nmea": "NMEA with bad checksums and no usable positions",
}

TRACK_FILES = (
    "tracks/straight.gpx",
    "tracks/nmea.nmea",
    "tracks/gaps.gpx",
    "tracks/straight.kml",
    "tracks/malformed.gpx",
    "tracks/malformed.nmea",
)


class GeneratorError(RuntimeError):
    """Fixture generation failed closed."""


def copy_hand_authored_tracks(
    output_dir: Path, source_dir: Path = DEFAULT_OUTPUT_DIR
) -> None:
    """Copy session 25 text tracks into output_dir for off-tree regen."""
    for relpath in TRACK_FILES:
        src = source_dir / relpath
        dest = output_dir / relpath
        if not src.is_file():
            raise GeneratorError(f"missing hand-authored track fixture {relpath}")
        dest.parent.mkdir(parents=True, exist_ok=True)
        if src.resolve() != dest.resolve():
            shutil.copyfile(src, dest)


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


def write_constant_gray_tiff(path: Path, width: int = 16, height: int = 16) -> None:
    """Write an uncompressed 8-bit grayscale TIFF filled with mid-gray (128)."""
    n_pixels = width * height
    type_short = 3
    type_long = 4
    type_rational = 5
    ifd_offset = 8
    n_entries = 12
    ifd_size = 2 + n_entries * 12 + 4
    rationals_offset = ifd_offset + ifd_size
    image_offset = rationals_offset + 16

    def entry(tag: int, typ: int, count: int, value: int) -> bytes:
        return struct.pack("<HHII", tag, typ, count, value)

    entries = b"".join(
        [
            entry(256, type_short, 1, width),
            entry(257, type_short, 1, height),
            entry(258, type_short, 1, 8),
            entry(259, type_short, 1, 1),
            entry(262, type_short, 1, 1),
            entry(273, type_long, 1, image_offset),
            entry(277, type_short, 1, 1),
            entry(278, type_short, 1, height),
            entry(279, type_long, 1, n_pixels),
            entry(282, type_rational, 1, rationals_offset),
            entry(283, type_rational, 1, rationals_offset + 8),
            entry(296, type_short, 1, 2),
        ]
    )
    header = b"II" + struct.pack("<HI", 42, ifd_offset)
    ifd = struct.pack("<H", n_entries) + entries + struct.pack("<I", 0)
    rationals = struct.pack("<IIII", 72, 1, 72, 1)
    path.write_bytes(header + ifd + rationals + bytes([128] * n_pixels))


def ensure_base_tiff(base_path: Path) -> Path:
    if base_path.is_file():
        return base_path
    write_constant_gray_tiff(base_path)
    return base_path


def png_chunk(tag: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(tag + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)


def write_constant_gray_png(path: Path, width: int = 16, height: int = 16) -> None:
    """Write an 8-bit grayscale PNG filled with mid-gray (128)."""
    raw = b"".join([b"\x00" + bytes([128] * width) for _ in range(height)])
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"IDAT", zlib.compress(raw, 9))
        + png_chunk(b"IEND", b"")
    )


def ensure_base_png(base_path: Path) -> Path:
    if base_path.is_file():
        return base_path
    write_constant_gray_png(base_path)
    return base_path


class _BitWriter:
    def __init__(self) -> None:
        self.bits = 0
        self.n = 0
        self.buf = bytearray()

    def write(self, value: int, nbits: int) -> None:
        self.bits |= (value & ((1 << nbits) - 1)) << self.n
        self.n += nbits
        while self.n >= 8:
            self.buf.append(self.bits & 0xFF)
            self.bits >>= 8
            self.n -= 8

    def finish(self) -> bytes:
        if self.n:
            self.buf.append(self.bits & 0xFF)
        return bytes(self.buf)


# ffmpeg lavfi `color=c=gray:s=16x16` still-picture via libaom-av1 (309 bytes).
# Bytes are committed so regen does not require libaom on every CI ffmpeg.
BASE_AVIF_BYTES = bytes.fromhex(
    "00000020667479706176696600000000617669666d6966316d6961664d413142"
    "000000f96d657461000000000000002f68646c72000000000000000070696374"
    "0000000000000000000000005069637475726548616e646c6572000000000e70"
    "69746d0000000000010000001e696c6f63000000004400000100010000000100"
    "000121000000140000002869696e660000000000010000001a696e6665020000"
    "000001000061763031436f6c6f72000000006a697072700000004b6970636f00"
    "0000146973706500000000000000100000001000000010706978690000000003"
    "0808080000000c6176314381000c0000000013636f6c726e636c780002000200"
    "02000000001769706d610000000000000001000104010283040000001c6d6461"
    "740a06180cffdb0080320a1800000050000000009c"
)


def write_constant_gray_avif(path: Path) -> None:
    """Write the session 35 16x16 gray AVIF still-picture seed."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(BASE_AVIF_BYTES)


def write_constant_gray_webp(path: Path, width: int = 16, height: int = 16) -> None:
    """Write a lossless VP8L WebP filled with opaque mid-gray (128)."""
    bits = _BitWriter()
    bits.write(0x2F, 8)
    bits.write(width - 1, 14)
    bits.write(height - 1, 14)
    bits.write(0, 1)  # no alpha used
    bits.write(0, 3)  # version
    bits.write(0, 1)  # no transform
    bits.write(0, 1)  # no color cache
    bits.write(0, 1)  # one Huffman group

    def simple_huffman(symbol: int) -> None:
        bits.write(0, 1)  # simple code
        bits.write(0, 1)  # one symbol
        bits.write(1, 1)  # 8-bit first symbol
        bits.write(symbol, 8)

    simple_huffman(128)  # green
    simple_huffman(128)  # red
    simple_huffman(128)  # blue
    simple_huffman(255)  # alpha
    simple_huffman(0)  # distance (unused; all pixels are the same literal)
    payload = bits.finish()
    chunk = b"VP8L" + struct.pack("<I", len(payload)) + payload
    if len(payload) % 2:
        chunk += b"\x00"
    path.write_bytes(b"RIFF" + struct.pack("<I", 4 + len(chunk)) + b"WEBP" + chunk)


def ensure_base_webp(base_path: Path) -> Path:
    if base_path.is_file():
        return base_path
    write_constant_gray_webp(base_path)
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


def find_ffmpeg(explicit: str | None) -> str:
    if explicit:
        return explicit
    env = os.environ.get("UMM_FFMPEG", "").strip()
    if env:
        return env
    found = shutil.which("ffmpeg")
    if found:
        return found
    raise GeneratorError("ffmpeg not found (set --ffmpeg or UMM_FFMPEG)")


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
        # -charset utf8 must precede -@ so ExifTool decodes the argfile as UTF-8.
        # Windows Perl/ExifTool argv uses the system ACP (and cp65001 is unreliable),
        # so Unicode tag values cannot be passed on the command line.
        return [
            self.perl,
            self.script,
            "-config",
            str(self.config),
            "-charset",
            "utf8",
            "-charset",
            "filename=UTF8",
            "-charset",
            "IPTC=UTF8",
        ]

    def run(self, args: list[str]) -> None:
        fd, name = tempfile.mkstemp(prefix="umm-exiftool-", suffix=".args")
        os.close(fd)
        argfile = Path(name)
        try:
            argfile.write_text("\n".join(args) + "\n", encoding="utf-8", newline="\n")
            cmd = self.prefix() + ["-@", str(argfile)]
            result = subprocess.run(cmd, capture_output=True, check=False)
        finally:
            argfile.unlink(missing_ok=True)
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


def try_write_tags(
    tool: ExifTool, dest: Path, tags: list[tuple[str, str]]
) -> list[str] | None:
    """Write tags, returning None when the backend cannot store them."""
    try:
        return write_tags(tool, dest, tags)
    except GeneratorError:
        return None


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


def make_avif(
    tool: ExifTool,
    dest: Path,
    tags: list[tuple[str, str]],
) -> list[str]:
    output_dir = dest.parent.parent
    write_constant_gray_avif(dest)
    commands = [
        "write 16x16 gray AVIF seed (ffmpeg lavfi libaom-av1 still-picture)",
        logical_command(rel_args(strip_all(tool, dest), output_dir)),
    ]
    if tags:
        commands.append(
            logical_command(rel_args(write_tags(tool, dest, tags), output_dir))
        )
    return commands


def make_image(
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


def make_jpeg(
    tool: ExifTool,
    base: Path,
    dest: Path,
    tags: list[tuple[str, str]],
) -> list[str]:
    return make_image(tool, base, dest, tags)


def write_lavfi_video(ffmpeg: str, dest: Path, container: str) -> list[str]:
    dest.parent.mkdir(parents=True, exist_ok=True)
    args = [
        ffmpeg,
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-f",
        "lavfi",
        "-i",
        "color=c=gray:s=16x16:r=10",
        "-t",
        "0.1",
        "-an",
        "-c:v",
        "mpeg4",
        "-q:v",
        "12",
    ]
    if container == "mov":
        args.extend(["-f", "mov", str(dest)])
    else:
        args.extend(["-movflags", "+faststart", str(dest)])
    result = subprocess.run(args, capture_output=True, check=False)
    if result.returncode != 0:
        stderr = result.stderr.decode("utf-8", errors="replace")
        raise GeneratorError(
            f"ffmpeg failed ({result.returncode}): {' '.join(args)}\n{stderr}"
        )
    if not dest.is_file():
        raise GeneratorError(f"ffmpeg did not write {dest}")
    recorded = ["ffmpeg"] + rel_args(args[1:], dest.parent.parent)
    return [" ".join(recorded)]


def make_video(
    tool: ExifTool,
    ffmpeg: str,
    dest: Path,
    tags: list[tuple[str, str]],
    container: str,
) -> list[str]:
    output_dir = dest.parent.parent
    commands = write_lavfi_video(ffmpeg, dest, container)
    commands.append(logical_command(rel_args(strip_all(tool, dest), output_dir)))
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
        "Tier A JPEG + TIFF + PNG + WebP + DNG + MP4/MOV + AVIF + XMP-sidecar + GPS-track corpus from [docs/test-media-plan.md](../../../docs/test-media-plan.md) §2.2–§2.3 and §4.",
        "Generated by `tests/fixtures/generator/generate.py` with the pinned ExifTool (and ffmpeg for video).",
        "GPS tracks under `tracks/` are hand-authored text (session 25) and hashed here; do not edit other entries by hand.",
        "",
        "Each committed media file is generator-produced (decision **M6**); track files are in-repo text. No third-party media.",
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


def generate(
    output_dir: Path,
    tool: ExifTool,
    base: Path,
    tiff_base: Path,
    png_base: Path,
    webp_base: Path,
    ffmpeg: str,
) -> None:
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

    add(
        "tiff/minimal.tif",
        make_image(tool, tiff_base, output_dir / "tiff/minimal.tif", []),
    )
    add(
        "tiff/exif-only.tif",
        make_image(
            tool,
            tiff_base,
            output_dir / "tiff/exif-only.tif",
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
        "tiff/iptc-only.tif",
        make_image(
            tool,
            tiff_base,
            output_dir / "tiff/iptc-only.tif",
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
        "tiff/xmp-only.tif",
        make_image(
            tool,
            tiff_base,
            output_dir / "tiff/xmp-only.tif",
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
        "tiff/full-agreeing.tif",
        make_image(
            tool,
            tiff_base,
            output_dir / "tiff/full-agreeing.tif",
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
        "tiff/full-conflicting.tif",
        make_image(
            tool,
            tiff_base,
            output_dir / "tiff/full-conflicting.tif",
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
        "tiff/gps.tif",
        make_image(
            tool,
            tiff_base,
            output_dir / "tiff/gps.tif",
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
        "tiff/unicode.tif",
        make_image(
            tool,
            tiff_base,
            output_dir / "tiff/unicode.tif",
            [
                ("XMP-dc:Creator", "Jürgen Müller"),
                ("XMP-dc:Description", "café — 日本語"),
                ("IPTC:CodedCharacterSet", "UTF8"),
                ("IPTC:By-line", "Jürgen Müller"),
                ("IPTC:Caption-Abstract", "café — 日本語"),
            ],
        ),
    )

    add(
        "png/minimal.png",
        make_image(tool, png_base, output_dir / "png/minimal.png", []),
    )
    add(
        "png/xmp-only.png",
        make_image(
            tool,
            png_base,
            output_dir / "png/xmp-only.png",
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
        "png/full-agreeing.png",
        make_image(
            tool,
            png_base,
            output_dir / "png/full-agreeing.png",
            [
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
        "png/gps.png",
        make_image(
            tool,
            png_base,
            output_dir / "png/gps.png",
            [
                ("EXIF:GPSLatitude", GPS_LAT),
                ("EXIF:GPSLatitudeRef", "N"),
                ("EXIF:GPSLongitude", GPS_LON),
                ("EXIF:GPSLongitudeRef", "W"),
                ("EXIF:GPSAltitude", "10"),
                ("EXIF:GPSAltitudeRef", "Above Sea Level"),
                ("XMP-exif:GPSLatitude", f"{GPS_LAT}N"),
                ("XMP-exif:GPSLongitude", f"{GPS_LON}W"),
            ],
        ),
    )

    add(
        "webp/minimal.webp",
        make_image(tool, webp_base, output_dir / "webp/minimal.webp", []),
    )
    add(
        "webp/xmp-only.webp",
        make_image(
            tool,
            webp_base,
            output_dir / "webp/xmp-only.webp",
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
        "webp/full-agreeing.webp",
        make_image(
            tool,
            webp_base,
            output_dir / "webp/full-agreeing.webp",
            [
                ("EXIF:Artist", "Agreeing Creator"),
                ("EXIF:Copyright", "Agreeing Copyright"),
                ("EXIF:DateTimeOriginal", DATE_AGREE),
                ("EXIF:CreateDate", DATE_AGREE),
                ("EXIF:ModifyDate", DATE_AGREE),
                ("EXIF:ImageDescription", "Agreeing description"),
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
        "avif/minimal.avif",
        make_avif(tool, output_dir / "avif/minimal.avif", []),
    )
    add(
        "avif/xmp-only.avif",
        make_avif(
            tool,
            output_dir / "avif/xmp-only.avif",
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
        "avif/full-agreeing.avif",
        make_avif(
            tool,
            output_dir / "avif/full-agreeing.avif",
            [
                ("EXIF:Artist", "Agreeing Creator"),
                ("EXIF:Copyright", "Agreeing Copyright"),
                ("EXIF:DateTimeOriginal", DATE_AGREE),
                ("EXIF:CreateDate", DATE_AGREE),
                ("EXIF:ModifyDate", DATE_AGREE),
                ("EXIF:ImageDescription", "Agreeing description"),
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
        "avif/gps.avif",
        make_avif(
            tool,
            output_dir / "avif/gps.avif",
            [
                ("EXIF:GPSLatitude", GPS_LAT),
                ("EXIF:GPSLatitudeRef", "N"),
                ("EXIF:GPSLongitude", GPS_LON),
                ("EXIF:GPSLongitudeRef", "W"),
                ("EXIF:GPSAltitude", "10"),
                ("EXIF:GPSAltitudeRef", "Above Sea Level"),
                ("XMP-exif:GPSLatitude", f"{GPS_LAT}N"),
                ("XMP-exif:GPSLongitude", f"{GPS_LON}W"),
            ],
        ),
    )

    add(
        "raw/minimal.dng",
        make_image(
            tool,
            tiff_base,
            output_dir / "raw/minimal.dng",
            [("EXIF:DNGVersion", "1.4.0.0")],
        ),
    )
    add(
        "raw/full-agreeing.dng",
        make_image(
            tool,
            tiff_base,
            output_dir / "raw/full-agreeing.dng",
            [
                ("EXIF:DNGVersion", "1.4.0.0"),
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
        "video/minimal.mp4",
        make_video(tool, ffmpeg, output_dir / "video/minimal.mp4", [], "mp4"),
    )
    add(
        "video/minimal.mov",
        make_video(tool, ffmpeg, output_dir / "video/minimal.mov", [], "mov"),
    )
    add(
        "video/full.mp4",
        make_video(
            tool,
            ffmpeg,
            output_dir / "video/full.mp4",
            [
                ("ItemList:Title", "Agreeing Title"),
                ("ItemList:Description", "Agreeing description"),
                ("ItemList:Artist", "Agreeing Creator"),
                ("ItemList:Copyright", "Agreeing Copyright"),
                ("Keys:Keywords", "alpha"),
                ("Keys:CreationDate", DATE_AGREE),
                ("XMP-dc:Title", "Agreeing Title"),
                ("XMP-dc:Description", "Agreeing description"),
                ("XMP-dc:Creator", "Agreeing Creator"),
                ("XMP-dc:Rights", "Agreeing Copyright"),
                ("XMP-dc:Subject", "alpha"),
                ("XMP-dc:Subject", "beta"),
                ("XMP-photoshop:DateCreated", DATE_AGREE.replace(" ", "T")),
            ],
            "mp4",
        ),
    )
    add(
        "video/gps.mp4",
        make_video(
            tool,
            ffmpeg,
            output_dir / "video/gps.mp4",
            [
                ("Keys:GPSCoordinates", f"{GPS_LAT}, -{GPS_LON}, 10"),
                ("XMP-exif:GPSLatitude", f"{GPS_LAT}N"),
                ("XMP-exif:GPSLongitude", f"{GPS_LON}W"),
            ],
            "mp4",
        ),
    )
    add(
        "video/conflicting.mp4",
        make_video(
            tool,
            ffmpeg,
            output_dir / "video/conflicting.mp4",
            [
                ("Keys:CreationDate", DATE_EXIF),
                ("XMP-photoshop:DateCreated", DATE_XMP),
            ],
            "mp4",
        ),
    )
    add(
        "video/xmp-shapes.mp4",
        make_video(
            tool,
            ffmpeg,
            output_dir / "video/xmp-shapes.mp4",
            [
                ("XMP-photoshop:Credit", "Shape Credit"),
                ("XMP-iptcExt:Headline", "Shape Headline"),
                ("XMP-iptcCore:AltTextAccessibility", "Shape alt text"),
                ("XMP-plus:DataMining", "http://example.com/data-mining"),
                ("XMP-dc:identifier", "shape-id-1"),
                (
                    "XMP-iptcExt:LocationCreated",
                    "{City=Shape City,CountryName=Shape Country}",
                ),
            ],
            "mp4",
        ),
    )

    add(
        "video/iphone-style.mov",
        make_video(
            tool,
            ffmpeg,
            output_dir / "video/iphone-style.mov",
            [
                ("Keys:CreationDate", IPHONE_KEYS_DATE),
                ("Keys:Make", "Apple"),
                ("Keys:Model", "iPhone X"),
                ("Keys:GPSCoordinates", IPHONE_GPS),
                ("QuickTime:CreateDate", IPHONE_MOVIE_DATE),
                ("QuickTime:ModifyDate", IPHONE_MOVIE_MODIFY),
            ],
            "mov",
        ),
    )
    add(
        "jpeg/iphone-heic-layout.jpg",
        make_jpeg(
            tool,
            base,
            output_dir / "jpeg/iphone-heic-layout.jpg",
            [
                ("EXIF:Make", "Apple"),
                ("EXIF:Model", "iPhone 16 Pro"),
                ("EXIF:LensModel", "iPhone 16 Pro back triple camera 6.765mm f/1.78"),
                ("EXIF:DateTimeOriginal", HEIC_DTO),
                ("EXIF:SubSecTimeOriginal", HEIC_SUBSEC),
                ("EXIF:OffsetTimeOriginal", HEIC_OFFSET),
                ("EXIF:GPSLatitude", HEIC_GPS_LAT),
                ("EXIF:GPSLatitudeRef", "N"),
                ("EXIF:GPSLongitude", HEIC_GPS_LON),
                ("EXIF:GPSLongitudeRef", "W"),
                ("EXIF:GPSAltitude", "12.07893416"),
                ("EXIF:GPSAltitudeRef", "Above Sea Level"),
                ("EXIF:GPSImgDirection", "87.8048401"),
                ("EXIF:GPSImgDirectionRef", "True North"),
                ("EXIF:GPSSpeed", "0.611859844"),
                ("EXIF:GPSSpeedRef", "km/h"),
                ("EXIF:GPSHPositioningError", "5.150825315"),
            ],
        ),
    )
    add(
        "raw/pixel-style.dng",
        make_image(
            tool,
            tiff_base,
            output_dir / "raw/pixel-style.dng",
            [
                ("EXIF:DNGVersion", "1.4.0.0"),
                ("IFD0:DateTimeOriginal", PIXEL_IFD0_DTO),
                ("IPTC:CodedCharacterSet", "UTF8"),
                ("IPTC:DateCreated", PIXEL_IIM_DATE),
                ("IPTC:TimeCreated", PIXEL_IIM_TIME),
            ],
        ),
    )
    add(
        "jpeg/pixel-style.jpg",
        make_jpeg(
            tool,
            base,
            output_dir / "jpeg/pixel-style.jpg",
            [
                ("EXIF:DateTimeOriginal", PIXEL_JPEG_DTO),
                ("EXIF:SubSecTimeOriginal", PIXEL_JPEG_SUBSEC),
                ("EXIF:GPSLatitude", PIXEL_EXIF_LAT),
                ("EXIF:GPSLatitudeRef", "N"),
                ("EXIF:GPSLongitude", PIXEL_EXIF_LON),
                ("EXIF:GPSLongitudeRef", "W"),
                ("EXIF:GPSAltitude", "0"),
                ("EXIF:GPSAltitudeRef", "Above Sea Level"),
                ("XMP-exif:GPSLatitude", PIXEL_XMP_LAT),
                ("XMP-exif:GPSLongitude", PIXEL_XMP_LON),
                ("XMP-exif:GPSAltitude", "0"),
                ("XMP-exif:GPSAltitudeRef", "0"),
                ("XMP-photoshop:DateCreated", PIXEL_PS_DATE),
                ("XMP-xmp:CreateDate", PIXEL_PS_DATE),
            ],
        ),
    )
    gopro_path = output_dir / "video/gopro-style.mp4"
    gopro_commands = make_video(
        tool,
        ffmpeg,
        gopro_path,
        [
            ("QuickTime:CreateDate", GOPRO_MOVIE_DATE),
            ("QuickTime:ModifyDate", GOPRO_MOVIE_DATE),
        ],
        "mp4",
    )
    gopro_extra = try_write_tags(
        tool,
        gopro_path,
        [
            ("GoPro:Model", "HERO12 Black"),
            ("GoPro:CameraSerialNumber", "C0000000000000"),
            ("UserData:LensSerialNumber", "LSU0000000000000"),
        ],
    )
    if gopro_extra:
        gopro_commands.append(logical_command(rel_args(gopro_extra, output_dir)))
    add("video/gopro-style.mp4", gopro_commands)

    copy_hand_authored_tracks(output_dir)
    for relpath in TRACK_FILES:
        add(relpath, ["hand-authored (session 25; test-media-plan §4)"])

    write_manifest(
        output_dir,
        entries,
        open_items=[
            "`jpeg/makernote.jpg` is not synthesizable (decision M6). Closed by "
            "Tier B sample `jpeg-makernote` in `tests/corpus/manifest.json` "
            "(session 27); never committed.",
            "Proprietary RAW (RAF/RW2/SR2-class) is not synthesizable small "
            "(decision M6). Closed by Tier B sample `raw-panasonic-rw2` in "
            "`tests/corpus/manifest.json` (session 27); never committed.",
            "HEIC/HEIF stills are not synthesizable with the pinned ffmpeg "
            "(no heif muxer). Closed by Tier B sample `heic-quicktime` in "
            "`tests/corpus/manifest.json` (session 35); never committed. "
            "Session 51 reproduces the iPhone HEIC EXIF layout on "
            "`jpeg/iphone-heic-layout.jpg`.",
            "CR3 is not synthesizable small (decision M6). Closed by Tier B "
            "sample `raw-canon-cr3` in `tests/corpus/manifest.json` (session 35); "
            "never committed.",
            "JPEG XL is not generated in Tier A (libjxl not pinned). Closed by "
            "Tier B sample `jxl-codestream` in `tests/corpus/manifest.json` "
            "(session 35); never committed.",
        ],
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--exiftool", default=None, help="Pinned ExifTool script")
    parser.add_argument("--perl", default=None, help="Perl interpreter")
    parser.add_argument("--ffmpeg", default=None, help="ffmpeg binary (session 21)")
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
        ffmpeg = find_ffmpeg(args.ffmpeg)
        base = ensure_base_jpeg(GENERATOR_DIR / BASE_JPEG_NAME)
        tiff_base = ensure_base_tiff(GENERATOR_DIR / BASE_TIFF_NAME)
        png_base = ensure_base_png(GENERATOR_DIR / BASE_PNG_NAME)
        webp_base = ensure_base_webp(GENERATOR_DIR / BASE_WEBP_NAME)
        tool = ExifTool(perl, script, GENERATOR_DIR / CONFIG_NAME)
        generate(output_dir, tool, base, tiff_base, png_base, webp_base, ffmpeg)
    except (GeneratorError, subprocess.CalledProcessError) as exc:
        print(f"generate.py: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
