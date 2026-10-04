# Session 47 — Cast engine and first rule set

## Goal

Implement up, down, and side casting as data-driven rules (C9–C11), the casting API
(C12a), and the first rule set. Reclassify the curated keys that no standard backs from
representations to casts (C7).

## Depends on

Session 46 (Location structs are the targets of several rules).

## Governing decisions

Decision record C7, C9, C10, C11, C12a, C19; §4 catalog rows marked "first".

## Scope

**In:**
- **Rule data:** `registry/casts/*.json` with a schema (rule: source, target, direction,
  conversion heuristic H1–H20, citation, `partial`, `approximate`; group: rules, source
  priority, one-way flag). Importer validation and generated C++ tables, byte-for-byte
  reproducible and LF-pinned (`.gitattributes`).
- **Headers first:** `umm::cast(path, CastDirection, CastOptions{dry_run, force, groups,
  include_approximate})` → `Result<CastReport>`; `CastStatus` (`can_cast`, `equal`,
  `needs_force`, `source_empty`, `target_not_storable`, `ambiguous`); `ReadOptions::
  report_casts` (default off) filling `Metadata::castCandidates()`; `WriteOptions::downcast`
  (default: `capturePosition` on video only).
- **Engine:** one code path for up, down, and side; heuristics H1–H14, H17, H18, H20.
  H15, H16 rules only with `include_approximate`; H19 and H21 not implemented.
- **First rule set:**
  - up: `capturePosition` (video; Keys `location.ISO6709` → UserData `©xyz` → EXIF GPS),
    `videoCreated` (movie-header `CreateDate`, `approximate`), `videoModified`,
    `recordingDevice` (Keys, EXIF, GoPro sources);
  - down: `capturePosition` (video);
  - side: legacy city/state/country/countryCode/sublocation ↔ `locationShownInTheImage[0]`;
    `personShownInTheImage` ↔ `personShownInTheImageWithDetails[*].name`; `creator` ↔
    `imageCreator[*].name`.
- **C7 reclassification:** movie-header `CreateDate` stops being a rank 2 `dateCreated`
  read fallback; QuickTime `GPSCoordinates` stops being read as video GPS directly.
- **Base-key group:** the ExifTool key translation keeps the QuickTime group (`Keys`,
  `UserData`, `ItemList`) in the base key (`src/backends/exiftool/keys.cpp`, about lines
  85–86 and 196–202) so rules can tell `location.ISO6709` from `©xyz`.
- **`dumpUnmapped()`:** flag entries that are sources of a cast rule.
- Tests per rule: dry run, apply, `equal`, `needs_force`, `force`, `source_empty`,
  `target_not_storable`, `ambiguous`; H3 merge never appends.

**Out:**
- `exif.gps.position` removal and geotag changes (session 48).
- Property map (session 49). Later rule sets (§4 "later").

## Documentation

- `docs/developer/canonical-model.md` (new): layer model L0–L3, base terminology,
  representation-versus-cast rule (C7), cast-rule schema, heuristics H1–H21, how to add a rule.
- `docs/developer/implementation-history.md`: standing constraints add the C7 rule and C17
  (no EXIF canonical domain); "Where the details live" lists the C-series records.
- `docs/user/guide.md`: "Casting" section (directions, statuses, dry run, defaults, the
  movie-header caveat from C19, legacy fields ↔ Location Shown with the MWG and
  "set afterwards" rationale).
- `registry/schema.md`: `registry/casts/` schema.
- `docs/umm-cli-concept.md`: `umm cast up|down|side`; `umm read` candidate hint.
- `docs/README.md`: link the canonical-model page.
- `docs/developer/release-notes.md`: casting API; movie-header-only videos and QuickTime GPS
  need an upcast; QuickTime group in base keys.

## Work items

1. Schema, importer, codegen, contract tests.
2. Headers, engine, statuses.
3. First rule set and C7 reclassification.
4. Base-key group preservation.
5. Tests and documentation.

## Exit criteria

- Every first-set rule has dry-run and apply tests on each backend that claims the target.
- A movie-header-only MP4 reads with no `dateCreated` and one `videoCreated` candidate
  when `report_casts` is on.
- Build contract tests cover `registry/casts/` reproducibility.
