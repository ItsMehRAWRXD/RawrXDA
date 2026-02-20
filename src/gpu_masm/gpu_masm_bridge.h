#ifndef GPU_MASM_BRIDGE_H
#define GPU_MASM_BRIDGE_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ========================================
// GPU Backend Functions (gpu_backend.asm)
// ========================================

// Initialize GPU backend with preferred backend type
// backend: 0=CPU, 1=Vulkan, 2=CUDA, 3=ROCm, 4=LevelZero(Intel), 5=Adreno(ARM64), 6=Cerebras(WSE)
// Returns: 0 on success, -1 on failure
int64_t InitializeGPUBackend(int64_t backend);

// Shutdown GPU backend
void ShutdownGPUBackend();

// Get current backend type
// Returns: 0=CPU, 1=Vulkan, 2=CUDA, 3=ROCm, 4=LevelZero, 5=Adreno, 6=Cerebras
int64_t GetCurrentBackend();

// Check if backend is initialized
// Returns: 1 if initialized, 0 if not
int64_t IsBackendInitialized();

// ========================================
// GPU Detection Functions (gpu_detection.asm)
// ========================================

// GPU Device Structure (matches MASM GPU_DEVICE struct)
struct GpuDeviceInfo {
    uint16_t vendorId;
    uint16_t deviceId;
    uint32_t classCode;
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
    uint64_t memorySize;
    uint32_t computeCapability;
    uint32_t clockSpeedMHz;      // GPU core clock speed in MHz
    uint32_t computeUnits;       // NVIDIA: SM count, AMD: CU count, Intel: EU count
    uint32_t memoryClockMHz;     // Memory clock speed in MHz
    uint32_t memoryBusWidth;     // Memory bus width in bits
    char deviceName[256];
};

// Initialize GPU detection system
// Returns: 0 on success, -1 on failure
int32_t GPU_Initialize();

// Detect all GPUs in the system
// Returns: Number of GPUs detected
int32_t GPU_Detect();

// Get number of detected GPUs
// Returns: GPU count
int32_t GPU_GetDeviceCount();

// Get device information for specific GPU
// index: GPU index (0-based)
// pDevice: Pointer to GpuDeviceInfo structure to fill
// Returns: 0 on success, -1 on failure
int32_t GPU_GetDevice(int32_t index, GpuDeviceInfo* pDevice);

// Shutdown GPU detection system
void GPU_Shutdown();

// ========================================
// GPU Memory Functions (gpu_memory.asm)
// ========================================

// Allocate GPU memory
// size: Number of bytes to allocate
// Returns: Pointer to GPU memory, or NULL on failure
void* gpu_malloc(uint64_t size);

// Free GPU memory
// ptr: Pointer to GPU memory to free
// Returns: 0 on success, -1 on failure
int64_t gpu_free(void* ptr);

// Copy data from host to GPU
// dst: GPU memory destination
// src: Host memory source
// size: Number of bytes to copy
// Returns: 0 on success, -1 on failure
int64_t gpu_memcpy_host_to_device(void* dst, const void* src, uint64_t size);

// Copy data from GPU to host
// dst: Host memory destination
// src: GPU memory source
// size: Number of bytes to copy
// Returns: 0 on success, -1 on failure
int64_t gpu_memcpy_device_to_host(void* dst, const void* src, uint64_t size);

// Get total GPU memory
// Returns: Total GPU memory in bytes
uint64_t gpu_get_total_memory();

// Get available GPU memory
// Returns: Available GPU memory in bytes
uint64_t gpu_get_available_memory();

// ========================================
// GPU Kernel Functions (gpu_kernels.asm)
// ========================================

// Launch matrix multiplication kernel
// A: Matrix A pointer (GPU memory)
// B: Matrix B pointer (GPU memory)
// C: Result matrix pointer (GPU memory)
// M: Matrix A rows
// N: Matrix B columns
// K: Matrix A columns / Matrix B rows
// Returns: 0 on success, -1 on failure
int64_t launch_matmul_kernel(void* A, void* B, void* C, int64_t M, int64_t N, int64_t K);

// Launch vector addition kernel
// a: Vector A pointer (GPU memory)
// b: Vector B pointer (GPU memory)
// result: Result vector pointer (GPU memory)
// size: Number of elements
// Returns: 0 on success, -1 on failure
int64_t launch_vector_add_kernel(void* a, void* b, void* result, int64_t size);

// Launch element-wise multiplication kernel
// a: Vector A pointer (GPU memory)
// b: Vector B pointer (GPU memory)
// result: Result vector pointer (GPU memory)
// size: Number of elements
// Returns: 0 on success, -1 on failure
int64_t launch_element_mul_kernel(void* a, void* b, void* result, int64_t size);

// Synchronize GPU execution (wait for all kernels to complete)
// Returns: 0 on success, -1 on failure
int64_t gpu_synchronize();

// ========================================
// Vulkan Backend Functions (vk_instance.asm)
// ========================================

// Initialize Vulkan instance
// Returns: 0 on success, -1 on failure
int64_t VK_CreateInstance();

// Destroy Vulkan instance
void VK_DestroyInstance();

// Enumerate Vulkan physical devices
// Returns: Number of devices found
int32_t VK_EnumeratePhysicalDevices();

// Get Vulkan device properties
// deviceIndex: Index of device
// Returns: Pointer to device properties structure
void* VK_GetPhysicalDeviceProperties(int32_t deviceIndex);

// ========================================
// CUDA Backend Functions (cuda_api.asm)
// ========================================

// Initialize CUDA runtime
// Returns: 0 on success, -1 on failure
int64_t CUDA_Initialize();

// Get CUDA device count
// Returns: Number of CUDA devices
int32_t CUDA_GetDeviceCount();

// Set current CUDA device
// deviceId: Device index
// Returns: 0 on success, -1 on failure
int64_t CUDA_SetDevice(int32_t deviceId);

// Get CUDA device properties
// deviceId: Device index
// Returns: Pointer to device properties structure
void* CUDA_GetDeviceProperties(int32_t deviceId);

// ========================================
// Phase 30: Accelerator Router Fast-Path (RawrXD_RouterBridge.asm)
// ========================================
// MASM fast-path entry points for the unified accelerator router.
// These bypass C++ dispatch overhead for hot-loop inference.
// Backend types: 0=None, 1=AMD_XDNA, 2=Intel_Xe, 3=ARM64_Adreno,
//                4=ARM64_NPU, 5=Cerebras_WSE, 6=CPU_Fallback, 255=Auto

// Initialize the fast-path router bridge (one-time setup)
// Returns: Router handle pointer, or NULL on failure
void* Router_FastInit();

// Shutdown the router bridge and clear cached state
void Router_FastShutdown();

// Hot-path inference dispatch — minimal overhead entry point
// task: Pointer to RouterInferenceTask struct
// result: Pointer to RouterResult struct (output)
// Returns: 0 on success, negative error code on failure
int64_t Router_FastSubmit(const void* task, void* result);

// Get cached active backend (no mutex, O(1))
// Returns: Backend type ID (see constants above)
int32_t Router_FastGetBackend();

// Force a specific backend via fast path
// backendType: Backend type ID to force
void Router_FastForceBackend(int32_t backendType);

// Check backend availability (O(1) lookup)
// backendType: Backend type ID to check
// Returns: 1 if available, 0 if not
int32_t Router_FastIsAvailable(int32_t backendType);

// Get total fast-path dispatch count
// Returns: 64-bit dispatch count
int64_t Router_FastGetDispatchCount();

#ifdef __cplusplus
}
#endif

#endif // GPU_MASM_BRIDGE_H
