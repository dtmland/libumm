// Cooperative cancellation (decision F3). A default token never cancels.
// CancelToken::create() shares one flag; copies see the same cancel().
// Deadlines are per call, not stored in the token.
#pragma once

#include <atomic>
#include <memory>

namespace umm {

class CancelToken {
 public:
  CancelToken() = default;

  // Shared flag. cancel() is safe from any thread; cancelled() is a snapshot.
  static CancelToken create();

  void cancel() noexcept;
  bool cancelled() const noexcept;
  // False for a default token. True after create(), including copies.
  bool shared() const noexcept;

 private:
  struct State {
    std::atomic<bool> flag{false};
  };

  explicit CancelToken(std::shared_ptr<State> state);

  std::shared_ptr<State> state_;
};

}  // namespace umm
