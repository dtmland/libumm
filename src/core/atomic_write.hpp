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
  // Durable path only (decision F14). Ignored when durable is false.
  before_file_sync,   // temp-file flush fails; original unchanged
  after_rename_sync,  // parent/destination flush fails after a successful rename
};

void set_atomic_write_fault_for_test(AtomicWriteFault fault);

// Succeed this many mutate_file_atomically calls before honoring the
// injected fault (session 24 second-carrier failure).
void set_atomic_write_fault_skip_for_test(int succeed_before_fault);

// Copy `destination` to a unique temp file in the same directory, run
// `mutate` on that copy, then atomically replace the original
// (POSIX rename / ReplaceFileW). On any failure before the rename the
// original is left byte-identical and the temp file is removed.
// `durable` flushes the temp file before the rename and the parent
// directory (POSIX) or destination handle (Windows) after it.
Result<void> mutate_file_atomically(
    const std::filesystem::path& destination,
    const std::function<Result<void>(const std::filesystem::path& working_copy)>&
        mutate,
    bool create_if_missing = false,
    bool durable = false);

}  // namespace umm::internal
