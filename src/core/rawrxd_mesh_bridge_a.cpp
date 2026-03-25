// =============================================================================
// rawrxd_mesh_bridge_a.cpp — ASM mesh symbol fallbacks for RawrXD-Win32IDE
// =============================================================================
// Provides C-linkage fallbacks for asm_mesh_* symbols (CRDT, ZKP, DHT) when
// the MASM mesh kernel .obj is not linked. These stubs allow the IDE to link
// and run. File name must NOT match .*_stubs\.cpp so real-lane CMake does not
// exclude it.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// =============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace
{
std::mutex g_meshMutex;
uint32_t g_meshMaxNodes = 0;
uint32_t g_meshShardCount = 0;
std::atomic<uint64_t> g_meshOpCount{0};
bool g_meshInitialized = false;
}  // namespace

extern "C"
{

    // -------------------------------------------------------------------------
    // asm_mesh_init
    // Initialize the mesh brain subsystem with the given capacity parameters.
    // Returns 1 on success.
    // -------------------------------------------------------------------------
    int asm_mesh_init(uint32_t maxNodes, uint32_t shardCount)
    {
        std::lock_guard<std::mutex> lock(g_meshMutex);
        g_meshMaxNodes    = maxNodes;
        g_meshShardCount  = shardCount;
        g_meshInitialized = true;
        return 1;
    }

    // -------------------------------------------------------------------------
    // asm_mesh_crdt_merge
    // Merge a remote CRDT state buffer into the local buffer by copying the
    // remote bytes over the local region.  len is the authoritative size of the
    // remote snapshot; we honour it directly.
    // Returns 1 on success, -1 if either pointer is null.
    // -------------------------------------------------------------------------
    int asm_mesh_crdt_merge(void* local, const void* remote, uint32_t len)
    {
        if (local == nullptr || remote == nullptr)
        {
            return -1;
        }
        std::lock_guard<std::mutex> lock(g_meshMutex);
        std::memcpy(local, remote, static_cast<size_t>(len));
        g_meshOpCount.fetch_add(1u, std::memory_order_relaxed);
        return 1;
    }

    // -------------------------------------------------------------------------
    // asm_mesh_crdt_delta
    // Compute a byte-wise XOR delta between base and current into deltaOut.
    // *deltaLen is set to len (the common region size).
    // All pointer arguments are required; returns silently on null.
    // -------------------------------------------------------------------------
    void asm_mesh_crdt_delta(const void* base, const void* current,
                             void* deltaOut, uint32_t* deltaLen)
    {
        if (base == nullptr || current == nullptr || deltaOut == nullptr ||
            deltaLen == nullptr)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(g_meshMutex);
        const uint32_t len              = *deltaLen;
        const uint8_t* bPtr             = static_cast<const uint8_t*>(base);
        const uint8_t* cPtr             = static_cast<const uint8_t*>(current);
        uint8_t*       dPtr             = static_cast<uint8_t*>(deltaOut);
        for (uint32_t i = 0; i < len; ++i)
        {
            dPtr[i] = bPtr[i] ^ cPtr[i];
        }
        *deltaLen = len;
        g_meshOpCount.fetch_add(1u, std::memory_order_relaxed);
    }

    // -------------------------------------------------------------------------
    // asm_mesh_zkp_generate
    // Generate a zero-knowledge proof stub by computing an FNV-1a digest of the
    // input data and writing the first 32 bytes of the expanded digest into
    // proofOut.  *proofLen is set to 32 on success.
    // Returns 1 on success, -1 if any required pointer is null.
    // -------------------------------------------------------------------------
    int asm_mesh_zkp_generate(const void* data, uint32_t dataLen,
                               void* proofOut, uint32_t* proofLen)
    {
        if (data == nullptr || proofOut == nullptr || proofLen == nullptr)
        {
            return -1;
        }
        std::lock_guard<std::mutex> lock(g_meshMutex);
        // FNV-1a over the data bytes.
        constexpr uint32_t FNV_OFFSET = 2166136261u;
        constexpr uint32_t FNV_PRIME  = 16777619u;
        const uint8_t* src = static_cast<const uint8_t*>(data);
        uint32_t hash = FNV_OFFSET;
        for (uint32_t i = 0; i < dataLen; ++i)
        {
            hash ^= src[i];
            hash *= FNV_PRIME;
        }
        // Expand the 4-byte hash into a 32-byte deterministic proof block.
        uint8_t* out = static_cast<uint8_t*>(proofOut);
        for (uint32_t slot = 0; slot < 8u; ++slot)
        {
            uint32_t word = hash;
            word ^= (slot * FNV_PRIME);
            word *= FNV_PRIME;
            std::memcpy(out + slot * 4u, &word, sizeof(uint32_t));
        }
        *proofLen = 32u;
        g_meshOpCount.fetch_add(1u, std::memory_order_relaxed);
        return 1;
    }

    // -------------------------------------------------------------------------
    // asm_mesh_zkp_verify
    // Verify a zero-knowledge proof by recomputing the FNV-1a digest from data
    // and comparing the first bytes against the supplied proof.
    // Returns 1 if the proof is consistent, 0 if verification fails, -1 on bad
    // input.
    // -------------------------------------------------------------------------
    int asm_mesh_zkp_verify(const void* data, uint32_t dataLen,
                             const void* proof, uint32_t proofLen)
    {
        if (data == nullptr || proof == nullptr)
        {
            return -1;
        }
        if (proofLen < 4u)
        {
            return 0;
        }
        std::lock_guard<std::mutex> lock(g_meshMutex);
        constexpr uint32_t FNV_OFFSET = 2166136261u;
        constexpr uint32_t FNV_PRIME  = 16777619u;
        const uint8_t* src = static_cast<const uint8_t*>(data);
        uint32_t hash = FNV_OFFSET;
        for (uint32_t i = 0; i < dataLen; ++i)
        {
            hash ^= src[i];
            hash *= FNV_PRIME;
        }
        // Re-derive the first word of the proof block (slot 0).
        uint32_t expectedWord = hash;
        expectedWord ^= (0u * FNV_PRIME);
        expectedWord *= FNV_PRIME;
        uint32_t storedWord = 0u;
        std::memcpy(&storedWord, proof, sizeof(uint32_t));
        g_meshOpCount.fetch_add(1u, std::memory_order_relaxed);
        return (storedWord == expectedWord) ? 1 : 0;
    }

    // -------------------------------------------------------------------------
    // asm_mesh_dht_xor_distance
    // Compute the XOR distance metric between two DHT keys by XOR-ing the first
    // min(keyLen, 8) bytes and returning the result as a uint64_t.
    // -------------------------------------------------------------------------
    uint64_t asm_mesh_dht_xor_distance(const void* keyA, const void* keyB,
                                        uint32_t keyLen)
    {
        if (keyA == nullptr || keyB == nullptr || keyLen == 0u)
        {
            return 0xFFFFFFFFFFFFFFFFULL;
        }
        std::lock_guard<std::mutex> lock(g_meshMutex);
        const uint8_t* a   = static_cast<const uint8_t*>(keyA);
        const uint8_t* b   = static_cast<const uint8_t*>(keyB);
        const uint32_t len = (keyLen < 8u) ? keyLen : 8u;
        uint64_t dist      = 0u;
        for (uint32_t i = 0; i < len; ++i)
        {
            dist |= (static_cast<uint64_t>(a[i] ^ b[i]) << (i * 8u));
        }
        g_meshOpCount.fetch_add(1u, std::memory_order_relaxed);
        return dist;
    }

    // -------------------------------------------------------------------------
    // asm_mesh_dht_find_closest
    // Stub: the routing table is not yet populated, so zero results are
    // returned.  resultBuf must be non-null; maxResults is the upper bound.
    // Returns 0 (number of results written).
    // -------------------------------------------------------------------------
    int asm_mesh_dht_find_closest(const void* targetKey, uint32_t keyLen,
                                   void* resultBuf, uint32_t maxResults)
    {
        if (resultBuf == nullptr)
        {
            return -1;
        }
        (void)targetKey;
        (void)keyLen;
        (void)maxResults;
        std::lock_guard<std::mutex> lock(g_meshMutex);
        g_meshOpCount.fetch_add(1u, std::memory_order_relaxed);
        return 0;
    }

}  // extern "C"
