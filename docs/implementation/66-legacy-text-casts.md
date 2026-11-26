# Session 66 — Legacy EXIF text casts (`windowsXP`, `exifUserComment`)

## Goal

Read Windows `XP*` tags and EXIF `UserComment` into canonical title, description,
keywords, and creator (U12, part 1).

## Depends on

Session 62.

## Governing decisions

F12, F9. C9, C10 (heuristics; `exifUserComment` approximate).

## Scope

**In:**
- `registry/casts/windows-xp.json` (`XPTitle` → title, `XPComment` / `XPSubject` →
  description, `XPKeywords` → keywords split on `;`, `XPAuthor` → creator split on `;`)
  and `registry/casts/exif-user-comment.json` (→ description, approximate). Up only.
  Regenerate `src/generated/` and the property reference.
- Decoding: `XP*` UCS-2LE byte arrays; `UserComment` 8-byte charset prefix. Confirm both
  backends hand over the same base value; normalize in the cast conversion if not.
- Fixtures: JPEG written with Windows-style `XP*` tags and `UserComment` (generator with
  ExifTool, `tests/fixtures/MANIFEST.md`).
- Tests: `castCandidates()` lists the groups; `upcast` fills the expected values; existing
  canonical values win (status `equal` / `needs_force`, never applied on read).

**Out:**
- Writing or removing the legacy tags (F12; session 68 documents the policy).

## Documentation

- `docs/user/guide.md` "Casting": new groups.
- `docs/developer/canonical-model.md`: rule list.

## Exit criteria

- Fixtures produce the expected canonical values on both backends.
