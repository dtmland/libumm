#include "core/atomic_write.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

std::string read_all(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

void write_all(const std::filesystem::path& path, std::string_view text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

}  // namespace

int main() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "umm-atomic-write";
  std::filesystem::create_directories(dir);
  const std::filesystem::path file = dir / "original.txt";
  write_all(file, "original-bytes");

  umm::internal::set_atomic_write_fault_for_test(
      umm::internal::AtomicWriteFault::none);
  const auto ok = umm::internal::mutate_file_atomically(
      file, [](const std::filesystem::path& working) {
        write_all(working, "mutated-bytes");
        return umm::Result<void>{};
      });
  if (!ok.ok()) {
    return fail("successful mutate failed");
  }
  if (read_all(file) != "mutated-bytes") {
    return fail("successful mutate did not replace destination");
  }

  write_all(file, "keep-me");
  umm::internal::set_atomic_write_fault_for_test(
      umm::internal::AtomicWriteFault::before_rename);
  const auto injected = umm::internal::mutate_file_atomically(
      file, [](const std::filesystem::path& working) {
        write_all(working, "should-not-commit");
        return umm::Result<void>{};
      });
  umm::internal::set_atomic_write_fault_for_test(
      umm::internal::AtomicWriteFault::none);
  if (injected.ok()) {
    return fail("injected before_rename should fail");
  }
  if (read_all(file) != "keep-me") {
    return fail("injected failure mutated the original");
  }

  const auto failed_mutate = umm::internal::mutate_file_atomically(
      file, [](const std::filesystem::path&) {
        return umm::Error{umm::ErrorCode::backend_failed, "mutator failed", "",
                          ""};
      });
  if (failed_mutate.ok()) {
    return fail("failed mutator should fail");
  }
  if (read_all(file) != "keep-me") {
    return fail("failed mutator mutated the original");
  }

  const std::filesystem::path created = dir / "new-sidecar.xmp";
  const auto created_ok = umm::internal::mutate_file_atomically(
      created,
      [](const std::filesystem::path& working) {
        write_all(working, "sidecar-bytes");
        return umm::Result<void>{};
      },
      true);
  if (!created_ok.ok()) {
    return fail("create_if_missing failed");
  }
  if (read_all(created) != "sidecar-bytes") {
    return fail("create_if_missing did not write destination");
  }

  std::filesystem::remove_all(dir);
  return 0;
}
