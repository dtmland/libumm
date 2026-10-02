#include "exiftool/exiftool_backend.hpp"
#include "umm/backend.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <stdlib.h>
#endif

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

void set_path(const char* value) {
#if defined(_WIN32)
  _putenv_s("PATH", value ? value : "");
#else
  if (value == nullptr) {
    unsetenv("PATH");
  } else {
    setenv("PATH", value, 1);
  }
#endif
}

std::string current_path() {
  const char* path = std::getenv("PATH");
  return path ? std::string(path) : std::string();
}

}  // namespace

int main() {
  const std::string saved_path = current_path();
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "umm-exiftool-native-exe";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    return fail("failed to create temp directory");
  }

  const std::filesystem::path exe = dir / "ExifTool.exe";
  const std::filesystem::path script = dir / "exiftool";
  {
    std::ofstream out(exe, std::ios::binary);
    if (!out) {
      return fail("failed to create fake ExifTool.exe");
    }
    out << "not a real windows executable\n";
  }
  {
    std::ofstream out(script, std::ios::binary);
    if (!out) {
      return fail("failed to create fake exiftool script");
    }
    out << "#!/usr/bin/env perl\n";
  }

  set_path("");

  umm::ExifToolConfig exe_config;
  exe_config.exiftool_script = exe;
  umm::internal::ExifToolBackend exe_backend(exe_config);
  const umm::BackendAvailability exe_status = exe_backend.availability();
  if (exe_status.reason == "Perl interpreter not found") {
    set_path(saved_path.c_str());
    return fail("Windows .exe packaging required Perl");
  }

  umm::ExifToolConfig script_config;
  script_config.exiftool_script = script;
  umm::internal::ExifToolBackend script_backend(script_config);
  const umm::BackendAvailability script_status = script_backend.availability();
  if (script_status.available) {
    set_path(saved_path.c_str());
    return fail("Perl script packaging reported available without Perl");
  }
  if (script_status.reason != "Perl interpreter not found") {
    set_path(saved_path.c_str());
    std::fprintf(stderr, "script absence reason: %s\n",
                 script_status.reason.c_str());
    return fail("Perl script packaging missing Perl reason");
  }

#ifndef _WIN32
  const std::filesystem::path stub = dir / "exiftool.exe";
  {
    std::ofstream out(stub, std::ios::binary);
    if (!out) {
      set_path(saved_path.c_str());
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
    set_path(saved_path.c_str());
    return fail("failed to chmod native stub");
  }

  umm::ExifToolConfig stub_config;
  stub_config.exiftool_script = stub;
  umm::internal::ExifToolBackend stub_backend(stub_config);
  const umm::BackendAvailability stub_status = stub_backend.availability();
  if (!stub_status.available) {
    set_path(saved_path.c_str());
    std::fprintf(stderr, "native stub unavailable: %s\n",
                 stub_status.reason.c_str());
    return 1;
  }
  if (stub_status.version != "13.40") {
    set_path(saved_path.c_str());
    std::fprintf(stderr, "native stub version \"%s\"\n",
                 stub_status.version.c_str());
    return fail("native .exe version probe did not spawn the stub directly");
  }
#endif

  set_path(saved_path.c_str());
  return 0;
}
