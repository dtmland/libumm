// Thread-local call context so adapters can see cancel/deadline without a
// public Backend signature change (decisions F3/F4).
#pragma once

#include <atomic>
#include <chrono>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

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

// Thread-local ExifTool tag hint for ReadOptions::properties (decision F7).
// nullptr means a full read. A non-null vector, including empty, is selective.
// Exiv2 ignores the hint. The pointed-to vector must outlive the guard.
class SelectiveTagGuard {
 public:
  explicit SelectiveTagGuard(const std::vector<std::string>* tags);
  ~SelectiveTagGuard();

  SelectiveTagGuard(const SelectiveTagGuard&) = delete;
  SelectiveTagGuard& operator=(const SelectiveTagGuard&) = delete;

 private:
  const std::vector<std::string>* previous_ = nullptr;
  bool active_ = false;
};

const std::vector<std::string>* current_selective_tags() noexcept;

}  // namespace umm::internal
