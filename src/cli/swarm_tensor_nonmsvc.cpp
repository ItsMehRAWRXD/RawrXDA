#if !defined(_MSC_VER)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

#include <cstdint>
#include <cstring>

namespace {

uint32_t crc32Table[256];
bool crcInit = false;

void initCrc32() {
    if (crcInit) {
        return;
    }
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            c = (c & 1U) ? (0xEDB88320U ^ (c >> 1U)) : (c >> 1U);
        }
        crc32Table[i] = c;
    }
    crcInit = true;
}

}  // namespace

extern "C" int swarm_receive_header(uint64_t socket_handle, void* header) {
    if (!header) {
        return -1;
    }

    SOCKET sock = static_cast<SOCKET>(socket_handle);
    uint8_t* out = static_cast<uint8_t*>(header);
    int total = 0;
    while (total < 32) {
        int got = recv(sock, reinterpret_cast<char*>(out + total), 32 - total, 0);
        if (got <= 0) {
            return -2;
        }
        total += got;
    }
    return 0;
}

extern "C" uint32_t swarm_compute_layer_crc32(const void* data, uint64_t size) {
    if (!data || size == 0) {
        return 0;
    }
    initCrc32();
    uint32_t crc = 0xFFFFFFFFU;
    const auto* p = static_cast<const uint8_t*>(data);
    for (uint64_t i = 0; i < size; ++i) {
        crc = crc32Table[(crc ^ p[i]) & 0xFFU] ^ (crc >> 8U);
    }
    return crc ^ 0xFFFFFFFFU;
}

extern "C" uint32_t swarm_build_discovery_packet(void* buffer, uint32_t buf_size, uint64_t total_vram,
                                                 uint64_t free_vram, uint32_t role, uint32_t max_layers) {
    if (!buffer || buf_size < 36) {
        return 0;
    }

    // Layout:
    //  0: u32 magic ('RAWR' little-endian: 0x52574152)
    //  4: u16 msgType (discovery = 6)
    //  6: u16 packetSize
    //  8: u64 total_vram
    // 16: u64 free_vram
    // 24: u32 role
    // 28: u32 max_layers
    // 32: u32 crc32 over bytes [0..31]
    uint8_t* out = static_cast<uint8_t*>(buffer);
    std::memset(out, 0, buf_size);

    const uint32_t magic = 0x52574152U;
    const uint16_t msgType = 6;
    const uint16_t packetSize = 36;

    std::memcpy(out + 0, &magic, sizeof(magic));
    std::memcpy(out + 4, &msgType, sizeof(msgType));
    std::memcpy(out + 6, &packetSize, sizeof(packetSize));
    std::memcpy(out + 8, &total_vram, sizeof(total_vram));
    std::memcpy(out + 16, &free_vram, sizeof(free_vram));
    std::memcpy(out + 24, &role, sizeof(role));
    std::memcpy(out + 28, &max_layers, sizeof(max_layers));

    const uint32_t crc = swarm_compute_layer_crc32(out, 32);
    std::memcpy(out + 32, &crc, sizeof(crc));
    return packetSize;
}

#endif  // !defined(_MSC_VER)
