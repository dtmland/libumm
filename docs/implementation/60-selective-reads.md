# Session 60 — Selective reads

## Goal

Read only the properties a scan needs, skipping work for the rest (U10, first half).

## Depends on

Session 52 (the read path is touched; keep the thread-safety notes accurate).

## Governing decisions

F7. C13 (dump views).

## Scope

**In:**
- **Header first** (`umm.hpp`): `ReadOptions::properties`
  (`std::optional<std::vector<std::string>>`), with the rules from F7 (unknown ids fail,
  dumps empty, sidecar still per `merge_sidecar`).
- `src/read.cpp` / `src/core/reconcile.cpp`: reconcile only the listed ids.
- Backend hint: an internal list of base keys derived from the registry representations of
  the listed ids (generated tables only; no hand-written keys in `src/`). The ExifTool
  adapter passes them as tag arguments; Exiv2 ignores the hint.
- Tests: for every Tier A fixture and a fixed id set (`dateCreated`, location,
  rating via their cross-media ids), selective results equal the same ids from a full
  read, on both backends.

**Out:**
- Multi-file reads and the benchmark (session 61).
- Technical pseudo-ids (`technical.*`, session 65 adds them).

## Documentation

- `docs/user/guide.md`: "Fast scans" (selective reads).

## Exit criteria

- Equality test green on both backends and all three CI OSes.
