// rawrxd_mesh_bridge_b.cpp
// Mesh bridge B: federated averaging, gossip, shard, quorum, topology, stats, shutdown

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>
#include <cmath>

namespace {
    std::mutex           g_meshMtx;
    std::atomic<uint32_t> g_activeNodes{0};
    std::atomic<uint64_t> g_gossipCount{0};
    std::atomic<uint64_t> g_fedAvgCount{0};

    static uint64_t fnv1a64(const void* data, uint32_t len)
    {
        const uint64_t FNV_OFFSET = 14695981039346656037ULL;
        const uint64_t FNV_PRIME  = 1099511628211ULL;
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        uint64_t hash = FNV_OFFSET;
        for (uint32_t i = 0; i < len; ++i)
        {
            hash ^= static_cast<uint64_t>(bytes[i]);
            hash *= FNV_PRIME;
        }
        return hash;
    }
} // namespace

extern "C" {

int asm_mesh_fedavg_aggregate(void* modelWeights, uint32_t weightCount, uint32_t nodeCount)
{
    if (modelWeights == nullptr || nodeCount == 0)
    {
        return -1;
    }
    float* weights = static_cast<float*>(modelWeights);
    const float divisor = static_cast<float>(nodeCount);
    for (uint32_t i = 0; i < weightCount; ++i)
    {
        weights[i] /= divisor;
    }
    g_fedAvgCount.fetch_add(1, std::memory_order_relaxed);
    return 1;
}

void asm_mesh_gossip_disseminate(const void* message, uint32_t msgLen, uint32_t ttl)
{
    (void)message;
    (void)msgLen;
    (void)ttl;
    g_gossipCount.fetch_add(1, std::memory_order_relaxed);
}

uint64_t asm_mesh_shard_hash(const void* data, uint32_t dataLen)
{
    if (data == nullptr || dataLen == 0)
    {
        return 0ULL;
    }
    return fnv1a64(data, dataLen);
}

uint32_t asm_mesh_shard_bitfield(uint32_t shardIndex)
{
    return (1u << (shardIndex & 31u));
}

int asm_mesh_quorum_vote(const void* proposal, uint32_t proposalLen, uint32_t* voteOut)
{
    (void)proposalLen;
    if (proposal == nullptr)
    {
        return -1;
    }
    if (voteOut != nullptr)
    {
        *voteOut = 1u;
    }
    return 1;
}

void asm_mesh_topology_update(uint32_t nodeId, uint32_t status)
{
    (void)nodeId;
    if (status == 1u)
    {
        g_activeNodes.fetch_add(1u, std::memory_order_relaxed);
    }
    else
    {
        uint32_t prev = g_activeNodes.load(std::memory_order_relaxed);
        while (prev > 0 && !g_activeNodes.compare_exchange_weak(
                prev, prev - 1u,
                std::memory_order_relaxed,
                std::memory_order_relaxed))
        {
        }
    }
}

uint32_t asm_mesh_topology_active_count(void)
{
    return g_activeNodes.load(std::memory_order_relaxed);
}

void asm_mesh_get_stats(void* statsOut)
{
    if (statsOut == nullptr)
    {
        return;
    }
    uint64_t* out = static_cast<uint64_t*>(statsOut);
    out[0] = g_gossipCount.load(std::memory_order_relaxed);
    out[1] = g_fedAvgCount.load(std::memory_order_relaxed);
    out[2] = static_cast<uint64_t>(g_activeNodes.load(std::memory_order_relaxed));
    out[3] = 0ULL; // reserved: quorum votes
    out[4] = 0ULL; // reserved: shard hash calls
    out[5] = 0ULL; // reserved: topology updates
    out[6] = 0ULL; // reserved: bytes disseminated
    out[7] = 0ULL; // reserved: aggregate rounds
}

void asm_mesh_shutdown(void)
{
    std::lock_guard<std::mutex> lock(g_meshMtx);
    g_activeNodes.store(0u, std::memory_order_relaxed);
    g_gossipCount.store(0ULL, std::memory_order_relaxed);
    g_fedAvgCount.store(0ULL, std::memory_order_relaxed);
}

} // extern "C"
