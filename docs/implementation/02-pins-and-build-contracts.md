# Session 02 — Pins and offline build contracts

Stage 0 · Estimated 30–45 min

## Goal

Single-source backend version pins plus offline Python contract tests that fail closed on
malformed pins or workflow drift — before any CI exists to depend on them.

## Prerequisites

Session 01 merged.

## Deliverables

- `tools/build/backends.env` — the **only** place backend versions/checksums live:

  ```text
  UMM_EXIV2_VERSION=<pin>
  UMM_EXIV2_SHA256=<sha>
  UMM_EXIFTOOL_VERSION=<pin>
  UMM_EXIFTOOL_SHA256=<sha>
  UMM_STRAWBERRY_PERL_VERSION=<pin>   # Windows provisioning (decision M4b)
  ```

  Choose current stable Exiv2 and ExifTool releases at implementation time; record real SHA-256s
  of the source archives.
- `tools/build/pins.sh` — POSIX sh; loads `backends.env`, validates every expected key is present
  and every SHA is 64 hex chars, exports to environment / appends to `GITHUB_ENV` when set;
  non-zero exit on any missing/malformed entry (fail closed, build-plan principle 4).
- `tools/build/linux-packages.txt` — apt prerequisites (ninja-build, perl, zlib/expat dev packages
  for Exiv2 source build, ccache), one per line, comments allowed.
- `tests/build/` Python `unittest` suite (stdlib only, no pip installs):
  - `test_pins.py`: backends.env parses; keys complete; SHA format valid; versions non-empty;
    a deliberately malformed temp copy is rejected by `pins.sh` (subprocess).
  - `test_layout.py`: required files exist (`CMakePresets.json`, `tools/build/pins.sh`,
    `tools/build/linux-packages.txt`); presets JSON parses and contains `default` configure /
    build / test presets with `noTestsAction: error`.
  - (Workflow-reference checks land in session 03 when the workflow exists.)

## Steps

1. Pick and record pins with real checksums (`curl -sL <src archive> | sha256sum`).
2. Write `backends.env`, `pins.sh`, `linux-packages.txt`.
3. Write the two unittest modules; run `python3 -m unittest discover -s tests/build -v`.
4. Confirm build/ctest still green.

## Acceptance criteria

- `python3 -m unittest discover -s tests/build -v` passes offline.
- Corrupting any pin value makes `test_pins.py` fail.
- `pins.sh` exits non-zero on a missing key.

## Cut line

If checksum retrieval is blocked, commit the structure with clearly marked `TODO-pin` values plus
a contract test asserting no `TODO-pin` remains **allowed to fail closed in CI later** — and note
it in the session summary. Prefer real pins.

## Out of scope

Actually downloading/building backends (sessions 04–05); the CI workflow (session 03).

## References

build-plan.md §5, §9.3, §14 step 3; analysis decisions M4a, M4b.
