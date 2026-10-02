# Release checklist

Use this before tagging a libumm release. The tag-triggered workflow is
`.github/workflows/release.yml` (session 34, decision **P1**).

## 1. Owner license confirmation (S1d)

Decision **S1d** chose Apache-2.0 for libumm's own source **provisionally**.
That choice must be confirmed (or replaced) by the owner **at or before the
first release**. Do not publish a non-draft GitHub release until this is
resolved in `LICENSE`, `NOTICE.md`, and this paragraph.

Binary artifacts that include the Exiv2 backend are conveyed under **GPL-3.0**
regardless of that confirmation; libumm's source remains Apache-2.0 only if
S1d stands. See
[docs/analysis/2026-09-29-stage-10-review-release-and-licensing.md](analysis/2026-09-29-stage-10-review-release-and-licensing.md)
decision **P1**.

## 2. Version bump (session 30 / `docs/abi-policy.md`)

Keep these identical; contract tests fail if they drift:

1. `UMM_VERSION_MAJOR` / `MINOR` / `PATCH` in `include/umm/version.hpp`
   (normative).
2. `project(libumm VERSION x.y.z)` in `CMakeLists.txt`.
3. Runtime `umm::version()` (reads the header macros).

Pre-1.0, a source-API break bumps **minor**. Patch is fixes only. Library
semver does not encode IPTC/ExifTool/Exiv2 versions (`Registry::standards()`).
`SOVERSION` follows `docs/abi-policy.md`.

## 3. Tag procedure

1. Complete the bump on `main` with green CI (`.github/workflows/ci.yml`).
2. Confirm S1d and that `THIRD-PARTY-NOTICES.md` / `tools/build/corresponding-source.json`
   still match `tools/build/backends.env`.
3. Tag `vX.Y.Z` matching `UMM_VERSION_*` exactly (example: version `0.1.0` →
   `v0.1.0`). The release workflow fails if they disagree.
4. Push the tag **without** creating a GitHub Release in the UI first. A UI
   release binds the tag and GitHub only shows auto-generated source archives
   until artifacts are uploaded. The workflow now uploads onto an existing tag
   release when one is already there; if none exists it opens a **draft** with
   the three-OS binaries, corresponding source, and `SHA256SUMS`. Do not use
   `*-latest` runners or a shared-Exiv2 (`UMM_EXIV2_SHARED`) artifact; default
   static three-OS builds only.

A `workflow_dispatch` run is a **dry-run**: it builds artifacts and
`SHA256SUMS` but does **not** create a GitHub release.

## 4. Artifact verification

The draft release (or the dry-run `release-dist` artifact) must contain:

- Per-OS binary archives `libumm-<version>-<os>.tar.gz` for `ubuntu-24.04`,
  `windows-2025`, and `macos-15`. Each archive includes headers, the library,
  CMake package files, `LICENSE`, `NOTICE.md`, `THIRD-PARTY-NOTICES.md`,
  `licenses/`, `tools/get-exiftool/` (`install.sh`, `install.ps1`,
  `backends.env`), `docs/abi-policy.md`, and `README.md`.
- Corresponding-source tarballs for Exiv2, Expat, and zlib (pinned URLs and
  SHA-256 from `tools/build/corresponding-source.json`).
- `libumm-<version>-src.tar.gz` (Apache-2.0 source).
- `SHA256SUMS` covering every other asset.

Spot-check:

```sh
sha256sum -c SHA256SUMS
tar -tzf libumm-*-ubuntu-24.04.tar.gz | grep tools/get-exiftool/install.sh
```

Corresponding-source SHA-256 values must match `backends.env` /
`cmake/LibummExiv2.cmake` byte-for-byte. The workflow never uploads release
archives if tests fail (`needs` on the green matrix; no `if: always()` on
asset uploads).

`test_track_match` in the default matrix covers `matchTrack` + `umm::write`
location write-back on JPEG, MP4, and MOV (session 26 cut line, closed here).

## 5. Publish the draft

1. Read the generated notes: libumm version, standards versions, backend pins,
   and the P1 GPL-3.0 / Apache-2.0 statement. Confirm the three-OS binary
   archives, corresponding source, `libumm-*-src.tar.gz`, and `SHA256SUMS` are
   attached as release assets (not only GitHub's auto-generated Source code
   zip/tarball).
2. Confirm S1d is settled.
3. If the workflow created a draft, mark the GitHub release as published. If
   artifacts were uploaded onto an already-published tag release, verify the
   assets on that release instead.

## 6. Post-release pin audit

Within a day of publishing, re-hash the attached corresponding-source archives
against `tools/build/corresponding-source.json`. If a pin is bumped later,
regenerate that manifest with `tools/build/generate_corresponding_source.py`
**before** the next tag; do not edit the JSON by hand.

Package-manager publication (vcpkg/Conan/Homebrew), code signing, and an
Exiv2-free "core" artifact are out of scope (P1 option 3, unscheduled).
