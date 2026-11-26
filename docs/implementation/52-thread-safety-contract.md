# Session 52 — Thread-safety contract and serialized entry points

## Goal

Make every public entry point safe to call from multiple threads (U1, request option 2) and
document the rule for each one, so callers can drop their own serialization.

## Depends on

None.

## Governing decisions

[Feature request 1 record](../analysis/2026-11-26-feature-request-1-decisions.md) F1
(step 1), F2. Standing constraints M1 (no exceptions), S1a (ExifTool `-stay_open`).

## Scope

**In:**
- **Header first:** a "Thread safety" note on every public function in `include/umm/`
  (`umm.hpp`, `capabilities.hpp`, `describe.hpp`, `track.hpp`, `registry.hpp`,
  `version.hpp`, `metadata.hpp`, `value.hpp`). Replace the "callers must not share"
  paragraph in `backend.hpp` with the manager-owned rule. Backend-free functions are
  listed as always concurrent (record §1 U1 row; `cast` and `describe(id, path)` are *not*
  backend-free).
- **Audit** process-wide mutable state: `BackendManager` (`src/backends/backend_manager.cpp`),
  Exiv2 XMP toolkit initialization and namespace registration (`src/backends/exiv2/`,
  `src/exiv2_shim.cpp`), registry singletons, any function-local statics in `src/core/`.
  Record findings in this doc's "Findings" section when the session runs.
- **Serialize** backend use: a per-backend-id mutex owned by `BackendManager`, taken for
  the duration of each `readBase` / `writeBase` / `typeCapabilities` call. Exiv2 one-time
  initialization under `std::call_once`.
- **`configureExifTool` (F2):** guarded by the manager; applies to calls that start after
  it returns; documented in `backend.hpp`.
- A small unit test that calls `configureExifTool` while reads run on another thread
  (no crash, later reads use the new config).

**Out:**
- Parallel-read equivalence suite and TSan job (session 53).
- Instance pooling (session 54).

## Documentation

- `docs/user/guide.md`: new "Threads" section (contract table, `configureExifTool` rule).
- `docs/developer/release-notes.md`: thread-safety contract entry.

## Exit criteria

- Every public entry point states its thread-safety rule in its header.
- Existing tests pass on all three CI OSes; the new configure-while-reading test passes.
