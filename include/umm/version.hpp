#pragma once

#include <string_view>

namespace umm {

// Library implementation version (independent of supported standards versions;
// see concept.md §21 and registry provenance for standards versions).
std::string_view version() noexcept;

}  // namespace umm
