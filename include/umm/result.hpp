// ============================================================================
// DESIGN DRAFT — NOT BUILT, NOT TESTED.
// Normative statement of API shape per docs/analysis decision M7.
// Promoted to a real header by docs/implementation/08-core-semantic-model.md.
//
// Decision M1: no exceptions cross the public API boundary. Every fallible
// public operation returns Result<T>. This keeps a future stable C ABI and
// language bindings possible without redesign.
// ============================================================================
#pragma once

#include <string>
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
  ErrorCode code;
  std::string message;        // human-readable; stable enough for logs, not for parsing
  std::string backend;        // originating backend id, empty when not backend-specific
  std::string detail;         // raw backend/library diagnostic, e.g. Exiv2 or ExifTool text
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
  Result();  // success
  /*implicit*/ Result(Error error);
  bool ok() const noexcept;
  explicit operator bool() const noexcept { return ok(); }
  const Error& error() const&;
};

}  // namespace umm
