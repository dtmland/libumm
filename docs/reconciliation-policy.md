# Reconciliation policy (Phase 1)

Status: **normative** for JPEG embedded read (session 12), write-synchronization
(session 13), XMP sidecar pairing (session 14), MP4/MOV video read
(session 21), ExifTool-only MP4/MOV write (session 22), the conflict
resolution API (session 23), `synchronize()` / mixed storage (session 24),
and the session 28 cross-backend comparison suite.

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
(MWG distinguishes ModifyDate / CreateDate). They stay in `Metadata::unmapped()`.

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

### Table-driven photo properties (session 39)

Mapped `iptc.photo.*` ids beyond the Phase 1 specials (for example title,
contributor, shownEvent, genre, dataMining, aiSystemUsed) are collected from
XMP using the same datatype dispatch as video. Write-sync for those extras is
XMP-only; IIM/EXIF families remain the Phase 1 specials listed above.

## Accessor transposition (session 40)

Convenience accessors store the **domain-native** shape. Getters return that
stored value; they do not normalize to the photo type. Setters take the
photo-native convenience type and transpose when `mediaDomain()` is `video`.

| Kind | Photo → video | Lossy notes |
| --- | --- | --- |
| `string_to_lang_alt` (headline) | string → `x-default` lang-alt | Reverse keeps `x-default` (or the sole language); extra languages drop |
| `string_list_to_lang_alt` (keywords) | bag joined with `", "` into `x-default` | Reverse splits on `", "` (same rule as video keyword reconcile) |
| `lang_alt_to_string` (otherConstraints) | `x-default` (or sole language) → string | Extra languages drop |
| `names_to_entity_list` (creator) | name strings → EntityWRole `{name}` lang-alt | Reverse reads `name`; role is not reconstructed |
| `uri_to_cv_term` (digitalSourceType, model/propertyReleaseStatus) | URI → `{cvId}` | Reverse reads `cvId` (and URI-like aliases) |
| `struct_field_subset` (copyrightOwner) | keep `name`/`identifiers` (PLUS aliases mapped) | Video-only `role` and other PLUS fields drop |
| `list_to_single` (licensor) | one Licensor/Entity → single Entity | Setter with 0 or >1 entries on video is `invalid_value`; extras stay only under the full property id |

## Video (MP4/MOV)

Sniffed file types `MP4` and `MOV` select the `iptc.video.*` domain instead of
`iptc.photo.*`. Shared XMP encodings (`dc:description`, `photoshop:DateCreated`)
must not populate photo properties on a video file.

### Read precedence (video)

**XMP > QuickTime item/keys metadata > movie-header `CreateDate`**

Cross-family disagreement is `reconciled` with that ranking. Same-tier
disagreement remains `conflict`. GPS is native to the container and inverts
XMP vs QuickTime (below).

Movie-header `QuickTime.CreateDate` is often stored as UTC with **no offset
field**. libumm does **not** invent UTC (`+00:00`) for a naive header date.
`Keys:CreationDate` (`com.apple.quicktime.creationdate`) may include an
offset; missing vs present offset stays equivalent (`opt_equal`).

### Table-driven video properties (session 38)

Every non-deferred video id in the generated cross-media map is reconciled and
write-synced from registry `PropertyDef` rows (XMP namespace+property, datatype,
and real `com.apple.quicktime.*` keys). See
[2026-09-30-table-driven-video-pipeline.md](analysis/2026-09-30-table-driven-video-pipeline.md).

| Shape | Read | Write |
| --- | --- | --- |
| lang-alt | XMP; QT plain text as `x-default` when mapped | XMP LangAlt + QT plain text |
| text / uri | XMP (and QT when mapped) | XMP string (+ QT when mapped) |
| date-time | XMP rank 0; QT `CreationDate` rank 1; movie-header `CreateDate` rank 2 for `dateCreated` | XMP + `CreationDate` |
| structure | JSON / ExifTool struct; a plain URI wraps as `{cvId}` | ExifTool struct, or the URI string when URI-like |
| structure list | JSON array / repeated structs; a name bag wraps as Entity `name` | structs, name bag, or URI strings |

Phase-1 specials stay behavior-identical:

| Family | Raw keys |
| --- | --- |
| XMP | Registry `xmp_property` (plus `Xmp.dc.creator` names flattened to EntityWRole `name`) |
| QuickTime | `QuickTime.Title`, `Description`, `Artist`/`Author`/`Director` (creator names), `Copyright`, `Keywords`, `CreationDate` |
| Movie header | `QuickTime.CreateDate` (date only; lowest rank) |

`iptc.video.title` / `description` / `copyrightNotice` / `keywords` are lang-alt.
`iptc.video.creator` is a structure list (`name` only for Phase 1 string sources).
`iptc.video.keywords` joins bag values into `x-default` (VMH types the property as lang-alt).
Creator write emits `Xmp.dc.creator` + `QuickTime.Artist` only (not a second same-tier XMP Creator group).

**Write-sync:** each property expands to its XMP encoding plus a QuickTime key
when the registry names a real apple key (scalar types only, plus creator).
`umm::write` then keeps the families named by `StorageDecision::formats`.
MP4/MOV ExifTool rows expose XMP plus `container_gps` (QuickTime); requesting
`backend: "exiv2"` is `unsupported_capability`. Sidecar-only writes keep XMP
only, as for stills.

### `exif.gps.position` on video

| Family | Raw keys |
| --- | --- |
| QuickTime | `QuickTime.GPSCoordinates` (`lat lon [alt]`, comma or whitespace; ISO 6709 accepted when decimal) |
| XMP | `Xmp.exif.GPSLatitude` / `GPSLongitude` / optional altitude |

Disagreement: `reconciled`, **container GPS > XMP**. Equivalence uses the same
degree/altitude tolerances as stills.

**Write-sync:** `QuickTime.GPSCoordinates` as `lat, lon[, alt]` plus XMP-exif
lat/lon (and altitude when set). EXIF GPS IFD is also produced by the shared
GPS writer and is dropped when the storage decision does not list EXIF (MP4/MOV).

### R3 (per-property dispatch)

Session 38 replaced the closed Phase-1-sized video `if` chains with a
table-driven engine over generated `PropertyDef` rows and the cross-media map.
Session 39 feeds the same engine the remaining mapped `iptc.photo.*` ids
(XMP-only write-sync for those extras). The original R3 deferral is superseded
by
[2026-09-30-table-driven-video-pipeline.md](analysis/2026-09-30-table-driven-video-pipeline.md).

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
sidecar is not an error unless `ReadOptions::sidecar_required` is true, in
which case the read fails with `ErrorCode::io_not_found`. A sidecar that
exists but cannot be parsed fails the read.

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

`StoragePolicy::sidecar_only` writes **only** the XMP representations of
write-sync to the paired `.xmp` path (create if missing) through the same
temp + atomic rename path (M3). The media file is not modified.

`StoragePolicy::sidecar_required` always writes the sidecar. If embedded
writes are also available and the type is not `sidecar_recommended`,
`evaluateStorage` returns `Method::mixed` and `umm::write` updates embedded
formats and the XMP sidecar in one call (embedded first, then sidecar). Types
with `sidecar_recommended` (read-only RAW) stay sidecar-only. A mixed write
that fails to produce the sidecar is an error even if the embedded file was
already replaced; there is no multi-file rollback.

### Synchronization

`synchronize(path, options)` reads (or uses `SyncOptions::metadata` from
`merge`), then writes the canonical state so the selected carriers agree.

| `SyncDirection` | Read | Write |
| --- | --- | --- |
| `both` (default) | merged embedded + sidecar | embedded (when writable) and sidecar |
| `embedded_to_sidecar` | embedded only | sidecar only (media bytes unchanged) |
| `sidecar_to_embedded` | sidecar only | embedded only |

`both` fails with `conflict_unresolved` when any property is still
`Resolution::conflict`; call `merge` first. Each carrier write uses
`mutate_file_atomically`. If the embedded write succeeds and the sidecar
write fails, the error message reports that partial state and the embedded
file is left updated.

## Conflict resolution API

`detectConflict(path, options)` runs the same read pipeline as `umm::read`
(backends, sidecar pairing, grouping, classification). It does **not** add a
second reconciliation engine. The report contains:

- `metadata` — the same canonical `Metadata` `read()` would return (when the
  call succeeds).
- `entries` — one `ConflictEntry` per canonical property whose groups
  **disagreed**. That is both `Resolution::conflict` (unranked same-tier, e.g.
  embedded XMP vs sidecar XMP) and `Resolution::reconciled` (policy already
  picked a winner, e.g. XMP vs IIM vs EXIF on `full-conflicting`, or XMP vs
  QuickTime on `video/conflicting.mp4`). `single` and `equivalent` are omitted.

Each entry lists every group's parsed `Value` with its `SourceRef`s (`raw_key`,
`backend`, `container`) and grouping `family`, plus the policy's
`preferred_source` (the winning group's primary raw key). Same-family embedded
disagreement, embedded-vs-sidecar disagreement, and video container-vs-XMP
disagreement share this one enumeration path.

`ReadOptions::conflicts_as_errors` has the same meaning as for `read`: if any
property is `conflict`, the call fails with `ErrorCode::conflict_unresolved`.
`reconciled` disagreements are not errors; they still appear in `entries` when
the call succeeds.

`merge(metadata, entry, source, container = {})` resolves one disagreed
property by choosing a candidate already listed in `entry`. `source` matches a
`SourceRef::raw_key` on that entry. When the same raw key exists in more than
one candidate (embedded XMP vs sidecar XMP), `container` must be `"embedded"`
or `"sidecar"`. The property's `value` becomes that candidate's value;
`resolution` becomes `reconciled`; `preferred_source` is `source`. **Every**
source already recorded on the property remains listed — losing candidates are
never dropped. This does not write files (session 24).

`merge(metadata, property_id, value)` is a user-supplied override. `resolution`
becomes `reconciled`; `preferred_source` is **empty** (empty means user-supplied,
not a raw key); existing sources are retained. This is the documented equivalent
of a preferred-source choice when the application supplies a new value.

`Metadata::set(property_id, Value)` still replaces provenance with
`Resolution::single` and empty sources. Applications that need to keep
candidates must use `merge`.

## Accessor transposition

Convenience accessors store the **domain-native** property (no shadow
storage). Transposition runs only inside the accessor; `get`/`set` with a
full registry id is unchanged. Getters return the stored domain value except
`shownEvent()`, which always assembles an Entity list.

| Accessor | Photo storage | Video storage | Lossy direction |
| --- | --- | --- | --- |
| `locationCreated` | `iptc.photo.locationCreated` Location list, including `gpsAltitudeRef` | `iptc.video.locationShot`; `gpsAltitudeRef` is dropped on set | Photo → video drops `gpsAltitudeRef` (absent from the VMH Location struct). |
| `shownEvent` | `eventName` (lang-alt) + `eventIdentifier` (uri list) | one Entity in `iptc.video.shownEvent` | Photo can represent one name; extra video entities stay reachable only via the full id. |
| `featuredOrganisation` | string list | Entity list (`name` only) | Video roles/identifiers are not representable on the photo accessor. |
| `supplier` | ImageSupplier list | single Entity (`name`/`identifiers`) | Setter with more than one entry on video returns `invalid_value`. |

`objectShown` remains deferred: ArtworkOrObject → Entity would keep only
`title`↔`name`.

## Cross-backend identity

Exiv2 and ExifTool must produce the same canonical **values** and
**classifications** for agreeing fixtures. `SourceRef::backend` differs by
adapter. Source key sets may differ when a backend omits an empty companion tag
(no `SubSecTimeOriginal` written). Documented GPS tolerance is `1e-5` degrees;
altitude within `0.5 m` when both present; missing vs present offset or altitude
stays equivalent (`opt_equal` / prefix rule).

Session 28 turns this into a maintained suite: write with one backend, read with
the other, and compare canonical `Metadata` (values, families the reader can see,
provenance classification). Byte-equality is not the goal. Capability data skips
pairs where a backend does not claim access. Accepted mismatches are listed in
`tests/verification/ledger.json` with a reason; unexplained divergence fails CI.
