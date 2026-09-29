# Plan review and decisions — 2026-09-27

Status: **accepted**. This is a historical analysis document. It records a full review of the
libumm planning documents ([concept.md](../../concept.md), [build-plan.md](../../build-plan.md),
[supported-types.md](../../supported-types.md)), the problems found, and the **decision taken for
each finding**. Later documents (including the [implementation plan series](../implementation/00-overview.md))
are written against these decisions. If a decision here is revised, revise it here first, then
propagate.

Decision notation:

- **Decision: YES / NO** — binary accept/reject of the recommendation.
- **Decision: choice N of [A | B | C]** — multiple options were viable; the chosen one is named,
  with the reason.

---

## 1. Overall verdict

The plan is fundamentally sound. The core insight — separating **semantics (IPTC / Video Metadata
Hub)**, **representation (XMP / EXIF / container)**, and **implementation (Exiv2 / ExifTool)** — is
the right architecture, and "adopt standards, don't invent them" is the correct strategy.
`supported-types.md` correctly demonstrates why capability discovery must be per-backend, per-type,
per-metadata-category. The build plan is disciplined and library-scoped.

**Decision: do we step back and solve the problem a different way? — NO.** Alternatives were
evaluated (see §4) and none justifies abandoning this plan.

---

## 2. Strong problems and decisions

### Finding S1 — ExifTool is not architecturally symmetric with Exiv2

The concept diagrams show Exiv2 and ExifTool as interchangeable boxes under a "Backend Manager".
They are not: Exiv2 is an in-process C++ library; ExifTool is a Perl program. They differ in
process model, performance profile, deployment burden, and licensing.

**Decisions:**

- **S1a — ExifTool backend is an out-of-process adapter using `-stay_open` batch mode with JSON
  output (`-j -G -struct`). Decision: YES.** Per-call process spawning would dominate performance,
  especially on Windows. The adapter contract must specify process lifecycle, error mapping,
  timeout/kill behavior, and character-encoding handling.
- **S1b — Backends are optional runtime plugins. Decision: YES.** libumm core + the Exiv2 backend
  must be fully usable with the ExifTool backend absent at runtime. CI still requires **both**
  backends on every matrix job.
- **S1c — Consumer acquisition of ExifTool/Perl.
  Decision: choice 2 of [1. libumm bundles ExifTool | 2. libumm locates an installed
  ExifTool/Perl with explicit configuration and a discovery API | 3. leave undefined].**
  Bundling raises GPL/Artistic distribution questions and platform packaging burden; leaving it
  undefined pushes the problem onto every consumer. libumm defines a documented discovery order
  (explicit path → environment variable → PATH) and reports absence through `capabilities()` /
  backend availability rather than failing at load.
  Later: **P3** adds a checksum-pinned end-user helper; **P9** specifies that helper as
  system-native POSIX `sh` + PowerShell scripts (not Python). S1c itself is unchanged.
- **S1d — Project licensing.
  Decision: choice 2 of [1. MIT | 2. Apache-2.0 | 3. LGPL-2.1+ | 4. GPL-3.0].**
  Apache-2.0 is chosen (provisionally, pending owner confirmation before the first code release)
  for its patent grant and broad consumer compatibility. It is compatible with linking Exiv2
  (GPL-2.0+ — note: this makes the *combined distributed work* effectively GPL-governed when the
  Exiv2 backend is statically distributed; document this in a NOTICE/licensing doc) and with
  invoking ExifTool **out-of-process only** (no bundling of ExifTool in libumm releases).

### Finding S2 — Phase ordering: standards registry after Phase 1 guarantees rework

concept.md ordered Phase 1 (photo read/write with IPTC Core + Extension + XMP + EXIF) before
Phase 2 (registry generated from the IPTC machine-readable Technical Reference). Phase 1 would
hand-code hundreds of property definitions the registry then regenerates, and the public API shape
depends on the registry design.

**Decision: YES — swap the phases.** The registry importer and generated property model come
first; read/write is then wired through the registry on a deliberately tiny format set. This also
matches concept.md §34's own stated first milestone ("Import IPTC's existing vocabulary and expose
it as a typed API").

### Finding S3 — Phase 1 format scope is too wide

JPEG + TIFF + PNG + WebP + "common RAW" + sidecars immediately drags in the hardest cross-backend
divergences (PNG has no EXIF in Exiv2; WebP has no IPTC in Exiv2; RAW is a swamp).

**Decision: choice 2 of [1. keep original scope | 2. JPEG + XMP sidecar only | 3. JPEG + TIFF].**
Phase 1 = **JPEG + XMP sidecar only**. That pair already exercises EXIF / IPTC-IIM / XMP
reconciliation, both backends, and embedded-vs-sidecar policy. TIFF, PNG, WebP, and RAW become
fast-follow increments after the round-trip is proven.

### Finding S4 — Reconciliation has no phase and no decision procedure

You cannot correctly read even a single JPEG without reconciliation (EXIF `DateTimeOriginal` vs
IPTC `DateCreated` vs XMP `photoshop:DateCreated`). Also: the **Metadata Working Group is defunct**
and its guidance is frozen (~2010); ExifTool's MWG module is the de facto living implementation.

**Decisions:**

- **S4a — A written, testable reconciliation policy is a Phase 1 deliverable. Decision: YES.**
  Per property: precedence order, conflict classification (equivalent / reconcilable / conflict),
  and write-synchronization rule.
- **S4b — MWG treatment.
  Decision: choice 2 of [1. ignore MWG | 2. adopt MWG guidance as a frozen historical input, with
  ExifTool's MWG module behavior as a compatibility reference | 3. treat MWG as a living
  authority].** Option 3 is impossible (the group is defunct); option 1 discards the only widely
  implemented reconciliation convention.

---

## 3. Moderate problems and decisions

### Finding M1 — Consumer / binding strategy undecided

**Decision: choice 2 of [1. C++-only API, exceptions allowed | 2. C++20 API primary with
exception-free result-based error handling, stable C ABI deferred but not precluded | 3. C ABI
first | 4. ship language bindings in v1].**
The public API uses a `Result<T>`/expected-style error model (no exceptions across the API
boundary) so a future C ABI and bindings do not require redesign. No bindings in v1.

### Finding M2 — supported-types.md will drift

**Decision: YES.** Capability data becomes a machine-readable file (part of the registry format);
`supported-types.md` becomes generated output; Tier A CI probes the *pinned* backend versions
against the capability data so drift fails a test instead of silently rotting. Until the generator
exists, the markdown carries a header noting it is a hand-maintained snapshot.

### Finding M3 — Write-safety tests missing from the testing plan

**Decision: YES — add to Tier A:**

1. image/video payload bytes unchanged after metadata-only writes;
2. unknown tags and MakerNotes preserved across a write;
3. write-to-temp-then-atomic-rename policy, tested;
4. non-ASCII paths and non-ASCII metadata values round-trip on all three OSes.

### Finding M4 — Build plan open decisions

- **M4a — Exiv2 acquisition.
  Decision: choice 1 of [1. pinned source via FetchContent with aggressive caching on all OSes |
  2. pinned prebuilt binaries | 3. system packages].** Pinned source keeps one acquisition path
  and exact version control. If Windows build time proves painful, a checksum-pinned prebuilt is
  the sanctioned fallback for Windows only (revisit as its own decision).
- **M4b — Windows Perl. Decision: YES — pin an explicit Strawberry Perl provisioning step** (do
  not rely on whatever the runner image has on PATH), covered by build-contract tests.
- **M4c — Default library linkage.
  Decision: choice 1 of [1. static default (`BUILD_SHARED_LIBS=OFF`), shared supported | 2. shared
  default].** Static simplifies first-consumer integration; shared remains a supported
  configuration (needed later for LGPL-style consumption questions).

### Finding M5 — Registry provenance

**Decision: YES.** The registry records the IPTC Technical Reference version (and every standard
version) it was generated from, so "which standards do you implement?" is answered from data.

### Finding M6 — Test media strategy (new requirement, this session)

**Decision: choice 3 of [1. copy existing third-party test corpora into the repo | 2. generate all
fixtures ourselves | 3. hybrid: generate tiny synthetic fixtures as the in-repo default; use
pinned, checksummed *downloads* of third-party samples for Tier B; never copy third-party corpora
into the repo].**
Reasons: third-party corpora (ExifTool `t/images`, Exiv2 `test/data`) carry their projects'
licenses (Artistic/GPL for ExifTool) and copying them into an Apache-2.0 repo is a licensing trap;
generated fixtures are tiny (1×1 to 16×16 pixel images are sufficient for metadata testing),
deterministic, and legally clean. Full strategy: [docs/test-media-plan.md](../test-media-plan.md).

### Finding M7 — Design-only interface code (new requirement, this session)

**Decision: YES — with constraints.** Public-interface shape is exactly the kind of design detail
that is too verbose for markdown and rots when transcribed. Design-draft C++ headers live under
`include/umm/`, each carrying a `DESIGN DRAFT` banner; they are **not** wired into any build and
**not** tested until the session that implements them. They are the normative statement of API
shape; implementation sessions may amend them (header first, then code).

---

## 4. Alternatives considered (why not a different approach)

| Alternative | Assessment | Decision |
|---|---|---|
| Single-backend library (Exiv2-only or ExifTool-only) | Simpler, but loses the coverage complementarity proven in supported-types.md (PGF vs CR3/HEIC/video) and forfeits cross-backend verification, one of the project's most distinctive ideas. | **NO** — but sequencing is Exiv2-first with the backend interface designed for two. |
| Contribute to an existing project (Exiv2, Adobe XMP Toolkit, KDE libs) | None provides a standards-registry semantic layer over multiple engines; that layer is precisely the gap libumm fills. | **NO** — record the prior-art rationale in concept.md. |
| Data/spec-only project (publish registry + mappings, no runtime) | Good internal decomposition — the registry should be consumable standalone — but applications need the read/write engine. | **NO as the whole project; YES as a component boundary** (registry is standalone-consumable). |

---

## 5. Decision index

| ID | Decision | Outcome |
|---|---|---|
| S1a | ExifTool adapter: out-of-process, `-stay_open`, JSON | YES |
| S1b | Backends optional at runtime; both required in CI | YES |
| S1c | Consumer ExifTool acquisition | Locate, don't bundle (choice 2); later P3/P9 helper |
| S1d | License | Apache-2.0, provisional (choice 2) |
| S2 | Registry before read/write implementation | YES (phases swapped) |
| S3 | Phase 1 scope | JPEG + XMP sidecar only (choice 2) |
| S4a | Reconciliation policy is a Phase 1 deliverable | YES |
| S4b | MWG as frozen input, ExifTool MWG as compat reference | choice 2 |
| M1 | API/error model | C++20, Result-based, no exceptions across API; C ABI deferred (choice 2) |
| M2 | Capabilities become machine-readable; markdown generated; CI drift checks | YES |
| M3 | Write-safety + encoding tests in Tier A | YES |
| M4a | Exiv2 acquisition | Pinned source FetchContent (choice 1) |
| M4b | Windows Perl pinned (Strawberry) | YES |
| M4c | Linkage default | Static default, shared supported (choice 1) |
| M5 | Registry records standard/TR versions | YES |
| M6 | Test media | Hybrid: generate in-repo, download Tier B (choice 3) |
| M7 | Design-draft headers under `include/umm/` | YES |
