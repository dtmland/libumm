# Session 33 — ExifTool end-user acquisition tool

Stage 11 · Estimated 45–60 min

## Goal

Give consumers of releases a supported way to obtain the pinned ExifTool without libumm ever
redistributing it (analysis 2026-09-29 decision **P3**; preserves S1c). The helper is a pair of
**system-native** scripts (decision **P9**), not Python: POSIX `sh` on Linux/macOS and PowerShell
on Windows. The same scripts are what the umm CLI's `umm setup exiftool` will wrap
(docs/umm-cli-concept.md §4.2).

## Prerequisites

None beyond Stage 10 (independent of sessions 29–32); scheduled here so session 34 can include
them in release archives.

## Deliverables

- `tools/get-exiftool/install.sh` (POSIX `sh`, Linux and macOS) and
  `tools/get-exiftool/install.ps1` (Windows PowerShell 5.1+, not pwsh-only). Layout and contract
  follow analysis P9; inspired by per-OS native install scripts rather than a cross-platform
  interpreter. Equivalent behavior; flags may be `--prefix`/`--check` vs `-Prefix`/`-Check`:
  1. each script parses the pin (version, SHA-256, URL template) from
     `tools/build/backends.env` itself — no duplicated literals, and the PowerShell script must
     not shell out to `pins.sh`;
  2. downloads the pinned archive from upstream into a cache dir, using native tools (`curl` /
     `sha256sum` or `shasum -a 256` / `tar` on Unix; `curl.exe` or `Invoke-WebRequest` /
     .NET SHA256 / `tar` on Windows). Missing tools fail with an install hint — no Python
     fallback. Windows hashing uses `[System.Security.Cryptography.SHA256]` rather than
     `Get-FileHash`, which is missing on some PowerShell 5.1 hosts (including GitHub Actions
     `windows-2025`);
  3. verifies SHA-256 **fail-closed** (mismatch = delete + non-zero exit);
  4. extracts to a per-user prefix (`--prefix` / `-Prefix` override; defaults:
     `$XDG_DATA_HOME/umm/exiftool-<ver>` / `%LOCALAPPDATA%\umm\exiftool-<ver>` /
     `~/Library/Application Support/umm/exiftool-<ver>`);
  5. prints exactly how to wire discovery: the `UMM_EXIFTOOL` environment line for the installed
     script path, and the explicit-config alternative;
  6. on Windows, `install.ps1` checks for a usable `perl` and, if absent, prints the pinned
     Strawberry Perl guidance (version from `backends.env`) instead of pretending success.
- `--check` / `-Check` mode: verify an existing installation against the pin (used by CI and,
  later, by the CLI's doctor command).
- Offline tests (Python `unittest` in `tests/build/`, same style as `test_pins.py`): the tests
  **invoke** the host-native script (`.sh` on Unix, `.ps1` on Windows); they are not a second
  implementation. Cover pin parsing, checksum fail-closed on a tampered local archive, prefix
  layout, `--check` / `-Check` behavior. Network download is exercised only in an opt-in CI step
  (reuse the existing ExifTool cache pattern), not in the default matrix.
- Docs: README "getting ExifTool" section for release users (which script to run on which OS);
  note in the discovery docs that the scripts write nothing outside the prefix and cache dir.

## Steps

1. Implement pin parsing + download/verify/extract in both scripts, with fail-closed tests on
   the host-native script.
2. Add `--check` / `-Check`, per-OS prefix defaults, and Windows Perl guidance.
3. Docs + optional CI step + contract tests; green.

## Acceptance criteria

- On a clean machine **without Python** and (on Windows) with Perl, the host-native script yields
  a working pinned ExifTool that the ExifTool backend discovers via the printed wiring.
- Checksum tampering fails closed; the scripts never modify PATH, shell profiles, or system dirs.
- The pin remains single-sourced in `backends.env`.
- Neither script requires Python, Git Bash on Windows, or pwsh (PowerShell 7) specifically.

## Cut line

The opt-in networked CI step may defer; both scripts, fail-closed tests, and docs may not.

## Out of scope

Bundling ExifTool in any artifact (forbidden, S1c); installing Perl for the user; a native
Windows-exe ExifTool pin (recorded as umm-cli open question 3); a Python or cmd.exe
reimplementation of the helper.

## References

analysis 2026-09-29 §4 (P3); analysis 2026-09-29-exiftool-native-acquisition-scripts.md (P9);
decisions S1a, S1c, M4b; tools/build/backends.env; tools/build/pins.sh (POSIX parsing style);
src/backends/exiftool/ discovery order; docs/umm-cli-concept.md §4.2.
