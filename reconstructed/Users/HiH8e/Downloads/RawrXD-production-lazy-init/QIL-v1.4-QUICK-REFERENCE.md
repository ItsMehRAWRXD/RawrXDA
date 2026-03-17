# Quantum Injection Library v1.4 - Quick Reference

## API Quick Start

### Initialize Library
```asm
INVOKE InitializeQuantumLibrary, 
       rcx=pModelPath,          ; File path (char*)
       rdx=qModelParams,        ; Parameters (100M-120B)
       r8=pTensorList,          ; Tensor array
       r9=tensorCount           ; Tensor count
; Returns: rax = HRESULT (0 = success)
```

### Check GPS SLA Status
```asm
INVOKE MeasureAndValidateGPS
; Returns: al = 'X' (Exceeded), 'M' (Met), 'B' (Below)
```

### Load Specific Tensor
```asm
INVOKE LoadTensorWithTimeout,
       rcx=tensorID,            ; Tensor to load
       rdx=timeoutMs            ; Timeout (default 5000ms)
; Returns: rax = HRESULT
```

### Cleanup Resources
```asm
INVOKE CleanupQuantumLibrary
; Returns: rax = TRUE (always succeeds)
```

---

## Error Codes (HRESULT)

| Code | Constant | Meaning |
|------|----------|---------|
| 0x00000000 | QIL_OK | Success |
| 0x80070057 | QIL_E_INVALIDARG | Invalid parameter |
| 0x8007000E | QIL_E_OUTOFMEMORY | Allocation failed |
| 0x800401F4 | QIL_E_THREADING | Thread pool error |
| 0x800401F5 | QIL_E_TIMEOUT | Operation timeout |
| 0x800401F6 | QIL_E_TENSOR_NOT_FOUND | Tensor missing |
| 0x800401F8 | QIL_E_PROTECTION | Memory protection failed |

---

## Model Tiers & Targets

| Model | Parameters | Load Time | GPS | Memory |
|-------|-----------|-----------|-----|--------|
| Tiny | <1B | 0.1s | 10 | 45MB |
| Small | 1B-3B | 0.15s | 20 | 65MB |
| Medium | 3B-8B | 0.27s | 30 | 100MB |
| Large | 8B-30B | 0.75s | 40 | 180MB |
| XL | 30B-70B | 1.4s | 50 | 250MB |
| Massive | 70B-120B | 2.0s | 60 | 320MB |
| Ultra | >120B | 1.7s | 70 | 400MB |

---

## Tensor States

```
UNLOADED (0)    → Not in memory
RESERVED (1)    → Forward pass marked
LOADING (2)     → In progress
LOADED (3)      → Ready to use
ACTIVE (4)      → Currently accessed
STALE (5)       → Needs refresh
UNLOADING (6)   → Being removed
```

---

## Constants

### Memory (Bytes)
```asm
MEMORY_BUDGET_TINY       = 45MB
MEMORY_BUDGET_SMALL      = 65MB
MEMORY_BUDGET_MEDIUM     = 100MB
MEMORY_BUDGET_LARGE      = 180MB
MEMORY_BUDGET_XL         = 250MB
MEMORY_BUDGET_MASSIVE    = 320MB
MEMORY_BUDGET_ULTRA      = 400MB
```

### Compression
```asm
TOTAL_LIBRARY_SIZE       = 1.096GB (original)
COMPRESSED_TOTAL         = 271MB (76% reduction)
ZSTD_LEVEL_CONTEXT       = 19 (75% ratio)
ZSTD_LEVEL_VOCAB         = 12 (75% ratio)
ZSTD_LEVEL_FEATURES      = 22 (75% ratio)
```

### Timeouts
```asm
MUTEX_TIMEOUT_MS         = 30000ms (30s)
TENSOR_LOAD_TIMEOUT_MS   = 5000ms (5s)
GPS_MEASUREMENT_WINDOW   = 1000ms (1s)
```

---

## Logging Levels

```
LOG_LEVEL_ERROR   (1)   - Critical failures only
LOG_LEVEL_WARN    (2)   - SLA breaches, timeouts
LOG_LEVEL_INFO    (3)   - Initialization, attachments
LOG_LEVEL_DEBUG   (4)   - Full trace (dev only)

CURRENT_LOG_LEVEL = LOG_LEVEL_INFO (production)
```

---

## GPS SLA Tolerance

```
Target: 60 GPS (120B model)
±10% = ±6 GPS

Status 'X' (Exceeded):  >66 GPS
Status 'M' (Met):       54-66 GPS
Status 'B' (Below):     <54 GPS
```

---

## Global State

```asm
; Thread-safe (mutex protected)
hLibraryMemory          ; Main memory allocation
hMutex                  ; Synchronization object
bInitialized            ; 0=not, 1=ready, 2=failed

; Statistics
dwCacheHits             ; Cache hit count
dwCacheMisses           ; Cache miss count
dwTotalRequests         ; Total requests
qParametersLoaded       ; Loaded parameters
```

---

## TensorMetadata Fields (128 bytes)

```
[+0]:   TensorID (qword)
[+8]:   ParameterCount (qword)
[+16]:  ByteSize (qword)
[+24]:  CompressedSize (qword)
[+32]:  MemoryOffset (qword)
[+40]:  State (dword)
[+44]:  AccessCount (dword)
[+48]:  LastAccessTime (qword)
[+56]:  Dependencies[4] (32 bytes)
[+88]:  Dependents[4] (32 bytes)
[+120]: HotnessScore (dword)
[+124]: CompressionHint (byte)
[+125]: Reserved[7] (padding)
```

---

## Build Commands

### Compile
```bash
ml64.exe /nologo /W4 /errorReport:prompt \
    /Fo quantum_injection_library.obj \
    quantum_injection_library.asm
```

### Create Library
```bash
lib.exe /OUT:quantum_injection_library.lib \
    quantum_injection_library.obj
```

### Link with ZSTD
```cmake
target_link_libraries(RawrXD-AgenticIDE PRIVATE
    quantum_injection_library.lib
    zstd_static.lib
)
```

---

## Error Response Example

```asm
INVOKE InitializeQuantumLibrary, rcx, rdx, r8, r9
cmp eax, QIL_OK
je _init_success

; Handle error
cmp eax, QIL_E_INVALIDARG
je _invalid_param
cmp eax, QIL_E_OUTOFMEMORY
je _no_memory
cmp eax, QIL_E_TIMEOUT
je _timeout_error

; Unknown error
jmp _unknown_error

_init_success:
    ; Continue with initialized library
```

---

## Common Issues & Fixes

| Issue | Cause | Fix |
|-------|-------|-----|
| QIL_E_OUTOFMEMORY | Budget exceeded | Check MAX_MEMORY_BUDGET (400MB) |
| QIL_E_TIMEOUT | Slow allocation | Check available system memory |
| QIL_E_INVALIDARG | Bad pointer | Verify pTensorList not NULL |
| QIL_E_TENSOR_NOT_FOUND | Invalid ID | Validate tensor ID in range [0, tensorCount) |
| GPS below SLA | Load too slow | Check if storage is slow (use SSD) |
| Cache hit rate low | Cold tensors | Review dependency graph |

---

## Performance Tips

1. **Maximize Hot Tensors**: Analyze dependency graph to increase hot set
2. **Batch Loading**: Load related tensors together to improve cache locality
3. **Memory Placement**: Use NUMA-aware allocation for multi-socket systems
4. **Threading**: Leverage Windows thread pool (16+ concurrent decompression)
5. **Dictionary Training**: Train ZSTD dictionary on model family for better ratios

---

## Monitoring Commands

### Check Initialization Status
```asm
mov al, bInitialized
; 0 = not init, 1 = ready, 2 = failed
```

### Get Current GPS
```asm
mov eax, (QuantumLibraryHeader PTR [hLibraryMemory]).LoadThroughputGPS
; eax = current GPS value
```

### Check SLA Status
```asm
mov al, (QuantumLibraryHeader PTR [hLibraryMemory]).GPSSLAStatus
; al = 'X'/'M'/'B'
```

### Get Error Info
```asm
mov eax, (QuantumLibraryHeader PTR [hLibraryMemory]).LastErrorCode
mov edx, (QuantumLibraryHeader PTR [hLibraryMemory]).LastErrorLine
```

---

## Version Info

```
Product: Quantum Injection Library v1.4
Build: 1.4.0.0
Released: December 28, 2025
Status: Production-Ready
Architecture: MASM64 / Windows x64
Compression: NEON BRUTAL v1.1 (76% reduction)
Max Model: 120B+ parameters
Thread Safety: Full (mutex + timeout)
```

---

*Quick Reference Card - Quantum Injection Library v1.4*  
*For detailed documentation, see QUANTUM-LIBRARY-v1.4-PRODUCTION.md*
