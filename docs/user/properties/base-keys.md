# Base keys

> **GENERATED** from the IPTC registries, `registry/mappings/`, and `registry/casts/` by `tools/registry/generate_property_reference.py`.
> Do not edit by hand. Decision **C14a**: this is the `umm::describe` / `PropertyMap` data rendered as text. Definitions are copied from the imported Technical References.

File-stored tags that are **not** canonical properties. Cast sources are opt-in links (C7). A short curated list of frequent camera keys stays non-canonical: libumm does not create an EXIF domain (C17).

Listing every ExifTool tag would run to thousands of rows. See [ExifTool tag names](https://exiftool.org/TagNames/), [EXIF tags](https://exiftool.org/TagNames/EXIF.html), and [QuickTime tags](https://exiftool.org/TagNames/QuickTime.html).

Index: [property reference](README.md).

## Cast sources

<a id="Exif.GPSInfo.GPSLatitude"></a>

## `Exif.GPSInfo.GPSLatitude`

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `capturePosition` | up | [`iptc.video.locationShot`](video.md#iptc.video.locationShot)`[0].gpsLatitude` | H10 — Fan-in: combine per rule (GPS refs) | EXIF GPS IFD combined with latitude/longitude refs (H10); extra GPS tags remain unmapped (H20) |

<a id="Exif.Image.Make"></a>

## `Exif.Image.Make`

Camera manufacturer. Intentionally non-canonical (C17); no EXIF property domain.

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `recordingDevice` | up | [`iptc.video.recordingDevice`](video.md#iptc.video.recordingDevice)`[0].manufacturer` | H8 — Struct ↔ scalar: field-level merge | EXIF IFD0 Make |

<a id="Exif.Image.Model"></a>

## `Exif.Image.Model`

Camera model. Intentionally non-canonical (C17).

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `recordingDevice` | up | [`iptc.video.recordingDevice`](video.md#iptc.video.recordingDevice)`[0].modelName` | H8 — Struct ↔ scalar: field-level merge | EXIF IFD0 Model |

<a id="Exif.Photo.BodySerialNumber"></a>

## `Exif.Photo.BodySerialNumber`

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `recordingDevice` | up | [`iptc.video.recordingDevice`](video.md#iptc.video.recordingDevice)`[0].serialNumber` | H8 — Struct ↔ scalar: field-level merge | EXIF BodySerialNumber |

<a id="Exif.Photo.LensModel"></a>

## `Exif.Photo.LensModel`

Lens model. Intentionally non-canonical.

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `recordingDevice` | up | [`iptc.video.recordingDevice`](video.md#iptc.video.recordingDevice)`[0].attLensDescription` | H8 — Struct ↔ scalar: field-level merge | EXIF LensModel |

<a id="ExifTool.GoPro.CameraSerialNumber"></a>

## `ExifTool.GoPro.CameraSerialNumber`

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `recordingDevice` | up | [`iptc.video.recordingDevice`](video.md#iptc.video.recordingDevice)`[0].serialNumber` | H8 — Struct ↔ scalar: field-level merge | C19 GoPro CameraSerialNumber |

<a id="ExifTool.GoPro.Model"></a>

## `ExifTool.GoPro.Model`

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `recordingDevice` | up | [`iptc.video.recordingDevice`](video.md#iptc.video.recordingDevice)`[0].modelName` | H8 — Struct ↔ scalar: field-level merge | C19 GoPro Model |

<a id="QuickTime.CreateDate"></a>

## `QuickTime.CreateDate`

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `videoCreated` (approximate) | up | [`iptc.video.dateCreated`](video.md#iptc.video.dateCreated) | H12 — Dates / offsets: never invent an offset | Movie-header CreateDate; never invent a UTC offset (H12) |

<a id="QuickTime.Keys.Make"></a>

## `QuickTime.Keys.Make`

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `recordingDevice` | up | [`iptc.video.recordingDevice`](video.md#iptc.video.recordingDevice)`[0].manufacturer` | H8 — Struct ↔ scalar: field-level merge | QuickTime Keys Make |

<a id="QuickTime.Keys.Model"></a>

## `QuickTime.Keys.Model`

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `recordingDevice` | up | [`iptc.video.recordingDevice`](video.md#iptc.video.recordingDevice)`[0].modelName` | H8 — Struct ↔ scalar: field-level merge | QuickTime Keys Model |

<a id="QuickTime.Keys.location.ISO6709"></a>

## `QuickTime.Keys.location.ISO6709`

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `capturePosition` | down | [`iptc.video.locationShot`](video.md#iptc.video.locationShot)`[0].gpsLatitude` | H1 — List → single: first entry | First locationShot GPS (H1) to Keys location.ISO6709 and UserData GPSCoordinates |
| `capturePosition` | up | [`iptc.video.locationShot`](video.md#iptc.video.locationShot)`[0].gpsLatitude` | H11 — Units / encodings; 1e-5° / 0.5 m | Apple QuickTime Keys location.ISO6709; H18 role 0 or absent only |

<a id="QuickTime.ModifyDate"></a>

## `QuickTime.ModifyDate`

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `videoModified` | up | [`iptc.video.dateModified`](video.md#iptc.video.dateModified) | H12 — Dates / offsets: never invent an offset | Movie-header ModifyDate; never invent a UTC offset (H12) |

<a id="QuickTime.UserData.GPSCoordinates"></a>

## `QuickTime.UserData.GPSCoordinates`

| Group | Direction | Canonical partner | Heuristic | Citation |
|---|---|---|---|---|
| `capturePosition` | up | [`iptc.video.locationShot`](video.md#iptc.video.locationShot)`[0].gpsLatitude` | H11 — Units / encodings; 1e-5° / 0.5 m | QuickTime UserData copyright-xyz / GPSCoordinates |

## Frequent camera keys

These tags are common in camera files and stay non-canonical. They are visible through `dumpAll()` / `dumpUnmapped()` unless a cast consumes them.

- [`Exif.Image.Make`](#Exif.Image.Make) — Camera manufacturer. Intentionally non-canonical (C17); no EXIF property domain. Also a cast source (see above).
- [`Exif.Image.Model`](#Exif.Image.Model) — Camera model. Intentionally non-canonical (C17). Also a cast source (see above).
<a id="Exif.Photo.ExposureTime"></a>

### `Exif.Photo.ExposureTime`

Shutter speed. Camera exposure; not a canonical IPTC property.

<a id="Exif.Photo.FNumber"></a>

### `Exif.Photo.FNumber`

Aperture. Camera exposure; not a canonical IPTC property.

<a id="Exif.Photo.ISOSpeedRatings"></a>

### `Exif.Photo.ISOSpeedRatings`

ISO sensitivity. Camera exposure; not a canonical IPTC property.

<a id="Exif.Photo.PhotographicSensitivity"></a>

### `Exif.Photo.PhotographicSensitivity`

ISO sensitivity (Exif 2.3). Camera exposure; not a canonical IPTC property.

<a id="Exif.Photo.FocalLength"></a>

### `Exif.Photo.FocalLength`

Lens focal length. Intentionally non-canonical.

<a id="Exif.Photo.LensMake"></a>

### `Exif.Photo.LensMake`

Lens manufacturer. Intentionally non-canonical.

- [`Exif.Photo.LensModel`](#Exif.Photo.LensModel) — Lens model. Intentionally non-canonical. Also a cast source (see above).
