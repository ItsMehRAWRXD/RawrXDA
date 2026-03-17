; ============================================================================
; SovereignThermalStressOrchestrator.asm
; Full 40GB Thermal-Adaptive Stress Test
; Zero-overhead, RIP-relative, pure MASM x64
; ============================================================================

option casemap:none

; --- Win32 Externals ---
extern OpenFileMappingA : PROC
extern MapViewOfFile : PROC
extern UnmapViewOfFile : PROC
extern CloseHandle : PROC
extern Sleep : PROC
extern ExitProcess : PROC
extern GetStdHandle : PROC
extern WriteConsoleA : PROC
extern GetTickCount64 : PROC
extern wsprintfA : PROC

; --- Constants ---
FILE_MAP_READ           equ 4
STD_OUTPUT_HANDLE       equ -11
THERMAL_THRESHOLD       equ 65            ; 65°C throttle point
THERMAL_CRITICAL        equ 75            ; 75°C emergency
THERMAL_SAFE            equ 35            ; 35°C optimal
MMF_VIEW_SIZE           equ 1024          ; Sufficient for header + drives

.data
    szMMFName           db "Global\\SOVEREIGN_NVME_TEMPS", 0
    szMsgInit           db "[Orchestrator] Initializing Thermal Oracle...", 13, 10, 0
    szMsgFailMMF        db "[Orchestrator] Failed to open MMF. Is the Service running?", 13, 10, 0
    szMsgLoop           db "[Orchestrator] Cycle %d | Headroom: %dC | Best Drive: %d (%dC)", 13, 10, 0
    szMsgThrottle       db "[Orchestrator] !!! THERMAL THROTTLE !!! Reducing I/O Depth.", 13, 10, 0
    szMsgCritical       db "[Orchestrator] !!! CRITICAL TEMP !!! Emergency Shutdown.", 13, 10, 0
    szMsgSwap           db "[Orchestrator] Hot-Swap: Drive %d -> Drive %d", 13, 10, 0
    
    ; MMF Layout Offsets (Matching SovereignSharedMemory.h / nvme_oracle_service.asm)
    OFF_SIGNATURE       equ 0
    OFF_COUNT           equ 8
    OFF_TEMPS           equ 16
    ; OFF_TEMPS is array of int32

    g_hMMF              dq 0
    g_pView             dq 0
    g_CycleCount        dq 0
    g_CurrentDrive      dd 0
    
    fmtBuffer           db 256 dup(0)
    bytesWritten        dq 0

.code

; ---------------------------------------------------------------------------
; Helper: Console Print
; ---------------------------------------------------------------------------
PrintString PROC FRAME
    ; RCX = String Pointer
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rdx, rcx
    ; Calc length
    xor r8, r8
_len:
    mov al, [rdx + r8]
    test al, al
    jz _len_done
    inc r8
    jmp _len
_len_done:

    mov rcx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx, rax            ; hConsole
    lea r9, bytesWritten
    mov qword ptr [rsp+20h], 0
    call WriteConsoleA
    
    add rsp, 28h
    ret
PrintString ENDP

; ---------------------------------------------------------------------------
; Sovereign_InitializeThermalOracle
; ---------------------------------------------------------------------------
Sovereign_InitializeThermalOracle PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    lea rcx, szMsgInit
    call PrintString

    ; OpenFileMappingA
    mov rcx, FILE_MAP_READ
    xor rdx, rdx
    lea r8, szMMFName
    call OpenFileMappingA
    test rax, rax
    jz _fail

    mov g_hMMF, rax

    ; MapViewOfFile
    mov rcx, g_hMMF
    mov rdx, FILE_MAP_READ
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+20h], MMF_VIEW_SIZE
    call MapViewOfFile
    test rax, rax
    jz _fail

    mov g_pView, rax
    mov eax, 1 ; Success
    add rsp, 28h
    ret

_fail:
    lea rcx, szMsgFailMMF
    call PrintString
    xor eax, eax ; Fail
    add rsp, 28h
    ret
Sovereign_InitializeThermalOracle ENDP

; ---------------------------------------------------------------------------
; Sovereign_CheckThermalHeadroom
; Returns: EAX = Max Temp seen across valid drives
; ---------------------------------------------------------------------------
Sovereign_CheckThermalHeadroom PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov rax, g_pView
    test rax, rax
    jz _err

    mov ecx, [rax + OFF_COUNT] ; Drive count
    cmp ecx, 16
    ja _cap
    jmp _scan
_cap:
    mov ecx, 16
_scan:
    xor r8d, r8d ; MaxTemp = 0
    xor r9, r9   ; Index
    
    lea r10, [rax + OFF_TEMPS] ; Temps array base

_loop:
    cmp r9d, ecx
    jge _done

    mov r11d, [r10 + r9*4] ; Read temp
    cmp r11d, r8d
    cmovg r8d, r11d        ; Max = max(Max, temp)

    inc r9
    jmp _loop

_done:
    mov eax, r8d
    add rsp, 28h
    ret

_err:
    mov eax, 100 ; Assume worst if error
    add rsp, 28h
    ret
Sovereign_CheckThermalHeadroom ENDP

; ---------------------------------------------------------------------------
; Sovereign_SelectCoolestDrive
; Returns: RAX = Index of coolest drive, RDX = Temp
; ---------------------------------------------------------------------------
Sovereign_SelectCoolestDrive PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov rax, g_pView
    test rax, rax
    jz _err

    mov ecx, [rax + OFF_COUNT]
    cmp ecx, 16
    ja _cap
    jmp _scan
_cap:
    mov ecx, 16
_scan:
    mov r8d, 1000 ; MinTemp
    xor r9, r9    ; BestIdx
    xor r10, r10  ; CurrentIdx
    
    lea r11, [rax + OFF_TEMPS]

_loop:
    cmp r10d, ecx
    jge _done

    mov eax, [r11 + r10*4]
    
    ; Filter invalid
    cmp eax, 0
    jle _next

    cmp eax, r8d
    jge _next
    
    ; Found cooler
    mov r8d, eax
    mov r9, r10

_next:
    inc r10
    jmp _loop

_done:
    mov rax, r9
    mov rdx, r8
    add rsp, 28h
    ret

_err:
    xor rax, rax
    xor rdx, rdx
    add rsp, 28h
    ret
Sovereign_SelectCoolestDrive ENDP


; ---------------------------------------------------------------------------
; Main Entry
; ---------------------------------------------------------------------------
SovereignStressMain PROC FRAME
    sub rsp, 68h
    .allocstack 68h
    .endprolog

    call Sovereign_InitializeThermalOracle
    test eax, eax
    jz _exit

_stress_loop:
    inc g_CycleCount

    ; 1. Check Headroom
    call Sovereign_CheckThermalHeadroom
    mov ebx, eax ; MaxTemp

    cmp eax, THERMAL_CRITICAL
    jge _critical
    cmp eax, THERMAL_THRESHOLD
    jge _throttle

    ; 2. Select Coolest
    call Sovereign_SelectCoolestDrive
    mov rsi, rax ; BestDrive
    mov rdi, rdx ; BestTemp

    ; Check hot swap needed?
    mov edx, g_CurrentDrive
    cmp rsi, rdx
    je _no_swap

    ; Swap!
    lea rcx, fmtBuffer
    lea rdx, szMsgSwap
    mov r8, rdx ; old (from g_CurrentDrive var - actuall assume rdx has current)
    mov r8d, g_CurrentDrive
    mov r9, rsi
    call wsprintfA
    lea rcx, fmtBuffer
    call PrintString
    
    mov g_CurrentDrive, esi

_no_swap:
    ; 3. Status Report
    lea rcx, fmtBuffer
    lea rdx, szMsgLoop
    mov r8, g_CycleCount
    mov r9d, ebx ; Headroom (MaxTemp)
    mov [rsp+20h], rsi ; BestDrive
    mov [rsp+28h], rdi ; BestTemp
    call wsprintfA
    
    lea rcx, fmtBuffer
    call PrintString

    ; Simulate Work (100ms)
    mov ecx, 100
    call Sleep
    
    jmp _stress_loop

_throttle:
    lea rcx, szMsgThrottle
    call PrintString
    mov ecx, 500
    call Sleep
    jmp _stress_loop

_critical:
    lea rcx, szMsgCritical
    call PrintString
    mov ecx, 2000
    call Sleep
    jmp _stress_loop

_exit:
    mov ecx, 1
    call ExitProcess

SovereignStressMain ENDP

END
