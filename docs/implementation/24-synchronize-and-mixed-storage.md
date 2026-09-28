# Session 24 — synchronize() and mixed storage completion

Stage 8 · Estimated 45–60 min

## Goal

Complete the embedded-vs-sidecar policy engine: `synchronize()` writes a resolved state back to
both carriers, `StorageDecision::Method::mixed` becomes real, and the `SidecarRequired` semantics
deferred from session 14 land.

## Prerequisites

Session 23 merged.

## Deliverables

- `synchronize(path, options) -> Result<SyncReport>`: read + reconcile (+ merged resolutions from
  session 23 where supplied), then write the canonical state to embedded and sidecar carriers per
  policy so both agree; report enumerates every representation updated per carrier. Built from
  the existing read pipeline, write-sync, and atomic-write pieces — both file mutations go through
  `mutate_file_atomically`, and the failure mode when the second write fails is documented and
  tested (first-succeeded/second-failed leaves a consistent, reported state — no torn multi-file
  guarantee is promised).
- `Method::mixed` write decisions: policy/capability combinations that write embedded + sidecar
  in one `umm::write` call (the "mixed sync" deferred in session 14's doc), driven by capability
  data via the session 16 engine.
- `SidecarRequired` completed: writes fail if the sidecar cannot be produced; reads flag a
  missing-required-sidecar condition per the policy doc.
- Direction control: sync options choose embedded→sidecar, sidecar→embedded, or
  reconciled-state→both (default), matching concept.md §12's policy vocabulary.
- Tests: paired fixtures brought into and out of agreement each direction; conflict → merge →
  synchronize end-to-end; byte-identical media when only the sidecar needed updating; policy
  decision-table extended for `mixed` and `SidecarRequired` across JPEG/TIFF/DNG/read-only-RAW
  capability rows.

## Steps

1. Header-first (`SyncReport`, options, `SidecarRequired` semantics documented).
2. `synchronize` + mixed write decisions.
3. End-to-end tests + policy tables; policy doc updates; push; three-OS green.

## Acceptance criteria

- `media.jpg` + `media.xmp` in conflict can be detected, merged, and synchronized to agreement
  with provenance intact, via public API only.
- Stage 8 exit: concept.md §28's `detectConflict()` / `merge()` / `synchronize()` triple exists,
  and no session 14 deferral remains open.

## Cut line

Direction control may ship with the reconciled-state→both default only (other directions
following in a patch session); `SidecarRequired` completion may not be cut.

## Out of scope

Multi-asset/batch synchronization; conflict resolution UIs; non-XMP sidecar formats.

## References

concept.md §12, §28; session 14 (deferred mixed sync); docs/reconciliation-policy.md sidecar
sections; analysis 2026-09-28 §4.
