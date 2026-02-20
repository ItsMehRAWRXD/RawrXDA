#include <windows.h>
#include <vector>

// --- Exports provided to ASM ---
extern "C" void LoadTensorBlock(int token_id, int slab_id) {
    // printf("[HOST] Prefetch Token %d (Slab %d)\n", token_id, slab_id);
    // In production, this issues an IoRing read
}

extern "C" void DispatchComputeStage(int buffer_id) {
    // printf("[HOST] Compute Dispatch (Buffer %d)\n", buffer_id);
    // In production, this fires Vulkan/CUDA
}

// --- Imports from ASM ---
extern "C" void GhostDispatchToken(int token_id);
extern "C" void GhostPrefetchStart();

int main() {


    // Initialize
    GhostPrefetchStart();


    // Simulate Streaming Generation
    const int TEST_TOKENS = 100; // 0.7 t/s would take a while, let's burst


    DWORD start = GetTickCount();
    
    for (int i = 0; i < TEST_TOKENS; i++) {
        // Linear scan to force prefetch behavior
        GhostDispatchToken(i * 256); // Jump slabs to force misses
    }
    
    DWORD end = GetTickCount();


    return 0;
}
