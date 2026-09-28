#include "umm/result.hpp"

#include <cstdio>
#include <string>
#include <utility>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

umm::Error sample_error() {
  return umm::Error{umm::ErrorCode::io_not_found, "missing file", "exiv2",
                    "ENOENT"};
}

}  // namespace

int main() {
  const umm::Result<int> ok = 7;
  if (!ok.ok()) {
    return fail("Result<int> from value is not ok");
  }
  if (!ok) {
    return fail("Result<int> bool conversion is false");
  }
  if (ok.value() != 7) {
    return fail("Result<int> value");
  }

  const umm::Result<int> err = sample_error();
  if (err.ok() || err) {
    return fail("Result<int> from Error is ok");
  }
  if (err.error().code != umm::ErrorCode::io_not_found) {
    return fail("Result<int> error code");
  }
  if (err.error().message != "missing file") {
    return fail("Result<int> error message");
  }
  if (err.error().backend != "exiv2") {
    return fail("Result<int> error backend");
  }
  if (err.error().detail != "ENOENT") {
    return fail("Result<int> error detail");
  }

  umm::Result<std::string> moved = std::string("hello");
  std::string taken = std::move(moved).value();
  if (taken != "hello") {
    return fail("Result<string> move value");
  }

  const umm::Result<void> void_ok;
  if (!void_ok.ok() || !void_ok) {
    return fail("Result<void> default is not ok");
  }

  const umm::Result<void> void_err = sample_error();
  if (void_err.ok() || void_err) {
    return fail("Result<void> from Error is ok");
  }
  if (void_err.error().code != umm::ErrorCode::io_not_found) {
    return fail("Result<void> error code");
  }

  return 0;
}
