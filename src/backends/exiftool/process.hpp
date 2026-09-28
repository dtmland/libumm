#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace umm::internal {

class ChildProcess {
 public:
  ChildProcess();
  ChildProcess(const ChildProcess&) = delete;
  ChildProcess& operator=(const ChildProcess&) = delete;
  ChildProcess(ChildProcess&&) noexcept;
  ChildProcess& operator=(ChildProcess&&) noexcept;
  ~ChildProcess();

  // argv[0] should be the program name (usually the same as exe).
  std::string spawn(const std::filesystem::path& exe,
                    const std::vector<std::string>& argv);

  bool running();
  void kill();
  bool wait_for(std::chrono::milliseconds timeout);

  std::string write_all(std::string_view bytes);

  enum class Read { ok, timeout, eof, error };
  Read read_until(std::string_view needle, std::chrono::milliseconds timeout,
                  std::string& out, std::string& err,
                  std::string& error_message);
  Read read_all(std::chrono::milliseconds timeout, std::string& out,
                std::string& err, std::string& error_message);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

std::string path_to_utf8(const std::filesystem::path& path);

}  // namespace umm::internal
