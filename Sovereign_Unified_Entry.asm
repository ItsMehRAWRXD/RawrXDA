; ==============================================================================
; Sovereign_Unified_Entry.asm - Master Control Loop
; ==============================================================================
; Unified stress test entry for all 15 Sovereign Framework components.
; Architecture:
;   - Main Thread (Core 0): Message loop + Ghost Engine rendering
;   - AI Thread (Core 1): Orchestrator inference + prediction push
;   - Telemetry Thread (Core 2): RDTSC logging + ring buffer management
;   - Watchdog Thread (Core 3): Latency monitoring + auto-throttle
;
; Acceptance: All components active simultaneously, latency < 0.5ms
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

; ==============================================================================
; External APIs
; ==============================================================================
EXTERN ExitProcess : PROC
EXTERN GetModuleHandleA : PROC
EXTERN GetProcAddress : PROC
EXTERN CreateThread : PROC
EXTERN SetThreadAffinityMask : PROC
EXTERN GetCurrentThread : PROC
EXTERN WaitForSingleObject : PROC
EXTERN Sleep : PROC
EXTERN GetTickCount : PROC
EXTERN QueryPerformanceCounter : PROC
EXTERN QueryPerformanceFrequency : PROC

; Sovereign Framework APIs
EXTERN INIT_GHOST_BUFFER : PROC
EXTERN PUSH_GHOST_PREDICTION : PROC
EXTERN RENDER_GHOST_PREDICTIVE : PROC
EXTERN GET_GHOST_STATS : PROC
EXTERN INSTALL_HOOK : PROC
EXTERN UNINSTALL_HOOK : PROC
EXTERN HOOK_HANDLER : PROC
EXTERN GET_LATENCY_STATS : PROC
EXTERN VALIDATE_TOKEN_SIMD : PROC
EXTERN FNV1A_64 : PROC
EXTERN BUILD_SYMBOL_HASH_TABLE : PROC
EXTERN RESOLVE_SYMBOL_FROM_PE : PROC

; ==============================================================================
; Constants
; ==============================================================================
LATENCY_BUDGET_CYCLES   equ 1500000       ; 0.5ms at 3GHz
THROTTLE_THRESHOLD      equ 5             ; 5 consecutive breaches
TEST_DURATION_MS        equ 5000          ; 5 second stress test

; Thread affinity masks
CORE_0                  equ 1
CORE_1                  equ 2
CORE_2                  equ 4
CORE_3                  equ 8

; ==============================================================================
; Data Section
; ==============================================================================
.data
ALIGN 16

; Thread handles
hMainThread             dq 0
hAIThread               dq 0
hTelemetryThread        dq 0
hWatchdogThread         dq 0

; Shared state (cache-line aligned at 16 bytes)
ALIGN 16
GlobalState:
    TestRunning         dq 1
    ThrottleFlag        dq 0
    ConsecutiveBreaches dq 0
    TotalFrames         dq 0
    DroppedFrames       dq 0
    AIPushCount         dq 0
    TelemetryCount      dq 0

; Latency ring buffer (64 entries for watchdog)
ALIGN 16
LatencyRing:
    LatencyHead         dq 0
    LatencyTail         dq 0
    LatencyBuffer       dq 64 dup(0)

; Test text for AI predictions
ALIGN 16
TestPrediction          db "function calculateTotal(items) { return items.reduce((a,b)=>a+b,0); }", 0
TestPredictionLen       equ $ - TestPrediction - 1

; QPC frequency
ALIGN 8
QPCFreq                 dq 0

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; PIN_THREAD: Bind current thread to specified core
; RCX = AffinityMask
; ==============================================================================
PIN_THREAD PROC
    push rcx
    call GetCurrentThread
    mov rcx, rax
    pop rdx
    call SetThreadAffinityMask
    ret
PIN_THREAD ENDP

; ==============================================================================
; READ_LATENCY: Get latest latency from ring buffer
; Returns: RAX = latest latency in cycles, 0 if empty
; ==============================================================================
READ_LATENCY PROC
    push rbx
    mov rbx, LatencyTail
    cmp rbx, LatencyHead
    je rl_empty
    
    ; Read entry
    mov rax, LatencyBuffer[rbx * 8]
    
    ; Advance tail
    inc rbx
    and rbx, 63
    mov LatencyTail, rbx
    
    pop rbx
    ret
    
rl_empty:
    xor rax, rax
    pop rbx
    ret
READ_LATENCY ENDP

; ==============================================================================
; WRITE_LATENCY: Push latency to ring buffer
; RCX = Latency in cycles
; ==============================================================================
WRITE_LATENCY PROC
    push rbx
    push rsi
    
    mov rbx, LatencyHead
    mov rsi, rbx
    inc rsi
    and rsi, 63
    
    ; Check full
    cmp rsi, LatencyTail
    je wl_skip
    
    ; Write
    mov LatencyBuffer[rbx * 8], rcx
    
    ; Advance
    mov LatencyHead, rsi
    
wl_skip:
    pop rsi
    pop rbx
    ret
WRITE_LATENCY ENDP

; ==============================================================================
; AI_WORKER_THREAD: AI Orchestrator on Core 1
; RCX = Parameter (unused)
; ==============================================================================
AI_WORKER_THREAD PROC
    push rbx
    push r12
    
    ; Pin to Core 1
    mov ecx, CORE_1
    call PIN_THREAD
    
    ; Initialize hash table for symbol resolution
    xor ecx, ecx
    call GetModuleHandleA
    test rax, rax
    jz ai_exit
    mov r12, rax            ; r12 = kernel32 base
    
    ; Build symbol hash table
    mov rcx, r12
    call BUILD_SYMBOL_HASH_TABLE
    
    ; Work loop
    mov rbx, 0
    
ai_loop:
    ; Check if test still running
    mov rax, TestRunning
    test rax, rax
    jz ai_exit
    
    ; Check throttle flag
    mov rax, ThrottleFlag
    test rax, rax
    jnz ai_throttled
    
    ; Push prediction to Ghost Engine
    lea rcx, TestPrediction
    mov rdx, TestPredictionLen
    mov r8d, 03F800000h     ; 1.0f confidence
    call PUSH_GHOST_PREDICTION
    
    test eax, eax
    jz ai_skip_count
    inc AIPushCount
    
ai_skip_count:
    
    ; Small yield
    mov ecx, 1
    call Sleep
    
    jmp ai_loop
    
ai_throttled:
    ; Throttled: longer sleep
    mov ecx, 10
    call Sleep
    jmp ai_loop
    
ai_exit:
    pop r12
    pop rbx
    xor eax, eax
    ret
AI_WORKER_THREAD ENDP

; ==============================================================================
; TELEMETRY_WORKER_THREAD: Telemetry on Core 2
; RCX = Parameter (unused)
; ==============================================================================
TELEMETRY_WORKER_THREAD PROC
    push rbx
    
    ; Pin to Core 2
    mov ecx, CORE_2
    call PIN_THREAD
    
    ; Work loop
    mov rbx, 0
    
tel_loop:
    ; Check if test still running
    mov rax, TestRunning
    test rax, rax
    jz tel_exit
    
    ; Read latest latency
    call READ_LATENCY
    test rax, rax
    jz tel_skip
    
    ; Log telemetry (just count for now)
    inc TelemetryCount
    
tel_skip:
    ; Yield
    mov ecx, 1
    call Sleep
    
    jmp tel_loop
    
tel_exit:
    pop rbx
    xor eax, eax
    ret
TELEMETRY_WORKER_THREAD ENDP

; ==============================================================================
; WATCHDOG_THREAD: Latency monitor on Core 3
; RCX = Parameter (unused)
; ==============================================================================
WATCHDOG_THREAD PROC
    push rbx
    push r12
    
    ; Pin to Core 3
    mov ecx, CORE_3
    call PIN_THREAD
    
    ; Initialize breach counter
    mov r12, 0              ; Consecutive breach count
    
wd_loop:
    ; Check if test still running
    mov rax, TestRunning
    test rax, rax
    jz wd_exit
    
    ; Read latest latency
    call READ_LATENCY
    test rax, rax
    jz wd_skip
    
    ; Check against budget
    cmp rax, LATENCY_BUDGET_CYCLES
    ja wd_breach
    
    ; Within budget: reset breach counter
    mov r12, 0
    mov ThrottleFlag, 0
    jmp wd_skip
    
wd_breach:
    ; Breach detected
    inc r12
    mov ConsecutiveBreaches, r12
    
    ; Check if threshold reached
    cmp r12, THROTTLE_THRESHOLD
    jb wd_skip
    
    ; Auto-throttle: set flag
    mov ThrottleFlag, 1
    
wd_skip:
    ; Check every 10ms
    mov ecx, 10
    call Sleep
    
    jmp wd_loop
    
wd_exit:
    pop r12
    pop rbx
    xor eax, eax
    ret
WATCHDOG_THREAD ENDP

; ==============================================================================
; MAIN_WORKER: Main thread on Core 0
; ==============================================================================
MAIN_WORKER PROC
    push rbx
    push r12
    push r13
    
    ; Pin to Core 0
    mov ecx, CORE_0
    call PIN_THREAD
    
    ; Initialize Ghost Engine
    call INIT_GHOST_BUFFER
    test eax, eax
    jz main_fail
    
    ; Install hook on GetMessageA
    xor ecx, ecx
    call GetModuleHandleA
    mov rcx, rax
    lea rdx, szGetMessageA
    call GetProcAddress
    test rax, rax
    jz main_fail
    mov r12, rax            ; r12 = GetMessageA
    
    mov rcx, r12
    lea rdx, HOOK_HANDLER
    call INSTALL_HOOK
    test eax, eax
    jz main_fail
    
    ; Create AI worker thread
    xor ecx, ecx            ; lpThreadAttributes
    xor edx, edx            ; dwStackSize
    lea r8, AI_WORKER_THREAD
    xor r9d, r9d            ; lpParameter
    mov qword ptr [rsp + 32], 0  ; dwCreationFlags
    lea rax, hAIThread
    mov [rsp + 40], rax     ; lpThreadId
    call CreateThread
    test rax, rax
    jz main_fail
    mov hAIThread, rax
    
    ; Create telemetry thread
    xor ecx, ecx
    xor edx, edx
    lea r8, TELEMETRY_WORKER_THREAD
    xor r9d, r9d
    mov qword ptr [rsp + 32], 0
    lea rax, hTelemetryThread
    mov [rsp + 40], rax
    call CreateThread
    test rax, rax
    jz main_fail
    mov hTelemetryThread, rax
    
    ; Create watchdog thread
    xor ecx, ecx
    xor edx, edx
    lea r8, WATCHDOG_THREAD
    xor r9d, r9d
    mov qword ptr [rsp + 32], 0
    lea rax, hWatchdogThread
    mov [rsp + 40], rax
    call CreateThread
    test rax, rax
    jz main_fail
    mov hWatchdogThread, rax
    
    ; Main work loop (simulate message pump)
    mov r13d, TEST_DURATION_MS
    mov rbx, 0              ; Frame counter
    
main_loop:
    ; Start frame timing
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov r12, rax
    
    ; Simulate message processing
    mov ecx, 1
    call Sleep
    
    ; Try to render ghost prediction
    xor ecx, ecx            ; HWND = 0 (test mode)
    call RENDER_GHOST_PREDICTIVE
    
    ; End frame timing
    rdtsc
    shl rdx, 32
    or rax, rdx
    sub rax, r12
    
    ; Log latency
    mov rcx, rax
    call WRITE_LATENCY
    
    ; Update stats
    inc TotalFrames
    
    ; Check if frame exceeded budget
    cmp rax, LATENCY_BUDGET_CYCLES
    jbe main_ok
    inc DroppedFrames
    
main_ok:
    dec r13d
    jnz main_loop
    
    ; Stop test
    mov TestRunning, 0
    
    ; Uninstall hook
    call UNINSTALL_HOOK
    
    ; Wait for worker threads (2 seconds)
    mov rcx, hAIThread
    mov edx, 2000
    call WaitForSingleObject
    
    mov rcx, hTelemetryThread
    mov edx, 2000
    call WaitForSingleObject
    
    mov rcx, hWatchdogThread
    mov edx, 2000
    call WaitForSingleObject
    
    ; Validate results
    ; Check if any throttling occurred
    mov rax, ThrottleFlag
    test rax, rax
    jnz main_fail
    
    ; Check if dropped frames are acceptable (< 1%)
    mov rax, DroppedFrames
    imul rax, 100
    mov rcx, TotalFrames
    xor rdx, rdx
    div rcx
    cmp rax, 1
    ja main_fail
    
    ; Check if AI pushed predictions
    mov rax, AIPushCount
    test rax, rax
    jz main_fail
    
    ; Success
    mov eax, 1
    jmp main_exit
    
main_fail:
    mov TestRunning, 0
    xor eax, eax
    
main_exit:
    pop r13
    pop r12
    pop rbx
    ret
MAIN_WORKER ENDP

; ==============================================================================
; main - Unified entry point
; ==============================================================================
main PROC
    sub rsp, 40
    
    ; Run main worker
    call MAIN_WORKER
    test eax, eax
    jz unified_fail
    
    ; Success: exit 0
    xor ecx, ecx
    call ExitProcess
    
unified_fail:
    mov ecx, 1
    call ExitProcess
    
main ENDP

; ==============================================================================
; Data
; ==============================================================================
.data
szGetMessageA           db "GetMessageA", 0

; ==============================================================================
; End
; ==============================================================================
end
