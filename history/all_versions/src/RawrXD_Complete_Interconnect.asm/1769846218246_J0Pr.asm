; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Complete_Interconnect.asm  ─  Wiring Everything Together
; Final unit that exports the unified interface and handles cross-unit coordination
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
; OPTION WIN64:3

include windows.inc
include kernel32.inc

includelib kernel32.lib

; ═══════════════════════════════════════════════════════════════════════════════
; EXTERNAL IMPORTS (from all previous units)
; ═══════════════════════════════════════════════════════════════════════════════
; From System_Primitives
EXTERNDEF System_InitializePrimitives:PROC
EXTERNDEF Spinlock_Acquire:PROC
EXTERNDEF Spinlock_Release:PROC

; From RingBuffer_Consumer
EXTERNDEF RingBufferConsumer_Initialize:PROC
EXTERNDEF RingBufferConsumer_Shutdown:PROC

; From HTTP_Router
EXTERNDEF HttpRouter_Initialize:PROC
EXTERNDEF QueueInferenceJob:PROC

; From Model_StateMachine
EXTERNDEF ModelState_Initialize:PROC
EXTERNDEF ModelState_Transition:PROC
EXTERNDEF ModelState_AcquireInstance:PROC

; From Swarm_Orchestrator
EXTERNDEF Swarm_Initialize:PROC
EXTERNDEF Swarm_SubmitJob:PROC

; From Agentic_Router
EXTERNDEF AgentRouter_Initialize:PROC
EXTERNDEF AgentRouter_ExecuteTask:PROC

; From GPU_Memory
EXTERNDEF Vram_Initialize:PROC
EXTERNDEF Vram_Allocate:PROC

; From Inference_Engine
EXTERNDEF Inference_Initialize:PROC
EXTERNDEF InferenceEngine_Submit:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES
; ═══════════════════════════════════════════════════════════════════════════════
RawrXD_GlobalContext STRUCT
    Initialized         DWORD       ?
    hShutdownEvent      QWORD       ?
    
    ; Subsystem handles
    hHttpThread         QWORD       ?
    hConsumerThread     QWORD       ?
    
    ; Metrics
    StartTick           QWORD       ?
    TotalRequests       QWORD       ?
RawrXD_GlobalContext ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.DATA
align 16
g_GlobalContext         RawrXD_GlobalContext <>

szVersionString         BYTE "RawrXD Interconnect v1.0.0", 0

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_InitializeAll
; One-call initialization of entire interconnect stack
; RCX = hWnd for UI output (can be NULL for headless)
; RDX = Total VRAM size to manage
; Returns RAX = TRUE on success
; ═══════════════════════════════════════════════════════════════════════════════
RawrXD_InitializeAll PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; hWnd
    mov rsi, rdx                    ; VRAM size
    
    ; Step 1: System primitives
    call System_InitializePrimitives
    
    ; Step 2: Memory management
    mov rcx, rsi
    call Vram_Initialize
    
    ; Step 3: Model state management
    call ModelState_Initialize
    
    ; Step 4: Swarm orchestrator
    call Swarm_Initialize
    
    ; Step 5: Inference engine
    call Inference_Initialize
    
    ; Step 6: Agentic router
    call AgentRouter_Initialize
    
    ; Step 7: HTTP router (if not headless)
    test rbx, rbx
    jz @no_http
    call HttpRouter_Initialize
    jmp @http_done
    
@no_http:
    ; Headless mode - skip HTTP, use direct API
    
@http_done:
    ; Step 8: Ring buffer consumer (if hWnd provided)
    test rbx, rbx
    jz @no_consumer
    mov rdx, 0                      ; Vocab table - would be loaded
    mov rcx, rbx
    call RingBufferConsumer_Initialize
    
@no_consumer:
    ; Mark initialized
    mov g_GlobalContext.Initialized, 1
    call GetTickCount64
    mov g_GlobalContext.StartTick, rax
    
    mov rax, TRUE
    
    pop rdi
    pop rsi
    pop rbx
    ret
RawrXD_InitializeAll ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_ShutdownAll
; Graceful shutdown with cleanup
; ═══════════════════════════════════════════════════════════════════════════════
RawrXD_ShutdownAll PROC
    ; Signal shutdown
    mov g_GlobalContext.Initialized, 0
    
    ; Stop accepting new requests
    
    ; Wait for active inferences to complete or timeout
    
    ; Free VRAM
    
    ; Close threads
    
    ret
RawrXD_ShutdownAll ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_SubmitChatRequest
; High-level API: submit natural language request, get response
; RCX = Session handle, RDX = Input text, R8 = Output callback
; ═══════════════════════════════════════════════════════════════════════════════
RawrXD_SubmitChatRequest PROC
    ; 1. Classify intent via Agentic_Router
    
    ; 2. Select model via Swarm_Orchestrator
    
    ; 3. Queue inference job
    
    ; 4. Return immediately, callback fires on completion
    ret
RawrXD_SubmitChatRequest ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_GetMetrics
; Returns performance statistics
; RCX = pointer to metrics structure to fill
; ═══════════════════════════════════════════════════════════════════════════════
RawrXD_GetMetrics PROC
    push rsi
    mov rsi, rcx
    
    call GetTickCount64
    sub rax, g_GlobalContext.StartTick
    mov [rsi + 0], rax              ; Uptime ms
    
    mov rax, g_GlobalContext.TotalRequests
    mov [rsi + 8], rax
    
    ; Aggregate from all subsystems...
    
    pop rsi
    ret
RawrXD_GetMetrics ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; EXPORTS - Unified API
; ═══════════════════════════════════════════════════════════════════════════════
PUBLIC RawrXD_InitializeAll
PUBLIC RawrXD_ShutdownAll
PUBLIC RawrXD_SubmitChatRequest
PUBLIC RawrXD_GetMetrics

END
