# Feature request 1 — decision record and session plan — 2026-11-26

Status: **F1–F5 implemented in v0.2.0** (sessions 52–59). **F6** (session 60),
**F7** (session 61), **F8** (session 62), **F9** (session 63), and **F14**
(session 71) are implemented toward v0.3.0.
F10–F13 and F15–F16 remain accepted design only. The maintainer answered OQ-F1–OQ-F6 on 2026-11-26 (§5). The same
answers added F16 (metadata diff) and the §6 research on payload hashing. Sessions
52–77 in [docs/implementation/](../implementation/) carry the *what*; this record
keeps the *why*.

Source: [docs/feature-request-1.md](../feature-request-1.md) (requests U1–U14, written
against the v0.1.2 tag). This record starts the **F** series (F1–F16). It does not change
any C-, H-, S-, M-, R-, or P-series decision except where a row says so.

Per the repository rules, the user, sysadmin, and developer docs are not edited ahead of the
code. Each session updates them for the behavior it ships.

---

## 1. Request claims rechecked against current sources

Checked on `main` at `804a230` (version macros still `0.1.2`).

| # | Claim in the request | Current source | Result |
|---|---|---|---|
| U1 | Backends single-threaded; manager holds one instance per id; free functions use the singleton | `include/umm/backend.hpp:11-13`, `BackendManager::instance()` and `backends_` (`backend.hpp:126-143`) | Confirmed. No locking in `src/backends/backend_manager.cpp` |
| U1 | `describe` and `cast` need no backend | `describe(id, path)` (`describe.hpp:97`) and `cast(path, …)` (`umm.hpp`) read the file; `cast` with `dry_run = false` also writes | **Correction:** only `describe(id)`, `registry()`, `version()`, `capabilitiesForType()`, `importTrack()`, `matchTrack(Metadata, …)`, path-pairing helpers and value helpers are backend-free |
| U9 | No cancellation or deadline; only `command_timeout` (30 s) | `ReadOptions`, `WriteOptions`, `SyncOptions` (`umm.hpp`); `ExifToolConfig::command_timeout` (`backend.hpp:121`); `ChildProcess` polls with a deadline (`src/backends/exiftool/process.cpp:580-585`) | Confirmed. The existing poll loop is the natural cancellation point |
| U10 | `read` always does the full read | `read()` → `load_read()` → reconcile (`src/read.cpp:66-127`) | Confirmed |
| U4 | Capabilities are per backend and category only | `include/umm/capabilities.hpp:18-62` | Confirmed |
| U8 | No `DateTime` arithmetic | `include/umm/value.hpp:30-40` | Confirmed |
| U2 | Make/model/lens/orientation/size are not canonical (C17) | C17 in [casting record](2026-10-03-casting-and-canonical-model-decisions.md) | Confirmed. C17 already allows **video** `recordingDevice` and VMH technical properties through upcasts; `recordingDevice` is in the first rule set (`registry/casts/recording-device.json`) |
| U3 | `videoTechnical` cast is "later" | Casting record §4 catalog | Confirmed. **Also:** Exiv2 is built with `EXIV2_ENABLE_VIDEO OFF` (`cmake/LibummExiv2.cmake:148`), so without ExifTool, MP4/MOV get only the BMFF "rudimentary read" (`docs/supported-types.md`) |
| U3/U12 | Casts fill canonical values on read | Casts are opt-in: `report_casts` previews, `umm::cast(dry_run = false)` applies **and writes** (`src/cast.cpp:43-67`); `docs/supported-types.md` §"direct + cast" | **Gap the request implies but does not name:** there is no read-time, in-memory upcast. F9 adds it |
| U11 | No `fsync`; Windows write-through only on fallback | `src/core/atomic_write.cpp:101-121` | Confirmed |
| U12 | `windowsXP`, `exifUserComment`, `pngText`, `quickTimeUserData`, `hierarchicalKeywords` are "later" | Casting record §4 | Confirmed |
| U13 | `regionsPeople` is "later" | Casting record §4 | Confirmed |
| U14 | No payload digest | No hashing code in `src/` | Confirmed. No SHA implementation exists in C++ sources |
| U5 | Path-only reads | `umm.hpp` entry points | Confirmed |
| U6 | BMP, MKV, AVI, WAV, ASF have no canonical mapping | Routed in `registry/capabilities/policy.json` | Confirmed. ExifTool reads but cannot write these types; Exiv2 BMP is identify-only |
| U7 | `configureExifTool` call-order rules undocumented | `backend.hpp:129` | Confirmed |

---

## 2. Decisions (accepted 2026-11-26)

### F1 — Thread-safety contract, in two steps (U1)

**Decision.** Ship the request's option 2 first, then option 1. Reject option 3 (a
`Session`/`Context` object) for now: it doubles every entry point and moves pool sizing to
callers.

- **Step 1 (session 52):** every public entry point is documented as thread-safe. Functions
  that touch a backend serialize per backend id inside libumm. Backend-free functions (§1
  U1 row) are documented as always concurrent.
- **Step 2 (session 54):** `BackendManager` leases backend instances from a bounded pool
  per backend id. `ExifToolConfig::max_processes` (default 1, so today's behavior is the
  default) bounds `-stay_open` children. Exiv2 instances are cheap; their pool size follows
  the same setting or hardware concurrency (session 54 decides after measuring).
- Exiv2 global state (XMP toolkit initialization, namespace registration) is initialized
  once, before any lease, under `std::call_once`. Session 52 audits `src/backends/exiv2/`
  and `src/exiv2_shim.cpp` for other process-wide state.
- `Backend` instances stay single-threaded; the contract moves from "callers must not
  share" to "the manager never shares".

### F2 — `configureExifTool` call order (U1, U7)

**Decision.** Callable any number of times, from any thread, before or after reads start.
A new configuration applies to calls that start after it returns. In-flight calls finish on
the old process. Idle children from the old configuration are shut down; busy ones are
reaped when their lease ends.

### F3 — Cancellation and deadlines (U9)

**Decision.** A new header `include/umm/cancel.hpp` defines `umm::CancelToken` (shared
state, `cancel()`, `cancelled()`; copyable; cheap). `ReadOptions`, `WriteOptions`,
`SyncOptions`, and `CastOptions` gain `CancelToken cancel` and
`std::optional<std::chrono::steady_clock::time_point> deadline`.

- New `ErrorCode::cancelled` (token) and `ErrorCode::deadline_exceeded` (deadline),
  appended at the end of the enum (two codes confirmed, OQ-F5). `backend_timeout` keeps
  meaning `command_timeout`.
- **Checkpoints:** before backend selection, between backend read and reconcile, between
  embedded and sidecar reads, and immediately before the atomic rename. Exiv2 calls are not
  interrupted mid-call (in-process, no safe abort); ExifTool calls are (F4).
- **Writes:** cancellation is honored up to the rename. After the rename the write has
  committed and the call returns success. Before it, the temp file is removed and the
  original is byte-identical (M3 already guarantees this shape).
- Mixed and `synchronize()` writes: a cancel between the two carriers returns `cancelled`
  with the same partial-state wording as today's second-carrier failure.

### F4 — In-flight ExifTool cancellation (U9)

**Decision.** `ChildProcess`'s existing poll loop (`process.cpp`) wakes at a bounded
interval (≤ 50 ms) to check the token and deadline. On cancel it kills the child and
respawns lazily, exactly like the `command_timeout` path, so no zombie remains and the next
call succeeds. The token check never blocks on the child.

### F5 — Per-property access (U4)

**Decision.** In `include/umm/capabilities.hpp`:

- `propertyAccess(path, property_id, StoragePolicy) -> Result<PropertyAccess>` and
  `propertyAccessAll(path, StoragePolicy) -> Result<std::vector<PropertyAccess>>`.
- `PropertyAccess { std::string property_id; Access access; std::string backend;
  AccessReason reason; std::string message; }`.
- `enum class AccessReason` with stable string forms (`to_string`):
  `ok`, `backend_unavailable` (with `backend` naming it, e.g. `exiftool`),
  `policy_requires_sidecar`, `policy_forbids_sidecar`, `no_mapping_for_type`,
  `container_read_only`, `property_read_only` (technical or one-way cast targets),
  `unknown_property`. Stable within a minor version (request); new reasons are appended.
- **One code path:** the implementation runs the same storage evaluation and write-sync
  mapping as `write(..., dry_run = true)` for a single-property change. It does not consult
  a second table. The agreement matrix (session 58) is the acceptance test.
- No file read beyond sniffing; the result depends on path type, policy, and the backends
  available now.

### F6 — `DateTime` shift and offset helpers (U8)

**Decision.** Free functions in `include/umm/value.hpp`, backend-free and always
thread-safe:

- `shift(const DateTime&, std::chrono::seconds) -> Result<DateTime>`. The result keeps the
  input's precision. The shift must be representable at that precision, else
  `invalid_value`: whole days for date-only values; whole minutes when seconds are absent;
  any seconds when seconds are present (sub-seconds unchanged). Year-only and year-month
  values reject every non-zero shift.
- `withOffset(const DateTime&, int minutes, OffsetMode) -> Result<DateTime>` with
  `OffsetMode::reinterpret` (same wall time, new zone) and `OffsetMode::convert` (same
  instant). `convert` fails with `invalid_value` when the offset is unknown or the value has
  no time component. An unknown offset is never treated as UTC (H12).
- Proleptic Gregorian calendar arithmetic (no time zone database, no leap seconds),
  implemented with `std::chrono` calendar types (C++20, already required).

### F7 — Selective reads (U10)

**Decision.** `ReadOptions::properties` (`std::optional<std::vector<std::string>>`). When
set: unknown ids fail with `unknown_property`; only the listed canonical ids (and cast
groups targeting them, when F9 upcasts are requested) are reconciled; `dumpAll()` and
`dumpUnmapped()` are empty and documented as such; sidecar pairing still follows
`merge_sidecar`. The ExifTool adapter receives the derived base-key list (from the registry
representations of those ids) and requests only those tags. Exiv2 still parses the whole
file; its saving is reconcile and dump work. Results for the listed ids must equal a full
read.

### F8 — `readMany` (U10)

**Decision.** `readMany(std::span<const std::filesystem::path>, ReadOptions,
std::function<void(const std::filesystem::path&, Result<Metadata>)>) -> Result<void>`.
The callback runs on the calling thread, once per path, in input order. Per-file errors go
to the callback; the return value fails only for cancellation, deadline, or argument
errors. The ExifTool adapter batches up to N files per `-execute` (N fixed in session 62
after measurement) and splits results by `SourceFile`.

### F9 — Read-time upcasts (prerequisite for U3, U12, U13)

**Decision.** `ReadOptions::upcast` (`std::optional<std::vector<std::string>>`), mirroring
`WriteOptions::downcast`: `nullopt` = library defaults, empty = none. Applied groups fill
canonical values **in memory only**, with provenance that names the cast group and source
key (visible through `report_casts` and `describe`). Nothing is written. Only rules whose
status would be `can_cast` apply; `needs_force` and `ambiguous` never apply on read.

**Default.** `nullopt` means no groups (OQ-F1, confirmed): C9 opt-in behavior is
unchanged. Callers that want technical or legacy-text values on read list the groups.

### F10 — Video technical values from QuickTime (U3)

**Decision.** Implement the `videoTechnical` one-way upcast group (casting record §4)
targeting `fileDuration`, `frameSize`, `videoFrameRate`, `videoCoding`, audio sample rate
and channels, and `orientation` (from rotation). Each source key's backend requirement is
documented in the generated property reference.

For "without ExifTool", session 64 first spikes `EXIV2_ENABLE_VIDEO ON` against the pinned
Exiv2 0.28.9 and records what it reports for the MP4/MOV fixtures. Exiv2's video readers
report container facts under synthetic `Xmp.video.*` / `Xmp.audio.*` keys. Those are not
stored XMP, so under C7 they are **cast sources**, never representations, and they must not
be written back. If the spike shows the Exiv2 path is unusable, the session records the
gap and U3 ships ExifTool-only with the reason surfaced by F5.

**Exiv2 video build (OQ-F2, accepted).** If the spike is clean, session 64 turns
`EXIV2_ENABLE_VIDEO` on in `cmake/LibummExiv2.cmake` for the bundled build, updates the
Exiv2 capability probe, and regenerates `docs/supported-types.md`. The page is generated;
it is never hand-edited.

### F11 — Read-only technical view (U2)

**Decision.** Keep C17: no EXIF canonical domain. Add a read-only, non-canonical view
`Metadata::technical() -> const TechnicalInfo&` in `include/umm/metadata.hpp`:
`cameraMake`, `cameraModel`, `lensModel`, `orientation` (EXIF 1–8, or rotation degrees plus
mirror flag for video), `pixelSize` (width, height). Each field is
`std::optional<TechnicalValue<T>>` carrying the value and the `BaseKey` it came from.

- **Sources are data:** a new `registry/technical/` source table (priority lists per field
  and per media domain), code-generated like the cast rules, so no hand-written tag names
  appear in `src/` (contract test).
- **Video** fills the view from the canonical `recordingDevice` and F10 values, so one
  accessor set works for both media types.
- **Pixel size** prefers the container's intrinsic dimensions over EXIF
  `PixelXDimension`/`PixelYDimension`, which editors often leave stale. Intrinsics are not
  stored metadata, so they travel in a new `BaseDocument::intrinsics` list, not
  `entries`; `dumpAll()` is unchanged. ExifTool's `File:` group is currently omitted
  (`backend.hpp` key mapping note); the adapter reads only the intrinsic tags it needs.
- The view is filled on every read (cheap) and honors F7 through the pseudo-ids
  `technical.*`.

### F12 — Legacy and hierarchical keyword sources (U12)

**Decision.** Implement the five "later" groups as **up-only** cast groups, applied on read
through F9 and listed in `castCandidates()`. Write-sync for legacy copies:

- A canonical write **does not** update or remove `XP*`, PNG text, QuickTime `©nam`…,
  `UserComment`, or `lr:hierarchicalSubject`. Rationale: libumm does not write
  non-standard carriers except through explicit downcasts, and removing data other tools
  rely on is destructive. After a canonical edit the legacy copy can disagree; `describe`
  and `report_casts` show it as `needs_force` / differing.
- Encoding rules: `XP*` is UCS-2LE in EXIF bytes; `UserComment` honors its 8-byte charset
  prefix (ASCII, UNICODE, JIS, undefined), with `exifUserComment` marked approximate as
  already planned; H6 takes the leaf term of `lr:hierarchicalSubject`, then H4 union.

### F13 — People regions (U13)

**Decision.** `regionsPeople` read cast from MWG `mwg-rs:Regions` (type `Face`, `Name`)
and Microsoft `MPRI` people to `personShownInTheImage` names (up only). Writing regions
stays through the canonical `iptc.photo.imageRegion` representation; libumm does **not**
mirror IPTC regions into MWG regions (no standard defines that link; C7). Video regions:
**not planned**; VMH has no region equivalent.

### F14 — Durable writes (U11)

**Decision.** `WriteOptions::durable` and `SyncOptions::durable` (default `false`).
POSIX: `fsync` the temp file, rename, `fsync` the parent directory (on macOS
`fcntl(F_FULLFSYNC)` for the file). Windows: `FlushFileBuffers` on the temp file, then
`ReplaceFileW` followed by a flush of the destination, or `MoveFileExW` with
`MOVEFILE_WRITE_THROUGH`. New fault-injection points cover each flush step. The guarantee
per platform is documented in the user guide.

### F15 — Nice-to-have scope (U14, U5, U6)

- **U14 payload digest.** `payloadDigest(path) -> Result<PayloadDigest>` with
  `{std::string scheme; std::string hex;}`. SHA-256 is implemented in-repo (OQ-F4,
  approved): no existing dependency provides it (Exiv2 does not expose hashing; OS crypto
  APIs differ per platform), and a small implementation checked against the FIPS 180-4 test
  vectors is cheaper than a new dependency.
  - **Compatibility first.** No IPTC, XMP, or EXIF field standardizes a "payload without
    metadata" hash (§6). The closest widely used convention is ExifTool's `ImageDataHash`.
    For every type where it is feasible, libumm hashes the **same byte ranges** as
    `exiftool -api ImageHashType=SHA256 -ImageDataHash`. Those digests use scheme
    `exiftool-imagedatahash-sha256`, so a libumm digest equals the one ExifTool users
    already have. Where libumm cannot or does not want to match (for example, ExifTool
    includes `JpgFromRaw` for some RAW types), the type uses scheme
    `umm-payload-v1-sha256` and the session doc records the byte-range definition.
  - **First cut:** JPEG, PNG, WebP, MP4/MOV. Session 73 starts by reading ExifTool's
    `ImageDataHash` implementation (by type) and fixes each type's scheme from that.
    Others return `unsupported_capability`.
  - **Cross-check:** when ExifTool is installed, a test compares libumm's digest with
    `ImageDataHash` (SHA256) for every Tier A fixture of an `exiftool-*` scheme type.
  - **Storing the digest** (ExifTool's `XMP-et:OriginalImageHash` /
    `OriginalImageHashType`) is not planned. `et` is ExifTool's private namespace, not a
    standard; under C7 it could only be an explicit cast. Revisit on request.
- **U5 bytes.** `read(std::span<const std::byte>, ReadOptions, std::string_view
  file_type_hint = {})`. Exiv2 uses `MemIo`. ExifTool's stdin is its argfile, so the
  adapter writes a private temp file and removes it after the read. No sidecar pairing.
- **U6 types.** Neither backend writes BMP, MKV, AVI, WAV, or ASF embedded metadata, so
  "editing" means `sidecar_only` / `sidecar_recommended` XMP sidecars plus read mapping of
  standard representations (embedded XMP in RIFF `_PMX`) and casts for container tags.
  Session 76 records this in `registry/capabilities/` and the generated supported-types
  page.
- **WAV (OQ-F3).** The maintainer accepted the video domain *if* it allowed embedded
  writes. It does not: ExifTool reads WAV only (`registry/capabilities/exiftool.json`,
  "Read-only in both backends") and Exiv2 has no WAV writer, so the media domain does not
  change where metadata can be written. WAV is routed to the **video** domain so its VMH
  audio fields (duration, sample rate, channels) and embedded XMP read through the same
  mappings as MP4/MOV audio. Editing stays **sidecar-only**. Embedded WAV writes would need
  a new RIFF writer, which is out of scope (libumm integrates existing implementations).

### F16 — Metadata diff and file comparison (follows U14)

**Decision.** Add a pure, backend-free comparison in a new header
`include/umm/diff.hpp`:

- `diff(const Metadata& a, const Metadata& b, DiffOptions = {}) -> MetadataDiff`. It lists
  canonical properties added, removed, and changed. Changes compare normalized values with
  the same rules reconcile uses to decide that two carriers agree (`src/core/reconcile.cpp`,
  behind `detectConflict`), so it never reports formatting-only noise. `Metadata` has no
  `operator==` today; the session adds the comparison helper. Optionally
  (`DiffOptions::base_entries`) it also lists base-entry and unmapped differences by
  `BaseKey` (`BaseEntry` already has defaulted equality). Diff is read-only and never
  applies casts. Changed entries carry both values in the `ConflictEntry` style so callers
  can render both sides.
- `compareFiles(path_a, path_b, CompareOptions = {}) -> Result<FileComparison>`. It reads
  both files and calls `payloadDigest` on each. It classifies the pair as `identical`,
  `metadata_only` (same payload digest, different metadata), `payload_only`, or `both`, and
  carries the `MetadataDiff`. If no digest is available for the type, it reports
  `payload_unknown` instead of guessing.

Rationale: the request behind U14 ("same photo, only metadata differs") is a diff
question. Putting it in libumm keeps the canonical comparison rules (H-series reconcile
normalization) in one place instead of every caller re-implementing them.

---

## 3. Session map and releases

Sessions are sized for about one hour. "Pairs with" marks small sessions that can share one
sitting. Session numbers follow release order (OQ-F6): **v0.2.0** ships sessions 52–59
(concurrency, cancellation, per-property access), **v0.3.0** ships sessions 60–77.

### v0.2.0 — concurrency, cancellation, property access

| Session | Title | Requests | Depends on | Pairs with |
|---|---|---|---|---|
| [52](../implementation/52-thread-safety-contract.md) | Thread-safety contract and serialized entry points | U1, U7 | — | — |
| [53](../implementation/53-parallel-read-tests-and-tsan.md) | Parallel-read tests and ThreadSanitizer CI | U1 | 52 | — |
| [54](../implementation/54-backend-instance-pool.md) | Backend instance pool | U1 | 53 | — |
| [55](../implementation/55-cancellation-api-and-checkpoints.md) | Cancellation API and core checkpoints | U9 | 52 | — |
| [56](../implementation/56-exiftool-in-flight-cancellation.md) | ExifTool in-flight cancellation | U9 | 54, 55 | — |
| [57](../implementation/57-property-access-api.md) | Per-property access API and reason codes | U4 | — | — |
| [58](../implementation/58-property-access-agreement-matrix.md) | Per-property access agreement matrix | U4 | 57 | — |
| [59](../implementation/59-v0.2.0-verification-and-release.md) | v0.2.0 verification and release | U1, U4, U7 (partial), U9 | 52–58 | — |

### v0.3.0 — remaining requests

| Session | Title | Requests | Depends on | Pairs with |
|---|---|---|---|---|
| [60](../implementation/60-datetime-shift-helpers.md) | `DateTime` shift and offset helpers | U8 | — | 71 |
| [61](../implementation/61-selective-reads.md) | Selective reads | U10 | 52 | — |
| [62](../implementation/62-read-many-and-batching.md) | `readMany`, ExifTool batching, benchmark | U10 | 54, 55, 61 | — |
| [63](../implementation/63-read-time-upcasts.md) | Read-time upcasts | (U3, U12, U13) | — | — |
| [64](../implementation/64-video-technical-upcasts.md) | Video technical upcasts from QuickTime | U3 | 63 | — |
| [65](../implementation/65-technical-view-contract.md) | Technical view: header, source table, codegen | U2 | — | — |
| [66](../implementation/66-technical-view-values.md) | Technical view: photo and video values | U2 | 64, 65 | — |
| [67](../implementation/67-legacy-text-casts.md) | Legacy EXIF text casts (`windowsXP`, `exifUserComment`) | U12 | 63 | — |
| [68](../implementation/68-container-text-casts.md) | Container text casts (`pngText`, `quickTimeUserData`) | U12 | 63 | — |
| [69](../implementation/69-hierarchical-keywords.md) | Hierarchical keywords and legacy write-sync policy | U12 | 67, 68 | 70 |
| [70](../implementation/70-people-regions.md) | People regions read cast and guidance | U13 | 63 | 69 |
| [71](../implementation/71-durable-writes.md) | Durable write option | U11 | — | 60, 72 |
| [72](../implementation/72-application-packaging-guide.md) | Application packaging and ExifTool guide | U7 | 52 (54 for the pool text) | 71 |
| [73](../implementation/73-payload-digest.md) | Payload digest (ExifTool `ImageDataHash`-compatible) | U14 | — | — |
| [74](../implementation/74-metadata-diff.md) | Metadata diff and file comparison | U14 (F16) | 73 | — |
| [75](../implementation/75-read-from-bytes.md) | Read from bytes | U5 | 52 | — |
| [76](../implementation/76-more-type-mappings.md) | Read mappings and sidecars for BMP, MKV, AVI, WAV, ASF | U6 | 64 (Exiv2 video spike) | — |
| [77](../implementation/77-feature-request-1-verification.md) | v0.3.0 verification, doc audit, release | all | 60–76 | — |

### Suggested order

Blocking items first, then the items that unblock other work:

1. **52 → 53** (U1 minimum; unblocks parallel scans). **57 → 58** (U4) can run in parallel.
2. **55**, **54**, **56**, then **59** (cut v0.2.0).
3. **60** (+ **71**), **61 → 62** (scan speed), **63 → 64** (video technical).
4. **65 → 66** (technical view), **67 → 68 → 69** (+ **70**).
5. **72**, **73 → 74**, **75**, **76**, then **77** (cut v0.3.0).

Tracks that touch the same files (`umm.hpp` options structs, `src/read.cpp`) are ordered so
that two sessions never change the same struct in parallel: 55, 61, 63, and 71 each add
`ReadOptions`/`WriteOptions` members, so they run one after another.

---

## 4. Documentation map

| Doc | Updated by |
|---|---|
| `include/umm/*.hpp` (normative, before code) | each session's first step |
| `docs/user/guide.md` | 52, 55, 57, 60, 61, 62, 63, 64, 66, 67–70, 71, 73, 74, 75 |
| `docs/user/properties/` (generated) | 64, 65, 67–70 (regenerate) |
| `docs/supported-types.md` (generated) | 64 (if Exiv2 video is enabled), 76 |
| `docs/sysadmin/install.md` | 53 (sanitizer build), 64 (Exiv2 video option), 72 |
| `docs/reconciliation-policy.md` | 63 (upcast provenance), 69 (legacy write-sync) |
| `docs/developer/canonical-model.md` | 63, 65, 67–70 |
| `docs/developer/implementation-history.md` | 59 (stage row for 52–59), 77 (stage row for 60–77) |
| `docs/developer/release-notes.md` | every session that changes the public API or layout; 59 and 77 cut the release |
| `docs/feature-request-1.md` | 59 and 77 (status column per request) |

---

## 5. Maintainer answers (2026-11-26)

| Question | Answer | Effect |
|---|---|---|
| **OQ-F1** — Default upcasts on read | Off by default | F9: `ReadOptions::upcast = nullopt` applies no groups; C9 unchanged. |
| **OQ-F2** — Exiv2 video readers | Yes, if the session 64 spike is clean | F10: session 64 enables `EXIV2_ENABLE_VIDEO`, updates the probe, regenerates `docs/supported-types.md`. |
| **OQ-F3** — WAV domain | Video, if that allows embedded writes | It does not (no backend writes WAV). F15: WAV reads as video domain; editing stays sidecar-only. |
| **OQ-F4** — In-repo SHA-256 | Approved; research a standard field | F15: in-repo SHA-256, byte-compatible with ExifTool `ImageDataHash` where feasible (§6). New F16 diff. |
| **OQ-F5** — ErrorCode additions | Two codes | F3: `cancelled` and `deadline_exceeded`. |
| **OQ-F6** — Version | v0.2.0, then v0.3.0 | §3: v0.2.0 = sessions 52–59; v0.3.0 = sessions 60–77. Each release bumps `SOVERSION` per `docs/abi-policy.md` (pre-1.0: major.minor). |

---

## 6. Research: is there a standard payload hash? (U14)

Question from the maintainer: is there a standard field for a hash of the media payload
that excludes metadata, so two files that differ only in metadata can be detected?

| Source | What it is | Fit for U14 |
|---|---|---|
| ExifTool `ImageDataHash` (`html/TagNames/Extra.html`, `exiftool/exiftool`) | Computed on request (`-ImageDataHash` or `-api requesttags=imagedatahash`). Algorithm `-api ImageHashType=MD5\|SHA256\|SHA512` (default MD5). Covers JPEG, TIFF, PNG, CRW, CR3, MRW, RAF, X3F, IIQ, JP2, JXL, HEIC, AVIF, MOV/MP4 (video and audio data), and RIFF AVI, WAV, WEBP. Hashes main image data (plus `JpgFromRaw`/`OtherImage` for some types); excludes `ThumbnailImage` and `PreviewImage`. | Best match: de facto convention, widely deployed (also used by exiftool-vendored.js consumers). Not a standard. |
| ExifTool `XMP-et:OriginalImageHash` / `OriginalImageHashType` | Where ExifTool suggests storing the hash. `et` is ExifTool's own XMP namespace (`lib/Image/ExifTool/XMP.pm`). | Non-standard carrier; not a representation under C7. |
| DNG `RawImageDigest`, `NewRawImageDigest`, `OriginalRawFileDigest` | MD5 digests defined by the DNG specification. | DNG only. |
| C2PA hard bindings (`c2pa.hash.data`, `c2pa.hash.bmff`) | Standard hash of the asset with declared exclusion ranges (the manifest, optionally other boxes). | Provenance signing; exclusion ranges are chosen per manifest, so two signers can hash different bytes. Not a general comparison key. |
| XMP `xmpMM:DocumentID` / `InstanceID` | Identity fields assigned by editors. | Identity, not content; editors change or copy them. |
| Perceptual hashes (pHash, dHash) | Similarity after re-encoding or resizing. | Different problem (near-duplicates); out of scope. |

**Conclusion.** No IPTC, XMP, EXIF, or container standard defines a "payload hash without
metadata". ExifTool's `ImageDataHash` is the convention most tools already produce, so F15
matches its byte ranges (scheme `exiftool-imagedatahash-sha256`) and verifies against
ExifTool in CI. F16 builds the "only metadata differs" comparison on top of it.
