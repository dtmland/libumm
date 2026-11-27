#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "umm/backend.hpp"
#include "umm/result.hpp"
#include "umm/umm.hpp"

namespace umm::internal {

struct LoadedRead {
  std::string backend_id;
  BaseDocument embedded;
  BaseDocument sidecar_document;
  bool has_sidecar{false};
  std::string file_type;

  const BaseDocument* sidecar() const {
    return has_sidecar ? &sidecar_document : nullptr;
  }
};

// Shared backend selection + base read used by umm::read and detectConflict.
Result<LoadedRead> load_read(const std::filesystem::path& media,
                             const ReadOptions& options);

// In-memory base read used by umm::read(std::span<const std::byte>, ...).
Result<LoadedRead> load_read_bytes(std::span<const std::byte> bytes,
                                   std::string_view file_type_hint,
                                   const ReadOptions& options);

// Magic-byte type sniffer for in-memory reads. Returns the canonical file type
// (hint-disambiguated) or an empty string when the type cannot be determined.
std::string sniff_type_from_bytes(std::span<const std::byte> bytes,
                                  std::string_view type_hint);

}  // namespace umm::internal
