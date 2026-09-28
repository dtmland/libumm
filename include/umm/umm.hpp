// Application-facing entry points (concept.md §32).
// umm::read is implemented in session 12 against docs/reconciliation-policy.md.
// umm::write / storage policy are declared here and implemented in sessions 13/14.
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
  std::vector<std::string> formats;  // e.g. {"XMP", "EXIF", "IPTC-IIM"}
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

// --- Entry points ------------------------------------------------------------

// Read + reconcile (docs/reconciliation-policy.md) into canonical Metadata.
Result<Metadata> read(const std::filesystem::path& media, ReadOptions options = {});

// Write canonical metadata through the mapping engine to synchronized
// representations, with temp-file + atomic-rename safety (decision M3).
Result<WriteReport> write(const std::filesystem::path& media,
                          const Metadata& metadata,
                          WriteOptions options = {});

}  // namespace umm
