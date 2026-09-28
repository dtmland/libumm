# Session 16 — Format dispatch generalization and hardening

Stage 6 · Estimated 45–60 min

## Goal

Make the write path capability-driven instead of JPEG-gated, and land the small hardening items
from the Stage 5 review (analysis decisions **R1**, **R3**, **R8**) so sessions 17–19 add formats
with data, fixtures, and tests only — no dispatch surgery.

## Prerequisites

Sessions 01–15 merged (Stage 5 complete);
[2026-09-28 review analysis](../analysis/2026-09-28-stage-5-review-and-later-stage-plan.md)
accepted.

## Deliverables

- `evaluateStorage()` (`src/core/sidecar.cpp`) no longer special-cases JPEG. The decision is
  derived from `registry/capabilities/` data for the sniffed type: per-backend category access,
  `preferred_backend`, and `sidecar_recommended` produce the `StorageDecision` (method, formats,
  backend). Unsupported combinations return `unsupported_capability` with the reporting backend —
  not `unsupported_type` on a hardcoded list. Behavior for JPEG and XMP sidecars is unchanged
  (regression-tested against the existing policy decision-table tests).
- `umm::write` sidecar/embedded branch consults the decision's formats rather than assuming
  sidecar = XMP-only structure beyond what the decision states (`src/write.cpp`); no functional
  change for Phase 1 types.
- Canonical property-ID constants deduplicated into one internal header (e.g.
  `src/core/property_ids.hpp`) replacing the triplicated definitions in `src/core/reconcile.cpp`,
  `src/core/write_sync.cpp`, and `src/metadata.cpp` (R3).
- `const Backend* BackendManager::get(std::string_view) const` overload (R8.1).
- `make_temp_path()` collision check (skip names that already exist) and a surfaced warning path
  when temp cleanup fails after a fault (R8.2) in `src/core/atomic_write.cpp`.
- Header comment refresh: `include/umm/umm.hpp` (and any other header with JPEG-specific
  commentary) describes capability-driven behavior; headers first, per M7 discipline.
- Tests: existing `test_storage_policy`, `test_write`, `test_sidecar`, `test_atomic_write` all
  green; new unit cases for the capability-driven decision (a type whose data says
  embedded-capable vs one that is sidecar-only vs one with no write capability) using capability
  records that already exist in the generated tables.

## Steps

1. Update headers (`umm.hpp` comments; `backend.hpp` for the const overload).
2. Extract property-ID header; mechanical replacement in the three consumers; build green.
3. Rewrite `evaluateStorage()` against capability data; keep the JPEG/XMP decision table output
   identical; extend unit tests.
4. Atomic-write hardening; full test suite; push; three-OS green.

## Acceptance criteria

- No file-type string comparison remains in the storage-decision path; decisions trace to
  capability data.
- JPEG + XMP sidecar behavior is byte-for-byte unchanged (existing tests unmodified and green).
- One definition of each canonical property ID in `src/`.

## Cut line

Atomic-write hardening (R8.2) may defer to a follow-up commit inside Stage 6; the
`evaluateStorage()` generalization and property-ID dedup may not.

## Out of scope

New formats, fixtures, or capability data entries (sessions 17–19); reconcile-engine
table-driving (R3 decision: revisit at Stage 7).

## References

Analysis 2026-09-28 findings R1, R3, R8; concept.md §12, §14; registry/capabilities/schema.md.
