;==============================================================================
; asm_globals.asm - One definition, exported, no trampolines
; Global symbol definitions for cross-module linking
;
; This file provides THE ONLY definitions for runtime globals.
; All other modules should use EXTERN to reference these symbols.
;
; Assemble: ml64 /c /Fo asm_globals.obj asm_globals.asm
;==============================================================================

OPTION CASEMAP:NONE

;==============================================================================
;                              DATA SECTION
;==============================================================================
.DATA

;------------------------------------------------------------------------------
; Core Runtime Globals
; These are the canonical definitions - no other module should define them
;------------------------------------------------------------------------------
PUBLIC g_hHeap
PUBLIC g_hInstance
PUBLIC g_hMainWindow
PUBLIC g_dwMainThreadId

ALIGN 8
g_hHeap             QWORD 0         ; Process heap handle (from GetProcessHeap)
g_hInstance         QWORD 0         ; Module instance handle (from WinMain)
g_hMainWindow       QWORD 0         ; Main window handle
g_dwMainThreadId    DWORD 0         ; Main thread ID

;------------------------------------------------------------------------------
; Beacon Function Pointers
; These are hot-patchable at runtime via LoadLibrary/GetProcAddress
;------------------------------------------------------------------------------
PUBLIC BeaconRecv
PUBLIC TryBeaconRecv
PUBLIC RunInference
PUBLIC RegisterAgent
PUBLIC HotSwapModel
PUBLIC BeaconSend
PUBLIC BeaconPing

ALIGN 8
BeaconRecv          QWORD 0         ; Beacon receive callback
TryBeaconRecv       QWORD 0         ; Non-blocking beacon receive
RunInference        QWORD 0         ; ML inference entry point
RegisterAgent       QWORD 0         ; Agent registration callback
HotSwapModel        QWORD 0         ; Model hot-swap handler
BeaconSend          QWORD 0         ; Beacon send callback
BeaconPing          QWORD 0         ; Beacon keepalive

;------------------------------------------------------------------------------
; Configuration Globals
;------------------------------------------------------------------------------
PUBLIC g_bDebugMode
PUBLIC g_bVerbose
PUBLIC g_dwLogLevel

ALIGN 4
g_bDebugMode        BYTE 0          ; Debug mode flag
g_bVerbose          BYTE 0          ; Verbose output flag
                    WORD 0          ; Padding
g_dwLogLevel        DWORD 0         ; Log level (0=none, 1=error, 2=warn, 3=info, 4=debug)

;------------------------------------------------------------------------------
; Inference Engine State
;------------------------------------------------------------------------------
PUBLIC g_pModelContext
PUBLIC g_pTokenizer
PUBLIC g_pSampler
PUBLIC g_nContextSize
PUBLIC g_nBatchSize

ALIGN 8
g_pModelContext     QWORD 0         ; Active model context
g_pTokenizer        QWORD 0         ; Tokenizer instance
g_pSampler          QWORD 0         ; Sampler instance
g_nContextSize      DWORD 4096      ; Context window size
g_nBatchSize        DWORD 512       ; Batch size

;------------------------------------------------------------------------------
; Memory Statistics
;------------------------------------------------------------------------------
PUBLIC g_MemStats
g_MemStats LABEL QWORD
    mem_allocated   QWORD 0         ; Total bytes allocated
    mem_freed       QWORD 0         ; Total bytes freed
    mem_peak        QWORD 0         ; Peak allocation
    mem_alloc_count QWORD 0         ; Allocation count
    mem_free_count  QWORD 0         ; Free count

;==============================================================================
;                              CODE SECTION
;==============================================================================
.CODE

;------------------------------------------------------------------------------
; RawrXD_GlobalsInit - Initialize globals at startup
; RCX = hInstance
; RDX = hHeap (or 0 to call GetProcessHeap)
; Returns: RAX = 1 on success, 0 on failure
;------------------------------------------------------------------------------
PUBLIC RawrXD_GlobalsInit
RawrXD_GlobalsInit PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; Store hInstance
    mov [g_hInstance], rcx
    
    ; Store or get heap handle
    test rdx, rdx
    jnz @have_heap
    
    ; Get process heap
    EXTERN GetProcessHeap:PROC
    call GetProcessHeap
    mov rdx, rax
    
@have_heap:
    mov [g_hHeap], rdx
    
    ; Get main thread ID
    EXTERN GetCurrentThreadId:PROC
    call GetCurrentThreadId
    mov [g_dwMainThreadId], eax
    
    ; Success
    mov eax, 1
    
    add rsp, 32
    pop rbp
    ret
RawrXD_GlobalsInit ENDP

;------------------------------------------------------------------------------
; RawrXD_SetBeaconHandler - Set a beacon callback
; RCX = Handler index (0=Recv, 1=TryRecv, 2=Inference, 3=Register, 4=HotSwap)
; RDX = Handler function pointer
; Returns: RAX = previous handler
;------------------------------------------------------------------------------
PUBLIC RawrXD_SetBeaconHandler
RawrXD_SetBeaconHandler PROC FRAME
    .endprolog
    
    cmp rcx, 6
    ja @invalid_handler
    
    lea rax, BeaconRecv
    shl rcx, 3              ; index * 8
    add rax, rcx
    
    mov rcx, [rax]          ; Get old value
    mov [rax], rdx          ; Set new value
    mov rax, rcx            ; Return old
    ret
    
@invalid_handler:
    xor eax, eax
    ret
RawrXD_SetBeaconHandler ENDP

;------------------------------------------------------------------------------
; RawrXD_GetGlobalPtr - Get pointer to a named global
; RCX = Global ID (see equates below)
; Returns: RAX = pointer to global, or 0 if invalid
;------------------------------------------------------------------------------
GLOBAL_ID_HEAP          EQU 0
GLOBAL_ID_INSTANCE      EQU 1
GLOBAL_ID_MAINWND       EQU 2
GLOBAL_ID_THREADID      EQU 3
GLOBAL_ID_MODEL_CTX     EQU 4
GLOBAL_ID_TOKENIZER     EQU 5
GLOBAL_ID_SAMPLER       EQU 6
GLOBAL_ID_MEMSTATS      EQU 7

PUBLIC RawrXD_GetGlobalPtr
RawrXD_GetGlobalPtr PROC FRAME
    .endprolog
    
    cmp rcx, GLOBAL_ID_MEMSTATS
    ja @invalid_global
    
    ; Jump table
    lea rax, @jump_table
    jmp QWORD PTR [rax + rcx * 8]
    
    ALIGN 8
@jump_table:
    QWORD OFFSET @ret_heap
    QWORD OFFSET @ret_instance
    QWORD OFFSET @ret_mainwnd
    QWORD OFFSET @ret_threadid
    QWORD OFFSET @ret_modelctx
    QWORD OFFSET @ret_tokenizer
    QWORD OFFSET @ret_sampler
    QWORD OFFSET @ret_memstats
    
@ret_heap:
    lea rax, g_hHeap
    ret
@ret_instance:
    lea rax, g_hInstance
    ret
@ret_mainwnd:
    lea rax, g_hMainWindow
    ret
@ret_threadid:
    lea rax, g_dwMainThreadId
    ret
@ret_modelctx:
    lea rax, g_pModelContext
    ret
@ret_tokenizer:
    lea rax, g_pTokenizer
    ret
@ret_sampler:
    lea rax, g_pSampler
    ret
@ret_memstats:
    lea rax, g_MemStats
    ret
    
@invalid_global:
    xor eax, eax
    ret
RawrXD_GetGlobalPtr ENDP

;------------------------------------------------------------------------------
; RawrXD_TrackAlloc - Track memory allocation (for diagnostics)
; RCX = bytes allocated
;------------------------------------------------------------------------------
PUBLIC RawrXD_TrackAlloc
RawrXD_TrackAlloc PROC FRAME
    .endprolog
    
    lock add [mem_allocated], rcx
    lock inc QWORD PTR [mem_alloc_count]
    
    ; Update peak if needed
    mov rax, [mem_allocated]
    sub rax, [mem_freed]
    cmp rax, [mem_peak]
    jbe @no_peak_update
    mov [mem_peak], rax
    
@no_peak_update:
    ret
RawrXD_TrackAlloc ENDP

;------------------------------------------------------------------------------
; RawrXD_TrackFree - Track memory free (for diagnostics)
; RCX = bytes freed
;------------------------------------------------------------------------------
PUBLIC RawrXD_TrackFree
RawrXD_TrackFree PROC FRAME
    .endprolog
    
    lock add [mem_freed], rcx
    lock inc QWORD PTR [mem_free_count]
    ret
RawrXD_TrackFree ENDP

;------------------------------------------------------------------------------
; RawrXD_GetMemStats - Get memory statistics
; RCX = pointer to output structure (5 QWORDs)
; Returns: RAX = current allocation (allocated - freed)
;------------------------------------------------------------------------------
PUBLIC RawrXD_GetMemStats
RawrXD_GetMemStats PROC FRAME
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    .endprolog
    
    test rcx, rcx
    jz @calc_only
    
    ; Copy stats to output buffer
    mov rdi, rcx
    lea rsi, g_MemStats
    mov ecx, 5
    rep movsq
    
@calc_only:
    ; Return current allocation
    mov rax, [mem_allocated]
    sub rax, [mem_freed]
    
    pop rsi
    pop rdi
    ret
RawrXD_GetMemStats ENDP

;------------------------------------------------------------------------------
; Default beacon stub - returns 0/null for uninitialized handlers
;------------------------------------------------------------------------------
PUBLIC __beacon_stub_null
__beacon_stub_null PROC
    xor eax, eax
    ret
__beacon_stub_null ENDP

END
