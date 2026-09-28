#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "umm/result.hpp"

namespace umm::internal {

bool is_xmp_sidecar_path(const std::filesystem::path& path);

std::string ascii_lower_ext(const std::filesystem::path& path);

// Minimal packet so backends can open a new sidecar working copy.
Result<void> write_xmp_stub(const std::filesystem::path& path);

}  // namespace umm::internal
