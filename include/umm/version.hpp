// ============================================================================
// DESIGN DRAFT — NOT BUILT, NOT TESTED.
// Normative statement of API shape per docs/analysis decision M7.
// Promoted to a real header by docs/implementation/01-repo-skeleton.md.
// ============================================================================
#pragma once

#include <string_view>

namespace umm {

// Library implementation version (independent of supported standards versions;
// see concept.md §21 and registry provenance for standards versions).
std::string_view version() noexcept;

}  // namespace umm
