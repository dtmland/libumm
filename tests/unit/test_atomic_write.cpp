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

  const std::filesystem::path collision = dir / "collision.txt";
  write_all(collision, "collision-original");
  for (int i = 1; i <= 256; ++i) {
    write_all(dir / ("collision.umm-" + std::to_string(i) + ".txt"), "occupied");
  }
  const auto skipped = umm::internal::mutate_file_atomically(
      collision, [](const std::filesystem::path& working) {
        if (working.filename().string().find(".umm-") == std::string::npos) {
          return umm::Error{umm::ErrorCode::internal, "temp name missing prefix",
                            "", ""};
        }
        write_all(working, "collision-mutated");
        return umm::Result<void>{};
      });
  if (!skipped.ok()) {
    return fail("collision skip failed");
  }
  if (read_all(collision) != "collision-mutated") {
    return fail("collision skip did not replace destination");
  }
  for (int i = 1; i <= 256; ++i) {
    if (read_all(dir / ("collision.umm-" + std::to_string(i) + ".txt")) !=
        "occupied") {
      return fail("collision skip overwrote an existing temp name");
    }
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
