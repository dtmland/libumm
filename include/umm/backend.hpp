// ============================================================================
// DESIGN DRAFT — NOT BUILT, NOT TESTED.
// Normative statement of API shape per docs/analysis decision M7.
// Promoted to a real header by docs/implementation/10-exiv2-backend-read.md.
//
// The backend adapter contract (decisions S1a/S1b/S1c):
//  - Backends are optional at RUNTIME. Absence is reported through
//    availability(), never a load failure. CI requires both.
//  - Exiv2 is in-process; ExifTool is an out-of-process adapter using
//    `-stay_open` batch mode with JSON output. Same contract for both.
//  - Backend types (Exiv2 classes, ExifTool JSON) never leak through this
//    interface; the neutral raw vocabulary below is the boundary.
// ============================================================================
#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "umm/metadata.hpp"  // RawKey, RawEntry
#include "umm/result.hpp"

namespace umm {

struct BackendAvailability {
  bool available{false};
  std::string version;  // pinned/discovered backend version when available
  std::string reason;   // human-readable absence reason when unavailable
};

// Raw document: what a backend read, before mapping/reconciliation.
// Key naming follows Exiv2 key syntax ("Exif.Image.Artist",
// "Iptc.Application2.City", "Xmp.dc.creator") as the neutral vocabulary;
// the ExifTool adapter translates its Group1:Tag names into it.
struct RawDocument {
  std::vector<RawEntry> entries;  // source order preserved
};

// Changes expressed in raw vocabulary, produced by the write-sync layer from
// the reconciliation policy (docs/reconciliation-policy.md).
struct RawChanges {
  std::vector<RawEntry> upserts;
  std::vector<RawKey> removals;
};

class Backend {
 public:
  virtual ~Backend() = default;

  virtual std::string id() const = 0;  // "exiv2" | "exiftool"
  virtual BackendAvailability availability() const = 0;

  // Read every raw entry the backend can see. Errors map to umm::Error;
  // backend exceptions/diagnostics never escape (decision M1).
  virtual Result<RawDocument> readRaw(const std::filesystem::path& media) = 0;

  // Write via temp-file + atomic rename, owned by core (decision M3.3);
  // the backend writes to the temp path it is handed.
  virtual Result<void> writeRaw(const std::filesystem::path& media,
                                const RawChanges& changes) = 0;
};

// ExifTool adapter configuration (decision S1c: locate, never bundle).
// Discovery order: explicit paths here -> UMM_EXIFTOOL env var -> PATH.
struct ExifToolConfig {
  std::filesystem::path exiftool_script;  // empty = discover
  std::filesystem::path perl_interpreter; // empty = discover
  std::chrono::milliseconds command_timeout{30'000};  // kill + restart after
};

class BackendManager {
 public:
  static BackendManager& instance();

  // Registration order defines default read preference.
  void configureExifTool(ExifToolConfig config);
  std::vector<std::string> backendIds() const;
  Backend* get(std::string_view id);          // nullptr if unknown
  Backend* firstAvailable();                  // nullptr if none (S1b)
};

}  // namespace umm
