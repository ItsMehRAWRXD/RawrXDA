#pragma once

// Phase 27 - ZENITH: Omniscient-Sovereign MoE Engine
// 800B Out-of-Core Execution via Aggressive Mixture-of-Experts Router
// Bypassing MSVC SDK dependencies.

typedef unsigned long long UINT64;
typedef unsigned int       UINT32;
typedef unsigned short     UINT16;
typedef unsigned char      UINT8;
typedef int                BOOL;
typedef void*              HANDLE;

#define NULL_PTR ((void*)0)
#define TRUE 1
#define FALSE 0

#define SHARD_2BIT_QUANT (0x02)
#define MAX_ACTIVE_EXPERTS (4) // 16GB VRAM limit allows ~4 8B experts at 2-bit
#define KV_COMPRESSION_RATIO (0x08) // 8x Spatial Holographic Compression

struct ExpertDescriptor {
    UINT32 ExpertID;
    UINT64 DiskOffset;
    UINT64 ByteSize;
    HANDLE FileHandle;
    void*  VramDestPtr;
};

struct KVCacheHolo {
    UINT64 SequenceID;
    UINT64 RawTokenCount;
    UINT64 CompressedByteSize;
    void*  L2Ddr5Buffer;
};

class ZenithMoERouter {
public:
    ZenithMoERouter();
    ~ZenithMoERouter();

    // The answer to workstation scalability: Only stage what is needed per-token.
    BOOL RouteTokenToExperts(UINT64 TokenID, ExpertDescriptor* OutExperts, UINT32& ExpertCount);
    
    // Asynchronous NVMe -> VRAM using FILE_FLAG_NO_BUFFERING & Overlapped I/O
    BOOL DispatchExpertStreaming(ExpertDescriptor* Experts, UINT32 Count);

    // KV Cache Semantic folding for the "Infinite" Context illusion
    BOOL FoldKVCache(KVCacheHolo* CacheSegment);
    
private:
    void* m_OverlappedPool; // Pinned memory pool for IOCP
    UINT64 m_TotalRoutedTokens;
};
