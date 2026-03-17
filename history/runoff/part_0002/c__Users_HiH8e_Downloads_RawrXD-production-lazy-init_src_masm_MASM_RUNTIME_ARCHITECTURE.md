# MASM x64 Pure Runtime Layer
## Zero-Dependency Thread-Safe Foundation for Hotpatching

**Goal**: Implement all foundational services (memory, sync, strings, events) in pure x64 MASM to replace Qt dependencies for hotpatch-critical paths.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│   Hotpatch C++ Layer (model_memory_hotpatch.cpp)   │
│   Calls: asm_malloc, asm_mutex_lock, asm_dispatch  │
└──────────────────┬──────────────────────────────────┘
                   │ extern "C"
┌──────────────────▼──────────────────────────────────┐
│     MASM x64 Runtime Foundation Layer               │
├──────────────────────────────────────────────────────┤
│ Layer 4: EVENT LOOP & SIGNAL ROUTING                │
│  - Event queue (circular buffer, thread-safe)       │
│  - Signal handler registry                          │
│  - Async dispatch mechanism                         │
│  - Handler invocation with parameter passing        │
├──────────────────────────────────────────────────────┤
│ Layer 3: UNICODE STRING HANDLER                     │
│  - UTF-8 / UTF-16 string allocation & manipulation  │
│  - Comparison, concatenation, search                │
│  - Format strings (sprintf-like functionality)      │
├──────────────────────────────────────────────────────┤
│ Layer 2: THREAD SYNCHRONIZATION (Win32 Native)     │
│  - Critical sections (CRITICAL_SECTION wrapper)     │
│  - Event objects (manual/auto-reset)                │
│  - Condition variable equivalents                   │
│  - Atomic operations (CAS, increment, xchg)         │
├──────────────────────────────────────────────────────┤
│ Layer 1: DYNAMIC MEMORY MANAGER                     │
│  - Heap allocation & deallocation                   │
│  - Alignment support (16, 32, 64 byte)              │
│  - Fragmentation tracking & statistics              │
│  - Metadata hidden in preheader (size, free flag)   │
└──────────────────────────────────────────────────────┘
```

---

## Layer 1: Memory Manager (asm_memory.asm)

### Design
- **Heap**: Use Windows `HeapAlloc`/`HeapFree` via Win32 API (no reinvention needed)
- **Custom tracking**: Preheader metadata + freelist for custom analysis
- **Alignment**: Support 16/32/64-byte boundaries for SIMD operations

### Key Functions
```asm
asm_malloc(size: rcx, alignment: rdx) -> rax (pointer)
asm_realloc(ptr: rcx, new_size: rdx) -> rax
asm_free(ptr: rcx) -> void
asm_memory_stats() -> rax (struct with alloc count, total bytes, fragmentation)
```

### Metadata Format
```
┌─────────────────────┐
│ Size (qword) [0]    │  <- Hidden preheader
│ Alignment (qword)[8]│
│ Magic (qword) [16]  │  0xDEADBEEFCAFEBABE
├─────────────────────┤
│ User Data           │  <- Pointer returned to caller
│ (size bytes)        │
├─────────────────────┤
```

---

## Layer 2: Thread Synchronization (asm_sync.asm)

### Design
- **Mutexes**: Wrap Windows `CRITICAL_SECTION` for recursive locking
- **Events**: Win32 event objects (`CreateEventEx`)
- **Atomics**: Use `lock` prefixed x64 instructions (cmpxchg, xadd, xchg)

### Key Functions
```asm
asm_mutex_create() -> rax (handle)
asm_mutex_lock(handle: rcx) -> void
asm_mutex_unlock(handle: rcx) -> void
asm_mutex_destroy(handle: rcx) -> void

asm_event_create(manual_reset: rcx) -> rax (handle)
asm_event_set(handle: rcx) -> void
asm_event_reset(handle: rcx) -> void
asm_event_wait(handle: rcx, timeout_ms: rdx) -> rax (WAIT_OBJECT_0, WAIT_TIMEOUT, etc.)
asm_event_destroy(handle: rcx) -> void

asm_atomic_increment(ptr: rcx) -> rax (new value)
asm_atomic_decrement(ptr: rcx) -> rax (new value)
asm_atomic_cmpxchg(ptr: rcx, old: rdx, new: r8) -> rax (success: 1, fail: 0)
asm_atomic_xchg(ptr: rcx, value: rdx) -> rax (old value)
```

---

## Layer 3: Unicode String Handler (asm_string.asm)

### Design
- **Encoding**: Support UTF-8 internally, UTF-16 for Win32 APIs
- **Storage**: Counted strings (length prefix) with null terminator
- **Operations**: Substring, concatenate, compare, search (Boyer-Moore)

### String Format
```
┌──────────────────────────────────────────┐
│ Length (qword) - char count              │
│ Capacity (qword) - allocated bytes       │
│ Encoding (byte) - 8 (UTF-8) or 16 (UTF16)│
│ [7 padding bytes]                        │
├──────────────────────────────────────────┤
│ Data (variable)                          │
│ [null terminator]                        │
└──────────────────────────────────────────┘
```

### Key Functions
```asm
asm_str_create(utf8_ptr: rcx, length: rdx) -> rax (string handle)
asm_str_length(str_handle: rcx) -> rax (character count)
asm_str_concat(str1: rcx, str2: rdx) -> rax (new string)
asm_str_compare(str1: rcx, str2: rdx) -> rax (0=equal, -1=less, 1=greater)
asm_str_find(haystack: rcx, needle: rdx) -> rax (offset or -1)
asm_str_substring(str: rcx, start: rdx, length: r8) -> rax (new string)
asm_str_to_utf16(utf8_handle: rcx) -> rax (UTF-16 pointer, caller must free)
asm_str_from_utf16(utf16_ptr: rcx) -> rax (UTF-8 string handle)
asm_str_format(format: rcx, args_ptr: rdx, args_count: r8) -> rax (new string)
asm_str_destroy(handle: rcx) -> void
```

---

## Layer 4: Event Loop & Signal Routing (asm_events.asm)

### Design
- **Queue**: Ring buffer of event records, protected by mutex
- **Handlers**: Map of signal name → handler function pointers
- **Dispatch**: Pull from queue, invoke handler, handle errors

### Event Record Format
```
┌──────────────────────────────────────────┐
│ Signal ID (qword)                        │
│ Handler Function Ptr (qword)             │
│ Param1 (qword)                           │
│ Param2 (qword)                           │
│ Param3 (qword)                           │
│ Timestamp (qword)                        │
└──────────────────────────────────────────┘
```

### Key Functions
```asm
asm_event_loop_create(queue_size: rcx) -> rax (loop handle)
asm_event_loop_register_signal(loop: rcx, signal_id: rdx, handler: r8) -> void
asm_event_loop_emit(loop: rcx, signal_id: rdx, p1: r8, p2: r9, p3: [rsp+40]) -> void
asm_event_loop_process_one(loop: rcx) -> rax (1=processed, 0=queue empty)
asm_event_loop_process_all(loop: rcx) -> rax (count processed)
asm_event_loop_destroy(loop: rcx) -> void
```

---

## Integration with C++ (Extern "C" Interface)

### C++ Header: asm_runtime.hpp
```cpp
#ifdef __cplusplus
extern "C" {
#endif

// Memory
void* asm_malloc(size_t size, size_t alignment = 16);
void asm_free(void* ptr);
void* asm_realloc(void* ptr, size_t new_size);

// Sync
typedef void* MutexHandle;
typedef void* EventHandle;
MutexHandle asm_mutex_create();
void asm_mutex_lock(MutexHandle h);
void asm_mutex_unlock(MutexHandle h);
void asm_mutex_destroy(MutexHandle h);

// Strings
typedef void* StringHandle;
StringHandle asm_str_create(const char* utf8, size_t len);
void asm_str_destroy(StringHandle h);
const char* asm_str_data(StringHandle h);
size_t asm_str_length(StringHandle h);

// Events
typedef void* EventLoopHandle;
typedef void (*SignalHandler)(uint64_t p1, uint64_t p2, uint64_t p3);
EventLoopHandle asm_event_loop_create(size_t queue_size);
void asm_event_loop_register_signal(EventLoopHandle h, uint32_t id, SignalHandler fn);
void asm_event_loop_emit(EventLoopHandle h, uint32_t id, uint64_t p1, uint64_t p2, uint64_t p3);
void asm_event_loop_destroy(EventLoopHandle h);

#ifdef __cplusplus
}
#endif
```

---

## Building & Linking

### CMakeLists.txt Integration
```cmake
# Enable MASM
enable_language(ASM_MASM)

# Add MASM runtime library
add_library(masm_runtime STATIC
    src/masm/asm_memory.asm
    src/masm/asm_sync.asm
    src/masm/asm_string.asm
    src/masm/asm_events.asm
)

# Link to hotpatch targets
target_link_libraries(model_memory_hotpatch masm_runtime)
target_link_libraries(unified_hotpatch_manager masm_runtime)
```

### Calling Convention
All functions use **Microsoft x64 calling convention**:
- Integer args: `rcx, rdx, r8, r9`
- Return: `rax` (64-bit), `rdx:rax` (128-bit)
- Caller cleans stack
- Caller preserves `rax, rcx, rdx, r8-r11`
- Callee preserves `rsi, rdi, rbx, rsp, rbp, r12-r15`

---

## Performance Targets

| Component | Target | Justification |
|-----------|--------|---------------|
| asm_malloc (16-byte align) | < 500 ns | Competitive with CRT malloc |
| asm_mutex_lock (uncontended) | < 50 ns | Inline CRITICAL_SECTION lock |
| asm_str_concat (1KB + 1KB) | < 1000 ns | Memcpy-bound operation |
| asm_event_emit | < 200 ns | Ring buffer push + atomic increment |

---

## Testing Strategy

1. **Unit tests** (Win32 console app calling extern "C")
   - Memory: allocate, free, reallocate, verify metadata
   - Sync: lock/unlock, concurrent access, deadlock detection
   - Strings: create, concat, compare, format, UTF-8/16 conversion
   - Events: emit, dispatch, handler invocation

2. **Integration tests** (Link with model_memory_hotpatch.cpp)
   - Apply hotpatch using MASM allocator
   - Synchronize multi-threaded patch operations
   - Log using MASM string formatter
   - Signal patch completion via event loop

3. **Stress tests**
   - 10K concurrent allocations
   - 1K simultaneous mutex contentions
   - 100K event emissions per second

---

## Phase Timeline

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| 1: Memory Manager | 4-6 hours | asm_memory.asm + unit tests |
| 2: Thread Sync | 3-4 hours | asm_sync.asm + contention tests |
| 3: String Handler | 5-7 hours | asm_string.asm + encoding tests |
| 4: Event Loop | 4-5 hours | asm_events.asm + dispatch tests |
| 5: Integration | 2-3 hours | Link with hotpatch, verify |
| **Total** | **18-25 hours** | **Production-ready MASM runtime** |

---

## Dependencies (Minimal)
- **Windows SDK**: Win32 API (CRITICAL_SECTION, HeapAlloc, CreateEventEx)
- **MSVC MASM**: ml64.exe assembler + linker
- **CRT**: Only for testing (unit test harness)

---

## Files to Create
1. `src/masm/asm_memory.asm` - Heap management
2. `src/masm/asm_sync.asm` - Mutexes, events, atomics
3. `src/masm/asm_string.asm` - UTF-8/16 strings
4. `src/masm/asm_events.asm` - Event loop & signal routing
5. `src/masm/asm_runtime.hpp` - Public C interface
6. `src/masm/asm_runtime.cpp` - C++ wrapper helpers
7. `test/masm_runtime_test.cpp` - Unit tests
8. `test/masm_integration_test.cpp` - Hotpatch integration

