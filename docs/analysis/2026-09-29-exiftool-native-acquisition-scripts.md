# ExifTool end-user acquisition: system-native scripts — 2026-09-29

Status: **accepted**. This document records a follow-up to
[2026-09-29-stage-10-review-release-and-licensing.md](2026-09-29-stage-10-review-release-and-licensing.md)
decision **P3**. P3's *yes* (a checksum-pinned end-user acquisition helper in session 33, preserving
S1c) stands. This document decides the **implementation vehicle**.

It follows the decision notation of
[2026-09-27-plan-review-and-decisions.md](2026-09-27-plan-review-and-decisions.md). The new finding
uses the next **P** (post-Stage-10) id.

---

## 1. Context

P3 requires a supported way for release consumers to obtain the pinned ExifTool without libumm
redistributing it. Session 33 was first drafted as a single Python program (`tools/get-exiftool.py`,
stdlib-only), matching *maintainer* tooling in this repo (registry importer/codegen, fixture
generator, offline contract tests).

That choice is wrong for an **end-user** helper. A consumer of a C++ library binary — or of the
future umm CLI — must not need a Python interpreter to fetch ExifTool. Python is already a
developer/CI dependency here; it is not a runtime of the library and must not become a runtime of
setup.

---

## 2. Finding

End-user acquisition is a product-facing install step, not a build-contract test. The right
precedent is a pair of **system-native** scripts, one per family of platforms, rather than a
third language runtime.

Inspiration: the multi-platform install layout of
[vlc-detective `install/`](https://github.com/dtmland/vlc-detective/tree/main/install)
(POSIX shell for Unix, PowerShell for Windows). This repo already uses POSIX `sh` for pin loading
(`tools/build/pins.sh`); Windows consumers cannot be assumed to have that shell, so PowerShell is
the native Windows counterpart.

**Decision P9 — ExifTool end-user acquisition is delivered as system-native scripts, not Python.
choice 2 of [1. keep a stdlib-only Python tool (`get-exiftool.py`) | 2. POSIX `sh` (Linux/macOS) +
PowerShell (Windows) under `tools/get-exiftool/` | 3. a single script language with optional
wrappers].**

Option 1 adds a Python requirement to every release user who wants the ExifTool backend. Option 3
still has a non-native core (Python/Perl/Node) and only hides it. Option 2 uses what the OS
already provides: `/bin/sh` (or equivalent) on Unix and Windows PowerShell 5.1+ on Windows. No
new dependency is introduced for the helper itself.

---

## 3. What P9 changes, and what it does not

P9 **replaces** the session-33 Python vehicle. It does **not** reopen:

| Still in force | Source |
|---|---|
| ExifTool located, never bundled | S1c |
| Out-of-process adapter only | S1a |
| Checksum-pinned helper; pin is `tools/build/backends.env` | P3 |
| SHA-256 fail-closed; no PATH / profile / system-dir mutation | P3 |
| Per-user prefix + printed `UMM_EXIFTOOL` / config wiring | P3 |
| Windows Perl is a separate prerequisite; point at the pinned Strawberry Perl | P3, M4b |
| Session 04 CMake FetchContent acquisition for CI/tests | session 04 |
| Python remaining valid for *maintainer/CI* tools and for **tests of** the scripts | sessions 02, 06, 07, 09 |

The umm CLI's `umm setup exiftool` (docs/umm-cli-concept.md §4.2) wraps **these** scripts and the
same pin, not a Python module.

---

## 4. Implementation shape (session 33)

### 4.1 Layout

    tools/get-exiftool/
      install.sh     # POSIX sh; Linux and macOS
      install.ps1    # Windows PowerShell 5.1+ (not pwsh-only)

The directory is the component named `tools/get-exiftool` in P3. Release archives ship the
directory plus the pin file (or the ExifTool keys from it). Do not add a Python driver, a
cmd/batch reimplementation of the PowerShell script, or a POSIX script that Windows is expected
to run via Git Bash.

### 4.2 Shared contract (both scripts)

Behavior must be equivalent; flags may follow each platform's convention (`--prefix` / `-Prefix`,
`--check` / `-Check`):

1. Parse `tools/build/backends.env` (version, SHA-256, URL template). **No duplicated pin
   literals.** Each script parses the env file itself: the PowerShell script must not shell out
   to `pins.sh`.
2. Download the pinned archive from upstream into a cache dir.
3. Verify SHA-256 **fail-closed** (mismatch = delete the bad file + non-zero exit).
4. Extract to a per-user prefix (`--prefix` / `-Prefix` override; defaults:
   `$XDG_DATA_HOME/umm/exiftool-<ver>` / `%LOCALAPPDATA%\umm\exiftool-<ver>` /
   `~/Library/Application Support/umm/exiftool-<ver>`).
5. Print exactly how to wire discovery: the `UMM_EXIFTOOL` line for the installed script path,
   and the explicit-config alternative.
6. On Windows, `install.ps1` checks for a usable `perl` and, if absent, prints the pinned
   Strawberry Perl guidance (version from `backends.env`) instead of pretending success.
7. `--check` / `-Check`: verify an existing installation against the pin (CI and, later, the
   CLI doctor).

Unix tools: `curl` (or fail with a clear "install curl" message), `tar`, and `sha256sum` or
`shasum -a 256`. Windows tools: `curl.exe` / `Invoke-WebRequest`, `tar`, `Get-FileHash`. Do not
fall back to Python if a native tool is missing.

The scripts write nothing outside the prefix and cache dir. They never modify PATH, shell
profiles, or system directories.

### 4.3 Tests

Offline contract tests stay Python `unittest` (stdlib, `tests/build/`), same family as
`test_pins.py`: they **invoke** the native scripts; they are not a second implementation.
Host-native script only (`.sh` on Unix, `.ps1` on Windows). Cover pin parsing, checksum
fail-closed on a tampered local archive, prefix layout, and `--check` / `-Check`. Network
download is opt-in CI, not the default matrix.

---

## 5. Document propagation

Session 33 is rewritten against this decision. Cross-references updated in
`docs/implementation/00-overview.md`, `docs/implementation-progress.md`,
`docs/implementation/04-exiftool-acquisition.md`, `docs/implementation/34-release-pipeline.md`,
`docs/umm-cli-concept.md`, a pointer from P3 in the 2026-09-29 Stage 10 analysis, and a "later"
note on S1c in the 2026-09-27 analysis. S1c itself is not revised.

---

## 6. Decision index

| ID | Decision | Outcome |
|---|---|---|
| P9 | End-user ExifTool acquisition vehicle | choice 2 — POSIX `sh` + PowerShell under `tools/get-exiftool/`; not Python; session 33 |
