# Backend file-type coverage: Exiv2 and ExifTool

> **GENERATED** from `registry/capabilities/` by `tools/registry/generate_supported_types.py`.
> Do not edit by hand. Decision **M2**: capability data is machine-readable; this document is generated output. Tier A CI probes pinned backends against the data so drift fails a test.

This is a snapshot of **file-type** (container) support in the two backends named in [concept.md](analysis/concept.md). It is **not** a list of IPTC/EXIF/XMP *properties*.

**Location is called out separately.** `getLocation()` / `setLocation()` in [concept.md](analysis/concept.md) is not a file-type flag. Coordinates and named place live in different encodings, and Exiv2 vs ExifTool do not offer the same location **read** or **write** path for the same type. See [§3 Location metadata](#3-location-metadata-gps-and-named-place).

Sources (check these for drift):

- Exiv2 FILE TYPES table: [exiv2.md](https://github.com/Exiv2/exiv2/blob/main/exiv2.md)
- Exiv2 GPS write example (`Exif.GPSInfo.GPSLatitude`): same [exiv2.md](https://github.com/Exiv2/exiv2/blob/main/exiv2.md)
- ExifTool file types and meta-information formats: [ExifTool README](https://github.com/exiftool/exiftool/blob/master/README) (lists **GPS** and **GeoTIFF** as their own r/w/c formats)
- ExifTool geotagging (track → GPS tags, including QuickTime `GPSCoordinates`): [geotag.html](https://github.com/exiftool/exiftool/blob/master/html/geotag.html)

Capability discovery in libumm must be **per backend, per file type, and per metadata category** — including **location** (GPS coordinates vs IPTC/XMP named place vs container GPS). A static “this extension is supported” table is not enough.

---

## Is Exiv2 a subset of ExifTool?

**Almost for still-image / RAW *file types*, but not strictly — and not for write capability.**

| Claim | Reality |
|---|---|
| Exiv2 file types ⊂ ExifTool file types | **Mostly.** ExifTool lists every Exiv2 still-image/RAW type except **TGA** (Exiv2 only identifies TGA: MIME type and dimensions). |
| Exiv2 read/write ⊂ ExifTool read/write | **No.** ExifTool writes many types Exiv2 can only read. The reverse also happens: Exiv2 **writes PGF**; ExifTool is **read-only** for PGF. |
| Same file type ⇒ same metadata categories | **No.** Example: Exiv2’s table has **no Exif** on PNG and **no IPTC** on WebP. ExifTool lists both containers as **r/w** (PNG including Exif; WebP via Exif/XMP rather than a separate IPTC column). |
| Video/audio/documents | Exiv2 has only **rudimentary read** of a few video/RIFF types. ExifTool covers video, audio, documents, fonts, archives, and more. |
| Same file type ⇒ same **location** metadata | **No.** Location is several encodings (EXIF GPS, IPTC/XMP named place, XMP GPS, QuickTime `GPSCoordinates`, GeoTIFF). Exiv2 has **no** location on identify-only types (BMP/GIF/TGA), **no EXIF GPS on PNG**, **no IPTC named place on WebP**, and **no video location write**. ExifTool lists **GPS** as r/w/c and writes location on many types Exiv2 can only read (CR3, HEIC, AVIF, JXL, RAF, MOV/MP4, …). |

So: treat Exiv2 as the **narrow native C++ image/RAW backend**, and ExifTool as the **broad compatibility backend**. Prefer ExifTool when write access, format coverage, or **location write** matters (the [concept.md](analysis/concept.md) CR3 example; PNG EXIF GPS; video GPS).

Legend used below:

- Exiv2: **Read/Write**, **Read**, or **-** (not applicable / not supported for that metadata category)
- ExifTool: **r** = read, **w** = write, **c** = create (new metadata file from scratch)
- “Identify only” (Exiv2): format recognized, MIME type assigned, width/height determined — no Exif/IPTC/XMP — therefore **no location metadata**
- Location: **any-kind** means at least one of GPS coordinates, IPTC/XMP named place, XMP GPS, or container GPS can be read or written

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

Identify-only (no metadata categories, **no location**): **BMP**, **GIF**, **TGA**.

### Exiv2 video (rudimentary read only)

Exiv2 documents limited **read** of QuickTime, Matroska, and RIFF-based files, for example:

| Type | Exiv2 | Notes |
|---|---|---|
| MOV / MP4 | Read (rudimentary) | QuickTime |
| MKV | Read (rudimentary) | Matroska |
| AVI | Read (rudimentary) | RIFF |
| WAV | Read (rudimentary) | RIFF |
| ASF | Read (rudimentary) | ASF |

There is **no Exiv2 write path** for these, including **no location write**. Rudimentary read is not a documented GPS / IPTC / XMP location API.

---

## 2. Same types in ExifTool — do not repeat the Exiv2 table

ExifTool covers the Exiv2 still-image/RAW types above (except **TGA**, which it does not list). The interesting part is **capability**, not the type name.

| Type | Exiv2 overall | ExifTool | Do not miss | Location (any kind) |
|---|---|---|---|---|
| JPEG, TIFF, DNG, JP2, PSD, EXV, ARW, CR2, NEF, ORF, PEF, SRW | Read/Write (see category table) | r/w (EXV also **c**) | Full Exif+IPTC+XMP in Exiv2 | **Both** read/write GPS and named place |
| CRW, DCP | Read/Write (see category table) | r/w | **No IPTC/XMP in Exiv2** | Exiv2: **GPS only**. ExifTool: broader |
| PNG | IPTC/XMP/ICC Read/Write; **no Exif** | r/w | ExifTool can read/write PNG Exif; Exiv2’s official table does not | Exiv2: named place + **XMP GPS only** (no EXIF GPS). ExifTool: EXIF GPS too |
| WEBP | Exif/XMP Read/Write; **no IPTC** | r/w | Exiv2 has no IPTC category for WebP; ExifTool writes WebP as Exif/XMP | Exiv2: GPS + XMP place (**no IPTC**). ExifTool: Exif/XMP |
| EPS | XMP Read/Write only | r/w | Broader than XMP-only | Exiv2: **XMP location only** |
| XMP sidecar | XMP Read/Write | r/w/**c** | ExifTool can create sidecars from scratch | **Both** XMP GPS + named place; ExifTool can **create** |
| PGF | **Read/Write** | **r** | Exiv2 is stronger on write | Exiv2 **writes** location; ExifTool **read-only** |
| AVIF, HEIC, HEIF, JXL | **Read** (BMFF build) | **r/w** | Write requires ExifTool | Exiv2 **read** location; **write via ExifTool** |
| CR3 | **Read** (BMFF build) | **r/w** | Matches the concept-doc CR3 example | Exiv2 **read** location; **write via ExifTool** |
| MRW, RAF, RW2, SR2 | **Read** | **r/w** | Write requires ExifTool | Exiv2 **read** location; **write via ExifTool** |
| GIF | Identify only | **r/w** |  | Exiv2: **none**. ExifTool: **r/w** |
| BMP | Identify only | **r** | Still no ExifTool write | Exiv2: **none**. ExifTool: **read only** |
| TGA | Identify only | *not listed* | Exiv2-only recognition | Exiv2: **none**. ExifTool: not listed |
| MOV / MP4 | Rudimentary read | **r/w** |  | Exiv2: **no documented location write**. ExifTool: **r/w** (`GPSCoordinates` / XMP) |
| MKV, AVI, WAV, ASF | Rudimentary read | **r** | Still no write in either backend | **Read-only** at best; **no location write** in either backend |

ExifTool **create** (`c`) among types Exiv2 also has: **EXV**, **XMP**. (ExifTool also creates **EXIF**, **ICC**, **MIE**, **DR4**, **VRD** files; those are listed in section 4.)

---

## 3. Location metadata (GPS and named place)

This is the section to use for `capabilities(media)` **GPS / location** and for `getLocation()` / `setLocation()`.

**Location is not one tag and not one backend flag.** A type can support *some* location and still fail a specific write. Always split:

| Kind | Typical tags / encodings | What it is |
|---|---|---|
| **GPS coordinates** | EXIF GPS IFD (`GPSLatitude` / `GPSLongitude` / altitude / time); XMP `exif:GPS*`; QuickTime `GPSCoordinates` | Camera/device position |
| **Named place** | IPTC Core (City, Country, CountryCode, Province/State, Sublocation); IPTC Extension Location Created / Location Shown; XMP `photoshop:City`, `Iptc4xmpExt:LocationCreated` / `LocationShown` | Human-readable location from [IPTC Photo Metadata](https://www.iptc.org/std/photometadata/specification/IPTC-PhotoMetadata-2025.1.html) |
| **GeoTIFF** | ModelTiepoint / ModelPixelScale / etc. | Raster georeferencing on TIFF-family files — **not** photo GPS |

### How each backend treats location

| Encoding | Exiv2 | ExifTool |
|---|---|---|
| EXIF GPS IFD | Follows the **Exif** column. Documented write path: `Exif.GPSInfo.*` (example in exiv2.md). **PNG has no Exif** in the official table → **no EXIF GPS**. Identify-only types → **none**. | Meta-information format **GPS r/w/c**. Writable where the container can hold EXIF (including **PNG Exif**, which Exiv2 does not list). |
| IPTC named place | Follows the **IPTC** column. **WebP, CRW, DCP, EPS, XMP sidecar** have no IPTC in Exiv2. | **IPTC r/w/c** on types that hold IPTC/IIM. |
| XMP GPS + named place | Follows the **XMP** column. This is Exiv2’s location path on **PNG** (no Exif) and **EPS** / **XMP sidecars** (XMP-only). | **XMP r/w/c**. Sidecar create (`c`) is ExifTool-only. |
| QuickTime `GPSCoordinates` | Video is **rudimentary read only** — **not** a documented location write path. | Written on **writable QuickTime-family files** (MOV/MP4 and siblings). Geotag writes `GPSCoordinates` into the preferred QuickTime group. The QuickTime *tag group* in the README is listed read-only; **file** metadata on MOV/MP4 is still writable. |
| GeoTIFF | Not a documented Exiv2 category. | **GeoTIFF r/w/c** (TIFF-family). |
| Track geotagging (GPX/NMEA/KML/…) | Not a backend feature. libumm’s GPS track engine sits **above** both backends and then writes through the table below. | ExifTool can interpolate a track and write GPS tags (including video `GPSCoordinates`). |

**Derive Exiv2 “any location” from §1:**

- **Read** any location if Exif **or** IPTC **or** XMP is Read or Read/Write.
- **Write** any location if Exif **or** IPTC **or** XMP is Read/Write.
- **None** if all three are `-` (BMP, GIF, TGA).
- **GPS write** specifically requires Exif Read/Write (so **not** PNG, EPS, XMP sidecar, or identify-only).
- **Named-place write** requires IPTC or XMP Read/Write (so **not** CRW/DCP in Exiv2).

**Derive ExifTool “any location” from the file-type r/w/c flag**, then pick encodings the container actually holds (EXIF GPS, XMP, IPTC, and/or QuickTime GPS). Read-only types cannot write location.

### Per-type location read / write (Exiv2 types)

Emphasized: **whether each backend can read or write any location**, and which encodings Exiv2 actually has.

| Type | Exiv2 GPS (Exif) | Exiv2 named place | Exiv2 any location | ExifTool any location |
|---|---|---|---|---|
| JPEG, TIFF, DNG, JP2, PSD, EXV, ARW, CR2, NEF, ORF, PEF, SRW | Read/Write | Read/Write (IPTC+XMP) | **Read/Write** | **r/w** |
| PGF | Read/Write | Read/Write (IPTC+XMP) | **Read/Write** | **r only** — prefer Exiv2 to **write** location |
| PNG | **-** (no Exif) | Read/Write (IPTC+XMP; XMP may hold GPS) | **Read/Write** (not EXIF GPS) | **r/w** including EXIF GPS |
| WEBP | Read/Write | Read/Write **XMP only** (no IPTC) | **Read/Write** | **r/w** (Exif/XMP) |
| CRW, DCP | Read/Write | **-** | **Read/Write GPS only** | **r/w** |
| EPS | **-** | Read/Write **XMP only** | **Read/Write** | **r/w** |
| XMP sidecar | **-** | Read/Write **XMP only** | **Read/Write** | **r/w/c** |
| AVIF, HEIC, HEIF, JXL, CR3 | Read (BMFF build) | Read (IPTC+XMP) | **Read only** | **r/w** — **write location via ExifTool** |
| MRW, RAF, RW2, SR2 | Read | Read (IPTC+XMP) | **Read only** | **r/w** — **write location via ExifTool** |
| BMP | **-** | **-** | **None** | **r** only |
| GIF | **-** | **-** | **None** | **r/w** — **write location via ExifTool** |
| TGA | **-** | **-** | **None** | *not listed* |
| MOV / MP4 | not documented as GPS | not documented as IPTC/XMP | Rudimentary **read**; **no write** | **r/w** (`GPSCoordinates` / XMP) |
| MKV, AVI, WAV, ASF | not documented as GPS | not documented as IPTC/XMP | Rudimentary **read**; **no write** | **r** — **no location write** |

### ExifTool-only types: location follows file r/w

Do not assume every extra ExifTool type stores GPS. Use the §4 r/w/c flag, then the container:

| Group | Location read | Location write | Notes |
|---|---|---|---|
| Extra writable still/RAW (ARQ, ERF, GPR, IIQ, MEF, MOS, NRW, X3F, HDP/WDP, MPO, PS/PSB, FLIF, 360, INSP, …) | yes | **yes** (ExifTool) | Typical photo GPS/XMP path; **not in Exiv2** |
| Extra writable graphics (PBM/PGM/PPM, JNG/MNG, QTIF, …) | sometimes | **if marked w** | Encoding is container-specific (often comment/XMP, not EXIF GPS) |
| Extra **read-only** still/RAW (3FR, DCR, K25, KDC, SRF, BMP-class graphics, SVG, EXR, …) | often | **no** | Read location if present; cannot embed new GPS |
| QuickTime-family video/audio (**3GP, 3G2, M4A/V, F4A/V, LRV, MQV, GLV, DVB, AAX**, plus MOV/MP4) | yes | **yes** | ExifTool location write; Exiv2 has no write |
| Other video (**MKV, WEBM, AVI, WMV, MXF, MPG, …**) | often | **no** | Same story as MKV/AVI in §2 |
| Audio except AAX | often | **no** (AAX is the writable exception) | WAV is read-only in both backends |
| Documents | PDF/AI/IND: yes | **PDF, AI, IND yes**; Office Open XML **no** | PDF location is typically **XMP** |
| Sidecars / metadata-only | EXIF, XMP, MIE, ICC, … | **EXIF/XMP/MIE create** | Strongest portable location write when the media type cannot embed GPS |
| Fonts, archives, executables, scientific | sometimes | **no** | Out of scope for photo/video location |

**Bottom line for location:**

1. **Exiv2 writes location** on types with Exif, IPTC, or XMP **Read/Write** — strongest on JPEG/TIFF/DNG/common writable RAW, plus PGF, PNG (not EXIF GPS), WebP (not IPTC).
2. **ExifTool writes location** on types it marks **w** *when the container holds EXIF, XMP, IPTC, or QuickTime GPS* — including CR3/HEIC/AVIF/JXL/RAF/GIF/MOV/MP4 and PNG EXIF GPS. **w** is necessary but not always GPS.
3. **Neither writes location** on MKV/WebM/AVI/WAV/ASF, most audio, and most office documents.
4. **PGF** is the rare case where **Exiv2 can write location and ExifTool cannot**.

---

## 4. Additional ExifTool types

Everything below is **in ExifTool and not in the Exiv2 FILE TYPES table** (video types already compared above are omitted here except where ExifTool adds *siblings* such as 3GP/M4V).

Grouped so the overlap with Exiv2 is not repeated. Values are ExifTool r / w / c. For **location**, treat **w** as “ExifTool may write some location encoding if the container allows it” and **r** as read-only; see §3 rather than repeating a location column here.

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
| 3FR | r | ARQ | r/w | CRM | r/w |
| CS1 | r/w | DCR | r | ERF | r/w |
| FFF | r/w | GPR | r/w | IIQ | r/w |
| INSP | r/w | K25 | r | KDC | r |
| MEF | r/w | MOS | r/w | NRW | r/w |
| ORI | r/w | RAW | r/w | RWL | r/w |
| SRF | r | X3F | r/w |  |  |

### Video and timed media (beyond the Exiv2 rudimentary set)

| Type | ExifTool | Type | ExifTool | Type | ExifTool |
|---|---|---|---|---|---|
| 3G2 | r/w | 3GP | r/w | DIVX | r |
| DV | r | DVB | r/w | DVR-MS | r |
| F4A | r/w | F4V | r/w | FLA | r |
| FLV | r | GLV | r/w | INSV | r |
| LRV | r/w | M2TS | r | M4A | r/w |
| M4V | r/w | MKS | r | MPG | r |
| MQV | r/w | MXF | r | OGV | r |
| R3D | r | RM | r | SEQ | r |
| SWF | r | WEBM | r | WMV | r |
| WTV | r |  |  |  |  |

**M4A/V**, **F4A/V**, **3GP**, **3G2**, **LRV**, **MQV**, **GLV**, **DVB** are writable QuickTime-family relatives of MOV/MP4. **MKV / WEBM / MKA / MKS** remain **read-only** in ExifTool.

### Audio

| Type | ExifTool | Type | ExifTool | Type | ExifTool |
|---|---|---|---|---|---|
| AAC | r | AAX | r/w | AIFF | r |
| APE | r | DSF | r | DSS | r |
| FLAC | r | LA | r | MKA | r |
| MP3 | r | MPC | r | OGG | r |
| OFR | r | OPUS | r | RA | r |
| WMA | r | WV | r |  |  |

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
| ONP | r | VRD | r/w/c |  |  |

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
| ZIP | r |  |  |  |  |

This group is transcribed at file-level r/w only (`coverage: partial`); per-category probes join in Stage 6.

---

## 5. ExifTool meta-information formats

Separate from file types. ExifTool README (r/w/c = read / write / create):

**Read/write/create:** EXIF, **GPS**, IPTC, XMP, MakerNotes, Photoshop IRB, ICC Profile, MIE, JFIF, Ducky APP12, PDF, PNG, Canon VRD, Nikon Capture, **GeoTIFF**, CIFF, AFCP, Kodak Meta, FotoStation, PhotoMechanic.

Those **GPS** and **GeoTIFF** rows are why ExifTool can advertise location as its own capability, not merely “the file type is r/w”. GPS here is the EXIF GPS IFD (ExifTool group `GPS`, family 2 `Location`). GeoTIFF is raster georeferencing, not `metadata.location` for a photograph.

**Read-only (examples):** JPEG 2000, DICOM, Flash, FlashPix, QuickTime, Matroska, MXF, PrintIM, FLAC, ID3, Ricoh RMETA, Picture Info, Adobe APP14, MPF, Stim, DPX, APE, Vorbis, SPIFF, DjVu, M2TS, PE/COFF, AVCHD, ZIP, and more.

QuickTime **file** metadata can be written for MOV/MP4-family files even though the QuickTime *tag group* is listed read-only in that second table — another reason libumm should not collapse “file type” and “metadata encoding” into one flag.

---

## 6. Implications for libumm

1. **JPEG, TIFF, DNG, and common writable RAW** can use Exiv2 or ExifTool; Exiv2 is the natural native default. **Location read/write** (EXIF GPS and IPTC/XMP named place) works in **both** backends on these types.
2. **CR3, HEIC/HEIF, AVIF, JXL, RAF, RW2, MRW, SR2** should advertise **read via Exiv2 (when built with BMFF) / write via ExifTool** — including **`setLocation()`**.
3. **PGF write** (including location) is an Exiv2-only backend capability.
4. **PNG Exif / EXIF GPS** is not interchangeable (ExifTool yes; Exiv2 table **no**). Exiv2 can still write PNG **named place and XMP GPS**. **WebP** has no IPTC named-place category in Exiv2 (use Exif/XMP). **CRW/DCP** in Exiv2 are **GPS only**.
5. **Video location write** is ExifTool + QuickTime-family only (`GPSCoordinates` / XMP). MKV/WebM/AVI/WAV/ASF stay **read-only** — **no location write** in either backend.
6. **Audio, office documents, PDF, fonts, archives** are ExifTool-only (PDF is writable, including XMP location). GIF location write is ExifTool-only. BMP location is ExifTool **read-only**. TGA has **no** location in Exiv2 and is not listed in ExifTool.
7. Phase 1 in [concept.md](analysis/concept.md) (JPEG, TIFF, PNG, WebP, common RAW, XMP sidecars) is inside Exiv2’s strong **location** set, except **PNG EXIF GPS** if that encoding is required (use ExifTool).
8. Phase 3 video (MP4, MOV, M4V, MKV, WebM, MXF) is **ExifTool-primary** for location; Exiv2 at most supplements read. Phase 5 GPS tracks write through this same location table — they do not create a new file-type capability.
9. `capabilities(media)` must report **GPS** and **named place** separately per backend. “Type supported” is not “location writable”.
