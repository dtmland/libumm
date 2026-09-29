# Session 33 — ExifTool end-user acquisition tool

Stage 11 · Estimated 45–60 min

## Goal

Give consumers of releases a supported way to obtain the pinned ExifTool without libumm ever
redistributing it (analysis 2026-09-29 decision **P3**; preserves S1c). The same tool is the
component the umm CLI's `umm setup exiftool` will wrap (docs/umm-cli-concept.md §4.2).

## Prerequisites

None beyond Stage 10 (independent of sessions 29–32); scheduled here so session 34 can include it
in release archives.

## Deliverables

- `tools/get-exiftool.py` (Python, stdlib-only, matching the repo's tooling conventions):
  1. reads the pin (version, SHA-256, URL template) from `tools/build/backends.env` — no
     duplicated literals;
  2. downloads the pinned archive from upstream into a cache dir;
  3. verifies SHA-256 **fail-closed** (mismatch = delete + non-zero exit);
  4. extracts to a per-user prefix (`--prefix` override; defaults:
     `$XDG_DATA_HOME/umm/exiftool-<ver>` / `%LOCALAPPDATA%\umm\exiftool-<ver>` /
     `~/Library/Application Support/umm/exiftool-<ver>`);
  5. prints exactly how to wire discovery: the `UMM_EXIFTOOL` environment line for the installed
     script path, and the explicit-config alternative;
  6. on Windows, checks for a usable `perl` and, if absent, prints the pinned Strawberry Perl
     guidance (version from `backends.env`) instead of pretending success.
- `--check` mode: verify an existing installation against the pin (used by CI and, later, by the
  CLI's doctor command).
- Offline tests (same style as the existing pin/build contract tests): pin parsing, checksum
  fail-closed on a tampered local archive, prefix layout, `--check` behavior. Network download is
  exercised only in an opt-in CI step (reuse the existing ExifTool cache pattern), not in the
  default matrix.
- Docs: README "getting ExifTool" section for release users; note in the discovery docs that the
  tool writes nothing outside its prefix.

## Steps

1. Implement pin parsing + download/verify/extract with fail-closed tests.
2. Add `--check`, per-OS prefix defaults, and Perl guidance.
3. Docs + optional CI step + contract tests; green.

## Acceptance criteria

- On a clean machine with Python and (on Windows) Perl, the tool yields a working pinned
  ExifTool that the ExifTool backend discovers via the printed wiring.
- Checksum tampering fails closed; the tool never modifies PATH, shell profiles, or system dirs.
- The pin remains single-sourced in `backends.env`.

## Cut line

The opt-in networked CI step may defer; the tool, fail-closed tests, and docs may not.

## Out of scope

Bundling ExifTool in any artifact (forbidden, S1c); installing Perl for the user; a native
Windows-exe ExifTool pin (recorded as umm-cli open question 3).

## References

analysis 2026-09-29 §4 (P3); decisions S1a, S1c, M4b; tools/build/backends.env;
src/backends/exiftool/ discovery order; docs/umm-cli-concept.md §4.2.
