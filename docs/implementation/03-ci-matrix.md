# Session 03 — CI matrix

Stage 0 · Estimated 30–45 min

## Goal

Three-OS GitHub Actions matrix building the skeleton and running contract + unit tests; proves
toolchains before any backend or metadata code exists.

## Prerequisites

Sessions 01–02 merged.

## Deliverables

- `.github/workflows/ci.yml`:
  - Triggers: `pull_request`, `push` to default branch, `workflow_dispatch` with boolean input
    `failing_selftest` (default false).
  - `permissions: contents: read`; concurrency group per ref with cancel-in-progress on PRs.
  - Single job `build-and-test`, `fail-fast: false`, matrix:
    Linux `ubuntu-24.04`, Windows `windows-2025`, macOS `macos-15` (pinned labels, never
    `*-latest`).
  - Steps (order per build-plan §11): checkout → `python3 -m unittest discover -s tests/build -v`
    → `tools/build/pins.sh` into `GITHUB_ENV` → per-OS deps (Linux: apt from
    `linux-packages.txt`; Windows: Ninja setup action + MSVC dev-env action + **pinned Strawberry
    Perl step (inert until session 04, installed now so the contract is stable)**; macOS: Ninja) →
    cache `.cache/` keyed on runner OS + pin values → configure `default` preset, passing
    `-DUMM_ENABLE_FAILING_SELFTEST=ON` only when the dispatch input is true → build preset →
    `ctest --preset default` with JUnit XML into `build/default/test-results/` → upload
    test-results + `Testing/` with `if: always()`.
- Extend `tests/build/test_workflow.py`: workflow YAML parses (stdlib-only parser or a minimal
  line-based check — do **not** add PyYAML); references `pins.sh` and `linux-packages.txt`;
  runner labels are the pinned trio; `fail-fast: false` present; permissions block present.

## Steps

1. Write ci.yml per above.
2. Add `test_workflow.py`; run the contract suite locally.
3. Push branch; watch all three jobs go green.
4. `workflow_dispatch` with `failing_selftest: true`; confirm all three jobs go **red**; re-run
   normal dispatch green.

## Acceptance criteria

- All three matrix jobs green on PR.
- Failing self-test dispatch turns all three red (proves failure reporting per OS).
- Contract tests fail if a workflow reference to pins/packages is removed.

## Cut line

The failing-self-test dispatch verification can move to a follow-up run comment, not the tests.

## Out of scope

Backend acquisition steps beyond the inert Perl install; release workflows; artifacts beyond test
results.

## References

build-plan.md §5, §11, §14 step 4; analysis decision M4b.
