#include "exiftool/process.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

std::filesystem::path echo_exe() {
#if defined(_WIN32)
  if (const char* comspec = std::getenv("ComSpec"); comspec && *comspec) {
    return std::filesystem::path(comspec);
  }
  return std::filesystem::path("cmd.exe");
#else
  std::error_code ec;
  if (std::filesystem::is_regular_file("/bin/echo", ec)) {
    return std::filesystem::path("/bin/echo");
  }
  return std::filesystem::path("echo");
#endif
}

}  // namespace

int main() {
  using umm::internal::ChildProcess;
  using umm::internal::path_to_utf8;

  const std::filesystem::path exe = echo_exe();
  const std::string exe_utf8 = path_to_utf8(exe);
  const std::vector<std::string> argv =
#if defined(_WIN32)
      {exe_utf8, "/c", "echo", "umm-child-process-ok"};
#else
      {exe_utf8, "umm-child-process-ok"};
#endif

  ChildProcess child;
  const std::string spawn_err = child.spawn(exe, argv);
  if (!spawn_err.empty()) {
    std::fprintf(stderr, "spawn failed: %s\n", spawn_err.c_str());
    return 1;
  }

  std::string out;
  std::string err;
  std::string read_err;
  const auto status =
      child.read_all(std::chrono::milliseconds{10'000}, out, err, read_err);
  if (status != ChildProcess::Read::ok) {
    std::fprintf(stderr, "read_all failed (%d): %s\n",
                 static_cast<int>(status), read_err.c_str());
    return 1;
  }

  const auto end = out.find_first_of("\r\n");
  if (end != std::string::npos) {
    out.resize(end);
  }
  if (out != "umm-child-process-ok") {
    std::fprintf(stderr, "stdout \"%s\", stderr \"%s\"\n", out.c_str(),
                 err.c_str());
    return fail("one-shot child stdout did not match");
  }
  return 0;
}
