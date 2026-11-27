// Application-facing entry points (concept.md §32).
// umm::read reconciles embedded metadata and, by default, a paired XMP
// sidecar (session 14). umm::write applies StoragePolicy (concept.md §12)
// using capability data for the sniffed type (session 16).
#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "umm/backend.hpp"
#include "umm/cancel.hpp"
#include "umm/capabilities.hpp"
#include "umm/describe.hpp"
#include "umm/digest.hpp"
#include "umm/metadata.hpp"
#include "umm/registry.hpp"
#include "umm/result.hpp"
#include "umm/track.hpp"
#include "umm/value.hpp"
#include "umm/version.hpp"

namespace umm {

struct StorageDecision {
  enum class Method { embedded, sidecar, mixed } method{Method::embedded};
  std::vector<std::string> formats;  // e.g. {"XMP", "EXIF", "IPTC-IIM", "QuickTime"}
  std::string backend;               // backend that performed/will perform the write
};

struct ReadOptions {
  std::string backend;          // empty = type preferred_backend, else first available
  bool merge_sidecar{true};     // pair media.xmp per session-14 pairing rules
  bool conflicts_as_errors{false};  // else recorded in Metadata::conflictedPropertyIds()
  // When true, a non-sidecar path with no paired .xmp fails
  // (ErrorCode::io_not_found). StoragePolicy::sidecar_required on write is
  // the matching persistence rule.
  bool sidecar_required{false};
  // When true, fill Metadata::castCandidates() with up/down/side previews.
  bool report_casts{false};
  CancelToken cancel;
  std::optional<std::chrono::steady_clock::time_point> deadline;
  // Selective read (decision F7). nullopt reconciles every domain id.
  // A set list reconciles only those registry ids (iptc.photo.* /
  // iptc.video.*, not accessor names). Unknown ids fail with
  // unknown_property. dumpAll() and dumpUnmapped() are empty. Sidecar
  // pairing still follows merge_sidecar and sidecar_required.
  std::optional<std::vector<std::string>> properties;
  // Read-time upcasts (decision F9). nullopt and empty apply no groups
  // (OQ-F1: off by default). A set list applies those up groups in memory
  // only. Unknown ids fail with invalid_value. A named approximate group
  // (videoCreated) is applied; umm::cast still requires include_approximate.
  // needs_force and ambiguous never apply. A representation always wins.
  std::optional<std::vector<std::string>> upcast;
};

struct WriteOptions {
  std::string backend;          // empty = capability-driven choice
  StoragePolicy policy{StoragePolicy::preferred};
  bool dry_run{false};          // compute WriteReport without touching files
  // Downcast groups to apply on write. nullopt = library defaults
  // (capturePosition on video). Empty vector = none.
  std::optional<std::vector<std::string>> downcast;
  CancelToken cancel;
  std::optional<std::chrono::steady_clock::time_point> deadline;
  // When true, the call does not return until the replacement is flushed
  // (decision F14). Default false leaves the existing rename path unchanged.
  // POSIX: fsync the temp file (macOS F_FULLFSYNC), rename, fsync the parent
  // directory. Windows: FlushFileBuffers on the temp file, ReplaceFileW, then
  // flush the destination; the fallback is MoveFileExW with
  // MOVEFILE_WRITE_THROUGH. A flush that fails before the rename leaves the
  // original byte-identical. A flush that fails after the rename returns
  // io_write_failed even though the new bytes are already in place.
  bool durable{false};
};

struct WriteReport {
  StorageDecision decision;
  std::vector<BaseKey> written;  // every base representation updated (write-sync)
};

// detectConflict() result: full read plus every disagreed property (session 23).
struct ConflictReport {
  Metadata metadata;
  std::vector<ConflictEntry> entries;
};

// synchronize() direction (concept.md §12 / §28). Default writes the
// reconciled canonical state to every selected carrier.
enum class SyncDirection {
  both,                 // reconciled state → embedded and sidecar
  embedded_to_sidecar,  // embedded-only read → sidecar write (media unchanged)
  sidecar_to_embedded,  // sidecar-only read → embedded write
};

struct SyncOptions {
  std::string backend;  // empty = type preferred_backend, else first available
  SyncDirection direction{SyncDirection::both};
  // When set, this canonical state is written (session 23 merge output).
  // When unset, synchronize() reads first according to `direction`.
  std::optional<Metadata> metadata;
  bool dry_run{false};
  CancelToken cancel;
  std::optional<std::chrono::steady_clock::time_point> deadline;
  // Same per-platform flush sequence as WriteOptions::durable. Sidecar and
  // embedded carriers each honor the flag.
  bool durable{false};
};

// One carrier actually targeted by synchronize() (or mixed umm::write).
struct SyncCarrierReport {
  std::string container;  // "embedded" | "sidecar"
  std::vector<BaseKey> written;
};

struct SyncReport {
  StorageDecision decision;
  std::vector<SyncCarrierReport> carriers;
  Metadata metadata;  // canonical state that was (or would be) written
};

// --- Asset pairing (concept.md §28) -----------------------------------------
// A media file and an XMP sidecar with the same stem in the same directory
// are one asset. The sidecar extension is ".xmp".
// On case-sensitive filesystems, ".xmp" is tried first, then ".XMP".
// On case-insensitive filesystems the OS resolves the name.
// A path that is itself an XMP sidecar is not paired with another sidecar.

bool isXmpSidecarPath(const std::filesystem::path& path);

// Canonical write path (same directory, same stem, ".xmp"), even if missing.
std::filesystem::path sidecarPath(const std::filesystem::path& media);

// Existing sidecar next to `media`, if any.
std::optional<std::filesystem::path> findSidecar(
    const std::filesystem::path& media);

// Thread-safety (decision F1): read, readMany, write, synchronize, cast,
// capabilities, and propertyAccess may be called concurrently, including on
// the same path.
// A Metadata instance is not synchronized; do not mutate one object from two
// threads. configureExifTool may run during reads: calls that have already
// started finish on the previous ExifTool process; later calls use the new
// config. CancelToken and deadline are checked at checkpoints, not mid-Exiv2
// call. After an atomic rename has succeeded, the call returns success even
// if the token is then cancelled.
//
// --- Entry points ------------------------------------------------------------

// Read + reconcile (docs/reconciliation-policy.md) into canonical Metadata.
// Embedded metadata, plus sidecar XMP when merge_sidecar and a pair exists.
// A standalone .xmp file is readable as sidecar-only. sidecar_required fails
// when a non-sidecar path has no paired .xmp. ReadOptions::properties limits
// reconciliation to those ids; listed-id values match a full read.
// ReadOptions::upcast fills empty targets from named up groups in memory
// only; the file is not modified. Metadata::technical() is filled on every
// read from the generated source table. A selective list fills only the
// technical.* ids it names; other technical fields stay absent.
Result<Metadata> read(const std::filesystem::path& media, ReadOptions options = {});

// Stream one result per path, on the calling thread, in input order
// (decision F8). Per-file errors go to the callback. The return fails only
// for cancellation, deadline, or argument errors (empty callback, unknown
// property, unknown upcast group, unknown backend name). A cancel or
// deadline stops further callbacks, including the rest of an in-flight
// ExifTool batch. ExifTool sends up to kExifToolReadBatch paths per
// -execute; Exiv2 loops. Sidecar pairing matches read().
Result<void> readMany(
    std::span<const std::filesystem::path> paths, ReadOptions options,
    std::function<void(const std::filesystem::path&, Result<Metadata>)> callback);

// Enumerate disagreed properties without inspecting every field of a read.
// Same pipeline as read(); no second reconciliation engine. Entries cover
// Resolution::conflict and Resolution::reconciled (policy-resolved
// disagreement). conflicts_as_errors fails only on unresolved `conflict`.
Result<ConflictReport> detectConflict(const std::filesystem::path& media,
                                      ReadOptions options = {});

// Resolve a disagreed property by choosing a candidate listed in `entry`
// (match SourceRef::base_key). When the same base key appears in more than one
// candidate (embedded vs sidecar XMP), pass `container` ("embedded" or
// "sidecar"). Resulting Metadata is `reconciled` with preferred_source set;
// every existing source is retained.
Result<Metadata> merge(Metadata metadata, const ConflictEntry& entry,
                       std::string_view source,
                       std::string_view container = {});

// User-supplied override. Resolution becomes `reconciled`; preferred_source
// is empty (not a base key); sources are retained. Unlike set(), provenance
// is not discarded.
Result<Metadata> merge(Metadata metadata, std::string_view property_id,
                       Value value);

// Write canonical metadata through the mapping engine to synchronized
// representations, with temp-file + atomic-rename safety (decision M3).
// preferred/embedded_only: writable embedded categories and container GPS
// from capabilities() (video capturePosition downcast writes Keys
// location.ISO6709 and UserData GPSCoordinates).
// sidecar_only: XMP sidecar (media bytes unchanged).
// sidecar_required: sidecar must be written; when embedded writes are also
// available and sidecar is not recommended, Method::mixed writes both.
// Types with sidecar_recommended prefer sidecar writes. A mixed write that
// commits embedded and then fails on the sidecar leaves the embedded file
// updated; there is no multi-file rollback.
Result<WriteReport> write(const std::filesystem::path& media,
                          const Metadata& metadata,
                          WriteOptions options = {});

Result<WriteReport> write(const std::filesystem::path& media,
                          const Metadata& metadata, StoragePolicy policy);

// preferred: sidecar when the path is an XMP sidecar or sidecar_recommended,
// else embedded formats from capabilities() (XMP/EXIF/IPTC-IIM plus QuickTime
// when container_gps is writable); embedded_only requires a writable embedded
// category or container GPS; sidecar_only → Sidecar(XMP);
// sidecar_required → Sidecar(XMP) when embedded is unavailable or sidecar is
// recommended, else Mixed (embedded formats + XMP sidecar).
Result<StorageDecision> evaluateStorage(const std::filesystem::path& media,
                                        WriteOptions options = {});

// Read + reconcile (or use SyncOptions::metadata), then write the canonical
// state so the selected carriers agree. Both file mutations go through
// mutate_file_atomically. Unresolved Resolution::conflict on direction
// `both` fails with conflict_unresolved (merge first). If the first carrier
// write succeeds and the second fails, the error documents that partial
// state; the first file is not rolled back.
Result<SyncReport> synchronize(const std::filesystem::path& media,
                               SyncOptions options = {});

struct CastOptions {
  bool dry_run{true};
  bool force{false};
  bool include_approximate{false};
  std::vector<std::string> groups;  // empty = all groups for the direction
  CancelToken cancel;
  std::optional<std::chrono::steady_clock::time_point> deadline;
};

struct CastReport {
  std::vector<CastCandidate> candidates;
  Metadata metadata;
};

// Evaluate or apply casts (C9–C12a). dry_run default does not write.
Result<CastReport> cast(const std::filesystem::path& media, CastDirection direction,
                        CastOptions options = {});

// Property map (C12b): definition, representations, casts, and cross-media
// partner. Overload with a path fills values and cast statuses.
// Declared in umm/describe.hpp.

// GPS track import and matchTrack() are declared in umm/track.hpp
// (sessions 25–26). Matched positions write through umm::write.

}  // namespace umm
