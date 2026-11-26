# Session 56 — ExifTool in-flight cancellation

## Goal

Cancel a running ExifTool read or write within a bounded time, without leaving a zombie
process, and keep the next call working (U9 acceptance).

## Depends on

Sessions 54 (pooled processes) and 55 (call context).

## Governing decisions

F4. S1a.

## Scope

**In:**
- `src/backends/exiftool/process.cpp`: the POSIX `poll` loop and the Windows wait loop
  wake at most every 50 ms to check the token and deadline. On cancel, reuse the
  `command_timeout` path (kill, mark for lazy respawn) and return `cancelled` /
  `deadline_exceeded`.
- The pool (session 54) discards the killed instance and the lease returns a fresh slot.
- Tests (`tests/backend/test_child_process.cpp`, `test_exiftool_read.cpp`): a slow command
  (a fake ExifTool script that sleeps, following the existing child-process test pattern)
  is cancelled from another thread; the call returns within 500 ms; no zombie remains
  (POSIX `waitpid`); the next read on the same backend succeeds; a cancelled ExifTool write
  leaves the original byte-identical.

**Out:**
- Exiv2 mid-call interruption (not possible safely; documented in session 55).

## Documentation

- `docs/user/guide.md` "Cancellation and deadlines": ExifTool latency bound.

## Exit criteria

- Cancellation tests green on all three CI OSes and under TSan.
