# Session 53 — Parallel-read tests and ThreadSanitizer CI

## Goal

Prove the session 52 contract: N fixtures read from M threads give results identical to
serial reads, for both backends, under ThreadSanitizer in CI (U1 acceptance).

## Depends on

Session 52.

## Governing decisions

F1. Standing constraint S1b (both backends required in CI).

## Scope

**In:**
- `tests/backend/test_parallel_read.cpp`: read every Tier A fixture serially, then from
  M = 8 threads in random order, per backend (`ReadOptions::backend`), and compare
  `Metadata` and `dumpAll()` for equality. Include sidecar pairs and at least one MP4/MOV.
  Also run concurrent `write(..., dry_run = true)` and `capabilities()` calls.
- CMake option `UMM_SANITIZE` (`thread` | `address` | empty, default empty) in the
  top-level `CMakeLists.txt`, applied to libumm and tests. Exiv2 is built with the same
  flags, or a suppressions file covers it if its build cannot take them.
- `.github/workflows/ci.yml`: a `tsan (ubuntu-24.04)` job that configures with
  `UMM_SANITIZE=thread` and runs the backend tests. Update the workflow contract test
  (`tests/build/test_workflow.py`).
- Keep the test bounded (< 60 s per OS).

**Out:**
- Pooling (session 54); the test is reused unchanged there.

## Documentation

- `docs/sysadmin/install.md`: `UMM_SANITIZE` build option.

## Exit criteria

- `tsan` job green with no reports (or only documented Exiv2 suppressions).
- Parallel-read test green on all three CI OSes with both backends.
