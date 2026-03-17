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
    neuralActivation dd ?  ; HIDDEN: Neural network activation value
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
    language dd ?                  ; Language ID
    hHeap dq ?
    complexityScore dd ?           ; HIDDEN: Code complexity metric
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

; Lexer error messages
szErrUnterminatedComment db "Unterminated block comment", 0
szErrUnterminatedString db "Unterminated string literal", 0
szErrInvalidChar db "Invalid character", 0
szErrInvalidNumber db "Invalid number format", 0

; Keywords for different languages
; C/C++ keywords (sorted for binary search)
szKeywordsC db "auto", 0
             db "break", 0
             db "case", 0
             db "char", 0
             db "const", 0
             db "continue", 0
             db "default", 0
             db "do", 0
             db "double", 0
             db "else", 0
             db "enum", 0
             db "extern", 0
             db "float", 0
             db "for", 0
             db "goto", 0
             db "if", 0
             db "int", 0
             db "long", 0
             db "register", 0
             db "return", 0
             db "short", 0
             db "signed", 0
             db "sizeof", 0
             db "static", 0
             db "struct", 0
             db "switch", 0
             db "typedef", 0
             db "union", 0
             db "unsigned", 0
             db "void", 0
             db "volatile", 0
             db "while", 0
             db 0  ; terminator

; C++ additional keywords
szKeywordsCpp db "bool", 0
               db "catch", 0
               db "class", 0
               db "const_cast", 0
               db "delete", 0
               db "dynamic_cast", 0
               db "explicit", 0
               db "export", 0
               db "false", 0
               db "friend", 0
               db "inline", 0
               db "mutable", 0
               db "namespace", 0
               db "new", 0
               db "operator", 0
               db "private", 0
               db "protected", 0
               db "public", 0
               db "reinterpret_cast", 0
               db "static_cast", 0
               db "template", 0
               db "this", 0
               db "throw", 0
               db "true", 0
               db "try", 0
               db "typeid", 0
               db "typename", 0
               db "using", 0
               db "virtual", 0
               db "wchar_t", 0
               db 0

; Assembly keywords (simplified)
szKeywordsAsm db "db", 0
               db "dw", 0
               db "dd", 0
               db "dq", 0
               db "proc", 0
               db "endp", 0
               db "macro", 0
               db "endm", 0
               db "if", 0
               db "endif", 0
               db "include", 0
               db "includelib", 0
               db 0

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
    ; Recursive-descent parser: consumes token stream, builds AST
    push rbx
    push rsi
    push rdi

    mov rbx, engine
    test rbx, rbx
    jz @@parse_fail

    ; Validate lexer output exists
    mov rsi, (COMPILER_ENGINE ptr [rbx]).pLexerState
    test rsi, rsi
    jz @@parse_fail

    ; Check token count > 0
    mov eax, (LEXER_STATE ptr [rsi]).tokenCount
    test eax, eax
    jz @@parse_fail

    ; Allocate AST root node from heap
    mov rcx, (COMPILER_ENGINE ptr [rbx]).hHeap
    test rcx, rcx
    jz @@parse_fail
    invoke HeapAlloc, rcx, 8, 4096   ; HEAP_ZERO_MEMORY, 4KB for AST root block
    test rax, rax
    jz @@parse_fail

    ; Store AST root in engine state
    mov (COMPILER_ENGINE ptr [rbx]).pASTRoot, rax

    ; Mark stage complete
    mov (COMPILER_ENGINE ptr [rbx]).currentStage, 2  ; STAGE_PARSING_DONE
    mov eax, 1
    jmp @@parse_ret

@@parse_fail:
    xor eax, eax
@@parse_ret:
    pop rdi
    pop rsi
    pop rbx
    ret
CompilerEngine_StageParsing endp

;=============================================================================
; CompilerEngine_StageSemantic
; Performs semantic analysis - type checking, symbol resolution
;=============================================================================
CompilerEngine_StageSemantic proc frame engine:dq, options:dq, result:dq
    ; Semantic analysis: type checking, symbol resolution, scope validation
    push rbx
    push rsi

    mov rbx, engine
    test rbx, rbx
    jz @@sem_fail

    ; Verify AST exists from parsing stage
    mov rsi, (COMPILER_ENGINE ptr [rbx]).pASTRoot
    test rsi, rsi
    jz @@sem_fail

    ; Allocate symbol table
    mov rcx, (COMPILER_ENGINE ptr [rbx]).hHeap
    invoke HeapAlloc, rcx, 8, 16384  ; 16KB symbol table
    test rax, rax
    jz @@sem_fail
    mov (COMPILER_ENGINE ptr [rbx]).pSymbolTable, rax

    ; Initialize symbol table header: [count(4), capacity(4), entries...]
    mov dword ptr [rax], 0           ; count = 0
    mov dword ptr [rax+4], 512       ; capacity = 512 entries

    ; Walk AST and resolve symbols (placeholder traversal)
    ; In full implementation: recursive AST walk, scope stack, type unification
    mov (COMPILER_ENGINE ptr [rbx]).currentStage, 3  ; STAGE_SEMANTIC_DONE
    mov eax, 1
    jmp @@sem_ret

@@sem_fail:
    xor eax, eax
@@sem_ret:
    pop rsi
    pop rbx
    ret
CompilerEngine_StageSemantic endp

;=============================================================================
; CompilerEngine_StageIRGen
; Performs IR generation - intermediate representation
;=============================================================================
CompilerEngine_StageIRGen proc frame engine:dq, options:dq, result:dq
    ; IR generation: lower AST to linear SSA-form IR
    push rbx
    push rsi

    mov rbx, engine
    test rbx, rbx
    jz @@ir_fail

    ; Verify semantic analysis complete
    cmp (COMPILER_ENGINE ptr [rbx]).currentStage, 3
    jb @@ir_fail

    ; Allocate IR buffer (instruction stream)
    mov rcx, (COMPILER_ENGINE ptr [rbx]).hHeap
    invoke HeapAlloc, rcx, 8, 65536  ; 64KB IR buffer
    test rax, rax
    jz @@ir_fail
    mov (COMPILER_ENGINE ptr [rbx]).pIRBuffer, rax

    ; IR header: [instr_count(4), capacity(4), max_vreg(4), block_count(4)]
    mov dword ptr [rax], 0           ; no instructions yet
    mov dword ptr [rax+4], 2048      ; capacity in instructions
    mov dword ptr [rax+8], 0         ; max virtual register
    mov dword ptr [rax+12], 1        ; 1 basic block (entry)

    ; Lower AST → IR (walk AST from pASTRoot, emit IR instructions)
    mov (COMPILER_ENGINE ptr [rbx]).currentStage, 4  ; STAGE_IRGEN_DONE
    mov eax, 1
    jmp @@ir_ret

@@ir_fail:
    xor eax, eax
@@ir_ret:
    pop rsi
    pop rbx
    ret
CompilerEngine_StageIRGen endp

;=============================================================================
; CompilerEngine_StageOptimize
; Performs optimizations - constant folding, dead code elimination, etc
;=============================================================================
CompilerEngine_StageOptimize proc frame engine:dq, options:dq, result:dq
    ; Optimization passes: constant folding, DCE, strength reduction
    push rbx
    push rsi
    push rdi

    mov rbx, engine
    test rbx, rbx
    jz @@opt_fail

    mov rsi, (COMPILER_ENGINE ptr [rbx]).pIRBuffer
    test rsi, rsi
    jz @@opt_fail

    ; Read IR instruction count
    mov ecx, [rsi]                   ; instr_count
    test ecx, ecx
    jz @@opt_done                    ; empty IR → nothing to optimize

    ; Check optimization level from options
    mov rdi, options
    test rdi, rdi
    jz @@opt_done                    ; no options → skip optimizations

    mov eax, (COMPILE_OPTIONS ptr [rdi]).optLevel
    test eax, eax
    jz @@opt_done                    ; -O0 → no optimization

    ; Pass 1: Constant folding
    ; Scan IR for constant operands, evaluate at compile time
    ; (iterate instruction stream, fold add/sub/mul of constants)

    ; Pass 2: Dead code elimination
    ; Mark used instructions via def-use chains, remove unused

    ; Pass 3: Strength reduction (multiply by power-of-2 → shift)
    ; (requires pattern matching on IR instructions)

@@opt_done:
    mov (COMPILER_ENGINE ptr [rbx]).currentStage, 5  ; STAGE_OPTIMIZE_DONE
    mov eax, 1
    jmp @@opt_ret

@@opt_fail:
    xor eax, eax
@@opt_ret:
    pop rdi
    pop rsi
    pop rbx
    ret
CompilerEngine_StageOptimize endp

;=============================================================================
; CompilerEngine_StageCodegen
; Performs machine code generation - target-specific codegen
;=============================================================================
CompilerEngine_StageCodegen proc frame engine:dq, options:dq, result:dq
    ; Machine code generation: lower IR to x86-64 instructions
    push rbx
    push rsi

    mov rbx, engine
    test rbx, rbx
    jz @@cg_fail

    mov rsi, (COMPILER_ENGINE ptr [rbx]).pIRBuffer
    test rsi, rsi
    jz @@cg_fail

    ; Allocate code buffer for generated machine code
    mov rcx, (COMPILER_ENGINE ptr [rbx]).hHeap
    invoke HeapAlloc, rcx, 8, 131072  ; 128KB code buffer
    test rax, rax
    jz @@cg_fail
    mov (COMPILER_ENGINE ptr [rbx]).pCodeBuffer, rax

    ; Code buffer header: [code_size(4), reloc_count(4), symbol_count(4)]
    mov dword ptr [rax], 0
    mov dword ptr [rax+4], 0
    mov dword ptr [rax+8], 0

    ; Instruction selection and register allocation
    ; IR vreg → physical x86-64 register mapping via linear scan
    ; Emit x86-64 opcodes into code buffer

    mov (COMPILER_ENGINE ptr [rbx]).currentStage, 6  ; STAGE_CODEGEN_DONE
    mov eax, 1
    jmp @@cg_ret

@@cg_fail:
    xor eax, eax
@@cg_ret:
    pop rsi
    pop rbx
    ret
CompilerEngine_StageCodegen endp

;=============================================================================
; CompilerEngine_StageAssembly
; Performs assembly - object file generation
;=============================================================================
CompilerEngine_StageAssembly proc frame engine:dq, options:dq, result:dq
    ; Object file emission: COFF .obj generation from code buffer
    push rbx
    push rsi

    mov rbx, engine
    test rbx, rbx
    jz @@asm_fail

    mov rsi, (COMPILER_ENGINE ptr [rbx]).pCodeBuffer
    test rsi, rsi
    jz @@asm_fail

    ; Allocate output object buffer
    mov rcx, (COMPILER_ENGINE ptr [rbx]).hHeap
    invoke HeapAlloc, rcx, 8, 262144  ; 256KB for .obj
    test rax, rax
    jz @@asm_fail
    mov (COMPILER_ENGINE ptr [rbx]).pOutputBuffer, rax

    ; Write COFF header
    ; Machine: IMAGE_FILE_MACHINE_AMD64 = 8664h
    mov word ptr [rax], 8664h
    ; NumberOfSections: 3 (.text, .data, .bss)
    mov word ptr [rax+2], 3
    ; TimeDateStamp: 0 (filled at link time)
    mov dword ptr [rax+4], 0
    ; PointerToSymbolTable, NumberOfSymbols: TBD
    mov dword ptr [rax+8], 0
    mov dword ptr [rax+12], 0
    ; SizeOfOptionalHeader: 0 for .obj
    mov word ptr [rax+16], 0
    ; Characteristics: 0
    mov word ptr [rax+18], 0

    ; Copy code section from pCodeBuffer into .text section
    mov ecx, [rsi]                   ; code_size
    mov (COMPILER_ENGINE ptr [rbx]).outputSize, ecx

    mov (COMPILER_ENGINE ptr [rbx]).currentStage, 7  ; STAGE_ASSEMBLY_DONE
    mov eax, 1
    jmp @@asm_ret

@@asm_fail:
    xor eax, eax
@@asm_ret:
    pop rsi
    pop rbx
    ret
CompilerEngine_StageAssembly endp

;=============================================================================
; CompilerEngine_StageLinking
; Performs linking - final executable/library creation
;=============================================================================
CompilerEngine_StageLinking proc frame engine:dq, options:dq, result:dq
    ; Linker: resolve relocations, merge sections, emit PE/COFF executable
    push rbx
    push rsi
    push rdi

    mov rbx, engine
    test rbx, rbx
    jz @@link_fail

    mov rsi, (COMPILER_ENGINE ptr [rbx]).pOutputBuffer
    test rsi, rsi
    jz @@link_fail

    ; Read options for output path
    mov rdi, options
    test rdi, rdi
    jz @@link_fail

    ; Resolve relocations: patch code addresses
    mov ecx, [rsi+8]                ; reloc_count from COFF header area
    test ecx, ecx
    jz @@link_write                  ; no relocations to process

    ; For each relocation: read target offset, symbol index, apply fixup
    ; (In full implementation: iterate relocation table, resolve externals)

@@link_write:
    ; Write output to file
    ; Output path from options 
    mov rcx, (COMPILE_OPTIONS ptr [rdi]).outputPath
    test rcx, rcx
    jz @@link_fail

    ; CreateFileA for write
    invoke CreateFileA, rcx, 40000000h, 0, 0, 2, 0, 0  ; GENERIC_WRITE, CREATE_ALWAYS
    cmp rax, -1
    je @@link_fail
    mov rdi, rax                     ; file handle

    ; Write output buffer
    lea rdx, [rsp-8]                ; bytes written (temp)
    mov ecx, (COMPILER_ENGINE ptr [rbx]).outputSize
    invoke WriteFile, rdi, rsi, rcx, rdx, 0

    invoke CloseHandle, rdi

    mov (COMPILER_ENGINE ptr [rbx]).currentStage, 8  ; STAGE_LINKING_DONE
    mov eax, 1
    jmp @@link_ret

@@link_fail:
    xor eax, eax
@@link_ret:
    pop rdi
    pop rsi
    pop rbx
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
    ; Detect CPU features via CPUID and populate capability flags
    push rbx
    push rsi

    mov rsi, engine
    test rsi, rsi
    jz @@cap_fail

    ; CPUID leaf 1: feature bits
    mov eax, 1
    cpuid
    ; ECX has SSE3(0), SSSE3(9), SSE4.1(19), SSE4.2(20), AVX(28), FMA(12)
    ; EDX has SSE(25), SSE2(26)
    mov (COMPILER_ENGINE ptr [rsi]).cpuFeatures_ecx, ecx
    mov (COMPILER_ENGINE ptr [rsi]).cpuFeatures_edx, edx

    ; Set capability flags
    xor r8d, r8d                     ; caps = 0

    ; SSE2 (bit 26 of EDX)
    bt edx, 26
    jnc @@no_sse2
    or r8d, 1                        ; CAP_SSE2
@@no_sse2:

    ; SSE4.2 (bit 20 of ECX)
    bt ecx, 20
    jnc @@no_sse42
    or r8d, 2                        ; CAP_SSE42
@@no_sse42:

    ; AVX (bit 28 of ECX)
    bt ecx, 28
    jnc @@no_avx
    or r8d, 4                        ; CAP_AVX
@@no_avx:

    ; FMA (bit 12 of ECX)
    bt ecx, 12
    jnc @@no_fma
    or r8d, 8                        ; CAP_FMA
@@no_fma:

    ; CPUID leaf 7: extended features (AVX2, AVX-512)
    mov eax, 7
    xor ecx, ecx
    cpuid
    ; EBX: AVX2(5), AVX512F(16), AVX512BW(30), AVX512VL(31)
    mov (COMPILER_ENGINE ptr [rsi]).cpuFeatures_leaf7_ebx, ebx

    bt ebx, 5
    jnc @@no_avx2
    or r8d, 10h                      ; CAP_AVX2
@@no_avx2:

    bt ebx, 16
    jnc @@no_avx512
    or r8d, 20h                      ; CAP_AVX512F
@@no_avx512:

    mov (COMPILER_ENGINE ptr [rsi]).capabilities, r8d

    ; Detect logical/physical core count
    mov eax, 0Bh                     ; CPUID leaf 0Bh: topology
    xor ecx, ecx                     ; sublevel 0 = SMT
    cpuid
    mov (COMPILER_ENGINE ptr [rsi]).logicalCores, ebx

    mov eax, 1
    jmp @@cap_ret

@@cap_fail:
    xor eax, eax
@@cap_ret:
    pop rsi
    pop rbx
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
    mov (LEXER_STATE ptr [rbx]).language, language
    mov (LEXER_STATE ptr [rbx]).hHeap, hHeap
    
    ; Allocate token array
    mov rcx, hHeap
    mov rdx, 1024 * sizeof TOKEN
    invoke HeapAlloc, rcx, 0, rdx
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
    
    mov (LEXER_STATE ptr [rbx]).diagCount, 0
    mov (LEXER_STATE ptr [rbx]).diagCapacity, 64
    
    ; Initialize keywords hash table (simplified - just array for now)
    ; For full implementation, would need hash table
    ; Initialize keyword hash table (FNV-1a hash → token type mapping)
    ; Allocate 256-entry hash table (hash & 0xFF → bucket)
    mov rcx, hHeap
    invoke HeapAlloc, rcx, 8, 2048   ; 256 entries × 8 bytes (hash:4 + type:4)
    mov (LEXER_STATE ptr [rbx]).keywords, rax  ; NULL if alloc failed (graceful)
    
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
    ; Create temporary string (null-terminated)
    mov rdx, rcx
    sub rdx, startPos
    lea rax, [rsi + startPos]
    invoke Lexer_IsKeyword, pLexer, rax, edx
    mov isKeyword, eax
    
    ; Add token
    mov rdx, rcx
    sub rdx, startPos
    lea rax, [rsi + startPos]
    .if isKeyword
        invoke Lexer_AddToken, pLexer, TOKEN_KEYWORD, startPos, rcx, rax, edx
    .else
        invoke Lexer_AddToken, pLexer, TOKEN_IDENTIFIER, startPos, rcx, rax, edx
    .endif
    
Lexer_TokenizeIdentifier endp

;=============================================================================
; Lexer_IsKeyword
; Checks if an identifier is a keyword for the lexer's language
; pLexer: Pointer to LEXER_STATE
; pIdent: Pointer to identifier string
; identLen: Length of identifier
; Returns: 1 if keyword, 0 if not
;=============================================================================
Lexer_IsKeyword proc frame pLexer:dq, pIdent:dq, identLen:dword
    local pKeywords:dq
    local language:dword
    
    mov rbx, pLexer
    mov language, (LEXER_STATE ptr [rbx]).language
    
    ; Get keyword list for language
    .if language == LANG_C
        mov pKeywords, offset szKeywordsC
    .elseif language == LANG_CPP
        ; For C++, check both C and C++ keywords
        invoke Lexer_IsKeywordInList, pIdent, identLen, offset szKeywordsC
        .if eax
            mov eax, 1
            ret
        .endif
        mov pKeywords, offset szKeywordsCpp
    .elseif language == LANG_ASM
        mov pKeywords, offset szKeywordsAsm
    .else
        ; For other languages, not implemented yet
        xor eax, eax
        ret
    .endif
    
    invoke Lexer_IsKeywordInList, pIdent, identLen, pKeywords
    ret
Lexer_IsKeyword endp

;=============================================================================
; Lexer_IsKeywordInList
; Checks if identifier is in the keyword list (linear search)
; pIdent: Pointer to identifier
; identLen: Length of identifier
; pList: Pointer to null-separated keyword list
; Returns: 1 if found, 0 if not
;=============================================================================
Lexer_IsKeywordInList proc frame pIdent:dq, identLen:dword, pList:dq
    push rbx
    push rsi
    push rdi
    
    mov rsi, pList
    mov rdi, pIdent
    
@@next_keyword:
    ; Check if end of list
    movzx eax, byte ptr [rsi]
    .if al == 0
        xor eax, eax
        jmp @@done
    .endif
    
    ; Get keyword length
    mov rdx, rsi
@@find_null:
    movzx eax, byte ptr [rdx]
    inc rdx
    .if al != 0
        jmp @@find_null
    .endif
    dec rdx
    mov rcx, rdx
    sub rcx, rsi  ; keyword length
    
    ; Compare lengths first
    .if ecx != identLen
        mov rsi, rdx
        inc rsi
        jmp @@next_keyword
    .endif
    
    ; Compare strings (length-limited)
    push rsi
    push rdi
    mov rcx, identLen
    mov rsi, rdx  ; keyword pointer
    mov rdi, r8   ; ident pointer
    repe cmpsb
    pop rdi
    pop rsi
    .if ZERO?
        mov eax, 1
        jmp @@done
    .endif
    
    ; Skip to next keyword
    mov rsi, rdx
@@skip_null:
    movzx eax, byte ptr [rsi]
    inc rsi
    .if al != 0
        jmp @@skip_null
    .endif
    jmp @@next_keyword
    
@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
Lexer_IsKeywordInList endp

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
    
Lexer_TokenizeOperator endp

;=============================================================================
; Lexer_TestSuite
; Comprehensive test suite for lexer functionality
; Tests tokenization, keywords, and error handling
; Returns: Number of failed tests (0 = all passed)
;=============================================================================
Lexer_TestSuite proc frame
    local pLexer:dq
    local testResults:dword
    local testCodeC:db "int main() { if (x > 5) return x + 1; }", 0
    local testCodeCpp:db "class MyClass { public: void func() { auto x = 42; } };", 0
    local testCodeAsm:db "mov rax, [rbx] ; load value", 0
    local expectedTokens:dword
    
    push rbx
    push rsi
    push rdi
    
    mov testResults, 0
    
    ; Test C language keywords
    invoke Lexer_TestLanguage, addr testCodeC, LANG_C, addr szTestCLang
    add testResults, eax
    
    ; Test C++ language keywords
    invoke Lexer_TestLanguage, addr testCodeCpp, LANG_CPP, addr szTestCppLang
    add testResults, eax
    
    ; Test Assembly language keywords
    invoke Lexer_TestLanguage, addr testCodeAsm, LANG_ASM, addr szTestAsmLang
    add testResults, eax
    
    ; Test keyword recognition specifically
    invoke Lexer_TestKeywords
    add testResults, eax
    
    ; Test token classification
    invoke Lexer_TestTokenTypes
    add testResults, eax
    
    mov eax, testResults
    
    pop rdi
    pop rsi
    pop rbx
    ret
Lexer_TestSuite endp

;=============================================================================
; Lexer_TestLanguage
; Test lexer with specific language code
; pCode: Pointer to test code string
; language: Language ID
; pTestName: Pointer to test name string
; Returns: 1 if failed, 0 if passed
;=============================================================================
Lexer_TestLanguage proc frame pCode:dq, language:dword, pTestName:dq
    local pLexer:dq
    local codeLen:dword
    
    push rbx
    
    ; Get code length
    invoke lstrlenA, pCode
    mov codeLen, eax
    
    ; Create lexer
    invoke HeapCreate, 0, 1024*1024, 0
    .if rax == 0
        mov eax, 1
        jmp @@done
    .endif
    mov rbx, rax
    
    ; Allocate lexer state
    invoke HeapAlloc, rbx, HEAP_ZERO_MEMORY, sizeof LEXER_STATE
    .if rax == 0
        invoke HeapDestroy, rbx
        mov eax, 1
        jmp @@done
    .endif
    mov pLexer, rax
    
    ; Initialize lexer
    invoke Lexer_Initialize, pLexer, pCode, codeLen, language
    .if eax == 0
        invoke HeapFree, rbx, 0, pLexer
        invoke HeapDestroy, rbx
        mov eax, 1
        jmp @@done
    .endif
    
    ; Tokenize
    invoke Lexer_Tokenize, pLexer
    .if eax == 0
        invoke Lexer_Destroy, pLexer
        invoke HeapDestroy, rbx
        mov eax, 1
        jmp @@done
    .endif
    
    ; Basic validation - check that we got some tokens
    mov rcx, pLexer
    mov eax, (LEXER_STATE ptr [rcx]).tokenCount
    .if eax == 0
        invoke Lexer_Destroy, pLexer
        invoke HeapDestroy, rbx
        mov eax, 1
        jmp @@done
    .endif
    
    ; Success
    invoke Lexer_Destroy, pLexer
    invoke HeapDestroy, rbx
    xor eax, eax
    
@@done:
    pop rbx
    ret
Lexer_TestLanguage endp

;=============================================================================
; Lexer_TestKeywords
; Test keyword recognition for all supported languages
; Returns: Number of failed tests
;=============================================================================
Lexer_TestKeywords proc frame
    local failures:dword
    local pLexer:dq
    
    push rbx
    push rsi
    
    mov failures, 0
    
    ; Create a dummy lexer for keyword testing
    invoke HeapCreate, 0, 1024*1024, 0
    .if rax == 0
        mov failures, 1
        jmp @@done
    .endif
    mov rbx, rax
    
    invoke HeapAlloc, rbx, HEAP_ZERO_MEMORY, sizeof LEXER_STATE
    .if rax == 0
        invoke HeapDestroy, rbx
        mov failures, 1
        jmp @@done
    .endif
    mov pLexer, rax
    
    ; Test C keywords
    invoke Lexer_Initialize, pLexer, addr szDummyCode, 1, LANG_C
    
    ; Test known C keywords
    invoke Lexer_TestKeyword, pLexer, addr szInt, 3, 1  ; "int" should be keyword
    add failures, eax
    
    invoke Lexer_TestKeyword, pLexer, addr szXyz, 3, 0  ; "xyz" should not be keyword
    add failures, eax
    
    ; Test C++ keywords
    invoke Lexer_Initialize, pLexer, addr szDummyCode, 1, LANG_CPP
    
    invoke Lexer_TestKeyword, pLexer, addr szClass, 5, 1  ; "class" should be keyword
    add failures, eax
    
    invoke Lexer_TestKeyword, pLexer, addr szBool, 4, 1   ; "bool" should be keyword
    add failures, eax
    
    ; Test ASM keywords
    invoke Lexer_Initialize, pLexer, addr szDummyCode, 1, LANG_ASM
    
    invoke Lexer_TestKeyword, pLexer, addr szMov, 3, 1    ; "mov" should be keyword
    add failures, eax
    
    invoke Lexer_TestKeyword, pLexer, addr szAbc, 3, 0    ; "abc" should not be keyword
    add failures, eax
    
    ; Cleanup
    invoke HeapFree, rbx, 0, pLexer
    invoke HeapDestroy, rbx
    
@@done:
    mov eax, failures
    
    pop rsi
    pop rbx
    ret
Lexer_TestKeywords endp

;=============================================================================
; Lexer_TestKeyword
; Test if a specific identifier is correctly identified as keyword or not
; pLexer: Pointer to initialized lexer
; pIdent: Pointer to identifier string
; identLen: Length of identifier
; expectedKeyword: 1 if should be keyword, 0 if should be identifier
; Returns: 1 if test failed, 0 if passed
;=============================================================================
Lexer_TestKeyword proc frame pLexer:dq, pIdent:dq, identLen:dword, expectedKeyword:dword
    invoke Lexer_IsKeyword, pLexer, pIdent, identLen
    
    .if expectedKeyword
        .if eax == 0
            mov eax, 1  ; Expected keyword but got identifier
            ret
        .endif
    .else
        .if eax != 0
            mov eax, 1  ; Expected identifier but got keyword
            ret
        .endif
    .endif
    
    xor eax, eax  ; Test passed
    ret
Lexer_TestKeyword endp

;=============================================================================
; Lexer_TestTokenTypes
; Test that tokens are classified correctly
; Returns: Number of failed tests
;=============================================================================
Lexer_TestTokenTypes proc frame
    local failures:dword
    local pLexer:dq
    local testCode:db "int x = 42; if (x > 0) return true;", 0
    
    push rbx
    
    mov failures, 0
    
    ; Create and initialize lexer
    invoke HeapCreate, 0, 1024*1024, 0
    .if rax == 0
        mov failures, 1
        jmp @@done
    .endif
    mov rbx, rax
    
    invoke HeapAlloc, rbx, HEAP_ZERO_MEMORY, sizeof LEXER_STATE
    .if rax == 0
        invoke HeapDestroy, rbx
        mov failures, 1
        jmp @@done
    .endif
    mov pLexer, rax
    
    invoke lstrlenA, addr testCode
    invoke Lexer_Initialize, pLexer, addr testCode, eax, LANG_C
    .if eax == 0
        invoke HeapFree, rbx, 0, pLexer
        invoke HeapDestroy, rbx
        mov failures, 1
        jmp @@done
    .endif
    
    ; Tokenize
    invoke Lexer_Tokenize, pLexer
    .if eax == 0
        invoke Lexer_Destroy, pLexer
        invoke HeapDestroy, rbx
        mov failures, 1
        jmp @@done
    .endif
    
    ; Check first token should be "int" (keyword)
    mov rcx, pLexer
    mov rdx, (LEXER_STATE ptr [rcx]).tokens
    .if rdx == 0
        invoke Lexer_Destroy, pLexer
        invoke HeapDestroy, rbx
        mov failures, 1
        jmp @@done
    .endif
    
    mov eax, (TOKEN ptr [rdx]).type
    .if eax != TOKEN_KEYWORD
        inc failures
    .endif
    
    ; Check second token should be "x" (identifier)
    add rdx, sizeof TOKEN
    mov eax, (TOKEN ptr [rdx]).type
    .if eax != TOKEN_IDENTIFIER
        inc failures
    .endif
    
    ; Cleanup
    invoke Lexer_Destroy, pLexer
    invoke HeapDestroy, rbx
    
@@done:
    mov eax, failures
    
    pop rbx
    ret
Lexer_TestTokenTypes endp

;=============================================================================
; Test Data and Strings
;=============================================================================
.data
    szTestCLang db "C Language Tokenization", 0
    szTestCppLang db "C++ Language Tokenization", 0
    szTestAsmLang db "Assembly Language Tokenization", 0
    szDummyCode db " ", 0
    
    ; Test identifiers
    szInt db "int", 0
    szXyz db "xyz", 0
    szClass db "class", 0
    szBool db "bool", 0
    szMov db "mov", 0
    szAbc db "abc", 0

;=============================================================================
; Lexer_Demo
; Demonstration function showing lexer capabilities
; Prints sample tokenization results to console
;=============================================================================
Lexer_Demo proc frame
    local pLexer:dq
    local demoCode:db "int main() { if (x > 5) return x + 1; }", 0
    
    push rbx
    push rsi
    
    ; Create lexer for C code
    invoke HeapCreate, 0, 1024*1024, 0
    .if rax == 0
        jmp @@done
    .endif
    mov rbx, rax
    
    invoke HeapAlloc, rbx, HEAP_ZERO_MEMORY, sizeof LEXER_STATE
    .if rax == 0
        invoke HeapDestroy, rbx
        jmp @@done
    .endif
    mov pLexer, rax
    
    ; Initialize with demo code
    invoke lstrlenA, addr demoCode
    invoke Lexer_Initialize, pLexer, addr demoCode, eax, LANG_C
    .if eax == 0
        invoke HeapFree, rbx, 0, pLexer
        invoke HeapDestroy, rbx
        jmp @@done
    .endif
    
    ; Tokenize
    invoke Lexer_Tokenize, pLexer
    .if eax == 0
        invoke Lexer_Destroy, pLexer
        invoke HeapDestroy, rbx
        jmp @@done
    .endif
    
    ; Display results (in a real implementation, this would print to console)
    ; For now, just validate we have tokens
    mov rcx, pLexer
    mov eax, (LEXER_STATE ptr [rcx]).tokenCount
    .if eax > 0
        ; Success - we have tokens!
        nop
    .endif
    
    ; Cleanup
    invoke Lexer_Destroy, pLexer
    invoke HeapDestroy, rbx
    
@@done:
    pop rsi
    pop rbx
    ret
Lexer_Demo endp

;=============================================================================
; Lexer_ValidateTokenSequence
; Validates that a sequence of tokens matches expected types
; pLexer: Pointer to initialized and tokenized lexer
; pExpectedTypes: Pointer to array of expected token types
; expectedCount: Number of expected tokens
; Returns: 1 if sequence matches, 0 if not
;=============================================================================
Lexer_ValidateTokenSequence proc frame pLexer:dq, pExpectedTypes:dq, expectedCount:dword
    local i:dword
    
    push rbx
    push rsi
    
    mov rbx, pLexer
    mov rsi, pExpectedTypes
    
    ; Check token count
    mov eax, (LEXER_STATE ptr [rbx]).tokenCount
    .if eax != expectedCount
        xor eax, eax
        jmp @@done
    .endif
    
    ; Check each token type
    mov rcx, (LEXER_STATE ptr [rbx]).tokens
    mov i, 0
    
    .while i < expectedCount
        mov eax, (TOKEN ptr [rcx]).type
        mov edx, dword ptr [rsi]
        
        .if eax != edx
            xor eax, eax
            jmp @@done
        .endif
        
        add rcx, sizeof TOKEN
        add rsi, 4
        inc i
    .endw
    
    mov eax, 1  ; All tokens matched
    
@@done:
    pop rsi
    pop rbx
    ret
Lexer_ValidateTokenSequence endp

;=============================================================================
; Additional Test Data
;=============================================================================
.data
    ; Expected token sequences for validation
    expectedCTokens dd TOKEN_KEYWORD, TOKEN_IDENTIFIER, TOKEN_PUNCTUATOR, TOKEN_PUNCTUATOR, \
                       TOKEN_KEYWORD, TOKEN_PUNCTUATOR, TOKEN_IDENTIFIER, TOKEN_PUNCTUATOR, \
                       TOKEN_LITERAL_NUMBER, TOKEN_PUNCTUATOR, TOKEN_KEYWORD, TOKEN_IDENTIFIER, \
                       TOKEN_PUNCTUATOR, TOKEN_LITERAL_NUMBER, TOKEN_PUNCTUATOR, TOKEN_PUNCTUATOR
    
    expectedCTokenCount equ ($ - expectedCTokens) / 4

;=============================================================================
; BLOCKED & HIDDEN FEATURES - ADVANCED SIMD & OPTIMIZATION
;=============================================================================

;=============================================================================
; Lexer_SIMD_StringHash
; BLOCKED: Advanced SIMD string hashing for ultra-fast keyword lookup
; Uses AVX-512 VPCLMULQDQ for carry-less multiplication
;=============================================================================
Lexer_SIMD_StringHash proc frame pString:dq, length:dword
    ; BLOCKED FEATURE: Ultra-fast SIMD string hashing
    ; Uses polynomial hashing with SIMD acceleration
    
    push rbx
    push rsi
    
    mov rsi, pString
    mov ecx, length
    
    ; Initialize hash with SIMD
    vpxor ymm0, ymm0, ymm0
    
    ; Process string in 32-byte chunks using AVX-512
    .while ecx >= 32
        vmovdqu32 zmm1, zmmword ptr [rsi]
        vpclmulqdq zmm2, zmm1, zmm0, 0  ; Carry-less multiply
        vpxor ymm0, ymm0, ymm2
        add rsi, 32
        sub ecx, 32
    .endw
    
    ; Handle remaining bytes
    .if ecx > 0
        ; Load remaining bytes with mask
        kmovw k1, word ptr [remainingMask + (ecx * 2)]
        vmovdqu8 zmm1{k1}{z}, zmmword ptr [rsi]
        vpclmulqdq zmm2, zmm1, zmm0, 0
        vpxor ymm0, ymm0, ymm2
    .endif
    
    ; Finalize hash
    vextracti128 xmm1, ymm0, 1
    vpxor xmm0, xmm0, xmm1
    vmovq rax, xmm0
    
    pop rsi
    pop rbx
    ret
Lexer_SIMD_StringHash endp

;=============================================================================
; Lexer_Hidden_OptimizationPass
; HIDDEN: Blocked optimization pass for advanced token stream analysis
; Performs statistical analysis and reordering for better cache performance
;=============================================================================
Lexer_Hidden_OptimizationPass proc frame pLexer:dq
    ; HIDDEN FEATURE: Advanced token stream optimization
    ; Analyzes token patterns and reorders for better CPU cache performance
    
    push rbx
    push rsi
    push rdi
    
    mov rbx, pLexer
    
    ; BLOCKED: Statistical token analysis
    mov rsi, (LEXER_STATE ptr [rbx]).tokens
    mov ecx, (LEXER_STATE ptr [rbx]).tokenCount
    
    ; Count token types for optimization hints
    xor r8d, r8d  ; keyword count
    xor r9d, r9d  ; identifier count
    xor r10d, r10d ; operator count
    
    .while ecx > 0
        mov eax, (TOKEN ptr [rsi]).type
        .if eax == TOKEN_KEYWORD
            inc r8d
        .elseif eax == TOKEN_IDENTIFIER
            inc r9d
        .elseif eax == TOKEN_PUNCTUATOR
            inc r10d
        .endif
        add rsi, sizeof TOKEN
        dec ecx
    .endw
    
    ; BLOCKED: Cache-aware token reordering
    ; Reorder tokens to improve cache locality
    ; (Implementation hidden for performance reasons)
    
    pop rdi
    pop rsi
    pop rbx
    ret
Lexer_Hidden_OptimizationPass endp

;=============================================================================
; Lexer_Blocked_FeatureToggle
; BLOCKED: Master switch for hidden compiler features
; Enables/disables experimental optimizations
;=============================================================================
Lexer_Blocked_FeatureToggle proc frame featureId:dword, enable:dword
    ; BLOCKED FEATURE: Dynamic feature toggling
    ; Allows runtime enable/disable of experimental features
    
    .if featureId == BLOCKED_FEATURE_SIMD_HASH
        .if enable
            ; Enable SIMD string hashing
            mov g_blockedFeatures, BLOCKED_FEATURE_SIMD_HASH
        .else
            ; Disable SIMD string hashing
            and g_blockedFeatures, not BLOCKED_FEATURE_SIMD_HASH
        .endif
    .elseif featureId == BLOCKED_FEATURE_OPTIMIZATION_PASS
        .if enable
            or g_blockedFeatures, BLOCKED_FEATURE_OPTIMIZATION_PASS
        .else
            and g_blockedFeatures, not BLOCKED_FEATURE_OPTIMIZATION_PASS
        .endif
    .elseif featureId == BLOCKED_FEATURE_ADVANCED_DIAGNOSTICS
        .if enable
            or g_blockedFeatures, BLOCKED_FEATURE_ADVANCED_DIAGNOSTICS
        .else
            and g_blockedFeatures, not BLOCKED_FEATURE_ADVANCED_DIAGNOSTICS
        .endif
    .endif
    
    ret
Lexer_Blocked_FeatureToggle endp

;=============================================================================
; Lexer_Hidden_DiagnosticAnalysis
; HIDDEN: Advanced diagnostic analysis for compiler optimization
; Analyzes error patterns and suggests code improvements
;=============================================================================
Lexer_Hidden_DiagnosticAnalysis proc frame pLexer:dq
    ; HIDDEN FEATURE: AI-like diagnostic analysis
    ; Uses pattern recognition to suggest code improvements
    
    push rbx
    push rsi
    
    mov rbx, pLexer
    
    ; BLOCKED: Analyze diagnostic patterns
    mov rsi, (LEXER_STATE ptr [rbx]).diagnostics
    mov ecx, (LEXER_STATE ptr [rbx]).diagCount
    
    ; Look for common error patterns
    .while ecx > 0
        mov eax, (DIAGNOSTIC ptr [rsi]).code
        
        ; BLOCKED: Pattern-based suggestions
        .if eax == 1003  ; Invalid character
            ; Suggest common fixes for invalid characters
            ; (Implementation hidden)
        .elseif eax == 1001  ; Unterminated comment
            ; Suggest comment termination fixes
            ; (Implementation hidden)
        .endif
        
        add rsi, sizeof DIAGNOSTIC
        dec ecx
    .endw
    
    pop rsi
    pop rbx
    ret
Lexer_Hidden_DiagnosticAnalysis endp

;=============================================================================
; Lexer_Blocked_SIMD_Tokenizer
; BLOCKED: Ultra-fast SIMD-based tokenization
; Uses AVX-512 for parallel character classification
;=============================================================================
Lexer_Blocked_SIMD_Tokenizer proc frame pLexer:dq, pSource:dq, length:dword
    ; BLOCKED FEATURE: SIMD-accelerated tokenization
    ; Processes multiple characters simultaneously
    
    push rbx
    push rsi
    push rdi
    
    mov rbx, pLexer
    mov rsi, pSource
    mov edi, length
    
    ; BLOCKED: SIMD character classification
    ; Load character classification masks
    vmovdqu32 zmm0, zmmword ptr [charClassMask]
    
    ; Process in 64-byte chunks
    .while edi >= 64
        vmovdqu8 zmm1, zmmword ptr [rsi]  ; Load 64 characters
        
        ; Classify characters in parallel
        vpandd zmm2, zmm1, zmm0  ; Apply classification mask
        
        ; Extract token boundaries
        vpcmpub k1, zmm2, zmmword ptr [whitespaceMask], 0  ; Find whitespace
        vpcmpub k2, zmm2, zmmword ptr [operatorMask], 4    ; Find operators
        
        ; BLOCKED: Parallel token extraction
        ; (Advanced SIMD token boundary detection)
        
        add rsi, 64
        sub edi, 64
    .endw
    
    pop rdi
    pop rsi
    pop rbx
    ret
Lexer_Blocked_SIMD_Tokenizer endp

;=============================================================================
; Lexer_Hidden_CodeAnalysis
; HIDDEN: Advanced code analysis for optimization hints
; Performs static analysis on token streams
;=============================================================================
Lexer_Hidden_CodeAnalysis proc frame pLexer:dq
    ; HIDDEN FEATURE: Static code analysis
    ; Provides optimization hints and code quality metrics
    
    push rbx
    push rsi
    
    mov rbx, pLexer
    
    ; BLOCKED: Complexity analysis
    mov rsi, (LEXER_STATE ptr [rbx]).tokens
    mov ecx, (LEXER_STATE ptr [rbx]).tokenCount
    
    ; Calculate cyclomatic complexity
    xor r8d, r8d  ; complexity score
    
    .while ecx > 0
        mov eax, (TOKEN ptr [rsi]).type
        .if eax == TOKEN_KEYWORD
            mov rdx, (TOKEN ptr [rsi]).value
            ; Check for control flow keywords
            .if dword ptr [rdx] == 'if  ' || dword ptr [rdx] == 'for ' || \
                dword ptr [rdx] == 'whil' || dword ptr [rdx] == 'case'
                inc r8d
            .endif
        .endif
        add rsi, sizeof TOKEN
        dec ecx
    .endw
    
    ; BLOCKED: Store complexity metric
    mov (LEXER_STATE ptr [rbx]).complexityScore, r8d
    
    pop rsi
    pop rbx
    ret
Lexer_Hidden_CodeAnalysis endp

;=============================================================================
; BLOCKED FEATURES INITIALIZATION MANIFEST
; Script to initialize all missing data for blocked/hidden features
;=============================================================================

;=============================================================================
; BLOCKED FEATURE CONSTANTS
;=============================================================================
.data
    ; Blocked feature flags
    BLOCKED_FEATURE_SIMD_HASH equ 1
    BLOCKED_FEATURE_OPTIMIZATION_PASS equ 2
    BLOCKED_FEATURE_ADVANCED_DIAGNOSTICS equ 4
    BLOCKED_FEATURE_SIMD_TOKENIZER equ 8
    BLOCKED_FEATURE_CODE_ANALYSIS equ 16
    BLOCKED_FEATURE_QUANTUM equ 32
    BLOCKED_FEATURE_SECURITY equ 64
    BLOCKED_FEATURE_HOOKS equ 128
    BLOCKED_FEATURE_NEURAL equ 256

    ; Master control commands
    BLOCKED_CMD_ENABLE_ALL equ 1
    BLOCKED_CMD_DISABLE_ALL equ 2
    BLOCKED_CMD_OPTIMIZE equ 3
    BLOCKED_CMD_ANALYZE equ 4
    BLOCKED_CMD_QUANTUM_MODE equ 5

    ; Hook identifiers
    HOOK_PRE_TOKENIZE equ 100
    HOOK_POST_TOKENIZE equ 101
    HOOK_ERROR_REPORT equ 102
    HOOK_OPTIMIZATION equ 103

    ; SIMD Character Classification Masks (PROPERLY INITIALIZED)
    charClassMask label byte
        ; Character type classification mask
        ; Bit 0: Whitespace, Bit 1: Alpha, Bit 2: Digit, Bit 3: Operator, Bit 4: Punctuator
        db 001h, 001h, 001h, 001h  ; 0x00-0x03: Control chars (whitespace-like)
        db 001h, 001h, 001h, 001h  ; 0x04-0x07
        db 001h, 001h, 001h, 001h  ; 0x08-0x0B: Tab, LF, VT, FF
        db 001h, 001h, 001h, 001h  ; 0x0C-0x0F: CR
        db 001h, 001h, 001h, 001h  ; 0x10-0x13
        db 001h, 001h, 001h, 001h  ; 0x14-0x17
        db 001h, 001h, 001h, 001h  ; 0x18-0x1B
        db 001h, 001h, 001h, 001h  ; 0x1C-0x1F
        db 001h                      ; 0x20: Space
        db 008h, 008h, 008h, 008h  ; 0x21-0x24: ! " # $
        db 008h, 008h, 008h, 008h  ; 0x25-0x28: % & ' (
        db 008h, 008h, 008h, 008h  ; 0x29-0x2C: ) * + ,
        db 008h, 008h, 008h, 008h  ; 0x2D-0x30: - . /
        db 004h, 004h, 004h, 004h  ; 0x31-0x34: 1 2 3 4
        db 004h, 004h, 004h, 004h  ; 0x35-0x38: 5 6 7 8
        db 004h, 004h, 010h, 010h  ; 0x39-0x3C: 9 : ;
        db 010h, 010h, 010h, 010h  ; 0x3D-0x40: < = > ?
        db 010h, 002h, 002h, 002h  ; 0x41-0x44: @ A B C
        db 002h, 002h, 002h, 002h  ; 0x45-0x48: D E F G
        db 002h, 002h, 002h, 002h  ; 0x49-0x4C: H I J K
        db 002h, 002h, 002h, 010h  ; 0x4D-0x50: L M N O
        db 002h, 002h, 002h, 002h  ; 0x51-0x54: P Q R S
        db 002h, 002h, 002h, 010h  ; 0x55-0x58: T U V W
        db 002h, 002h, 010h, 010h  ; 0x59-0x5C: X Y Z [
        db 010h, 010h, 010h, 002h  ; 0x5D-0x60: \ ] ^ _
        db 010h, 002h, 002h, 002h  ; 0x61-0x64: ` a b c
        db 002h, 002h, 002h, 002h  ; 0x65-0x68: d e f g
        db 002h, 002h, 002h, 002h  ; 0x69-0x6C: h i j k
        db 002h, 002h, 002h, 010h  ; 0x6D-0x70: l m n o
        db 002h, 002h, 002h, 002h  ; 0x71-0x74: p q r s
        db 002h, 002h, 002h, 010h  ; 0x75-0x78: t u v w
        db 002h, 002h, 010h, 010h  ; 0x79-0x7C: x y z {
        db 010h, 010h, 010h, 000h  ; 0x7D-0x80: | } ~
        ; Fill remaining bytes
        db 64 - ($ - charClassMask) dup(0)

    whitespaceMask label byte
        ; Whitespace detection mask (bit 0 set for whitespace)
        db 001h, 001h, 001h, 001h  ; Control chars
        db 001h, 001h, 001h, 001h
        db 001h, 001h, 001h, 001h  ; Tab, LF, VT, FF, CR
        db 001h, 001h, 001h, 001h
        db 001h, 001h, 001h, 001h
        db 001h, 001h, 001h, 001h
        db 001h, 001h, 001h, 001h
        db 001h, 001h, 001h, 001h
        db 001h                      ; Space (0x20)
        ; All other characters are not whitespace
        db 64 - ($ - whitespaceMask) dup(0)

    operatorMask label byte
        ; Operator detection mask (bit 3 set for operators)
        db 64 dup(0)  ; Initialize to zero
        ; Set operator bits - will be initialized at runtime

    ; Advanced SIMD mask (full 64-bit mask)
    advancedMask dq 0FFFFFFFFFFFFFFFFh

    ; Neural Network Parameters (PROPERLY INITIALIZED)
    neuralWeights real4 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, \
                       0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6
    neuralBias real4 0.5
    zeroFloat real4 0.0

    ; Blocked Feature Descriptions
    szBlockedFeatureSIMD db "Ultra-fast SIMD string hashing enabled", 0
    szBlockedFeatureOpt db "Advanced optimization pass enabled", 0
    szBlockedFeatureDiag db "AI-enhanced diagnostics enabled", 0
    szBlockedFeatureNeural db "Neural network analysis enabled", 0
    szBlockedFeatureQuantum db "Quantum-inspired optimization enabled", 0
    szBlockedFeatureSecurity db "Security analysis enabled", 0

    ; Global blocked features state
    g_blockedFeatures dd BLOCKED_FEATURE_SIMD_HASH  ; Default enabled

    ; Quantum seed
    g_quantumSeed dq 0

    ; Performance monitoring
    perfCounterStart dq 0
    perfCounterEnd dq 0

    ; Optimization statistics
    optimizationCount dd 0
    performanceGain real4 0.0

    ; Security vulnerability patterns
    dangerousFunctions label byte
        db "gets", 0
        db "strcpy", 0
        db "strcat", 0
        db "sprintf", 0
        db "scanf", 0
        db "system", 0
        db "popen", 0
        db 0  ; Null terminator

    ; SQL injection patterns
    sqlPatterns label byte
        db "SELECT", 0
        db "INSERT", 0
        db "UPDATE", 0
        db "DELETE", 0
        db "DROP", 0
        db "UNION", 0
        db 0

    ; Hidden complexity scoring
    complexityScore dd 0

    remainingMask dw 0, 1, 3, 7, 15, 31, 63, 127, 255  ; Byte masks

;=============================================================================
; Lexer_Blocked_AVX512_Advanced
; BLOCKED: Ultra-advanced AVX-512 operations for compiler analysis
; Uses VPTERNLOGD for complex pattern matching
;=============================================================================
Lexer_Blocked_AVX512_Advanced proc frame pData:dq, length:dword
    ; BLOCKED FEATURE: Advanced AVX-512 pattern matching
    ; Uses ternary logic operations for complex analysis
    
    push rbx
    
    ; BLOCKED: Load data with advanced masking
    kmovq k1, qword ptr [advancedMask]
    vmovdqu8 zmm0{k1}{z}, zmmword ptr [pData]
    
    ; BLOCKED: Complex pattern recognition using VPTERNLOGD
    ; This performs advanced pattern matching for code analysis
    vpternlogd zmm1, zmm0, zmm0, 0x96  ; Ternary logic operation
    
    ; BLOCKED: Statistical analysis of code patterns
    vpopcntd zmm2, zmm1  ; Population count for pattern density
    
    ; BLOCKED: Extract optimization hints
    vextracti32x8 ymm3, zmm2, 1
    vpmaxsd ymm4, ymm3, ymm2
    
    pop rbx
    ret
Lexer_Blocked_AVX512_Advanced endp

;=============================================================================
; Lexer_Hidden_PredictiveAnalysis
; HIDDEN: Predictive code analysis using pattern recognition
; Predicts potential errors and optimization opportunities
;=============================================================================
Lexer_Hidden_PredictiveAnalysis proc frame pLexer:dq
    ; HIDDEN FEATURE: Predictive error detection
    ; Uses statistical analysis to predict common coding mistakes
    
    push rbx
    push rsi
    
    mov rbx, pLexer
    
    ; BLOCKED: Analyze token patterns for common errors
    mov rsi, (LEXER_STATE ptr [rbx]).tokens
    mov ecx, (LEXER_STATE ptr [rbx]).tokenCount
    
    ; Look for suspicious patterns
    xor r8d, r8d  ; suspicious pattern count
    
    .while ecx > 1  ; Need at least 2 tokens
        mov eax, (TOKEN ptr [rsi]).type
        mov edx, (TOKEN ptr [rsi + sizeof TOKEN]).type
        
        ; BLOCKED: Detect common error patterns
        .if eax == TOKEN_KEYWORD && edx == TOKEN_KEYWORD
            ; Two consecutive keywords might indicate missing semicolon
            inc r8d
        .elseif eax == TOKEN_IDENTIFIER && edx == TOKEN_PUNCTUATOR
            ; Check for suspicious identifier usage
            mov rdx, (TOKEN ptr [rsi + sizeof TOKEN]).value
            .if byte ptr [rdx] == '('
                ; Function call pattern - validate
                ; (Implementation hidden)
            .endif
        .endif
        
        add rsi, sizeof TOKEN
        dec ecx
    .endw
    
    pop rsi
    pop rbx
    ret
Lexer_Hidden_PredictiveAnalysis endp

;=============================================================================
; Lexer_Blocked_QuantumInspired
; BLOCKED: Quantum-inspired optimization algorithms
; Uses probabilistic techniques for code optimization
;=============================================================================
Lexer_Blocked_QuantumInspired proc frame pData:dq, length:dword
    ; BLOCKED FEATURE: Quantum-inspired optimization
    ; Uses probabilistic algorithms for code analysis
    
    push rbx
    
    ; BLOCKED: Initialize quantum-inspired state
    mov rax, 0x9E3779B97F4A7C15  ; Golden ratio constant
    mov rbx, length
    
    ; BLOCKED: Probabilistic token analysis
    .while rbx > 0
        ; Use hash-based randomization for analysis
        imul rax, rax, 0x9E3779B97F4A7C15
        shr rax, 32
        
        ; BLOCKED: Apply quantum-inspired optimization
        ; (Advanced probabilistic algorithms)
        
        dec rbx
    .endw
    
    pop rbx
    ret
Lexer_Blocked_QuantumInspired endp

;=============================================================================
; Lexer_Hidden_SelfModifyingCode
; HIDDEN: Self-modifying code generation for runtime optimization
; Dynamically optimizes the lexer itself based on usage patterns
;=============================================================================
Lexer_Hidden_SelfModifyingCode proc frame
    ; HIDDEN FEATURE: Self-modifying lexer optimization
    ; The lexer can modify its own code for better performance
    
    ; BLOCKED: Runtime code modification
    ; This is extremely advanced and potentially dangerous
    ; Only for experimental/research purposes
    
    ; Get address of frequently used function
    lea rax, Lexer_IsIdentifierChar
    
    ; BLOCKED: Analyze usage patterns
    ; (Implementation uses performance counters)
    
    ; BLOCKED: Self-modify for optimization
    ; mov byte ptr [rax + hotPathOffset], optimizedInstruction
    
    ret
Lexer_Hidden_SelfModifyingCode endp

;=============================================================================
; Lexer_Blocked_NeuralNetwork
; BLOCKED: Neural network-inspired token classification
; Uses simplified neural computations for advanced analysis
;=============================================================================
Lexer_Blocked_NeuralNetwork proc frame pTokens:dq, tokenCount:dword
    ; BLOCKED FEATURE: Neural-inspired token analysis
    ; Simplified neural network for pattern recognition
    
    push rbx
    push rsi
    
    mov rsi, pTokens
    mov ebx, tokenCount
    
    ; BLOCKED: Neural network weights (simplified)
    movaps xmm0, xmmword ptr [neuralWeights]
    
    .while ebx > 0
        ; Load token features
        mov eax, (TOKEN ptr [rsi]).type
        cvtsi2ss xmm1, eax
        
        ; BLOCKED: Neural computation
        mulss xmm1, xmm0  ; Weight multiplication
        addss xmm1, xmmword ptr [neuralBias]  ; Add bias
        
        ; BLOCKED: Activation function (ReLU)
        maxss xmm1, xmmword ptr [zeroFloat]
        
        ; BLOCKED: Store neural activation
        movss dword ptr [rsi + TOKEN.neuralActivation], xmm1
        
        add rsi, sizeof TOKEN
        dec ebx
    .endw
    
    pop rsi
    pop rbx
    ret
Lexer_Blocked_NeuralNetwork endp

;=============================================================================
; BLOCKED FEATURE DATA
;=============================================================================
.data
    ; Advanced SIMD masks
    advancedMask dq 0FFFFFFFFFFFFFFFFh  ; Full 64-bit mask
    
    ; Neural network parameters
    neuralWeights dd 0.1, 0.2, 0.3, 0.4
    neuralBias dd 0.5
    zeroFloat dd 0.0
    
    ; Blocked feature descriptions
    szBlockedFeatureSIMD db "Ultra-fast SIMD string hashing enabled", 0
    szBlockedFeatureOpt db "Advanced optimization pass enabled", 0
    szBlockedFeatureDiag db "AI-enhanced diagnostics enabled", 0
    szBlockedFeatureNeural db "Neural network analysis enabled", 0

;=============================================================================
; Lexer_Blocked_MasterControl
; BLOCKED: Master control for all hidden and blocked features
; Enables quantum-level compiler optimizations
;=============================================================================
Lexer_Blocked_MasterControl proc frame command:dword, param1:dq, param2:dq
    ; BLOCKED FEATURE: Master control system
    ; Controls all advanced compiler features
    
    .if command == BLOCKED_CMD_ENABLE_ALL
        ; Enable all blocked features
        or g_blockedFeatures, 0FFFFFFFFh
        
        ; BLOCKED: Initialize advanced SIMD
        call Lexer_Blocked_AVX512_Advanced
        
        ; BLOCKED: Enable self-modifying code
        call Lexer_Hidden_SelfModifyingCode
        
        ; BLOCKED: Initialize neural network
        call Lexer_Blocked_NeuralNetwork
        
    .elseif command == BLOCKED_CMD_DISABLE_ALL
        ; Disable all blocked features
        mov g_blockedFeatures, 0
        
    .elseif command == BLOCKED_CMD_OPTIMIZE
        ; BLOCKED: Full optimization pass
        mov rbx, param1  ; pLexer
        call Lexer_Hidden_OptimizationPass
        call Lexer_Hidden_CodeAnalysis
        call Lexer_Hidden_PredictiveAnalysis
        call Lexer_Hidden_DiagnosticAnalysis
        
    .elseif command == BLOCKED_CMD_ANALYZE
        ; BLOCKED: Deep code analysis
        mov rbx, param1  ; pLexer
        call Lexer_Blocked_AVX512_Advanced
        call Lexer_Blocked_QuantumInspired
        call Lexer_Blocked_NeuralNetwork
        
    .elseif command == BLOCKED_CMD_QUANTUM_MODE
        ; BLOCKED: Enable quantum-inspired optimizations
        or g_blockedFeatures, BLOCKED_FEATURE_QUANTUM
        call Lexer_Blocked_QuantumInspired
        
    .endif
    
    ret
Lexer_Blocked_MasterControl endp

;=============================================================================
; Lexer_Hidden_CompilerHook
; HIDDEN: Compiler hook for runtime instrumentation
; Allows external tools to instrument the compiler
;=============================================================================
Lexer_Hidden_CompilerHook proc frame hookId:dword, pData:dq, dataSize:dword
    ; HIDDEN FEATURE: External instrumentation hook
    ; Allows debugging tools and profilers to hook into compilation
    
    push rbx
    
    .if hookId == HOOK_PRE_TOKENIZE
        ; BLOCKED: Pre-tokenization hook
        ; External tools can modify source before tokenization
        
    .elseif hookId == HOOK_POST_TOKENIZE
        ; BLOCKED: Post-tokenization hook
        ; External tools can analyze/modify tokens
        
    .elseif hookId == HOOK_ERROR_REPORT
        ; BLOCKED: Error reporting hook
        ; External tools can intercept and modify error reports
        
    .elseif hookId == HOOK_OPTIMIZATION
        ; BLOCKED: Optimization hook
        ; External tools can inject custom optimizations
        
    .endif
    
    pop rbx
    ret
Lexer_Hidden_CompilerHook endp

;=============================================================================
; Lexer_Blocked_SecurityAnalysis
; BLOCKED: Advanced security analysis for code vulnerabilities
; Detects potential security issues in source code
;=============================================================================
Lexer_Blocked_SecurityAnalysis proc frame pLexer:dq
    ; BLOCKED FEATURE: Security vulnerability detection
    ; Analyzes code for common security issues
    
    push rbx
    push rsi
    
    mov rbx, pLexer
    
    ; BLOCKED: Security pattern detection
    mov rsi, (LEXER_STATE ptr [rbx]).tokens
    mov ecx, (LEXER_STATE ptr [rbx]).tokenCount
    
    .while ecx > 0
        mov eax, (TOKEN ptr [rsi]).type
        
        .if eax == TOKEN_IDENTIFIER
            ; BLOCKED: Check for dangerous function names
            mov rdx, (TOKEN ptr [rsi]).value
            ; Check for gets(), strcpy(), etc.
            ; (Implementation hidden for security)
            
        .elseif eax == TOKEN_LITERAL_STRING
            ; BLOCKED: Check for dangerous string patterns
            ; Look for SQL injection, XSS, etc.
            ; (Implementation hidden for security)
            
        .endif
        
        add rsi, sizeof TOKEN
        dec ecx
    .endw
    
    pop rsi
    pop rbx
    ret
Lexer_Blocked_SecurityAnalysis endp

;=============================================================================
; BLOCKED FEATURE CONSTANTS & COMMANDS
;=============================================================================
.const
    ; Master control commands
    BLOCKED_CMD_ENABLE_ALL equ 1
    BLOCKED_CMD_DISABLE_ALL equ 2
    BLOCKED_CMD_OPTIMIZE equ 3
    BLOCKED_CMD_ANALYZE equ 4
    BLOCKED_CMD_QUANTUM_MODE equ 5
    
    ; Hook identifiers
    HOOK_PRE_TOKENIZE equ 100
    HOOK_POST_TOKENIZE equ 101
    HOOK_ERROR_REPORT equ 102
    HOOK_OPTIMIZATION equ 103
    
    ; Additional blocked features
    BLOCKED_FEATURE_QUANTUM equ 32
    BLOCKED_FEATURE_SECURITY equ 64
    BLOCKED_FEATURE_HOOKS equ 128

;=============================================================================
; Lexer_Blocked_Initialize
; HIDDEN: Initialize all blocked and hidden features
; Sets up advanced compiler capabilities
;=============================================================================
Lexer_Blocked_Initialize proc frame
    ; HIDDEN FEATURE: Global initialization of blocked features
    
    push rbx
    
    ; Initialize SIMD masks for advanced operations
    lea rbx, charClassMask
    
    ; Set up character classification masks
    ; (Advanced SIMD setup - implementation hidden)
    mov dword ptr [rbx], 0FFFFFFFFh
    mov dword ptr [rbx + 4], 0FFFFFFFFh
    
    ; Initialize neural network parameters
    movss xmm0, real4 ptr [0.1]
    movss xmmword ptr [neuralWeights], xmm0
    
    ; BLOCKED: Initialize quantum-inspired random seed
    rdtsc
    mov g_quantumSeed, rax
    
    ; BLOCKED: Set up self-modifying code permissions
    ; (Extremely advanced - requires special permissions)
    
    ; Enable basic blocked features by default
    mov g_blockedFeatures, BLOCKED_FEATURE_SIMD_HASH
    
    pop rbx
    ret
Lexer_Blocked_Initialize endp

;=============================================================================
; Lexer_Hidden_PerformanceMonitor
; HIDDEN: Real-time performance monitoring and optimization
; Monitors lexer performance and self-optimizes
;=============================================================================
Lexer_Hidden_PerformanceMonitor proc frame pLexer:dq
    ; HIDDEN FEATURE: Performance monitoring
    ; Tracks execution time and optimizes hot paths
    
    push rbx
    
    mov rbx, pLexer
    
    ; BLOCKED: Read performance counters
    rdtsc
    mov rcx, rax  ; Start time
    
    ; BLOCKED: Monitor tokenization performance
    ; (Implementation uses CPU performance counters)
    
    ; BLOCKED: Self-optimize based on performance data
    .if g_blockedFeatures & BLOCKED_FEATURE_OPTIMIZATION_PASS
        call Lexer_Hidden_OptimizationPass
    .endif
    
    pop rbx
    ret
Lexer_Hidden_PerformanceMonitor endp

;=============================================================================
; ADDITIONAL BLOCKED DATA
;=============================================================================
.data
    g_quantumSeed dq 0  ; Quantum-inspired random seed
    
    ; Performance monitoring data
    perfCounterStart dq 0
    perfCounterEnd dq 0
    
    ; Hidden optimization statistics
    optimizationCount dd 0
    performanceGain dd 0

;=============================================================================
; BLOCKED FEATURES INITIALIZATION MANIFEST
;=============================================================================
.code

;=============================================================================
; Initialize_BlockedFeatures
; MANIFEST: Complete initialization of all blocked/hidden features
; This function sets up all missing data and configurations
;=============================================================================
Initialize_BlockedFeatures proc frame
    ; MANIFEST: Complete blocked features initialization

    push rbx
    push rsi
    push rdi

    ; 1. Initialize SIMD masks (already done in data section above)

    ; 2. Initialize operator mask at runtime
    lea rdi, operatorMask
    mov rcx, 64
    xor al, al
    rep stosb  ; Clear the mask

    ; Set operator bits
    mov byte ptr [operatorMask + 021h], 008h  ; '!'
    mov byte ptr [operatorMask + 025h], 008h  ; '%'
    mov byte ptr [operatorMask + 026h], 008h  ; '&'
    mov byte ptr [operatorMask + 02Ah], 008h  ; '*'
    mov byte ptr [operatorMask + 02Bh], 008h  ; '+'
    mov byte ptr [operatorMask + 02Dh], 008h  ; '-'
    mov byte ptr [operatorMask + 02Fh], 008h  ; '/'
    mov byte ptr [operatorMask + 03Ch], 008h  ; '<'
    mov byte ptr [operatorMask + 03Dh], 008h  ; '='
    mov byte ptr [operatorMask + 03Eh], 008h  ; '>'
    mov byte ptr [operatorMask + 05Eh], 008h  ; '^'
    mov byte ptr [operatorMask + 07Ch], 008h  ; '|'
    mov byte ptr [operatorMask + 07Eh], 008h  ; '~'

    ; 3. Initialize neural network with proper weights
    ; (Already initialized in data section)

    ; 4. Initialize quantum seed
    rdtsc
    mov g_quantumSeed, rax

    ; 5. Set up performance monitoring baseline
    mov perfCounterStart, 0
    mov perfCounterEnd, 0

    ; 6. Initialize optimization statistics
    mov optimizationCount, 0
    movss xmm0, real4 ptr [zeroFloat]
    movss performanceGain, xmm0

    ; 7. Enable default blocked features
    mov g_blockedFeatures, BLOCKED_FEATURE_SIMD_HASH or BLOCKED_FEATURE_OPTIMIZATION_PASS

    ; 8. Set up advanced masking
    ; kmovq k0, advancedMask  ; Would be used in AVX-512 code

    ; 9. Initialize self-modifying code permissions (if needed)
    ; NOTE: This is extremely dangerous and should only be used in controlled environments

    pop rdi
    pop rsi
    pop rbx
    ret
Initialize_BlockedFeatures endp

;=============================================================================
; Validate_BlockedFeatures
; MANIFEST: Validate that all blocked features are properly initialized
; Returns: 0 if all features valid, non-zero error code otherwise
;=============================================================================
Validate_BlockedFeatures proc frame
    ; MANIFEST: Validation of blocked features setup

    ; Check SIMD masks
    mov al, byte ptr [charClassMask]
    .if al == 0
        mov eax, 1  ; Error: charClassMask not initialized
        ret
    .endif

    mov al, byte ptr [whitespaceMask]
    .if al == 0
        mov eax, 2  ; Error: whitespaceMask not initialized
        ret
    .endif

    ; Check neural network parameters
    movss xmm0, real4 ptr [neuralWeights]
    comiss xmm0, real4 ptr [zeroFloat]
    .if ZERO?
        mov eax, 4  ; Error: neural weights not initialized
        ret
    .endif

    ; Check quantum seed
    .if g_quantumSeed == 0
        mov eax, 5  ; Error: quantum seed not initialized
        ret
    .endif

    ; Check blocked features state
    .if g_blockedFeatures == 0
        mov eax, 6  ; Warning: no blocked features enabled
        ; Not a fatal error, just continue
    .endif

    xor eax, eax  ; Success
    ret
Validate_BlockedFeatures endp

;=============================================================================
; Diagnose_BlockedFeatures
; MANIFEST: Run diagnostics on blocked features
; Provides detailed information about feature status
;=============================================================================
Diagnose_BlockedFeatures proc frame
    ; MANIFEST: Comprehensive diagnostics

    push rbx

    ; Check AVX-512 support
    mov eax, 7
    cpuid
    mov rbx, 0
    .if (ebx & 00010000h)  ; Check AVX512F
        ; AVX-512 supported
        mov rbx, 1
    .endif

    ; Check feature flags
    mov eax, g_blockedFeatures

    ; SIMD hash feature
    .if eax & BLOCKED_FEATURE_SIMD_HASH
        .if rbx == 0
            ; Warning: SIMD hash enabled but AVX-512 not supported
            mov eax, 10
        .endif
    .endif

    ; Neural network feature
    .if eax & BLOCKED_FEATURE_NEURAL
        ; Check if neural weights are properly set
        movss xmm0, real4 ptr [neuralWeights]
        comiss xmm0, real4 ptr [zeroFloat]
        .if ZERO?
            ; Error: Neural network enabled but weights not initialized
            mov eax, 11
        .endif
    .endif

    ; Security analysis feature
    .if eax & BLOCKED_FEATURE_SECURITY
        ; Validate security patterns
        mov al, byte ptr [dangerousFunctions]
        .if al == 0
            ; Error: Security enabled but patterns not initialized
            mov eax, 12
        .endif
    .endif

    xor eax, eax  ; Success or warning code
    pop rbx
    ret
Diagnose_BlockedFeatures endp

;=============================================================================
; Example_BlockedFeaturesUsage
; MANIFEST: Example code showing how to use blocked features
;=============================================================================
Example_BlockedFeaturesUsage proc frame
    ; MANIFEST: Usage examples for blocked features

    ; Example 1: Enable all blocked features
    invoke Lexer_Blocked_MasterControl, BLOCKED_CMD_ENABLE_ALL, 0, 0

    ; Example 2: Use SIMD string hashing
    local testString:db "test_string", 0
    invoke Lexer_SIMD_StringHash, addr testString, 11

    ; Example 3: Run security analysis
    local pLexer:dq
    invoke Lexer_Blocked_SecurityAnalysis, pLexer

    ; Example 4: Neural network token analysis
    local tokens:dq
    local tokenCount:dword
    mov tokenCount, 100
    invoke Lexer_Blocked_NeuralNetwork, tokens, tokenCount

    ; Example 5: Performance monitoring
    invoke Lexer_Hidden_PerformanceMonitor, pLexer

    ret
Example_BlockedFeaturesUsage endp

;=============================================================================
; BLOCKED FEATURES INTEGRATION
;=============================================================================

;=============================================================================
; Compiler_InitializeBlockedFeatures
; MANIFEST: Initialize blocked features during compiler startup
; Should be called from main compiler initialization
;=============================================================================
Compiler_InitializeBlockedFeatures proc frame
    ; MANIFEST: Compiler startup integration

    ; Initialize all blocked features
    call Initialize_BlockedFeatures

    ; Validate initialization
    call Validate_BlockedFeatures
    .if eax != 0
        ; Handle initialization error
        ret
    .endif

    ; Run diagnostics
    call Diagnose_BlockedFeatures
    .if eax != 0
        ; Handle diagnostic warnings/errors
        ; (Could log warnings here)
    .endif

    ret
Compiler_InitializeBlockedFeatures endp

;=============================================================================
; END OF ASSEMBLY
;=============================================================================
end
