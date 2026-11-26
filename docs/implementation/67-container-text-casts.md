# Session 67 — Container text casts (`pngText`, `quickTimeUserData`)

## Goal

Read PNG text chunks and QuickTime user data into canonical title, creator, description,
copyright, and date created (U12, part 2).

## Depends on

Session 62. Run after session 66 (same registry files and generator).

## Governing decisions

F12, F9. C9, C10, H12 (dates without offset stay without offset).

## Scope

**In:**
- `registry/casts/png-text.json` (`Title`, `Author`, `Description`, `Copyright`) and
  `registry/casts/quicktime-user-data.json` (`©nam`, `©ART`, `©day`, `©des`). Up only.
  Note that Exiv2 PNG is EXIF-blind but reads text chunks; confirm the base keys for both
  backends.
- `©day` parsing: partial-precision `DateTime`; no offset invented.
- Fixtures: PNG with `tEXt` and `iTXt` chunks; MOV/MP4 with user data atoms (generator,
  MANIFEST).
- Tests as in session 66.

**Out:**
- Writing the container text carriers.

## Documentation

- `docs/user/guide.md` "Casting": new groups.

## Exit criteria

- Fixtures produce the expected canonical values on each backend that reads the source;
  gaps in `tests/verification/ledger.json`.
