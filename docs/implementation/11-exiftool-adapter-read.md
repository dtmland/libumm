# Session 11 — ExifTool stay_open adapter (read)

Stage 4 · Estimated 45–60 min (process plumbing on three OSes; the riskiest session)

## Goal

The out-of-process ExifTool adapter per decision S1a: persistent `-stay_open` process, JSON
output, mapped into the same `RawDocument` vocabulary as the Exiv2 backend.

## Prerequisites

Sessions 04, 10 merged.

## Deliverables

- `src/backends/exiftool/`:
  - **Process lifecycle:** spawn `<perl> <exiftool> -stay_open True -@ -` once per adapter
    instance; commands written to stdin ending with `-execute\n`; responses delimited by
    `{ready}`; `-stay_open False` + wait on shutdown; kill after a configurable timeout
    (default e.g. 30 s per command) with the process restarted on the next call; adapter
    survives and reports `backend_unavailable`/`internal` errors instead of hanging (contract
    from session 10 header).
  - **Read command:** `-j -G1 -struct -b`-style JSON read; parse (vendor a minimal JSON parser or
    a single-header library — decide inside the session, run the advisory check if adding a
    dependency); map `Group1:Tag` names into the `Exif./Iptc./Xmp.` raw vocabulary with an
    explicit translation table for the Phase 1 property set + passthrough naming for the rest
    (documented as adapter-specific keys).
  - **Encoding:** UTF-8 in/out (`-charset` flags as needed); non-ASCII **paths** handled on
    Windows (argfile is bytes — write UTF-8 and set `-charset filename=UTF8`).
  - **Discovery (decision S1c):** explicit configuration → `UMM_EXIFTOOL` env var → PATH;
    absence ⇒ `availability()` reports absent-with-reason; nothing fails at library load.
- Tests mirror session 10's read tests, same fixtures, ExifTool backend; plus: adapter restarts
  after a killed process (simulate by killing the child); absent-ExifTool path reports
  unavailable cleanly (point discovery at an empty dir).
- CI: tests receive the pinned script/perl paths via the existing CTest properties.

## Steps

1. Implement process manager (platform-specific spawn: CreateProcess vs fork/exec — keep it in
   one file with a narrow interface).
2. Implement read + key translation; unit-test translation table separately (no process needed).
3. Fixture tests on all three OSes; push; expect Windows iteration.

## Acceptance criteria

- Same fixture expectations pass under both backends (shared parameterized test source).
- 100 sequential reads reuse one process (assert via adapter counter) — no per-call spawn.
- Kill/timeout recovery test green; unavailable path green.

## Cut line

`-struct` structured-XMP fidelity may be limited to the Phase 1 properties; full struct fidelity
is revisited with IPTC Extension location structures in Stage 6.

## Out of scope

Writing (session 13); MWG composite tags (session 12 uses them only as a *reference*, not via
this adapter).

## References

Analysis decisions S1a, S1b, S1c, M1, M3(encoding); ExifTool `-stay_open` documentation.
