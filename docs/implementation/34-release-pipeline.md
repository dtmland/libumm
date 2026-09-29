# Session 34 — Release pipeline

Stage 11 · Estimated 45–60 min

## Goal

A tag-triggered GitHub Actions release workflow producing compliant, consumable artifacts
(analysis 2026-09-29 §7, decision **P1**), plus the release checklist. Also closes the session 26
cut-line deferral (video write-back coverage in track correlation) as a pre-release verification
item.

## Prerequisites

Sessions 29–33 (install tree, version policy, notices, get-exiftool native scripts). Session 32's
shared mode is not part of the default artifacts.

## Deliverables

- `.github/workflows/release.yml`, triggered on version tags (and `workflow_dispatch` dry-run):
  - builds the static default configuration on all three OSes with both backends required and the
    full test suite green (reusing the CI presets — one build path, per principle 1 of
    build-plan.md);
  - assembles per-OS binary archives from the install prefix: headers, library, CMake package
    files, `LICENSE`, `NOTICE.md`, `THIRD-PARTY-NOTICES.md`, `licenses/`, `tools/get-exiftool/`
    (`install.sh`, `install.ps1`, plus pin data), and the abi-policy/README pointers;
  - attaches the corresponding-source archives (pinned exiv2/expat/zlib tarballs, fetched and
    checksum-verified from the session 31 manifest) and a `SHA256SUMS` file covering every asset;
  - creates a draft GitHub release with generated notes naming: libumm version, standards
    versions (from the registry), backend pins, and the P1 licensing statement (binary conveyed
    under GPL-3.0; source Apache-2.0).
- `docs/release-checklist.md`: owner license confirmation (S1d is still provisional — must be
  resolved at or before the first release), version bump procedure (session 30 policy), tag
  procedure, artifact verification steps, and post-release pin-audit note.
- Video write-back verification: extend the track-correlation tests so `matchTrack` +
  `umm::write` location on an MP4/MOV fixture is exercised (session 26 cut line), keeping the
  release gate honest about video.
- Workflow contract tests: release workflow references the presets and pin loaders, includes the
  three OSes, attaches corresponding source, and never uploads on test failure.

## Steps

1. Video write-back test closure (small, independent — do first so the release gate includes it).
2. Workflow: build/test/install/archive per OS; corresponding-source + checksums; draft release.
3. Checklist doc + workflow contract tests; validate via `workflow_dispatch` dry-run.

## Acceptance criteria

- A `workflow_dispatch` dry-run produces all artifacts with correct contents (spot-checked by the
  contract tests where offline-verifiable: archive manifest lists, checksum file coverage).
- Corresponding-source archives match the build pins byte-for-byte (SHA-256).
- The track-correlation video write-back test runs in the default matrix.
- No release can publish with a red test suite.

## Cut line

Release-notes generation polish and macOS/Windows shared-mode extras may defer; the three-OS
static artifacts, notices, corresponding source, and checksums may not.

## Out of scope

Package-manager publication (vcpkg/Conan/Homebrew — post-first-release decision); code signing
and notarization; the Exiv2-free "core" artifact (P1 option 3, unscheduled).

## References

analysis 2026-09-29 §4, §7 (P1); P9 native get-exiftool scripts; sessions 29–33;
docs/implementation/26 cut line; build-plan.md principles; decision S1d (owner confirmation).

## Implementation notes

- **Workflow:** `.github/workflows/release.yml` on `v*.*.*` tags and `workflow_dispatch` (dry-run: artifacts only). Matrix reuses CI presets (`cmake --preset default`, both backends required, static default, no `UMM_EXIV2_SHARED`). Tests run before `cmake --install` / packaging; asset uploads have no `if: always()`. A draft GitHub release is created only on tag pushes.
- **Packaging:** `tools/build/package_release.py` archives the install prefix plus `tools/get-exiftool/` (scripts + `backends.env`), `docs/abi-policy.md`, and `README.md`. `tools/build/fetch_corresponding_source.py` checksum-verifies the session 31 manifest (fail-closed). `tools/build/generate_release_notes.py` names the library version, registry standards, backend pins, and the P1 GPL-3.0 / Apache-2.0 statement.
- **Video write-back:** `tests/backend/test_track_match.cpp` exercises `matchTrack` + `umm::write` GPS on MP4 and MOV (session 26 cut line).
- **Checklist:** `docs/release-checklist.md` (S1d confirmation, version bump, tag, artifact verification, pin audit).
