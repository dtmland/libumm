# Session 63 — Read-time upcasts

## Goal

Fill canonical values from cast sources in memory on read, without writing the file. This
is the prerequisite for U3, U12, and U13, whose values must show up for search and display.

## Depends on

None (cast engine from session 47).

## Governing decisions

F9. C7, C9 (casts are opt-in), C12a.

## Scope

**In:**
- **Header first** (`umm.hpp`): `ReadOptions::upcast`
  (`std::optional<std::vector<std::string>>`), semantics mirroring `WriteOptions::downcast`;
  default `nullopt` applies no groups (OQ-F1: off by default).
- `src/read.cpp`: after reconcile, evaluate up casts for the requested groups (reuse
  `internal::evaluate_casts` / `apply_cast_candidates` from `src/cast.cpp`) and apply only
  `can_cast` rows in memory.
- Provenance: applied values record the cast group and source base key; `describe(id,
  path)` and `castCandidates()` show it.
- Interaction with `ReadOptions::properties` (session 61): only groups targeting listed ids
  run.
- Tests with the existing first-set groups (`recordingDevice`, `videoCreated`,
  `capturePosition`): values appear in memory, file bytes unchanged, a following
  `umm::write` of that `Metadata` writes the representations (documented behavior).

**Out:**
- New cast groups (sessions 64, 67–70).

## Documentation

- `docs/user/guide.md` "Casting": read-time upcasts.
- `docs/reconciliation-policy.md`: precedence of representation values over upcast values
  (a representation always wins; upcast fills only empty targets).
- `docs/developer/canonical-model.md`: read-time cast application.

## Exit criteria

- No file is modified by any read; existing cast tests unchanged and green.
