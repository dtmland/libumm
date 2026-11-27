#include "core/call_context.hpp"
#include "umm/umm.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const char* message) {
  std::cerr << message << '\n';
  return 1;
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}

std::filesystem::path copy_fixture(const char* name) {
  const std::filesystem::path source =
      std::filesystem::path(UMM_FIXTURES_DIR) / name;
  const std::filesystem::path dest =
      std::filesystem::temp_directory_path() / "umm-cancel-copy.jpg";
  std::filesystem::copy_file(source, dest,
                             std::filesystem::copy_options::overwrite_existing);
  return dest;
}

class Arm {
 public:
  explicit Arm(const char* name) {
    umm::internal::set_cancel_at_checkpoint_for_test(name);
  }
  ~Arm() { umm::internal::set_cancel_at_checkpoint_for_test(nullptr); }
};

}  // namespace

int main() {
  umm::CancelToken idle;
  idle.cancel();
  if (idle.cancelled() || idle.shared()) {
    return fail("default token must never cancel");
  }
  umm::CancelToken token = umm::CancelToken::create();
  umm::CancelToken copy = token;
  if (!token.shared() || token.cancelled()) {
    return fail("created token should be shared and clear");
  }
  copy.cancel();
  if (!token.cancelled()) {
    return fail("cancel must be visible to copies");
  }

  const auto media = copy_fixture("jpeg/minimal.jpg");
  const auto original = read_bytes(media);
  umm::Metadata metadata;
  umm::Value headline;
  headline.data = std::string{"umm"};
  if (auto set = metadata.set("iptc.photo.headline", headline); !set.ok()) {
    return fail("failed to set headline");
  }

  {
    umm::WriteOptions options;
    options.cancel = token;
    const auto written = umm::write(media, metadata, options);
    if (written.ok() || written.error().code != umm::ErrorCode::cancelled) {
      return fail("pre-cancelled write should return cancelled");
    }
    if (read_bytes(media) != original) {
      return fail("pre-cancelled write changed the file");
    }
  }

  {
    umm::ReadOptions options;
    options.deadline = std::chrono::steady_clock::now() - std::chrono::seconds{1};
    const auto read = umm::read(media, options);
    if (read.ok() || read.error().code != umm::ErrorCode::deadline_exceeded) {
      return fail("past deadline should return deadline_exceeded");
    }
    if (read_bytes(media) != original) {
      return fail("deadline read changed the file");
    }
  }

  {
    Arm arm("before_rename");
    const auto written = umm::write(media, metadata, umm::StoragePolicy::preferred);
    if (written.ok() || written.error().code != umm::ErrorCode::cancelled) {
      return fail("before_rename should cancel");
    }
    if (written.error().detail != "before_rename") {
      return fail("before_rename detail missing");
    }
    if (read_bytes(media) != original) {
      return fail("cancel before rename changed the file");
    }
  }

  {
    Arm arm("between_carriers");
    const auto written =
        umm::write(media, metadata, umm::StoragePolicy::sidecar_required);
    if (written.ok() || written.error().code != umm::ErrorCode::cancelled) {
      return fail("between_carriers should cancel");
    }
    if (written.error().message !=
        "embedded write succeeded; sidecar write failed") {
      std::cerr << written.error().message << '\n';
      return fail("mixed cancel message mismatch");
    }
  }

  std::filesystem::remove(media);
  std::filesystem::remove(umm::sidecarPath(media));
  return 0;
}
