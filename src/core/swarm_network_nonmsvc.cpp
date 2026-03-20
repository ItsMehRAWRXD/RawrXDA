#if !defined(_MSC_VER)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

#include "swarm_protocol.h"

#include <cstdint>
#include <cstring>

namespace {

struct SwarmRingFallback {
    uint8_t* buffer;
    uint32_t capacity;
    uint32_t slotSize;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t reserved0;
    uint64_t pushes;
    uint64_t pops;
};

static volatile LONG g_swarmSequence = 0;
static volatile LONG64 g_heartbeatTicks[SWARM_MAX_NODES] = {};

static uint64_t monotonicTicks() {
    LARGE_INTEGER now = {};
    if (QueryPerformanceCounter(&now) != 0) {
        return static_cast<uint64_t>(now.QuadPart);
    }
    return static_cast<uint64_t>(GetTickCount64());
}

static uint64_t fnv1a64(const uint8_t* data, uint64_t len, uint64_t seed) {
    uint64_t hash = 1469598103934665603ULL ^ (seed + 0x9e3779b97f4a7c15ULL);
    for (uint64_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= 1099511628211ULL;
    }
    hash ^= hash >> 33;
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= hash >> 33;
    hash *= 0xc4ceb9fe1a85ec53ULL;
    hash ^= hash >> 33;
    return hash;
}

}  // namespace

extern "C" int Swarm_RingBuffer_Init(void* ring, void* buffer) {
    if (!ring || !buffer) {
        return -1;
    }
    auto* r = static_cast<SwarmRingFallback*>(ring);
    r->buffer = static_cast<uint8_t*>(buffer);
    r->capacity = SWARM_RING_CAPACITY;
    r->slotSize = SWARM_RING_SLOT_SIZE;
    r->head = 0;
    r->tail = 0;
    r->count = 0;
    r->reserved0 = 0;
    r->pushes = 0;
    r->pops = 0;
    return 0;
}

extern "C" int Swarm_RingBuffer_Push(void* ring, const void* data, uint64_t size) {
    if (!ring || !data) {
        return -1;
    }
    auto* r = static_cast<SwarmRingFallback*>(ring);
    if (!r->buffer || r->capacity == 0 || r->slotSize < sizeof(uint64_t)) {
        return -2;
    }
    if (r->count >= r->capacity) {
        return -3;
    }

    const uint64_t payloadCap = static_cast<uint64_t>(r->slotSize - sizeof(uint64_t));
    const uint64_t toCopy = (size < payloadCap) ? size : payloadCap;
    const uint32_t idx = r->tail % r->capacity;
    uint8_t* slot = r->buffer + static_cast<uint64_t>(idx) * r->slotSize;

    std::memcpy(slot, &toCopy, sizeof(toCopy));
    std::memcpy(slot + sizeof(toCopy), data, static_cast<size_t>(toCopy));
    r->tail = (r->tail + 1U) % r->capacity;
    ++r->count;
    ++r->pushes;
    return 0;
}

extern "C" uint64_t Swarm_RingBuffer_Pop(void* ring, void* dest) {
    if (!ring || !dest) {
        return 0;
    }
    auto* r = static_cast<SwarmRingFallback*>(ring);
    if (!r->buffer || r->count == 0) {
        return 0;
    }

    const uint32_t idx = r->head % r->capacity;
    const uint8_t* slot = r->buffer + static_cast<uint64_t>(idx) * r->slotSize;
    uint64_t size = 0;
    std::memcpy(&size, slot, sizeof(size));

    const uint64_t payloadCap = static_cast<uint64_t>(r->slotSize - sizeof(uint64_t));
    if (size > payloadCap) {
        size = payloadCap;
    }
    std::memcpy(dest, slot + sizeof(uint64_t), static_cast<size_t>(size));

    r->head = (r->head + 1U) % r->capacity;
    --r->count;
    ++r->pops;
    return size;
}

extern "C" uint64_t Swarm_RingBuffer_Count(void* ring) {
    if (!ring) {
        return 0;
    }
    auto* r = static_cast<SwarmRingFallback*>(ring);
    return static_cast<uint64_t>(r->count);
}

extern "C" int Swarm_Blake2b_128(const void* data, uint64_t len, void* out16) {
    if (!out16) {
        return -1;
    }
    const auto* bytes = static_cast<const uint8_t*>(data);
    const uint64_t h0 = fnv1a64(bytes, len, 0x6a09e667f3bcc909ULL);
    const uint64_t h1 = fnv1a64(bytes, len, 0xbb67ae8584caa73bULL);
    std::memcpy(out16, &h0, sizeof(h0));
    std::memcpy(static_cast<uint8_t*>(out16) + sizeof(h0), &h1, sizeof(h1));
    return 0;
}

extern "C" uint64_t Swarm_XXH64(const void* data, uint64_t len, uint64_t seed) {
    if (!data || len == 0) {
        return seed;
    }
    return fnv1a64(static_cast<const uint8_t*>(data), len, seed);
}

extern "C" int Swarm_ValidatePacketHeader(const void* packet) {
    if (!packet) {
        return -1;
    }

    const auto* hdr = static_cast<const SwarmPacketHeader*>(packet);
    if (hdr->magic != SWARM_MAGIC || hdr->version != SWARM_VERSION) {
        return -2;
    }
    if (hdr->payloadLen > SWARM_MAX_PAYLOAD) {
        return -3;
    }

    uint8_t computed[16] = {};
    const uint8_t* payload = static_cast<const uint8_t*>(packet) + SWARM_HEADER_SIZE;
    if (Swarm_Blake2b_128(payload, hdr->payloadLen, computed) != 0) {
        return -4;
    }
    if (std::memcmp(computed, hdr->checksum, sizeof(computed)) != 0) {
        return -5;
    }
    return 0;
}

extern "C" int Swarm_BuildPacketHeader(void* buffer, uint8_t opcode,
                                       uint16_t payloadLen, uint64_t taskId) {
    if (!buffer || payloadLen > SWARM_MAX_PAYLOAD) {
        return -1;
    }

    auto* hdr = static_cast<SwarmPacketHeader*>(buffer);
    std::memset(hdr, 0, sizeof(*hdr));
    hdr->magic = SWARM_MAGIC;
    hdr->version = SWARM_VERSION;
    hdr->opcode = opcode;
    hdr->payloadLen = payloadLen;
    hdr->sequenceId = static_cast<uint32_t>(InterlockedIncrement(&g_swarmSequence));
    hdr->timestampNs = monotonicTicks();
    hdr->taskId = taskId;
    hdr->reserved = 0;

    const void* payload = static_cast<const uint8_t*>(buffer) + SWARM_HEADER_SIZE;
    return Swarm_Blake2b_128(payload, payloadLen, hdr->checksum);
}

extern "C" uint64_t Swarm_HeartbeatRecord(uint32_t nodeSlot) {
    if (nodeSlot >= SWARM_MAX_NODES) {
        return 0;
    }
    const LONG64 now = static_cast<LONG64>(GetTickCount64());
    g_heartbeatTicks[nodeSlot] = now;
    return static_cast<uint64_t>(now);
}

extern "C" int Swarm_HeartbeatCheck(uint32_t nodeSlot) {
    if (nodeSlot >= SWARM_MAX_NODES) {
        return 0;
    }
    const LONG64 now = static_cast<LONG64>(GetTickCount64());
    const LONG64 last = g_heartbeatTicks[nodeSlot];
    if (last <= 0) {
        return 0;
    }
    return ((now - last) <= static_cast<LONG64>(SWARM_HEARTBEAT_TIMEOUT_MS)) ? 1 : 0;
}

extern "C" uint32_t Swarm_ComputeNodeFitness(void) {
    SYSTEM_INFO si = {};
    GetSystemInfo(&si);

    MEMORYSTATUSEX ms = {};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);

    const uint32_t cores = si.dwNumberOfProcessors > 0 ? si.dwNumberOfProcessors : 1;
    const uint64_t totalGb = ms.ullTotalPhys / (1024ULL * 1024ULL * 1024ULL);
    return cores * 100U + static_cast<uint32_t>(totalGb * 10ULL);
}

extern "C" void* Swarm_IOCP_Create(void) {
    return CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
}

extern "C" void* Swarm_IOCP_Associate(void* socket, void* iocp, uint64_t key) {
    if (!socket || !iocp) {
        return nullptr;
    }
    const HANDLE h = reinterpret_cast<HANDLE>(socket);
    const HANDLE port = reinterpret_cast<HANDLE>(iocp);
    return CreateIoCompletionPort(h, port, static_cast<ULONG_PTR>(key), 0);
}

extern "C" int Swarm_IOCP_GetCompletion(void* iocp, uint32_t timeoutMs,
                                        uint32_t* bytesTransferred, uint64_t* key) {
    if (!iocp || !bytesTransferred || !key) {
        return -1;
    }
    DWORD bytes = 0;
    ULONG_PTR completionKey = 0;
    OVERLAPPED* ov = nullptr;
    const BOOL ok = GetQueuedCompletionStatus(
        reinterpret_cast<HANDLE>(iocp), &bytes, &completionKey, &ov, timeoutMs);
    if (!ok) {
        return -2;
    }
    *bytesTransferred = static_cast<uint32_t>(bytes);
    *key = static_cast<uint64_t>(completionKey);
    return 0;
}

extern "C" uint64_t Swarm_MemCopy_NT(void* dest, const void* src, uint64_t count) {
    if (!dest || !src || count == 0) {
        return 0;
    }
    std::memcpy(dest, src, static_cast<size_t>(count));
    return count;
}

#endif  // !defined(_MSC_VER)
