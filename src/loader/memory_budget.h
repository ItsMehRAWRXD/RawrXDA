#pragma once

#include <cstddef>
#include <cstdint>

namespace RawrXD
{

// Simple resident-set budget for tensor zones.
// Bunny-hop policy is implemented at the GGufTensorLoader level (load one zone, evict others).
struct MemoryBudget
{
    // Maximum resident bytes across all loaded zones (soft cap; loader will evict to satisfy).
    uint64_t maxResidentBytes = 2ull * 1024ull * 1024ull * 1024ull;  // 2 GiB default

    // Maximum bytes per zone load request (StreamingGGUFLoader has its own per-zone budget in MB).
    uint64_t maxZoneBytes = 512ull * 1024ull * 1024ull;  // 512 MiB default

    // If true, loader will keep only one zone resident at a time (strong bunny-hop).
    bool singleZoneResident = true;
};

}  // namespace RawrXD
