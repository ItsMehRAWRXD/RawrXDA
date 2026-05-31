; ==============================================================================
; Sovereign Hook Simulator - High-Fidelity Telemetry Engine
; ==============================================================================
; Deploys atomic detour to GetMessageW with trampoline-based original execution.
; Measures round-trip latency via RDTSC and pushes telemetry to Ghost Engine.
;
; Architecture:
;   - INSTALL_HOOK: Atomic 5-byte JMP detour with VirtualProtect
;   - UNINSTALL_HOOK: Restore original bytes from backup
;   - TRAMPOLINE_STUB: Allocated executable stub for original code path
;   - HOOK_HANDLER: RDTSC timing + Ghost Engine telemetry push
;   - GET_LATENCY_STATS: Retrieve accumulated timing statistics
;
; Exports:
;   INSTALL_HOOK, UNINSTALL_HOOK, GET_LATENCY_STATS
; ==============================================================================

option casemap:none

; ==============================================================================
; External APIs
; ==============================================================================
EXTERN VirtualProtect : PROC
EXTERN FlushInstructionCache : PROC
EXTERN GetCurrentProcess : PROC
EXTERN GetModuleHandleA : PROC
EXTERN GetProcAddress : PROC
EXTERN VirtualAlloc : PROC
EXTERN VirtualFree : PROC

; Ghost Engine APIs for telemetry
EXTERN PUSH_GHOST_PREDICTION : PROC
EXTERN GET_GHOST_LATENCY : PROC

; ==============================================================================
; Constants
; ==============================================================================
PAGE_EXECUTE_READWRITE  equ 040h
MEM_COMMIT              equ 01000h
MEM_RELEASE             equ 08000h

; Hook configuration
HOOK_SIZE               equ 5
TRAMPOLINE_SIZE         equ 32

; Latency budget (0.5ms at 3GHz = 1,500,000 cycles)
LATENCY_BUDGET          equ 1500000

; ==============================================================================
; Data Section
; ==============================================================================
.data
ALIGN 16

; Hook state
HookInstalled           dq 0
TargetFuncAddr          dq 0
HookFuncAddr            dq 0
TrampolineAddr          dq 0

; Original bytes backup
ALIGN 16
OriginalBytes           db HOOK_SIZE dup(0)

; Latency statistics
ALIGN 16
LatencyTotal            dq 0
LatencyCount            dq 0
LatencyMax              dq 0
LatencyMin              dq 0FFFFFFFFFFFFFFFFh

; Ghost Engine telemetry text
ALIGN 16
TelemetryText           db "HOOK_LATENCY", 0
TelemetryLen            equ $ - TelemetryText - 1

; Confidence value for telemetry (1.0f)
ALIGN 16
TelemetryConfidence     dd 03F800000h

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; INSTALL_HOOK: Deploy atomic detour with trampoline
; RCX = TargetFunc (e.g., GetMessageW)
; RDX = HookFunc (HOOK_HANDLER)
; Returns: RAX = 1 (success), 0 (failure)
; ==============================================================================
INSTALL_HOOK PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    .endprolog
    
    mov r12, rcx            ; TargetFunc
    mov r13, rdx            ; HookFunc
    
    ; Check if already installed
    mov rax, [HookInstalled]
    test rax, rax
    jnz install_already
    
    ; Save addresses
    mov [TargetFuncAddr], r12
    mov [HookFuncAddr], r13
    
    ; 1. Allocate trampoline stub (executable memory)
    mov rcx, 0              ; lpAddress (let system choose)
    mov rdx, TRAMPOLINE_SIZE
    mov r8, MEM_COMMIT
    mov r9, PAGE_EXECUTE_READWRITE
    call VirtualAlloc
    
    test rax, rax
    jz install_fail
    mov [TrampolineAddr], rax
    
    ; 2. Build trampoline:
    ;    [0-4]:  Original 5 bytes from target
    ;    [5-9]:  JMP rel32 back to TargetFunc+5
    mov rdi, rax            ; Trampoline destination
    mov rsi, r12            ; Target source
    mov rcx, HOOK_SIZE
    rep movsb               ; Copy original 5 bytes
    
    ; Write JMP back to TargetFunc+5
    mov byte ptr [rdi], 0E9h
    mov rbx, r12
    add rbx, HOOK_SIZE      ; TargetFunc + 5
    sub rbx, rdi
    sub rbx, 5              ; Relative offset
    mov [rdi + 1], ebx
    
    ; 3. Change target protection to RWX
    sub rsp, 40
    mov rcx, r12
    mov rdx, HOOK_SIZE
    mov r8, PAGE_EXECUTE_READWRITE
    lea r9, [rsp + 32]      ; lpflOldProtect
    call VirtualProtect
    add rsp, 40
    
    test rax, rax
    jz install_fail
    
    ; 4. Backup original bytes (5 bytes = 4 + 1)
    mov rax, r12
    mov ebx, dword ptr [rax]
    mov dword ptr [OriginalBytes], ebx
    mov bl, byte ptr [rax + 4]
    mov byte ptr [OriginalBytes + 4], bl
    
    ; 5. Write atomic 5-byte JMP detour
    mov rax, r12
    mov byte ptr [rax], 0E9h
    mov rbx, r13
    sub rbx, r12
    sub rbx, HOOK_SIZE
    mov dword ptr [rax + 1], ebx
    
    ; 6. Flush instruction cache
    call GetCurrentProcess
    mov rcx, rax
    mov rdx, r12
    mov r8, HOOK_SIZE
    call FlushInstructionCache
    
    ; 7. Mark as installed
    mov qword ptr [HookInstalled], 1
    
    ; Initialize latency stats
    mov qword ptr [LatencyTotal], 0
    mov qword ptr [LatencyCount], 0
    mov qword ptr [LatencyMax], 0
    mov qword ptr [LatencyMin], 0FFFFFFFFFFFFFFFFh
    
    mov rax, 1
    jmp install_exit
    
install_already:
    mov rax, 1              ; Already installed = success
    jmp install_exit
    
install_fail:
    xor rax, rax
    
install_exit:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
INSTALL_HOOK ENDP

; ==============================================================================
; UNINSTALL_HOOK: Restore original bytes and cleanup
; Returns: RAX = 1 (success), 0 (failure)
; ==============================================================================
UNINSTALL_HOOK PROC FRAME
    push rbx
    .endprolog
    
    ; Check if installed
    mov rax, [HookInstalled]
    test rax, rax
    jz uninstall_fail
    
    mov rbx, [TargetFuncAddr]
    
    ; 1. Change protection to RWX
    sub rsp, 40
    mov rcx, rbx
    mov rdx, HOOK_SIZE
    mov r8, PAGE_EXECUTE_READWRITE
    lea r9, [rsp + 32]
    call VirtualProtect
    add rsp, 40
    
    test rax, rax
    jz uninstall_fail
    
    ; 2. Restore original bytes (5 bytes = 4 + 1)
    mov eax, dword ptr [OriginalBytes]
    mov dword ptr [rbx], eax
    mov al, byte ptr [OriginalBytes + 4]
    mov byte ptr [rbx + 4], al
    
    ; 3. Flush cache
    call GetCurrentProcess
    mov rcx, rax
    mov rdx, rbx
    mov r8, HOOK_SIZE
    call FlushInstructionCache
    
    ; 4. Free trampoline
    mov rcx, [TrampolineAddr]
    mov rdx, 0
    mov r8, MEM_RELEASE
    call VirtualFree
    
    ; 5. Clear state
    mov qword ptr [HookInstalled], 0
    mov qword ptr [TargetFuncAddr], 0
    mov qword ptr [HookFuncAddr], 0
    mov qword ptr [TrampolineAddr], 0
    
    mov rax, 1
    jmp uninstall_exit
    
uninstall_fail:
    xor rax, rax
    
uninstall_exit:
    pop rbx
    ret
UNINSTALL_HOOK ENDP

; ==============================================================================
; HOOK_HANDLER: Detour execution path with RDTSC timing
; This is called when the hooked function is invoked.
; Preserves all registers, measures latency, pushes telemetry.
; ==============================================================================
PUBLIC HOOK_HANDLER
HOOK_HANDLER PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    .endprolog
    
    ; Save argument registers (x64 calling convention)
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    
    ; 1. Start timing
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov rbx, rax            ; StartTick
    
    ; 2. Call original function via trampoline
    mov rcx, r12
    mov rdx, r13
    mov r8, r14
    mov r9, r15
    call qword ptr [TrampolineAddr]
    
    ; Save return value
    mov rsi, rax
    
    ; 3. End timing
    rdtsc
    shl rdx, 32
    or rax, rdx
    sub rax, rbx            ; Delta = EndTick - StartTick
    
    ; 4. Update statistics
    mov rdi, rax            ; Current latency
    
    ; Total
    add [LatencyTotal], rdi
    
    ; Count
    inc qword ptr [LatencyCount]
    
    ; Max
    mov rax, [LatencyMax]
    cmp rdi, rax
    jbe skip_max
    mov [LatencyMax], rdi
skip_max:
    
    ; Min
    mov rax, [LatencyMin]
    cmp rdi, rax
    jae skip_min
    mov [LatencyMin], rdi
skip_min:
    
    ; 5. Push telemetry to Ghost Engine (if latency > budget)
    cmp rdi, LATENCY_BUDGET
    jbe skip_telemetry
    
    ; Push prediction with latency info
    lea rcx, TelemetryText
    mov rdx, TelemetryLen
    mov r8d, [TelemetryConfidence]
    call PUSH_GHOST_PREDICTION
    
skip_telemetry:
    
    ; Restore return value
    mov rax, rsi
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
HOOK_HANDLER ENDP

; ==============================================================================
; GET_LATENCY_STATS: Retrieve accumulated timing statistics
; RCX = Pointer to output buffer (32 bytes)
;   [RCX+0]  = Total cycles
;   [RCX+8]  = Call count
;   [RCX+16] = Max latency
;   [RCX+24] = Min latency
; Returns: RAX = 1 always
; ==============================================================================
GET_LATENCY_STATS PROC
    mov rax, [LatencyTotal]
    mov [rcx + 0], rax
    mov rax, [LatencyCount]
    mov [rcx + 8], rax
    mov rax, [LatencyMax]
    mov [rcx + 16], rax
    mov rax, [LatencyMin]
    mov [rcx + 24], rax
    mov eax, 1
    ret
GET_LATENCY_STATS ENDP

; ==============================================================================
; GET_HOOK_STATUS: Check if hook is currently installed
; Returns: RAX = 1 (installed), 0 (not installed)
; ==============================================================================
GET_HOOK_STATUS PROC
    mov rax, [HookInstalled]
    ret
GET_HOOK_STATUS ENDP

; ==============================================================================
; RESOLVE_GETMESSAGEW: Find GetMessageW address via kernel32/user32
; Returns: RAX = Address of GetMessageW (or 0)
; ==============================================================================
RESOLVE_GETMESSAGEW PROC FRAME
    push rbx
    .endprolog
    
    ; Load user32.dll
    lea rcx, szUser32
    call GetModuleHandleA
    test rax, rax
    jz resolve_fail
    
    mov rbx, rax
    
    ; Get GetMessageW address
    mov rcx, rbx
    lea rdx, szGetMessageW
    call GetProcAddress
    
    test rax, rax
    jz resolve_fail
    
    jmp resolve_exit
    
resolve_fail:
    xor rax, rax
    
resolve_exit:
    pop rbx
    ret
RESOLVE_GETMESSAGEW ENDP

; ==============================================================================
; Data for RESOLVE_GETMESSAGEW
; ==============================================================================
.data
szUser32        db "user32.dll", 0
szGetMessageW   db "GetMessageW", 0

; ==============================================================================
; End
; ==============================================================================
end
