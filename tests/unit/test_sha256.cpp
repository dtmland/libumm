// FIPS 180-4 known-answer tests for the in-repo SHA-256 (decision OQ-F4).
#include "core/sha256.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

std::string hashOf(const std::string& input) {
  umm::internal::Sha256 sha;
  sha.update(input.data(), input.size());
  return sha.finishHex();
}

}  // namespace

int main() {
  if (hashOf("") !=
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") {
    return fail("sha256 of empty string");
  }
  if (hashOf("abc") !=
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") {
    return fail("sha256 of abc");
  }
  if (hashOf(
          "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") !=
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") {
    return fail("sha256 of 56-byte message");
  }

  // One million 'a' characters, fed in several chunks to exercise buffering.
  {
    umm::internal::Sha256 sha;
    const std::string block(1000, 'a');
    for (int i = 0; i < 1000; ++i) {
      sha.update(block.data(), block.size());
    }
    if (sha.finishHex() !=
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") {
      return fail("sha256 of one million a");
    }
  }

  return 0;
}
