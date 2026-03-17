// Minimal link shims for RawrEngine / Gold / InferenceEngine.
// These are no-op fallbacks to satisfy references after stub purge.
#include <cstdint>

extern "C" {
int64_t Scheduler_SubmitTask(void*, void*, uint8_t, uint8_t, void*) { return 0; }
void* AllocateDMABuffer(uint64_t, uint32_t) { return nullptr; }
uint32_t CalculateCRC32(const void*, uint64_t) { return 0; }
int Heartbeat_AddNode(const char*, uint32_t) { return 0; }
int Tensor_QuantizedMatMul(const void*, const void*, void*, uint32_t) { return 0; }
int ConflictDetector_LockResource(uint32_t) { return 0; }
int GPU_WaitForDMA(uint32_t) { return 0; }

// Pyre compute kernels
int asm_pyre_gemm_fp32(const void*, const void*, void*, int, int, int) { return 0; }
int asm_pyre_rmsnorm(void*, const void*, int) { return 0; }
int asm_pyre_silu(void*, const void*, int) { return 0; }
int asm_pyre_rope(void*, const void*, int) { return 0; }
int asm_pyre_embedding_lookup(const void*, const void*, void*, int, int) { return 0; }
int asm_pyre_gemv_fp32(const void*, const void*, void*, int, int) { return 0; }
int asm_pyre_add_fp32(void*, const void*, const void*, int) { return 0; }
}
