# Session 44 — Read coverage for every registry id

## Goal

`umm::read` reconciles **every registry id for the file's domain** (C5), not just the ids in
the cross-media accessor map. The accessor map decides only which ids get short names.
Also land the Tier 1 `rating` accessor (C3c) and the photo `dateCreated` source fixes (C16,
C19).

## Depends on

Session 43 (terminology; `dumpUnmapped()` then shrinks automatically as coverage grows).

## Governing decisions

Review record C3c, C5; decision record C7 (IFD0 row), C16, C19.

## Scope

**In:**
- Replace `mapped_photo_property_ids()` / `mapped_video_property_ids()`
  (`src/core/xmp_codec.cpp`) with lists generated from the registry for each domain. The
  existing table-driven engine handles the shapes. Remaining id-specific branches each name
  the reconciliation-policy section that justifies them (C3b).
- Legacy location ids (`cityLegacy`, `provinceOrStateLegacy`, `countryLegacy`,
  `countryCodeLegacy`, `sublocationLegacy`) reconcile under their own ids from IIM and
  `photoshop:`/`Iptc4xmpCore:` keys. Their link to `locationCreated` is removed in session 46.
- **Rating (C3c):** add a Tier 1 row `rating` (`iptc.photo.imageRating` ↔
  `iptc.video.workflowRating`) to `registry/mappings/cross-media-accessors.json`;
  regenerate; `rating()` / `setRating()` become cross-media. Update the `metadata.hpp`
  comments that say "photo-only".
- **Photo `dateCreated`:** remove `Exif.Image.DateTime` (the modify date) from the candidate
  list; add IFD0 `DateTimeOriginal` (`Exif.Image.DateTimeOriginal`, as DNG writes it) after
  the ExifIFD copy. Today's candidates are in `src/core/reconcile.cpp` (about lines 1390–1415).
- Tests: a parameterized read test asserting every registry id written by `set()` comes back
  from `read` on JPEG and MP4; the DNG IFD0 case using `tests/fixtures/raw/`; `rating`
  on both domains.

**Out:**
- Location struct GPS fields (session 46).
- `exif.gps.position` stays until session 48.
- Casts (session 47).

## Documentation

- `docs/user/guide.md`: intro to canonical properties: canonical = IPTC Photo 2025.1 + VMH 1.7
  registry ids; EXIF, IIM, and QuickTime are representations, not separate canonical
  properties. Remove `rating` from the "most common properties" list and add it to the
  Tier 1 table. Remove the statement that only some ids read back. Time fields: taken versus
  digitized versus modified (C16).
- `docs/reconciliation-policy.md`: precedence for the newly reconciled ids (default rule
  plus any exceptions); the `dateCreated` candidate change.
- `docs/developer/release-notes.md`: more properties returned by `read`; `rating` on video;
  `dateCreated` no longer falls back to the modify date.

## Work items

1. Generated per-domain id lists and engine wiring.
2. Rating map row and codegen.
3. `dateCreated` candidate changes.
4. Tests and documentation.

## Exit criteria

- Every non-struct-GPS registry id round-trips through `set` → `write` → `read` on JPEG and
  MP4 with both backends.
- A JPEG with only `Exif.Image.DateTime` reads with no `dateCreated`.
- `test_cross_media_accessors.py` catalog contract passes with the new `rating` row.
