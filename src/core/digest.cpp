// Payload digest walkers (decision F15, §6 payload-hash research). Each walker
// hashes the same bytes as ExifTool's ImageDataHash so an `exiftool-*` scheme
// digest equals `exiftool -api ImageHashType=SHA256 -ImageDataHash`. Parsing is
// bounded and defensive: a structural error returns ErrorCode::format_corrupt.
#include "umm/digest.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "core/sha256.hpp"

namespace umm {
namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr const char* kScheme = "exiftool-imagedatahash-sha256";

Error corrupt(const std::filesystem::path& path, const std::string& what) {
  return Error{ErrorCode::format_corrupt, what, "",
               reinterpret_cast<const char*>(path.u8string().c_str())};
}

std::uint32_t be32(const Bytes& b, std::size_t off) {
  return (static_cast<std::uint32_t>(b[off]) << 24) |
         (static_cast<std::uint32_t>(b[off + 1]) << 16) |
         (static_cast<std::uint32_t>(b[off + 2]) << 8) |
         static_cast<std::uint32_t>(b[off + 3]);
}

std::uint64_t be64(const Bytes& b, std::size_t off) {
  std::uint64_t v = 0;
  for (int i = 0; i < 8; ++i) {
    v = (v << 8) | b[off + static_cast<std::size_t>(i)];
  }
  return v;
}

bool matches(const Bytes& b, std::size_t off, const char* sig, std::size_t n) {
  if (off + n > b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < n; ++i) {
    if (b[off + i] != static_cast<std::uint8_t>(sig[i])) {
      return false;
    }
  }
  return true;
}

// --- JPEG (ExifTool.pm ImageDataHash) ---------------------------------------
// Hash every scan: from each SOS marker (FF DA) through its entropy-coded data,
// including embedded restart markers (FF D0-D7) and byte stuffing (FF 00), up
// to but not including the next non-scan marker. Segments carrying a length
// word (APPn, DQT, DHT, SOFn, COM, ...) are skipped, so interleaved tables in a
// progressive file are excluded. EOI (FF D9) ends the walk and is not hashed.
Result<PayloadDigest> digestJpeg(const Bytes& b,
                                 const std::filesystem::path& path) {
  internal::Sha256 sha;
  const std::size_t n = b.size();
  std::size_t pos = 2;  // past SOI (FF D8)
  bool hashedScan = false;
  while (pos + 1 < n) {
    if (b[pos] != 0xFF) {
      return corrupt(path, "JPEG: expected marker prefix");
    }
    const std::uint8_t marker = b[pos + 1];
    if (marker == 0xFF) {  // fill byte before the next marker
      ++pos;
      continue;
    }
    if (marker == 0xD9) {  // EOI
      break;
    }
    if (marker == 0xDA) {  // SOS: hash marker + scan data
      std::size_t p = pos + 2;
      while (p + 1 < n) {
        if (b[p] == 0xFF) {
          const std::uint8_t next = b[p + 1];
          if (next == 0x00 || (next >= 0xD0 && next <= 0xD7)) {
            p += 2;  // stuffing / restart marker: part of the scan
            continue;
          }
          if (next == 0xFF) {
            ++p;  // fill byte
            continue;
          }
          break;  // real marker: end of this scan
        }
        ++p;
      }
      sha.update(b.data() + pos, p - pos);
      hashedScan = true;
      pos = p;
      continue;
    }
    // Marker carrying a 2-byte length word; skip the whole segment.
    if (pos + 4 > n) {
      return corrupt(path, "JPEG: truncated marker segment");
    }
    const std::size_t segLen = (static_cast<std::size_t>(b[pos + 2]) << 8) |
                               static_cast<std::size_t>(b[pos + 3]);
    if (segLen < 2 || pos + 2 + segLen > n) {
      return corrupt(path, "JPEG: bad segment length");
    }
    pos += 2 + segLen;
  }
  if (!hashedScan) {
    return corrupt(path, "JPEG: no scan data");
  }
  return PayloadDigest{kScheme, sha.finishHex()};
}

// --- PNG (PNG.pm %isDatChunk) ------------------------------------------------
// Hash the data payload (excluding 4-byte length, 4-byte type, 4-byte CRC) of
// every IDAT / JDAT / JDAA chunk.
Result<PayloadDigest> digestPng(const Bytes& b,
                                const std::filesystem::path& path) {
  internal::Sha256 sha;
  const std::size_t n = b.size();
  std::size_t pos = 8;  // past the 8-byte PNG signature
  bool found = false;
  while (pos + 8 <= n) {
    const std::uint32_t len = be32(b, pos);
    const std::size_t dataOff = pos + 8;
    if (dataOff + len + 4 > n) {
      return corrupt(path, "PNG: chunk exceeds file");
    }
    const bool isDat = matches(b, pos + 4, "IDAT", 4) ||
                       matches(b, pos + 4, "JDAT", 4) ||
                       matches(b, pos + 4, "JDAA", 4);
    if (isDat) {
      sha.update(b.data() + dataOff, len);
      found = true;
    }
    const bool isEnd = matches(b, pos + 4, "IEND", 4);
    pos = dataOff + len + 4;  // skip data + CRC
    if (isEnd) {
      break;
    }
  }
  if (!found) {
    return corrupt(path, "PNG: no image data chunk");
  }
  return PayloadDigest{kScheme, sha.finishHex()};
}

// --- RIFF / WebP (RIFF.pm %isImageData) --------------------------------------
// Hash the chunk data (including the trailing pad byte when the length is odd)
// of the image-data chunks: WebP VP8 / VP8L / ALPH / ANIM / ANMF.
Result<PayloadDigest> digestWebp(const Bytes& b,
                                 const std::filesystem::path& path) {
  internal::Sha256 sha;
  const std::size_t n = b.size();
  std::size_t pos = 12;  // past "RIFF" + size + "WEBP"
  bool found = false;
  while (pos + 8 <= n) {
    // RIFF chunk sizes are little-endian.
    const std::uint32_t leLen = static_cast<std::uint32_t>(b[pos + 4]) |
                                (static_cast<std::uint32_t>(b[pos + 5]) << 8) |
                                (static_cast<std::uint32_t>(b[pos + 6]) << 16) |
                                (static_cast<std::uint32_t>(b[pos + 7]) << 24);
    const std::size_t dataOff = pos + 8;
    const std::size_t padded = leLen + (leLen & 1u);
    if (dataOff + padded > n) {
      // Allow a final chunk missing only its pad byte.
      if (dataOff + leLen > n) {
        return corrupt(path, "WebP: chunk exceeds file");
      }
    }
    const bool isImage = matches(b, pos, "VP8 ", 4) ||
                         matches(b, pos, "VP8L", 4) ||
                         matches(b, pos, "ALPH", 4) ||
                         matches(b, pos, "ANIM", 4) ||
                         matches(b, pos, "ANMF", 4);
    if (isImage) {
      const std::size_t avail =
          dataOff + padded <= n ? padded : (n - dataOff);
      sha.update(b.data() + dataOff, avail);
      found = true;
    }
    pos = dataOff + padded;
  }
  if (!found) {
    return corrupt(path, "WebP: no image data chunk");
  }
  return PayloadDigest{kScheme, sha.finishHex()};
}

// --- MP4 / MOV (QuickTimeStream.pl ProcessSamples) ---------------------------
// Hash the sample data of every vide and soun track, chunk by chunk, in trak
// file order, into one digest. Per-chunk size is the sum of its sample sizes
// (from stsc + stsz/stz2); chunk offsets come from stco/co64.
struct BoxIter {
  const Bytes& b;
  std::size_t pos;
  std::size_t end;
  bool ok{true};
};

// Returns false at end; fills type/dataOff/dataEnd for the next box.
bool nextBox(BoxIter& it, std::array<char, 4>& type, std::size_t& dataOff,
             std::size_t& dataEnd) {
  if (it.pos + 8 > it.end) {
    return false;
  }
  std::uint64_t size = be32(it.b, it.pos);
  std::size_t header = 8;
  if (size == 1) {
    if (it.pos + 16 > it.end) {
      it.ok = false;
      return false;
    }
    size = be64(it.b, it.pos + 8);
    header = 16;
  } else if (size == 0) {
    size = it.end - it.pos;
  }
  if (size < header || it.pos + size > it.end) {
    it.ok = false;
    return false;
  }
  for (int i = 0; i < 4; ++i) {
    type[static_cast<std::size_t>(i)] =
        static_cast<char>(it.b[it.pos + 4 + static_cast<std::size_t>(i)]);
  }
  dataOff = it.pos + header;
  dataEnd = it.pos + size;
  it.pos += size;
  return true;
}

bool isType(const std::array<char, 4>& t, const char* s) {
  return t[0] == s[0] && t[1] == s[1] && t[2] == s[2] && t[3] == s[3];
}

Result<PayloadDigest> digestMp4(const Bytes& b,
                                const std::filesystem::path& path) {
  internal::Sha256 sha;
  const std::size_t n = b.size();
  bool found = false;

  // Locate moov at top level.
  BoxIter top{b, 0, n};
  std::array<char, 4> type{};
  std::size_t off = 0;
  std::size_t boxEnd = 0;
  while (nextBox(top, type, off, boxEnd)) {
    if (!isType(type, "moov")) {
      continue;
    }
    BoxIter moov{b, off, boxEnd};
    std::array<char, 4> t1{};
    std::size_t o1 = 0;
    std::size_t e1 = 0;
    while (nextBox(moov, t1, o1, e1)) {
      if (!isType(t1, "trak")) {
        continue;
      }
      // Walk trak -> mdia -> {hdlr, minf -> stbl -> {stsc,stsz/stz2,stco/co64}}.
      std::array<char, 4> handler{};
      std::vector<std::uint64_t> offsets;
      std::vector<std::uint32_t> sizes;
      std::uint32_t fixedSize = 0;
      std::uint32_t sampleCount = 0;
      std::vector<std::array<std::uint32_t, 3>> stsc;  // first,samples,descidx

      BoxIter trak{b, o1, e1};
      std::array<char, 4> t2{};
      std::size_t o2 = 0;
      std::size_t e2 = 0;
      while (nextBox(trak, t2, o2, e2)) {
        if (!isType(t2, "mdia")) {
          continue;
        }
        BoxIter mdia{b, o2, e2};
        std::array<char, 4> t3{};
        std::size_t o3 = 0;
        std::size_t e3 = 0;
        while (nextBox(mdia, t3, o3, e3)) {
          if (isType(t3, "hdlr")) {
            if (o3 + 12 <= e3) {
              for (int i = 0; i < 4; ++i) {
                handler[static_cast<std::size_t>(i)] = static_cast<char>(
                    b[o3 + 8 + static_cast<std::size_t>(i)]);
              }
            }
            continue;
          }
          if (!isType(t3, "minf")) {
            continue;
          }
          BoxIter minf{b, o3, e3};
          std::array<char, 4> t4{};
          std::size_t o4 = 0;
          std::size_t e4 = 0;
          while (nextBox(minf, t4, o4, e4)) {
            if (!isType(t4, "stbl")) {
              continue;
            }
            BoxIter stbl{b, o4, e4};
            std::array<char, 4> t5{};
            std::size_t o5 = 0;
            std::size_t e5 = 0;
            while (nextBox(stbl, t5, o5, e5)) {
              if (isType(t5, "stco")) {
                if (o5 + 8 > e5) {
                  return corrupt(path, "MP4: truncated stco");
                }
                const std::uint32_t c = be32(b, o5 + 4);
                std::size_t p = o5 + 8;
                for (std::uint32_t i = 0; i < c; ++i) {
                  if (p + 4 > e5) {
                    return corrupt(path, "MP4: stco overflow");
                  }
                  offsets.push_back(be32(b, p));
                  p += 4;
                }
              } else if (isType(t5, "co64")) {
                if (o5 + 8 > e5) {
                  return corrupt(path, "MP4: truncated co64");
                }
                const std::uint32_t c = be32(b, o5 + 4);
                std::size_t p = o5 + 8;
                for (std::uint32_t i = 0; i < c; ++i) {
                  if (p + 8 > e5) {
                    return corrupt(path, "MP4: co64 overflow");
                  }
                  offsets.push_back(be64(b, p));
                  p += 8;
                }
              } else if (isType(t5, "stsz")) {
                if (o5 + 12 > e5) {
                  return corrupt(path, "MP4: truncated stsz");
                }
                fixedSize = be32(b, o5 + 4);
                sampleCount = be32(b, o5 + 8);
                if (fixedSize == 0) {
                  std::size_t p = o5 + 12;
                  for (std::uint32_t i = 0; i < sampleCount; ++i) {
                    if (p + 4 > e5) {
                      return corrupt(path, "MP4: stsz overflow");
                    }
                    sizes.push_back(be32(b, p));
                    p += 4;
                  }
                }
              } else if (isType(t5, "stz2")) {
                if (o5 + 12 > e5) {
                  return corrupt(path, "MP4: truncated stz2");
                }
                const std::uint32_t fieldSize = be32(b, o5 + 4) & 0xFF;
                sampleCount = be32(b, o5 + 8);
                std::size_t p = o5 + 12;
                for (std::uint32_t i = 0; i < sampleCount; ++i) {
                  if (fieldSize == 16) {
                    if (p + 2 > e5) {
                      return corrupt(path, "MP4: stz2 overflow");
                    }
                    sizes.push_back((static_cast<std::uint32_t>(b[p]) << 8) |
                                    b[p + 1]);
                    p += 2;
                  } else if (fieldSize == 8) {
                    if (p + 1 > e5) {
                      return corrupt(path, "MP4: stz2 overflow");
                    }
                    sizes.push_back(b[p]);
                    p += 1;
                  } else {
                    return corrupt(path, "MP4: unsupported stz2 field size");
                  }
                }
              } else if (isType(t5, "stsc")) {
                if (o5 + 8 > e5) {
                  return corrupt(path, "MP4: truncated stsc");
                }
                const std::uint32_t c = be32(b, o5 + 4);
                std::size_t p = o5 + 8;
                for (std::uint32_t i = 0; i < c; ++i) {
                  if (p + 12 > e5) {
                    return corrupt(path, "MP4: stsc overflow");
                  }
                  stsc.push_back({be32(b, p), be32(b, p + 4), be32(b, p + 8)});
                  p += 12;
                }
              }
            }
          }
        }
      }

      const bool wanted = isType(handler, "vide") || isType(handler, "soun");
      if (!wanted || offsets.empty() || stsc.empty()) {
        continue;
      }
      if (fixedSize != 0) {
        sizes.assign(sampleCount, fixedSize);
      }

      // Samples-per-chunk for each chunk, expanded from stsc runs.
      const std::size_t nChunks = offsets.size();
      std::vector<std::uint32_t> spc(nChunks, 0);
      for (std::size_t i = 0; i < stsc.size(); ++i) {
        const std::uint32_t first = stsc[i][0];
        const std::uint32_t samples = stsc[i][1];
        const std::uint32_t last =
            (i + 1 < stsc.size()) ? stsc[i + 1][0] - 1
                                  : static_cast<std::uint32_t>(nChunks);
        if (first < 1) {
          return corrupt(path, "MP4: bad stsc first-chunk");
        }
        for (std::uint32_t c = first; c <= last && c <= nChunks; ++c) {
          spc[c - 1] = samples;
        }
      }

      std::size_t si = 0;
      for (std::size_t c = 0; c < nChunks; ++c) {
        std::uint64_t chunkSize = 0;
        for (std::uint32_t s = 0; s < spc[c]; ++s) {
          if (si >= sizes.size()) {
            return corrupt(path, "MP4: sample size table underflow");
          }
          chunkSize += sizes[si++];
        }
        const std::uint64_t start = offsets[c];
        if (start > n || start + chunkSize > n) {
          return corrupt(path, "MP4: chunk exceeds file");
        }
        sha.update(b.data() + start, static_cast<std::size_t>(chunkSize));
        found = true;
      }
    }
  }
  if (!top.ok) {
    return corrupt(path, "MP4: malformed box structure");
  }
  if (!found) {
    return corrupt(path, "MP4: no vide/soun sample data");
  }
  return PayloadDigest{kScheme, sha.finishHex()};
}

enum class Kind { unknown, jpeg, png, webp, mp4 };

Kind sniff(const Bytes& b) {
  if (b.size() >= 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF) {
    return Kind::jpeg;
  }
  if (b.size() >= 8 && b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' &&
      b[3] == 'G' && b[4] == 0x0D && b[5] == 0x0A && b[6] == 0x1A &&
      b[7] == 0x0A) {
    return Kind::png;
  }
  if (b.size() >= 12 && matches(b, 0, "RIFF", 4) && matches(b, 8, "WEBP", 4)) {
    return Kind::webp;
  }
  if (b.size() >= 12 && matches(b, 4, "ftyp", 4)) {
    return Kind::mp4;
  }
  return Kind::unknown;
}

}  // namespace

Result<PayloadDigest> payloadDigest(const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return Error{ErrorCode::io_not_found, "file not found", "",
                 reinterpret_cast<const char*>(path.u8string().c_str())};
  }
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    return Error{ErrorCode::io_read_failed, "cannot open file", "",
                 reinterpret_cast<const char*>(path.u8string().c_str())};
  }
  const std::streamoff size = in.tellg();
  if (size < 0) {
    return Error{ErrorCode::io_read_failed, "cannot size file", "",
                 reinterpret_cast<const char*>(path.u8string().c_str())};
  }
  Bytes b(static_cast<std::size_t>(size));
  in.seekg(0);
  if (!b.empty() &&
      !in.read(reinterpret_cast<char*>(b.data()),
               static_cast<std::streamsize>(b.size()))) {
    return Error{ErrorCode::io_read_failed, "cannot read file", "",
                 reinterpret_cast<const char*>(path.u8string().c_str())};
  }

  switch (sniff(b)) {
    case Kind::jpeg:
      return digestJpeg(b, path);
    case Kind::png:
      return digestPng(b, path);
    case Kind::webp:
      return digestWebp(b, path);
    case Kind::mp4:
      return digestMp4(b, path);
    case Kind::unknown:
      break;
  }
  return Error{ErrorCode::unsupported_capability,
               "payload digest not supported for this type", "",
               reinterpret_cast<const char*>(path.u8string().c_str())};
}

}  // namespace umm
