# Session 54 — Backend instance pool

## Goal

Let reads run in parallel instead of one at a time (U1, request option 1): the manager
leases backend instances from a bounded pool, with one ExifTool `-stay_open` process per
pooled instance.

## Depends on

Session 53 (the parallel-read test and TSan job guard the change).

## Governing decisions

F1 (step 2), F2. S1a, S1c.

## Scope

**In:**
- **Header first** (`backend.hpp`): `ExifToolConfig::max_processes` (default 1); document
  the pool, lease lifetime, lazy spawn, and shutdown. Exiv2 pool size: measured in this
  session; either follows hardware concurrency or a separate setting (record the choice
  in the header).
- Replace the per-id mutex from session 52 with a lease (RAII handle) over a pool of
  instances per backend id. Instances are created lazily up to the bound; callers wait
  when the pool is exhausted.
- **Shutdown and reconfigure:** manager destruction sends `-stay_open False` to every idle
  child and waits with `command_timeout`; busy children are reaped when their lease ends.
  `configureExifTool` drains idle children (F2).
- `command_timeout` applies per process, unchanged.
- Tests: pool bound respected (process count never exceeds `max_processes`), children
  reaped at shutdown (POSIX: no zombie; Windows: handle closed), reconfigure under load.
  The session 53 parallel suite runs with `max_processes = 4`.

**Out:**
- Cancellation (sessions 55–56).
- Cross-file batching (session 62).

## Documentation

- `docs/user/guide.md` "Threads": pool sizing guidance.
- `docs/developer/release-notes.md`: new `ExifToolConfig` member (layout change).

## Exit criteria

- Parallel suite and TSan job green with `max_processes` 1 and 4.
- A timing check in the parallel test shows wall time drops with 4 processes on the
  ExifTool backend (reported, not asserted on CI timing).
