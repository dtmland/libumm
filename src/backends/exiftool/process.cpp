#include "exiftool/process.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace umm::internal {
namespace {

using Clock = std::chrono::steady_clock;

int remaining_ms(Clock::time_point deadline) {
  const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
      deadline - Clock::now());
  if (left.count() <= 0) {
    return 0;
  }
  if (left.count() > 1'000'000) {
    return 1'000'000;
  }
  return static_cast<int>(left.count());
}

#if defined(_WIN32)

std::wstring utf8_to_wide(std::string_view text) {
  if (text.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                    static_cast<int>(text.size()), nullptr, 0);
  if (n <= 0) {
    return {};
  }
  std::wstring out(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      out.data(), n);
  return out;
}

std::wstring quote_win_arg(std::string_view arg) {
  const std::wstring wide = utf8_to_wide(arg);
  bool need_quote = wide.empty();
  for (wchar_t c : wide) {
    if (c == L' ' || c == L'\t' || c == L'"') {
      need_quote = true;
      break;
    }
  }
  if (!need_quote) {
    return wide;
  }
  std::wstring out;
  out.push_back(L'"');
  std::size_t slashes = 0;
  for (wchar_t c : wide) {
    if (c == L'\\') {
      ++slashes;
      continue;
    }
    if (c == L'"') {
      out.append(slashes * 2 + 1, L'\\');
      out.push_back(L'"');
      slashes = 0;
      continue;
    }
    if (slashes != 0) {
      out.append(slashes, L'\\');
      slashes = 0;
    }
    out.push_back(c);
  }
  out.append(slashes * 2, L'\\');
  out.push_back(L'"');
  return out;
}

std::wstring build_command_line(const std::vector<std::string>& argv) {
  std::wstring line;
  for (std::size_t i = 0; i < argv.size(); ++i) {
    if (i != 0) {
      line.push_back(L' ');
    }
    line += quote_win_arg(argv[i]);
  }
  return line;
}

void close_handle(HANDLE& handle) {
  if (handle && handle != INVALID_HANDLE_VALUE) {
    CloseHandle(handle);
  }
  handle = nullptr;
}

#endif

}  // namespace

std::string path_to_utf8(const std::filesystem::path& path) {
  const std::u8string utf8 = path.u8string();
  return {utf8.begin(), utf8.end()};
}

struct ChildProcess::Impl {
#if defined(_WIN32)
  HANDLE process{nullptr};
  HANDLE stdin_w{nullptr};
  HANDLE stdout_r{nullptr};
  HANDLE stderr_r{nullptr};
#else
  pid_t pid{-1};
  int stdin_fd{-1};
  int stdout_fd{-1};
  int stderr_fd{-1};
#endif

  std::string stdout_acc;
  std::string stderr_acc;

  ~Impl() { close_all(); }

  void close_all() {
#if defined(_WIN32)
    close_handle(stdin_w);
    close_handle(stdout_r);
    close_handle(stderr_r);
    close_handle(process);
#else
    auto closer = [](int& fd) {
      if (fd >= 0) {
        ::close(fd);
        fd = -1;
      }
    };
    closer(stdin_fd);
    closer(stdout_fd);
    closer(stderr_fd);
    if (pid > 0) {
      int status = 0;
      ::waitpid(pid, &status, WNOHANG);
      pid = -1;
    }
#endif
  }
};

ChildProcess::ChildProcess() : impl_(std::make_unique<Impl>()) {}

ChildProcess::ChildProcess(ChildProcess&&) noexcept = default;
ChildProcess& ChildProcess::operator=(ChildProcess&&) noexcept = default;

ChildProcess::~ChildProcess() {
  if (impl_ && running()) {
    kill();
    wait_for(std::chrono::milliseconds{500});
  }
}

#if !defined(_WIN32)
namespace {
void ignore_sigpipe_once() {
  static const int ignored = [] {
    struct sigaction action {};
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGPIPE, &action, nullptr);
    return 1;
  }();
  (void)ignored;
}
}  // namespace
#endif

std::string ChildProcess::spawn(const std::filesystem::path& exe,
                                const std::vector<std::string>& argv) {
  if (running()) {
    kill();
    wait_for(std::chrono::milliseconds{500});
  }
  impl_ = std::make_unique<Impl>();

#if defined(_WIN32)
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE stdin_r = nullptr;
  HANDLE stdin_w = nullptr;
  HANDLE stdout_r = nullptr;
  HANDLE stdout_w = nullptr;
  HANDLE stderr_r = nullptr;
  HANDLE stderr_w = nullptr;
  if (!CreatePipe(&stdin_r, &stdin_w, &sa, 0) ||
      !CreatePipe(&stdout_r, &stdout_w, &sa, 0) ||
      !CreatePipe(&stderr_r, &stderr_w, &sa, 0)) {
    close_handle(stdin_r);
    close_handle(stdin_w);
    close_handle(stdout_r);
    close_handle(stdout_w);
    close_handle(stderr_r);
    close_handle(stderr_w);
    return "CreatePipe failed";
  }
  SetHandleInformation(stdin_w, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(stdout_r, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(stderr_r, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  si.hStdInput = stdin_r;
  si.hStdOutput = stdout_w;
  si.hStdError = stderr_w;

  std::wstring command = build_command_line(argv);
  std::wstring exe_wide = utf8_to_wide(path_to_utf8(exe));
  PROCESS_INFORMATION pi{};
  const BOOL ok =
      CreateProcessW(exe_wide.empty() ? nullptr : exe_wide.c_str(),
                     command.empty() ? nullptr : command.data(), nullptr,
                     nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si,
                     &pi);
  close_handle(stdin_r);
  close_handle(stdout_w);
  close_handle(stderr_w);
  if (!ok) {
    close_handle(stdin_w);
    close_handle(stdout_r);
    close_handle(stderr_r);
    return "CreateProcessW failed";
  }
  CloseHandle(pi.hThread);
  impl_->process = pi.hProcess;
  impl_->stdin_w = stdin_w;
  impl_->stdout_r = stdout_r;
  impl_->stderr_r = stderr_r;
  return {};
#else
  ignore_sigpipe_once();
  int in_pipe[2]{-1, -1};
  int out_pipe[2]{-1, -1};
  int err_pipe[2]{-1, -1};
  if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
    if (in_pipe[0] >= 0) {
      ::close(in_pipe[0]);
    }
    if (in_pipe[1] >= 0) {
      ::close(in_pipe[1]);
    }
    if (out_pipe[0] >= 0) {
      ::close(out_pipe[0]);
    }
    if (out_pipe[1] >= 0) {
      ::close(out_pipe[1]);
    }
    if (err_pipe[0] >= 0) {
      ::close(err_pipe[0]);
    }
    if (err_pipe[1] >= 0) {
      ::close(err_pipe[1]);
    }
    return "pipe failed";
  }

  const pid_t pid = fork();
  if (pid < 0) {
    ::close(in_pipe[0]);
    ::close(in_pipe[1]);
    ::close(out_pipe[0]);
    ::close(out_pipe[1]);
    ::close(err_pipe[0]);
    ::close(err_pipe[1]);
    return "fork failed";
  }
  if (pid == 0) {
    ::dup2(in_pipe[0], STDIN_FILENO);
    ::dup2(out_pipe[1], STDOUT_FILENO);
    ::dup2(err_pipe[1], STDERR_FILENO);
    ::close(in_pipe[0]);
    ::close(in_pipe[1]);
    ::close(out_pipe[0]);
    ::close(out_pipe[1]);
    ::close(err_pipe[0]);
    ::close(err_pipe[1]);
    std::vector<char*> raw;
    raw.reserve(argv.size() + 1);
    for (const auto& arg : argv) {
      raw.push_back(const_cast<char*>(arg.c_str()));
    }
    raw.push_back(nullptr);
    const std::string exe_utf8 = path_to_utf8(exe);
    ::execv(exe_utf8.c_str(), raw.data());
    const char msg[] = "exec failed\n";
    if (::write(STDERR_FILENO, msg, sizeof(msg) - 1) < 0) {
      _exit(127);
    }
    _exit(127);
  }

  ::close(in_pipe[0]);
  ::close(out_pipe[1]);
  ::close(err_pipe[1]);
  impl_->pid = pid;
  impl_->stdin_fd = in_pipe[1];
  impl_->stdout_fd = out_pipe[0];
  impl_->stderr_fd = err_pipe[0];
  return {};
#endif
}

bool ChildProcess::running() {
  if (!impl_) {
    return false;
  }
#if defined(_WIN32)
  if (!impl_->process) {
    return false;
  }
  const DWORD wait = WaitForSingleObject(impl_->process, 0);
  return wait == WAIT_TIMEOUT;
#else
  if (impl_->pid <= 0) {
    return false;
  }
  int status = 0;
  const pid_t r = ::waitpid(impl_->pid, &status, WNOHANG);
  if (r == 0) {
    return true;
  }
  if (r == impl_->pid) {
    impl_->pid = -1;
  }
  return false;
#endif
}

void ChildProcess::kill() {
  if (!impl_) {
    return;
  }
#if defined(_WIN32)
  if (impl_->process) {
    TerminateProcess(impl_->process, 1);
  }
  close_handle(impl_->stdin_w);
#else
  if (impl_->pid > 0) {
    ::kill(impl_->pid, SIGKILL);
  }
  if (impl_->stdin_fd >= 0) {
    ::close(impl_->stdin_fd);
    impl_->stdin_fd = -1;
  }
#endif
}

bool ChildProcess::wait_for(std::chrono::milliseconds timeout) {
  if (!impl_) {
    return true;
  }
  const auto deadline = Clock::now() + timeout;
#if defined(_WIN32)
  if (!impl_->process) {
    return true;
  }
  const DWORD ms = static_cast<DWORD>(std::max(0, remaining_ms(deadline)));
  const DWORD wait = WaitForSingleObject(impl_->process, ms);
  if (wait == WAIT_OBJECT_0) {
    close_handle(impl_->process);
    return true;
  }
  return false;
#else
  if (impl_->pid <= 0) {
    return true;
  }
  while (Clock::now() < deadline) {
    int status = 0;
    const pid_t r = ::waitpid(impl_->pid, &status, WNOHANG);
    if (r == impl_->pid) {
      impl_->pid = -1;
      return true;
    }
    if (r < 0 && errno != EINTR) {
      impl_->pid = -1;
      return true;
    }
    ::usleep(5000);
  }
  return false;
#endif
}

std::string ChildProcess::write_all(std::string_view bytes) {
  if (!impl_) {
    return "process not started";
  }
  std::size_t off = 0;
  while (off < bytes.size()) {
#if defined(_WIN32)
    if (!impl_->stdin_w) {
      return "stdin closed";
    }
    DWORD n = 0;
    const DWORD chunk = static_cast<DWORD>(
        std::min<std::size_t>(bytes.size() - off, 32 * 1024));
    if (!WriteFile(impl_->stdin_w, bytes.data() + off, chunk, &n, nullptr)) {
      return "WriteFile failed";
    }
    if (n == 0) {
      return "WriteFile wrote zero bytes";
    }
    off += n;
#else
    if (impl_->stdin_fd < 0) {
      return "stdin closed";
    }
    const ssize_t n =
        ::write(impl_->stdin_fd, bytes.data() + off, bytes.size() - off);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return std::strerror(errno);
    }
    if (n == 0) {
      return "write returned zero";
    }
    off += static_cast<std::size_t>(n);
#endif
  }
  return {};
}

namespace {

#if defined(_WIN32)
bool is_pipe_exhausted(DWORD err) {
  return err == ERROR_BROKEN_PIPE || err == ERROR_HANDLE_EOF ||
         err == ERROR_NO_DATA;
}

// PeekNamedPipe can return ERROR_BROKEN_PIPE as soon as the child closes
// stdout, including while WaitForSingleObject still reports the process as
// running. Treat that as EOF and try ReadFile once for leftover bytes so a
// short-lived command (exiftool -ver) is not reported as empty/timeout.
bool read_available(HANDLE handle, std::string& acc) {
  if (!handle) {
    return false;
  }
  DWORD avail = 0;
  if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &avail, nullptr)) {
    const DWORD peek_err = GetLastError();
    if (!is_pipe_exhausted(peek_err)) {
      return false;
    }
    char buf[4096];
    DWORD n = 0;
    if (ReadFile(handle, buf, sizeof(buf), &n, nullptr) && n > 0) {
      acc.append(buf, n);
    }
    return true;
  }
  if (avail == 0) {
    return true;
  }
  std::string chunk(avail, '\0');
  DWORD n = 0;
  if (!ReadFile(handle, chunk.data(), avail, &n, nullptr)) {
    return is_pipe_exhausted(GetLastError());
  }
  acc.append(chunk.data(), n);
  return true;
}
#else
bool read_fd(int fd, std::string& acc, bool& eof) {
  if (fd < 0) {
    eof = true;
    return true;
  }
  char buf[4096];
  const ssize_t n = ::read(fd, buf, sizeof(buf));
  if (n < 0) {
    if (errno == EINTR) {
      return true;
    }
    return false;
  }
  if (n == 0) {
    eof = true;
    return true;
  }
  acc.append(buf, static_cast<std::size_t>(n));
  return true;
}
#endif

bool take_needle(std::string& acc, std::string_view needle, std::string& out) {
  const auto pos = acc.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  out = acc.substr(0, pos);
  acc.erase(0, pos + needle.size());
  return true;
}

}  // namespace

ChildProcess::Read ChildProcess::read_until(std::string_view needle,
                                            std::chrono::milliseconds timeout,
                                            std::string& out, std::string& err,
                                            std::string& error_message) {
  out.clear();
  err.clear();
  error_message.clear();
  if (!impl_) {
    error_message = "process not started";
    return Read::error;
  }
  if (take_needle(impl_->stdout_acc, needle, out)) {
    err = impl_->stderr_acc;
    return Read::ok;
  }
  const auto deadline = Clock::now() + timeout;
  bool stdout_eof = false;
  bool stderr_eof = false;
  while (Clock::now() < deadline) {
#if defined(_WIN32)
    if (!read_available(impl_->stdout_r, impl_->stdout_acc) ||
        !read_available(impl_->stderr_r, impl_->stderr_acc)) {
      const bool dead = !running();
      if (dead) {
        read_available(impl_->stdout_r, impl_->stdout_acc);
        read_available(impl_->stderr_r, impl_->stderr_acc);
        stdout_eof = true;
      } else {
        error_message = "pipe read failed";
        err = impl_->stderr_acc;
        return Read::error;
      }
    }
    if (take_needle(impl_->stdout_acc, needle, out)) {
      err = impl_->stderr_acc;
      return Read::ok;
    }
    if (!running() && impl_->stdout_acc.find(needle) == std::string::npos) {
      err = impl_->stderr_acc;
      out = impl_->stdout_acc;
      return Read::eof;
    }
    Sleep(5);
#else
    pollfd fds[2]{};
    fds[0].fd = impl_->stdout_fd;
    fds[0].events = POLLIN;
    fds[1].fd = impl_->stderr_fd;
    fds[1].events = POLLIN;
    const int rc = ::poll(fds, 2, remaining_ms(deadline));
    if (rc < 0) {
      if (errno == EINTR) {
        continue;
      }
      error_message = std::strerror(errno);
      err = impl_->stderr_acc;
      return Read::error;
    }
    if (rc == 0) {
      break;
    }
    if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
      if (!read_fd(impl_->stdout_fd, impl_->stdout_acc, stdout_eof)) {
        error_message = std::strerror(errno);
        err = impl_->stderr_acc;
        return Read::error;
      }
    }
    if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
      if (!read_fd(impl_->stderr_fd, impl_->stderr_acc, stderr_eof)) {
        error_message = std::strerror(errno);
        err = impl_->stderr_acc;
        return Read::error;
      }
    }
    if (take_needle(impl_->stdout_acc, needle, out)) {
      err = impl_->stderr_acc;
      return Read::ok;
    }
    if (stdout_eof) {
      err = impl_->stderr_acc;
      out = impl_->stdout_acc;
      return Read::eof;
    }
#endif
  }
  err = impl_->stderr_acc;
  out = impl_->stdout_acc;
  return Read::timeout;
}

ChildProcess::Read ChildProcess::read_all(std::chrono::milliseconds timeout,
                                          std::string& out, std::string& err,
                                          std::string& error_message) {
  out.clear();
  err.clear();
  error_message.clear();
  if (!impl_) {
    error_message = "process not started";
    return Read::error;
  }
  const auto deadline = Clock::now() + timeout;
#if defined(_WIN32)
  close_handle(impl_->stdin_w);
  int idle_after_exit = 0;
  while (Clock::now() < deadline) {
    const bool stdout_ok =
        read_available(impl_->stdout_r, impl_->stdout_acc);
    const bool stderr_ok =
        read_available(impl_->stderr_r, impl_->stderr_acc);
    const bool alive = running();
    if (!alive) {
      // Process exit and pipe EOF can be observed in either order. Keep
      // draining briefly so the last write is not lost.
      if (!impl_->stdout_acc.empty() || ++idle_after_exit >= 8) {
        break;
      }
    } else {
      idle_after_exit = 0;
      if (!stdout_ok || !stderr_ok) {
        Sleep(5);
        continue;
      }
    }
    Sleep(5);
  }
  read_available(impl_->stdout_r, impl_->stdout_acc);
  read_available(impl_->stderr_r, impl_->stderr_acc);
  if (!running() || idle_after_exit != 0) {
    out = impl_->stdout_acc;
    err = impl_->stderr_acc;
    impl_->stdout_acc.clear();
    return Read::ok;
  }
  return Read::timeout;
#else
  if (impl_->stdin_fd >= 0) {
    ::close(impl_->stdin_fd);
    impl_->stdin_fd = -1;
  }
  bool stdout_eof = false;
  bool stderr_eof = false;
  while (Clock::now() < deadline && !(stdout_eof && stderr_eof)) {
    pollfd fds[2]{};
    int nfd = 0;
    int stdout_i = -1;
    int stderr_i = -1;
    if (!stdout_eof && impl_->stdout_fd >= 0) {
      stdout_i = nfd;
      fds[nfd].fd = impl_->stdout_fd;
      fds[nfd].events = POLLIN;
      ++nfd;
    }
    if (!stderr_eof && impl_->stderr_fd >= 0) {
      stderr_i = nfd;
      fds[nfd].fd = impl_->stderr_fd;
      fds[nfd].events = POLLIN;
      ++nfd;
    }
    if (nfd == 0) {
      break;
    }
    const int rc = ::poll(fds, nfd, remaining_ms(deadline));
    if (rc < 0) {
      if (errno == EINTR) {
        continue;
      }
      error_message = std::strerror(errno);
      return Read::error;
    }
    if (rc == 0) {
      return Read::timeout;
    }
    if (stdout_i >= 0 &&
        (fds[stdout_i].revents & (POLLIN | POLLHUP | POLLERR))) {
      if (!read_fd(impl_->stdout_fd, impl_->stdout_acc, stdout_eof)) {
        error_message = std::strerror(errno);
        return Read::error;
      }
    }
    if (stderr_i >= 0 &&
        (fds[stderr_i].revents & (POLLIN | POLLHUP | POLLERR))) {
      if (!read_fd(impl_->stderr_fd, impl_->stderr_acc, stderr_eof)) {
        error_message = std::strerror(errno);
        return Read::error;
      }
    }
  }
  wait_for(std::chrono::milliseconds{remaining_ms(deadline)});
  out = impl_->stdout_acc;
  err = impl_->stderr_acc;
  impl_->stdout_acc.clear();
  return stdout_eof ? Read::ok : Read::timeout;
#endif
}

}  // namespace umm::internal
