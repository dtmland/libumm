#include "core/call_context.hpp"

namespace umm::internal {
namespace {

thread_local CallState* t_call = nullptr;
std::atomic<const char*> g_cancel_checkpoint{nullptr};

}  // namespace

const CallState* current_call() noexcept { return t_call; }

CallGuard::CallGuard(
    const CancelToken& token,
    std::optional<std::chrono::steady_clock::time_point> deadline) {
  previous_ = t_call;
  // Default options must not hide an outer synchronize()/cast() token.
  if (previous_ && !token.shared() && !deadline) {
    return;
  }
  local_.token = token;
  local_.deadline = std::move(deadline);
  t_call = &local_;
  active_ = true;
}

CallGuard::~CallGuard() {
  if (active_) {
    t_call = previous_;
  }
}

Result<void> checkpoint(const char* name) {
  const char* armed = g_cancel_checkpoint.load(std::memory_order_acquire);
  if (armed && armed[0] != '\0' && name && std::strcmp(armed, name) == 0) {
    g_cancel_checkpoint.store(nullptr, std::memory_order_release);
    return Error{ErrorCode::cancelled, "cancelled", "", name};
  }
  if (const CallState* call = t_call) {
    if (call->token.cancelled()) {
      return Error{ErrorCode::cancelled, "cancelled", "", name ? name : ""};
    }
    if (call->deadline &&
        std::chrono::steady_clock::now() >= *call->deadline) {
      return Error{ErrorCode::deadline_exceeded, "deadline exceeded", "",
                   name ? name : ""};
    }
  }
  return {};
}

void set_cancel_at_checkpoint_for_test(const char* name) {
  g_cancel_checkpoint.store(name, std::memory_order_release);
}

}  // namespace umm::internal
