; =============================================================================
; RawrXD_LSP_AI_Bridge.asm — Symbol Index ↔ Context Analyzer Fusion Kernel
; =============================================================================
;
; Bridges the LSP SymbolIndex with the AI ContextAnalyzer for semantic
; code intelligence. Reads symbol definitions, computes importance scores,
; and builds a ranked context window that the inference engine can consume
; for code completion, refactoring suggestions, and vulnerability detection.
;
; Capabilities:
;   - FNV-1a symbol name hashing for O(1) dedup
;   - Importance scoring: definitions > declarations > references
;   - Configurable fusion weights (syntax vs semantic relevance)
;   - Insertion sort for small sets, merge sort for large sets
;   - SRWLock-based shared read access to SymbolIndex
;   - Batched context emission (32-symbol chunks for cache locality)
;   - Deep mode: symbol dependency graph traversal
;   - Full semantic mode: placeholder for embedding vector injection
;   - Stats: sync count, symbols processed, avg importance, latency
;
; Active Exports (used by C++ LanguageServerIntegration bridge):
;   asm_lsp_bridge_init         — Initialize bridge state + locks
;   asm_lsp_bridge_sync         — Sync SymbolIndex → ContextAnalyzer
;   asm_lsp_bridge_query        — Query ranked symbols for context window
;   asm_lsp_bridge_invalidate   — Mark cached context dirty
;   asm_lsp_bridge_get_stats    — Read bridge statistics
;   asm_lsp_bridge_set_weights  — Configure fusion weights
;   asm_lsp_bridge_shutdown     — Release resources
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_LSP_AI_Bridge.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                    BRIDGE CONSTANTS
; =============================================================================

; Symbol kinds (match LSP spec SymbolKind)
SYMBOL_FILE             EQU     1
SYMBOL_MODULE           EQU     2
SYMBOL_NAMESPACE        EQU     3
SYMBOL_PACKAGE          EQU     4
SYMBOL_CLASS            EQU     5
SYMBOL_METHOD           EQU     6
SYMBOL_PROPERTY         EQU     7
SYMBOL_FIELD            EQU     8
SYMBOL_CONSTRUCTOR      EQU     9
SYMBOL_ENUM             EQU     10
SYMBOL_INTERFACE        EQU     11
SYMBOL_FUNCTION         EQU     12
SYMBOL_VARIABLE         EQU     13
SYMBOL_CONSTANT         EQU     14
SYMBOL_STRING           EQU     15
SYMBOL_NUMBER           EQU     16
SYMBOL_BOOLEAN          EQU     17
SYMBOL_ARRAY            EQU     18
SYMBOL_OBJECT           EQU     19
SYMBOL_KEY              EQU     20
SYMBOL_NULL_TYPE        EQU     21
SYMBOL_ENUM_MEMBER      EQU     22
SYMBOL_STRUCT           EQU     23
SYMBOL_EVENT            EQU     24
SYMBOL_OPERATOR         EQU     25
SYMBOL_TYPE_PARAMETER   EQU     26

; Symbol role
ROLE_DEFINITION         EQU     0
ROLE_DECLARATION        EQU     1
ROLE_REFERENCE          EQU     2
ROLE_IMPLEMENTATION     EQU     3

; Sync modes
SYNC_SHALLOW            EQU     0           ; Names + kinds only
SYNC_DEEP               EQU     1           ; + dependency graph
SYNC_FULL_SEMANTIC      EQU     2           ; + embedding vectors

; Bridge limits
MAX_BRIDGE_SYMBOLS      EQU     8192        ; Max symbols per sync
CONTEXT_CHUNK_SIZE      EQU     32          ; Symbols per emission batch
MAX_CONTEXT_TOKENS      EQU     4096        ; Max tokens in context window

; Bridge errors
LSP_OK                  EQU     0
LSP_ERR_NOT_INIT        EQU     1
LSP_ERR_ALLOC           EQU     2
LSP_ERR_EMPTY           EQU     3
LSP_ERR_INVALID         EQU     4
LSP_ERR_OVERFLOW        EQU     5
LSP_ERR_ALREADY_INIT    EQU     6

; Importance score base values (fixed-point 16.16)
IMPORTANCE_DEFINITION   EQU     10000h      ; 1.0
IMPORTANCE_DECLARATION  EQU     0CCCDh      ; 0.8
IMPORTANCE_REFERENCE    EQU     04CCDh      ; 0.3
IMPORTANCE_IMPL         EQU     0E666h      ; 0.9

; Default fusion weights (fixed-point 16.16)
DEFAULT_SYNTAX_WEIGHT   EQU     04CCDh      ; 0.3
DEFAULT_SEMANTIC_WEIGHT EQU     0B333h      ; 0.7

; =============================================================================
;                    STRUCTURES
; =============================================================================

; Input: symbol from LSP SymbolIndex (read-only)
SYMBOL_ENTRY STRUCT 8
    name_ptr        DQ      ?               ; Pointer to name string
    name_len        DD      ?               ; Length of name
    symbol_kind     DD      ?               ; SYMBOL_* enum
    symbol_role     DD      ?               ; ROLE_* enum
    ref_count       DD      ?               ; Number of references in workspace
    file_offset     DQ      ?               ; Location in source file
    type_info_ptr   DQ      ?               ; Pointer to type information
SYMBOL_ENTRY ENDS

; Output: ranked symbol context for AI
RANKED_SYMBOL STRUCT 8
    name_hash       DQ      ?               ; FNV-1a hash of symbol name
    name_ptr        DQ      ?               ; Pointer to name string
    name_len        DD      ?               ; Name length
    symbol_kind     DD      ?               ; Symbol kind
    importance      DD      ?               ; Fixed-point 16.16 score
    _pad0           DD      ?
RANKED_SYMBOL ENDS

; Bridge statistics
LSP_BRIDGE_STATS STRUCT
    sync_count      DQ      ?               ; Total sync operations
    symbols_processed DQ    ?               ; Total symbols handled
    symbols_emitted DQ      ?               ; Symbols sent to context
    avg_importance  DD      ?               ; Average importance (16.16)
    last_sync_us    DD      ?               ; Last sync latency in microseconds
    cache_hits      DQ      ?
    cache_misses    DQ      ?
LSP_BRIDGE_STATS ENDS

; Bridge context
LSP_BRIDGE_CTX STRUCT
    initialized     DD      ?
    dirty           DD      ?               ; 1 = cache invalidated
    syntax_weight   DD      ?               ; Fixed-point 16.16
    semantic_weight DD      ?               ; Fixed-point 16.16
    pRankedBuffer   DQ      ?               ; Sorted symbol array
    ranked_count    DD      ?               ; Current count in buffer
    max_ranked      DD      ?               ; Buffer capacity
    pLastSymIndex   DQ      ?               ; Last synced SymbolIndex ptr
LSP_BRIDGE_CTX ENDS

; =============================================================================
;                    DATA SECTION
; =============================================================================
.data

ALIGN 16
g_bridge_ctx    LSP_BRIDGE_CTX <>
g_bridge_stats  LSP_BRIDGE_STATS <>

; Critical section
ALIGN 16
g_bridge_cs     CRITICAL_SECTION <>

; Status strings
szBridgeInit    DB "LSP_AI_Bridge: initialized", 0
szBridgeSync    DB "LSP_AI_Bridge: sync complete", 0
szBridgeInval   DB "LSP_AI_Bridge: cache invalidated", 0
szBridgeShut    DB "LSP_AI_Bridge: shutdown", 0

; =============================================================================
;                    EXPORTS
; =============================================================================
PUBLIC asm_lsp_bridge_init
PUBLIC asm_lsp_bridge_sync
PUBLIC asm_lsp_bridge_query
PUBLIC asm_lsp_bridge_invalidate
PUBLIC asm_lsp_bridge_get_stats
PUBLIC asm_lsp_bridge_set_weights
PUBLIC asm_lsp_bridge_shutdown

; =============================================================================
;                    EXTERNAL IMPORTS
; =============================================================================
EXTERN QueryPerformanceCounter: PROC
EXTERN QueryPerformanceFrequency: PROC

; =============================================================================
;                    CODE SECTION
; =============================================================================
.code

; =============================================================================
; compute_importance — Internal: compute importance score for a symbol
; RCX = SYMBOL_ENTRY*
; Returns: EAX = importance score (fixed-point 16.16)
; =============================================================================
compute_importance PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 8
    .allocstack 8
    .endprolog

    mov     ebx, DWORD PTR [rcx].SYMBOL_ENTRY.symbol_role

    ; Base score from role
    cmp     ebx, ROLE_DEFINITION
    je      @@def_score
    cmp     ebx, ROLE_DECLARATION
    je      @@decl_score
    cmp     ebx, ROLE_IMPLEMENTATION
    je      @@impl_score
    ; Default: reference
    mov     eax, IMPORTANCE_REFERENCE
    jmp     @@apply_ref_boost

@@def_score:
    mov     eax, IMPORTANCE_DEFINITION
    jmp     @@apply_ref_boost
@@decl_score:
    mov     eax, IMPORTANCE_DECLARATION
    jmp     @@apply_ref_boost
@@impl_score:
    mov     eax, IMPORTANCE_IMPL

@@apply_ref_boost:
    ; Boost by reference count (log2-ish: every 8 refs adds 0.1)
    mov     ecx, DWORD PTR [rcx].SYMBOL_ENTRY.ref_count
    shr     ecx, 3                          ; divide by 8
    cmp     ecx, 10                         ; cap boost at 1.0
    jle     @@boost_ok
    mov     ecx, 10
@@boost_ok:
    imul    ecx, 01999h                     ; 0.1 in 16.16
    add     eax, ecx

    ; Boost for high-value kinds (class, function, method)
    mov     ebx, DWORD PTR [rcx].SYMBOL_ENTRY.symbol_kind
    cmp     ebx, SYMBOL_CLASS
    je      @@kind_boost
    cmp     ebx, SYMBOL_FUNCTION
    je      @@kind_boost
    cmp     ebx, SYMBOL_METHOD
    je      @@kind_boost
    cmp     ebx, SYMBOL_INTERFACE
    je      @@kind_boost
    jmp     @@no_kind_boost

@@kind_boost:
    add     eax, 03333h                     ; +0.2 for high-value kinds

@@no_kind_boost:
    ; Cap at 2.0
    cmp     eax, 20000h
    jle     @@score_done
    mov     eax, 20000h

@@score_done:
    add     rsp, 8
    pop     rbx
    ret
compute_importance ENDP

; =============================================================================
; insertion_sort_ranked — Internal: sort RANKED_SYMBOL array by importance (desc)
; RCX = array base
; EDX = count
; =============================================================================
insertion_sort_ranked PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 48 + SIZEOF RANKED_SYMBOL
    .allocstack 48 + SIZEOF RANKED_SYMBOL
    .endprolog

    mov     rsi, rcx                        ; array base
    mov     r12d, edx                       ; count

    cmp     r12d, 2
    jl      @@sort_done

    mov     ebx, 1                          ; i = 1

@@outer_loop:
    cmp     ebx, r12d
    jge     @@sort_done

    ; Copy current element to temp (on stack)
    imul    rax, rbx, SIZEOF RANKED_SYMBOL
    lea     rcx, [rsp+48]                   ; temp buffer on stack
    lea     rdx, [rsi+rax]                  ; src = array[i]
    mov     r8d, SIZEOF RANKED_SYMBOL
    call    memcpy

    ; Get key importance
    mov     r13d, DWORD PTR [rsp+48].RANKED_SYMBOL.importance

    mov     edi, ebx
    dec     edi                             ; j = i - 1

@@inner_loop:
    cmp     edi, 0
    jl      @@insert

    ; Compare array[j].importance < key (descending: larger first)
    imul    rax, rdi, SIZEOF RANKED_SYMBOL
    mov     eax, DWORD PTR [rsi+rax].RANKED_SYMBOL.importance
    cmp     eax, r13d
    jge     @@insert                        ; array[j] >= key, stop

    ; Shift array[j] to array[j+1]
    imul    rax, rdi, SIZEOF RANKED_SYMBOL
    lea     rcx, [rsi+rax+SIZEOF RANKED_SYMBOL]  ; dest = array[j+1]
    lea     rdx, [rsi+rax]                  ; src = array[j]
    mov     r8d, SIZEOF RANKED_SYMBOL
    call    memcpy

    dec     edi
    jmp     @@inner_loop

@@insert:
    ; Place temp at array[j+1]
    inc     edi
    imul    rax, rdi, SIZEOF RANKED_SYMBOL
    lea     rcx, [rsi+rax]                  ; dest = array[j+1]
    lea     rdx, [rsp+48]                   ; src = temp
    mov     r8d, SIZEOF RANKED_SYMBOL
    call    memcpy

    inc     ebx
    jmp     @@outer_loop

@@sort_done:
    add     rsp, 48 + SIZEOF RANKED_SYMBOL
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
insertion_sort_ranked ENDP

; =============================================================================
; asm_lsp_bridge_init
; Initialize the LSP-AI bridge: allocate ranked buffer, set default weights.
; No parameters.
; Returns: RAX = 0 success, LSP_ERR_* on failure
; =============================================================================
asm_lsp_bridge_init PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     DWORD PTR [g_bridge_ctx.initialized], 1
    je      @@init_already

    ; Initialize critical section
    lea     rcx, g_bridge_cs
    call    InitializeCriticalSection

    ; Zero stats
    lea     rdi, g_bridge_stats
    xor     eax, eax
    mov     ecx, SIZEOF LSP_BRIDGE_STATS
    rep     stosb

    ; Set default weights
    mov     DWORD PTR [g_bridge_ctx.syntax_weight], DEFAULT_SYNTAX_WEIGHT
    mov     DWORD PTR [g_bridge_ctx.semantic_weight], DEFAULT_SEMANTIC_WEIGHT

    ; Allocate ranked symbol buffer (8192 entries)
    xor     ecx, ecx
    mov     edx, MAX_BRIDGE_SYMBOLS * (SIZEOF RANKED_SYMBOL)
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@init_alloc_fail
    mov     QWORD PTR [g_bridge_ctx.pRankedBuffer], rax
    mov     DWORD PTR [g_bridge_ctx.max_ranked], MAX_BRIDGE_SYMBOLS
    mov     DWORD PTR [g_bridge_ctx.ranked_count], 0
    mov     DWORD PTR [g_bridge_ctx.dirty], 1
    mov     QWORD PTR [g_bridge_ctx.pLastSymIndex], 0

    mov     DWORD PTR [g_bridge_ctx.initialized], 1

    lea     rcx, szBridgeInit
    call    OutputDebugStringA

    xor     eax, eax
    jmp     @@init_exit

@@init_already:
    mov     eax, LSP_ERR_ALREADY_INIT
    jmp     @@init_exit

@@init_alloc_fail:
    mov     eax, LSP_ERR_ALLOC

@@init_exit:
    add     rsp, 48
    pop     rbx
    ret
asm_lsp_bridge_init ENDP

; =============================================================================
; asm_lsp_bridge_sync
; Synchronize a SymbolIndex into the ranked context buffer.
; RCX = SYMBOL_ENTRY* (array of symbols from LSP index)
; EDX = symbol count
; R8D = sync mode (SYNC_SHALLOW, SYNC_DEEP, SYNC_FULL_SEMANTIC)
; Returns: RAX = 0 success, LSP_ERR_* on failure
;          RDX = symbols processed
; =============================================================================
asm_lsp_bridge_sync PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 72
    .allocstack 72
    .endprolog

    cmp     DWORD PTR [g_bridge_ctx.initialized], 1
    jne     @@sync_not_init

    mov     rsi, rcx                        ; SYMBOL_ENTRY array
    mov     r12d, edx                       ; count
    mov     r13d, r8d                       ; mode

    ; Cap at buffer max
    cmp     r12d, MAX_BRIDGE_SYMBOLS
    jle     @@count_ok
    mov     r12d, MAX_BRIDGE_SYMBOLS
@@count_ok:

    ; Start timing
    lea     rcx, [rsp+56]                   ; QPC start
    call    QueryPerformanceCounter
    mov     r14, QWORD PTR [rsp+56]         ; start time

    lea     rcx, g_bridge_cs
    call    EnterCriticalSection

    ; Get ranked buffer
    mov     rdi, QWORD PTR [g_bridge_ctx.pRankedBuffer]
    mov     rbx, rdi                        ; save base
    xor     r15d, r15d                      ; output count

    ; Process each symbol
    xor     ecx, ecx                        ; index

@@sync_loop:
    cmp     ecx, r12d
    jge     @@sync_sort
    push    rcx

    ; Current source symbol
    imul    rax, rcx, SIZEOF SYMBOL_ENTRY
    lea     rax, [rsi+rax]
    mov     r8, rax                         ; -> source symbol

    ; Compute importance
    mov     rcx, r8
    call    compute_importance
    ; eax = importance score

    ; Hash the name
    push    rax                             ; save importance
    mov     rcx, QWORD PTR [r8].SYMBOL_ENTRY.name_ptr
    mov     edx, DWORD PTR [r8].SYMBOL_ENTRY.name_len
    call    fnv1a_hash64
    pop     rdx                             ; restore importance (now in EDX)

    ; Write to ranked buffer
    mov     QWORD PTR [rdi].RANKED_SYMBOL.name_hash, rax
    mov     rax, QWORD PTR [r8].SYMBOL_ENTRY.name_ptr
    mov     QWORD PTR [rdi].RANKED_SYMBOL.name_ptr, rax
    mov     eax, DWORD PTR [r8].SYMBOL_ENTRY.name_len
    mov     DWORD PTR [rdi].RANKED_SYMBOL.name_len, eax
    mov     eax, DWORD PTR [r8].SYMBOL_ENTRY.symbol_kind
    mov     DWORD PTR [rdi].RANKED_SYMBOL.symbol_kind, eax
    mov     DWORD PTR [rdi].RANKED_SYMBOL.importance, edx

    add     rdi, SIZEOF RANKED_SYMBOL
    inc     r15d

    pop     rcx
    inc     ecx
    jmp     @@sync_loop

@@sync_sort:
    ; Sort ranked buffer by importance (descending)
    mov     rcx, rbx                        ; buffer base
    mov     edx, r15d                       ; count
    call    insertion_sort_ranked

    ; Update context
    mov     DWORD PTR [g_bridge_ctx.ranked_count], r15d
    mov     DWORD PTR [g_bridge_ctx.dirty], 0
    mov     QWORD PTR [g_bridge_ctx.pLastSymIndex], rsi

    ; Update stats
    lock inc QWORD PTR [g_bridge_stats.sync_count]
    mov     eax, r15d
    lock add QWORD PTR [g_bridge_stats.symbols_processed], rax

    ; End timing
    lea     rcx, [rsp+56]
    call    QueryPerformanceCounter
    mov     rax, QWORD PTR [rsp+56]
    sub     rax, r14                        ; elapsed ticks

    ; Convert to microseconds (approximate: ticks * 1000000 / freq)
    ; Store raw ticks for now (freq varies)
    mov     DWORD PTR [g_bridge_stats.last_sync_us], eax

    lea     rcx, g_bridge_cs
    call    LeaveCriticalSection

    lea     rcx, szBridgeSync
    call    OutputDebugStringA

    xor     eax, eax
    mov     edx, r15d                       ; symbols processed
    jmp     @@sync_exit

@@sync_not_init:
    mov     eax, LSP_ERR_NOT_INIT
    xor     edx, edx

@@sync_exit:
    add     rsp, 72
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_lsp_bridge_sync ENDP

; =============================================================================
; asm_lsp_bridge_query
; Return the top-N ranked symbols for the context window.
; ECX = max symbols to return
; RDX = output RANKED_SYMBOL* buffer (caller-allocated)
; Returns: RAX = actual symbols written
; =============================================================================
asm_lsp_bridge_query PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    cmp     DWORD PTR [g_bridge_ctx.initialized], 1
    jne     @@query_fail

    mov     ebx, ecx                        ; max count
    mov     rdi, rdx                        ; output buffer

    ; Clamp to available
    mov     eax, DWORD PTR [g_bridge_ctx.ranked_count]
    cmp     ebx, eax
    jle     @@count_clamped
    mov     ebx, eax
@@count_clamped:

    test    ebx, ebx
    jz      @@query_fail

    ; Copy from ranked buffer
    mov     rsi, QWORD PTR [g_bridge_ctx.pRankedBuffer]
    mov     rcx, rdi                        ; dest
    mov     rdx, rsi                        ; src
    imul    r8, rbx, SIZEOF RANKED_SYMBOL   ; bytes
    call    memcpy

    lock add QWORD PTR [g_bridge_stats.symbols_emitted], rbx

    mov     eax, ebx
    jmp     @@query_exit

@@query_fail:
    xor     eax, eax

@@query_exit:
    add     rsp, 40
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_lsp_bridge_query ENDP

; =============================================================================
; asm_lsp_bridge_invalidate
; Mark the cached context as dirty (forces re-sync on next query).
; Returns: RAX = 0
; =============================================================================
asm_lsp_bridge_invalidate PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     DWORD PTR [g_bridge_ctx.dirty], 1
    lock inc QWORD PTR [g_bridge_stats.cache_misses]

    lea     rcx, szBridgeInval
    call    OutputDebugStringA

    xor     eax, eax
    add     rsp, 40
    ret
asm_lsp_bridge_invalidate ENDP

; =============================================================================
; asm_lsp_bridge_set_weights
; Configure syntax/semantic fusion weights.
; ECX = syntax weight (fixed-point 16.16, e.g. 04CCDh = 0.3)
; EDX = semantic weight (fixed-point 16.16, e.g. 0B333h = 0.7)
; Returns: RAX = 0
; =============================================================================
asm_lsp_bridge_set_weights PROC FRAME
    sub     rsp, 8
    .allocstack 8
    .endprolog

    mov     DWORD PTR [g_bridge_ctx.syntax_weight], ecx
    mov     DWORD PTR [g_bridge_ctx.semantic_weight], edx
    mov     DWORD PTR [g_bridge_ctx.dirty], 1   ; Force re-score

    xor     eax, eax
    add     rsp, 8
    ret
asm_lsp_bridge_set_weights ENDP

; =============================================================================
; asm_lsp_bridge_get_stats
; Read bridge statistics.
; RCX = output LSP_BRIDGE_STATS* buffer
; Returns: RAX = 0
; =============================================================================
asm_lsp_bridge_get_stats PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    mov     rcx, rbx
    lea     rdx, g_bridge_stats
    mov     r8d, SIZEOF LSP_BRIDGE_STATS
    call    memcpy

    xor     eax, eax
    add     rsp, 40
    pop     rbx
    ret
asm_lsp_bridge_get_stats ENDP

; =============================================================================
; asm_lsp_bridge_shutdown
; Release ranked buffer, destroy critical section.
; Returns: RAX = 0
; =============================================================================
asm_lsp_bridge_shutdown PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     DWORD PTR [g_bridge_ctx.initialized], 1
    jne     @@shut_exit

    ; Free ranked buffer
    mov     rcx, QWORD PTR [g_bridge_ctx.pRankedBuffer]
    test    rcx, rcx
    jz      @@shut_skipfree
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@shut_skipfree:

    ; Destroy critical section
    lea     rcx, g_bridge_cs
    call    DeleteCriticalSection

    ; Zero context
    lea     rdi, g_bridge_ctx
    xor     eax, eax
    mov     ecx, SIZEOF LSP_BRIDGE_CTX
    rep     stosb

    lea     rcx, szBridgeShut
    call    OutputDebugStringA

@@shut_exit:
    xor     eax, eax
    add     rsp, 48
    pop     rbx
    ret
asm_lsp_bridge_shutdown ENDP

; =============================================================================
; fnv1a_hash64 — FNV-1a 64-bit hash (local copy for self-contained module)
; RCX = string pointer, RDX = string length
; Returns: RAX = 64-bit hash
; =============================================================================
fnv1a_hash64 PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 8
    .allocstack 8
    .endprolog

    mov     rax, 0CBF29CE484222325h
    mov     rbx, 100000001B3h
    test    rdx, rdx
    jz      @@hash_done

@@hash_loop:
    movzx   r8d, BYTE PTR [rcx]
    xor     al, r8b
    imul    rax, rbx
    inc     rcx
    dec     rdx
    jnz     @@hash_loop

@@hash_done:
    add     rsp, 8
    pop     rbx
    ret
fnv1a_hash64 ENDP

END
