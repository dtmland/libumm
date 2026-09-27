# Backend file-type coverage: Exiv2 and ExifTool

This is a snapshot of **file-type** (container) support in the two backends named in [concept.md](concept.md). It is **not** a list of IPTC/EXIF/XMP *properties*.

Sources (check these for drift):

- Exiv2 FILE TYPES table: [exiv2.md](https://github.com/Exiv2/exiv2/blob/main/exiv2.md)
- ExifTool file types and meta-information formats: [ExifTool README](https://github.com/exiftool/exiftool/blob/master/README)

Capability discovery in libumm must be **per backend, per file type, and per metadata category**. A static “this extension is supported” table is not enough.

---

## Is Exiv2 a subset of ExifTool?

**Almost for still-image / RAW *file types*, but not strictly — and not for write capability.**

| Claim | Reality |
|---|---|
| Exiv2 file types ⊂ ExifTool file types | **Mostly.** ExifTool lists every Exiv2 still-image/RAW type except **TGA** (Exiv2 only identifies TGA: MIME type and dimensions). |
| Exiv2 read/write ⊂ ExifTool read/write | **No.** ExifTool writes many types Exiv2 can only read. The reverse also happens: Exiv2 **writes PGF**; ExifTool is **read-only** for PGF. |
| Same file type ⇒ same metadata categories | **No.** Example: Exiv2’s table has **no Exif** on PNG and **no IPTC** on WebP. ExifTool lists both containers as **r/w** (PNG including Exif; WebP via Exif/XMP rather than a separate IPTC column). |
| Video/audio/documents | Exiv2 has only **rudimentary read** of a few video/RIFF types. ExifTool covers video, audio, documents, fonts, archives, and more. |

So: treat Exiv2 as the **narrow native C++ image/RAW backend**, and ExifTool as the **broad compatibility backend**. Prefer ExifTool when write access or format coverage matters (the [concept.md](concept.md) CR3 example).

Legend used below:

- Exiv2: **Read/Write**, **Read**, or **-** (not applicable / not supported for that metadata category)
- ExifTool: **r** = read, **w** = write, **c** = create (new metadata file from scratch)
- “Identify only” (Exiv2): format recognized, MIME type assigned, width/height determined — no Exif/IPTC/XMP

---

## 1. Exiv2 supported types

Official Exiv2 table (metadata categories per type). BMFF types (AVIF, CR3, HEIF, HEIC) are a **build option** (`enable_bmff=1`). Naked JPEG XL *codestreams* do not contain Exif/IPTC/XMP. Other unlisted TIFF-like RAW files may still read. RAF extra internal metadata is only partially supported.

| Type | Exif | IPTC | XMP | Comments | ICC | Thumbnail |
|---|---|---|---|---|---|---|
| ARW | Read/Write | Read/Write | Read/Write | - | Read/Write | Read/Write |
| AVIF | Read | Read | Read | - | - | Read |
| BMP | - | - | - | - | - | - |
| CR2 | Read/Write | Read/Write | Read/Write | - | Read/Write | Read/Write |
| CR3 | Read | Read | Read | - | - | Read |
| CRW | Read/Write | - | - | Read/Write | - | Read/Write |
| DCP | Read/Write | - | - | - | - | - |
| DNG | Read/Write | Read/Write | Read/Write | - | Read/Write | Read/Write |
| EPS | - | - | Read/Write | - | - | - |
| EXV | Read/Write | Read/Write | Read/Write | Read/Write | Read/Write | Read/Write |
| GIF | - | - | - | - | - | - |
| HEIC | Read | Read | Read | - | - | Read |
| HEIF | Read | Read | Read | - | - | Read |
| JP2 | Read/Write | Read/Write | Read/Write | - | Read/Write | Read/Write |
| JPEG | Read/Write | Read/Write | Read/Write | Read/Write | Read/Write | Read/Write |
| JXL | Read | Read | Read | - | - | Read |
| MRW | Read | Read | Read | - | - | Read |
| NEF | Read/Write | Read/Write | Read/Write | - | Read/Write | Read/Write |
| ORF | Read/Write | Read/Write | Read/Write | - | - | Read/Write |
| PEF | Read/Write | Read/Write | Read/Write | - | Read/Write | Read/Write |
| PGF | Read/Write | Read/Write | Read/Write | Read/Write | Read/Write | - |
| PNG | - | Read/Write | Read/Write | - | Read/Write | Read/Write |
| PSD | Read/Write | Read/Write | Read/Write | - | - | Read/Write |
| RAF | Read | Read | Read | - | - | Read |
| RW2 | Read | Read | Read | - | - | Read |
| SR2 | Read | Read | Read | - | - | Read |
| SRW | Read/Write | Read/Write | Read/Write | - | - | Read/Write |
| TGA | - | - | - | - | - | - |
| TIFF | Read/Write | Read/Write | Read/Write | - | Read/Write | Read/Write |
| WEBP | Read/Write | - | Read/Write | - | Read/Write | Read/Write |
| XMP | - | - | Read/Write | - | - | - |

Identify-only (no metadata categories): **BMP**, **GIF**, **TGA**.

### Exiv2 video (rudimentary read only)

Exiv2 documents limited **read** of QuickTime, Matroska, and RIFF-based files, for example:

| Type | Exiv2 | Notes |
|---|---|---|
| MOV / MP4 | Read (rudimentary) | QuickTime |
| MKV | Read (rudimentary) | Matroska |
| AVI | Read (rudimentary) | RIFF |
| WAV | Read (rudimentary) | RIFF |
| ASF | Read (rudimentary) | |

There is **no Exiv2 write path** for these.

---

## 2. Same types in ExifTool — do not repeat the Exiv2 table

ExifTool covers the Exiv2 still-image/RAW types above (except **TGA**, which it does not list). The interesting part is **capability**, not the type name.

| Type | Exiv2 overall | ExifTool | Do not miss |
|---|---|---|---|
| JPEG, TIFF, DNG, JP2, PSD, EXV, ARW, CR2, CRW, NEF, ORF, PEF, SRW, DCP | Read/Write (see category table) | r/w (EXV also **c**) | Category holes still exist (e.g. CRW has no IPTC/XMP in Exiv2) |
| PNG | IPTC/XMP/ICC Read/Write; **no Exif** | r/w | ExifTool can read/write PNG Exif; Exiv2’s official table does not |
| WEBP | Exif/XMP Read/Write; **no IPTC** | r/w | Exiv2 has no IPTC category for WebP; ExifTool writes WebP as Exif/XMP |
| EPS | XMP Read/Write only | r/w | Broader than XMP-only |
| XMP sidecar | XMP Read/Write | r/w/**c** | ExifTool can create sidecars from scratch |
| PGF | **Read/Write** | **r** | Exiv2 is stronger on write |
| AVIF, HEIC, HEIF, JXL | **Read** (BMFF build) | **r/w** | Write requires ExifTool |
| CR3 | **Read** (BMFF build) | **r/w** | Matches the concept-doc CR3 example |
| MRW, RAF, RW2, SR2 | **Read** | **r/w** | Write requires ExifTool |
| GIF | Identify only | **r/w** | |
| BMP | Identify only | **r** | Still no ExifTool write |
| TGA | Identify only | *not listed* | Exiv2-only recognition |
| MOV / MP4 | Rudimentary read | **r/w** | |
| MKV, AVI, WAV, ASF | Rudimentary read | **r** | Still no write in either backend |

ExifTool **create** (`c`) among types Exiv2 also has: **EXV**, **XMP**. (ExifTool also creates **EXIF**, **ICC**, **MIE**, **DR4**, **VRD** files; those are listed in section 3.)

---

## 3. Additional ExifTool types

Everything below is **in ExifTool and not in the Exiv2 FILE TYPES table** (video types already compared above are omitted here except where ExifTool adds *siblings* such as 3GP/M4V).

Grouped so the overlap with Exiv2 is not repeated. Values are ExifTool r / w / c.

### Additional still image and graphics

| Type | ExifTool | Type | ExifTool | Type | ExifTool |
|---|---|---|---|---|---|
| 360 | r/w | BPG | r | BTF | r |
| CUR | r | DPX | r | EXR | r |
| FLIF | r/w | FPX | r | HDR | r |
| ICO | r | J2C | r | JNG | r/w |
| MNG | r/w | MPO | r/w | MIFF | r |
| PBM | r/w | PCD | r | PCX | r |
| PFM | r | PGM | r/w | PICT | r |
| PPM | r/w | PS | r/w | PSB | r/w |
| PSP | r | QTIF | r/w | SVG | r |
| WPG | r | XCF | r | XISF | r |

Related TIFF-family still image: **HDP r/w**, **WDP r/w**. **THM r/w** is JPEG thumbnail naming.

### Additional camera RAW / vendor still

| Type | ExifTool | Type | ExifTool | Type | ExifTool |
|---|---|---|---|---|---|
| 3FR | r | ARQ | r/w | CR3 siblings: CRM | r/w |
| CS1 | r/w | DCR | r | ERF | r/w |
| FFF | r/w | GPR | r/w | IIQ | r/w |
| INSP | r/w | K25 | r | KDC | r |
| MEF | r/w | MOS | r/w | NRW | r/w |
| ORI | r/w | RAW | r/w | RWL | r/w |
| SRF | r | X3F | r/w | | |

### Video and timed media (beyond the Exiv2 rudimentary set)

| Type | ExifTool | Type | ExifTool | Type | ExifTool |
|---|---|---|---|---|---|
| 3G2 | r/w | 3GP | r/w | DIVX | r |
| DV | r | DVB | r/w | DVR-MS | r |
| F4A/V | r/w | FLA | r | FLV | r |
| GLV | r/w | INSV | r | LRV | r/w |
| M2TS | r | M4A/V | r/w | MKS | r |
| MPG | r | MQV | r/w | MXF | r |
| OGV | r | R3D | r | RM | r |
| SEQ | r | SWF | r | WEBM | r |
| WMV | r | WTV | r | | |

**M4A/V**, **F4A/V**, **3GP**, **3G2**, **LRV**, **MQV**, **GLV**, **DVB** are writable QuickTime-family relatives of MOV/MP4. **MKV / WEBM / MKA / MKS** remain **read-only** in ExifTool.

### Audio

| Type | ExifTool | Type | ExifTool | Type | ExifTool |
|---|---|---|---|---|---|
| AAC | r | AAX | r/w | AIFF | r |
| APE | r | DSF | r | DSS | r |
| FLAC | r | LA | r | MKA | r |
| MP3 | r | MPC | r | OGG | r |
| OFR | r | OPUS | r | RA | r |
| WAV | r | WMA | r | WV | r |

Almost all audio is **read-only**. **AAX** is the writable exception in this list (QuickTime-family). WAV is read-only in both backends.

### Documents, office, ebook, page layout

| Type | ExifTool | Type | ExifTool | Type | ExifTool |
|---|---|---|---|---|---|
| AI | r/w | DOC | r | DOCX | r |
| EPUB | r | HTML | r | IDML | r |
| IND | r/w | INX | r | KEY | r |
| MAX | r | MOBI | r | NUMBERS | r |
| ODP | r | ODS | r | ODT | r |
| PAGES | r | PDF | r/w | PPT | r |
| PPTX | r | RTF | r | VSD | r |
| VSDX | r | XLS | r | XLSX | r |
| AZW | r | CHM | r | DJVU | r |

Writable documents among these: **PDF**, **AI**, **IND**. Office Open XML (DOCX/XLSX/PPTX) is **read-only**.

### Sidecars, metadata-only, and color

| Type | ExifTool | Type | ExifTool | Type | ExifTool |
|---|---|---|---|---|---|
| AAE | r | C2PA | r | DR4 | r/w/c |
| EXIF | r/w/c | ICC | r/w/c | JSON | r |
| MIE | r/w/c | MODD | r | NKSC | r/w |
| ONP | r | VRD | r/w/c | XMP | r/w/c |

### Fonts, executables, archives, scientific, and other

| Type | ExifTool | Type | ExifTool | Type | ExifTool |
|---|---|---|---|---|---|
| 7Z | r | A | r | AA | r |
| ACR | r | AFM | r | COS | r |
| CSV | r | CZI | r | DCM | r |
| DFONT | r | DLL | r | DYLIB | r |
| EIP | r | EXE | r | FIT | r |
| FITS | r | FPF | r | GZ | r |
| ICS | r | ISO | r | ITC | r |
| KVAR | r | LFP | r | LIF | r |
| LNK | r | MACOS | r | MOI | r |
| MRC | r | NKA | r | NXD | r |
| O | r | OTF | r | PAC | r |
| PCAP | r | PCAPNG | r | PDB | r |
| PFA | r | PFB | r | PLIST | r |
| PMP | r | RAM | r | RAR | r |
| RIFF | r | RSRC | r | RWZ | r |
| SKETCH | r | SO | r | TNEF | r |
| TORRENT | r | TTC | r | TTF | r |
| TXT | r | URL | r | VCF | r |
| VNT | r | WOFF | r | WOFF2 | r |
| ZIP | r | | | | |

---

## 4. ExifTool meta-information formats

Separate from file types. ExifTool README (r/w/c = read / write / create):

**Read/write/create:** EXIF, GPS, IPTC, XMP, MakerNotes, Photoshop IRB, ICC Profile, MIE, JFIF, Ducky APP12, PDF, PNG, Canon VRD, Nikon Capture, GeoTIFF, CIFF, AFCP, Kodak Meta, FotoStation, PhotoMechanic.

**Read-only (examples):** JPEG 2000, DICOM, Flash, FlashPix, QuickTime, Matroska, MXF, PrintIM, FLAC, ID3, Ricoh RMETA, Picture Info, Adobe APP14, MPF, Stim, DPX, APE, Vorbis, SPIFF, DjVu, M2TS, PE/COFF, AVCHD, ZIP, and more.

QuickTime **file** metadata can be written for MOV/MP4-family files even though the QuickTime *tag group* is listed read-only in that second table — another reason libumm should not collapse “file type” and “metadata encoding” into one flag.

---

## 5. Implications for libumm

1. **JPEG, TIFF, DNG, and common writable RAW** can use Exiv2 or ExifTool; Exiv2 is the natural native default.
2. **CR3, HEIC/HEIF, AVIF, JXL, RAF, RW2, MRW, SR2** should advertise **read via Exiv2 (when built with BMFF) / write via ExifTool**.
3. **PGF write** is an Exiv2-only backend capability.
4. **PNG Exif** is not interchangeable across backends (ExifTool yes; Exiv2 table no). **WebP** has no IPTC category in Exiv2.
5. **Video write** is ExifTool + QuickTime-family only in this pairing. MKV/WebM/AVI stay read-only.
6. **Audio, office documents, PDF, fonts, archives** are ExifTool-only (PDF is writable).
7. Phase 1 in [concept.md](concept.md) (JPEG, TIFF, PNG, WebP, common RAW, XMP sidecars) is inside Exiv2’s strong set, except **PNG Exif** if that is required (use ExifTool).
8. Phase 3 video (MP4, MOV, M4V, MKV, WebM, MXF) is **ExifTool-primary**; Exiv2 at most supplements read.
