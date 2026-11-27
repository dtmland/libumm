// Thread-local call context so adapters can see cancel/deadline without a
// public Backend signature change (decisions F3/F4).
#pragma once

#include <atomic>
#include <chrono>
#include <cstring>
#include <optional>
#include <string>

#include "umm/cancel.hpp"
#include "umm/result.hpp"

namespace umm::internal {

struct CallState {
  CancelToken token;
  std::optional<std::chrono::steady_clock::time_point> deadline;
};

const CallState* current_call() noexcept;

// Installs this call's token and deadline. A default token with no deadline
// inherits an outer call so synchronize()/cast() stay cancellable.
class CallGuard {
 public:
  CallGuard(const CancelToken& token,
            std::optional<std::chrono::steady_clock::time_point> deadline);
  ~CallGuard();

  CallGuard(const CallGuard&) = delete;
  CallGuard& operator=(const CallGuard&) = delete;

 private:
  CallState local_;
  CallState* previous_ = nullptr;
  bool active_ = false;
};

// Named checkpoints. A test may arm one name; the first match returns
// cancelled and disarms. Public tokens and deadlines are checked every time.
Result<void> checkpoint(const char* name);

void set_cancel_at_checkpoint_for_test(const char* name);

}  // namespace umm::internal
