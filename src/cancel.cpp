#include "umm/cancel.hpp"

namespace umm {

CancelToken::CancelToken(std::shared_ptr<State> state) : state_(std::move(state)) {}

CancelToken CancelToken::create() {
  return CancelToken(std::make_shared<State>());
}

void CancelToken::cancel() noexcept {
  if (state_) {
    state_->flag.store(true, std::memory_order_release);
  }
}

bool CancelToken::cancelled() const noexcept {
  return state_ && state_->flag.load(std::memory_order_acquire);
}

bool CancelToken::shared() const noexcept { return static_cast<bool>(state_); }

}  // namespace umm
