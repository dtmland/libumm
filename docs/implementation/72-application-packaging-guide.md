# Session 72 — Application packaging and ExifTool guide

## Goal

Tell GUI application authors what to ship, how to provide corresponding source, and how
to locate and configure ExifTool (U7).

## Depends on

Session 52 (call-order rules). Session 54 for pool sizing text. Documentation-only; can
share a sitting with session 71.

## Governing decisions

F2. P1, P2 (licensing, corresponding source), S1c (locate, never bundle), P3/P9
(`tools/get-exiftool/`).

## Scope

**In:**
- `docs/sysadmin/install.md`, new section "Shipping an application that links libumm":
  - notices to ship (`THIRD-PARTY-NOTICES`, vendored license texts) for static Exiv2
    (GPL-3.0-governed binary) versus `UMM_EXIV2_SHARED`;
  - how to provide corresponding source (the existing manifest and fetch tools in
    `tools/build/`);
  - locating ExifTool: explicit `ExifToolConfig` path from app settings, `UMM_EXIFTOOL`,
    PATH; offering `tools/get-exiftool/` to users; Windows `.exe` vs Perl packaging;
  - reconfiguring at runtime (F2) and pool sizing (session 54);
  - surfacing ExifTool absence in the UI via `propertyAccess` reasons (session 57).
- Link from `docs/user/guide.md` "Threads" and from `docs/README.md` if a new page is
  created instead of a section.

**Out:**
- Legal advice; the guide states facts about the shipped artifacts and points to the
  licenses.

## Documentation

- As above; `tests/build/test_notices.py` still passes (no notice changes expected).

## Exit criteria

- Every statement cites a file or tool in the repository.
