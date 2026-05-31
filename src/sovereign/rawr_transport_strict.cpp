#include "rawrxd/rawr_unified_abi.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>

#include <winsock2.h>

namespace rawr::sovereign {

namespace {
constexpr std::uint32_t kRecvChunkBytes = 4096;

static_assert(sizeof(rawr::abi::RawrUnifiedFlags) == rawr::abi::kBytesFlags,
              "ABI header mismatch");
} // namespace

// Strict contract: transport only. No parsing, no decode, no dynamic allocation.
int RawrTransportPulseStrict(SOCKET sock,
                             rawr::abi::RawrUnifiedFlags* flags,
                             std::uint8_t* token_ring,
                             std::uint32_t token_ring_bytes) {
    if (sock == INVALID_SOCKET || flags == nullptr || token_ring == nullptr || token_ring_bytes == 0) {
        return -1;
    }

    std::uint8_t tmp_buf[kRecvChunkBytes];
    const int n = ::recv(sock, reinterpret_cast<char*>(tmp_buf), static_cast<int>(sizeof(tmp_buf)), 0);
    if (n <= 0) {
        return n;
    }

    std::atomic_ref<std::uint32_t> data_seq_ref(const_cast<std::uint32_t&>(flags->data_seq));
    std::atomic_ref<std::uint32_t> new_data_ref(const_cast<std::uint32_t&>(flags->new_data));

    const std::uint32_t data_seq = data_seq_ref.load(std::memory_order_relaxed);
    const std::uint32_t write_offset = data_seq % token_ring_bytes;

    const std::uint32_t copy_len = static_cast<std::uint32_t>(n);
    const std::uint32_t first_chunk = std::min(copy_len, token_ring_bytes - write_offset);

    std::memcpy(token_ring + write_offset, tmp_buf, first_chunk);
    if (copy_len > first_chunk) {
        std::memcpy(token_ring, tmp_buf + first_chunk, copy_len - first_chunk);
    }

    const std::uint32_t next_seq = data_seq + copy_len;

    // Publish payload before sequence update.
    std::atomic_thread_fence(std::memory_order_release);
    data_seq_ref.store(next_seq, std::memory_order_relaxed);
    new_data_ref.store(next_seq, std::memory_order_release);

    return n;
}

} // namespace rawr::sovereign
