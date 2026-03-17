#pragma once

#include <cstdint>
#include <cstddef>

// ============================================================================
// CATEGORY 4: extern "C" ASM Bindings (~25 symbols)
// Pure C-style declarations for low-level assembly functions
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DMA Operations
// ============================================================================

/**
 * Allocate a DMA-capable buffer for GPU transfers
 * @param size_bytes Number of bytes to allocate
 * @return Pointer to allocated DMA buffer, NULL on failure
 */
void* AllocateDMABuffer(uint64_t size_bytes);

/**
 * Submit a DMA transfer operation to GPU
 * @param src_buffer Source buffer pointer
 * @param dst_buffer Destination buffer pointer
 * @param size_bytes Number of bytes to transfer
 * @return DMA transfer ID, 0 on failure
 */
uint64_t GPU_SubmitDMATransfer(const void* src_buffer, void* dst_buffer, uint64_t size_bytes);

/**
 * Wait for a DMA transfer to complete
 * @param transfer_id DMA transfer ID from SubmitDMATransfer
 * @param timeout_ms Timeout in milliseconds, 0 for infinite
 * @return Non-zero on success, 0 on timeout/failure
 */
int GPU_WaitForDMA(uint64_t transfer_id, uint32_t timeout_ms);

// ============================================================================
// CRC32 Calculation
// ============================================================================

/**
 * Calculate CRC32 checksum of data
 * @param data Pointer to data buffer
 * @param length Number of bytes to process
 * @param initial_crc Initial CRC value (typically 0)
 * @return Calculated CRC32 value
 */
uint32_t CalculateCRC32(const void* data, uint64_t length, uint32_t initial_crc);

// ============================================================================
// Conflict Detector
// ============================================================================

/**
 * Initialize the conflict detection subsystem
 * @return Non-zero on success
 */
int ConflictDetector_Initialize(void);

/**
 * Register a resource for conflict tracking
 * @param resource_id Unique resource identifier
 * @param resource_size Size of the resource in bytes
 * @return Resource handle, 0 on failure
 */
uint64_t ConflictDetector_RegisterResource(uint64_t resource_id, uint64_t resource_size);

/**
 * Lock a resource for exclusive access
 * @param resource_handle Handle from RegisterResource
 * @param lock_timeout_us Timeout in microseconds
 * @return Non-zero on success (lock acquired)
 */
int ConflictDetector_LockResource(uint64_t resource_handle, uint32_t lock_timeout_us);

/**
 * Unlock a previously locked resource
 * @param resource_handle Handle from RegisterResource
 * @return Non-zero on success
 */
int ConflictDetector_UnlockResource(uint64_t resource_handle);

// ============================================================================
// High Resolution Timing
// ============================================================================

/**
 * Get current high-resolution tick counter
 * @return Current tick value
 */
uint64_t GetHighResTick(void);

/**
 * Convert high-resolution ticks to microseconds
 * @param ticks Tick value from GetHighResTick
 * @return Microseconds
 */
uint64_t TicksToMicroseconds(uint64_t ticks);

/**
 * Convert high-resolution ticks to milliseconds
 * @param ticks Tick value from GetHighResTick
 * @return Milliseconds
 */
uint64_t TicksToMilliseconds(uint64_t ticks);

// ============================================================================
// Heartbeat/Monitoring
// ============================================================================

/**
 * Initialize the heartbeat monitoring subsystem
 * @param interval_ms Heartbeat interval in milliseconds
 * @return Non-zero on success
 */
int Heartbeat_Initialize(uint32_t interval_ms);

/**
 * Add a node to heartbeat monitoring
 * @param node_id Unique node identifier
 * @param node_name Name of the node (for logging)
 * @return Non-zero on success
 */
int Heartbeat_AddNode(uint32_t node_id, const char* node_name);

/**
 * Shutdown the heartbeat monitoring subsystem
 * @return Non-zero on success
 */
int Heartbeat_Shutdown(void);

// ============================================================================
// Task Scheduler
// ============================================================================

/**
 * Initialize the task scheduler subsystem
 * @param max_workers Maximum number of worker threads
 * @return Non-zero on success
 */
int Scheduler_Initialize(uint32_t max_workers);

/**
 * Submit a task to the scheduler
 * @param task_func Function pointer for the task
 * @param task_context Context data to pass to task
 * @param priority Task priority (0=lowest, 31=highest)
 * @return Task ID, 0 on failure
 */
uint64_t Scheduler_SubmitTask(void (*task_func)(void*), void* task_context, uint8_t priority);

/**
 * Wait for a scheduled task to complete
 * @param task_id Task ID from SubmitTask
 * @param timeout_ms Timeout in milliseconds, 0 for infinite
 * @param result_buffer Buffer to store task result
 * @param result_size Size of result buffer
 * @return Non-zero on success
 */
int Scheduler_WaitForTask(uint64_t task_id, uint32_t timeout_ms, 
                          void* result_buffer, uint32_t result_size);

/**
 * Shutdown the task scheduler subsystem
 * @return Non-zero on success
 */
int Scheduler_Shutdown(void);

// ============================================================================
// Tensor Operations
// ============================================================================

/**
 * Perform optimized quantized matrix multiplication
 * @param a Matrix A (MxK)
 * @param b Matrix B (KxN)
 * @param result Result matrix (MxN)
 * @param m Rows in A/Result
 * @param k Cols in A / Rows in B
 * @param n Cols in B/Result
 * @param quant_bits Quantization bits (8, 4, etc)
 * @return Non-zero on success
 */
int Tensor_QuantizedMatMul(const float* a, const float* b, float* result,
                           uint32_t m, uint32_t k, uint32_t n, uint8_t quant_bits);

// ============================================================================
// INFINITY Subsystem
// ============================================================================

/**
 * Shutdown the INFINITY subsystem
 * @return Non-zero on success
 */
int INFINITY_Shutdown(void);

// ============================================================================
// Windows NT API (required by some assembly modules)
// ============================================================================

/**
 * Query system information (Windows API)
 * @param info_class Information class to query
 * @param info_buffer Buffer for result
 * @param info_length Length of buffer
 * @param return_length Actual data length
 * @return NTSTATUS code
 */
int NtQuerySystemInformation(uint32_t info_class, void* info_buffer, 
                             uint32_t info_length, uint32_t* return_length);

/**
 * Get OS version information (Windows API)
 * @param version_info Pointer to version info structure
 * @param version_info_size Size of structure
 * @return NTSTATUS code
 */
int RtlGetVersion(void* version_info, uint32_t version_info_size);

#ifdef __cplusplus
}  // extern "C"
#endif
