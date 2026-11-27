// Backend adapter contract (decisions S1a/S1b/S1c). Promoted from design draft
// by docs/implementation/10-exiv2-backend-read.md.
//
//  - Backends are optional at RUNTIME. Absence is reported through
//    availability(), never a load failure. CI requires both.
//  - Exiv2 is in-process; ExifTool is an out-of-process adapter using
//    `-stay_open` batch mode with JSON output. Same contract for both.
//  - Backend types (Exiv2 classes, ExifTool JSON) never leak through this
//    interface; the neutral base-metadata vocabulary below is the boundary.
//
// Thread-safety (decision F1): BackendManager methods are safe to call
// concurrently. get() returns a process-lifetime instance; callers may use
// that pointer from multiple threads. Exiv2 metadata calls are serialized
// (the XMP toolkit is process-global). ExifTool calls lease a child from a
// pool of at most ExifToolConfig::max_processes. Do not destroy a backend
// while a call on it is in flight; the manager's instances live until exit.
//
// Timeout: Exiv2 is in-process and has no adapter-level timeout. ExifTool
// uses ExifToolConfig::command_timeout: on expiry the adapter kills that
// child and returns ErrorCode::backend_timeout; the next call respawns.
// A CancelToken or deadline on the calling read/write/sync/cast is checked
// at least every 50 ms during an ExifTool wait and uses the same kill path,
// returning cancelled or deadline_exceeded.
//
// ExifTool process (decisions S1a, F2): each pooled child is
// `-stay_open True -@ -`. Commands are UTF-8 argfile lines on stdin ending
// with `-execute`; responses end at `{ready}`. Idle children shut down with
// `-stay_open False`. configureExifTool applies to calls that start after it
// returns; in-flight calls finish on the process they already leased, and
// that process is reaped when the lease ends. `-charset utf8` is passed
// before `-@` so Windows Unicode paths and values survive (stdin is the
// argfile).
//
// ExifTool key mapping: JSON `-j -G1` names (`Group1:Tag`) are translated
// into the Exiv2-syntax vocabulary. Phase 1 tags have an explicit table
// (IFD0:Artist -> Exif.Image.Artist, IPTC:By-line -> Iptc.Application2.Byline,
// XMP-dc:Creator -> Xmp.dc.creator, GPS:* -> Exif.GPSInfo.*, mapped
// QuickTime/Keys/ItemList/UserData tags -> QuickTime.<Tag>, ...). Other
// XMP-ns:Tag keys become Xmp.ns.Tag; anything else is kept as the
// adapter-specific key ExifTool.<Group1>.<Tag>. File/ExifTool groups are
// omitted (not stored metadata), except File:ImageWidth and File:ImageHeight,
// which become BaseDocument::intrinsics PixelWidth/PixelHeight (F11). Composite
// is omitted except GPS (when the GPS IFD copy is dropped) and Rotation
// (video display matrix; F10).
//
// Error mapping: missing/unreadable files -> io_*; unrecognized, truncated,
// or corrupt containers -> format_*; thrown backend diagnostics ->
// backend_failed. Exceptions never escape (decision M1).
#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "umm/metadata.hpp"  // BaseKey, BaseEntry
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

// Base document: what a backend read, before mapping/reconciliation.
// Key naming follows Exiv2 key syntax ("Exif.Image.Artist",
// "Iptc.Application2.City", "Xmp.dc.creator") as the neutral vocabulary;
// the ExifTool adapter translates its Group1:Tag names into it.
struct BaseDocument {
  std::vector<BaseEntry> entries;  // source order preserved
  // Container header facts that are not stored metadata (F11), such as pixel
  // dimensions. Not part of dumpAll().
  std::vector<BaseEntry> intrinsics;
};

// Changes expressed in base vocabulary, produced by the write-sync layer from
// the reconciliation policy (docs/reconciliation-policy.md).
struct BaseChanges {
  std::vector<BaseEntry> upserts;
  std::vector<BaseKey> removals;
};

class Backend {
 public:
  virtual ~Backend() = default;

  virtual std::string id() const = 0;  // "exiv2" | "exiftool"
  virtual BackendAvailability availability() const = 0;

  // Read every base entry the backend can see. Errors map to umm::Error;
  // backend exceptions/diagnostics never escape (decision M1).
  virtual Result<BaseDocument> readBase(const std::filesystem::path& media) = 0;

  // Read every base entry from an in-memory buffer (session 75, decision U5).
  // `file_type` is the container the core sniffed or the caller declared (e.g.
  // "JPEG", "XMP"); there is no sidecar pairing. Exiv2 reads through MemIo;
  // ExifTool writes a private, owner-only temp file and removes it afterwards,
  // including on error. The default is unsupported for backends that only read
  // paths. Backend exceptions never escape (decision M1).
  virtual Result<BaseDocument> readBase(std::span<const std::byte> bytes,
                                        std::string_view file_type) {
    (void)bytes;
    (void)file_type;
    return Error{ErrorCode::unsupported_capability,
                 "backend does not support reading from bytes", id(), ""};
  }

  // Multi-file read (decision F8). Default loops readBase. ExifTool overrides
  // this and sends up to kExifToolReadBatch paths in one -execute. A returned
  // error (not a vector) means the whole call stopped: cancelled or
  // deadline_exceeded. Per-file failures are elements of the vector, in
  // input order, same length as paths.
  virtual Result<std::vector<Result<BaseDocument>>> readBaseMany(
      std::span<const std::filesystem::path> paths) {
    std::vector<Result<BaseDocument>> documents;
    documents.reserve(paths.size());
    for (const std::filesystem::path& path : paths) {
      documents.push_back(readBase(path));
    }
    return documents;
  }

  // Write via temp-file + atomic rename, owned by core (decision M3.3);
  // the backend writes to the temp path it is handed (a working copy).
  virtual Result<void> writeBase(const std::filesystem::path& media,
                                 const BaseChanges& changes) = 0;

  // Per-type capability query from registry/capabilities/ (session 15).
  // media_type is a container name such as "JPEG" or "XMP".
  virtual Result<void> typeCapabilities(std::string_view media_type) const = 0;
};

// Paths per ExifTool -execute inside readMany (session 62). Stay_open
// round-trips dominate below this; larger chunks grow the JSON payload
// without a matching gain on the Tier A fixtures.
inline constexpr std::size_t kExifToolReadBatch = 32;

// ExifTool adapter configuration (decision S1c: locate, never bundle).
// Discovery order: non-empty paths here -> UMM_EXIFTOOL env var -> PATH.
// An explicit path that does not exist is absent (no env/PATH fallback).
// Windows `.exe` packaging (`exiftool.exe` / `ExifTool.exe`, including the
// official Oliver Betz / winget build) is spawned directly; Perl is not
// required. Perl-script packaging (`exiftool` without `.exe`, or `.pl`)
// still runs as `perl script …`: non-empty perl_interpreter here, else
// PATH (`perl` / `perl.exe`). End-user acquisition is
// tools/get-exiftool/ (session 33); those scripts write only to the
// install prefix and cache dir (never PATH/profiles).
struct ExifToolConfig {
  std::filesystem::path exiftool_script;  // empty = discover
  std::filesystem::path perl_interpreter; // empty = discover (script packaging)
  std::chrono::milliseconds command_timeout{30'000};  // kill + restart after
  // Parallel -stay_open children. Values below 1 are treated as 1.
  // Exiv2 stays serialized regardless of this field (decision F2).
  int max_processes{1};
};

class BackendManager {
 public:
  static BackendManager& instance();

  // Registration order defines default read preference.
  void configureExifTool(ExifToolConfig config);
  std::vector<std::string> backendIds() const;
  Backend* get(std::string_view id);              // nullptr if unknown
  const Backend* get(std::string_view id) const;  // nullptr if unknown
  Backend* firstAvailable();                      // nullptr if none (S1b)

 private:
  BackendManager();
  BackendManager(const BackendManager&) = delete;
  BackendManager& operator=(const BackendManager&) = delete;

  std::vector<std::unique_ptr<Backend>> backends_;
  ExifToolConfig exiftool_config_;
  mutable std::mutex mu_;
};

}  // namespace umm
