#include "core/atomic_write.hpp"

#include <atomic>
#include <cstdint>
#include <fstream>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace umm::internal {
namespace {

std::atomic<std::uint64_t> g_temp_counter{0};
std::atomic<int> g_fault{
    static_cast<int>(AtomicWriteFault::none)};
std::atomic<int> g_fault_skip{0};

Error io_error(ErrorCode code, std::string message, std::string detail) {
  return Error{code, std::move(message), "", std::move(detail)};
}

std::string path_utf8(const std::filesystem::path& path) {
  const std::u8string utf8 = path.u8string();
  return {utf8.begin(), utf8.end()};
}

bool remove_quietly(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
  return !ec;
}

Error with_temp_cleanup(Error error, const std::filesystem::path& temp) {
  if (!remove_quietly(temp)) {
    if (!error.detail.empty()) {
      error.detail += "; ";
    }
    error.detail += "failed to remove temp file " + path_utf8(temp);
  }
  return error;
}

std::filesystem::path make_temp_path(const std::filesystem::path& destination) {
  std::error_code ec;
  std::filesystem::path temp;
  for (int attempt = 0; attempt < 1024; ++attempt) {
    const std::uint64_t n = g_temp_counter.fetch_add(1) + 1;
    temp = destination.parent_path();
    temp /= destination.stem();
    temp += ".umm-";
    temp += std::to_string(n);
    temp += destination.extension();
    if (!std::filesystem::exists(temp, ec)) {
      return temp;
    }
  }
  return temp;
}

Result<void> copy_file_bytes(const std::filesystem::path& from,
                             const std::filesystem::path& to) {
  std::ifstream in(from, std::ios::binary);
  if (!in) {
    return io_error(ErrorCode::io_read_failed, "failed to read original file",
                    path_utf8(from));
  }
  std::ofstream out(to, std::ios::binary | std::ios::trunc);
  if (!out) {
    return io_error(ErrorCode::io_write_failed, "failed to create temp file",
                    path_utf8(to));
  }
  out << in.rdbuf();
  if (!in || !out) {
    return io_error(ErrorCode::io_write_failed, "failed to copy to temp file",
                    path_utf8(to));
  }
  out.close();
  if (!out) {
    return io_error(ErrorCode::io_write_failed, "failed to flush temp file",
                    path_utf8(to));
  }
  return {};
}

Result<void> replace_file(const std::filesystem::path& destination,
                          const std::filesystem::path& temp) {
#if defined(_WIN32)
  const std::wstring dest_w = destination.wstring();
  const std::wstring temp_w = temp.wstring();
  std::error_code exists_ec;
  if (std::filesystem::exists(destination, exists_ec)) {
    if (ReplaceFileW(dest_w.c_str(), temp_w.c_str(), nullptr,
                     REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
      return {};
    }
    if (MoveFileExW(temp_w.c_str(), dest_w.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      return {};
    }
    return io_error(ErrorCode::io_write_failed, "atomic replace failed",
                    path_utf8(destination));
  }
  if (MoveFileExW(temp_w.c_str(), dest_w.c_str(), MOVEFILE_WRITE_THROUGH)) {
    return {};
  }
  return io_error(ErrorCode::io_write_failed, "atomic replace failed",
                  path_utf8(destination));
#else
  std::error_code ec;
  std::filesystem::rename(temp, destination, ec);
  if (ec) {
    return io_error(ErrorCode::io_write_failed, "atomic rename failed",
                    ec.message());
  }
  return {};
#endif
}

}  // namespace

void set_atomic_write_fault_for_test(AtomicWriteFault fault) {
  g_fault.store(static_cast<int>(fault));
  g_fault_skip.store(0);
}

void set_atomic_write_fault_skip_for_test(int succeed_before_fault) {
  g_fault_skip.store(succeed_before_fault);
}

Result<void> mutate_file_atomically(
    const std::filesystem::path& destination,
    const std::function<Result<void>(const std::filesystem::path& working_copy)>&
        mutate,
    bool create_if_missing) {
  if (destination.empty()) {
    return io_error(ErrorCode::io_not_found, "media file not found", "");
  }
  std::error_code ec;
  const bool exists = std::filesystem::exists(destination, ec);
  if (!exists && !create_if_missing) {
    return io_error(ErrorCode::io_not_found, "media file not found",
                    path_utf8(destination));
  }
  if (exists && !std::filesystem::is_regular_file(destination, ec)) {
    return io_error(ErrorCode::io_write_failed, "media path is not a file",
                    path_utf8(destination));
  }
  if (!mutate) {
    return io_error(ErrorCode::internal, "atomic write missing mutator", "");
  }

  const std::filesystem::path temp = make_temp_path(destination);
  if (exists) {
    Result<void> copied = copy_file_bytes(destination, temp);
    if (!copied.ok()) {
      return with_temp_cleanup(copied.error(), temp);
    }
  } else {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) {
      return io_error(ErrorCode::io_write_failed, "failed to create temp file",
                      path_utf8(temp));
    }
  }

  const auto fault =
      static_cast<AtomicWriteFault>(g_fault.load());
  bool inject = false;
  if (fault != AtomicWriteFault::none) {
    int skip = g_fault_skip.load();
    if (skip > 0) {
      g_fault_skip.store(skip - 1);
    } else {
      inject = true;
    }
  }

  if (inject && fault == AtomicWriteFault::before_write) {
    return with_temp_cleanup(
        io_error(ErrorCode::io_write_failed, "injected write failure",
                 "before_write"),
        temp);
  }

  Result<void> mutated = mutate(temp);
  if (!mutated.ok()) {
    return with_temp_cleanup(mutated.error(), temp);
  }

  if (inject && fault == AtomicWriteFault::before_rename) {
    return with_temp_cleanup(
        io_error(ErrorCode::io_write_failed, "injected write failure",
                 "before_rename"),
        temp);
  }

  Result<void> replaced = replace_file(destination, temp);
  if (!replaced.ok()) {
    return with_temp_cleanup(replaced.error(), temp);
  }
  return {};
}

}  // namespace umm::internal
