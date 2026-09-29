// Application-facing entry points (concept.md §32).
// umm::read reconciles embedded metadata and, by default, a paired XMP
// sidecar (session 14). umm::write applies StoragePolicy (concept.md §12)
// using capability data for the sniffed type (session 16).
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "umm/backend.hpp"
#include "umm/capabilities.hpp"
#include "umm/metadata.hpp"
#include "umm/registry.hpp"
#include "umm/result.hpp"
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
};

struct WriteOptions {
  std::string backend;          // empty = capability-driven choice
  StoragePolicy policy{StoragePolicy::preferred};
  bool dry_run{false};          // compute WriteReport without touching files
};

struct WriteReport {
  StorageDecision decision;
  std::vector<RawKey> written;  // every raw representation updated (write-sync)
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
// A standalone .xmp file is readable as sidecar-only.
Result<Metadata> read(const std::filesystem::path& media, ReadOptions options = {});

// Write canonical metadata through the mapping engine to synchronized
// representations, with temp-file + atomic-rename safety (decision M3).
// preferred/embedded_only: writable embedded categories and container GPS
// from capabilities() (QuickTime GPSCoordinates when container_gps is writable).
// sidecar_only/sidecar_required: XMP sidecar (media bytes unchanged).
// Types with sidecar_recommended prefer sidecar writes. Mixed sync is Stage 8.
Result<WriteReport> write(const std::filesystem::path& media,
                          const Metadata& metadata,
                          WriteOptions options = {});

Result<WriteReport> write(const std::filesystem::path& media,
                          const Metadata& metadata, StoragePolicy policy);

// preferred: sidecar when the path is an XMP sidecar or sidecar_recommended,
// else embedded formats from capabilities() (XMP/EXIF/IPTC-IIM plus QuickTime
// when container_gps is writable); embedded_only requires a writable embedded
// category or container GPS; sidecar_only/sidecar_required → Sidecar(XMP).
// Mixed sync is Stage 8.
Result<StorageDecision> evaluateStorage(const std::filesystem::path& media,
                                        WriteOptions options = {});

}  // namespace umm
