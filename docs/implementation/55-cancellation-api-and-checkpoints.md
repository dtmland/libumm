# Session 55 — Cancellation API and core checkpoints

## Goal

Add per-call cancellation and deadlines to reads, writes, synchronize, and cast (U9), with
checks at every stage boundary in core.

## Depends on

Session 52.

## Governing decisions

F3. M1, M3 (atomic temp-plus-rename).

## Scope

**In:**
- **Header first:** `include/umm/cancel.hpp` (`CancelToken`: shared state, `cancel()`,
  `cancelled()`, default-constructed token never cancels). Add `cancel` and `deadline` to
  `ReadOptions`, `WriteOptions`, `SyncOptions`, `CastOptions` (`umm.hpp`). Append
  `ErrorCode::cancelled` and `ErrorCode::deadline_exceeded` (`result.hpp`).
- Checkpoints (F3) in `src/read.cpp`, `src/write.cpp`, `src/sync.cpp`, `src/cast.cpp`, and
  `src/core/atomic_write.cpp` (immediately before the rename). After the rename the call
  returns success.
- An internal "call context" passed down to backends (token + deadline), so session 56 can
  use it in the ExifTool adapter without another API change.
- Tests (`tests/unit/test_atomic_write.cpp`, `tests/backend/test_write.cpp`): cancel at each
  checkpoint via the existing fault-injection hook; original file byte-identical; temp file
  removed; deadline in the past fails fast with `deadline_exceeded`.

**Out:**
- Interrupting a running ExifTool command (session 56). Exiv2 calls are not interrupted.

## Documentation

- `docs/user/guide.md`: "Cancellation and deadlines" (where checks happen, write commit
  point, mixed/sync partial state).
- `docs/developer/release-notes.md`: new options members and error codes.

## Exit criteria

- Cancelled writes leave the original byte-identical at every pre-rename checkpoint.
- All existing tests pass; the new error codes have `to_string` coverage if one exists.
