#pragma once

#include <filesystem>
#include <string>

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

}  // namespace umm::internal
