#pragma once

#include <filesystem>
#include <functional>

#include "umm/result.hpp"

namespace umm::internal {

// Test-only fault injection for decision M3.3. Production code leaves this
// at none. The original destination is never modified when a fault fires.
enum class AtomicWriteFault {
  none,
  before_write,   // after the working copy exists, before mutating it
  before_rename,  // after a successful mutate, before replacing the original
};

void set_atomic_write_fault_for_test(AtomicWriteFault fault);

// Succeed this many mutate_file_atomically calls before honoring the
// injected fault (session 24 second-carrier failure).
void set_atomic_write_fault_skip_for_test(int succeed_before_fault);

// Copy `destination` to a unique temp file in the same directory, run
// `mutate` on that copy, then atomically replace the original
// (POSIX rename / ReplaceFileW). On any failure the original is left
// byte-identical and the temp file is removed.
Result<void> mutate_file_atomically(
    const std::filesystem::path& destination,
    const std::function<Result<void>(const std::filesystem::path& working_copy)>&
        mutate,
    bool create_if_missing = false);

}  // namespace umm::internal
