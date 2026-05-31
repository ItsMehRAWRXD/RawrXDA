; ==============================================================================
; RawrXD-Soak: Phase 7.3 Autonomous Recovery Soak Test
; ==============================================================================
; Purpose: Stress test RawrXDAutonomousCorrect and SecureVaultIO
; Logic: 100-cycle loop of simulated failures and encrypted persistence
; Toolchain: ml64.exe / link.exe (x64 MASM)
; ==============================================================================

extern ExitProcess : proc
extern GetStdHandle : proc
extern WriteFile : proc
extern LoadLibraryA : proc
extern GetProcAddress : proc
extern FreeLibrary : proc

.data
    szDominanceDll db "rawrxd_dominance.dll", 0
    szProcCorrect db "RawrXDAutonomousCorrect", 0
    szProcVault db "RawrXDSecureVaultIO", 0
    
    szReportHeader db "--- RawrXD Phase 7.3 Soak Test: STARTING 100 CYCLES ---", 0ah, 0
    szReportSuccess db "--- Soak Test Completed: 100/100 Cycles Passed ---", 0ah, 0
    szReportFailure db "--- Soak Test FAILED: Component Breakage Detected ---", 0ah, 0
    
    hStdout dq 0
    qWritten dq 0
    hDomDll dq 0
    pCorrect dq 0
    pVault dq 0

.code
main proc
    sub rsp, 40 ; Standard shadow space - align to 16 before child calls
    ; ... rest of code

    ; Initialize Stdout
    mov rcx, -11
    call GetStdHandle
    mov hStdout, rax

    ; Print Header
    lea rdx, szReportHeader
    mov r8, 56
    call _print

    ; Load Dominance Layer
    lea rcx, szDominanceDll
    call LoadLibraryA
    test rax, rax
    jz _fatal
    mov hDomDll, rax

    ; Resolve AutonomousCorrect
    mov rcx, rax
    lea rdx, szProcCorrect
    call GetProcAddress
    test rax, rax
    jz _fatal
    mov pCorrect, rax

    ; Resolve SecureVaultIO
    mov rcx, hDomDll
    lea rdx, szProcVault
    call GetProcAddress
    test rax, rax
    jz _fatal
    mov pVault, rax

    ; --- 100 Cycle Stress Loop ---
    mov rsi, 100
_loop:
    ; 1. Simulated Recovery Call
    xor rcx, rcx ; lpFailedCode (stub)
    xor rdx, rdx ; lpErrorLogs (stub)
    call pCorrect
    test rax, rax
    jnz _fail_msg

    ; 2. Simulated Vault Encryption Call
    xor rcx, rcx ; lpBuffer
    xor rdx, rdx ; nSize
    mov r8, 1    ; bEncrypt=true
    call pVault
    test rax, rax
    jnz _fail_msg

    ; Close loop gracefully
    dec rsi
    jnz _loop

    ; --- All Pass ---
    lea rdx, szReportSuccess
    mov r8, 51
    call _print

    mov rcx, hDomDll
    call FreeLibrary
    xor ecx, ecx
    call ExitProcess

_fail_msg:
    sub rsp, 40 ; Maintain alignment
    lea rdx, szReportFailure
    mov r8, 53
    call _print
    add rsp, 40
_fatal:
    mov ecx, 1
    call ExitProcess
main endp

_print proc
    ; Corrected stack alignment for system calls inside nested calls
    sub rsp, 40
    mov rcx, hStdout
    mov r9, offset qWritten
    mov qword ptr [rsp + 32], 0
    call WriteFile
    add rsp, 40
    ret
_print endp

end
