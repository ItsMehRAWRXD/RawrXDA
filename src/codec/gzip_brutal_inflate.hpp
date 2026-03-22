#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace RawrXD::Codec {

/// Decompress gzip produced by `deflate_brutal_masm` (DEFLATE stored blocks only, CM=8).
/// No zlib dependency — matches the on-wire format emitted by the MASM encoder.
[[nodiscard]] bool gzipBrutalInflateStoredBlocks(const std::uint8_t* in, std::size_t inLen,
                                                  std::vector<std::uint8_t>& out);

}  // namespace RawrXD::Codec
