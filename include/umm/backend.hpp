// Backend adapter contract (decisions S1a/S1b/S1c). Promoted from design draft
// by docs/implementation/10-exiv2-backend-read.md.
//
//  - Backends are optional at RUNTIME. Absence is reported through
//    availability(), never a load failure. CI requires both.
//  - Exiv2 is in-process; ExifTool is an out-of-process adapter using
//    `-stay_open` batch mode with JSON output. Same contract for both.
//  - Backend types (Exiv2 classes, ExifTool JSON) never leak through this
//    interface; the neutral raw vocabulary below is the boundary.
//
// Thread-safety: each Backend instance is single-threaded. Callers must not
// share an instance across threads. BackendManager may pool instances later;
// this session's manager holds one instance per registered id.
//
// Timeout: Exiv2 is in-process and has no adapter-level timeout. ExifTool
// uses ExifToolConfig::command_timeout: on expiry the adapter kills the
// child and returns ErrorCode::backend_timeout; the next call respawns.
//
// ExifTool process (decision S1a): one `-stay_open True -@ -` child per
// adapter instance; commands are UTF-8 argfile lines on stdin ending with
// `-execute`; responses end at `{ready}`. Shutdown writes
// `-stay_open False` and waits. `-charset utf8` is passed before `-@`
// so Windows Unicode paths and values survive (stdin is the argfile).
//
// ExifTool key mapping: JSON `-j -G1` names (`Group1:Tag`) are translated
// into the Exiv2-syntax vocabulary. Phase 1 tags have an explicit table
// (IFD0:Artist -> Exif.Image.Artist, IPTC:By-line -> Iptc.Application2.Byline,
// XMP-dc:Creator -> Xmp.dc.creator, GPS:* -> Exif.GPSInfo.*, ...). Unmapped
// XMP-ns:Tag keys become Xmp.ns.Tag; anything else is kept as the
// adapter-specific key ExifTool.<Group1>.<Tag>. File/ExifTool/Composite
// groups are omitted (not stored metadata).
//
// Error mapping: missing/unreadable files -> io_*; unrecognized, truncated,
// or corrupt containers -> format_*; thrown backend diagnostics ->
// backend_failed. Exceptions never escape (decision M1).
#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "umm/metadata.hpp"  // RawKey, RawEntry
#include "umm/result.hpp"

namespace umm {

enum class BackendId {
  exiv2,
  exiftool,
};

inline std::string_view to_string(BackendId id) noexcept {
  switch (id) {
    case BackendId::exiv2:
      return "exiv2";
    case BackendId::exiftool:
      return "exiftool";
  }
  return {};
}

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
  // the backend writes to the temp path it is handed (a working copy).
  virtual Result<void> writeRaw(const std::filesystem::path& media,
                                const RawChanges& changes) = 0;

  // Per-type capability query. Declared here; implemented in session 15.
  // media_type is a container name such as "JPEG" or "XMP".
  virtual Result<void> typeCapabilities(std::string_view media_type) const = 0;
};

// ExifTool adapter configuration (decision S1c: locate, never bundle).
// Discovery order: non-empty paths here -> UMM_EXIFTOOL env var -> PATH.
// An explicit path that does not exist is absent (no env/PATH fallback).
// Perl: non-empty perl_interpreter here, else PATH (`perl` / `perl.exe`).
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
  Backend* get(std::string_view id);  // nullptr if unknown
  Backend* firstAvailable();          // nullptr if none (S1b)

 private:
  BackendManager();
  BackendManager(const BackendManager&) = delete;
  BackendManager& operator=(const BackendManager&) = delete;

  std::vector<std::unique_ptr<Backend>> backends_;
  ExifToolConfig exiftool_config_;
};

}  // namespace umm
