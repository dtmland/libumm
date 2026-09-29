# Session 31 — Third-party notices and license compliance artifacts

Stage 11 · Estimated 45–60 min

## Goal

Produce the compliance artifacts every binary release must carry (analysis 2026-09-29 decision
**P1**): third-party notices with full license texts, the GPL-3.0 conveyance statement for
builds containing the Exiv2 backend, and a corresponding-source manifest tied to the pins.

## Prerequisites

Session 29 (there is an install tree to put the artifacts into). Session 30 recommended.

## Deliverables

- `THIRD-PARTY-NOTICES.md`: per component (Exiv2, Expat, zlib) — copyright line, license
  identifier, and where its full text lives; a statement that ExifTool is located at runtime and
  never distributed (S1a/S1c); a statement that binaries containing the Exiv2 backend are
  conveyed under GPL-3.0 while libumm's own code is Apache-2.0 (P1 analysis §4.2).
- `licenses/` directory with the verbatim texts: GPL-3.0, GPL-2.0 (Exiv2's base license, for
  reference), Expat/MIT, Zlib. Texts vendored from the pinned upstream archives, not retyped.
- Corresponding-source manifest (`tools/build/corresponding-source.json` or similar): for each
  statically absorbed dependency, the exact pinned upstream URL + SHA-256 (sourced from
  `backends.env` / `LibummExiv2.cmake` pins — single source of truth, no duplicated literals) so
  session 34 can attach the archives to releases mechanically.
- `NOTICE.md` finalized against the corrected GPL analysis (the interim wording fix landed with
  the 2026-09-29 analysis; this session makes it release-grade and adds pointers to the new
  artifacts).
- Install wiring: notices + license texts installed into the prefix (e.g. `share/doc/umm/`).
- Contract tests: notices mention every pinned component and version; the corresponding-source
  manifest checksums match the build pins (drift fails closed); license text files are non-empty
  and referenced from the notices.

## Steps

1. Extract license texts from the pinned dependency archives; write the notices.
2. Generate/author the corresponding-source manifest from the pin files.
3. Install rules + contract tests; green.

## Acceptance criteria

- A binary install prefix contains everything a distributor needs to convey the combined work
  under GPL-3.0 except the source archives themselves (attached at release time, session 34).
- Bumping a pin without updating the manifest/notices fails a contract test.
- No license text is hand-abridged.

## Cut line

Install-tree placement polish may defer; notices, texts, manifest, and drift tests may not.

## Out of scope

The release workflow that attaches corresponding-source archives (session 34); any relicensing
decision (S1d owner confirmation is a release-checklist item, session 34).

## References

analysis 2026-09-29 §4 (P1); decisions S1a, S1c, S1d, M4c; NOTICE.md; GPLv3 §4–§6 requirements
summarized in the analysis.
