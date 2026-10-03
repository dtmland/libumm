# Canonical properties and location — design review (2026-10-03)

Status: **analysis and recommendations only; no API or implementation decision taken here.**
This review distinguishes the intended standards vocabulary from what `umm::read`
currently materializes. Any change to the public contract should first amend the
relevant `include/umm/` headers and [reconciliation policy](../reconciliation-policy.md).

## Executive answer

**Canonical** means a standards-derived semantic property represented by a stable
libumm ID and a `PropertyValue`, not an accessor, a raw tag, or a guarantee that
every carrier and backend can read and write it. The registry currently contains
the 66 top-level IPTC Photo Metadata 2025.1 properties and the 105 top-level IPTC
Video Metadata Hub 1.7 properties in the [user guide](../user/guide.md#full-property-reference).
It also exposes one *well-known*, non-registry canonical value,
`exif.gps.position`. Thus the guide's claim that its tables list **every**
canonical property is false. The guide's reference to EXIF does **not** imply
that all EXIF tags have canonical IDs: a general EXIF-domain registry does not
exist. Some EXIF tags are instead *representations* of IPTC properties, and
EXIF GPS is assembled into the dedicated well-known coordinate value.

IPTC `Location` is **not just a place name**. Both photo and video standards
define structured locations with named-place fields and GPS latitude, longitude
and altitude. But *standard-defined structure* is not the same as *verified
end-to-end libumm support for every member*: the photo `locationCreated` read
special case only extracts city, province/state and country; its write special
case writes only those legacy fields. The separate `gps()` value does not
automatically populate an IPTC Location structure.

## 1. What belongs to which layer?

| Layer | Examples | Actual contract |
|---|---|---|
| Canonical property definition | `iptc.photo.locationCreated`, `iptc.video.locationShot` | Top-level entries in the generated `umm::Registry`; IDs, standard/version, datatype, cardinality and representation mappings. Photo and video IDs are distinct even if their XMP representation is shared. |
| Canonical well-known exception | `exif.gps.position` | A real `Metadata` property with `GpsCoordinate` value, `gps()`/`setGps()`, reconciliation and write-sync, but **not** a `Registry::find()` result. Its ID and coordinate shape are project-defined integration of standardized EXIF GPS concepts, pending an EXIF-domain registry. |
| Structured members | `iptc.photo.struct.Location.gpsLatitude`, `iptc.video.struct.Location.gpsLongitude` | Standard-derived fields *inside* Location values, described by registry JSON; not independent top-level `Metadata::propertyIds()` properties or entries in the generated `Registry::all()` table. |
| Representations | EXIF GPS IFD tags, `Xmp.exif.GPSLatitude`, `Xmp.Iptc4xmpExt.LocationCreated`, QuickTime `GPSCoordinates` | Carrier-specific keys; multiple representations may reconcile to one canonical value. EXIF Artist, for example, can represent IPTC Creator; that does not make every EXIF tag an IPTC property. |
| Convenience accessors | `locationCreated()`, `gps()`, photo-only `rating()` | Ways of reaching canonical values, not a second definition or exhaustive list of fields. |
| Raw entries | `Metadata::unmapped()` | Backend output retained separately from canonical properties. **Despite its name and guide wording, current reconciliation copies all raw entries here, including mapped inputs**; it is not currently a clean “only unmapped” partition. |

Evidence: [`include/umm/registry.hpp`](../../include/umm/registry.hpp),
[`include/umm/metadata.hpp`](../../include/umm/metadata.hpp),
[`src/metadata.cpp`](../../src/metadata.cpp),
[`src/core/reconcile.cpp`](../../src/core/reconcile.cpp),
[`src/core/property_ids.hpp`](../../src/core/property_ids.hpp),
[`src/generated/property_registry.cpp`](../../src/generated/property_registry.cpp),
and the [implementation history](../developer/implementation-history.md).
The [original concept](concept.md#10-dont-force-everything-into-iptc)
envisages additional EXIF/technical domains; that is a direction, not a
description of today's complete registry.

**Terminology recommendation:** document “defined in the registry,” “well-known
canonical exception,” “mapped on read,” “writable for this backend/type,” and
“raw entry” separately. Do not promise all 171 IPTC definitions are read or
round-tripped just because they appear in the property reference. Document
`Registry::find("exif.gps.position")` as absent until an EXIF-domain definition
is deliberately introduced. Do not silently rename its stable ID.

## 2. Does “most common properties” control behavior?

There is no separate “most common” runtime registry or read filter. The guide's
table is a historical **selection of early convenience accessors**, not a
property category: it even labels `iptc.photo.title` as `get`/`set` while
`title()`/`setTitle()` now exist. The Phase 2
[cross-media accessor decision](phase-2-video-convenience-accessors.md)
and [generated map](../../registry/mappings/cross-media-accessors.json) now
govern the photo↔video accessor pairs. `rating()` remains photo-only because
photo `imageRating` is an aesthetic number whereas video `rating` is a
classification structure; `gps()` is a separately implemented cross-media
exception not in that IPTC-generated map. These two exceptions are **not**
reasons to hide their canonical values.

There *is* a real, different selection in the implementation:
`mapped_photo_property_ids()` iterates non-deferred cross-media photo IDs plus
photo rating and GPS; `mapped_video_property_ids()` iterates non-deferred video
IDs plus GPS. Reconciliation loops over those lists, not over all registry
definitions. Accordingly a parseable property can be standards-defined and
accepted by generic `Metadata::set()`, yet not be populated by `umm::read`.
Write-sync handles selected special cases and generic IPTC IDs, subject to
backend/storage support; registry inclusion alone does not establish a
round-trip guarantee. This behavior is specified in the
[table-driven pipeline decision](2026-09-30-table-driven-video-pipeline.md)
and [reconciliation policy](../reconciliation-policy.md#table-driven-video-properties-session-38).

**Recommendation:** replace the “most common properties” section in the user
guide with an explicit “Accessor overview” linking the complete cross-media
catalog, photo-only rating and well-known GPS. Do not remove the existing
accessors or constrain the read pipeline to an editorial “common” list.
Track read/write coverage separately from the standards catalog; decide in a
future implementation review whether to generalize reconciliation to additional
registry IDs, with per-type/backend tests, rather than promising it here.

## 3. What should `read` return?

`umm::read` is a **C++ function**, not currently a shipped CLI command. It
loads backend raw metadata and an optional paired XMP sidecar, reconciles the
mapped IDs applicable to the sniffed media domain, then returns *only present
parseable canonical values* in `Metadata::propertyIds()`; it retains raw
entries separately in `Metadata::unmapped()`. A missing value does not appear
just because it is defined in `Registry::all()`. Provenance and conflicts
remain attached to the canonical value. The guide's example `read()` result
can contain `exif.gps.position` because this is an explicit canonical exception,
**not** because it is an unmapped GPS tag.

The separate [CLI concept](../umm-cli-concept.md#21-command-vocabulary-inspect-vs-mutate-dump-vs-property)
proposes `umm read` for a full canonical dump, `umm get` for selected IDs or
accessors, and `umm unmapped` for raw inspection; there is no CLI implementation
yet. If/when implemented, keep full canonical output available by default or
in machine-readable mode. A compact human summary emphasizing cross-media
concepts could be optional, clearly labeled and never substituted for the
full dump. Cross-media accessors are an ergonomic subset, not a reason to
discard valid photo-only or video-only values. Fix the raw-entry/“unmapped”
terminology or partition before claiming that the CLI's proposed `unmapped`
command returns *only* unrecognized tags.

## 4. GPS versus Location: two related but distinct claims

| Question | `exif.gps.position` | IPTC `Location` (`locationCreated`/`locationShot`, `locationShown`) |
|---|---|---|
| What is described? | One coordinate for the asset/capture position, as currently reconciled from EXIF/XMP-exif for stills and QuickTime/XMP-exif for video. | A potentially repeatable place *where captured* or a place *depicted*. The location can contain a name, city, country, identifiers **and** coordinates; edited video may have several shot locations. |
| Value/API | One `GpsCoordinate` with decimal latitude/longitude, optional altitude and GPS time; `gps()`/`setGps()`. | A list of `Structure` values; `locationCreated()` maps photo Location Created to video Location Shot, and `locationShown()` maps the depicted-place properties. |
| Current conversion | EXIF GPS IFD wins over XMP-exif on stills; QuickTime GPS wins over XMP-exif on video. `setGps()` writes those representations according to storage capabilities. | No general bidirectional conversion to/from `GpsCoordinate`. Photo Location Created's special read and write paths cover **named-place subset only**; video Location lists take the generic XMP structure path. Do not assert that nested GPS is preserved or synthesized on either path without tests. |

The IPTC photo `Location` struct lists `gpsLatitude`, `gpsLongitude`,
`gpsAltitude`, and `gpsAltitudeRef`, as well as names and administrative
geography. VMH `Location` lists latitude, longitude, altitude but no
`gpsAltitudeRef`; the cross-media setter drops that photo-only member on
video. The [Photo registry](../../registry/iptc-photo/iptc-photo.json)
and [Video registry](../../registry/iptc-video/iptc-video.json) explicitly
define these members. The [photo read special case](../../src/core/reconcile.cpp)
recognizes just city/state/country in structured Location Created; its
[write special case](../../src/core/write_sync.cpp) emits only the legacy
Photoshop/IIM city/state/country keys, not a structured XMP Location Created
value. The generic video structure reader/writer is not evidence of
standards-typed, multi-location nested-GPS fidelity across backends. The
[existing location policy](../reconciliation-policy.md#iptcphotolocationcreated-structure-list)
already calls full Extension structures deferred; documentation should be
equally explicit.

Practical guidance **today**: use `setGps()` for the supported single capture
coordinate path. Use full IPTC Location IDs/accessors to represent place
semantics, but do not depend on photo nested GPS round-tripping or automatic
GPS↔Location sync. Distinguish *where the camera was* from *what is shown*;
never infer a shown location from capture GPS (or vice versa).

## 5. About the quoted `Iptc4xmpExt` paths

`Iptc4xmpExt` is the established XMP namespace for IPTC Photo Metadata
Extension (`http://iptc.org/std/Iptc4xmpExt/2008-02-29/`), **not** a
libumm-specific newer version of IPTC or an indication of a separate
“4xmp” field family. Both imported registries map Photo Location Created and
VMH Location Shot to `Iptc4xmpExt:LocationCreated`, and the shown-location
pair to `Iptc4xmpExt:LocationShown`. The VMH standard has other representations
too (the registry includes EBUCore and descriptive QuickTime location
guidance); reusing an XMP path does not make the *canonical IDs* identical.

**Important correction to the quoted search result:** the imported **2025.1
Photo and VMH 1.7 registry data specify nested `exif:GPSLatitude`,
`exif:GPSLongitude`, `exif:GPSAltitude` members**, with namespace
`http://ns.adobe.com/exif/1.0/`; they do *not* label these GPS members
`Iptc4xmpExt:GPSLatitude` etc. A slash-delimited path in a tool's report
denotes traversal into the structured location and is not itself another
libumm property ID. The exact prefixes and serialized shape must be checked
against IPTC's technical reference and real XMP/ExifTool output before
publishing copy-paste paths. The namespace prefix is an alias for a URI, not
a standard version indicator.

## 6. Proposed follow-up, not authorized implementation

1. **Documentation first:** make the user guide's canonical/reference wording
   exhaustive and truthful (including the GPS exception and nested members),
   remove the misleading “most common” taxonomy, illustrate capture versus
   depicted location and the *currently supported* coordinate path, and
   distinguish registry availability from tested read/write coverage.
2. **Decide the intended GPS–Location relationship:** keep independent values
   (least surprising for multi-location/depicted content), or explicitly
   specify directional projection from capture GPS to the matching shot
   location, including multiple locations, provenance, conflicts, altitude
   units/reference and backend/storage differences. Do not conflate
   `locationShown` with camera position.
3. **If full nested Location support is desired:** define the precise
   standard-backed field types and XMP path semantics; specify photo/video
   cardinality, ExifTool/Exiv2 behavior, synchronization of structured and
   legacy location fields, and lossless versus lossy conversions in the
   reconciliation policy and normative headers before implementation. Test
   multiple shot/shown locations and nested coordinates with each backend and
   the JPEG/MP4/MOV paths; verify round trips instead of inferring them from
   the registry.
4. **Separately decide EXIF registry scope and raw semantics:** a complete
   EXIF-domain import is a new design decision, not a documentation fix.
   Inventory which standardized EXIF fields remain raw, specify collision
   policy with existing IPTC mappings and GPS, and only then extend the
   generated registry and read/write coverage. Decide whether `unmapped()`
   should truly filter mapped raw inputs; preserve the provenance and
   lossless read/write contract while changing it.

No production API, data, policy or user guide is changed by this analysis.
