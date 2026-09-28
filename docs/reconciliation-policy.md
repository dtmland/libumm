# Reconciliation policy (Phase 1)

Status: **normative** for JPEG embedded read (session 12), write-synchronization
(session 13), and XMP sidecar pairing (session 14).

This is the written, testable policy required by decision **S4a**. Classification
and provenance shapes are those in `include/umm/provenance.hpp` (concept.md §15,
§17).

## Sources of the rules

| Tag | Document | Role |
| --- | --- | --- |
| IPTC-MG | [IPTC Photo Metadata Mapping Guidelines](https://www.iptc.org/std/photometadata/documentation/mappingguidelines/) 2025.1 | Which EXIF / IIM / XMP tags represent the same IPTC property. |
| IPTC-TR | IPTC Photo Metadata Technical Reference 2025.1 (`registry/iptc-photo/iptc-photo.json`) | Datatype, cardinality, and representation fields used to group raw keys. |
| MWG | Metadata Working Group *Guidelines for Handling Image Metadata* 2.0 (2010) | Frozen historical input (decision **S4b**). Defunct; not a living authority. |
| ExifTool-MWG | ExifTool `MWG` composite tags | Compatibility reference for MWG read precedence (decision **S4b**). Not invoked as a backend. |
| M3 | Analysis decision M3 | UTF-8 values; no charset guessing on read. |

libumm does not invent property semantics. Grouping starts from registry
representations. Extra raw keys listed below are only the well-known companion
tags the standards already treat as one value (EXIF sub-second/offset, IIM time,
legacy named-place IIM/photoshop fields).

## Classification

After grouping and normalization, each present canonical property is classified:

| `Resolution` | Meaning |
| --- | --- |
| `single` | Exactly one source group held a parseable value. |
| `equivalent` | Two or more groups agree after normalization. The stored value is the **most complete** agreed form (union of present components that do not disagree). `preferred_source` is empty. |
| `reconciled` | Groups disagree; this policy selects a winner. `value` is the winner; `preferred_source` is that group's primary raw key; **every** contributing raw origin remains in `sources`. |
| `conflict` | Groups disagree and this policy refuses to auto-resolve (unranked same-tier disagreement). `value` is still the first group in document order so applications have a candidate; `preferred_source` is that group's primary key. |

`Metadata::conflictedPropertyIds()` lists `conflict` only. `ReadOptions::conflicts_as_errors`
turns any `conflict` into `ErrorCode::conflict_unresolved`; `reconciled` is not an error.

No source is dropped: `PropertyValue::sources` lists every raw entry that contributed.

## Normalization (all properties)

- Compare UTF-8 text after trimming leading/trailing ASCII whitespace. IIM
  `CodedCharacterSet` is not a semantic property; backends already emit UTF-8
  (M3). A missing charset marker does not change equivalence.
- Indexed raw keys (`Xmp.dc.creator[1]`, `Iptc.Application2.Keywords[2]`) belong
  to the same group as the unindexed key. An XMP Bag/Seq container whose
  `type_hint` is `XmpBag`/`XmpSeq` (Exiv2 joins items with `", "`) is not itself
  a list item; use indexed children, or those joined components, instead.
- Empty values are ignored.
- Partial values never invent missing fields (unknown EXIF offset is not UTC).

## Read precedence (default)

Unless a property table says otherwise:

**XMP > IPTC IIM > EXIF**

This matches MWG's "XMP is the current specification" reading and ExifTool-MWG
composites that prefer the XMP encoding when values disagree (S4b). GPS is
EXIF-native and inverts the last two steps (see below).

Same-tier means the same metadata family after mapping (XMP vs XMP, IIM vs IIM,
EXIF vs EXIF). Two unequal XMP encodings of one property are `conflict`.
Cross-family disagreement is `reconciled` using the precedence row.

## Phase 1 properties

Raw keys below are the Exiv2-syntax vocabulary (`Exif.*` / `Iptc.*` / `Xmp.*`).
Registry `exif_tag` / `xmp_property` / `iim_dataset` fields are translated into
that vocabulary at read time (session 07 tables + IPTC-MG overlay).

### `iptc.photo.creator` (text list)

| Family | Raw keys | Origin |
| --- | --- | --- |
| XMP | `Xmp.dc.creator` (seq) | IPTC-TR `dc:creator` |
| IIM | `Iptc.Application2.Byline` (repeatable) | IPTC-TR `2:80` |
| EXIF | `Exif.Image.Artist` | IPTC-MG `IFD0:Artist` |

- EXIF Artist is one name (do not split on commas). IIM/XMP may hold several.
- Equivalence: ordered lists of trimmed names. A single EXIF/IIM string equals
  an XMP seq of one matching name.
- Disagreement: `reconciled`, prefer XMP then IIM then EXIF (MWG Creator /
  ExifTool-MWG `Creator`).
- **Write-sync (session 13):** write all three; XMP seq and repeated IIM By-line
  get the full list; EXIF Artist gets the list joined with `"; "` (MWG write).

### `iptc.photo.description` (lang-alt)

| Family | Raw keys | Origin |
| --- | --- | --- |
| XMP | `Xmp.dc.description` | IPTC-TR `dc:description` |
| IIM | `Iptc.Application2.Caption` | IPTC-TR `2:120` |
| EXIF | `Exif.Image.ImageDescription` | IPTC-MG `IFD0:ImageDescription` |

- IIM/EXIF plain text become `{x-default: text}`.
- Equivalence: `x-default` matches, and every language present on both sides
  matches. Extra languages on one side are not equivalent.
- Disagreement: `reconciled`, XMP > IIM > EXIF (MWG Description).
- **Write-sync:** write all three; IIM/EXIF receive `x-default` (or the sole
  language if `x-default` is absent).

### `iptc.photo.copyrightNotice` (lang-alt)

| Family | Raw keys | Origin |
| --- | --- | --- |
| XMP | `Xmp.dc.rights` | IPTC-TR `dc:rights` |
| IIM | `Iptc.Application2.Copyright` | IPTC-TR `2:116` |
| EXIF | `Exif.Image.Copyright` | IPTC-MG `IFD0:Copyright` |

Same normalization, precedence, classification, and write-sync as description
(MWG Copyright / IPTC-MG).

### `iptc.photo.headline` (text)

| Family | Raw keys | Origin |
| --- | --- | --- |
| XMP | `Xmp.photoshop.Headline` | IPTC-TR `photoshop:Headline` |
| IIM | `Iptc.Application2.Headline` | IPTC-TR `2:105` |

Not `dc:title` / IIM ObjectName (`2:05`) — that is `iptc.photo.title` if present.
Equivalence: trimmed UTF-8. Disagreement: `reconciled`, XMP > IIM.
**Write-sync:** both representations.

### `iptc.photo.creditLine` (text)

| Family | Raw keys | Origin |
| --- | --- | --- |
| XMP | `Xmp.photoshop.Credit` | IPTC-TR `photoshop:Credit` |
| IIM | `Iptc.Application2.Credit` | IPTC-TR `2:110` |

Equivalence: trimmed UTF-8. Disagreement: `reconciled`, XMP > IIM.
**Write-sync:** both representations.

### `iptc.photo.dateCreated` (date-time) — heart of Phase 1

| Family | Raw keys | Origin |
| --- | --- | --- |
| XMP | `Xmp.photoshop.DateCreated` | IPTC-TR `photoshop:DateCreated` |
| EXIF | `Exif.Photo.DateTimeOriginal` plus companion `Exif.Photo.SubSecTimeOriginal` and `Exif.Photo.OffsetTimeOriginal` when present | IPTC-MG `ExifIFD:DateTimeOriginal+SubSecTimeOriginal+OffsetTimeOriginal` |
| IIM | `Iptc.Application2.DateCreated` (`2:55`) plus `Iptc.Application2.TimeCreated` (`2:60`) | IPTC-TR “Date Created + Time Created” |
| XMP (same-tier) | `Xmp.exif.DateTimeOriginal` | MWG / ExifTool-MWG additional XMP encoding |

Parse into `umm::DateTime` without guessing absent components:

- EXIF civil time `YYYY:MM:DD HH:MM:SS`; sub-second digits; offset `±HH:MM` or `Z`.
- IIM date `YYYYMMDD` / `YYYY:MM:DD` / `YYYY-MM-DD` and time `HHMMSS` / `HH:MM:SS` with optional zone.
- XMP ISO-8601 / Photoshop forms (`YYYY-MM-DDTHH:MM:SS` with optional fraction and offset).

**Equivalence:** every component present on both values is equal. A date-only IIM
value is equivalent to a date-time whose date matches (extra time is more
complete, not a conflict). An absent offset is not equal to `Z` and is not
conflicted with another absent offset.

**Most complete merge** (equivalent path): union of present components.

**Disagreement:**

- Cross-family (XMP photoshop vs EXIF vs IIM): `reconciled`. Precedence:
  `Xmp.photoshop.DateCreated` > EXIF DateTimeOriginal family > IIM date+time
  (ExifTool-MWG `DateTimeOriginal` compatibility: XMP-photoshop first).
- Same-tier XMP (`photoshop:DateCreated` vs `exif:DateTimeOriginal`) with
  unequal normalized values: `conflict` (unranked).

`Exif.Image.DateTime` / `Exif.Photo.DateTimeDigitized` are **not** Date Created
(MWG distinguishes ModifyDate / CreateDate). They stay in `Metadata::raw()`.

**Write-sync:** write photoshop DateCreated, EXIF DateTimeOriginal (+ subsec +
offset when known), and IIM DateCreated+TimeCreated. Do not invent an offset.

### `iptc.photo.keywords` (text list)

| Family | Raw keys | Origin |
| --- | --- | --- |
| XMP | `Xmp.dc.subject` | IPTC-TR `dc:subject` |
| IIM | `Iptc.Application2.Keywords` | IPTC-TR `2:25` |

- Equivalence: set equality (order ignored). Stored order is the preferred
  group's order.
- Disagreement: `reconciled`, XMP > IIM (MWG Keywords; libumm does **not**
  silently union unequal sets — that would hide a source's extra terms).
- **Write-sync:** both representations; full canonical list.

### `iptc.photo.imageRating` (real)

| Family | Raw keys | Origin |
| --- | --- | --- |
| XMP | `Xmp.xmp.Rating` | IPTC-TR `xmp:Rating` |

Single representation in Phase 1. Other rating tags remain raw.
**Write-sync:** `Xmp.xmp.Rating` only.

### `exif.gps.position` (GPS coordinate)

Well-known Phase 1 id until an EXIF-domain registry exists (session 08).

| Family | Raw keys | Origin |
| --- | --- | --- |
| EXIF | `Exif.GPSInfo.GPSLatitude` + `GPSLatitudeRef` + `GPSLongitude` + `GPSLongitudeRef`; optional `GPSAltitude` + `GPSAltitudeRef`; optional GPS date/time | IPTC-MG GPS tags; EXIF 2.32 GPS IFD |
| XMP | `Xmp.exif.GPSLatitude` / `GPSLongitude` / optional `GPSAltitude` | IPTC-MG / EXIF XMP |

- Decimal degrees, WGS 84; west/south negative. Parse decimal, `deg/min/sec`,
  and EXIF rational triplets. Altitude ref 1 / “Below Sea Level” negates altitude.
- Equivalence: latitude/longitude within `1e-5` degrees; altitude within `0.5 m`
  when both present; missing altitude/time is not a conflict (prefix rule).
- Disagreement: `reconciled`, **EXIF GPS IFD > XMP-exif** (GPS is native EXIF;
  XMP is a projection).
- Named place is **not** this property (supported-types.md §3).
- **Write-sync:** EXIF GPS IFD plus XMP-exif lat/lon (and altitude when set).

### `iptc.photo.locationCreated` (structure list)

| Family | Raw keys | Origin |
| --- | --- | --- |
| XMP structured | `Xmp.Iptc4xmpExt.LocationCreated` | IPTC-TR Extension LocationCreated |
| XMP legacy | `Xmp.photoshop.City` / `State` / `Country` | IPTC-MG legacy Photoshop mapping |
| IIM legacy | `Iptc.Application2.City` (`2:90`), `ProvinceState` (`2:95`), `CountryName` (`2:101`) | IPTC-MG legacy IIM named place |

Phase 1 stores one `Structure` with fields `city`, `provinceState`,
`countryName` when only legacy tags exist (fixture `gps.jpg` /
`full-agreeing.jpg`). Structured Extension values win when present.

- Equivalence: shared fields equal after trim; extra fields on one side are
  more complete, not a conflict, if the shared fields match.
- Disagreement: `reconciled`, structured XMP > photoshop legacy > IIM.
- **Write-sync:** structured LocationCreated when the writer can; also legacy
  IIM + photoshop fields from `city` / `provinceState` / `countryName`. Full
  Extension location structures are Stage 6.

## Sidecars

A media file and an XMP sidecar with the **same stem** in the **same directory**
are one asset (concept.md §28). Pairing uses the `.xmp` extension. On
case-sensitive filesystems `.xmp` is tried first, then `.XMP`. On
case-insensitive filesystems the OS resolves the name. A path that is itself
`.xmp` is a standalone sidecar and is not paired with another sidecar.

`ReadOptions::merge_sidecar` (default true) reads the paired sidecar when it
exists and feeds it to reconciliation as an additional source. Sidecar files
are XMP carriers: only `Xmp.*` raw entries are used even if a backend also
projects EXIF/IIM copies. `false` reads embedded metadata only. A missing
sidecar is not an error. A sidecar that exists but cannot be parsed fails the
read.

`SourceRef::container` is `"embedded"` or `"sidecar"`. No source is dropped.

### Sidecar vs embedded precedence

Sidecar XMP and embedded XMP are **same-tier XMP**. They are grouped separately
so a sidecar `Xmp.dc.creator` is never concatenated with the embedded XMP list.
Read-only RAW types with `sidecar_recommended` (session 19) use this same
pairing: sidecar XMP is still same-tier as any embedded XMP; writes under
`StoragePolicy::preferred` go to the sidecar without a format-specific path.

- Equal after normalization: `equivalent` (or `reconciled` against IIM/EXIF if
  those disagree).
- Unequal: `conflict`. libumm does **not** pick a winner by mtime. The
  sidecar-newer vs embedded-newer situation is this conflict class (the
  `sidecar/paired.*` fixtures). `value` is the first group in document order
  (embedded XMP, then sidecar XMP, then IIM, then EXIF for default-ranked
  properties). `preferred_source` is that group's primary key. Every origin
  remains in `sources`.
- Sidecar XMP present and embedded XMP absent: sidecar XMP is the XMP group
  and still outranks IIM/EXIF (`reconciled` / `single` as usual).

`conflicts_as_errors` treats sidecar-vs-embedded `conflict` like any other.

### Sidecar write

`StoragePolicy::sidecar_only` and `sidecar_required` write **only** the XMP
representations of write-sync to the paired `.xmp` path (create if missing)
through the same temp + atomic rename path (M3). The JPEG file is not
modified. Mixed embedded+sidecar synchronization is Stage 8.
`sidecar_required` has the same Phase 1 write effect as `sidecar_only`; the
sidecar is required to be written.

## Cross-backend identity

Exiv2 and ExifTool must produce the same canonical **values** and
**classifications** for agreeing fixtures. `SourceRef::backend` differs by
adapter. Source key sets may differ when a backend omits an empty companion tag
(no `SubSecTimeOriginal` written). Documented GPS tolerance is `1e-5` degrees.
