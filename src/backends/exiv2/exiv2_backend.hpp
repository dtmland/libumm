#pragma once

#include <memory>

#include "umm/backend.hpp"

namespace umm::internal {

std::unique_ptr<Backend> make_exiv2_backend();

}  // namespace umm::internal
