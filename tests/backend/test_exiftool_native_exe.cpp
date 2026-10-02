#include "exiftool/exiftool_backend.hpp"
#include "umm/backend.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

}  // namespace

int main() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "umm-exiftool-native-exe";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    return fail("failed to create temp directory");
  }

  const std::filesystem::path exe = dir / "ExifTool.exe";
  const std::filesystem::path script = dir / "exiftool";
  std::filesystem::remove(exe, ec);
#if defined(_WIN32)
  // CreateProcessW of a non-PE .exe can block on a modal error dialog, so the
  // native-exe fixture must be a real image. hostname.exe exits immediately
  // even with a dummy `-ver` argument.
  const char* root = std::getenv("SystemRoot");
  const std::filesystem::path host =
      std::filesystem::path(root && *root ? root : "C:\\Windows") / "System32" /
      "hostname.exe";
  std::filesystem::copy_file(host, exe, ec);
  if (ec) {
    std::fprintf(stderr, "failed to copy hostname.exe: %s\n",
                 ec.message().c_str());
    return 1;
  }
#else
  {
    std::ofstream out(exe, std::ios::binary);
    if (!out) {
      return fail("failed to create fake ExifTool.exe");
    }
    out << "not a real windows executable\n";
  }
#endif
  {
    std::ofstream out(script, std::ios::binary);
    if (!out) {
      return fail("failed to create fake exiftool script");
    }
    out << "#!/usr/bin/env perl\n";
  }

  umm::ExifToolConfig exe_config;
  exe_config.exiftool_script = exe;
  exe_config.command_timeout = std::chrono::milliseconds{2000};
  umm::internal::ExifToolBackend exe_backend(exe_config);
  const umm::BackendAvailability exe_status = exe_backend.availability();
  if (exe_status.reason == "Perl interpreter not found") {
    return fail("Windows .exe packaging required Perl");
  }

  umm::ExifToolConfig script_config;
  script_config.exiftool_script = script;
  script_config.perl_interpreter =
      std::filesystem::path("umm-missing-perl-interpreter");
  script_config.command_timeout = std::chrono::milliseconds{2000};
  umm::internal::ExifToolBackend script_backend(script_config);
  const umm::BackendAvailability script_status = script_backend.availability();
  if (script_status.available) {
    return fail("Perl script packaging reported available without Perl");
  }
  if (script_status.reason != "Perl interpreter not found") {
    std::fprintf(stderr, "script absence reason: %s\n",
                 script_status.reason.c_str());
    return fail("Perl script packaging missing Perl reason");
  }

#ifndef _WIN32
  const std::filesystem::path stub = dir / "exiftool.exe";
  {
    std::ofstream out(stub, std::ios::binary);
    if (!out) {
      return fail("failed to create native stub");
    }
    out << "#!/bin/sh\n"
           "if [ \"$1\" = \"-ver\" ]; then\n"
           "  echo 13.40\n"
           "  exit 0\n"
           "fi\n"
           "exit 1\n";
  }
  std::filesystem::permissions(
      stub,
      std::filesystem::perms::owner_exec | std::filesystem::perms::owner_read,
      std::filesystem::perm_options::add, ec);
  if (ec) {
    return fail("failed to chmod native stub");
  }

  umm::ExifToolConfig stub_config;
  stub_config.exiftool_script = stub;
  stub_config.command_timeout = std::chrono::milliseconds{2000};
  umm::internal::ExifToolBackend stub_backend(stub_config);
  const umm::BackendAvailability stub_status = stub_backend.availability();
  if (!stub_status.available) {
    std::fprintf(stderr, "native stub unavailable: %s\n",
                 stub_status.reason.c_str());
    return 1;
  }
  if (stub_status.version != "13.40") {
    std::fprintf(stderr, "native stub version \"%s\"\n",
                 stub_status.version.c_str());
    return fail("native .exe version probe did not spawn the stub directly");
  }
#endif

  return 0;
}
