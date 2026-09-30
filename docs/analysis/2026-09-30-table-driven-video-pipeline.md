# Table-driven video reconcile/write-sync engine — 2026-09-30

Status: **accepted**. Session
[38](../implementation/38-video-pipeline-generalization.md) replaces the closed
Phase-1-sized video `if` chains with a registry-keyed engine. This record
supersedes the R3 deferral in `docs/reconciliation-policy.md` for video.

## Decision

Video read reconciliation and write-sync iterate **every non-deferred video id
in the generated cross-media map**, plus `exif.gps.position`. Rows come from
generated `PropertyDef` data (`representations.xmp` namespace+property,
datatype, and real `com.apple.quicktime.*` keys). Hand-written `ns:prop`
tokens stay forbidden in `src/` outside `src/generated/`.

Photo-side dispatch is unchanged.

## Shapes

| Registry datatype | Read | Write |
|---|---|---|
| lang-alt | XMP lang-alt; QuickTime plain text as `x-default` when a real QT key exists | XMP LangAlt + QT plain text |
| text / uri | XMP (and QT when mapped) as text | XMP string (+ QT when mapped) |
| date-time | XMP rank 0; QT `CreationDate` rank 1; movie-header `CreateDate` rank 2 for `dateCreated` | XMP + `CreationDate` (not movie-header) |
| structure | JSON / ExifTool struct; a plain URI string wraps as `{cvId}` | ExifTool `{Field=value}`, or a URI string when the structure is URI-like (PLUS CVs / `DigitalSourceType`) |
| structure list | JSON array / repeated structs; a name bag (multi-token XMP) wraps as Entity `name` | ExifTool `{Field=value}` structs (CVTerm lists reject a bare URI; Entity tags reject a bare display name), or a name bag |

## Special cases kept behavior-identical

- **creator:** `Xmp.dc.creator` names flatten to EntityWRole `name` (lang-alt
  `x-default`); QuickTime Artist/Author/Director are rank 1. Write emits
  `Xmp.dc.creator` + `QuickTime.Artist` only — not a second same-tier XMP
  Creator group.
- **keywords:** bag values join into lang-alt `x-default`; write splits commas
  to XMP bag + QT Keywords.
- **dateCreated:** movie-header `CreateDate` remains lowest rank.
- **GPS:** QuickTime `GPSCoordinates` outranks XMP (unchanged inversion).
- **featuredOrganisation:** two XMP tokens; names round-trip as a bag on the
  first token.

QuickTime is not written for structure/structure-list properties other than
creator. Prose registry QuickTime values (`See location structure + …`) are
ignored.

## ExifTool namespaces

Read maps Group1 `iptcExt` / `iptcCore` to registry namespaces `Iptc4xmpExt` /
`Iptc4xmpCore`. Write inverts that mapping. `dc`, `photoshop`, `plus`,
`xmpRights`, and unknown XMP groups stay pass-through.

## Precedence

Unchanged: **XMP > QuickTime item/keys > movie-header `CreateDate`**. Same-tier
disagreement is `conflict`. GPS remains container > XMP.
