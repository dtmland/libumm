# Session 70 — Durable write option

## Goal

Make `umm::write` durable on return when the caller asks for it (U11).

## Depends on

None. Small; can share a sitting with session 57 or 71.

## Governing decisions

F14. M3 (temp file + atomic rename / `ReplaceFileW`).

## Scope

**In:**
- **Header first** (`umm.hpp`): `WriteOptions::durable`, `SyncOptions::durable` (default
  `false`), with the per-platform guarantee in the comment.
- `src/core/atomic_write.cpp`: POSIX `fsync` of the temp file (macOS `F_FULLFSYNC`),
  rename, `fsync` of the parent directory. Windows `FlushFileBuffers` on the temp file, then
  `ReplaceFileW` and a flush of the destination handle, or `MoveFileExW` with
  `MOVEFILE_WRITE_THROUGH` on the fallback path.
- New `AtomicWriteFault` points for each flush step; `tests/unit/test_atomic_write.cpp`
  covers failure at each (original unchanged before rename; error reported after).
- Sidecar writes and mixed writes honor the flag.

**Out:**
- Power-loss testing (not feasible in CI; the guarantee is the documented syscall
  sequence).

## Documentation

- `docs/user/guide.md`: "Durable writes" per platform.
- `docs/developer/release-notes.md`: new options member.

## Exit criteria

- Fault-injection tests green on all three CI OSes; default write path unchanged.
