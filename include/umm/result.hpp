// Exception-free public error model (decision M1). Every fallible public
// operation returns Result<T>; backend exceptions never escape.
#pragma once

#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace umm {

enum class ErrorCode {
  // I/O
  io_not_found,
  io_read_failed,
  io_write_failed,
  // File/metadata format
  format_unrecognized,
  format_corrupt,
  // Backends (decision S1b: backends are optional at runtime)
  backend_unavailable,
  backend_failed,
  backend_timeout,
  // Capability (decision M2: capability-driven, never silent)
  unsupported_type,
  unsupported_capability,
  // Semantics
  conflict_unresolved,
  invalid_value,
  unknown_property,
  // Catch-all: a bug in libumm, never expected input
  internal,
};

struct Error {
  ErrorCode code{};
  std::string message;  // human-readable; stable enough for logs, not for parsing
  std::string backend;  // originating backend id, empty when not backend-specific
  std::string detail;   // raw backend/library diagnostic, e.g. Exiv2 or ExifTool text

  bool operator==(const Error&) const = default;
};

// Expected-style result. Implementation may become std::expected<T, Error>
// when toolchain baselines allow; the observable API stays as declared here.
template <typename T>
class [[nodiscard]] Result {
 public:
  /*implicit*/ Result(T value);
  /*implicit*/ Result(Error error);

  bool ok() const noexcept;
  explicit operator bool() const noexcept { return ok(); }

  // Preconditions: ok() for value(); !ok() for error().
  // Violations abort rather than throw (decision M1).
  const T& value() const&;
  T&& value() &&;
  const Error& error() const&;

 private:
  std::variant<T, Error> state_;
};

// void specialization: success carries no value.
template <>
class [[nodiscard]] Result<void> {
 public:
  Result() = default;  // success
  /*implicit*/ Result(Error error);

  bool ok() const noexcept;
  explicit operator bool() const noexcept { return ok(); }
  const Error& error() const&;

 private:
  std::optional<Error> error_;
};

template <typename T>
Result<T>::Result(T value) : state_(std::in_place_type<T>, std::move(value)) {}

template <typename T>
Result<T>::Result(Error error)
    : state_(std::in_place_type<Error>, std::move(error)) {}

template <typename T>
bool Result<T>::ok() const noexcept {
  return std::holds_alternative<T>(state_);
}

template <typename T>
const T& Result<T>::value() const& {
  if (const T* value = std::get_if<T>(&state_)) {
    return *value;
  }
  std::abort();
}

template <typename T>
T&& Result<T>::value() && {
  if (T* value = std::get_if<T>(&state_)) {
    return std::move(*value);
  }
  std::abort();
}

template <typename T>
const Error& Result<T>::error() const& {
  if (const Error* error = std::get_if<Error>(&state_)) {
    return *error;
  }
  std::abort();
}

inline Result<void>::Result(Error error) : error_(std::move(error)) {}

inline bool Result<void>::ok() const noexcept { return !error_.has_value(); }

inline const Error& Result<void>::error() const& {
  if (error_) {
    return *error_;
  }
  std::abort();
}

}  // namespace umm
