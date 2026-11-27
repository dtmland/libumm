// In-repo SHA-256 (FIPS 180-4), decision OQ-F4. Backend-free so payload digests
// and diffs do not depend on a crypto library or a metadata backend.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace umm::internal {

// Incremental SHA-256. Not thread-safe for a single instance; each caller uses
// its own object. finish() may be called once; the object is then spent.
class Sha256 {
 public:
  Sha256();

  void update(std::span<const std::byte> data);
  void update(const void* data, std::size_t size);

  std::array<std::uint8_t, 32> finish();
  std::string finishHex();

 private:
  void processBlock(const std::uint8_t* block);

  std::array<std::uint32_t, 8> state_;
  std::array<std::uint8_t, 64> buffer_;
  std::size_t bufferLen_{0};
  std::uint64_t totalBytes_{0};
};

// Lowercase hex of `bytes`.
std::string toHex(std::span<const std::uint8_t> bytes);

}  // namespace umm::internal
