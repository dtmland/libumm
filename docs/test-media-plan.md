# Test media plan

Status: planning. Implements decision **M6** in
[docs/analysis/2026-09-27-plan-review-and-decisions.md](analysis/2026-09-27-plan-review-and-decisions.md):
**hybrid strategy — generate tiny synthetic fixtures as the in-repo default; use pinned,
checksummed downloads of third-party samples for Tier B; never copy third-party corpora into the
repo.**

Goals: fixtures small enough that checkout size and build time are unaffected, legally clean under
the project's Apache-2.0 license, deterministic, and sufficient to exercise every function of the
metadata API.

---

## 1. Why generated fixtures are the primary source

Metadata testing does not need photographic content. A 1×1 or 16×16 pixel image with a full EXIF +
IPTC-IIM + XMP payload tests exactly the same code paths as a 40 MB photo. Generated fixtures are:

- **tiny** — a 1×1 JPEG with rich metadata is ~2–20 KB;
- **deterministic** — regenerable from a script, diffable expectations;
- **legally clean** — no third-party license attaches;
- **precise** — each fixture can isolate one scenario (e.g. "EXIF date disagrees with XMP date").

Existing third-party corpora were evaluated and rejected for in-repo use:

| Source | Why not copied in-repo |
|---|---|
| ExifTool `t/images` | Artistic/GPL licensed with the ExifTool distribution; copying into an Apache-2.0 repo is a licensing trap. Fine to *reference behavior against* locally. |
| Exiv2 `test/data` | Same class of problem (GPL project). |
| Camera-sample sites (rawsamples, imaging-resource, etc.) | Unclear/large files; unsuitable for git. |

Third-party samples remain valuable for **Tier B** cross-backend verification — acquired at test
time by checksum-pinned download, never committed.

## 2. In-repo fixture corpus (Tier A)

Location: `tests/fixtures/` with a generator under `tests/fixtures/generator/`.

### 2.1 Generation tooling

- **Base pixels:** generate minimal valid images with ImageMagick (`magick -size 16x16 xc:gray
  fixture.jpg`) or a tiny checked-in byte-exact base file produced once by the generator.
- **Metadata injection:** use the *pinned* ExifTool to write EXIF/IPTC/XMP payloads
  (`exiftool -Artist=... -IPTC:City=... -XMP-dc:creator=... fixture.jpg`), and Exiv2's `exiv2` /
  API for Exiv2-authored variants. Using both authors matters: it produces fixtures with each
  backend's real-world quirks.
- **Sidecars:** ExifTool `-o fixture.xmp` creates XMP sidecars from scratch (it has `c` capability
  for XMP).
- **Determinism:** fix all timestamps in the payload; regeneration must be byte-stable or the
  suite compares parsed metadata rather than bytes.

The generator script is committed; the generated fixtures are **also committed** (so tests do not
require ImageMagick at test time), with a CI contract test that regenerating produces
metadata-equivalent output.

### 2.2 Fixture matrix (Phase 1: JPEG + XMP sidecar)

Each ≤ ~25 KB; whole Phase-1 corpus target **< 1 MB**:

| Fixture | Purpose |
|---|---|
| `jpeg/minimal.jpg` | No metadata at all — read returns empty, write starts from scratch |
| `jpeg/exif-only.jpg` | EXIF without IPTC/XMP |
| `jpeg/iptc-only.jpg` | IPTC-IIM without EXIF/XMP |
| `jpeg/xmp-only.jpg` | XMP without EXIF/IPTC |
| `jpeg/full-agreeing.jpg` | All three blocks, values agree — reconciliation "equivalent" path |
| `jpeg/full-conflicting.jpg` | Dates/creator deliberately differ per block — conflict detection |
| `jpeg/gps.jpg` | EXIF GPS IFD + XMP GPS + IPTC named place |
| `jpeg/unicode.jpg` | Non-ASCII values (UTF-8 XMP, IPTC charset marker) in creator/description |
| `jpeg/makernote.jpg` | A vendor MakerNote blob — write-preservation test input |
| `jpeg/unknown-tags.jpg` | Unregistered/vendor XMP namespace + unknown EXIF tags — unmapped access + preservation |
| `sidecar/paired.jpg` + `sidecar/paired.xmp` | Embedded + sidecar as one asset; sidecar/embedded conflict variant |
| `sidecar/orphan.xmp` | Sidecar with no media file |
| `naming/übüng ünïcode.jpg` | Non-ASCII *filename* (Windows path encoding) |
| `corrupt/truncated.jpg` | Error-path input |

Each later format increment (TIFF, PNG, WebP, RAW, video) adds its own subdirectory following the
same pattern; the per-type location capabilities in
[supported-types.md](supported-types.md) drive which location fixtures each type gets.

### 2.4 Real-device layouts (C19, OQ-R1)

Session 51 adds tiny generator-produced copies of the *tag layouts* in
[docs/sample-output.txt](sample-output.txt). GPS coordinates, serials, and unique ids are
synthetic (same format as the redacted sample). Original camera files are never committed.

| Fixture | Layout |
|---|---|
| `video/iphone-style.mov` | Keys `GPSCoordinates` with altitude, Keys `CreationDate` with offset, Keys Make/Model, movie-header `CreateDate` years later |
| `jpeg/iphone-heic-layout.jpg` | iPhone HEIC EXIF on JPEG (HEIC container remains Tier B `heic-quicktime`): DateTimeOriginal + sub-seconds + offset; GPS IFD with ImgDirection, Speed, HPositioningError |
| `raw/pixel-style.dng` | IFD0 DateTimeOriginal; IIM TimeCreated with an offset |
| `jpeg/pixel-style.jpg` | EXIF GPS and top-level `exif:GPS*` within 1e-5°; photoshop DateCreated 4-digit fraction versus EXIF 6-digit sub-seconds; `xmp:CreateDate` present |
| `video/gopro-style.mp4` | No Keys, no XMP, movie-header date only; GoPro gpmd Model/serial are not synthesizable |

### 2.3 Repo hygiene rules

- No fixture over **100 KB** without a written justification in this file.
- No third-party media committed, ever. Every committed fixture is generator-produced;
  `tests/fixtures/MANIFEST.md` lists each file, its generator command, and SHA-256.
- RAW/video Tier A fixtures: prefer generator-produced (ExifTool can create sidecar-style EXIF
  files; minimal MP4 via ffmpeg `-f lavfi -i color=... -t 0.1`). If a format cannot be
  synthesized small (e.g. proprietary RAW), that format's integration tests move to Tier B
  download instead of committing a real camera file.

## 3. Downloaded corpus (Tier B — cross-backend verification)

- A manifest file `tests/corpus/manifest.json`: URL, SHA-256, size, license note per sample.
- Fetched into `.cache/corpus/` at test time (CI caches by manifest hash); **fail closed** on
  checksum mismatch; skipped-with-visible-status when the Tier B job is not requested.
- Candidate sources at that time: exiftool/exiv2 repository files referenced *in place* (fetched,
  not vendored), rawsamples-style archives, and self-shot CC0 samples from real cameras
  contributed to a `dtmland` fixtures repository (preferred long-term home for anything big).
- Large-corpus growth goes in that separate fixtures repository or LFS bucket — never this repo.

## 4. What each API function needs

| API surface | Fixtures used |
|---|---|
| `read()` | every fixture |
| `write()` / round-trip | `minimal`, `full-agreeing`, `unicode`, `makernote`, `unknown-tags` |
| `capabilities()` | one fixture per (type × metadata category) claim in the capability data |
| reconciliation | `full-agreeing`, `full-conflicting`, sidecar conflict pair |
| provenance | `full-*` fixtures (multi-source values) |
| unmapped access | `unknown-tags`, `makernote` |
| location | `gps` per type; later QuickTime `GPSCoordinates` fixture |
| sidecar policy | `sidecar/*` |
| GPS track engine (later) | tiny hand-written GPX/NMEA/KML files (text, trivially small, authored in-repo) |

## 5. Success criteria

- Full Tier A corpus < 1 MB for Phase 1; < 5 MB through the stills expansion; video adds < 5 MB.
- `git clone` and build times unaffected (no LFS required for Tier A).
- Every fixture regenerable by `tests/fixtures/generator/` with pinned tools.
- Zero third-party-licensed media bytes committed.
