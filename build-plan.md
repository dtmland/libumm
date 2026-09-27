# libumm multi-platform build and test plan

Status: **planning only**. This document defines how libumm will be built and tested on Linux, Windows, and macOS. Implementation (CMake, sources, workflows) belongs in a later session.

Related:

- Design plan: [concept.md](concept.md)
- Backend file-type coverage: [supported-types.md](supported-types.md)

## 1. Goal

libumm is currently design documentation only. Before (and while) the library is implemented, the project needs a clear build and CI plan so that:

- every change can be proven on **Linux, Windows, and macOS**
- a future first consumer can depend on libumm without inventing a second build system
- Exiv2 and ExifTool backends are exercised the same way on every platform
- app-only complexity (GUI toolkits, desktop packaging) is kept out of the library

## 2. Scope

### In scope

- CMake-based native library build
- Shared configure / build / test commands via CMake Presets
- Hosted CI matrix on Linux, Windows, and macOS
- Pinned acquisition of **Exiv2** and **ExifTool** for tests and backends
- Offline build-contract checks (pins and workflow wiring)
- Platform policy and testing-tier documentation (to be written with implementation)

### Out of scope for the first implementation increment

- Implementing the full metadata API (beyond minimal smoke symbols CI needs)
- GUI, Qt, or display-server testing
- Release packaging, code signing, notarization, or installers
- Package-manager publication (vcpkg, Conan, Homebrew, etc.)
- Changing any sister application repository; patterns may be adapted here, but this plan is for **libumm only**

## 3. Design constraints from concept.md

- Backends: **Exiv2** (native C++) and **ExifTool** (Perl script + modules).
- Phase 1 product surface: `read` / `write` / `capabilities` for stills (JPEG, TIFF, PNG, WebP, common RAW, XMP sidecars).
- Cross-backend verification (Exiv2 ↔ ExifTool round-trips) is a later milestone, so CI must be able to run **both** backends on every OS once code exists.
- libumm is a **library**, not an application: no GUI test tiers, no desktop deploy doctor.

## 4. Principles

1. **One build path.** Local developers and CI use the same CMake presets.
2. **Provision per OS; do not fork the build.** Platform jobs install toolchains and packages differently; they must not redefine configure/build/test commands.
3. **Pins are source of truth.** Backend versions and checksums live in repo files, not duplicated as magic strings only in workflow YAML.
4. **Fail closed on required backends.** In CI, missing Exiv2 or ExifTool fails configure or the job—never a quiet skip that stays green.
5. **CI runners are not the product matrix.** Document supported OS versions separately from GitHub-hosted image labels.
6. **Library-first CI.** No GUI toolkit install, no Xvfb GUI pass, no app archive verification in the first milestone.

## 5. Patterns to adopt (adapted for a library)

Reuse these proven multi-platform shapes; rename prefixes to `libumm` / `UMM_` as needed:

| Pattern | Purpose |
| --- | --- |
| CMake ≥ 3.24 and `CMakePresets.json` (Ninja, RelWithDebInfo) | Shared configure/build/test on all OSes |
| GitHub Actions matrix: `ubuntu-24.04`, `windows-2025`, `macos-15` | Pinned runners; avoid `*-latest` drift |
| `fail-fast: false` | One platform failure does not hide the others |
| PR concurrency with cancel-in-progress | Save runners |
| `permissions: contents: read` on CI | Least privilege |
| Shared pin loaders under `tools/build/` | Versions not re-stated only in YAML |
| Offline build-contract tests (Python unittest before compile) | Guard workflow and pin consistency |
| MSVC developer environment on Windows | Native C++ on `windows-2025` |
| Ninja via packages on Linux; setup action on Windows/macOS | Same generator everywhere |
| Checksum-pinned FetchContent for ExifTool | Reproducible Perl backend |
| Perl required wherever ExifTool runs | Explicit OS install notes |
| CTest with JUnit XML artifacts uploaded always | Cross-platform failure triage |
| Optional deliberately failing self-test (`workflow_dispatch`) | Prove CI reports failures on every OS |

## 6. Patterns not to adopt

- GUI toolkit install, module pins, or offscreen/display platform presets for UI
- Durable-store or server promotion dependencies unrelated to media metadata
- Extra Linux display server test jobs
- Release deploy trees, runtime library bundling for a shipped app, or archive “doctor” steps
- Assembler toolchains unless a future native dependency truly needs them
- Local container/sandbox harnesses mirroring CI — useful later, not required for first multi-platform CI

## 7. Provisional platform policy

Record formally in `docs/supported-platforms.md` when implementation starts. Working baseline:

| Item | Baseline |
| --- | --- |
| Language | C++20 |
| Build system | CMake ≥ 3.24, Ninja, CMake Presets |
| Linux | Ubuntu 22.04+ x86-64 (CI image: `ubuntu-24.04`) |
| Windows | Windows 10 22H2+ / Windows 11, x86-64 (CI image: `windows-2025`, MSVC) |
| macOS | macOS 14+, **arm64** (CI image: `macos-15`) |
| macOS Intel | Unsupported until a concrete consumer need exists |
| Linux arm64 | Buildable when dependencies allow; not a hosted CI target initially |
| Backends in CI | Exiv2 **and** ExifTool required on every matrix job |

Open decisions to confirm before coding:

- Exiv2: always build from pinned source vs allow system Exiv2 locally with pin-only enforcement in CI
- Default `BUILD_SHARED_LIBS` (static vs shared) for the first consumer
- Exact minimum compiler versions (MSVC, Apple Clang, GCC/Clang on Linux) beside runner images
- How downstream apps may bundle ExifTool/Perl (library CI only needs them as build/test tools at first)

## 8. Target repository layout

Planned paths for the first implementation increment (not created by this planning document alone):

```text
CMakeLists.txt
CMakePresets.json
cmake/
  LibummExiv2.cmake       # find or fetch pinned Exiv2
  LibummExifTool.cmake    # FetchContent + Perl, checksum-pinned
include/umm/              # public headers
src/                      # library implementation
tests/
  build/                  # offline pin/workflow contracts (Python)
  unit/                   # pure logic CTest
  backend/                # Exiv2 / ExifTool integration
  fixtures/               # small licensed media samples
tools/build/
  pins.sh                 # load backend version pins into the environment
  linux-packages.txt      # apt prerequisites (ninja, perl, Exiv2 deps, …)
.github/workflows/
  ci.yml                  # matrix build-and-test
docs/
  supported-platforms.md
  build-architecture.md
  testing.md
```

Public CMake package target naming (provisional): `umm::umm` (or `libumm::libumm`), with a `UMM_BUILD_TESTS` option defaulting on for developers and CI.

## 9. Dependency strategy

### 9.1 Exiv2

- Prefer a **pinned version** acquired the same way on all three OSes (FetchContent from source, or prebuilt artifacts with SHA-256), so CI does not depend on “whatever apt/brew shipped.”
- Linux may still install **build prerequisites** (zlib, expat, and similar) via `tools/build/linux-packages.txt`.
- Windows: build under the same Ninja + MSVC preset, or consume a pinned binary if source build time becomes painful—decision locked when implementing.
- macOS arm64: same pin; universal binaries are out of scope for v1.
- CMake: `UMM_REQUIRE_EXIV2=ON` in CI so a missing backend fails configure.

### 9.2 ExifTool

- Checksum-pinned source archive via FetchContent (or equivalent).
- `find_package(Perl REQUIRED)`; run the `exiftool` script with the discovered interpreter.
- CI installs Perl on every runner:
  - Linux: `perl` in `linux-packages.txt`
  - macOS: document system Perl or Homebrew if the image is insufficient
  - Windows: verify Perl on PATH; if missing, pin an install step and cover it in build contracts
- Tests receive compile definitions such as `UMM_TEST_EXIFTOOL_SCRIPT` and `UMM_TEST_EXIFTOOL_PERL`.

### 9.3 Pins and evidence

- Single source for versions/checksums (for example values in `cmake/Libumm*.cmake` and/or `tools/build/backends.env`), loaded by `tools/build/pins.sh` for workflow env and cache keys.
- At configure time write `build/default/backends-acquired.txt` (versions and paths) and print it in CI logs.

## 10. CMake presets (shared commands)

Minimum preset surface:

| Kind | Name | Role |
| --- | --- | --- |
| configure | `default` | Ninja, RelWithDebInfo, tests on |
| configure | `debug` | optional local Debug inherit |
| build | `default` / `debug` | match configure |
| test | `default` | CTest; `noTestsAction: error`; output on failure |

Developer and CI loop:

```text
cmake --preset default
cmake --build --preset default
ctest --preset default
```

CI configure flags (illustrative):

```text
cmake --preset default
  -DUMM_REQUIRE_EXIV2=ON
  -DUMM_REQUIRE_EXIFTOOL=ON
  -DUMM_BUILD_TESTS=ON
  -DUMM_ENABLE_FAILING_SELFTEST=<ON only from workflow_dispatch>
```

No offscreen GUI environment variables and no second display-server test preset in the first milestone.

## 11. CI workflow shape

File: `.github/workflows/ci.yml` (to be added later).

**Triggers:** `pull_request`, `push` to the default branch, `workflow_dispatch` (optional failing self-test input).

**Job:** single `build-and-test` matrix:

| name | runner |
| --- | --- |
| Linux | `ubuntu-24.04` |
| Windows | `windows-2025` |
| macOS | `macos-15` |

**Steps (logical order):**

1. Check out the repository.
2. Validate build contracts (`python -m unittest discover -s tests/build -v`).
3. Load shared pins into `GITHUB_ENV`.
4. Install platform dependencies (Linux apt from `linux-packages.txt`; Ninja on Windows/macOS; MSVC env on Windows; Perl as needed).
5. Cache backend downloads under `.cache/…`, keyed by runner + pin versions.
6. Configure with required-backend flags.
7. Report acquired backend versions from `backends-acquired.txt`.
8. Build with the default preset.
9. Run `ctest --preset default` with JUnit output under `build/default/test-results/`.
10. Upload test results and CTest `Testing/` directories with `if: always()`.

**Not in first CI milestone:** release workflow, multi-job GUI matrix, packaging verification.

## 12. Testing plan

### Tier A — always on CI (all three OS)

- **Build contracts:** pins load correctly; workflows reference pin loaders and package lists; malformed pins fail closed.
- **Unit tests:** semantic model, mapping tables, capability logic without media files.
- **Fixture integration:** read/write small checked-in samples through Exiv2 and ExifTool backends.
- **Capabilities alignment:** expectations stay consistent with [supported-types.md](supported-types.md) (later: machine-readable slice or generated asserts).
- **Failing self-test option:** one deliberately failing CTest gated by CMake option, enabled only from `workflow_dispatch`, proving red CI on each OS.

### Tier B — when cross-backend verification lands

- Write with one backend, read with the other, compare canonical fields.
- Larger fixture corpus via git-lfs or cached download with a checksum manifest.

### Tier C — manual / rare

- Vendor MakerNote oddities, OS-specific path encoding, very large files.
- Documented field notes; do not block everyday CI green.

### Fixture policy

- Tiny synthetic or clearly licensed samples only in-repo at first.
- No huge RAW trees in git; optional cache download keyed by manifest SHA when needed.
- Fixture directories passed through CMake so Windows paths do not break tests.

## 13. Documentation to add with implementation

| Document | Purpose |
| --- | --- |
| `docs/supported-platforms.md` | Product matrix vs CI runners; backend requirements; open decisions |
| `docs/build-architecture.md` | Shared presets vs per-OS provisioning; pin ownership |
| `docs/testing.md` | Tiers A/B/C; how to run presets; artifact locations |
| `README.md` | Clone → configure → build → test; link this plan and platform docs |
| `concept.md` | Short pointer section to build/CI docs without dumping YAML |

This file (`build-plan.md`) remains the planning source until those docs exist; implementation may fold or split content into `docs/` without changing the decisions here unless explicitly revised.

## 14. Implementation order (later sessions)

1. **Lock docs:** supported platforms, build architecture, testing (or keep this plan as the interim source of truth).
2. **Skeleton:** root `CMakeLists.txt`, presets, empty library target, `enable_testing`, trivial passing test + optional failing self-test.
3. **`tools/build`:** pins, `linux-packages.txt`, Python build-contract tests.
4. **CI matrix:** three-OS green build on the trivial test (proves toolchains).
5. **ExifTool module:** pin + Perl on all OS + smoke test (`exiftool -ver` or equivalent).
6. **Exiv2 module:** pin/link + smoke test (version or minimal API probe).
7. **Phase 1 fixture tests:** e.g. JPEG round-trip on all three platforms.
8. **Expand API and mapping engine** only under the same CI gate.

## 15. Success criteria

- A change that breaks Linux, Windows, or macOS cannot merge unnoticed; matrix runs all three with `fail-fast: false`.
- CI configure or tests fail if Exiv2 or ExifTool cannot be acquired when required.
- Developers use the same CMake presets as CI.
- Backend versions are pinned and visible in job logs via `backends-acquired.txt`.
- First CI milestone stays free of GUI toolkit and app-packaging complexity.

## 16. Relationship to product phases

| concept.md phase | Build/CI expectation |
| --- | --- |
| Phase 1 photo metadata | Tier A fixture tests on stills; both backends available |
| Phase 2 standards registry | Generated/registry unit tests in CI; no new OS matrix |
| Phase 3 video | Extend fixtures and capabilities; same three OS |
| Phase 4 sidecars | Sidecar path tests on all OS (path encoding matters on Windows) |
| Phase 5 GPS tracks | Additional unit/integration tests; no special GUI CI |
| Phase 6 verification | Tier B cross-backend corpus jobs; may add caches, not new runners at first |

## 17. Explicit non-goals of this document

- No CMake, workflow, or source files are required to exist yet for this plan to be accepted.
- No commitment to a specific Exiv2 minor version until the implementation session pins one with a checksum.
- No requirement that a sister application already consume libumm before multi-platform CI exists.
