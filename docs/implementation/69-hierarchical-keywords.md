# Session 69 — Hierarchical keywords and legacy write-sync policy

## Goal

Read Lightroom `lr:hierarchicalSubject` into keywords (H6) and document what canonical
writes do to legacy copies (U12, part 3).

## Depends on

Sessions 67, 68. Small; can share a sitting with session 70.

## Governing decisions

F12. H6 (leaf term), H4 (union).

## Scope

**In:**
- `registry/casts/hierarchical-keywords.json`: `lr:hierarchicalSubject` → `keywords`, leaf
  term after the last `|`, then union with existing keywords. Up only.
- Fixtures: JPEG XMP with hierarchical subjects, including a Lightroom "export containing
  keywords" layout (parents also flat) so the union is checked.
- **Write-sync policy (F12):** a test proves a canonical keyword write leaves
  `lr:hierarchicalSubject`, `XP*`, PNG text, and QuickTime user data unchanged.

**Out:**
- Writing hierarchical keywords.

## Documentation

- `docs/reconciliation-policy.md`: legacy carriers are not updated or removed by canonical
  writes; disagreement is visible through casts.
- `docs/user/guide.md`: importing existing libraries (all U12 groups together).

## Exit criteria

- All five U12 groups listed in `castCandidates()` and covered by fixtures.
