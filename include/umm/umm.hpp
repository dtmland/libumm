// Application-facing entry points (concept.md §32).
// umm::read reconciles embedded metadata and, by default, a paired XMP
// sidecar (session 14). umm::write applies StoragePolicy (concept.md §12)
// using capability data for the sniffed type (session 16).
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "umm/backend.hpp"
#include "umm/capabilities.hpp"
#include "umm/metadata.hpp"
#include "umm/registry.hpp"
#include "umm/result.hpp"
#include "umm/track.hpp"
#include "umm/value.hpp"
#include "umm/version.hpp"

namespace umm {

// Embedded-vs-sidecar policy (concept.md §12).
enum class StoragePolicy {
  preferred,        // library decides per capabilities + policy engine
  embedded_only,
  sidecar_only,
  sidecar_required, // sidecar must exist/be written; embedded optional
};

struct StorageDecision {
  enum class Method { embedded, sidecar, mixed } method{Method::embedded};
  std::vector<std::string> formats;  // e.g. {"XMP", "EXIF", "IPTC-IIM", "QuickTime"}
  std::string backend;               // backend that performed/will perform the write
};

struct ReadOptions {
  std::string backend;          // empty = first available (manager order)
  bool merge_sidecar{true};     // pair media.xmp per session-14 pairing rules
  bool conflicts_as_errors{false};  // else recorded in Metadata::conflictedPropertyIds()
  // When true, a non-sidecar path with no paired .xmp fails
  // (ErrorCode::io_not_found). StoragePolicy::sidecar_required on write is
  // the matching persistence rule.
  bool sidecar_required{false};
};

struct WriteOptions {
  std::string backend;          // empty = capability-driven choice
  StoragePolicy policy{StoragePolicy::preferred};
  bool dry_run{false};          // compute WriteReport without touching files
};

struct WriteReport {
  StorageDecision decision;
  std::vector<UnmappedKey> written;  // every unmapped representation updated (write-sync)
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
  std::string backend;  // empty = first available (manager order)
  SyncDirection direction{SyncDirection::both};
  // When set, this canonical state is written (session 23 merge output).
  // When unset, synchronize() reads first according to `direction`.
  std::optional<Metadata> metadata;
  bool dry_run{false};
};

// One carrier actually targeted by synchronize() (or mixed umm::write).
struct SyncCarrierReport {
  std::string container;  // "embedded" | "sidecar"
  std::vector<UnmappedKey> written;
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

// --- Entry points ------------------------------------------------------------

// Read + reconcile (docs/reconciliation-policy.md) into canonical Metadata.
// Embedded metadata, plus sidecar XMP when merge_sidecar and a pair exists.
// A standalone .xmp file is readable as sidecar-only. sidecar_required fails
// when a non-sidecar path has no paired .xmp.
Result<Metadata> read(const std::filesystem::path& media, ReadOptions options = {});

// Enumerate disagreed properties without inspecting every field of a read.
// Same pipeline as read(); no second reconciliation engine. Entries cover
// Resolution::conflict and Resolution::reconciled (policy-resolved
// disagreement). conflicts_as_errors fails only on unresolved `conflict`.
Result<ConflictReport> detectConflict(const std::filesystem::path& media,
                                      ReadOptions options = {});

// Resolve a disagreed property by choosing a candidate listed in `entry`
// (match SourceRef::raw_key). When the same raw_key appears in more than one
// candidate (embedded vs sidecar XMP), pass `container` ("embedded" or
// "sidecar"). Resulting Metadata is `reconciled` with preferred_source set;
// every existing source is retained.
Result<Metadata> merge(Metadata metadata, const ConflictEntry& entry,
                       std::string_view source,
                       std::string_view container = {});

// User-supplied override. Resolution becomes `reconciled`; preferred_source
// is empty (not a raw key); sources are retained. Unlike set(), provenance
// is not discarded.
Result<Metadata> merge(Metadata metadata, std::string_view property_id,
                       Value value);

// Write canonical metadata through the mapping engine to synchronized
// representations, with temp-file + atomic-rename safety (decision M3).
// preferred/embedded_only: writable embedded categories and container GPS
// from capabilities() (QuickTime GPSCoordinates when container_gps is writable).
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

// GPS track import and matchTrack() are declared in umm/track.hpp
// (sessions 25–26). Matched positions write through umm::write.

}  // namespace umm
