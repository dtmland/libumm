# Session 36 — Media domain context on `Metadata`

## Goal

Give `Metadata` an explicit media domain (photo / video / unknown) so cross-media
setters can pick the domain-correct property ID, without changing any Phase 1 behavior.

## Depends on

Nothing new; builds on `umm::capabilities()` sniffing (session 15) and the read-side
domain selection already documented in `docs/reconciliation-policy.md`
("Domain selection": sniffed `MP4`/`MOV` select the `iptc.video.*` domain).

## Scope

**In:**
- `enum class MediaDomain { photo, video, unknown };` in `include/umm/metadata.hpp`
  (header first — headers are normative).
- `Metadata::mediaDomain()` / `Metadata::setMediaDomain(MediaDomain)`. Default is
  `unknown`, which preserves exact Phase 1 semantics (accessors resolve to
  `iptc.photo.*`, as today).
- `umm::read()` sets the domain from the sniffed file type via the same rule the
  reconcile engine already applies — do **not** invent a second detection path. Factor
  the existing sniffed-type → domain decision into one shared internal helper used by
  both reconcile and `read`.
- Copy/assignment keep the domain; `remove()`/`set()` by full ID are unaffected.

**Out:**
- No accessor changes yet (sessions 39–41).
- No audio domain enum value (added only when an audio registry exists).

## Design decisions

- **Getters never need the domain** (they probe both domain IDs; a reconciled `Metadata`
  populates only one domain). The enum exists for **setters** and for user introspection.
- `unknown` → photo on set: chosen over an error because it is exactly Phase 1 behavior
  for default-constructed `Metadata`; documented in the header comment.
- Adding a private member changes `Metadata` layout; acceptable under
  `docs/abi-policy.md` (pre-1.0 minor releases may break ABI) — record it in the next
  release notes.

## Work items

1. Update `include/umm/metadata.hpp` (enum, two methods, doc comments).
2. Implement in `src/metadata.cpp`.
3. Add the shared domain-selection helper (internal header under `src/core/`) and use it
   from `src/read.cpp` and `src/core/reconcile.cpp` (pure refactor of the existing rule).
4. Unit tests in `tests/unit/test_metadata.cpp`: default `unknown`; set/get; preserved by
   copy; `umm::read()` on a JPEG fixture yields `photo`, on an MP4 fixture yields `video`
   (backend-gated test alongside the existing video read tests).

## Exit criteria

- All existing tests pass unchanged (no behavior change for Phase 1 code paths).
- New unit tests pass on all three CI OSes.
- `git diff` shows no reconcile behavior change — only the shared helper refactor.
