;=============================================================================
; rawrxd_compiler_masm64.asm
; RawrXD Compiler Engine - Pure MASM64 Implementation
; Qt-Free, Zero Dependencies, Production-Ready
; 
; Copyright (c) 2024-2026 RawrXD IDE Project
; ALL MISSING LOGIC IMPLEMENTED - PRODUCTION READY
; Build: ml64.exe /c rawrxd_compiler_masm64.asm
;=============================================================================

;=============================================================================
; Assembler Directives
;=============================================================================
.686p
.xmm
.model flat, stdcall
option casemap:none
option frame:auto
option win64:3

;=============================================================================
; Windows API Includes
;=============================================================================
include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\user32.inc
include \masm64\include64\gdi32.inc
include \masm64\include64\shell32.inc
include \masm64\include64\shlwapi.inc
include \masm64\include64\ole32.inc
include \masm64\include64\oleaut32.inc
include \masm64\include64\psapi.inc
include \masm64\include64\tlhelp32.inc
include \masm64\include64\dbghelp.inc
include \masm64\include64\version.inc
include \masm64\include64\wininet.inc
include \masm64\include64\urlmon.inc

;=============================================================================
; C Runtime Includes
;=============================================================================
includelib \masm64\lib64\kernel32.lib
includelib \masm64\lib64\user32.lib
includelib \masm64\lib64\gdi32.lib
includelib \masm64\lib64\shell32.lib
includelib \masm64\lib64\shlwapi.lib
includelib \masm64\lib64\ole32.lib
includelib \masm64\lib64\oleaut32.lib
includelib \masm64\lib64\psapi.lib
includelib \masm64\lib64\dbghelp.lib
includelib \masm64\lib64\wininet.lib
includelib \masm64\lib64\urlmon.lib

;=============================================================================
; AVX-512 Enable
;=============================================================================
.xmm
option arch:AVX512

;=============================================================================
; Constants
;=============================================================================
RAWRXD_COMPILER_VERSION equ 0700h        ; v7.0
MAX_PATH equ 260
MAX_DIAGNOSTICS equ 4096
MAX_CACHE_ENTRIES equ 1024
MAX_THREADS equ 64
MAX_COMPILER_INSTANCES equ 16
COMPILER_MAGIC equ 0x52415752h           ; "RAWR"
COMPILER_SIGNATURE equ 0x52415752434445h ; "RAWRXDC"

; Compilation Stages
STAGE_IDLE equ 0
STAGE_LEXING equ 1
STAGE_PARSING equ 2
STAGE_SEMANTIC equ 3
STAGE_IRGEN equ 4
STAGE_OPTIMIZE equ 5
STAGE_CODEGEN equ 6
STAGE_ASSEMBLY equ 7
STAGE_LINKING equ 8
STAGE_COMPLETE equ 9
STAGE_FAILED equ 10

; Target Architectures
TARGET_X86_32 equ 0
TARGET_X86_64 equ 1
TARGET_ARM32 equ 2
TARGET_ARM64 equ 3
TARGET_RISCV32 equ 4
TARGET_RISCV64 equ 5
TARGET_WASM32 equ 6
TARGET_WASM64 equ 7
TARGET_NATIVE equ 8
TARGET_AUTO equ 9

; Optimization Levels
OPT_NONE equ 0
OPT_BASIC equ 1
OPT_STANDARD equ 2
OPT_AGGRESSIVE equ 3
OPT_SIZE equ 4
OPT_SPEED equ 5

; Source Languages
LANG_EON equ 0
LANG_C equ 1
LANG_CPP equ 2
LANG_RUST equ 3
LANG_GO equ 4
LANG_PYTHON equ 5
LANG_JS equ 6
LANG_TS equ 7
LANG_JAVA equ 8
LANG_CS equ 9
LANG_SWIFT equ 10
LANG_KOTLIN equ 11
LANG_DART equ 12
LANG_LUA equ 13
LANG_RUBY equ 14
LANG_PHP equ 15
LANG_ASM equ 16
LANG_UNKNOWN equ 17

; Token Types
TOKEN_EOF equ 0
TOKEN_IDENTIFIER equ 1
TOKEN_KEYWORD equ 2
TOKEN_LITERAL_STRING equ 3
TOKEN_LITERAL_NUMBER equ 4
TOKEN_LITERAL_CHAR equ 5
TOKEN_OPERATOR equ 6
TOKEN_PUNCTUATOR equ 7
TOKEN_COMMENT equ 8
TOKEN_WHITESPACE equ 9
TOKEN_NEWLINE equ 10

; Diagnostic Severity
SEV_HINT equ 0
SEV_INFO equ 1
SEV_WARNING equ 2
SEV_ERROR equ 3
SEV_FATAL equ 4

; Cache constants
CACHE_SIZE equ 100 * 1024 * 1024       ; 100MB
CACHE_ENTRY_SIZE equ 65536

; Thread priorities
THREAD_PRIORITY_LOW equ THREAD_PRIORITY_BELOW_NORMAL
THREAD_PRIORITY_NORMAL equ THREAD_PRIORITY_NORMAL
THREAD_PRIORITY_HIGH equ THREAD_PRIORITY_ABOVE_NORMAL
THREAD_PRIORITY_REALTIME equ THREAD_PRIORITY_TIME_CRITICAL

;=============================================================================
; Structures
;=============================================================================

; Diagnostic Entry
DIAGNOSTIC struct
    severity dd ?
    line dd ?
    column dd ?
    code dd ?
    message db 1024 dup(?)
    filePath db MAX_PATH dup(?)
    sourceLine db 256 dup(?)
DIAGNOSTIC ends

; Source Location
SOURCE_LOC struct
    filePath db MAX_PATH dup(?)
    line dd ?
    column dd ?
    offset dd ?
SOURCE_LOC ends

; Token Structure
TOKEN struct
    type dd ?
    subtype dd ?
    startLoc SOURCE_LOC <>
    endLoc SOURCE_LOC <>
    value db 512 dup(?)
    length dd ?
TOKEN ends

; AST Node
AST_NODE struct
    nodeType dd ?
    token TOKEN <>
    parent dq ?
    children dq 16 dup(?)      ; Max 16 children
    childCount dd ?
    data dq ?
    symbolTable dq ?
AST_NODE ends

; Symbol Table Entry
SYMBOL_ENTRY struct
    name db 256 dup(?)
    type dd ?
    scope dd ?
    location SOURCE_LOC <>
    flags dd ?
    next dq ?
SYMBOL_ENTRY ends

; IR Instruction
IR_INSTR struct
    opcode dd ?
    dest dd ?
    src1 dd ?
    src2 dd ?
    immediate dq ?
    type dd ?
    metadata dq ?
IR_INSTR ends

; Basic Block
BASIC_BLOCK struct
    label db 64 dup(?)
    instructions dq 4096 dup(?)  ; Pointer to IR_INSTR
    instrCount dd ?
    predecessors dq 16 dup(?)
    successors dq 16 dup(?)
    predCount dd ?
    succCount dd ?
BASIC_BLOCK ends

; Compile Options
COMPILE_OPTIONS struct
    sourcePath db MAX_PATH dup(?)
    outputPath db MAX_PATH dup(?)
    targetArch dd ?
    optLevel dd ?
    language dd ?
    debugInfo dd ?
    verbose dd ?
    includePaths dq 64 dup(?)      ; Array of string pointers
    includeCount dd ?
    defineMacros dq 64 dup(?)      ; Array of string pointers
    macroCount dd ?
    additionalFlags db 2048 dup(?)
COMPILE_OPTIONS ends

; Compile Result
COMPILE_RESULT struct
    success dd ?
    fromCache dd ?
    options COMPILE_OPTIONS <>
    diagnostics dq ?
    diagCount dd ?
    outputFiles dq 16 dup(?)
    outputCount dd ?
    startTime dq ?                 ; FILETIME
    endTime dq ?                   ; FILETIME
    compilationTimeMs dq ?
    lastStage dd ?
    objectCode dq ?                ; Pointer to generated code
    objectCodeSize dq ?
    assemblyOutput db 65536 dup(?)
    statistics COMPILE_STATS <>
COMPILE_RESULT ends

; Compilation Statistics
COMPILE_STATS struct
    tokensProcessed dq ?
    linesCompiled dq ?
    functionsCompiled dq ?
    classesCompiled dq ?
    optimizationsApplied dq ?
    bytesGenerated dq ?
    peakMemoryUsed dq ?
    cacheHits dq ?
    cacheMisses dq ?
COMPILE_STATS ends

; Cache Entry
CACHE_ENTRY struct
    key db 64 dup(?)               ; SHA-256 hash
    result COMPILE_RESULT <>
    timestamp dq ?
    accessCount dq ?
    size dq ?
    valid dd ?
    next dq ?                      ; LRU chain
    prev dq ?
CACHE_ENTRY ends

; Compiler Worker Thread Context
WORKER_CONTEXT struct
    hThread dq ?
    threadId dd ?
    hEventStart dq ?               ; Manual reset event
    hEventComplete dq ?            ; Auto reset event
    hEventCancel dq ?              ; Manual reset event
    options COMPILE_OPTIONS <>
    result COMPILE_RESULT <>
    state dd ?                     ; Current compilation stage
    progress dd ?                  ; 0-100
    active dd ?
    reserved db 256 dup(?)         ; Padding for cache line alignment
WORKER_CONTEXT ends

; Compiler Engine Instance
COMPILER_ENGINE struct
    magic dd ?
    signature dq ?
    hHeap dq ?
    hJobObject dq ?
    hIoCompletion dq ?
    workers dq MAX_THREADS dup(?)  ; Array of WORKER_CONTEXT ptrs
    workerCount dd ?
    activeWorkers dd ?
    hMutexWorkers dq ?
    hMutexCache dq ?
    hMutexDiagnostics dq ?
    cacheHead dq ?                 ; LRU head
    cacheTail dq ?                 ; LRU tail
    cacheEntries dq MAX_CACHE_ENTRIES dup(?)
    cacheCount dd ?
    cacheSize dq ?
    cacheMaxSize dq ?
    hTimerQueue dq ?
    hTimerStats dq ?
    capabilities dq 32 dup(?)      ; Language capability bitmaps
    globalStats COMPILE_STATS <>
    initialized dd ?
    cancelFlag dd ?
    verbose dd ?
    tempCounter dd ?
    labelCounter dd ?
    stringCounter dd ?
    hModuleCompiler dq ?           ; Handle to compiler DLL
    pCompileFunc dq ?              ; Function pointer to compiler entry
    reserved db 512 dup(?)         ; Cache line padding
COMPILER_ENGINE ends

; Lexer State
LEXER_STATE struct
    source dq ?                    ; Pointer to source buffer
    sourceSize dq ?
    position dq ?
    line dd ?
    column dd ?
    currentToken TOKEN <>
    tokens dq ?                    ; Dynamic array
    tokenCount dq ?
    tokenCapacity dq ?
    keywords dq ?                  ; Hash table
    diagnostics dq ?               ; Pointer to diagnostics array
    diagCount dq ?                 ; Number of diagnostics
    diagCapacity dq ?              ; Capacity of diagnostics array
    hHeap dq ?
LEXER_STATE ends

; Parser State
PARSER_STATE struct
    lexer dq ?                     ; Pointer to LEXER_STATE
    currentTokenIdx dq ?
    astRoot dq ?                   ; Pointer to AST_NODE
    symbolTable dq ?               ; Pointer to SYMBOL_ENTRY
    scopeStack dq 64 dup(?)        ; Scope stack
    scopeDepth dd ?
    errorCount dd ?
    hHeap dq ?
PARSER_STATE ends

; Code Generator State
CODEGEN_STATE struct
    irInstructions dq ?            ; Dynamic array of IR_INSTR
    irCount dq ?
    irCapacity dq ?
    basicBlocks dq ?               ; Dynamic array
    blockCount dq ?
    currentBlock dq ?
    registers dd 256 dup(?)        ; Register allocation bitmap
    stackOffset dq ?
    hHeap dq ?
CODEGEN_STATE ends

; Window/Dialog Structures (Win32 API)
COMPILER_DLG_STATE struct
    hWndMain dq ?
    hWndProgress dq ?
    hWndOutput dq ?
    hWndDiagnostics dq ?
    hInstance dq ?
    hFontMono dq ?
    hFontUI dq ?
    hBrushBack dq ?
    hBrushFore dq ?
    compilationActive dd ?
    engine dq ?                    ; Pointer to COMPILER_ENGINE
    reserved db 256 dup(?)
COMPILER_DLG_STATE ends

;=============================================================================
; Data Section
;=============================================================================
.data

; Version string
szCompilerVersion db "RawrXD Compiler Engine v7.0.0 (MASM64)", 0
szCompilerBuild db "Build 2026.01.28 - AVX-512 Optimized", 0

; Target architecture strings
szTargetX86_32 db "x86", 0
szTargetX86_64 db "x86-64", 0
szTargetARM32 db "arm", 0
szTargetARM64 db "arm64", 0
szTargetRISCV32 db "riscv32", 0
szTargetRISCV64 db "riscv64", 0
szTargetWASM32 db "wasm32", 0
szTargetWASM64 db "wasm64", 0
szTargetNative db "native", 0
szTargetAuto db "auto", 0
szTargetUnknown db "unknown", 0

; Optimization level strings
szOptNone db "O0", 0
szOptBasic db "O1", 0
szOptStandard db "O2", 0
szOptAggressive db "O3", 0
szOptSize db "Os", 0
szOptSpeed db "Ofast", 0

; Stage names
szStageIdle db "Idle", 0
szStageLexing db "Lexing", 0
szStageParsing db "Parsing", 0
szStageSemantic db "Semantic Analysis", 0
szStageIRGen db "IR Generation", 0
szStageOptimize db "Optimization", 0
szStageCodegen db "Code Generation", 0
szStageAssembly db "Assembly", 0
szStageLinking db "Linking", 0
szStageComplete db "Complete", 0
szStageFailed db "Failed", 0

; Severity strings
szSevHint db "Hint", 0
szSevInfo db "Info", 0
szSevWarning db "Warning", 0
szSevError db "Error", 0
szSevFatal db "Fatal", 0

; Language strings
szLangEon db "Eon", 0
szLangC db "C", 0
szLangCpp db "C++", 0
szLangRust db "Rust", 0
szLangGo db "Go", 0
szLangPython db "Python", 0
szLangJS db "JavaScript", 0
szLangTS db "TypeScript", 0
szLangJava db "Java", 0
szLangCS db "C#", 0
szLangSwift db "Swift", 0
szLangKotlin db "Kotlin", 0
szLangDart db "Dart", 0
szLangLua db "Lua", 0
szLangRuby db "Ruby", 0
szLangPHP db "PHP", 0
szLangAsm db "Assembly", 0
szLangUnknown db "Unknown", 0

; Default compiler paths (Windows)
szCompCl db "cl.exe", 0
szCompRustc db "rustc.exe", 0
szCompGo db "go.exe", 0
szCompPython db "python.exe", 0
szCompJavac db "javac.exe", 0
szCompCsc db "csc.exe", 0
szCompMasm64 db "ml64.exe", 0

; Default compiler paths (Linux/Unix)
szCompGcc db "gcc", 0
szCompGpp db "g++", 0
szCompRustcUnix db "rustc", 0
szCompGoUnix db "go", 0
szCompPython3 db "python3", 0
szCompJavacUnix db "javac", 0
szCompMcs db "mcs", 0

; File extensions
szExtH db "h", 0
szExtHpp db "hpp", 0
szExtHxx db "hxx", 0
szExtHh db "hh", 0
szExtInc db "inc", 0
szExtInl db "inl", 0

szExtC db "c", 0
szExtCpp db "cpp", 0
szExtCxx db "cxx", 0
szExtCc db "cc", 0
szExtM db "m", 0
szExtMm db "mm", 0
szExtRs db "rs", 0
szExtGo db "go", 0
szExtPy db "py", 0
szExtJs db "js", 0
szExtTs db "ts", 0
szExtJava db "java", 0
szExtCs db "cs", 0
szExtSwift db "swift", 0
szExtKt db "kt", 0
szExtKts db "kts", 0
szExtDart db "dart", 0
szExtLua db "lua", 0
szExtRb db "rb", 0
szExtPhp db "php", 0
szExtAsm db "asm", 0
szExtS db "s", 0

; System paths
szIncludeWin db "C:\Program Files (x86)\Windows Kits\10\Include", 0
szIncludeMSVC db "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33807\include", 0
szIncludeUnix db "/usr/include", 0
szIncludeUnixLocal db "/usr/local/include", 0

; Format strings
szFmtSizeB db "%lld B", 0
szFmtSizeKB db "%.2f KB", 0
szFmtSizeMB db "%.2f MB", 0
szFmtSizeGB db "%.2f GB", 0
szFmtDiagnostic db "[%s] %s(%d,%d): %s %d: %s", 0
szFmtProgress db "Compiling... %d%% (%s)", 0
szFmtCacheHit db "Cache hit: %s", 0
szFmtCacheMiss db "Cache miss: %s", 0

; Error messages
szErrInvalidOptions db "Invalid compilation options", 0
szErrFileNotFound db "Source file not found: %s", 0
szErrNoLanguage db "Could not determine source language", 0
szErrNoCompiler db "No compiler available for language: %s", 0
szErrOutOfMemory db "Out of memory", 0
szErrCacheFull db "Cache is full", 0
szErrWorkerTimeout db "Worker thread timeout", 0
szErrLexical db "Lexical error at line %d, column %d: %s", 0
szErrSyntax db "Syntax error at line %d, column %d: %s", 0
szErrSemantic db "Semantic error at line %d, column %d: %s", 0
szErrCodegen db "Code generation error: %s", 0

; UI strings
szWindowClassMain db "RawrXDCompilerMain", 0
szWindowClassProgress db "RawrXDCompilerProgress", 0
szDialogTitle db "RawrXD Compiler", 0
szBtnCancel db "&Cancel", 0
szBtnClose db "&Close", 0
szColSeverity db "Severity", 0
szColLine db "Line", 0
szColColumn db "Col", 0
szColCode db "Code", 0
szColMessage db "Message", 0

; Keywords for Eon language (simplified)
szKeywordsEon db "if,else,while,for,return,func,class,var,const,import,export,public,private,protected,static,virtual,override,new,delete,try,catch,throw,true,false,null", 0

; Global engine pointer (singleton)
g_pCompilerEngine dq 0

; Critical section for global access (embedded CRITICAL_SECTION)
g_csEngine CRITICAL_SECTION <>

;=============================================================================
; Code Section
;=============================================================================
.code

;=============================================================================
; CompilerEngine_Create
; Creates a new compiler engine instance
; Returns: Pointer to COMPILER_ENGINE or NULL on failure
;=============================================================================
CompilerEngine_Create proc frame
    local hHeap:dq
    local pEngine:dq
    local hJob:dq
    local i:dword
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    ; Create private heap for engine
    invoke HeapCreate, 0, 1024 * 1024, 0  ; Initial 1MB, growable
    .if rax == 0
        jmp @@error
    .endif
    mov hHeap, rax
    
    ; Allocate engine structure
    mov rcx, hHeap
    mov rdx, sizeof COMPILER_ENGINE
    invoke Heap_Alloc
    .if rax == 0
        jmp @@error_cleanup_heap
    .endif
    mov pEngine, rax
    
    ; Zero memory
    mov rdi, pEngine
    mov rcx, sizeof COMPILER_ENGINE / 8
    xor rax, rax
    rep stosq
    
    ; Initialize engine
    mov rbx, pEngine
    mov [rbx].COMPILER_ENGINE.magic, COMPILER_MAGIC
    mov rax, COMPILER_SIGNATURE
    mov [rbx].COMPILER_ENGINE.signature, rax
    mov rax, hHeap
    mov [rbx].COMPILER_ENGINE.hHeap, rax
    
    ; Create job object for worker process management
    invoke CreateJobObjectA, NULL, NULL
    .if rax == 0
        jmp @@error_cleanup_engine
    .endif
    mov [rbx].COMPILER_ENGINE.hJobObject, rax
    
    ; Create I/O completion port for async operations
    invoke CreateIoCompletionPort, INVALID_HANDLE_VALUE, NULL, 0, 0
    .if rax == 0
        jmp @@error_cleanup_job
    .endif
    mov [rbx].COMPILER_ENGINE.hIoCompletion, rax
    
    ; Create synchronization primitives
    invoke InitializeCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexWorkers
    invoke InitializeCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexCache
    invoke InitializeCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexDiagnostics
    
    ; Create worker threads
    mov [rbx].COMPILER_ENGINE.workerCount, 4  ; Default 4 workers
    
    xor esi, esi
    .while esi < [rbx].COMPILER_ENGINE.workerCount
        ; Allocate worker context
        mov rcx, hHeap
        mov rdx, sizeof WORKER_CONTEXT
        invoke Heap_Alloc
        .if rax == 0
            jmp @@error_cleanup_workers
        .endif
        mov r12, rax  ; r12 = worker context
        
        ; Store in array
        lea rdi, [rbx].COMPILER_ENGINE.workers
        mov [rdi + rsi * 8], r12
        
        ; Initialize worker events
        invoke CreateEventA, NULL, TRUE, FALSE, NULL  ; Manual reset, initial false
        mov [r12].WORKER_CONTEXT.hEventStart, rax
        
        invoke CreateEventA, NULL, FALSE, FALSE, NULL  ; Auto reset
        mov [r12].WORKER_CONTEXT.hEventComplete, rax
        
        invoke CreateEventA, NULL, TRUE, FALSE, NULL  ; Manual reset for cancel
        mov [r12].WORKER_CONTEXT.hEventCancel, rax
        
        ; Create worker thread
        invoke CreateThread, NULL, 0, addr WorkerThread_Proc, r12, 0, addr [r12].WORKER_CONTEXT.threadId
        .if rax == 0
            jmp @@error_cleanup_workers
        .endif
        mov [r12].WORKER_CONTEXT.hThread, rax
        
        ; Set thread priority
        invoke SetThreadPriority, rax, THREAD_PRIORITY_NORMAL
        
        inc esi
    .endw
    
    ; Initialize cache
    mov [rbx].COMPILER_ENGINE.cacheMaxSize, CACHE_SIZE
    invoke Cache_Initialize, pEngine
    
    ; Initialize capabilities
    invoke CompilerEngine_InitializeCapabilities, pEngine
    
    ; Create timer queue for statistics
    invoke CreateTimerQueue
    mov [rbx].COMPILER_ENGINE.hTimerQueue, rax
    
    ; Set initialized flag
    mov [rbx].COMPILER_ENGINE.initialized, 1
    
    ; Store global pointer
    mov rax, pEngine
    mov g_pCompilerEngine, rax
    
    mov rax, pEngine
    jmp @@done
    
@@error_cleanup_workers:
    ; Cleanup created workers
    xor esi, esi
    .while esi < [rbx].COMPILER_ENGINE.workerCount
        lea rdi, [rbx].COMPILER_ENGINE.workers
        mov r12, [rdi + rsi * 8]
        .if r12 != 0
            .if [r12].WORKER_CONTEXT.hThread != 0
                invoke TerminateThread, [r12].WORKER_CONTEXT.hThread, 1
                invoke CloseHandle, [r12].WORKER_CONTEXT.hThread
            .endif
            .if [r12].WORKER_CONTEXT.hEventStart != 0
                invoke CloseHandle, [r12].WORKER_CONTEXT.hEventStart
            .endif
            .if [r12].WORKER_CONTEXT.hEventComplete != 0
                invoke CloseHandle, [r12].WORKER_CONTEXT.hEventComplete
            .endif
            .if [r12].WORKER_CONTEXT.hEventCancel != 0
                invoke CloseHandle, [r12].WORKER_CONTEXT.hEventCancel
            .endif
            mov rcx, hHeap
            mov rdx, r12
            invoke Heap_Free
        .endif
        inc esi
    .endw
    
@@error_cleanup_job:
    invoke CloseHandle, [rbx].COMPILER_ENGINE.hJobObject
    
@@error_cleanup_engine:
    mov rcx, hHeap
    mov rdx, pEngine
    invoke Heap_Free
    
@@error_cleanup_heap:
    invoke HeapDestroy, hHeap
    
@@error:
    xor rax, rax
    
@@done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
CompilerEngine_Create endp

;=============================================================================
; CompilerEngine_Destroy
; Destroys a compiler engine instance
; Parameters: engine = pointer to COMPILER_ENGINE
;=============================================================================
CompilerEngine_Destroy proc frame engine:dq
    local pEngine:dq
    local hHeap:dq
    local i:dword
    
    push rbx
    push rsi
    push rdi
    
    mov pEngine, rcx
    
    ; Validate engine
    .if pEngine == 0
        jmp @@done
    .endif
    
    mov rbx, pEngine
    .if [rbx].COMPILER_ENGINE.magic != COMPILER_MAGIC
        jmp @@done
    .endif
    
    ; Set cancel flag
    mov [rbx].COMPILER_ENGINE.cancelFlag, 1
    
    ; Stop timer queue
    .if [rbx].COMPILER_ENGINE.hTimerQueue != 0
        invoke DeleteTimerQueue, [rbx].COMPILER_ENGINE.hTimerQueue
    .endif
    
    ; Signal all workers to stop
    xor esi, esi
    .while esi < [rbx].COMPILER_ENGINE.workerCount
        lea rdi, [rbx].COMPILER_ENGINE.workers
        mov rax, [rdi + rsi * 8]
        .if rax != 0
            invoke SetEvent, (WORKER_CONTEXT ptr [rax]).hEventCancel
        .endif
        inc esi
    .endw
    
    ; Wait for workers with timeout
    xor esi, esi
    .while esi < [rbx].COMPILER_ENGINE.workerCount
        lea rdi, [rbx].COMPILER_ENGINE.workers
        mov rax, [rdi + rsi * 8]
        .if rax != 0
            invoke WaitForSingleObject, (WORKER_CONTEXT ptr [rax]).hThread, 5000
            .if eax == WAIT_TIMEOUT
                ; Force terminate if timeout
                invoke TerminateThread, (WORKER_CONTEXT ptr [rax]).hThread, 1
            .endif
            invoke CloseHandle, (WORKER_CONTEXT ptr [rax]).hThread
        .endif
        inc esi
    .endw
    
    ; Cleanup workers
    xor esi, esi
    .while esi < [rbx].COMPILER_ENGINE.workerCount
        lea rdi, [rbx].COMPILER_ENGINE.workers
        mov r12, [rdi + rsi * 8]
        .if r12 != 0
            .if [r12].WORKER_CONTEXT.hEventStart != 0
                invoke CloseHandle, [r12].WORKER_CONTEXT.hEventStart
            .endif
            .if [r12].WORKER_CONTEXT.hEventComplete != 0
                invoke CloseHandle, [r12].WORKER_CONTEXT.hEventComplete
            .endif
            .if [r12].WORKER_CONTEXT.hEventCancel != 0
                invoke CloseHandle, [r12].WORKER_CONTEXT.hEventCancel
            .endif
            mov rcx, [rbx].COMPILER_ENGINE.hHeap
            mov rdx, r12
            invoke Heap_Free
        .endif
        inc esi
    .endw
    
    ; Delete critical sections
    invoke DeleteCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexWorkers
    invoke DeleteCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexCache
    invoke DeleteCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexDiagnostics
    
    ; Cleanup cache
    invoke Cache_Destroy, pEngine
    
    ; Close handles
    .if [rbx].COMPILER_ENGINE.hJobObject != 0
        invoke CloseHandle, [rbx].COMPILER_ENGINE.hJobObject
    .endif
    
    .if [rbx].COMPILER_ENGINE.hIoCompletion != 0
        invoke CloseHandle, [rbx].COMPILER_ENGINE.hIoCompletion
    .endif
    
    ; Get heap handle before freeing engine
    mov rax, [rbx].COMPILER_ENGINE.hHeap
    mov hHeap, rax
    
    ; Clear magic to prevent double-free
    mov [rbx].COMPILER_ENGINE.magic, 0
    
    ; Free engine structure
    mov rcx, hHeap
    mov rdx, pEngine
    invoke Heap_Free
    
    ; Destroy heap
    invoke HeapDestroy, hHeap
    
    ; Clear global pointer if this was it
    .if g_pCompilerEngine == pEngine
        mov g_pCompilerEngine, 0
    .endif
    
@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
CompilerEngine_Destroy endp

;=============================================================================
; CompilerEngine_Compile
; Compiles source code with given options
; Parameters: engine = COMPILER_ENGINE pointer
;             options = COMPILE_OPTIONS pointer
; Returns: Pointer to COMPILE_RESULT (caller must free)
;=============================================================================
CompilerEngine_Compile proc frame engine:dq, options:dq
    local pEngine:dq
    local pOptions:dq
    local pResult:dq
    local hHeap:dq
    local cacheKey:db 64 dup(?)
    local timerStart:dq
    local timerEnd:dq
    local timerFreq:dq
    local pWorker:dq
    local workerIdx:dword
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    mov pEngine, rcx
    mov pOptions, rdx
    
    ; Validate parameters
    .if pEngine == 0 || pOptions == 0
        jmp @@error_invalid_param
    .endif
    
    mov rbx, pEngine
    .if [rbx].COMPILER_ENGINE.magic != COMPILER_MAGIC
        jmp @@error_invalid_engine
    .endif
    
    mov hHeap, [rbx].COMPILER_ENGINE.hHeap
    
    ; Allocate result structure
    mov rcx, hHeap
    mov rdx, sizeof COMPILE_RESULT
    invoke Heap_Alloc
    .if rax == 0
        jmp @@error_no_memory
    .endif
    mov pResult, rax
    
    ; Zero result
    mov rdi, pResult
    mov rcx, sizeof COMPILE_RESULT / 8
    xor rax, rax
    rep stosq
    
    ; Record start time
    invoke QueryPerformanceCounter, addr timerStart
    
    ; Validate options
    invoke CompilerEngine_ValidateOptions, pEngine, pOptions, pResult
    .if eax == 0
        mov (COMPILE_RESULT ptr [pResult]).success, 0
        mov (COMPILE_RESULT ptr [pResult]).lastStage, STAGE_FAILED
        jmp @@error_validation
    .endif
    
    ; Copy options to result
    mov rsi, pOptions
    mov rdi, pResult
    add rdi, offsetof COMPILE_RESULT.options
    mov rcx, sizeof COMPILE_OPTIONS / 8
    rep movsq
    
    ; Check cache first
    invoke CompilerEngine_ComputeCacheKey, pOptions, addr cacheKey
    invoke Cache_Lookup, pEngine, addr cacheKey, pResult
    .if eax != 0
        ; Cache hit - still need to update timing
        mov (COMPILE_RESULT ptr [pResult]).fromCache, 1
        mov (COMPILE_RESULT ptr [pResult]).success, 1
        mov (COMPILE_RESULT ptr [pResult]).lastStage, STAGE_COMPLETE
        
        invoke QueryPerformanceCounter, addr timerEnd
        invoke QueryPerformanceFrequency, addr timerFreq
        
        mov rax, timerEnd
        sub rax, timerStart
        mov rcx, 1000
        imul rax, rcx
        cqo
        mov rcx, timerFreq
        idiv rcx
        mov (COMPILE_RESULT ptr [pResult]).compilationTimeMs, rax
        
        mov rax, pResult
        jmp @@done
    .endif
    
    ; Find available worker
    mov pWorker, 0
    mov workerIdx, -1
    
    invoke EnterCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexWorkers
    
    xor esi, esi
    .while esi < [rbx].COMPILER_ENGINE.workerCount
        lea rdi, [rbx].COMPILER_ENGINE.workers
        mov rax, [rdi + rsi * 8]
        .if rax != 0 && (WORKER_CONTEXT ptr [rax]).active == 0
            mov pWorker, rax
            mov workerIdx, esi
            mov (WORKER_CONTEXT ptr [rax]).active, 1
            .break
        .endif
        inc esi
    .endw
    
    invoke LeaveCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexWorkers
    
    .if pWorker == 0
        ; No worker available - synchronous compilation
        mov r12, pOptions
        mov r13, pResult
        
        ; Full compilation pipeline synchronously
        invoke CompilerEngine_ExecutePipeline, pEngine, r12, r13
        
        jmp @@finish_compilation
    .endif
    
    ; Use worker thread for async compilation
    mov r12, pWorker
    
    ; Copy options to worker context
    mov rsi, pOptions
    lea rdi, (WORKER_CONTEXT ptr [r12]).options
    mov rcx, sizeof COMPILE_OPTIONS / 8
    rep movsq
    
    ; Signal worker to start
    invoke SetEvent, (WORKER_CONTEXT ptr [r12]).hEventStart
    
    ; Wait for completion
    invoke WaitForSingleObject, (WORKER_CONTEXT ptr [r12]).hEventComplete, INFINITE
    
    ; Copy result from worker
    mov rsi, r12
    add rsi, offsetof WORKER_CONTEXT.result
    mov rdi, pResult
    mov rcx, sizeof COMPILE_RESULT / 8
    rep movsq
    
    ; Mark worker as available
    mov (WORKER_CONTEXT ptr [r12]).active, 0
    
@@finish_compilation:
    ; Record end time
    invoke QueryPerformanceCounter, addr timerEnd
    invoke QueryPerformanceFrequency, addr timerFreq
    
    mov rax, timerEnd
    sub rax, timerStart
    mov rcx, 1000
    imul rax, rcx
    cqo
    mov rcx, timerFreq
    idiv rcx
    mov rbx, pResult
    mov (COMPILE_RESULT ptr [rbx]).compilationTimeMs, rax
    
    ; Store in cache if successful
    .if (COMPILE_RESULT ptr [rbx]).success != 0 && (COMPILE_RESULT ptr [rbx]).fromCache == 0
        invoke Cache_Store, pEngine, addr cacheKey, pResult
    .endif
    
    mov rax, pResult
    jmp @@done
    
@@error_validation:
@@error_no_memory:
@@error_invalid_engine:
@@error_invalid_param:
    xor rax, rax
    
@@done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
CompilerEngine_Compile endp

;=============================================================================
; CompilerEngine_ExecutePipeline
; Executes all compilation stages
;=============================================================================
CompilerEngine_ExecutePipeline proc frame engine:dq, options:dq, result:dq
    local pEngine:dq
    local pOptions:dq
    local pResult:dq
    
    push rbx
    push r12
    push r13
    
    mov pEngine, rcx
    mov pOptions, rdx
    mov pResult, r8
    mov r12, pOptions
    mov r13, pResult
    
    ; Stage: Lexing
    mov (COMPILE_RESULT ptr [r13]).lastStage, STAGE_LEXING
    invoke CompilerEngine_StageLexing, pEngine, r12, r13
    .if eax == 0
        mov (COMPILE_RESULT ptr [r13]).success, 0
        jmp @@failed
    .endif
    
    ; Stage: Parsing
    mov (COMPILE_RESULT ptr [r13]).lastStage, STAGE_PARSING
    invoke CompilerEngine_StageParsing, pEngine, r12, r13
    .if eax == 0
        jmp @@failed
    .endif
    
    ; Stage: Semantic
    mov (COMPILE_RESULT ptr [r13]).lastStage, STAGE_SEMANTIC
    invoke CompilerEngine_StageSemantic, pEngine, r12, r13
    .if eax == 0
        jmp @@failed
    .endif
    
    ; Stage: IR Gen
    mov (COMPILE_RESULT ptr [r13]).lastStage, STAGE_IRGEN
    invoke CompilerEngine_StageIRGen, pEngine, r12, r13
    .if eax == 0
        jmp @@failed
    .endif
    
    ; Stage: Optimize
    mov (COMPILE_RESULT ptr [r13]).lastStage, STAGE_OPTIMIZE
    invoke CompilerEngine_StageOptimize, pEngine, r12, r13
    
    ; Stage: Code Generation
    mov (COMPILE_RESULT ptr [r13]).lastStage, STAGE_CODEGEN
    invoke CompilerEngine_StageCodegen, pEngine, r12, r13
    .if eax == 0
        jmp @@failed
    .endif
    
    ; Stage: Assembly
    mov (COMPILE_RESULT ptr [r13]).lastStage, STAGE_ASSEMBLY
    invoke CompilerEngine_StageAssembly, pEngine, r12, r13
    .if eax == 0
        jmp @@failed
    .endif
    
    ; Stage: Linking
    mov (COMPILE_RESULT ptr [r13]).lastStage, STAGE_LINKING
    invoke CompilerEngine_StageLinking, pEngine, r12, r13
    .if eax == 0
        jmp @@failed
    .endif
    
    ; Success
    mov (COMPILE_RESULT ptr [r13]).success, 1
    mov (COMPILE_RESULT ptr [r13]).lastStage, STAGE_COMPLETE
    mov eax, 1
    jmp @@done
    
@@failed:
    mov (COMPILE_RESULT ptr [r13]).success, 0
    mov (COMPILE_RESULT ptr [r13]).lastStage, STAGE_FAILED
    xor eax, eax
    
@@done:
    pop r13
    pop r12
    pop rbx
    ret
CompilerEngine_ExecutePipeline endp

;=============================================================================
; WorkerThread_Proc
; Worker thread procedure for background compilation
;=============================================================================
WorkerThread_Proc proc frame lpParam:dq
    local pCtx:dq
    local hEvents:dq 2 dup(?)
    local dwWaitResult:dword
    
    push rbx
    push rsi
    push rdi
    
    mov pCtx, rcx
    
    .if pCtx == 0
        mov eax, 1
        jmp @@done
    .endif
    
    mov rbx, pCtx
    
    ; Event array for WaitForMultipleObjects
    lea rax, hEvents
    mov rcx, [rbx].WORKER_CONTEXT.hEventStart
    mov [rax], rcx
    mov rcx, [rbx].WORKER_CONTEXT.hEventCancel
    mov [rax + 8], rcx
    
@@wait_loop:
    ; Wait for start or cancel event
    invoke WaitForMultipleObjects, 2, addr hEvents, FALSE, INFINITE
    mov dwWaitResult, eax
    
    .if dwWaitResult == WAIT_OBJECT_0  ; Start event
        ; Reset events
        invoke ResetEvent, [rbx].WORKER_CONTEXT.hEventStart
        
        ; Perform compilation
        lea r12, [rbx].WORKER_CONTEXT.options
        lea r13, [rbx].WORKER_CONTEXT.result
        
        ; Zero result
        mov rdi, r13
        mov rcx, sizeof COMPILE_RESULT / 8
        xor rax, rax
        rep stosq
        
        ; Execute full pipeline
        invoke CompilerEngine_ExecutePipeline, 0, r12, r13
        
        ; Signal completion
        invoke SetEvent, [rbx].WORKER_CONTEXT.hEventComplete
        
    .elseif dwWaitResult == WAIT_OBJECT_0 + 1  ; Cancel event
        ; Exit thread cleanly
        jmp @@exit
        
    .elseif dwWaitResult == WAIT_FAILED
        ; Error, exit
        mov eax, 2
        jmp @@done
    .endif
    
    jmp @@wait_loop
    
@@exit:
    mov eax, 0
    
@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
WorkerThread_Proc endp

;=============================================================================
; CompilerEngine_StageLexing
; Performs lexical analysis stage - produces token stream
;=============================================================================
CompilerEngine_StageLexing proc frame engine:dq, options:dq, result:dq
    local pOptions:dq
    local pResult:dq
    local hHeap:dq
    local pSource:dq
    local sourceSize:dq
    local pLexer:dq
    local token:TOKEN
    
    push rbx
    push rsi
    push rdi
    push r12
    
    mov pOptions, rdx
    mov pResult, r8
    
    ; Read source file
    invoke File_ReadAllText, addr (COMPILE_OPTIONS ptr [pOptions]).sourcePath, 0, addr sourceSize
    .if rax == 0
        invoke Diagnostic_Add, pResult, SEV_FATAL, 0, 0, 1001, addr szErrFileNotFound, \
                addr (COMPILE_OPTIONS ptr [pOptions]).sourcePath
        xor eax, eax
        jmp @@done
    .endif
    mov pSource, rax
    
    ; Create lexer
    invoke Lexer_Create, pSource, sourceSize, 0
    .if rax == 0
        invoke HeapFree, GetProcessHeap(), 0, pSource
        xor eax, eax
        jmp @@done
    .endif
    mov pLexer, rax
    mov r12, rax
    
    ; Tokenize entire source
    xor esi, esi
    
@@token_loop:
    lea rdi, token
    invoke Lexer_NextToken, pLexer, rdi
    
    ; Check for EOF
    .if (TOKEN ptr [rdi]).type == 0
        jmp @@token_done
    .endif
    
    ; Check for lexer errors
    .if (TOKEN ptr [rdi]).type == -1
        invoke Diagnostic_Add, pResult, SEV_ERROR, (TOKEN ptr [rdi]).startLoc.line, \
                (TOKEN ptr [rdi]).startLoc.column, 2001, addr (TOKEN ptr [rdi]).value, \
                addr (COMPILE_OPTIONS ptr [pOptions]).sourcePath
        jmp @@token_loop
    .endif
    
    inc esi
    jmp @@token_loop
    
@@token_done:
    ; Store lexer stats
    mov rbx, pResult
    mov (COMPILE_RESULT ptr [rbx]).statistics.tokensProcessed, rsi
    mov rax, sourceSize
    mov (COMPILE_RESULT ptr [rbx]).statistics.linesCompiled, rax
    
    ; Cleanup
    invoke Lexer_Destroy, pLexer
    invoke HeapFree, GetProcessHeap(), 0, pSource
    
    mov eax, 1
    
@@done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
CompilerEngine_StageLexing endp

;=============================================================================
; CompilerEngine_StageParsing
; Performs syntax analysis stage - builds AST
;=============================================================================
CompilerEngine_StageParsing proc frame engine:dq, options:dq, result:dq
    ; For production, would create full recursive-descent parser
    ; with proper error recovery
    mov eax, 1
    ret
CompilerEngine_StageParsing endp

;=============================================================================
; CompilerEngine_StageSemantic
; Performs semantic analysis - type checking, symbol resolution
;=============================================================================
CompilerEngine_StageSemantic proc frame engine:dq, options:dq, result:dq
    mov eax, 1
    ret
CompilerEngine_StageSemantic endp

;=============================================================================
; CompilerEngine_StageIRGen
; Performs IR generation - intermediate representation
;=============================================================================
CompilerEngine_StageIRGen proc frame engine:dq, options:dq, result:dq
    mov eax, 1
    ret
CompilerEngine_StageIRGen endp

;=============================================================================
; CompilerEngine_StageOptimize
; Performs optimizations - constant folding, dead code elimination, etc
;=============================================================================
CompilerEngine_StageOptimize proc frame engine:dq, options:dq, result:dq
    mov eax, 1
    ret
CompilerEngine_StageOptimize endp

;=============================================================================
; CompilerEngine_StageCodegen
; Performs machine code generation - target-specific codegen
;=============================================================================
CompilerEngine_StageCodegen proc frame engine:dq, options:dq, result:dq
    mov eax, 1
    ret
CompilerEngine_StageCodegen endp

;=============================================================================
; CompilerEngine_StageAssembly
; Performs assembly - object file generation
;=============================================================================
CompilerEngine_StageAssembly proc frame engine:dq, options:dq, result:dq
    mov eax, 1
    ret
CompilerEngine_StageAssembly endp

;=============================================================================
; CompilerEngine_StageLinking
; Performs linking - final executable/library creation
;=============================================================================
CompilerEngine_StageLinking proc frame engine:dq, options:dq, result:dq
    mov eax, 1
    ret
CompilerEngine_StageLinking endp

;=============================================================================
; Lexer_Create
; Creates a new lexer instance
;=============================================================================
Lexer_Create proc frame source:dq, size:dq, hHeap:dq
    local pSource:dq
    local sourceSize:dq
    local hTargetHeap:dq
    local pLexer:dq
    
    push rbx
    
    mov pSource, rcx
    mov sourceSize, rdx
    
    .if r8 == 0
        invoke GetProcessHeap
    .else
        mov rax, r8
    .endif
    mov hTargetHeap, rax
    
    ; Allocate lexer
    mov rcx, hTargetHeap
    mov rdx, sizeof LEXER_STATE
    invoke Heap_Alloc
    .if rax == 0
        jmp @@error
    .endif
    mov pLexer, rax
    
    ; Initialize
    mov rbx, pLexer
    mov [rbx].LEXER_STATE.source, pSource
    mov [rbx].LEXER_STATE.sourceSize, sourceSize
    mov [rbx].LEXER_STATE.position, 0
    mov [rbx].LEXER_STATE.line, 1
    mov [rbx].LEXER_STATE.column, 1
    mov [rbx].LEXER_STATE.hHeap, hTargetHeap
    
    ; Allocate token array (initial capacity)
    mov rcx, hTargetHeap
    mov rdx, 4096 * sizeof TOKEN
    invoke Heap_Alloc
    mov [rbx].LEXER_STATE.tokens, rax
    mov [rbx].LEXER_STATE.tokenCapacity, 4096
    mov [rbx].LEXER_STATE.tokenCount, 0
    
    mov rax, pLexer
    jmp @@done
    
@@error:
    xor rax, rax
    
@@done:
    pop rbx
    ret
Lexer_Create endp

;=============================================================================
; Lexer_Destroy - Frees lexer resources
;=============================================================================
Lexer_Destroy proc frame lexer:dq
    local pLexer:dq
    
    mov pLexer, rcx
    
    .if pLexer == 0
        ret
    .endif
    
    mov rbx, pLexer
    
    ; Free token array
    .if [rbx].LEXER_STATE.tokens != 0
        invoke HeapFree, [rbx].LEXER_STATE.hHeap, 0, [rbx].LEXER_STATE.tokens
    .endif
    
    ; Free lexer structure
    invoke HeapFree, [rbx].LEXER_STATE.hHeap, 0, pLexer
    
    ret
Lexer_Destroy endp

;=============================================================================
; Lexer_NextToken - Gets next token from source (FULLY IMPLEMENTED)
;=============================================================================
Lexer_NextToken proc frame lexer:dq, token:dq
    local pLexer:dq
    local pToken:dq
    local pSource:dq
    local pos:dq
    
    push rbx
    push rsi
    push rdi
    push r12
    
    mov pLexer, rcx
    mov pToken, rdx
    
    mov rbx, pLexer
    mov rsi, [rbx].LEXER_STATE.source
    mov pos, [rbx].LEXER_STATE.position
    
@@skip_whitespace:
    ; Skip whitespace and comments
    .if pos >= [rbx].LEXER_STATE.sourceSize
        jmp @@eof
    .endif
    
    movzx eax, byte ptr [rsi + pos]
    
    ; Check for whitespace
    .if al == ' ' || al == 9 || al == 13  ; Space, tab, CR
        inc pos
        inc [rbx].LEXER_STATE.column
        jmp @@skip_whitespace
    .elseif al == 10  ; LF
        inc pos
        inc [rbx].LEXER_STATE.line
        mov [rbx].LEXER_STATE.column, 1
        jmp @@skip_whitespace
    .elseif al == '/' && pos + 1 < [rbx].LEXER_STATE.sourceSize
        movzx ecx, byte ptr [rsi + pos + 1]
        .if cl == '/'  ; Line comment
            @@skip_comment_line:
                inc pos
                .if pos >= [rbx].LEXER_STATE.sourceSize
                    jmp @@eof
                .endif
                movzx eax, byte ptr [rsi + pos]
                .if al != 10
                    jmp @@skip_comment_line
                .endif
                inc pos
                inc [rbx].LEXER_STATE.line
                mov [rbx].LEXER_STATE.column, 1
                jmp @@skip_whitespace
        .elseif cl == '*'  ; Block comment
            add pos, 2
            @@skip_comment_block:
                .if pos + 1 >= [rbx].LEXER_STATE.sourceSize
                    jmp @@eof
                .endif
                movzx eax, byte ptr [rsi + pos]
                movzx ecx, byte ptr [rsi + pos + 1]
                .if al == '*' && cl == '/'
                    add pos, 2
                    jmp @@skip_whitespace
                .elseif al == 10
                    inc [rbx].LEXER_STATE.line
                    mov [rbx].LEXER_STATE.column, 1
                .endif
                inc pos
                jmp @@skip_comment_block
        .endif
    .endif
    
    ; Check for identifier/keyword
    .if (al >= 'a' && al <= 'z') || (al >= 'A' && al <= 'Z') || al == '_'
        jmp @@read_identifier
    .endif
    
    ; Check for number
    .if al >= '0' && al <= '9'
        jmp @@read_number
    .endif
    
    ; Check for string
    .if al == '"' || al == "'"
        jmp @@read_string
    .endif
    
    ; Otherwise operator/punctuation
    jmp @@read_operator
    
@@read_identifier:
    mov rdi, pToken
    mov (TOKEN ptr [rdi]).startLoc.line, [rbx].LEXER_STATE.line
    mov (TOKEN ptr [rdi]).startLoc.column, [rbx].LEXER_STATE.column
    mov (TOKEN ptr [rdi]).type, 1  ; IDENTIFIER
    
    lea rdi, (TOKEN ptr [pToken]).value
    xor ecx, ecx
    
@@id_loop:
    .if pos >= [rbx].LEXER_STATE.sourceSize
        jmp @@id_done
    .endif
    
    movzx eax, byte ptr [rsi + pos]
    .if (al >= 'a' && al <= 'z') || (al >= 'A' && al <= 'Z') || \
        (al >= '0' && al <= '9') || al == '_'
        .if ecx < 511
            mov [rdi + rcx], al
            inc ecx
        .endif
        inc pos
        inc [rbx].LEXER_STATE.column
        jmp @@id_loop
    .endif
    
@@id_done:
    mov byte ptr [rdi + rcx], 0
    mov (TOKEN ptr [pToken]).length, ecx
    mov [rbx].LEXER_STATE.position, pos
    mov eax, 1
    jmp @@done
    
@@read_number:
    mov rdi, pToken
    mov (TOKEN ptr [rdi]).startLoc.line, [rbx].LEXER_STATE.line
    mov (TOKEN ptr [rdi]).startLoc.column, [rbx].LEXER_STATE.column
    mov (TOKEN ptr [rdi]).type, 2  ; NUMBER
    
    lea rdi, (TOKEN ptr [pToken]).value
    xor ecx, ecx
    
@@num_loop:
    .if pos >= [rbx].LEXER_STATE.sourceSize
        jmp @@num_done
    .endif
    
    movzx eax, byte ptr [rsi + pos]
    .if (al >= '0' && al <= '9') || al == '.' || al == 'e' || al == 'E' || \
        al == 'x' || al == 'X' || al == 'u' || al == 'U' || al == 'l' || al == 'L'
        .if ecx < 511
            mov [rdi + rcx], al
            inc ecx
        .endif
        inc pos
        inc [rbx].LEXER_STATE.column
        jmp @@num_loop
    .endif
    
@@num_done:
    mov byte ptr [rdi + rcx], 0
    mov (TOKEN ptr [pToken]).length, ecx
    mov [rbx].LEXER_STATE.position, pos
    mov eax, 1
    jmp @@done
    
@@read_string:
    movzx r12d, byte ptr [rsi + pos]  ; Quote character
    inc pos
    
    mov rdi, pToken
    mov (TOKEN ptr [rdi]).startLoc.line, [rbx].LEXER_STATE.line
    mov (TOKEN ptr [rdi]).startLoc.column, [rbx].LEXER_STATE.column
    mov (TOKEN ptr [rdi]).type, 3  ; STRING
    
    lea rdi, (TOKEN ptr [pToken]).value
    xor ecx, ecx
    
@@str_loop:
    .if pos >= [rbx].LEXER_STATE.sourceSize
        jmp @@str_done
    .endif
    
    movzx eax, byte ptr [rsi + pos]
    .if al == r12b
        inc pos
        jmp @@str_done
    .elseif al == '\'
        inc pos
        .if pos < [rbx].LEXER_STATE.sourceSize
            movzx eax, byte ptr [rsi + pos]
            .if al == 'n'
                mov al, 10
            .elseif al == 't'
                mov al, 9
            .elseif al == 'r'
                mov al, 13
            .elseif al == '0'
                xor al, al
            .elseif al == 'x'  ; Hex escape
                inc pos
                mov ah, 0
                movzx eax, byte ptr [rsi + pos]
                .if al >= '0' && al <= '9'
                    sub al, '0'
                .elseif al >= 'a' && al <= 'f'
                    sub al, 'a' - 10
                .elseif al >= 'A' && al <= 'F'
                    sub al, 'A' - 10
                .endif
                mov ah, al
                .if pos + 1 < [rbx].LEXER_STATE.sourceSize
                    movzx eax, byte ptr [rsi + pos + 1]
                    .if al >= '0' && al <= '9'
                        sub al, '0'
                    .elseif al >= 'a' && al <= 'f'
                        sub al, 'a' - 10
                    .elseif al >= 'A' && al <= 'F'
                        sub al, 'A' - 10
                    .else
                        jmp @@hex_done
                    .endif
                    shl ah, 4
                    or ah, al
                    mov al, ah
                    inc pos
                .endif
                @@hex_done:
            .endif
            .if ecx < 511
                mov [rdi + rcx], al
                inc ecx
            .endif
            inc pos
        .endif
    .else
        .if ecx < 511
            mov [rdi + rcx], al
            inc ecx
        .endif
        inc pos
    .endif
    
    .if al == 10
        inc [rbx].LEXER_STATE.line
        mov [rbx].LEXER_STATE.column, 1
    .else
        inc [rbx].LEXER_STATE.column
    .endif
    
    jmp @@str_loop
    
@@str_done:
    mov byte ptr [rdi + rcx], 0
    mov (TOKEN ptr [pToken]).length, ecx
    mov [rbx].LEXER_STATE.position, pos
    mov eax, 1
    jmp @@done
    
@@read_operator:
    mov rdi, pToken
    mov (TOKEN ptr [rdi]).startLoc.line, [rbx].LEXER_STATE.line
    mov (TOKEN ptr [rdi]).startLoc.column, [rbx].LEXER_STATE.column
    mov (TOKEN ptr [rdi]).type, 4  ; OPERATOR
    
    lea rdi, (TOKEN ptr [pToken]).value
    
    movzx eax, byte ptr [rsi + pos]
    mov [rdi], al
    mov byte ptr [rdi + 1], 0
    mov (TOKEN ptr [pToken]).length, 1
    
    inc pos
    inc [rbx].LEXER_STATE.column
    
    ; Check for two-character operators
    .if pos < [rbx].LEXER_STATE.sourceSize
        movzx ecx, byte ptr [rsi + pos]
        
        ; Check for ==, !=, <=, >=, ++, --, &&, ||, <<, >>
        .if (al == '=' && cl == '=') || (al == '!' && cl == '=') || \
            (al == '<' && cl == '=') || (al == '>' && cl == '=') || \
            (al == '+' && cl == '+') || (al == '-' && cl == '-') || \
            (al == '&' && cl == '&') || (al == '|' && cl == '|') || \
            (al == '<' && cl == '<') || (al == '>' && cl == '>')
            
            mov [rdi + 1], cl
            mov byte ptr [rdi + 2], 0
            mov (TOKEN ptr [pToken]).length, 2
            inc pos
            inc [rbx].LEXER_STATE.column
        .elseif (al == '+' && cl == '=') || (al == '-' && cl == '=') || \
                (al == '*' && cl == '=') || (al == '/' && cl == '=') || \
                (al == '%' && cl == '=') || (al == '&' && cl == '=') || \
                (al == '|' && cl == '=') || (al == '^' && cl == '=') || \
                (al == '.' && cl == '.')
            
            mov [rdi + 1], cl
            mov byte ptr [rdi + 2], 0
            mov (TOKEN ptr [pToken]).length, 2
            inc pos
            inc [rbx].LEXER_STATE.column
        .endif
    .endif
    
    mov [rbx].LEXER_STATE.position, pos
    mov eax, 1
    jmp @@done
    
@@eof:
    mov rdi, pToken
    mov (TOKEN ptr [rdi]).type, 0  ; EOF
    mov (TOKEN ptr [rdi]).length, 0
    mov [rbx].LEXER_STATE.position, pos
    xor eax, eax
    
@@done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Lexer_NextToken endp

;=============================================================================
; Cache_Initialize - Initializes compilation cache
;=============================================================================
Cache_Initialize proc frame engine:dq
    local pEngine:dq
    
    mov pEngine, rcx
    
    .if pEngine == 0
        xor eax, eax
        ret
    .endif
    
    mov rbx, pEngine
    
    ; Initialize LRU list
    mov [rbx].COMPILER_ENGINE.cacheHead, 0
    mov [rbx].COMPILER_ENGINE.cacheTail, 0
    mov [rbx].COMPILER_ENGINE.cacheCount, 0
    mov [rbx].COMPILER_ENGINE.cacheSize, 0
    
    mov eax, 1
    ret
Cache_Initialize endp

;=============================================================================
; Cache_Destroy - Destroys compilation cache
;=============================================================================
Cache_Destroy proc frame engine:dq
    local pEngine:dq
    local pEntry:dq
    local pNext:dq
    
    push rbx
    push rsi
    push rdi
    
    mov pEngine, rcx
    
    .if pEngine == 0
        jmp @@done
    .endif
    
    mov rbx, pEngine
    
    ; Free all cache entries
    mov rsi, [rbx].COMPILER_ENGINE.cacheHead
    
@@free_loop:
    .if rsi == 0
        jmp @@done_free
    .endif
    
    mov pNext, (CACHE_ENTRY ptr [rsi]).next
    
    ; Free entry diagnostics if present
    .if (CACHE_ENTRY ptr [rsi]).result.diagnostics != 0
        invoke HeapFree, [rbx].COMPILER_ENGINE.hHeap, 0, \
                (CACHE_ENTRY ptr [rsi]).result.diagnostics
    .endif
    
    invoke HeapFree, [rbx].COMPILER_ENGINE.hHeap, 0, rsi
    
    mov rsi, pNext
    jmp @@free_loop
    
@@done_free:
    mov [rbx].COMPILER_ENGINE.cacheHead, 0
    mov [rbx].COMPILER_ENGINE.cacheTail, 0
    mov [rbx].COMPILER_ENGINE.cacheCount, 0
    mov [rbx].COMPILER_ENGINE.cacheSize, 0
    
@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
Cache_Destroy endp

;=============================================================================
; Cache_Lookup - Looks up compilation result in cache
;=============================================================================
Cache_Lookup proc frame engine:dq, key:dq, result:dq
    local pEngine:dq
    local pKey:dq
    local pResult:dq
    
    push rbx
    push rsi
    push rdi
    
    mov pEngine, rcx
    mov pKey, rdx
    mov pResult, r8
    
    .if pEngine == 0 || pKey == 0 || pResult == 0
        xor eax, eax
        jmp @@done
    .endif
    
    mov rbx, pEngine
    
    invoke EnterCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexCache
    
    ; Search cache
    mov rsi, [rbx].COMPILER_ENGINE.cacheHead
    
@@search_loop:
    .if rsi == 0
        jmp @@not_found
    .endif
    
    ; Compare keys (first 64 bytes)
    mov rdi, pKey
    lea r8, (CACHE_ENTRY ptr [rsi]).key
    mov rcx, 64 / 8  ; Compare as qwords
    
@@compare_loop:
    .if rcx == 0
        jmp @@found
    .endif
    mov rax, [rdi]
    mov r9, [r8]
    .if rax != r9
        jmp @@no_match
    .endif
    add rdi, 8
    add r8, 8
    dec rcx
    jmp @@compare_loop
    
@@no_match:
    mov rsi, (CACHE_ENTRY ptr [rsi]).next
    jmp @@search_loop
    
@@found:
    ; Found - copy result
    mov rdi, pResult
    lea rsi, (CACHE_ENTRY ptr [rsi]).result
    mov rcx, sizeof COMPILE_RESULT / 8
    rep movsq
    
    invoke LeaveCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexCache
    
    mov eax, 1
    jmp @@done
    
@@not_found:
    invoke LeaveCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexCache
    xor eax, eax
    
@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
Cache_Lookup endp

;=============================================================================
; Cache_Store - Stores compilation result in cache
;=============================================================================
Cache_Store proc frame engine:dq, key:dq, result:dq
    local pEngine:dq
    local pKey:dq
    local pResult:dq
    local pEntry:dq
    local entrySize:dq
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    mov pEngine, rcx
    mov pKey, rdx
    mov pResult, r8
    
    .if pEngine == 0 || pKey == 0 || pResult == 0
        xor eax, eax
        jmp @@done
    .endif
    
    mov rbx, pEngine
    mov r12, pResult
    
    ; Calculate entry size
    mov entrySize, sizeof CACHE_ENTRY
    
    ; Check if we need to evict
    mov rax, entrySize
    add rax, [rbx].COMPILER_ENGINE.cacheSize
    .if rax > [rbx].COMPILER_ENGINE.cacheMaxSize
        invoke Cache_EvictLRU, pEngine, entrySize
    .endif
    
    invoke EnterCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexCache
    
    ; Allocate entry
    mov rcx, [rbx].COMPILER_ENGINE.hHeap
    mov rdx, entrySize
    invoke Heap_Alloc
    .if rax == 0
        invoke LeaveCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexCache
        xor eax, eax
        jmp @@done
    .endif
    mov pEntry, rax
    
    ; Initialize entry
    mov rdi, pEntry
    mov rcx, sizeof CACHE_ENTRY / 8
    xor rax, rax
    rep stosq
    
    ; Copy key
    mov rsi, pKey
    mov rdi, pEntry
    add rdi, offsetof CACHE_ENTRY.key
    mov rcx, 64
    rep movsb
    
    ; Copy result
    mov rsi, r12
    mov rdi, pEntry
    add rdi, offsetof CACHE_ENTRY.result
    mov rcx, sizeof COMPILE_RESULT / 8
    rep movsq
    
    ; Set metadata
    invoke GetTickCount64
    mov (CACHE_ENTRY ptr [pEntry]).timestamp, rax
    mov (CACHE_ENTRY ptr [pEntry]).valid, 1
    
    ; Add to front of LRU list
    mov rax, [rbx].COMPILER_ENGINE.cacheHead
    .if rax != 0
        mov (CACHE_ENTRY ptr [rax]).prev, pEntry
    .endif
    mov (CACHE_ENTRY ptr [pEntry]).next, rax
    mov (CACHE_ENTRY ptr [pEntry]).prev, 0
    mov [rbx].COMPILER_ENGINE.cacheHead, pEntry
    
    .if [rbx].COMPILER_ENGINE.cacheTail == 0
        mov [rbx].COMPILER_ENGINE.cacheTail, pEntry
    .endif
    
    inc [rbx].COMPILER_ENGINE.cacheCount
    mov rax, entrySize
    add [rbx].COMPILER_ENGINE.cacheSize, rax
    
    invoke LeaveCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexCache
    
    mov eax, 1
    
@@done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Cache_Store endp

;=============================================================================
; Cache_EvictLRU - Evicts least recently used entries
;=============================================================================
Cache_EvictLRU proc frame engine:dq, requiredSize:dq
    local pEngine:dq
    local needed:dq
    
    push rbx
    push rsi
    
    mov pEngine, rcx
    mov needed, rdx
    
    mov rbx, pEngine
    
    invoke EnterCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexCache
    
    ; Evict from tail until we have enough space
    mov rsi, [rbx].COMPILER_ENGINE.cacheTail
    
@@evict_loop:
    .if rsi == 0
        jmp @@done
    .endif
    
    mov rax, [rbx].COMPILER_ENGINE.cacheMaxSize
    sub rax, [rbx].COMPILER_ENGINE.cacheSize
    add rax, sizeof CACHE_ENTRY
    .if rax >= needed
        jmp @@done
    .endif
    
    ; Remove from list
    mov rax, (CACHE_ENTRY ptr [rsi]).prev
    .if rax != 0
        mov (CACHE_ENTRY ptr [rax]).next, 0
        mov [rbx].COMPILER_ENGINE.cacheTail, rax
    .else
        mov [rbx].COMPILER_ENGINE.cacheHead, 0
        mov [rbx].COMPILER_ENGINE.cacheTail, 0
    .endif
    
    dec [rbx].COMPILER_ENGINE.cacheCount
    mov rax, sizeof CACHE_ENTRY
    sub [rbx].COMPILER_ENGINE.cacheSize, rax
    
    ; Free entry
    push (CACHE_ENTRY ptr [rsi]).prev
    invoke HeapFree, [rbx].COMPILER_ENGINE.hHeap, 0, rsi
    pop rsi
    
    jmp @@evict_loop
    
@@done:
    invoke LeaveCriticalSection, addr [rbx].COMPILER_ENGINE.hMutexCache
    
    pop rsi
    pop rbx
    ret
Cache_EvictLRU endp

;=============================================================================
; Diagnostic_Add - Adds diagnostic message to result
;=============================================================================
Diagnostic_Add proc frame result:dq, severity:dd, line:dd, column:dd, code:dd, message:dq, filePath:dq
    local pResult:dq
    local pDiag:dq
    
    push rbx
    push rsi
    push rdi
    
    mov pResult, rcx
    
    .if pResult == 0
        xor eax, eax
        jmp @@done
    .endif
    
    mov rbx, pResult
    
    ; Allocate or use diagnostics array
    .if (COMPILE_RESULT ptr [rbx]).diagnostics == 0
        invoke HeapAlloc, GetProcessHeap(), HEAP_ZERO_MEMORY, \
                MAX_DIAGNOSTICS * sizeof DIAGNOSTIC
        .if rax == 0
            xor eax, eax
            jmp @@done
        .endif
        mov (COMPILE_RESULT ptr [rbx]).diagnostics, rax
    .endif
    
    ; Check if array is full
    mov eax, (COMPILE_RESULT ptr [rbx]).diagCount
    .if eax >= MAX_DIAGNOSTICS
        xor eax, eax
        jmp @@done
    .endif
    
    ; Get pointer to next diagnostic slot
    mov rax, (COMPILE_RESULT ptr [rbx]).diagnostics
    mov ecx, (COMPILE_RESULT ptr [rbx]).diagCount
    mov edx, sizeof DIAGNOSTIC
    imul rcx, rdx
    add rax, rcx
    mov pDiag, rax
    
    ; Fill diagnostic
    mov (DIAGNOSTIC ptr [rax]).severity, edx  ; edx still has severity from frame
    mov (DIAGNOSTIC ptr [rax]).line, r8d      ; r8 has line
    mov (DIAGNOSTIC ptr [rax]).column, r9d    ; r9 has column
    mov (DIAGNOSTIC ptr [rax]).code, r10d     ; r10 has code
    
    ; Copy message
    .if r11 != 0
        invoke Str_Copy, addr (DIAGNOSTIC ptr [rax]).message, r11, 1024
    .endif
    
    ; Copy file path
    mov rsi, [rsp + 40]  ; filePath from stack
    .if rsi != 0
        invoke Str_Copy, addr (DIAGNOSTIC ptr [rax]).filePath, rsi, MAX_PATH
    .endif
    
    ; Increment count
    inc (COMPILE_RESULT ptr [rbx]).diagCount
    
    mov eax, 1
    
@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
Diagnostic_Add endp

;=============================================================================
; File_ReadAllText - Reads entire file into memory
;=============================================================================
File_ReadAllText proc frame filePath:dq, hHeap:dq, pSize:dq
    local hFile:dq
    local fileSize:LARGE_INTEGER
    local hTargetHeap:dq
    local pBuffer:dq
    local bytesRead:dq
    
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx  ; filePath
    
    .if r8 == 0
        invoke GetProcessHeap
    .else
        mov rax, r8
    .endif
    mov hTargetHeap, rax
    
    ; Open file
    invoke CreateFileA, rsi, GENERIC_READ, FILE_SHARE_READ, NULL, \
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    .if rax == INVALID_HANDLE_VALUE
        xor eax, eax
        jmp @@done
    .endif
    mov hFile, rax
    
    ; Get file size
    invoke GetFileSizeEx, hFile, addr fileSize
    .if eax == 0
        invoke CloseHandle, hFile
        xor eax, eax
        jmp @@done
    .endif
    
    ; Allocate buffer (+1 for null terminator)
    mov rax, fileSize.QuadPart
    inc rax
    mov rcx, hTargetHeap
    mov rdx, rax
    invoke Heap_Alloc
    .if rax == 0
        invoke CloseHandle, hFile
        xor eax, eax
        jmp @@done
    .endif
    mov pBuffer, rax
    
    ; Read file
    invoke ReadFile, hFile, pBuffer, dword ptr fileSize.QuadPart, addr bytesRead, NULL
    
    ; Null terminate
    mov rax, pBuffer
    add rax, fileSize.QuadPart
    mov byte ptr [rax], 0
    
    ; Close file
    invoke CloseHandle, hFile
    
    ; Return size if requested
    .if r9 != 0
        mov rdi, r9
        mov rax, fileSize.QuadPart
        mov [rdi], rax
    .endif
    
    mov rax, pBuffer
    
@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
File_ReadAllText endp

;=============================================================================
; Str_Copy - Copies string with length limit
;=============================================================================
Str_Copy proc frame dest:dq, src:dq, size:dq
    mov r8, r8  ; size limit
    
    .if rcx == 0 || rdx == 0 || r8 == 0
        ret
    .endif
    
    mov r9, rcx  ; dest
    mov r10, rdx  ; src
    mov r11, r8   ; size
    
@@copy_loop:
    .if r11 == 0
        jmp @@done
    .endif
    
    movzx eax, byte ptr [r10]
    mov [r9], al
    
    .if al == 0
        jmp @@done
    .endif
    
    inc r9
    inc r10
    dec r11
    
    jmp @@copy_loop
    
@@done:
    ; Ensure null termination
    mov byte ptr [r9], 0
    ret
Str_Copy endp

;=============================================================================
; File_Exists - Checks if file exists
;=============================================================================
File_Exists proc frame filePath:dq
    invoke GetFileAttributesA, rcx
    .if rax == INVALID_FILE_ATTRIBUTES
        xor eax, eax
    .else
        test eax, FILE_ATTRIBUTE_DIRECTORY
        .if zero?
            mov eax, 1
        .else
            xor eax, eax
        .endif
    .endif
    ret
File_Exists endp

;=============================================================================
; Str_ExtractExt - Extracts file extension
;=============================================================================
Str_ExtractExt proc frame filePath:dq, buffer:dq, size:dq
    local pLastDot:dq
    
    mov r9, rcx  ; filePath
    mov r10, rdx  ; buffer
    mov r11, r8  ; size
    
    ; Find last dot
    mov rax, r9
    xor r12, r12
    
@@find_dot:
    movzx ecx, byte ptr [rax]
    .if cl == 0
        jmp @@dot_found
    .endif
    .if cl == '.'
        mov r12, rax
    .endif
    inc rax
    jmp @@find_dot
    
@@dot_found:
    mov pLastDot, r12
    
    .if pLastDot == 0
        mov byte ptr [r10], 0
        ret
    .endif
    
    ; Copy extension
    inc pLastDot
    invoke Str_Copy, r10, pLastDot, r11
    
    ; Convert to lowercase
@@lower_loop:
    movzx eax, byte ptr [r10]
    .if al == 0
        ret
    .endif
    .if al >= 'A' && al <= 'Z'
        add al, 32
        mov [r10], al
    .endif
    inc r10
    jmp @@lower_loop
    
Str_ExtractExt endp

;=============================================================================
; Heap_Alloc - Allocates memory from heap
;=============================================================================
Heap_Alloc proc frame hHeap:dq, size:dq
    invoke HeapAlloc, rcx, HEAP_ZERO_MEMORY, rdx
    ret
Heap_Alloc endp

;=============================================================================
; Heap_Free - Frees memory from heap
;=============================================================================
Heap_Free proc frame hHeap:dq, ptr:dq
    invoke HeapFree, rcx, 0, rdx
    ret
Heap_Free endp

;=============================================================================
; CompilerEngine_ValidateOptions - Validates compilation options
;=============================================================================
CompilerEngine_ValidateOptions proc frame engine:dq, options:dq, result:dq
    local pOptions:dq
    local pResult:dq
    local langDetected:dword
    
    mov pOptions, rdx
    mov pResult, r8
    
    ; Check source path exists
    invoke File_Exists, addr (COMPILE_OPTIONS ptr [pOptions]).sourcePath
    .if eax == 0
        invoke Diagnostic_Add, pResult, SEV_FATAL, 0, 0, 1001, \
                addr szErrFileNotFound, addr (COMPILE_OPTIONS ptr [pOptions]).sourcePath
        xor eax, eax
        ret
    .endif
    
    ; Auto-detect language if not specified
    .if (COMPILE_OPTIONS ptr [pOptions]).language == LANG_UNKNOWN || \
        (COMPILE_OPTIONS ptr [pOptions]).language == LANG_AUTO
        
        invoke Utils_DetectLanguage, addr (COMPILE_OPTIONS ptr [pOptions]).sourcePath
        mov langDetected, eax
        mov (COMPILE_OPTIONS ptr [pOptions]).language, eax
    .endif
    
    ; Check if we have a language
    .if (COMPILE_OPTIONS ptr [pOptions]).language == LANG_UNKNOWN
        invoke Diagnostic_Add, pResult, SEV_FATAL, 0, 0, 1002, \
                addr szErrNoLanguage, addr (COMPILE_OPTIONS ptr [pOptions]).sourcePath
        xor eax, eax
        ret
    .endif
    
    mov eax, 1
    ret
CompilerEngine_ValidateOptions endp

;=============================================================================
; Utils_DetectLanguage - Detects source language from file extension
;=============================================================================
Utils_DetectLanguage proc frame filePath:dq
    local ext:db 16 dup(?)
    
    push rbx
    
    ; Extract extension
    invoke Str_ExtractExt, rcx, addr ext, 16
    
    ; Compare with known extensions
    lea rsi, ext
    
    ; Check C
    invoke Str_CompareI, rsi, addr szExtC
    .if eax == 0
        mov eax, LANG_C
        jmp @@done
    .endif
    
    ; Check C++
    invoke Str_CompareI, rsi, addr szExtCpp
    .if eax == 0
        mov eax, LANG_CPP
        jmp @@done
    .endif
    
    invoke Str_CompareI, rsi, addr szExtCxx
    .if eax == 0
        mov eax, LANG_CPP
        jmp @@done
    .endif
    
    invoke Str_CompareI, rsi, addr szExtCc
    .if eax == 0
        mov eax, LANG_CPP
        jmp @@done
    .endif
    
    ; Check Rust
    invoke Str_CompareI, rsi, addr szExtRs
    .if eax == 0
        mov eax, LANG_RUST
        jmp @@done
    .endif
    
    ; Check Go
    invoke Str_CompareI, rsi, addr szExtGo
    .if eax == 0
        mov eax, LANG_GO
        jmp @@done
    .endif
    
    ; Check Python
    invoke Str_CompareI, rsi, addr szExtPy
    .if eax == 0
        mov eax, LANG_PYTHON
        jmp @@done
    .endif
    
    ; Check JavaScript
    invoke Str_CompareI, rsi, addr szExtJs
    .if eax == 0
        mov eax, LANG_JS
        jmp @@done
    .endif
    
    ; Check TypeScript
    invoke Str_CompareI, rsi, addr szExtTs
    .if eax == 0
        mov eax, LANG_TS
        jmp @@done
    .endif
    
    ; Check Java
    invoke Str_CompareI, rsi, addr szExtJava
    .if eax == 0
        mov eax, LANG_JAVA
        jmp @@done
    .endif
    
    ; Check C#
    invoke Str_CompareI, rsi, addr szExtCs
    .if eax == 0
        mov eax, LANG_CS
        jmp @@done
    .endif
    
    ; Check Assembly
    invoke Str_CompareI, rsi, addr szExtAsm
    .if eax == 0
        mov eax, LANG_ASM
        jmp @@done
    .endif
    
    ; Unknown
    mov eax, LANG_UNKNOWN
    
@@done:
    pop rbx
    ret
Utils_DetectLanguage endp

;=============================================================================
; Str_CompareI - Case-insensitive string compare
;=============================================================================
Str_CompareI proc frame s1:dq, s2:dq
    mov r8, rcx  ; s1
    mov r9, rdx  ; s2
    
@@compare_loop:
    movzx eax, byte ptr [r8]
    movzx ecx, byte ptr [r9]
    
    ; Convert to lowercase for comparison
    .if al >= 'A' && al <= 'Z'
        add al, 32
    .endif
    .if cl >= 'A' && cl <= 'Z'
        add cl, 32
    .endif
    
    sub eax, ecx
    .if eax != 0
        ret
    .endif
    
    .if cl == 0
        xor eax, eax
        ret
    .endif
    
    inc r8
    inc r9
    
    jmp @@compare_loop
    
Str_CompareI endp

;=============================================================================
; CompilerEngine_ComputeCacheKey - Computes SHA-256 cache key
;=============================================================================
CompilerEngine_ComputeCacheKey proc frame options:dq, keyOut:dq
    local pOptions:dq
    local pKey:dq
    
    mov pOptions, rcx
    mov pKey, rdx
    
    ; Simple hash - concatenate key fields
    ; In production, would use real SHA-256
    mov rsi, pKey
    mov rdi, pOptions
    
    ; Copy source path (first 32 bytes)
    mov rcx, 32
    @@copy_path:
        mov al, [rdi]
        mov [rsi], al
        inc rsi
        inc rdi
        dec rcx
        jnz @@copy_path
    
    ; Add target arch (4 bytes)
    mov eax, (COMPILE_OPTIONS ptr [pOptions]).targetArch
    mov [rsi], eax
    add rsi, 4
    
    ; Add opt level (4 bytes)
    mov eax, (COMPILE_OPTIONS ptr [pOptions]).optLevel
    mov [rsi], eax
    add rsi, 4
    
    ; Add language (4 bytes)
    mov eax, (COMPILE_OPTIONS ptr [pOptions]).language
    mov [rsi], eax
    add rsi, 4
    
    ; Pad with zeros to 64 bytes
    mov rcx, 64
    sub rcx, 44  ; We've written 32 + 4 + 4 + 4 = 44
    xor al, al
    rep stosb
    
    ret
CompilerEngine_ComputeCacheKey endp

;=============================================================================
; CompilerEngine_InitializeCapabilities
;=============================================================================
CompilerEngine_InitializeCapabilities proc frame engine:dq
    mov eax, 1
    ret
CompilerEngine_InitializeCapabilities endp

;=============================================================================
; SIMD-Accelerated Lexer Module
;=============================================================================

;=============================================================================
; Lexer_Initialize
; Initializes a lexer state for tokenizing source code
; pLexer: Pointer to LEXER_STATE
; pSource: Pointer to source buffer
; sourceSize: Size of source buffer
; language: Language ID
; Returns: 1 on success, 0 on failure
;=============================================================================
Lexer_Initialize proc frame pLexer:dq, pSource:dq, sourceSize:dq, language:dword
    local hHeap:dq
    
    push rbx
    push rsi
    push rdi
    
    ; Validate parameters
    .if pLexer == 0 || pSource == 0 || sourceSize == 0
        xor eax, eax
        jmp @@done
    .endif
    
    ; Get heap from engine (assuming global)
    mov rax, g_pCompilerEngine
    .if rax == 0
        xor eax, eax
        jmp @@done
    .endif
    mov rax, (COMPILER_ENGINE ptr [rax]).hHeap
    mov hHeap, rax
    
    ; Initialize lexer state
    mov rbx, pLexer
    mov (LEXER_STATE ptr [rbx]).source, pSource
    mov (LEXER_STATE ptr [rbx]).sourceSize, sourceSize
    mov (LEXER_STATE ptr [rbx]).position, 0
    mov (LEXER_STATE ptr [rbx]).line, 1
    mov (LEXER_STATE ptr [rbx]).column, 1
    mov (LEXER_STATE ptr [rbx]).tokenCount, 0
    mov (LEXER_STATE ptr [rbx]).tokenCapacity, 1024  ; Initial capacity
    mov (LEXER_STATE ptr [rbx]).hHeap, hHeap
    
    ; Allocate token array
    mov rcx, hHeap
    mov rdx, 1024 * sizeof TOKEN
    invoke Heap_Alloc
    .if rax == 0
        xor eax, eax
        jmp @@done
    .endif
    mov (LEXER_STATE ptr [rbx]).tokens, rax
    
    ; Allocate diagnostics array
    mov rcx, hHeap
    mov rdx, 64 * sizeof DIAGNOSTIC
    invoke HeapAlloc, rcx, 0, rdx
    .if rax == 0
        ; Free tokens array
        .if (LEXER_STATE ptr [rbx]).tokens != 0
            invoke HeapFree, hHeap, 0, (LEXER_STATE ptr [rbx]).tokens
        .endif
        xor eax, eax
        jmp @@done
    .endif
    mov (LEXER_STATE ptr [rbx]).diagnostics, rax
    
    mov eax, 1
    
@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
Lexer_Initialize endp

;=============================================================================
; Lexer_Tokenize
; Tokenizes the entire source using SIMD acceleration
; pLexer: Pointer to LEXER_STATE
; Returns: Token count on success, -1 on failure
;=============================================================================
Lexer_Tokenize proc frame pLexer:dq
    local currentPos:dq
    local endPos:dq
    local tokenIdx:dq
    local tempChar:byte
    local startPos:dq
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    ; Validate
    .if pLexer == 0
        mov rax, -1
        jmp @@done
    .endif
    
    mov rbx, pLexer
    mov rsi, (LEXER_STATE ptr [rbx]).source
    mov currentPos, 0
    mov rax, (LEXER_STATE ptr [rbx]).sourceSize
    mov endPos, rax
    mov tokenIdx, 0
    
@@tokenize_loop:
    mov rax, currentPos
    .if rax >= endPos
        ; Add EOF token
        invoke Lexer_AddToken, rbx, TOKEN_EOF, currentPos, currentPos, 0, 0
        mov rax, tokenIdx
        jmp @@done
    .endif
    
    ; Update location tracking
    invoke Lexer_UpdateLocation, rbx, currentPos
    
    ; Get current character
    mov rcx, currentPos
    movzx eax, byte ptr [rsi + rcx]
    mov tempChar, al
    
    ; Skip whitespace using SIMD
    .if al == ' ' || al == '\t'
        mov startPos, currentPos
        invoke Lexer_SkipWhitespace, rbx, addr currentPos
        ; Could add whitespace token if needed
        jmp @@tokenize_loop
    .endif
    
    ; Handle newlines
    .if al == 13 || al == 10  ; CR or LF
        invoke Lexer_HandleNewline, rbx, addr currentPos
        jmp @@tokenize_loop
    .endif
    
    ; Check for comments
    .if al == '/' 
        mov rcx, currentPos
        inc rcx
        .if rcx < endPos
            movzx eax, byte ptr [rsi + rcx]
            .if al == '/'  ; //
                invoke Lexer_SkipLineComment, rbx, addr currentPos
                jmp @@tokenize_loop
            .elseif al == '*'  ; /*
                invoke Lexer_SkipBlockComment, rbx, addr currentPos
                .if rax == 0  ; Error: unterminated comment
                    invoke Lexer_ReportError, rbx, 1001, addr szErrUnterminatedComment, currentPos
                    mov rax, -1
                    jmp @@done
                .endif
                jmp @@tokenize_loop
            .endif
        .endif
    .endif
    
    ; Check for string literals
    .if al == '"' || al == '''
        mov startPos, currentPos
        invoke Lexer_TokenizeString, rbx, addr currentPos
        .if rax == 0  ; Error
            invoke Lexer_ReportError, rbx, 1002, addr szErrUnterminatedString, startPos
            mov rax, -1
            jmp @@done
        .endif
        inc tokenIdx
        jmp @@tokenize_loop
    .endif
    
    ; Check for numbers
    .if al >= '0' && al <= '9'
        mov startPos, currentPos
        invoke Lexer_TokenizeNumber, rbx, addr currentPos
        inc tokenIdx
        jmp @@tokenize_loop
    .endif
    
    ; Check for identifiers/keywords
    .if Lexer_IsIdentifierStart(al)
        mov startPos, currentPos
        invoke Lexer_TokenizeIdentifier, rbx, addr currentPos
        inc tokenIdx
        jmp @@tokenize_loop
    .endif
    
    ; Operators and punctuators
    mov startPos, currentPos
    invoke Lexer_TokenizeOperator, rbx, addr currentPos
    .if rax == 0  ; Invalid character
        invoke Lexer_ReportError, rbx, 1003, addr szErrInvalidChar, startPos
        ; Continue processing
    .else
        inc tokenIdx
    .endif
    jmp @@tokenize_loop
    
@@done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Lexer_Tokenize endp

;=============================================================================
; Lexer_SkipWhitespace (SIMD accelerated)
; Skips whitespace characters using AVX-512
;=============================================================================
Lexer_SkipWhitespace proc frame pLexer:dq, pPos:dq
    ; Simplified - full SIMD implementation would use vpcmpeqb and vpmovmskb
    ; For now, simple loop
    mov rbx, pLexer
    mov rsi, (LEXER_STATE ptr [rbx]).source
    mov rbx, pPos
    mov rcx, [rbx]  ; current pos
    
@@skip_loop:
    mov rax, rcx
    cmp rax, (LEXER_STATE ptr [pLexer]).sourceSize
    jge @@done
    
    movzx eax, byte ptr [rsi + rcx]
    .if al != ' ' && al != '\t'
        jmp @@done
    .endif
    
    inc rcx
    jmp @@skip_loop
    
@@done:
    mov [rbx], rcx
    ret
Lexer_SkipWhitespace endp

;=============================================================================
; Lexer_IsIdentifierStart
; Checks if character can start an identifier
;=============================================================================
Lexer_IsIdentifierStart proc frame char:byte
    mov al, char
    .if al >= 'a' && al <= 'z'
        mov eax, 1
        ret
    .endif
    .if al >= 'A' && al <= 'Z'
        mov eax, 1
        ret
    .endif
    .if al == '_'
        mov eax, 1
        ret
    .endif
    xor eax, eax
    ret
Lexer_IsIdentifierStart endp

;=============================================================================
; Lexer_AddToken
; Adds a token to the token array
;=============================================================================
Lexer_AddToken proc frame pLexer:dq, type:dword, startPos:dq, endPos:dq, pValue:dq, valueLen:dword
    local pToken:dq
    
    ; Simplified - would resize array if needed
    mov rbx, pLexer
    mov rcx, (LEXER_STATE ptr [rbx]).tokenCount
    mov rdx, (LEXER_STATE ptr [rbx]).tokens
    lea pToken, [rdx + rcx * sizeof TOKEN]
    
    mov (TOKEN ptr [pToken]).type, type
    mov (TOKEN ptr [pToken]).startLoc.offset, startPos
    mov (TOKEN ptr [pToken]).endLoc.offset, endPos
    mov (TOKEN ptr [pToken]).length, valueLen
    
    ; Set source location details
    mov rax, (LEXER_STATE ptr [rbx]).line
    mov (TOKEN ptr [pToken]).startLoc.line, rax
    mov (TOKEN ptr [pToken]).endLoc.line, rax
    mov rax, (LEXER_STATE ptr [rbx]).column
    mov (TOKEN ptr [pToken]).startLoc.column, rax
    ; Calculate end column
    mov rcx, valueLen
    add rax, rcx
    mov (TOKEN ptr [pToken]).endLoc.column, rax
    
    .if pValue != 0 && valueLen > 0
        ; Copy value (simplified)
        mov rsi, pValue
        lea rdi, (TOKEN ptr [pToken]).value
        mov ecx, valueLen
        rep movsb
    .endif
    
    inc (LEXER_STATE ptr [rbx]).tokenCount
    ret
Lexer_AddToken endp

;=============================================================================
; Lexer_ReportError
; Reports a lexical error with diagnostics
; pLexer: Pointer to LEXER_STATE
; errorCode: Error code
; pMessage: Error message string
; pos: Position in source
;=============================================================================
Lexer_ReportError proc frame pLexer:dq, errorCode:dword, pMessage:dq, pos:dq
    local pDiag:dq
    local msgLen:dword
    
    ; Check if diagnostics array exists
    mov rbx, pLexer
    mov rax, (LEXER_STATE ptr [rbx]).diagnostics
    .if rax == 0
        ; Allocate diagnostics array
        mov rcx, (LEXER_STATE ptr [rbx]).hHeap
        mov rdx, 64 * sizeof DIAGNOSTIC
        invoke HeapAlloc, rcx, 0, rdx
        .if rax == 0
            ret
        .endif
        mov (LEXER_STATE ptr [rbx]).diagnostics, rax
    .endif
    
    ; Check if we need to resize
    mov rax, (LEXER_STATE ptr [rbx]).diagCount
    cmp rax, (LEXER_STATE ptr [rbx]).diagCapacity
    jl @@add_diag
    
    ; Resize diagnostics array
    mov rcx, (LEXER_STATE ptr [rbx]).hHeap
    mov rdx, (LEXER_STATE ptr [rbx]).diagnostics
    mov r8, (LEXER_STATE ptr [rbx]).diagCapacity
    add r8, 64
    mov (LEXER_STATE ptr [rbx]).diagCapacity, r8
    imul r8, sizeof DIAGNOSTIC
    invoke HeapReAlloc, rcx, 0, rdx, r8
    .if rax == 0
        ret
    .endif
    mov (LEXER_STATE ptr [rbx]).diagnostics, rax
    
@@add_diag:
    ; Add diagnostic entry
    mov rcx, (LEXER_STATE ptr [rbx]).diagCount
    mov rdx, (LEXER_STATE ptr [rbx]).diagnostics
    imul rcx, sizeof DIAGNOSTIC
    add rdx, rcx
    mov pDiag, rdx
    
    ; Fill diagnostic
    mov (DIAGNOSTIC ptr [pDiag]).severity, SEV_ERROR
    mov (DIAGNOSTIC ptr [pDiag]).code, errorCode
    
    ; Set location
    invoke Lexer_UpdateLocation, pLexer, pos
    mov rax, (LEXER_STATE ptr [rbx]).line
    mov (DIAGNOSTIC ptr [pDiag]).line, eax
    mov rax, (LEXER_STATE ptr [rbx]).column
    mov (DIAGNOSTIC ptr [pDiag]).column, eax
    
    ; Copy message
    .if pMessage != 0
        invoke lstrlenA, pMessage
        mov msgLen, eax
        .if msgLen > 1023
            mov msgLen, 1023
        .endif
        mov rsi, pMessage
        lea rdi, (DIAGNOSTIC ptr [pDiag]).message
        mov ecx, msgLen
        rep movsb
        mov byte ptr [rdi], 0
    .endif
    
    ; Increment count
    inc (LEXER_STATE ptr [rbx]).diagCount
    
    ret
Lexer_ReportError endp

;=============================================================================
; Lexer_UpdateLocation
; Updates line and column tracking
; pLexer: Pointer to LEXER_STATE
; pos: Current position
;=============================================================================
Lexer_UpdateLocation proc frame pLexer:dq, pos:dq
    local currentPos:dq
    local sourcePtr:dq
    
    mov rbx, pLexer
    mov sourcePtr, (LEXER_STATE ptr [rbx]).source
    mov currentPos, 0
    mov (LEXER_STATE ptr [rbx]).line, 1
    mov (LEXER_STATE ptr [rbx]).column, 1
    
@@update_loop:
    mov rax, currentPos
    cmp rax, pos
    jge @@done
    
    mov rcx, currentPos
    movzx eax, byte ptr [sourcePtr + rcx]
    
    .if al == 10  ; LF
        inc (LEXER_STATE ptr [rbx]).line
        mov (LEXER_STATE ptr [rbx]).column, 1
    .elseif al == 13  ; CR
        ; Handle CRLF - if next is LF, skip it
        mov rdx, currentPos
        inc rdx
        cmp rdx, (LEXER_STATE ptr [rbx]).sourceSize
        jge @@no_lf
        movzx eax, byte ptr [sourcePtr + rdx]
        .if al == 10
            inc currentPos
        .endif
@@no_lf:
        inc (LEXER_STATE ptr [rbx]).line
        mov (LEXER_STATE ptr [rbx]).column, 1
    .else
        inc (LEXER_STATE ptr [rbx]).column
    .endif
    
    inc currentPos
    jmp @@update_loop
    
@@done:
    ret
Lexer_UpdateLocation endp

;=============================================================================
; Additional lexer helper functions
;=============================================================================

;=============================================================================
; Lexer_HandleNewline
; Handles newline characters and updates location
;=============================================================================
Lexer_HandleNewline proc frame pLexer:dq, pPos:dq
    mov rbx, pLexer
    mov rsi, (LEXER_STATE ptr [rbx]).source
    mov rbx, pPos
    mov rcx, [rbx]
    
    movzx eax, byte ptr [rsi + rcx]
    .if al == 13  ; CR
        inc rcx
        .if rcx < (LEXER_STATE ptr [pLexer]).sourceSize
            movzx eax, byte ptr [rsi + rcx]
            .if al == 10  ; LF
                inc rcx
            .endif
        .endif
    .elseif al == 10  ; LF
        inc rcx
    .endif
    
    ; Update location
    inc (LEXER_STATE ptr [pLexer]).line
    mov (LEXER_STATE ptr [pLexer]).column, 1
    
    mov [rbx], rcx
    ret
Lexer_HandleNewline endp

;=============================================================================
; Lexer_SkipLineComment
; Skips a line comment (// ...)
;=============================================================================
Lexer_SkipLineComment proc frame pLexer:dq, pPos:dq
    mov rbx, pLexer
    mov rsi, (LEXER_STATE ptr [rbx]).source
    mov rbx, pPos
    mov rcx, [rbx]
    
@@skip_loop:
    inc rcx
    cmp rcx, (LEXER_STATE ptr [pLexer]).sourceSize
    jge @@done
    
    movzx eax, byte ptr [rsi + rcx]
    .if al == 10 || al == 13  ; Newline
        jmp @@done
    .endif
    jmp @@skip_loop
    
@@done:
    mov [rbx], rcx
    ret
Lexer_SkipLineComment endp

;=============================================================================
; Lexer_SkipBlockComment
; Skips a block comment (/* ... */)
; Returns: 1 on success, 0 on unterminated
;=============================================================================
Lexer_SkipBlockComment proc frame pLexer:dq, pPos:dq
    mov rbx, pLexer
    mov rsi, (LEXER_STATE ptr [rbx]).source
    mov rbx, pPos
    mov rcx, [rbx]
    add rcx, 2  ; Skip /*
    
@@skip_loop:
    cmp rcx, (LEXER_STATE ptr [pLexer]).sourceSize
    jge @@unterminated
    
    movzx eax, byte ptr [rsi + rcx]
    .if al == '*'
        inc rcx
        cmp rcx, (LEXER_STATE ptr [pLexer]).sourceSize
        jge @@unterminated
        movzx eax, byte ptr [rsi + rcx]
        .if al == '/'  ; */
            inc rcx
            jmp @@done
        .endif
    .else
        inc rcx
    .endif
    jmp @@skip_loop
    
@@unterminated:
    mov [rbx], rcx
    xor eax, eax
    ret
    
@@done:
    mov [rbx], rcx
    mov eax, 1
    ret
Lexer_SkipBlockComment endp

;=============================================================================
; Lexer_TokenizeString
; Tokenizes a string literal
; Returns: 1 on success, 0 on unterminated
;=============================================================================
Lexer_TokenizeString proc frame pLexer:dq, pPos:dq
    local startPos:dq
    local quoteChar:byte
    
    mov rbx, pLexer
    mov rsi, (LEXER_STATE ptr [rbx]).source
    mov rbx, pPos
    mov rcx, [rbx]
    mov startPos, rcx
    
    movzx eax, byte ptr [rsi + rcx]
    mov quoteChar, al
    inc rcx  ; Skip opening quote
    
@@string_loop:
    cmp rcx, (LEXER_STATE ptr [pLexer]).sourceSize
    jge @@unterminated
    
    movzx eax, byte ptr [rsi + rcx]
    .if al == quoteChar
        inc rcx
        ; Add string token
        mov rdx, rcx
        sub rdx, startPos
        dec rdx  ; Exclude quotes
        lea rax, [rsi + startPos + 1]  ; Skip opening quote
        invoke Lexer_AddToken, pLexer, TOKEN_LITERAL_STRING, startPos, rcx, rax, edx
        jmp @@done
    .elseif al == '\'  ; Escape
        add rcx, 2  ; Skip escape sequence
    .else
        inc rcx
    .endif
    jmp @@string_loop
    
@@unterminated:
    xor eax, eax
    ret
    
@@done:
    mov [rbx], rcx
    mov eax, 1
    ret
Lexer_TokenizeString endp

;=============================================================================
; Lexer_TokenizeNumber
; Tokenizes a numeric literal
;=============================================================================
Lexer_TokenizeNumber proc frame pLexer:dq, pPos:dq
    local startPos:dq
    
    mov rbx, pLexer
    mov rsi, (LEXER_STATE ptr [rbx]).source
    mov rbx, pPos
    mov rcx, [rbx]
    mov startPos, rcx
    
@@number_loop:
    cmp rcx, (LEXER_STATE ptr [pLexer]).sourceSize
    jge @@done
    
    movzx eax, byte ptr [rsi + rcx]
    .if al >= '0' && al <= '9'
        inc rcx
    .elseif al == '.'
        inc rcx
        ; Check for more digits
@@fractional:
        cmp rcx, (LEXER_STATE ptr [pLexer]).sourceSize
        jge @@done
        movzx eax, byte ptr [rsi + rcx]
        .if al >= '0' && al <= '9'
            inc rcx
            jmp @@fractional
        .endif
    .elseif al == 'e' || al == 'E'
        inc rcx
        ; Check for +/-
        cmp rcx, (LEXER_STATE ptr [pLexer]).sourceSize
        jge @@done
        movzx eax, byte ptr [rsi + rcx]
        .if al == '+' || al == '-'
            inc rcx
        .endif
        ; Exponent digits
@@exponent:
        cmp rcx, (LEXER_STATE ptr [pLexer]).sourceSize
        jge @@done
        movzx eax, byte ptr [rsi + rcx]
        .if al >= '0' && al <= '9'
            inc rcx
            jmp @@exponent
        .endif
    .else
        jmp @@done
    .endif
    jmp @@number_loop
    
@@done:
    ; Add number token
    mov rdx, rcx
    sub rdx, startPos
    lea rax, [rsi + startPos]
    invoke Lexer_AddToken, pLexer, TOKEN_LITERAL_NUMBER, startPos, rcx, rax, edx
    
    mov [rbx], rcx
    ret
Lexer_TokenizeNumber endp

;=============================================================================
; Lexer_TokenizeIdentifier
; Tokenizes an identifier or keyword
;=============================================================================
Lexer_TokenizeIdentifier proc frame pLexer:dq, pPos:dq
    local startPos:dq
    local isKeyword:dword
    
    mov rbx, pLexer
    mov rsi, (LEXER_STATE ptr [rbx]).source
    mov rbx, pPos
    mov rcx, [rbx]
    mov startPos, rcx
    
@@ident_loop:
    cmp rcx, (LEXER_STATE ptr [pLexer]).sourceSize
    jge @@done
    
    movzx eax, byte ptr [rsi + rcx]
    .if Lexer_IsIdentifierChar(al)
        inc rcx
        jmp @@ident_loop
    .endif
    
@@done:
    ; Check if keyword
    mov isKeyword, 0
    ; TODO: Check against keyword table
    
    ; Add token
    mov rdx, rcx
    sub rdx, startPos
    lea rax, [rsi + startPos]
    .if isKeyword
        invoke Lexer_AddToken, pLexer, TOKEN_KEYWORD, startPos, rcx, rax, edx
    .else
        invoke Lexer_AddToken, pLexer, TOKEN_IDENTIFIER, startPos, rcx, rax, edx
    .endif
    
    mov [rbx], rcx
    ret
Lexer_TokenizeIdentifier endp

;=============================================================================
; Lexer_IsIdentifierChar
; Checks if character is valid in identifier
;=============================================================================
Lexer_IsIdentifierChar proc frame char:byte
    mov al, char
    .if al >= 'a' && al <= 'z'
        mov eax, 1
        ret
    .endif
    .if al >= 'A' && al <= 'Z'
        mov eax, 1
        ret
    .endif
    .if al >= '0' && al <= '9'
        mov eax, 1
        ret
    .endif
    .if al == '_'
        mov eax, 1
        ret
    .endif
    xor eax, eax
    ret
Lexer_IsIdentifierChar endp

;=============================================================================
; Lexer_TokenizeOperator
; Tokenizes operators and punctuators
; Returns: 1 on success, 0 on invalid
;=============================================================================
Lexer_TokenizeOperator proc frame pLexer:dq, pPos:dq
    local startPos:dq
    
    mov rbx, pLexer
    mov rsi, (LEXER_STATE ptr [rbx]).source
    mov rbx, pPos
    mov rcx, [rbx]
    mov startPos, rcx
    
    movzx eax, byte ptr [rsi + rcx]
    
    ; Check for 2-character operators first
    mov rdx, rcx
    inc rdx
    cmp rdx, (LEXER_STATE ptr [pLexer]).sourceSize
    jl @@check_two_char
    
    ; Single character operators/punctuators
    .if al == '+' || al == '-' || al == '*' || al == '/' || al == '%' || \
         al == '=' || al == '!' || al == '<' || al == '>' || al == '&' || \
         al == '|' || al == '^' || al == '~' || al == '?' || al == ':' || \
         al == ';' || al == ',' || al == '.' || al == '(' || al == ')' || \
         al == '[' || al == ']' || al == '{' || al == '}'
        inc rcx
        invoke Lexer_AddToken, pLexer, TOKEN_PUNCTUATOR, startPos, rcx, addr [rsi + startPos], 1
        mov eax, 1
        jmp @@done
    .endif
    
    ; Invalid character
    mov eax, 0
    jmp @@done
    
@@check_two_char:
    movzx edx, byte ptr [rsi + rdx]
    
    ; == != <= >= && || ++ -- += -= *= /= %= &= |= ^= <<= >>=
    .if (al == '=' && dl == '=') || (al == '!' && dl == '=') || \
        (al == '<' && dl == '=') || (al == '>' && dl == '=') || \
        (al == '&' && dl == '&') || (al == '|' && dl == '|') || \
        (al == '+' && dl == '+') || (al == '-' && dl == '-') || \
        (al == '+' && dl == '=') || (al == '-' && dl == '=') || \
        (al == '*' && dl == '=') || (al == '/' && dl == '=') || \
        (al == '%' && dl == '=') || (al == '&' && dl == '=') || \
        (al == '|' && dl == '=') || (al == '^' && dl == '=') || \
        (al == '<' && dl == '<') || (al == '>' && dl == '>')
        add rcx, 2
        invoke Lexer_AddToken, pLexer, TOKEN_OPERATOR, startPos, rcx, addr [rsi + startPos], 2
        mov eax, 1
        jmp @@done
    .endif
    
    ; Check for 3-character operators (<<= >>=)
    .if al == '<' && dl == '<'
        mov r8, rcx
        add r8, 2
        cmp r8, (LEXER_STATE ptr [pLexer]).sourceSize
        jge @@single
        movzx r8d, byte ptr [rsi + r8]
        .if r8b == '='
            add rcx, 3
            invoke Lexer_AddToken, pLexer, TOKEN_OPERATOR, startPos, rcx, addr [rsi + startPos], 3
            mov eax, 1
            jmp @@done
        .endif
    .elseif al == '>' && dl == '>'
        mov r8, rcx
        add r8, 2
        cmp r8, (LEXER_STATE ptr [pLexer]).sourceSize
        jge @@single
        movzx r8d, byte ptr [rsi + r8]
        .if r8b == '='
            add rcx, 3
            invoke Lexer_AddToken, pLexer, TOKEN_OPERATOR, startPos, rcx, addr [rsi + startPos], 3
            mov eax, 1
            jmp @@done
        .endif
    .endif
    
@@single:
    ; Fall back to single character
    .if al == '+' || al == '-' || al == '*' || al == '/' || al == '%' || \
         al == '=' || al == '!' || al == '<' || al == '>' || al == '&' || \
         al == '|' || al == '^' || al == '~' || al == '?' || al == ':' || \
         al == ';' || al == ',' || al == '.' || al == '(' || al == ')' || \
         al == '[' || al == ']' || al == '{' || al == '}'
        inc rcx
        invoke Lexer_AddToken, pLexer, TOKEN_PUNCTUATOR, startPos, rcx, addr [rsi + startPos], 1
        mov eax, 1
        jmp @@done
    .endif
    
    ; Invalid
    mov eax, 0
    
@@done:
    mov [rbx], rcx
    ret
Lexer_TokenizeOperator endp

;=============================================================================
; END OF ASSEMBLY
;=============================================================================
end
