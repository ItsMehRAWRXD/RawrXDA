# RawrXD Unified Memory Executor (AMD SAM Edition)

## Overview

The Unified Memory Executor provides **Apple Silicon-style unified memory semantics** on AMD RX 7800 XT (16GB) using **AMD Smart Access Memory (Resizable BAR)**. This enables CPU and GPU to share the same physical memory pool with zero-copy access—no PCIe transfers, no DMA staging, no separate VRAM/RAM boundaries.

## Architecture

### Key Features

- **Zero-Copy Model Loading**: GGUF files are mapped directly into unified memory
- **CPU/GPU Shared Pointers**: Same memory address valid on both CPU and GPU
- **Unified Heap**: 15GB of unified compute memory (16GB total - 1GB system reserve)
- **Hardware Coherency**: AMD SAM provides hardware-level memory coherency
- **Streaming Execution**: Models larger than VRAM can stream through unified memory

### Memory Layout

```
┌─────────────────────────────────────┐
│ 16GB BAR0 (GPU VRAM)                │
├─────────────────────────────────────┤
│ 1GB System Reserve                  │
├─────────────────────────────────────┤
│ 15GB Unified Heap                   │
│  ├─ Model Weights                   │
│  ├─ Activations                     │
│  ├─ KV Cache                        │
│  └─ Scratch Buffers                 │
└─────────────────────────────────────┘
```

## Requirements

### Hardware
- AMD RX 7800 XT (or compatible AMD GPU with 16GB+ VRAM)
- AMD Ryzen 5000+ or Intel 10th Gen+ CPU (Resizable BAR support)

### BIOS Settings
- `Above 4G Decoding` = **Enabled**
- `Re-Size BAR` = **Auto** (or **Enabled**)

### Software
- AMD Adrenalin 21.9.1+ driver
- Windows 10 20H2+ or Windows 11

### Verification

```powershell
# Check if SAM is enabled (PowerShell as Admin)
Get-WmiObject Win32_PnPEntity | Where-Object {$_.Name -like "*7800*"} | 
    Select-Object Name, @{N="BAR0";E={
        (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Enum\$($_.DeviceID)\Device Parameters" -Name "Bar0" -ErrorAction SilentlyContinue).Bar0
    }}
```

Alternatively, check GPU-Z or AMD Software: "Smart Access Memory" must show **"Enabled"**.

## IDE integration (RawrXD Win32)

1. Open **Settings** → **Open full Settings…** (property grid).
2. Under **AI / Model**, toggle **AMD Unified Memory (SAM)**.
3. Click **Apply** then **Save** (persists to `%APPDATA%\RawrXD\settings.json` as `amdUnifiedMemoryEnabled`).

When enabled, `Win32IDE::applySettings()` calls `AMDGPUAccelerator::enableUnifiedMemory()`, which starts the unified executor (hardware SAM when available, otherwise a **512 MiB host-backed** bump arena so development machines still work without a kernel driver). Toggle off to run `disableUnifiedMemory()` and release the arena / restore the accelerator memory pool.

## Usage

### Initialization

```cpp
#include "core/unified_memory_executor.h"

using namespace RawrXD::UnifiedMemory;

// Initialize unified memory executor
auto& executor = UnifiedMemoryExecutor::instance();
auto initResult = executor.initialize();

if (!initResult) {
    // Handle error
    switch (initResult.error()) {
        case UnifiedMemoryError::SAMNotEnabled:
            // SAM not enabled in BIOS
            break;
        case UnifiedMemoryError::BARMappingFailed:
            // Failed to map BAR0 (requires kernel driver)
            break;
        // ...
    }
}
```

### Model Loading (Zero-Copy)

```cpp
// Load model directly into unified memory
std::string modelPath = "D:\\models\\200b-unified.gguf";
uint64_t fileSize = 80ULL * 1024 * 1024 * 1024;  // 80GB

auto modelResult = executor.loadModelUnified(modelPath, fileSize);
if (modelResult) {
    UnifiedBuffer modelBuffer = modelResult.value();
    
    // modelBuffer.ptr is valid for both CPU and GPU
    // No copy needed - file is already in GPU-accessible memory
}
```

### Memory Allocation

```cpp
// Allocate unified buffer (CPU and GPU see same memory)
auto bufferResult = executor.allocate(1024 * 1024 * 1024, 64);  // 1GB, 64-byte aligned
if (bufferResult) {
    UnifiedBuffer buffer = bufferResult.value();
    
    // Use buffer.ptr on both CPU and GPU
    // CPU: float* data = static_cast<float*>(buffer.ptr);
    // GPU: __global float* data = (float*)buffer.ptr;
}
```

### Layer Execution

```cpp
// Execute layer with unified memory (zero-copy)
UnifiedBuffer input = /* ... */;
UnifiedBuffer output = /* ... */;

auto execResult = executor.executeLayerUnified(
    layerIndex,  // Layer 0, 1, 2, ...
    input,       // Input activations (unified)
    output       // Output activations (unified)
);
```

### Streaming Execution

```cpp
// Execute model layer-by-layer (streaming through unified memory)
auto streamResult = executor.streamingExecutorUnified();

// For 80GB model on 16GB VRAM:
// - Model streams through unified memory
// - No separate VRAM vs RAM - just unified compute memory
// - Automatic CPU/GPU paging at PCIe speed
```

### Heterogeneous Execution

```cpp
// CPU + GPU simultaneous execution (Apple-style)
auto heteroResult = executor.heterogeneousScheduler();

// Automatically splits work:
// - Attention layers → GPU (compute bound)
// - Embedding/Norm → CPU (memory bound, sequential)
```

## Integration with AMD GPU Accelerator

```cpp
#include "core/amd_gpu_accelerator.h"

// Enable unified memory mode in AMD accelerator
auto& amdAccel = AMDGPUAccelerator::instance();
auto unifiedResult = amdAccel.enableUnifiedMemory();

if (unifiedResult.success) {
    // GPU allocations now use unified memory
    GPUBuffer buffer;
    amdAccel.allocGPU(1024 * 1024, buffer);
    
    // buffer.devicePtr == buffer.hostPtr (unified memory)
    // No copy needed between CPU and GPU
}
```

## Performance Characteristics

### Traditional (Discrete GPU)
```
Disk → RAM → PCIe → VRAM → Compute → PCIe → RAM → Disk
(4 copies, 2 PCIe transfers)
```

### Unified (AMD SAM)
```
Disk → Unified → Compute → Unified → Disk
(2 copies, 0 transfers - same physical memory)
```

### Bandwidth Comparison

| Architecture | CPU↔GPU Bandwidth | Zero-Copy |
|-------------|-------------------|-----------|
| Traditional PCIe | 64 GB/s (PCIe 4.0 x16) | ❌ |
| AMD SAM | Same memory domain | ✅ |
| Apple Silicon | 800 GB/s | ✅ |

## Implementation Details

### Assembly Layer

Low-level operations are implemented in assembly (`src/asm/RawrXD_Unified_Memory_Executor.asm`):

- **PCI Configuration Space Access**: `ReadPciConfigAMD()` reads BAR0 from PCI config
- **Physical Memory Mapping**: `MmMapIoSpaceEx()` maps BAR0 to CPU address space (requires kernel driver)
- **Coherency Testing**: `TestUnifiedCoherency()` verifies CPU/GPU memory coherency
- **GPU Initialization**: `HipInitUnifiedMemory()` initializes ROCm/HIP with unified memory base

### Kernel Driver Requirement

**Note**: Direct physical memory mapping requires a kernel-mode driver. The current implementation includes placeholder functions that would interface with such a driver via IOCTL.

For production use, you would need:
1. A Windows kernel driver that can map physical memory
2. IOCTL interface to request BAR0 mapping
3. Driver returns virtual address to user-mode process

Alternatively, use AMD's official APIs if available:
- AMD Memory Management APIs
- ROCm/HIP unified memory support
- DirectML unified memory (if supported)

## Error Handling

All functions return `std::expected<T, UnifiedMemoryError>`:

```cpp
enum class UnifiedMemoryError {
    Success = 0,
    SAMNotEnabled,          // Resizable BAR not enabled in BIOS
    BARMappingFailed,       // Failed to map BAR0 to CPU address space
    InsufficientMemory,     // Not enough unified memory available
    InvalidParameter,       // Invalid function parameter
    AlreadyInitialized,     // Unified memory already initialized
    NotInitialized,         // Unified memory not initialized
    AllocationFailed,       // Memory allocation failed
    ModelLoadFailed,        // Failed to load model into unified memory
    CoherencyTestFailed     // Memory coherency test failed
};
```

## Statistics

```cpp
auto stats = executor.getStats();
// stats.totalAllocatedBytes
// stats.peakAllocatedBytes
// stats.allocationCount
// stats.modelLoadCount
// stats.layerExecutionCount
```

## Future Enhancements

1. **Multi-GPU Unified**: Extend to 4x RX 7800 XT = 60GB unified space
2. **NVMe → Unified Direct**: Bypass OS page cache entirely
3. **Full Heterogeneous Scheduler**: CPU attention heads + GPU FFN
4. **Production Kernel Driver**: Complete BAR0 mapping implementation
5. **ROCm/HIP Integration**: Full ROCm unified memory support

## See Also

- `src/core/unified_memory_executor.h` - Header file
- `src/core/unified_memory_executor.cpp` - C++ implementation
- `src/asm/RawrXD_Unified_Memory_Executor.asm` - Assembly layer
- `src/core/amd_gpu_accelerator.h` - AMD GPU accelerator integration
