// Payload digest of media essence (decision F15, U14). A stable hash of the
// pixels/samples that ignores metadata edits, for duplicate detection.
// Backend-free and thread-safe: no backend process or metadata lock.
#pragma once

#include <filesystem>
#include <string>

#include "umm/result.hpp"

namespace umm {

// Result of payloadDigest(). `scheme` identifies both the hash algorithm and
// the byte-range definition, so a definition change bumps the string and old
// digests stay comparable only within a scheme. When libumm hashes the same
// bytes as ExifTool's ImageDataHash the scheme is
// "exiftool-imagedatahash-sha256".
struct PayloadDigest {
  std::string scheme;
  std::string hex;  // lowercase hex-encoded SHA-256
  bool operator==(const PayloadDigest&) const = default;
};

// Digest the media essence of `path`. Supported containers: JPEG, PNG, WebP,
// and MP4/MOV (ISO base media, 64-bit box sizes). Thumbnails and previews are
// excluded, matching ExifTool. Other types return
// ErrorCode::unsupported_capability. A truncated or malformed container returns
// ErrorCode::format_corrupt; a missing or unreadable file returns an io_* code.
Result<PayloadDigest> payloadDigest(const std::filesystem::path& path);

}  // namespace umm
