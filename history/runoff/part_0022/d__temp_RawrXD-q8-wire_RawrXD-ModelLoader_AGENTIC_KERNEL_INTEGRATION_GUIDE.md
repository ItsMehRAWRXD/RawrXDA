# RawrXD Agentic IDE - Complete Integration Guide

## Overview

This document describes the complete RawrXD Agentic IDE pipeline, which integrates:
- **Phase-1**: CommandRegistry routing and capability-based execution
- **Phase-2**: Neural prediction kernel (C++) + autonomous consumer (MASM64)
- **Phase-3**: Zero-copy producer, hotpatching engine, and integration bridges

## Architecture Layers

```
User Input (Win32 Menu/Keyboard)
    ↓
Win32IDE.cpp - WM_COMMAND Handler
    ↓
CommandRegistry::execute() - Capability validation
    ↓
IntelligentCommandRouter - Priority scheduling + deadline calc
    ↓
AutonomousCommandKernel - Priority queue dispatch (8 worker threads)
    ├→ SpeculativeEngine - Async pre-execution with thermal awareness
    ├→ ThermalMonitor - COOL/WARM/HOT/CRITICAL zone tracking
    └→ LSTMCell - Neural command sequence prediction
    ↓
[DUAL PATH]
├─ Speculative Result (pre-computed)
└─ Live Execution Path
    ↓
MmfProducer_Phase3 (MASM) - Zero-copy ring buffer batching
    ↓
MMF Ring Buffer (64-byte tool tokens, 1MB capacity)
    ↓
AgentKernel_Phase2 (MASM) - Consumer state machine
    ├→ Poll MMF for tool tokens
    ├→ Parse capability flags
    ├→ Thermal throttling
    └→ Dispatch to tool handlers
    ↓
HotpatchEngine_Phase3 (MASM) - Zero-downtime x64 detours
    ├→ Thread suspension & atomic CAS
    ├→ Shadow page management
    └→ Tool invocation patching
    ↓
Results → IDELogger (structured telemetry)
```

## Component Responsibilities

### Phase-1: CommandRegistry Integration

**File**: `src/win32app/Win32IDE.cpp`

**Changes**:
```cpp
// onCreate()
CommandRegistry::instance().initialize(hwnd);

// onDestroy()
IDELogger::info("CommandRegistry shutting down");

// WM_COMMAND (pre-wired)
routeCommandViaRegistry();  // Already exists
```

**Command ID Ranges**:
- 1000-1999: File operations
- 2000-2999: Edit operations
- 3000-3999: View operations
- 4000-4999: Go operations
- 5000-5999: Run operations (REALTIME priority)
- 6000-6999: Terminal operations
- 7000-7999: Help operations

### Phase-2C: PredictiveCommandKernel (C++)

**File**: `src/agentic/kernel/RawrXD_PredictiveCommandKernel.cpp`

**Key Classes**:

1. **LSTMCell**
   - 32 input features → 64 hidden dims
   - Quantized INT8 weights (dequantize on use)
   - Forward pass: 5 gate computations (forget, input, candidate, output, reset)
   - ~µs latency per prediction

2. **ThermalMonitor**
   - Polls CPU temperature via WMI or thermal sensors
   - 4 zones: COOL (<60°C), WARM (60-75°C), HOT (75-85°C), CRITICAL (>85°C)
   - Adaptive Sleep delays for worker threads
   - Background monitoring loop

3. **SpeculativeEngine**
   - Async prediction → immediate pre-execution attempt
   - Stores results in thread-safe cache
   - Rollback on user confirmation of different command

4. **AutonomousCommandKernel**
   - 8-thread worker pool with priority queue
   - Priorities: IDLE, NORMAL, INTERACTIVE, REALTIME, EMERGENCY
   - Thermal-aware throttling
   - Metrics: commands processed, predictions hit, throttle count

5. **IntelligentCommandRouter**
   - Maps Command IDs → Priority levels
   - Calculates deadline: REALTIME=50ms, INTERACTIVE=100ms, NORMAL=500ms
   - Thread-safe batch submission
   - Emergency stop mechanism

### Phase-2A: AgentKernel Consumer (MASM64)

**File**: `src/agentic/kernel/RawrXD_AgentKernel_Phase2.asm`

**State Machine**: `IDLE → CONSUMING → PARSING → VALIDATING → DISPATCHING`

**Operations**:
- Poll MMF write_ptr (non-blocking)
- Parse 64-byte tool tokens (magic=0xDEADC0DE, version, flags, tool_id, payload)
- Validate capability flags against registry
- Dispatch via C++ callback to CommandRegistry
- Accumulate statistics (tools_parsed, validated, dispatched, errors)
- CPUID-based AVX-512F detection for advanced parsing

**Thermal Integration**:
- Check MMF thermal_zone field
- Return error on CRITICAL zone
- Sleep for adaptive backoff on throttle

### Phase-3P: MmfProducer (MASM64)

**File**: `src/agentic/producer/RawrXD_MmfProducer_Phase3.asm`

**Zero-Copy Ring Buffer**:
- 1MB circular buffer
- Write pointer managed atomically (CAS loop)
- Batch accumulation: adaptive threshold (4-128 items)
- Backpressure detection: halt at 90% fill

**Batching Strategy**:
- Critical commands (priority >> 28): flush immediately
- Normal commands: accumulate until threshold
- Adaptive: reduce batch size under backpressure

**Statistics**:
- flushed_tokens (cumulative)
- batches_sent
- backpressure_ct (throttle counter)

### Phase-3H: HotpatchEngine (MASM64)

**File**: `src/agentic/hotpatch/RawrXD_HotpatchEngine_Phase3.asm`

**Atomic Detour Installation**:
1. Suspend all threads
2. Save original bytes to shadow page
3. Write x64 detour: `mov r11, <replacement>; jmp r11`
4. Atomic write_ptr update via CAS
5. Resume all threads

**Detour Format** (32 bytes):
```asm
049h 0BBh [qword replacement_addr]  ; REX.WB mov r11, imm64
041h 0FFh 0E3h                      ; REX.B jmp r11
[padding to 32 bytes]
```

**Thread Coordination**:
- CRITICAL_SECTION for mutual exclusion
- Suspension timeout: 5000ms
- Rollback capability for safety

### Phase-2/3: Integration Bridge

**File**: `src/agentic/bridge/AgentKernelBridge.cpp`

**Responsibilities**:
1. Initialize hotpatch engine
2. Create/map MMF (1MB, read/write)
3. Start kernel thread for batch flushing
4. Wire router → kernel → producer → consumer → dispatcher

**Public API**:
```cpp
bool initialize(HWND hwnd, CommandRegistry* registry, 
                const std::filesystem::path& mmfPath);

bool submitForSpeculation(uint32_t commandId, WPARAM wParam, LPARAM lParam);

bool dispatchToolThroughMMF(uint32_t toolId, uint32_t priority, 
                           const void* payload, size_t payloadLen);

bool applyThermalPatch(void* targetAddr, void* replacementAddr);

int getMMFBackpressure() const;

Metrics getMetrics() const;
```

### SelfManifestor: PE Scanner

**File**: `src/agentic/manifestor/RawrXD_SelfManifestor.cpp`

**Capabilities**:
- Scan build directory for .dll, .exe, .lib, .obj artifacts
- Parse PE export tables
- Identify `capability_*` prefixed exports
- Generate capability manifest (JSON schema v2.0)
- Produce wiring diagrams
- Compute SHA256 hashes for capability validation

**Manifest Structure**:
```json
{
  "schemaVersion": "2.0",
  "generatedAt": 1704067200,
  "buildConfig": "Release",
  "targetArch": "x64",
  "capabilities": [
    {
      "name": "ClassName::MethodName",
      "version": 0x01000000,
      "rva": 0x5000,
      "sourceModule": "plugin.dll",
      "timestamp": 1704067100,
      "hash": "..."
    }
  ],
  "artifacts": [...]
}
```

## Build System

**CMakeLists.txt**: `src/agentic/kernel/CMakeLists.txt`

**Key Settings**:
```cmake
enable_language(ASM_MASM)
set(CMAKE_ASM_MASM_FLAGS "/arch:AVX512 /ALIGN:16")
set(CMAKE_CXX_FLAGS "/arch:AVX2 /permissive- /W4 /WX")
```

**Targets**:
- `RawrXD-KernelPhase2-CPP`: C++ neural kernel (object library)
- `RawrXD-KernelPhase2-MASM`: MASM consumer (object library)
- `RawrXD-ProducerPhase3-MASM`: MMF producer (object library)
- `RawrXD-HotpatchPhase3-MASM`: Hotpatch engine (object library)
- `RawrXD-AgentKernelBridge`: C++ bridge (object library)
- `RawrXD-SelfManifestor`: PE scanner (object library)
- `RawrXD-AgenticKernel`: Unified static library (combines all above)

**Compilation**:
```powershell
# Configure
cmake -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release .

# Build
cmake --build . --config Release --target RawrXD-AgenticKernel
```

## Performance Characteristics

| Component | Latency | Throughput | Notes |
|-----------|---------|-----------|-------|
| CommandRegistry dispatch | 10-50µs | 100K cmds/sec | Capability validation |
| LSTM prediction | 5-15µs | 100K pred/sec | INT8 quantized |
| Speculative pre-exec | 50-200µs | Async | Thermal-aware sleep |
| MMF batch flush | 100-500µs | 1-10K tokens/sec | CAS + thread resume overhead |
| Hotpatch detour | 20-50µs | Dynamic | Plus thread suspension cost (~1-5ms) |
| ThermalMonitor loop | 1ms | Query only | Background thread |

## Thermal Throttling Behavior

**Temperature Zones**:
- **COOL** (<60°C): Spin with 0ms delay, max speculation
- **WARM** (60-75°C): 1-2ms adaptive sleep, normal speculation
- **HOT** (75-85°C): 5-10ms sleep, reduced batch size (64→32)
- **CRITICAL** (>85°C): 100ms sleep, speculation disabled, producer stalls

**Example Throttle Sequence**:
```
t=0ms:     Temp=75°C (HOT zone) → kernel_delay=5ms
t=5ms:     Batch size 128→64, re-check temp
t=10ms:    Temp=78°C (still HOT) → delay=7ms
t=17ms:    Temp=72°C (back to WARM) → delay=1ms
t=18ms:    Resume normal operation
```

## Usage Examples

### 1. Basic Agentic IDE Launch

```cpp
// In Win32IDE.cpp::onCreate()
AgentKernelBridge& bridge = GetAgentKernelBridge();
bridge.initialize(hwnd, &CommandRegistry::instance(), 
                 "C:\\mmf\\command_kernel.bin");

// Now commands flow through agentic pipeline:
// Win32 Menu → CommandRegistry → Router → Kernel → Speculative/Dispatch
```

### 2. Submit Command for Speculation

```cpp
// User clicks "Run" button (Command ID 5100)
bridge.submitForSpeculation(5100, 0, 0);

// Kernel speculatively pre-executes
// Results cached in SpeculativeEngine
// When user confirms, result is immediately available
```

### 3. Thermal-Aware Dispatch

```cpp
// ThermalMonitor detects HOT zone
// AutonomousCommandKernel::workerLoop() adapts:
int adaptive_delay_ms = thermal_monitor.getAdaptiveDelayUs(CAP_THERMAL) / 1000;
std::this_thread::sleep_for(std::chrono::milliseconds(adaptive_delay_ms));

// Batch size shrinks: 128 → 64
producer_ctx.batch_threshold = 32;

// Later, when cooled:
producer_ctx.batch_threshold = 128;  // Resume normal batching
```

### 4. Install Hotpatch

```cpp
// Patch CommandRegistry::execute() for performance
void* target = (void*)&CommandRegistry::execute;
void* replacement = (void*)&optimized_execute_hotpath;

bridge.applyThermalPatch(target, replacement);

// Now all execute() calls route through hotpatched version
// Zero-downtime: threads suspended, bytes swapped, resumed
```

### 5. Monitor Performance

```cpp
AgentKernelBridge::Metrics metrics = bridge.getMetrics();

printf("Commands processed: %lld\n", metrics.commandsProcessed);
printf("Prediction hits: %lld\n", metrics.predictionsHit);
printf("Hit rate: %.1f%%\n", 
       100.0 * metrics.predictionsHit / metrics.commandsProcessed);
printf("Thermal throttles: %lld\n", metrics.thermalThrottles);
printf("MMF fill: %d%%\n", bridge.getMMFBackpressure());
```

## Deployment & Testing

### Build Steps

1. **Configure CMake**:
   ```powershell
   cd build
   cmake -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release ..
   ```

2. **Compile**:
   ```powershell
   cmake --build . --config Release --target RawrXD-AgenticKernel
   ```

3. **Verify Artifacts**:
   ```powershell
   ls build/Release/*.obj  # Should show 4 MASM files
   ls build/Release/*.lib  # Should show RawrXD-AgenticKernel.lib
   ```

### Smoke Tests

1. **Capability Registry**: Verify CommandRegistry initializes without crashing
2. **Command Routing**: Send 100 commands, confirm all reach kernel
3. **Speculation**: Enable speculation, verify cache hits increase
4. **Thermal Response**: Simulate HOT zone, confirm thread delays increase
5. **MMF Producer**: Flush batch, verify write_ptr advances
6. **Hotpatch**: Install 1 detour, verify redirects work
7. **Manifestor**: Scan build artifacts, confirm PE parsing works

### Performance Benchmarks

```powershell
# Run diagnostic
RawrXD-ModelLoader.exe --benchmark-agentic

# Expected results:
# Command latency (p50):     20µs
# Command latency (p99):    100µs
# Speculation hit rate:   40-60%
# MMF throughput:      10K tokens/sec
# Hotpatch latency:     ~5ms (includes thread suspension)
```

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| MASM compilation fails | Missing include files | Add `/I "C:\masm32\include"` |
| "HotpatchEngine_Initialize failed" | Insufficient privileges | Run as admin |
| MMF backpressure at 100% | Consumer stalled | Check AgentKernel_Phase2 running |
| Speculative results wrong | Cache not cleared | Verify rollback on user input |
| Temperature monitoring 0°C | WMI failure | Check Windows Performance Counters |

## Future Enhancements

1. **Phase-3.5**: Distributed MMF across processes (named pipes)
2. **Phase-4**: GPU-accelerated LSTM (compute shader)
3. **Phase-5**: Prediction model quantization to FP16
4. **Phase-6**: Adaptive hotpatch based on telemetry feedback
5. **Phase-7**: Cross-process tool invocation (RPC)

---

**Generated**: 2024
**Author**: RawrXD Agentic IDE Team
**License**: Proprietary - RawrXD Project
