# Session 28 — Cross-backend verification suite

Stage 10 · Estimated 45–60 min

## Goal

The verification loop from concept.md §30: write with one backend, read with the other, compare
canonical results — run across the Tier A corpus (every format from Stages 4–7) and the Tier B
corpus, turning backend complementarity into an executable compatibility suite.

## Prerequisites

Session 27 merged.

## Deliverables

- Comparison harness (test-side, not public API): for each (fixture × writable property set ×
  direction), write via backend A, read via backend B (and the reverse), and compare canonical
  `Metadata` — values, families present, and provenance shape — with a normalization layer for
  documented, expected representation differences (the reconciliation policy's equivalence rules
  are the comparator; byte-equality is explicitly not the goal).
- Expected-divergence ledger: a machine-readable list (with human-readable notes) of known,
  accepted cross-backend differences (e.g. categories one backend cannot write per capability
  data; representation quirks) so the suite fails only on *unexpected* divergence, and every
  accepted divergence is documented rather than silently tolerated. Capability data drives the
  skip logic — a pair is only compared where both backends claim the needed access.
- Coverage: Tier A — JPEG, TIFF, PNG, WebP, DNG, XMP sidecar, MP4/MOV (video is one-directional:
  write ExifTool, read both where capable); Tier B — the session 27 samples, exercised in the
  opt-in job.
- Round-trip stability check: write A → read B → write B → read A converges (second pass produces
  no new differences) for the stable property set.
- CI: Tier A comparisons join the default matrix (they reuse committed fixtures and both pinned
  backends already required in CI, decision S1b); Tier B comparisons run in the session 27 job.

## Steps

1. Harness + comparator + divergence-ledger format.
2. Tier A matrix runs; populate the ledger with verified, explained entries only.
3. Tier B integration + stability check; push; three-OS green.

## Acceptance criteria

- Every capability-data claim that both backends can access a (type × category) is verified by an
  actual cross-backend write/read pair somewhere in the suite.
- An unexplained cross-backend divergence fails CI; every ledger entry carries a reason.
- Stage 10 exit: the concept.md §30 loop is a maintained test suite, and the project's
  stated later-stage scope (overview stage map, Stages 6–10) is complete.

## Cut line

The round-trip stability check and Tier B integration may defer to a patch session; the Tier A
harness and ledger may not.

## Out of scope

Performance benchmarking; fuzzing; corpus growth beyond the session 27 manifest.

## References

concept.md §13, §30; supported-types.md §6; analysis 2026-09-28 §4; decision S1b;
docs/reconciliation-policy.md (cross-backend identity section).
