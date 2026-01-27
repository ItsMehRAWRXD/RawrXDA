# Copilot Instructions for RawrXD-QtShell

**Project**: RawrXD-QtShell - Advanced GGUF Model Loader with Live Hotpatching & Agentic Correction
**Architecture**: Qt6-based IDE with three-layer hotpatching system (memory, byte-level, server)
**Build**: CMake 3.20+ on Windows (MSVC 2022) / POSIX (Clang)
**Language**: C++20 with Qt6 + custom hotpatch engines

---

## 🏗 Architecture Overview

### Three-Layer Hotpatching System (Critical to Understand)

The codebase implements a coordinated **three-layer hotpatching architecture** for live model modification:

1. **Memory Layer** (`src/qtapp/model_memory_hotpatch.*`)
   - Direct RAM patching using OS memory protection (VirtualProtect/mprotect)
   - Operates on loaded model tensors in GPU/CPU memory
   - **Key pattern**: Uses `PatchResult { bool success, QString detail, int errorCode }`
   - Cross-platform abstractions (`_WIN32` → VirtualProtect; POSIX → mprotect)

2. **Byte-Level Layer** (`src/qtapp/byte_level_hotpatcher.*`)
   - Precision GGUF binary file manipulation (no re-parsing)
   - Pattern matching (Boyer-Moore) for tensor location discovery
   - **Key pattern**: Zero-copy modifications via `directWrite()`, `directRead()`, `directSearch()`
   - Atomic operations (swap, XOR, rotate, reverse)

3. **Server Layer** (`src/qtapp/gguf_server_hotpatch.*`)
   - Request/response transformation for inference servers
   - Hotpatch injection points: `PreRequest`, `PostRequest`, `PreResponse`, `PostResponse`, `StreamChunk`
   - **Key pattern**: ServerHotpatch struct with transform functions and caching

### Coordination Layer

**UnifiedHotpatchManager** (`src/qtapp/unified_hotpatch_manager.*`) coordinates all three:
- Provides single public API: `applyMemoryPatch()`, `applyBytePatch()`, `addServerHotpatch()`
- Qt signals for cross-system events: `patchApplied()`, `errorOccurred()`, `optimizationComplete()`
- Maintains unified statistics and preset save/load via JSON

### Agentic Failure Recovery System

**Three-component failure handling** (new in build):

1. **AgenticFailureDetector** (`src/agent/agentic_failure_detector.*`)
   - Pattern-based detection: refusal, hallucination, timeout, resource exhaustion, safety violations
   - Confidence scoring (0.0-1.0) with multi-failure aggregation
   - **Key pattern**: Uses Qt signals for async failure notifications

2. **AgenticPuppeteer** (`src/agent/agentic_puppeteer.*`)
   - Automatic response correction for detected failures
   - Mode-specific formatting: Plan, Agent, Ask modes
   - **Key pattern**: Static factory methods `CorrectionResult::ok()` / `CorrectionResult::error()` (naming avoids conflict with member variable)

3. **ProxyHotpatcher** (`src/qtapp/proxy_hotpatcher.*`)
   - Proxy-layer byte manipulation for agent output correction
   - Token logit bias support (RST injection for stream termination)
   - **Key pattern**: Uses `void* customValidator` (not std::function) to avoid MSVC template issues

---

## 🔑 Critical Patterns & Conventions

### 1. Result Structs (Error Handling Convention)

All operations return structured results, never throw:

```cpp
// Memory layer uses PatchResult
PatchResult result = hotpatch->applyMemoryPatch(patch);
if (!result.success) { /* handle error */ }

// Server/Unified layers use UnifiedResult / ServerHotpatch structs
UnifiedResult result = manager->applyBytePatch(name, patch);
if (!result.success) { /* handle via errorOccurred() signal */ }
```

**Why**: Enables detailed error tracking across distributed hotpatch layers; Qt signal propagation for async failures.

### 2. Qt Threading Model (Every Layer Respects This)

All hotpatch classes inherit `QObject` and use `QMutex` + `QMutexLocker`:

```cpp
class ModelMemoryHotpatch : public QObject {
    mutable QMutex m_mutex;  // Protects all state
    
    PatchResult applyPatch(...) {
        QMutexLocker lock(&m_mutex);  // RAII auto-unlock
        // modify state safely
    }
};
```

**Critical**: Never call `lock.unlock()` explicitly; always use scope-based locking.

### 3. Factory Methods (Not Constructors for Results)

Use static factory methods with `::ok()` and `::error()` suffixes:

```cpp
// ✅ Correct pattern
return PatchResult::ok("Applied weight patch");
return UnifiedResult::failureResult("operation", "error detail", PatchLayer::Memory);

// ❌ Wrong: Constructors don't convey success/failure semantically
return PatchResult{true, "msg", 0};
```

### 4. Include Guards & Header Dependencies

**Layer isolation**: Each layer includes only what it needs:
- `model_memory_hotpatch.hpp` → includes PatchResult; includes `<windows.h>` conditionally
- `byte_level_hotpatcher.hpp` → includes model_memory_hotpatch (for PatchResult unification)
- `gguf_server_hotpatch.hpp` → includes model_memory_hotpatch (for PatchResult)

**Why**: Prevents circular includes; unifies struct definitions (only one `PatchResult` definition wins).

### 5. Memory Access Patterns

Both memory and byte layers use similar patterns:

```cpp
// Memory layer: direct pointer arithmetic
const char* found = base_ptr + offset;
size_t distance = found - base_ptr;  // Use pointer arithmetic, not std::distance

// Byte layer: QByteArray operations
QByteArray data = model_data;
data.replace(pattern, replacement);  // Create copy first if const

// Server layer: QJsonObject for structured data
QJsonObject patch;
patch["parameterName"] = "temperature";
patch["parameterValue"] = 0.7;
```

---

## 🛠 Build & Workflow

### Build Targets

```bash
# Main executable (Qt IDE + all hotpatchers)
cmake --build . --config Release --target RawrXD-QtShell  # → build/bin/Release/RawrXD-QtShell.exe (1.49 MB)

# Test gate (self-testing components)
cmake --build . --config Release --target self_test_gate

# Quant utilities library
cmake --build . --config Release --target quant_utils
```

### Build System Notes

- **Qt MOC**: Enabled via `set(CMAKE_AUTOMOC ON)` — automatically generates meta-object code
- **Compiler flags**: C++20 required (`CMAKE_CXX_STANDARD 20`)
- **ggml submodule**: Optional dependency for quantization kernels
- **Qt6 path**: Hardcoded to `C:/Qt/6.7.3/msvc2022_64` (check CMakeLists.txt:36 if build fails)

### Common Build Issues

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| `error C2275: 'QByteArray': expected expression instead of type` | `std::function<fn(const QByteArray&)>` template instantiation | Use `void*` pointer instead of std::function |
| `error C2663: no overloaded function has valid conversion for 'this' pointer` | Calling non-const method on const QByteArray | Create copy: `QByteArray copy = data; copy.replace(...)` |
| MOC errors in QtShell | Header with Q_OBJECT not listed in CMakeLists | Add `.hpp` file to source list (MOC auto-compiles) |
| `unresolved external symbol` for signal/slot | Slot implementation missing in `.cpp` | Ensure all Q_SLOT methods have implementations |

---

## 📂 File Organization

```
src/
  qtapp/              # Qt-based IDE layer
    model_memory_hotpatch.{hpp,cpp}       # Memory layer (120 KB)
    byte_level_hotpatcher.{hpp,cpp}       # Byte layer (100 KB)
    gguf_server_hotpatch.{hpp,cpp}        # Server layer (150 KB)
    unified_hotpatch_manager.{hpp,cpp}    # Coordinator (80 KB)
    proxy_hotpatcher.{hpp,cpp}            # Agentic proxy (70 KB)
    MainWindow.cpp                         # Qt UI framework
    main_qt.cpp                            # Qt entry point
  agent/              # Agentic systems
    agentic_failure_detector.{hpp,cpp}    # Failure detection (90 KB)
    agentic_puppeteer.{hpp,cpp}           # Response correction (70 KB)
    auto_bootstrap.cpp                     # Self-initialization
    self_patch.cpp                         # Self-patching
CMakeLists.txt        # Build config (891 lines)
BUILD_COMPLETE.md     # Latest build status + fixes applied
```

---

## 🔴 Known Constraints & Gotchas

1. **MSVC Template Issues**: Avoid `std::function` with const references as parameters. Use function pointers or void* instead.
2. **Qt MOC Limitations**: All signals/slots must be in Q_OBJECT derived classes; can't forward-declare Q_OBJECT types.
3. **Memory Layout Assumptions**: Hotpatchers assume model tensors are contiguous in memory; fragmentation breaks assumptions.
4. **Thread Safety**: All public APIs are thread-safe (use QMutex), but signals are async — callbacks may be delayed.
5. **Const Correctness in Qt**: QByteArray methods like `replace()` are non-const; always create copies for const inputs.

---

## 🎯 Common Tasks for AI Agents

### Task 1: Add a New Hotpatch Type
1. Define struct in appropriate layer header (e.g., `gguf_server_hotpatch.hpp`)
2. Use `PatchResult::ok()/error()` for return values
3. Add slot implementations to `.cpp` if using Qt signals
4. Ensure `QMutexLocker lock(&m_mutex)` guards all state modifications
5. Update `UnifiedHotpatchManager` to route new type to correct layer

### Task 2: Debug Build Failures
1. Check error code prefix (C2xxx = template, C3xxx = override specifier, LNK = linker)
2. Verify includes chain: check if dependent header includes its dependencies
3. If MOC error, ensure class inherits `QObject` and has `Q_OBJECT` macro
4. For const correctness errors, trace the const flow — create copies where needed

### Task 3: Add Agentic Failure Type
1. Add enum value to `AgenticFailureDetector::FailureType`
2. Add detection pattern in `detectFailure()` method
3. Add confidence calculation in `calculateConfidence()` override
4. Update `AgenticPuppeteer` to handle new failure type in `correctResponse()`
5. Emit signal: `emit failureDetected(newType, confidence, description)`

---

## 📚 Key Documentation Files

- `BUILD_COMPLETE.md` — Latest build status, all fixes applied, component checklist
- `README.md` — Project overview, feature list, prerequisites
- `QUICK-REFERENCE.md` — Build commands, file locations, troubleshooting
- `ARCHITECTURE-EDITOR.md` — UI/UX architecture for IDE components
- `AUTONOMOUS-AGENT-GUIDE.md` — Agent framework design

---

## ✅ Last Build Status (Dec 4, 2025)

- **Executable**: `build/bin/Release/RawrXD-QtShell.exe` (1.49 MB)
- **Status**: All systems integrated ✅
  - ✅ model_memory_hotpatch - direct memory access with OS protection
  - ✅ byte_level_hotpatcher - pattern-based binary manipulation
  - ✅ gguf_server_hotpatch - server hotpatching with caching
  - ✅ unified_hotpatch_manager - coordinated three-layer system
  - ✅ proxy_hotpatcher - agentic byte manipulation (void* validator)
  - ✅ agentic_puppeteer - response correction (CorrectionResult::ok/error)
  - ✅ agentic_failure_detector - multi-layer failure detection
- **Compiler**: MSVC 2022 (14.44.35207), C++20, Release optimized
- **Recent Fixes**: std::function → void*, const QByteArray copies, method naming conflicts
