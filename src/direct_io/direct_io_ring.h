// src/direct_io/direct_io_ring.h
#pragma once
#include <cstdint>
#include <vector>

// RAWRXD v1.1.0 Unified I/O Interface
// Supporting Tier-7 Quantum Bursting

struct DirectIOContext;

struct IORequest {
    uint64_t file_offset;
    size_t size;
    uint32_t zone_index;
    uint32_t zone_offset;
    uint64_t request_id;
};

struct IOCompletion {
    uint64_t request_id;
    int32_t result_code;
};

// C-API for MASM Scheduler
extern "C" {
    bool DirectIO_Init(DirectIOContext** ctx, const char* filepath);
    bool DirectIO_Prefetch(DirectIOContext* ctx, uint64_t offset, size_t size, void* dst);
    int  DirectIO_Poll(DirectIOContext* ctx);
    int  DirectIO_GetPendingCount(DirectIOContext* ctx);
    void DirectIO_Shutdown(DirectIOContext* ctx);
}

// Metadata interface for Planner
extern "C" {
    uint64_t GetTensorOffset(uint32_t tensor_id);
    uint64_t GetTensorSize(uint32_t tensor_id);
    void* ResolveZonePointer(uint32_t zone_index);
    uint32_t GetBurstCount();
    uint32_t* GetBurstPlan();
}

// Swarm / Hypervisor state
extern "C" {
    extern DirectIOContext* g_pDirectIOCtx;
    extern void* g_zoneBuffer;
    extern uint64_t g_BurstTick;
}
