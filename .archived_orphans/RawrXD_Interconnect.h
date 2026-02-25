#pragma once

// RawrXD Interconnect - Assembly Core Interface
// This header exposes the assembly-level primitives to C++ code

#ifdef __cplusplus
extern "C" {
#endif

// System Primitives
extern "C" void System_InitializePrimitives();
extern "C" void Spinlock_Acquire(volatile long* lock);
extern "C" void Spinlock_Release(volatile long* lock);
extern "C" void RWLock_AcquireRead(volatile long* lock);
extern "C" void RWLock_ReleaseRead(volatile long* lock);
extern "C" void RWLock_AcquireWrite(volatile long* lock);
extern "C" void RWLock_ReleaseWrite(volatile long* lock);
extern "C" void* Aligned_Allocate(size_t size, size_t alignment);
extern "C" void Aligned_Free(void* ptr);
extern "C" void Memory_PrefetchRead(void* address, int locality);
extern "C" void Thread_AffinitySet(void* threadHandle, int coreIndex);

// GPU Memory Management
extern "C" void Vram_Initialize(uint64_t totalSize);
extern "C" int64_t Vram_Allocate(uint64_t size, uint64_t alignment);
extern "C" void Vram_Free(uint64_t vramOffset);
extern "C" uint64_t Vram_SubmitUpload(void* hostPtr, uint64_t vramOffset, uint64_t size);
extern "C" int Vram_Defragment();

// Inference Engine
extern "C" int Inference_Initialize(void* modelHandle);
extern "C" int Inference_AllocateSequence(uint64_t sequenceId);
extern "C" void Inference_SubmitToken(int slotIndex, uint32_t tokenId, float* outputLogits);
extern "C" void Inference_SubmitBatch(int* slotIndices, int count, uint32_t* tokenIds, float** outputLogits);
extern "C" void Inference_ReleaseSequence(int slotIndex);
extern "C" int InferenceEngine_Submit(void* modelInstance, void* swarmJob);

// Complete Interconnect
extern "C" int RawrXD_InitializeAll(void* hWnd, uint64_t vramSize);
extern "C" void RawrXD_ShutdownAll();
extern "C" int RawrXD_SubmitChatRequest(void* session, const char* inputText, void (*callback)(const char*));
extern "C" void RawrXD_GetMetrics(void* metrics);

// Metrics structure
struct RawrXD_Metrics {
    uint64_t uptimeMs;
    uint64_t totalRequests;
    uint64_t tokensGenerated;
    uint32_t activeSequences;
    float avgLatencyUs;
    float vramFragmentation;
};

#ifdef __cplusplus
}
#endif

// C++ wrapper for convenience
namespace RawrXD {

class Interconnect {
public:
    static bool Initialize(void* hWnd = nullptr, uint64_t vramSize = 8ULL * 1024 * 1024 * 1024) {
        return RawrXD_InitializeAll(hWnd, vramSize) != 0;
    }
    
    static void Shutdown() {
        RawrXD_ShutdownAll();
    }
    
    static bool SubmitChatRequest(void* session, const std::string& inputText, 
                                  std::function<void(const std::string&)> callback) {
        // Convert std::function to C callback
        auto c_callback = [](const char* response) {
            // This would need to capture the std::function properly
            // For now, simplified implementation
        };
        return RawrXD_SubmitChatRequest(session, inputText.c_str(), c_callback) != 0;
    }
    
    static RawrXD_Metrics GetMetrics() {
        RawrXD_Metrics metrics;
        RawrXD_GetMetrics(&metrics);
        return metrics;
    }
};

// RAII wrapper for automatic cleanup
class InterconnectGuard {
public:
    InterconnectGuard(void* hWnd = nullptr, uint64_t vramSize = 8ULL * 1024 * 1024 * 1024) {
        if (!Interconnect::Initialize(hWnd, vramSize)) {
            throw std::runtime_error("Failed to initialize RawrXD Interconnect");
        }
    }
    
    ~InterconnectGuard() {
        Interconnect::Shutdown();
    }
    
    // Prevent copying
    InterconnectGuard(const InterconnectGuard&) = delete;
    InterconnectGuard& operator=(const InterconnectGuard&) = delete;
    
    // Allow moving
    InterconnectGuard(InterconnectGuard&&) = default;
    InterconnectGuard& operator=(InterconnectGuard&&) = default;
};

} // namespace RawrXD