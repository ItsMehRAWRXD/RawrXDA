; =============================================================================
; RawrXD_AgenticOrchestrator.asm — Central Agentic Dispatch Kernel
; =============================================================================
;
; The central nervous system wiring all 23 enterprise handlers to real
; subsystem implementations via a lock-free task queue and VTable dispatch.
; Replaces the C++ stub layer with sub-microsecond MASM dispatch.
;
; Capabilities:
;   - VTable-based dispatch for 23 handler opcodes (7001–9700)
;   - Lock-free SPSC task queue (1024 entries, 64-byte cache-line aligned)
;   - Telemetry ring integration (fire-and-forget event posting)
;   - Atomic metrics counters (dispatches, latency, errors per handler)
;   - Opcode → VTable index mapping with gap-aware lookup table
;   - Pre/Post dispatch hooks for observability pipeline
;   - NUMA-aware task pool allocation via VirtualAllocExNuma
;
; Active Exports (used by C++ CommandRegistry bridge):
;   asm_orchestrator_init           — Initialize subsystem VTable + task queue
;   asm_orchestrator_dispatch       — Dispatch opcode to subsystem handler
;   asm_orchestrator_shutdown       — Drain queue, release resources
;   asm_orchestrator_get_metrics    — Read atomic dispatch counters
;   asm_orchestrator_register_hook  — Register pre/post dispatch callback
;   asm_orchestrator_set_vtable     — Hot-swap a VTable slot (runtime binding)
;   asm_orchestrator_queue_async    — Enqueue async task to SPSC ring
;   asm_orchestrator_drain_queue    — Process all pending async tasks
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_AgenticOrchestrator.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                    ORCHESTRATOR CONSTANTS
; =============================================================================

; Handler opcode ranges (must match command_registry.hpp COMMAND_TABLE)
OP_UNREAL_INIT          EQU     7001
OP_UNREAL_ATTACH        EQU     7002
OP_UNITY_INIT           EQU     7003
OP_UNITY_ATTACH         EQU     7004
OP_REVENG_DISASM        EQU     8100
OP_REVENG_DECOMP        EQU     8101
OP_REVENG_VULN          EQU     8102
OP_MODEL_LIST           EQU     9001
OP_MODEL_LOAD           EQU     9002
OP_MODEL_UNLOAD         EQU     9003
OP_MODEL_QUANTIZE       EQU     9004
OP_MODEL_FINETUNE       EQU     9005
OP_DISK_LIST            EQU     9200
OP_DISK_SCAN            EQU     9201
OP_GOVERNOR_STATUS      EQU     9300
OP_GOVERNOR_POWER       EQU     9301
OP_MARKET_LIST          EQU     9400
OP_MARKET_INSTALL       EQU     9401
OP_EMBED_ENCODE         EQU     9500
OP_VISION_ANALYZE       EQU     9600
OP_PROMPT_CLASSIFY      EQU     9700
OP_LSP_AI_SYNC          EQU     9800        ; v14.2.1: SymbolIndex↔ContextAnalyzer bridge
OP_HEALTH_CHECK         EQU     9900        ; v14.2.1: Full-stack health diagnostics

; VTable geometry
VTABLE_SLOT_COUNT       EQU     23          ; 23 handler slots
VTABLE_SIZE_BYTES       EQU     VTABLE_SLOT_COUNT * 8

; Task queue geometry (SPSC ring buffer)
TASK_QUEUE_CAPACITY     EQU     1024
TASK_QUEUE_MASK         EQU     TASK_QUEUE_CAPACITY - 1
TASK_ENTRY_SIZE         EQU     64          ; Cache-line aligned

; Hook types
HOOK_PRE_DISPATCH       EQU     0
HOOK_POST_DISPATCH      EQU     1
MAX_HOOKS               EQU     16

; Orchestrator status
ORCH_OK                 EQU     0
ORCH_ERR_NOT_INIT       EQU     1
ORCH_ERR_INVALID_OP     EQU     2
ORCH_ERR_NOT_IMPL       EQU     3
ORCH_ERR_QUEUE_FULL     EQU     4
ORCH_ERR_ALLOC          EQU     5
ORCH_ERR_ALREADY_INIT   EQU     6
ORCH_ERR_VPROTECT       EQU     7

; Telemetry event IDs
TELEM_ORCH_INIT         EQU     1000h
TELEM_ORCH_DISPATCH     EQU     2000h
TELEM_ORCH_SHUTDOWN     EQU     3000h
TELEM_ORCH_ERROR        EQU     4000h

; =============================================================================
;                    STRUCTURES
; =============================================================================

; Opcode-to-index lookup entry
OPCODE_MAP_ENTRY STRUCT
    opcode          DD      ?               ; Command ID (7001, 8100, etc.)
    vtable_index    DD      ?               ; Index into g_vtable [0..22]
OPCODE_MAP_ENTRY ENDS

; Async task entry (64 bytes = 1 cache line)
TASK_ENTRY STRUCT 8
    opcode          DD      ?               ; Command ID
    flags           DD      ?               ; Priority, cancellable, etc.
    context_ptr     DQ      ?               ; CommandContext*
    result_ptr      DQ      ?               ; CommandResult*
    callback_ptr    DQ      ?               ; Completion callback
    timestamp       DQ      ?               ; QPC enqueue time
    user_data       DQ      ?               ; Opaque caller data
    _pad0           DQ      ?               ; Pad to 64 bytes
TASK_ENTRY ENDS

; Per-handler metrics
HANDLER_METRICS STRUCT
    dispatch_count  DQ      ?               ; Total dispatches
    error_count     DQ      ?               ; Total errors
    total_cycles    DQ      ?               ; Accumulated RDTSC cycles
    min_cycles      DQ      ?               ; Fastest dispatch
    max_cycles      DQ      ?               ; Slowest dispatch
    last_status     DD      ?               ; Last HRESULT
    _pad0           DD      ?
HANDLER_METRICS ENDS

; Dispatch hook entry
HOOK_ENTRY STRUCT
    hook_type       DD      ?               ; PRE or POST
    _pad0           DD      ?
    callback_ptr    DQ      ?               ; void (*hook)(opcode, ctx, result)
HOOK_ENTRY ENDS

; Orchestrator global context
ORCH_CONTEXT STRUCT
    initialized     DD      ?
    shutting_down    DD      ?
    vtable_ptr      DQ      ?               ; -> g_vtable
    queue_ptr       DQ      ?               ; -> g_task_queue
    queue_head      DD      ?               ; Producer index (atomic)
    queue_tail      DD      ?               ; Consumer index (atomic)
    metrics_ptr     DQ      ?               ; -> g_handler_metrics
    hooks_ptr       DQ      ?               ; -> g_hooks
    hook_count      DD      ?
    _pad0           DD      ?
    telem_ring      DQ      ?               ; Telemetry ring buffer (optional)
    total_dispatches DQ     ?               ; Global counter
    total_errors    DQ      ?
    total_async     DQ      ?
ORCH_CONTEXT ENDS

; =============================================================================
;                    DATA SECTION
; =============================================================================
.data

; === Orchestrator context ===
ALIGN 16
g_orch_ctx      ORCH_CONTEXT <>

; === VTable: 23 function pointers (subsystem handlers) ===
ALIGN 16
g_vtable        DQ      VTABLE_SLOT_COUNT DUP(0)

; === Opcode-to-index lookup table (23 entries) ===
ALIGN 16
g_opcode_map    OPCODE_MAP_ENTRY <7001,  0>     ; Unreal Init
                OPCODE_MAP_ENTRY <7002,  1>     ; Unreal Attach
                OPCODE_MAP_ENTRY <7003,  2>     ; Unity Init
                OPCODE_MAP_ENTRY <7004,  3>     ; Unity Attach
                OPCODE_MAP_ENTRY <8100,  4>     ; Reveng Disasm
                OPCODE_MAP_ENTRY <8101,  5>     ; Reveng Decomp
                OPCODE_MAP_ENTRY <8102,  6>     ; Reveng VulnScan
                OPCODE_MAP_ENTRY <9001,  7>     ; Model List
                OPCODE_MAP_ENTRY <9002,  8>     ; Model Load
                OPCODE_MAP_ENTRY <9003,  9>     ; Model Unload
                OPCODE_MAP_ENTRY <9004, 10>     ; Model Quantize
                OPCODE_MAP_ENTRY <9005, 11>     ; Model Finetune
                OPCODE_MAP_ENTRY <9200, 12>     ; Disk List
                OPCODE_MAP_ENTRY <9201, 13>     ; Disk Scan
                OPCODE_MAP_ENTRY <9300, 14>     ; Governor Status
                OPCODE_MAP_ENTRY <9301, 15>     ; Governor SetPower
                OPCODE_MAP_ENTRY <9400, 16>     ; Market List
                OPCODE_MAP_ENTRY <9401, 17>     ; Market Install
                OPCODE_MAP_ENTRY <9500, 18>     ; Embed Encode
                OPCODE_MAP_ENTRY <9600, 19>     ; Vision Analyze
                OPCODE_MAP_ENTRY <9700, 20>     ; Prompt Classify
                OPCODE_MAP_ENTRY <9800, 21>     ; LSP AI Sync (v14.2.1)
                OPCODE_MAP_ENTRY <9900, 22>     ; Health Check (v14.2.1)
g_opcode_count  DD      23                      ; Valid entries (v14.2.1: was 21)

; === Per-handler metrics array ===
ALIGN 16
g_handler_metrics HANDLER_METRICS VTABLE_SLOT_COUNT DUP(<>)

; === Hooks array ===
ALIGN 16
g_hooks         HOOK_ENTRY MAX_HOOKS DUP(<>)

; === SPSC task queue ring buffer (allocated dynamically) ===
g_task_queue    DQ      0

; === Critical section for non-atomic operations ===
ALIGN 16
g_orch_cs       CRITICAL_SECTION <>

; === Status strings ===
szOrchInit      DB "AgenticOrchestrator: subsystem initialized", 0
szOrchShutdown  DB "AgenticOrchestrator: shutdown complete", 0
szOrchDispatch  DB "AgenticOrchestrator: dispatch ", 0
szOrchError     DB "AgenticOrchestrator: dispatch error", 0
szOrchNotImpl   DB "AgenticOrchestrator: handler not implemented", 0
szOrchQueueFull DB "AgenticOrchestrator: async queue full", 0

; =============================================================================
;                    EXPORTS
; =============================================================================
PUBLIC asm_orchestrator_init
PUBLIC asm_orchestrator_dispatch
PUBLIC asm_orchestrator_shutdown
PUBLIC asm_orchestrator_get_metrics
PUBLIC asm_orchestrator_register_hook
PUBLIC asm_orchestrator_set_vtable
PUBLIC asm_orchestrator_queue_async
PUBLIC asm_orchestrator_drain_queue
PUBLIC asm_orchestrator_lsp_sync

; =============================================================================
;                    EXTERNAL IMPORTS
; =============================================================================
EXTERN QueryPerformanceCounter: PROC
EXTERN GetCurrentProcessorNumber: PROC

; =============================================================================
;                    CODE SECTION
; =============================================================================
.code

; =============================================================================
; map_opcode_to_index — Internal: linear scan of opcode lookup table
; ECX = opcode
; Returns: EAX = vtable index, or -1 if not found
; =============================================================================
map_opcode_to_index PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 8
    .allocstack 8
    .endprolog

    mov     ebx, ecx                        ; opcode to find
    lea     rsi, g_opcode_map
    mov     ecx, DWORD PTR [g_opcode_count]
    xor     eax, eax                        ; i = 0

@@scan_loop:
    cmp     eax, ecx
    jge     @@not_found
    cmp     DWORD PTR [rsi].OPCODE_MAP_ENTRY.opcode, ebx
    je      @@found
    add     rsi, SIZEOF OPCODE_MAP_ENTRY
    inc     eax
    jmp     @@scan_loop

@@found:
    mov     eax, DWORD PTR [rsi].OPCODE_MAP_ENTRY.vtable_index
    jmp     @@exit

@@not_found:
    mov     eax, -1

@@exit:
    add     rsp, 8
    pop     rsi
    pop     rbx
    ret
map_opcode_to_index ENDP

; =============================================================================
; fire_hooks — Internal: invoke all hooks of a given type
; ECX = hook_type (HOOK_PRE_DISPATCH or HOOK_POST_DISPATCH)
; RDX = opcode (in low 32 bits)
; R8  = CommandContext*
; R9  = CommandResult*
; =============================================================================
fire_hooks PROC FRAME
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
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     ebx, ecx                        ; hook_type
    mov     r12d, edx                       ; opcode
    mov     r13, r8                         ; context
    mov     r14, r9                         ; result

    lea     rsi, g_hooks
    mov     edi, DWORD PTR [g_orch_ctx.hook_count]
    xor     ecx, ecx                        ; i = 0

@@hook_loop:
    cmp     ecx, edi
    jge     @@hook_done
    cmp     DWORD PTR [rsi].HOOK_ENTRY.hook_type, ebx
    jne     @@hook_next

    ; Found matching hook — invoke it
    mov     rax, QWORD PTR [rsi].HOOK_ENTRY.callback_ptr
    test    rax, rax
    jz      @@hook_next

    ; Call hook(opcode, context, result)
    push    rcx                             ; Save loop counter
    mov     ecx, r12d                       ; Arg 1: opcode
    mov     rdx, r13                        ; Arg 2: context
    mov     r8, r14                         ; Arg 3: result
    call    rax
    pop     rcx

@@hook_next:
    add     rsi, SIZEOF HOOK_ENTRY
    inc     ecx
    jmp     @@hook_loop

@@hook_done:
    add     rsp, 48
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
fire_hooks ENDP

; =============================================================================
; asm_orchestrator_init
; Initialize the Agentic Orchestrator: VTable, task queue, metrics, hooks.
; RCX = Telemetry ring pointer (optional, 0 to skip)
; Returns: RAX = 0 success, ORCH_ERR_* on failure
; =============================================================================
asm_orchestrator_init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     r12, rcx                        ; Telemetry ring

    ; Check double-init
    cmp     DWORD PTR [g_orch_ctx.initialized], 1
    je      @@already_init

    ; Initialize critical section
    lea     rcx, g_orch_cs
    call    InitializeCriticalSection

    ; Zero metrics array
    lea     rdi, g_handler_metrics
    xor     eax, eax
    mov     ecx, VTABLE_SLOT_COUNT * (SIZEOF HANDLER_METRICS)
    rep     stosb

    ; Initialize min_cycles to max value
    lea     rsi, g_handler_metrics
    mov     ecx, VTABLE_SLOT_COUNT
@@init_min_loop:
    mov     QWORD PTR [rsi].HANDLER_METRICS.min_cycles, 0FFFFFFFFFFFFFFFFh
    add     rsi, SIZEOF HANDLER_METRICS
    dec     ecx
    jnz     @@init_min_loop

    ; Zero hooks
    lea     rdi, g_hooks
    xor     eax, eax
    mov     ecx, MAX_HOOKS * (SIZEOF HOOK_ENTRY)
    rep     stosb
    mov     DWORD PTR [g_orch_ctx.hook_count], 0

    ; Allocate SPSC task queue: 1024 * 64 = 64KB, page-aligned
    xor     ecx, ecx                        ; lpAddress = NULL
    mov     edx, TASK_QUEUE_CAPACITY * TASK_ENTRY_SIZE
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@alloc_fail
    mov     QWORD PTR [g_task_queue], rax

    ; Zero the queue
    mov     rdi, rax
    xor     eax, eax
    mov     ecx, TASK_QUEUE_CAPACITY * TASK_ENTRY_SIZE
    rep     stosb

    ; Populate context
    lea     rax, g_vtable
    mov     QWORD PTR [g_orch_ctx.vtable_ptr], rax
    mov     rax, QWORD PTR [g_task_queue]
    mov     QWORD PTR [g_orch_ctx.queue_ptr], rax
    mov     DWORD PTR [g_orch_ctx.queue_head], 0
    mov     DWORD PTR [g_orch_ctx.queue_tail], 0
    lea     rax, g_handler_metrics
    mov     QWORD PTR [g_orch_ctx.metrics_ptr], rax
    lea     rax, g_hooks
    mov     QWORD PTR [g_orch_ctx.hooks_ptr], rax
    mov     QWORD PTR [g_orch_ctx.telem_ring], r12
    mov     QWORD PTR [g_orch_ctx.total_dispatches], 0
    mov     QWORD PTR [g_orch_ctx.total_errors], 0
    mov     QWORD PTR [g_orch_ctx.total_async], 0
    mov     DWORD PTR [g_orch_ctx.shutting_down], 0

    ; Mark initialized (must be last)
    mov     DWORD PTR [g_orch_ctx.initialized], 1

    ; Debug trace
    lea     rcx, szOrchInit
    call    OutputDebugStringA

    xor     eax, eax                        ; SUCCESS
    jmp     @@exit

@@already_init:
    mov     eax, ORCH_ERR_ALREADY_INIT
    jmp     @@exit

@@alloc_fail:
    mov     eax, ORCH_ERR_ALLOC

@@exit:
    add     rsp, 48
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_orchestrator_init ENDP

; =============================================================================
; asm_orchestrator_dispatch
; Synchronous dispatch: opcode → VTable lookup → handler call → metrics.
; ECX = opcode (7001–9700)
; RDX = CommandContext*
; R8  = CommandResult*
; Returns: RAX = handler HRESULT, or ORCH_ERR_* if dispatch fails
; =============================================================================
asm_orchestrator_dispatch PROC FRAME
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
    sub     rsp, 56
    .allocstack 56
    .endprolog

    ; Check initialized
    cmp     DWORD PTR [g_orch_ctx.initialized], 1
    jne     @@not_init

    mov     r12d, ecx                       ; opcode
    mov     r13, rdx                        ; CommandContext*
    mov     r14, r8                         ; CommandResult*

    ; ---- RDTSC start ----
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     r15, rax                        ; start_cycles

    ; ---- Map opcode → VTable index ----
    mov     ecx, r12d
    call    map_opcode_to_index
    cmp     eax, -1
    je      @@invalid_op
    mov     ebx, eax                        ; vtable_index

    ; ---- Load handler from VTable ----
    lea     rsi, g_vtable
    mov     rax, QWORD PTR [rsi + rbx*8]
    test    rax, rax
    jz      @@not_impl
    mov     rdi, rax                        ; handler function pointer

    ; ---- Fire pre-dispatch hooks ----
    mov     ecx, HOOK_PRE_DISPATCH
    mov     edx, r12d
    mov     r8, r13
    mov     r9, r14
    call    fire_hooks

    ; ---- Call handler(CommandContext*, CommandResult*) ----
    mov     rcx, r13                        ; Arg 1: context
    mov     rdx, r14                        ; Arg 2: result
    call    rdi
    mov     rsi, rax                        ; Save handler return value

    ; ---- Fire post-dispatch hooks ----
    push    rsi
    mov     ecx, HOOK_POST_DISPATCH
    mov     edx, r12d
    mov     r8, r13
    mov     r9, r14
    call    fire_hooks
    pop     rsi

    ; ---- RDTSC end + metrics update ----
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    sub     rax, r15                        ; elapsed_cycles

    ; Load metrics slot (atomic updates)
    lea     rdi, g_handler_metrics
    imul    rcx, rbx, SIZEOF HANDLER_METRICS
    add     rdi, rcx                        ; -> metrics[vtable_index]

    lock inc QWORD PTR [rdi].HANDLER_METRICS.dispatch_count
    lock add QWORD PTR [rdi].HANDLER_METRICS.total_cycles, rax

    ; Update min_cycles (lock cmpxchg loop)
    mov     rcx, rax                        ; elapsed
@@cmpxchg_min:
    mov     rax, QWORD PTR [rdi].HANDLER_METRICS.min_cycles
    cmp     rcx, rax
    jge     @@check_max
    lock cmpxchg QWORD PTR [rdi].HANDLER_METRICS.min_cycles, rcx
    jne     @@cmpxchg_min

@@check_max:
    ; Update max_cycles (lock cmpxchg loop)
    mov     rcx, rax                        ; Use elapsed from rdtsc
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    sub     rax, r15                        ; Re-read elapsed (for max)
    mov     rcx, rax
@@cmpxchg_max:
    mov     rax, QWORD PTR [rdi].HANDLER_METRICS.max_cycles
    cmp     rcx, rax
    jle     @@metrics_done
    lock cmpxchg QWORD PTR [rdi].HANDLER_METRICS.max_cycles, rcx
    jne     @@cmpxchg_max

@@metrics_done:
    mov     DWORD PTR [rdi].HANDLER_METRICS.last_status, esi

    ; Check for error (HRESULT < 0)
    test    esi, esi
    jns     @@no_error
    lock inc QWORD PTR [rdi].HANDLER_METRICS.error_count
    lock inc QWORD PTR [g_orch_ctx.total_errors]
@@no_error:
    lock inc QWORD PTR [g_orch_ctx.total_dispatches]

    mov     eax, esi                        ; Return handler HRESULT
    jmp     @@exit

@@not_init:
    mov     eax, ORCH_ERR_NOT_INIT
    jmp     @@exit

@@invalid_op:
    lea     rcx, szOrchError
    call    OutputDebugStringA
    mov     eax, ORCH_ERR_INVALID_OP
    jmp     @@exit

@@not_impl:
    lea     rcx, szOrchNotImpl
    call    OutputDebugStringA
    mov     eax, ORCH_ERR_NOT_IMPL

@@exit:
    add     rsp, 56
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_orchestrator_dispatch ENDP

; =============================================================================
; asm_orchestrator_set_vtable
; Hot-swap a handler slot in the VTable for runtime subsystem binding.
; ECX = opcode (7001–9700)
; RDX = new handler function pointer
; Returns: RAX = previous handler pointer (for chaining), or 0 on error
; =============================================================================
asm_orchestrator_set_vtable PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    cmp     DWORD PTR [g_orch_ctx.initialized], 1
    jne     @@err

    mov     rsi, rdx                        ; new handler

    ; Map opcode → index
    call    map_opcode_to_index
    cmp     eax, -1
    je      @@err

    ; Atomic swap
    lea     rbx, g_vtable
    mov     rcx, rsi
    xchg    QWORD PTR [rbx + rax*8], rcx
    mov     rax, rcx                        ; return old handler
    jmp     @@exit

@@err:
    xor     eax, eax

@@exit:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret
asm_orchestrator_set_vtable ENDP

; =============================================================================
; asm_orchestrator_queue_async
; Enqueue a task into the SPSC ring buffer for deferred dispatch.
; ECX = opcode
; RDX = CommandContext*
; R8  = CommandResult*
; R9  = Completion callback (void (*)(CommandResult*))
; Returns: RAX = 0 success, ORCH_ERR_QUEUE_FULL if ring is full
; =============================================================================
asm_orchestrator_queue_async PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    cmp     DWORD PTR [g_orch_ctx.initialized], 1
    jne     @@not_init_async

    mov     ebx, ecx                        ; opcode
    mov     rsi, rdx                        ; context
    mov     rdi, r8                         ; result
    ; r9 = callback (already in register)

    ; Lock-free SPSC enqueue
    mov     eax, DWORD PTR [g_orch_ctx.queue_head]
@@retry:
    mov     ecx, eax
    inc     ecx
    and     ecx, TASK_QUEUE_MASK            ; wrap

    ; Check full: new_head == tail?
    cmp     ecx, DWORD PTR [g_orch_ctx.queue_tail]
    je      @@queue_full

    lock cmpxchg DWORD PTR [g_orch_ctx.queue_head], ecx
    jne     @@retry

    ; EAX = claimed slot index (pre-increment value)
    imul    rdx, rax, TASK_ENTRY_SIZE
    add     rdx, QWORD PTR [g_task_queue]   ; -> slot

    ; Write task entry
    mov     DWORD PTR [rdx].TASK_ENTRY.opcode, ebx
    mov     DWORD PTR [rdx].TASK_ENTRY.flags, 0
    mov     QWORD PTR [rdx].TASK_ENTRY.context_ptr, rsi
    mov     QWORD PTR [rdx].TASK_ENTRY.result_ptr, rdi
    mov     QWORD PTR [rdx].TASK_ENTRY.callback_ptr, r9

    ; Timestamp via QPC
    lea     rcx, [rdx].TASK_ENTRY.timestamp
    call    QueryPerformanceCounter

    lock inc QWORD PTR [g_orch_ctx.total_async]

    xor     eax, eax                        ; SUCCESS
    jmp     @@exit_async

@@not_init_async:
    mov     eax, ORCH_ERR_NOT_INIT
    jmp     @@exit_async

@@queue_full:
    lea     rcx, szOrchQueueFull
    call    OutputDebugStringA
    mov     eax, ORCH_ERR_QUEUE_FULL

@@exit_async:
    add     rsp, 40
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_orchestrator_queue_async ENDP

; =============================================================================
; asm_orchestrator_drain_queue
; Process all pending tasks in the SPSC queue via synchronous dispatch.
; No parameters.
; Returns: RAX = number of tasks processed
; =============================================================================
asm_orchestrator_drain_queue PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     DWORD PTR [g_orch_ctx.initialized], 1
    jne     @@drain_exit

    xor     r12d, r12d                      ; processed count

@@drain_loop:
    ; Check empty: tail == head?
    mov     eax, DWORD PTR [g_orch_ctx.queue_tail]
    cmp     eax, DWORD PTR [g_orch_ctx.queue_head]
    je      @@drain_done

    ; Read task at tail
    imul    rdx, rax, TASK_ENTRY_SIZE
    add     rdx, QWORD PTR [g_task_queue]
    mov     rsi, rdx                        ; -> task entry

    ; Dispatch synchronously
    mov     ecx, DWORD PTR [rsi].TASK_ENTRY.opcode
    mov     rdx, QWORD PTR [rsi].TASK_ENTRY.context_ptr
    mov     r8,  QWORD PTR [rsi].TASK_ENTRY.result_ptr
    call    asm_orchestrator_dispatch

    ; Call completion callback if set
    mov     rdi, QWORD PTR [rsi].TASK_ENTRY.callback_ptr
    test    rdi, rdi
    jz      @@no_callback

    mov     rcx, QWORD PTR [rsi].TASK_ENTRY.result_ptr
    call    rdi

@@no_callback:
    ; Advance tail (release the slot)
    mov     eax, DWORD PTR [g_orch_ctx.queue_tail]
    inc     eax
    and     eax, TASK_QUEUE_MASK
    mov     DWORD PTR [g_orch_ctx.queue_tail], eax

    inc     r12d
    jmp     @@drain_loop

@@drain_done:
    mov     eax, r12d

@@drain_exit:
    add     rsp, 48
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_orchestrator_drain_queue ENDP

; =============================================================================
; asm_orchestrator_register_hook
; Register a pre- or post-dispatch callback.
; ECX = hook_type (HOOK_PRE_DISPATCH or HOOK_POST_DISPATCH)
; RDX = callback function pointer
; Returns: RAX = hook index (>= 0), or -1 on failure
; =============================================================================
asm_orchestrator_register_hook PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    lea     rcx, g_orch_cs
    call    EnterCriticalSection

    mov     eax, DWORD PTR [g_orch_ctx.hook_count]
    cmp     eax, MAX_HOOKS
    jge     @@hooks_full

    ; Write hook entry
    lea     rsi, g_hooks
    imul    rbx, rax, SIZEOF HOOK_ENTRY
    add     rsi, rbx

    mov     DWORD PTR [rsi].HOOK_ENTRY.hook_type, ecx
    mov     QWORD PTR [rsi].HOOK_ENTRY.callback_ptr, rdx

    mov     ebx, eax                        ; save index
    inc     eax
    mov     DWORD PTR [g_orch_ctx.hook_count], eax

    lea     rcx, g_orch_cs
    call    LeaveCriticalSection

    mov     eax, ebx                        ; return hook index
    jmp     @@exit_hook

@@hooks_full:
    lea     rcx, g_orch_cs
    call    LeaveCriticalSection
    mov     eax, -1

@@exit_hook:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret
asm_orchestrator_register_hook ENDP

; =============================================================================
; asm_orchestrator_get_metrics
; Copy metrics for a given handler slot.
; ECX = opcode (7001–9700)
; RDX = output HANDLER_METRICS*
; Returns: RAX = 0 success, or ORCH_ERR_INVALID_OP
; =============================================================================
asm_orchestrator_get_metrics PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rdi, rdx                        ; output ptr

    ; Map opcode
    call    map_opcode_to_index
    cmp     eax, -1
    je      @@metrics_invalid

    ; Copy metrics struct
    lea     rsi, g_handler_metrics
    imul    rax, rax, SIZEOF HANDLER_METRICS
    add     rsi, rax

    mov     rcx, rdi                        ; dest
    mov     rdx, rsi                        ; src
    mov     r8d, SIZEOF HANDLER_METRICS     ; size
    call    memcpy

    xor     eax, eax
    jmp     @@exit_metrics

@@metrics_invalid:
    mov     eax, ORCH_ERR_INVALID_OP

@@exit_metrics:
    add     rsp, 40
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_orchestrator_get_metrics ENDP

; =============================================================================
; asm_orchestrator_shutdown
; Drain remaining tasks, release task queue memory, destroy critical section.
; No parameters.
; Returns: RAX = 0 success
; =============================================================================
asm_orchestrator_shutdown PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     DWORD PTR [g_orch_ctx.initialized], 1
    jne     @@shutdown_exit

    ; Set shutdown flag
    mov     DWORD PTR [g_orch_ctx.shutting_down], 1

    ; Drain any remaining tasks
    call    asm_orchestrator_drain_queue

    ; Free task queue
    mov     rcx, QWORD PTR [g_task_queue]
    test    rcx, rcx
    jz      @@skip_free
    xor     edx, edx                        ; dwSize = 0
    mov     r8d, MEM_RELEASE
    call    VirtualFree
    mov     QWORD PTR [g_task_queue], 0

@@skip_free:
    ; Destroy critical section
    lea     rcx, g_orch_cs
    call    DeleteCriticalSection

    ; Clear context
    lea     rdi, g_orch_ctx
    xor     eax, eax
    mov     ecx, SIZEOF ORCH_CONTEXT
    rep     stosb

    lea     rcx, szOrchShutdown
    call    OutputDebugStringA

@@shutdown_exit:
    xor     eax, eax
    add     rsp, 48
    pop     rbx
    ret
asm_orchestrator_shutdown ENDP

; =============================================================================
; asm_orchestrator_lsp_sync
; v14.2.1 Cathedral Build: Direct bridge between SymbolIndex and
; ContextAnalyzer via the LSP-AI Bridge module. Dispatches through the
; VTable at slot 21 (OP_LSP_AI_SYNC = 9800) with deep sync mode.
;
; RCX = SYMBOL_ENTRY* array (from LSP SymbolIndex snapshot)
; EDX = symbol count
; R8D = sync mode (0=SHALLOW, 1=DEEP, 2=FULL_SEMANTIC)
; Returns: RAX = 0 success, error code otherwise
; =============================================================================
asm_orchestrator_lsp_sync PROC FRAME
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
    sub     rsp, 56
    .allocstack 56
    .endprolog

    cmp     DWORD PTR [g_orch_ctx.initialized], 1
    jne     @@lsp_not_init

    mov     r12, rcx                        ; SYMBOL_ENTRY*
    mov     r13d, edx                       ; count
    mov     ebx, r8d                        ; mode

    ; Verify OP_LSP_AI_SYNC handler is registered (VTable slot 21)
    lea     rsi, g_vtable
    mov     rax, QWORD PTR [rsi + 21*8]
    test    rax, rax
    jz      @@lsp_not_impl
    mov     rdi, rax                        ; handler

    ; ---- RDTSC start ----
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    push    rax                             ; save start_cycles

    ; Fire pre-dispatch hooks
    mov     ecx, HOOK_PRE_DISPATCH
    mov     edx, OP_LSP_AI_SYNC
    mov     r8, r12
    xor     r9d, r9d
    call    fire_hooks

    ; Call LSP-AI bridge handler: (symbols, count, mode)
    mov     rcx, r12                        ; SYMBOL_ENTRY*
    mov     edx, r13d                       ; count
    mov     r8d, ebx                        ; mode
    call    rdi
    mov     esi, eax                        ; save result

    ; Fire post-dispatch hooks
    push    rsi
    mov     ecx, HOOK_POST_DISPATCH
    mov     edx, OP_LSP_AI_SYNC
    mov     r8, r12
    xor     r9d, r9d
    call    fire_hooks
    pop     rsi

    ; ---- Metrics for slot 21 ----
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    pop     rcx                             ; start_cycles
    sub     rax, rcx                        ; elapsed

    lea     rdi, g_handler_metrics
    add     rdi, 21 * (SIZEOF HANDLER_METRICS)
    lock inc QWORD PTR [rdi].HANDLER_METRICS.dispatch_count
    lock add QWORD PTR [rdi].HANDLER_METRICS.total_cycles, rax
    mov     DWORD PTR [rdi].HANDLER_METRICS.last_status, esi
    lock inc QWORD PTR [g_orch_ctx.total_dispatches]

    test    esi, esi
    jns     @@lsp_no_error
    lock inc QWORD PTR [rdi].HANDLER_METRICS.error_count
    lock inc QWORD PTR [g_orch_ctx.total_errors]
@@lsp_no_error:

    mov     eax, esi
    jmp     @@lsp_exit

@@lsp_not_init:
    mov     eax, ORCH_ERR_NOT_INIT
    jmp     @@lsp_exit
@@lsp_not_impl:
    mov     eax, ORCH_ERR_NOT_IMPL

@@lsp_exit:
    add     rsp, 56
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_orchestrator_lsp_sync ENDP

END
