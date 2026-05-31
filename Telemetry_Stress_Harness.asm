; ==============================================================================
; Telemetry_Stress_Harness.asm - Direct Telemetry Push Overhead
; ==============================================================================
; Measures pure ring buffer push cost in CPU cycles.
;   - Baseline: 1000 empty iterations
;   - Telemetry: 1000 iterations with TELEMETRY_PUSH
;   - Validates avg delta < 150K cycles (~50μs at 3GHz)
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

EXTERN ExitProcess : PROC

; ==============================================================================
; Constants
; ==============================================================================
TELEMETRY_SLOTS         equ 4096
TELEMETRY_MASK          equ 4095
TEST_ITERATIONS         equ 1000
BUDGET_CYCLES           equ 150000          ; ~50μs at 3GHz

; ==============================================================================
; Data Section
; ==============================================================================
.data
ALIGN 16

; Ring buffer state
TelemetryHead         dq 0
TelemetryTail         dq 0
TelemetryDropped      dq 0

; Ring buffer entries (4096 * 64 bytes)
ALIGN 16
TelemetryBuffer       db 262144 dup(0)

; Stats
ALIGN 16
BaselineTotal         dq 0
TelemetryTotal        dq 0

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; TELEMETRY_PUSH: Write entry to ring buffer
; RCX = cycle count
; ==============================================================================
TELEMETRY_PUSH PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, TelemetryHead
    mov rsi, rbx
    inc rsi
    and rsi, TELEMETRY_MASK
    
    cmp rsi, TelemetryTail
    je tp_drop
    
    mov rdi, rbx
    shl rdi, 6
    lea rax, TelemetryBuffer
    add rdi, rax
    
    mov [rdi], rcx
    mov qword ptr [rdi + 8], 0
    mov qword ptr [rdi + 16], 0
    
    mfence
    mov TelemetryHead, rsi
    
    pop rdi
    pop rsi
    pop rbx
    ret
    
tp_drop:
    inc TelemetryDropped
    pop rdi
    pop rsi
    pop rbx
    ret
TELEMETRY_PUSH ENDP

; ==============================================================================
; main
; ==============================================================================
main PROC
    sub rsp, 40
    
    ; --- Baseline: 1000 empty iterations ---
    mov r12d, TEST_ITERATIONS
    
baseline_loop:
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov rbx, rax
    
    ; Empty body
    nop
    
    rdtsc
    shl rdx, 32
    or rax, rdx
    sub rax, rbx
    add BaselineTotal, rax
    
    dec r12d
    jnz baseline_loop
    
    ; --- Telemetry: 1000 iterations with push ---
    mov r12d, TEST_ITERATIONS
    
telemetry_loop:
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov rbx, rax
    
    ; Push telemetry
    mov rcx, 1234
    call TELEMETRY_PUSH
    
    rdtsc
    shl rdx, 32
    or rax, rdx
    sub rax, rbx
    add TelemetryTotal, rax
    
    dec r12d
    jnz telemetry_loop
    
    ; --- Validate ---
    ; Delta = (TelemetryTotal - BaselineTotal) / TEST_ITERATIONS
    mov rax, TelemetryTotal
    sub rax, BaselineTotal
    mov rcx, TEST_ITERATIONS
    xor rdx, rdx
    div rcx
    
    ; Accept if avg delta < budget
    cmp rax, BUDGET_CYCLES
    ja test_fail
    
    ; Accept if no drops
    mov rax, TelemetryDropped
    test rax, rax
    jnz test_fail
    
    xor ecx, ecx
    call ExitProcess
    
test_fail:
    mov ecx, 1
    call ExitProcess
    
main ENDP

end
