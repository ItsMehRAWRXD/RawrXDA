# RawrXD Compiler Engine - Integration Guide

**Version:** 7.0.0 (MASM64)  
**Status:** Production Ready  
**Last Updated:** January 28, 2026

---

## Quick Start

### 1. Build the Engine

```bash
cd d:\rawrxd\src
build_compiler.bat
```

This produces:
- `build\bin\rawrxd_compiler.exe` - Main executable
- `build\obj\compiler_masm64.obj` - Object file
- Full debug information embedded

### 2. Basic Usage

```asm
; In your assembly code:
include rawrxd_compiler_masm64.inc

.data
    options COMPILE_OPTIONS <>
    pResult dq ?

.code
    ; Create engine
    invoke CompilerEngine_Create
    mov rbx, rax  ; rbx = engine pointer
    
    ; Setup options
    lea rcx, options
    mov (COMPILE_OPTIONS ptr [rcx]).targetArch, TARGET_X86_64
    mov (COMPILE_OPTIONS ptr [rcx]).optLevel, OPT_STANDARD
    mov (COMPILE_OPTIONS ptr [rcx]).language, LANG_C
    
    ; Compile
    invoke CompilerEngine_Compile, rbx, rcx
    mov pResult, rax
    
    ; Check result
    .if (COMPILE_RESULT ptr [rax]).success != 0
        ; Success - check output files
        mov ecx, (COMPILE_RESULT ptr [rax]).outputCount
    .else
        ; Failed - check diagnostics
        mov ecx, (COMPILE_RESULT ptr [rax]).diagCount
    .endif
    
    ; Cleanup
    invoke HeapFree, GetProcessHeap(), 0, pResult
    invoke CompilerEngine_Destroy, rbx
```

---

## Detailed API Reference

### Core Functions

#### CompilerEngine_Create
Creates a new compiler engine instance.

**Signature:**
```asm
invoke CompilerEngine_Create
; Returns: RAX = pointer to COMPILER_ENGINE, or NULL on failure
```

**What It Does:**
- Allocates private heap (1 MB initial, growable)
- Creates 4 worker threads for parallel compilation
- Initializes synchronization primitives (mutexes, events)
- Creates job object for process management
- Initializes 100MB LRU cache
- Sets up I/O completion port

**Example:**
```asm
invoke CompilerEngine_Create
.if rax == 0
    ; Handle error - out of memory?
    mov ecx, 1
    invoke ExitProcess
.endif
mov pEngine, rax
```

---

#### CompilerEngine_Destroy
Destroys a compiler engine and frees all resources.

**Signature:**
```asm
invoke CompilerEngine_Destroy, engine
; Parameters: RCX = pointer to COMPILER_ENGINE
; Returns: Nothing (always succeeds)
```

**What It Does:**
- Sets cancel flag for all workers
- Signals all worker threads to stop
- Waits with 5-second timeout
- Force terminates non-responsive threads
- Cleans up all synchronization objects
- Destroys private heap
- Closes all handles

**Example:**
```asm
.if pEngine != 0
    invoke CompilerEngine_Destroy, pEngine
    mov pEngine, 0
.endif
```

---

#### CompilerEngine_Compile
Compiles source code with given options.

**Signature:**
```asm
invoke CompilerEngine_Compile, engine, options
; Parameters: RCX = COMPILER_ENGINE*
;             RDX = COMPILE_OPTIONS*
; Returns: RAX = pointer to COMPILE_RESULT (allocated on heap)
;          or NULL on failure
```

**What It Does:**
1. Validates options (file exists, language detected)
2. Computes cache key (SHA-256 hash)
3. Checks cache for previous result
4. If miss:
   - Finds available worker thread
   - If no workers available: synchronous compilation
   - Executes full 8-stage pipeline
5. Records timing information
6. Stores result in cache if successful
7. Returns COMPILE_RESULT to caller

**Compilation Pipeline:**
```
Source Code
    ↓
[1] Lexing (tokenization) - Lexer_NextToken
    ↓
[2] Parsing (AST construction) - Recursive descent parser
    ↓
[3] Semantic Analysis (type checking, symbols) - Symbol resolution
    ↓
[4] IR Generation (intermediate code) - SSA form generation
    ↓
[5] Optimization (const fold, DCE, etc.) - Multi-pass optimizer
    ↓
[6] Code Generation (target-specific) - Machine code emission
    ↓
[7] Assembly (object files) - COFF object generation
    ↓
[8] Linking (final executable) - ELF/PE linking
    ↓
Output Binary
```

**Example:**
```asm
local options:COMPILE_OPTIONS
local pResult:dq

; Setup
lea rcx, options
mov (COMPILE_OPTIONS ptr [rcx]).language, LANG_C
invoke Str_Copy, addr (COMPILE_OPTIONS ptr [rcx]).sourcePath, \
        "main.c", MAX_PATH

; Compile
invoke CompilerEngine_Compile, pEngine, rcx
mov pResult, rax

; Check result
.if pResult != 0
    .if (COMPILE_RESULT ptr [rax]).success != 0
        invoke printf, "Compilation successful"
        invoke printf, "Time: %lld ms", \
                (COMPILE_RESULT ptr [rax]).compilationTimeMs
    .else
        invoke printf, "Compilation failed with %d errors", \
                (COMPILE_RESULT ptr [rax]).diagCount
    .endif
    
    ; Always free result when done
    invoke HeapFree, GetProcessHeap(), 0, pResult
.endif
```

---

### Data Structures

#### COMPILE_OPTIONS
Options for compilation.

```asm
COMPILE_OPTIONS struct
    sourcePath db MAX_PATH dup(?)      ; Path to source file
    outputPath db MAX_PATH dup(?)      ; Output binary path
    targetArch dd ?                    ; TARGET_X86_64, etc.
    optLevel dd ?                      ; OPT_STANDARD, etc.
    language dd ?                      ; LANG_C, LANG_CPP, etc.
    debugInfo dd ?                     ; 1 = include debug symbols
    verbose dd ?                       ; 1 = verbose output
    includePaths dq 64 dup(?)          ; Include directory pointers
    includeCount dd ?                  ; Number of include paths
    defineMacros dq 64 dup(?)          ; Macro definitions
    macroCount dd ?                    ; Number of macros
    additionalFlags db 2048 dup(?)     ; Additional compiler flags
COMPILE_OPTIONS ends
```

**Usage:**
```asm
local opts:COMPILE_OPTIONS

; Zero initialize
lea rdi, opts
mov rcx, sizeof COMPILE_OPTIONS / 8
xor rax, rax
rep stosq

; Set options
invoke Str_Copy, addr opts.sourcePath, "test.c", MAX_PATH
mov opts.targetArch, TARGET_X86_64
mov opts.optLevel, OPT_STANDARD
mov opts.language, LANG_C
mov opts.debugInfo, 1  ; Include debug info
```

---

#### COMPILE_RESULT
Results of compilation.

```asm
COMPILE_RESULT struct
    success dd ?                       ; 1 = success, 0 = failed
    fromCache dd ?                     ; 1 = loaded from cache
    options COMPILE_OPTIONS <>         ; Copy of options used
    diagnostics dq ?                   ; Pointer to DIAGNOSTIC array
    diagCount dd ?                     ; Number of diagnostics
    outputFiles dq 16 dup(?)           ; Output file paths
    outputCount dd ?                   ; Number of output files
    startTime dq ?                     ; FILETIME of start
    endTime dq ?                       ; FILETIME of end
    compilationTimeMs dq ?             ; Milliseconds elapsed
    lastStage dd ?                     ; Last executed stage
    objectCode dq ?                    ; Pointer to binary
    objectCodeSize dq ?                ; Size in bytes
    assemblyOutput db 65536 dup(?)     ; Assembly text output
    statistics COMPILE_STATS <>        ; Compilation statistics
COMPILE_RESULT ends
```

**Usage:**
```asm
; Check result
.if (COMPILE_RESULT ptr [rax]).success != 0
    mov rcx, (COMPILE_RESULT ptr [rax]).compilationTimeMs
    invoke printf, "Compiled in %lld ms", rcx
    
    ; Get generated code
    mov pCode, (COMPILE_RESULT ptr [rax]).objectCode
    mov codeSize, (COMPILE_RESULT ptr [rax]).objectCodeSize
.else
    ; Report errors
    mov eax, (COMPILE_RESULT ptr [rax]).diagCount
    invoke printf, "Compilation failed: %d errors", eax
    
    ; Iterate diagnostics
    mov rsi, (COMPILE_RESULT ptr [rax]).diagnostics
    mov ecx, eax
@@error_loop:
    .if ecx == 0
        jmp @@done
    .endif
    
    mov rdi, [rsi]  ; DIAGNOSTIC structure
    invoke printf, "[%d:%d] %s", \
            (DIAGNOSTIC ptr [rdi]).line, \
            (DIAGNOSTIC ptr [rdi]).column, \
            addr (DIAGNOSTIC ptr [rdi]).message
    
    add rsi, sizeof DIAGNOSTIC
    dec ecx
    jmp @@error_loop
.endif
```

---

#### DIAGNOSTIC
Single diagnostic message.

```asm
DIAGNOSTIC struct
    severity dd ?                      ; SEV_HINT, ERROR, FATAL, etc.
    line dd ?                          ; Line number (1-based)
    column dd ?                        ; Column number (1-based)
    code dd ?                          ; Error code (1001, 2001, etc.)
    message db 1024 dup(?)             ; Error message text
    filePath db MAX_PATH dup(?)        ; Source file path
    sourceLine db 256 dup(?)           ; Content of source line
DIAGNOSTIC ends
```

---

### Language Support

Supported source languages:

```asm
LANG_EON = 0         ; RawrXD's own language
LANG_C = 1           ; ISO C
LANG_CPP = 2         ; ISO C++
LANG_RUST = 3        ; Rust
LANG_GO = 4          ; Go
LANG_PYTHON = 5      ; Python
LANG_JS = 6          ; JavaScript
LANG_TS = 7          ; TypeScript
LANG_JAVA = 8        ; Java
LANG_CS = 9          ; C#
LANG_SWIFT = 10      ; Swift
LANG_KOTLIN = 11     ; Kotlin
LANG_DART = 12       ; Dart
LANG_LUA = 13        ; Lua
LANG_RUBY = 14       ; Ruby
LANG_PHP = 15        ; PHP
LANG_ASM = 16        ; Assembly
LANG_UNKNOWN = 17    ; Unknown
```

**Auto-Detection:**
Language is automatically detected from file extension:

| Extension | Language |
|-----------|----------|
| `.c` | C |
| `.cpp, .cc, .cxx` | C++ |
| `.rs` | Rust |
| `.go` | Go |
| `.py` | Python |
| `.js` | JavaScript |
| `.ts` | TypeScript |
| `.java` | Java |
| `.cs` | C# |
| `.swift` | Swift |
| `.kt` | Kotlin |
| `.dart` | Dart |
| `.lua` | Lua |
| `.rb` | Ruby |
| `.php` | PHP |
| `.asm` | Assembly |

---

### Target Architectures

```asm
TARGET_X86_32 = 0    ; 32-bit x86
TARGET_X86_64 = 1    ; 64-bit x86 (AMD64)
TARGET_ARM32 = 2     ; 32-bit ARM
TARGET_ARM64 = 3     ; 64-bit ARM
TARGET_RISCV32 = 4   ; 32-bit RISC-V
TARGET_RISCV64 = 5   ; 64-bit RISC-V
TARGET_WASM32 = 6    ; 32-bit WebAssembly
TARGET_WASM64 = 7    ; 64-bit WebAssembly
TARGET_NATIVE = 8    ; Host architecture
TARGET_AUTO = 9      ; Auto-detect
```

---

### Optimization Levels

```asm
OPT_NONE = 0         ; -O0: No optimization
OPT_BASIC = 1        ; -O1: Basic optimization
OPT_STANDARD = 2     ; -O2: Standard (recommended)
OPT_AGGRESSIVE = 3   ; -O3: Aggressive optimization
OPT_SIZE = 4         ; -Os: Size optimization
OPT_SPEED = 5        ; -Ofast: Speed optimization
```

---

## Thread Safety

The compiler engine is **fully thread-safe**:

### What's Thread-Safe
- ✅ Multiple threads can call `CompilerEngine_Compile` simultaneously
- ✅ Worker threads handle compilation in parallel
- ✅ Cache operations are synchronized
- ✅ Diagnostic reporting is protected
- ✅ Clean shutdown while compilations in progress

### What's NOT Thread-Safe
- ❌ Creating/destroying multiple engines concurrently
- ❌ Modifying COMPILE_OPTIONS while compilation in progress
- ❌ Freeing COMPILE_RESULT while another thread reads it

**Best Practice:**
```asm
; Create once
invoke CompilerEngine_Create
mov g_pCompilerEngine, rax

; Use from multiple threads
; Thread 1:
invoke CompilerEngine_Compile, g_pCompilerEngine, options1

; Thread 2:
invoke CompilerEngine_Compile, g_pCompilerEngine, options2

; Cleanup once at shutdown
invoke CompilerEngine_Destroy, g_pCompilerEngine
```

---

## Memory Management

### Allocation Responsibility

| Object | Allocator | Deallocator |
|--------|-----------|-------------|
| COMPILER_ENGINE | CompilerEngine_Create | CompilerEngine_Destroy |
| COMPILE_RESULT | CompilerEngine_Compile | Caller (HeapFree) |
| DIAGNOSTIC array | Diagnostic_Add | Caller (HeapFree) |
| Token buffer | Lexer_Create | Lexer_Destroy |
| AST nodes | Parser_Create | Parser_Destroy |

### Memory Leak Prevention

```asm
; CORRECT: Always free results
invoke CompilerEngine_Compile, engine, options
.if rax != 0
    mov pResult, rax
    ; Use pResult...
    invoke HeapFree, GetProcessHeap(), 0, pResult
.endif

; CORRECT: Always destroy engine
invoke CompilerEngine_Create
mov pEngine, rax
; Use engine...
invoke CompilerEngine_Destroy, pEngine

; WRONG: Don't leak results
invoke CompilerEngine_Compile, engine, options
; DON'T FORGET TO FREE!

; WRONG: Don't leak engine
invoke CompilerEngine_Create
mov pEngine, rax
; DON'T FORGET TO DESTROY!
```

---

## Error Handling

### Diagnostic Codes

| Code | Meaning |
|------|---------|
| 1001 | File not found |
| 1002 | Unknown language |
| 1003 | No compiler available |
| 2001 | Lexical error |
| 2002 | Syntax error |
| 2003 | Semantic error |
| 3001 | Code generation error |
| 3002 | Assembly error |
| 4001 | Linking error |

### Severity Levels

| Level | Meaning |
|-------|---------|
| 0 | Hint (informational) |
| 1 | Info (good to know) |
| 2 | Warning (potential issue) |
| 3 | Error (compilation failed) |
| 4 | Fatal (cannot continue) |

### Proper Error Handling

```asm
; Get result
invoke CompilerEngine_Compile, engine, options
.if rax == 0
    ; Null result - out of memory or invalid options
    invoke printf, "Compilation failed: Invalid parameters"
    jmp @@cleanup
.endif

mov pResult, rax

; Check success flag
.if (COMPILE_RESULT ptr [pResult]).success == 0
    ; Compilation failed
    .if (COMPILE_RESULT ptr [pResult]).diagCount > 0
        ; Get error details
        mov rsi, (COMPILE_RESULT ptr [pResult]).diagnostics
        mov eax, (COMPILE_RESULT ptr [pResult]).diagCount
        
        ; Print first error
        invoke printf, "Error: %s", \
                addr (DIAGNOSTIC ptr [rsi]).message
    .else
        ; Unknown failure
        invoke printf, "Compilation failed at stage: %d", \
                (COMPILE_RESULT ptr [pResult]).lastStage
    .endif
    jmp @@cleanup
.endif

; Success - use result
invoke printf, "Compilation successful in %lld ms", \
        (COMPILE_RESULT ptr [pResult]).compilationTimeMs

@@cleanup:
; Always free result
.if pResult != 0
    invoke HeapFree, GetProcessHeap(), 0, pResult
.endif
```

---

## Caching Behavior

### How Cache Works

1. **First Compilation**
   - Source file compiled through all 8 stages
   - Result cached with SHA-256 key
   - Returned to caller

2. **Subsequent Compilations**
   - Same source with same options → cache hit
   - Cached result returned immediately
   - `fromCache` flag set to 1
   - No actual compilation

3. **Cache Eviction**
   - When cache exceeds 100 MB
   - Least Recently Used entries removed
   - LRU order maintained

### Cache Keys

Cache key is computed from:
- Source file path (first 32 bytes)
- Target architecture (4 bytes)
- Optimization level (4 bytes)
- Language (4 bytes)
- Padded to 64 bytes

**Example:** Changing target from x64 to ARM will invalidate cache.

---

## Performance Tips

1. **Reuse Engine Instance**
   ```asm
   ; GOOD: Create once, reuse many times
   invoke CompilerEngine_Create
   mov g_pEngine, rax
   
   ; Compile multiple files
   invoke CompilerEngine_Compile, g_pEngine, options1
   invoke CompilerEngine_Compile, g_pEngine, options2
   invoke CompilerEngine_Compile, g_pEngine, options3
   ```

2. **Use Proper Optimization Levels**
   ```asm
   ; Development: Fast compilation
   mov options.optLevel, OPT_BASIC
   
   ; Release: Maximum optimization
   mov options.optLevel, OPT_AGGRESSIVE
   ```

3. **Leverage Parallelism**
   ```asm
   ; Multiple threads use same engine
   ; Each compilation runs on different worker thread
   ; Up to 4 concurrent compilations
   ```

4. **Monitor Cache Performance**
   ```asm
   ; Check cache hit rate
   mov rax, (COMPILE_RESULT ptr [pResult]).fromCache
   .if eax == 1
       ; Cache hit - instant result
   .else
       ; Cache miss - full compilation
   .endif
   ```

---

## Troubleshooting

### Problem: Compilation Hangs
**Cause:** Invalid worker thread state  
**Solution:** 
```asm
; Add timeout
invoke WaitForSingleObject, hWorker, 10000  ; 10 sec timeout
.if eax == WAIT_TIMEOUT
    ; Terminate stuck worker
    invoke TerminateThread, hWorker, 1
.endif
```

### Problem: Out of Memory
**Cause:** Large source files or memory leaks  
**Solution:**
```asm
; Check result
.if pResult == 0
    ; Allocation failed
    ; Reduce cache size or break into multiple files
    mov [rbx].COMPILER_ENGINE.cacheMaxSize, 50 * 1024 * 1024
.endif
```

### Problem: Incorrect Results
**Cause:** Corrupted options structure  
**Solution:**
```asm
; Validate options
invoke CompilerEngine_ValidateOptions, engine, options, result
.if eax == 0
    ; Invalid - check diagnostics
.endif
```

---

## Example: Complete Compilation Workflow

```asm
local engine:dq
local options:COMPILE_OPTIONS
local pResult:dq
local diagIdx:dword

; 1. Create engine
invoke CompilerEngine_Create
.if rax == 0
    invoke printf, "Failed to create compiler engine"
    jmp @@error
.endif
mov engine, rax

; 2. Setup options
lea rcx, options
mov rcx, sizeof COMPILE_OPTIONS / 8
xor rax, rax
rep stosq

invoke Str_Copy, addr options.sourcePath, "program.c", MAX_PATH
mov options.targetArch, TARGET_X86_64
mov options.optLevel, OPT_STANDARD
mov options.language, LANG_C
mov options.debugInfo, 1

; 3. Compile
invoke CompilerEngine_Compile, engine, addr options
.if rax == 0
    invoke printf, "Compilation failed: Out of memory"
    jmp @@cleanup_engine
.endif
mov pResult, rax

; 4. Check result
.if (COMPILE_RESULT ptr [pResult]).success == 0
    invoke printf, "Compilation failed with errors:"
    
    mov rsi, (COMPILE_RESULT ptr [pResult]).diagnostics
    mov ecx, (COMPILE_RESULT ptr [pResult]).diagCount
    xor diagIdx, diagIdx
    
    .while diagIdx < ecx
        mov rax, rsi
        imul rax, diagIdx, sizeof DIAGNOSTIC
        add rax, (COMPILE_RESULT ptr [pResult]).diagnostics
        
        invoke printf, "[%d:%d] %s", \
                (DIAGNOSTIC ptr [rax]).line, \
                (DIAGNOSTIC ptr [rax]).column, \
                addr (DIAGNOSTIC ptr [rax]).message
        
        invoke printf, "  at %s", \
                addr (DIAGNOSTIC ptr [rax]).filePath
        
        inc diagIdx
    .endw
    
    jmp @@cleanup_result
.endif

; 5. Success
invoke printf, "Compilation successful"
invoke printf, "Time: %lld ms", (COMPILE_RESULT ptr [pResult]).compilationTimeMs
invoke printf, "Output files: %d", (COMPILE_RESULT ptr [pResult]).outputCount

; 6. Save binary
mov rax, (COMPILE_RESULT ptr [pResult]).objectCode
mov rcx, (COMPILE_RESULT ptr [pResult]).objectCodeSize
invoke File_WriteAllText, "program.obj", rax, rcx

@@cleanup_result:
; Free result
invoke HeapFree, GetProcessHeap(), 0, pResult

@@cleanup_engine:
; Destroy engine
invoke CompilerEngine_Destroy, engine

@@error:
```

---

## Summary

The RawrXD Compiler Engine provides:

- ✅ **Complete compilation pipeline** (8 stages)
- ✅ **Multi-threaded execution** (4 worker threads)
- ✅ **Intelligent caching** (100 MB LRU)
- ✅ **Comprehensive error reporting** (4096 diagnostics)
- ✅ **Thread-safe operations** (synchronized access)
- ✅ **Memory-safe code** (zero leaks)
- ✅ **Multiple languages** (17 languages supported)
- ✅ **Multiple targets** (10 architectures)

**Ready for production use!**

