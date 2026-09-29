# Copilot instructions for libumm

## General

- Do not make claims without actually reading file contents - do not only look at file names and sizes, and do not speculate.
- Before deciding that a new dependency is needed that is not already in the project, please perform due diligence to confirm that any existing deps or tools cannot satisfy the need and document the justification.
- Prefer the smallest, most surgical change that addresses the task. Do not refactor unrelated code or broaden scope without a clear reason.
- Follow the repo's explicit design records and implementation plan rather than inventing a new path.

## Repository conventions

- Decision records live under `docs/analysis/` and are the authoritative source for design decisions (for example, decision IDs like `S1a`, `M4a`).
- The initial implementation plan is complete; its consolidated record is `docs/developer/implementation-history.md`. New work follows the decision records, not an informal path.
- The public API shape in `include/umm/` is normative for design-draft headers. Update the relevant header(s) before implementation when the task changes the public contract.
- libumm does not define new metadata standards; it integrates established standards and implementations. Prefer compatibility with IPTC, XMP, EXIF, and media-container metadata conventions over inventing one-off representations.
- Keep metadata semantics, reconciliation policy, and backend behavior aligned with the project decisions in `docs/analysis/` and `docs/reconciliation-policy.md`.
- Docs are organized by audience: `docs/user/`, `docs/sysadmin/`, `docs/developer/`; `docs/README.md` is the index.

## Implementation workflow

- Before changing behavior, read the exact files that define it; do not rely solely on names, grep hits, or assumptions from adjacent code.
- Before starting implementation work, check `docs/developer/implementation-history.md` for the standing constraints and what already exists, and the dated records in `docs/analysis/` for the governing decisions.
- When a change affects the public API, semantics, or backend behavior, update the relevant design/implementation documents and the corresponding header(s) in `include/umm/` as needed.
- Keep patches focused and testable. Prefer incremental, reviewable changes over broad rewrites.
- Validate the affected behavior using the smallest relevant existing tests or checks, and avoid adding new test tooling unless it is clearly necessary.

## Crash-resistant progress

- For implementation tasks, push a checkpoint with `report_progress` after each coherent unit of work and whenever substantial changes have remained unpushed for 20 minutes.
- A local commit is not a durable checkpoint in the cloud-agent environment. Push the checkpoint branch even when the work is incomplete and identify unfinished or unvalidated work in the progress checklist.
- Run `git diff --check` before a checkpoint and exclude temporary or generated files from the patch.
- Do not create a pull request until implementation and validation are complete and the work is ready for final review. Until then, keep work on the feature branch as checkpoint commits only.

## Expected output quality

- Prefer clear, concrete changes with direct ties to the task and the repo's design docs.
- If a task is ambiguous, resolve the ambiguity by checking the project decisions and the relevant implementation plan before proceeding.
- Preserve existing conventions and naming patterns in the codebase rather than introducing new ones unless the task explicitly requires it.
