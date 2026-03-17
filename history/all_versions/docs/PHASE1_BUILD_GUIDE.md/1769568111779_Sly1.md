# Phase 1 Foundation - Integration & Build Guide

## Quick Start

### For Developers

```cpp
#include "Phase1_Foundation.h"
using namespace Phase1;

int main() {
    // Single-line initialization
    Foundation::Initialize();
    
    // Query capabilities
    printf("Cores: %d\n", PHASE1_CORES());
    
    // Allocate memory
    void* buffer = PHASE1_MALLOC(1024);
    
    // Measure time
    uint64_t start = PHASE1_CYCLES();
    // ... do work ...
    printf("Time: %.2f ms\n", PHASE1_MILLIS());
    
    return 0;
}
```

## Build Instructions

### Option 1: PowerShell Script (Recommended)

```powershell
# Navigate to project root
cd D:\rawrxd

# Simple build
.\scripts\Build-Phase1.ps1

# Release build with verbose output
.\scripts\Build-Phase1.ps1 -Release -Verbose

# Clean rebuild
.\scripts\Build-Phase1.ps1 -Clean -Release
```

### Option 2: Manual Command Line

```bash
# Compile assembly
ml64.exe /c /O2 /Zi /W3 /nologo ^
  D:\rawrxd\src\foundation\Phase1_Master.asm

# Compile C++
cl.exe /c /O2 /W4 /Zi /std:c++17 /arch:AVX2 ^
  /I D:\rawrxd\include ^
  D:\rawrxd\src\foundation\Phase1_Foundation.cpp

# Link library
lib.exe /OUT:Phase1_Foundation.lib ^
  Phase1_Master.obj Phase1_Foundation.obj
```

### Option 3: CMake

```bash
# From project root
mkdir build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_PHASE1_TESTS=ON
cmake --build . --target Phase1_Foundation

# Run tests
ctest -C Release -V
```

## File Structure

```
D:\rawrxd\
├── include\
│   ├── Phase1_Foundation.h              # Main C++ header
│   ├── Phase1_Integration_Examples.h    # Examples for phases 2-5
│   └── foundation\
│       └── (future sub-headers)
├── src\
│   └── foundation\
│       ├── Phase1_Master.asm            # x64 assembly implementation
│       └── Phase1_Foundation.cpp        # C++ wrappers
├── docs\
│   └── PHASE1_FOUNDATION.md             # Architecture documentation
├── scripts\
│   └── Build-Phase1.ps1                 # Build automation
├── test\
│   └── Phase1_Test.cpp                  # Validation tests
├── lib\
│   └── Phase1_Foundation.lib            # Built library (after build)
└── CMakeLists_Phase1.txt                # CMake configuration
```

## Verification

### Run Tests

```powershell
# Build with tests enabled
.\scripts\Build-Phase1.ps1

# Run test executable
.\build\phase1\Phase1_Test.exe
```

Expected output:
```
================================================================================
Phase 1 Foundation - Initialization Test
================================================================================

[1/5] Initializing Phase 1 Foundation...
✓ Phase 1 initialized successfully

[2/5] Querying CPU Capabilities...
  CPU Vendor:           GenuineIntel
  CPU Brand:            Intel(R) Core(TM) i9-...
  Physical Cores:       16
  Logical Threads:      32
  [... more details ...]

[5/5] Testing Performance Timing...
  TSC Cycles (busy loop):  1234567890
  Elapsed Time:            0.50 seconds

✓ ALL TESTS PASSED - Phase 1 Foundation is operational
```

## Integration with Phase 2-5

### Phase 2: Model Loader

```cpp
#include "Phase1_Foundation.h"
#include "Phase1_Integration_Examples.h"

using namespace Phase1;
using namespace Phase2_ModelLoader;

void SetupModelLoader() {
    ModelLoader loader;
    
    // Automatically uses Phase 1 for:
    // - Memory allocation (PHASE1_MALLOC)
    // - CPU capability detection (PHASE1_HAS_AVX512)
    // - Core count queries (PHASE1_CORES)
    
    auto model = loader.LoadModel("model.onnx");
}
```

### Phase 3: Agent Kernel

```cpp
#include "Phase1_Integration_Examples.h"

using namespace Phase3_AgentKernel;

void SetupAgents() {
    AgentKernel kernel;
    
    // Kernel automatically uses Phase 1 for:
    // - Per-thread memory (PHASE1_MALLOC)
    // - Cycle-accurate timing (PHASE1_CYCLES)
    // - Decision latency measurement
    
    kernel.BatchReasoningStep(PHASE1_THREADS());
}
```

### Phase 4: Swarm Inference

```cpp
#include "Phase1_Integration_Examples.h"

using namespace Phase4_SwarmInference;

void SetupSwarm() {
    SwarmInferenceEngine swarm(4);  // 4 models
    
    // Swarm uses Phase 1 for:
    // - NUMA-aware allocation (PHASE1_NUMA_MALLOC)
    // - Performance tracking (PHASE1_MICROS)
    // - Dynamic load balancing
    // - Core count auto-detection (PHASE1_CORES)
    
    swarm.LoadModel(0, "model1.onnx", 0);
    swarm.LoadModel(1, "model2.onnx", 1);
}
```

### Phase 5: Orchestrator

```cpp
#include "Phase1_Integration_Examples.h"

using namespace Phase5_Orchestrator;

void SetupOrchestrator() {
    SwarmOrchestrator orchestrator;
    
    // Orchestrator uses Phase 1 for:
    // - Resource capacity calculation (PHASE1_THREADS)
    // - Frame timing (PHASE1_MICROS)
    // - NUMA node awareness (GetNUMANodeCount)
    // - System-wide performance monitoring
    
    while (true) {
        orchestrator.OrchestrationFrame();
    }
}
```

## Troubleshooting

### Build Errors

**Error: `ml64.exe not found`**
- Solution: Run from Visual Studio Developer Command Prompt or call `Build-Phase1.ps1`

**Error: `cl.exe not found`**
- Solution: Ensure Visual Studio 2022 with C++ support is installed

**Error: `LINK : fatal error LNK1104`**
- Solution: Make sure object files (.obj) were created successfully before linking

### Runtime Errors

**Error: `AVX not supported`**
- This is a fatal error - Phase 1 requires AVX-capable CPU
- The system running RawrXD must have an Intel/AMD CPU with AVX support (released 2011+)

**Error: `Memory allocation failed`**
- Check available system memory
- Verify no other applications are using excessive memory
- Check Windows Virtual Memory settings

### Performance Issues

**Slow initialization**
- CPUID detection and TSC calibration take 50-100ms - this is normal
- Subsequent operations are very fast (O(1) for most operations)

**Inconsistent timing**
- Use `PHASE1_CYCLES()` for cycle-accurate timing (preferred)
- Use `PHASE1_MICROS()` for wall-clock timing
- Avoid mixing timing sources

## Performance Benchmarks

Typical performance on Intel Core i9 (12 cores, 3.9 GHz):

| Operation | Latency | Notes |
|-----------|---------|-------|
| Initialization | 50-100ms | One-time startup cost |
| Memory allocation | <1µs | O(1) operation |
| NUMA allocation | <1µs | O(1) per NUMA node |
| TSC read | ~10 cycles | Serialized RDTSC |
| QPC read | ~100 cycles | System call |
| Context query | <1µs | Zero-copy access |

## Memory Usage

Static allocations:
- `PHASE1_CONTEXT`: 8 KB
- Per NUMA node arena structures: ~512 bytes
- Reserved virtual address space: 1 TB (not physically committed)

Dynamic allocations (on first use):
- System arena commits: Lazy on allocation
- Typical committed pages: 2-10 MB at startup

## Deployment Checklist

- [ ] Phase1_Master.asm compiled to .obj
- [ ] Phase1_Foundation.cpp compiled to .obj
- [ ] Phase1_Foundation.lib created
- [ ] Phase1_Foundation.h in include path
- [ ] All phases linked against Phase1_Foundation.lib
- [ ] Test executable runs successfully
- [ ] CPU has AVX support (required)
- [ ] At least 100 MB available memory
- [ ] Kernel32.lib and ntdll.lib available

## Future Enhancements

Phase 1 v2.0 planned improvements:
- [ ] Lock-free allocators for ultra-low-latency scenarios
- [ ] Persistent memory (PMEM) support
- [ ] GPU memory integration (CUDA/HIP)
- [ ] Real-time priority thread support
- [ ] CPU frequency scaling awareness
- [ ] More detailed NUMA topology support
- [ ] Arm64 architecture support

## Support & Debugging

### Enable Verbose Logging

```cpp
// During initialization
Foundation::Initialize(PHASE1_VERBOSE_LOGGING);
```

### Query Current State

```cpp
auto ctx = PHASE1_CTX();
printf("Initialized: %s\n", ctx->IsInitialized() ? "yes" : "no");
printf("Memory committed: %llu bytes\n", ctx->system_arena.current_offset);
printf("Last error: %u\n", ctx->last_error_code);
```

### Profile Performance

```cpp
uint64_t start = PHASE1_CYCLES();
// ... operation ...
uint64_t elapsed = PHASE1_CYCLES() - start;
printf("Operation took %.2f µs\n", elapsed / (PHASE1().GetTSCFrequency() / 1e6));
```

## Version Information

- **Phase 1 Version**: 1.0 (Foundation Release)
- **Target Platform**: Windows x64 (Windows 10+)
- **Minimum CPU**: Any Intel/AMD with AVX support (Sandy Bridge/Bulldozer era, 2011+)
- **Compiler Support**: MSVC 2022, Intel ICC
- **Language Support**: C/C++ (x64 assembly backend)

## Architecture Overview

```
Your Application
      │
      ▼
┌─────────────────────────────────┐
│   Phase 5: Orchestrator         │
│   (Task coordination)           │
└──────────────┬──────────────────┘
               │
┌──────────────▼──────────────────┐
│   Phase 4: Swarm Inference      │
│   (Model execution)             │
└──────────────┬──────────────────┘
               │
┌──────────────▼──────────────────┐
│   Phase 3: Agent Kernel         │
│   (Reasoning)                   │
└──────────────┬──────────────────┘
               │
┌──────────────▼──────────────────┐
│   Phase 2: Model Loader         │
│   (Model management)            │
└──────────────┬──────────────────┘
               │
┌──────────────▼──────────────────────────────────┐
│   Phase 1: Foundation (You are here)            │
│   • Hardware detection                          │
│   • Memory management (NUMA-aware)              │
│   • Synchronization primitives                  │
│   • Performance monitoring                      │
│   • Thread pool infrastructure                  │
└─────────────────────────────────────────────────┘
               │
┌──────────────▼──────────────────────────────────┐
│   Windows Kernel (64-bit)                       │
│   • Virtual memory management                   │
│   • Thread scheduling                           │
│   • I/O completion ports                        │
└─────────────────────────────────────────────────┘
```

## License & Attribution

Phase 1 Foundation is part of the RawrXD project.
Built with x64 assembly for maximum performance and hardware awareness.

---

**Ready to build?** Start with:
```powershell
.\scripts\Build-Phase1.ps1 -Release -Verbose
```

