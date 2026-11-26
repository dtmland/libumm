# Session 61 — `readMany`, ExifTool batching, benchmark

## Goal

Stream results for many paths with ExifTool batching, and show that selective reads are
measurably faster (U10 acceptance).

## Depends on

Sessions 54 (pool), 55 (cancellation), 60 (selective reads).

## Governing decisions

F8, F7.

## Scope

**In:**
- **Header first** (`umm.hpp`): `readMany(std::span<const std::filesystem::path>,
  ReadOptions, callback)`; callback on the calling thread, once per path, input order;
  per-file errors in the callback; the token is checked between files and inside batches.
- ExifTool adapter: batch up to N paths per `-execute`, split the JSON array by
  `SourceFile`, map per-file errors. Exiv2: loop.
- Sidecar pairing per path as in `read`.
- **Benchmark:** `tests/bench/read_bench.cpp`, built only with a new
  `UMM_BUILD_BENCHMARKS` option (default off; `std::chrono` only, no new dependency).
  Measures full vs selective `read` and `readMany` on the JPEG, HEIC (Tier B, when
  present), and MP4 fixtures per backend, and prints a table. Not run in CI by default.
- Tests: `readMany` results equal per-path `read`; order preserved; cancel mid-batch stops
  further callbacks and returns `cancelled`.

**Out:**
- Parallel fan-out inside `readMany` (callers use threads + the pool).

## Documentation

- `docs/user/guide.md` "Fast scans": `readMany`, benchmark results table.
- `docs/sysadmin/install.md`: `UMM_BUILD_BENCHMARKS`.

## Exit criteria

- Benchmark shows selective and batched reads faster on JPEG and MP4 with ExifTool; Exiv2
  numbers recorded honestly even if the gain is small (F7).
