; ============================================================================
; SovereignThermalDemo.asm
; Safe Thermal Demonstration Mode (MASM x64)
; Controlled 30-second demo with simulated thermal cycling
; No real I/O stress - just thermal decision-making logic demonstration
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
DEMO_DURATION_MS        equ 30000         ; 30 seconds safe demo
CYCLE_INTERVAL_MS       equ 500           ; 500ms per cycle

; Demo simulation states
DEMO_STATE_NORMAL       equ 0
DEMO_STATE_HEATING      equ 1
DEMO_STATE_COOLING      equ 2
DEMO_STATE_THROTTLE     equ 3

.data
    szMMFName           db "Global\SOVEREIGN_NVME_TEMPS", 0
    szMsgBanner         db 13,10
                        db "======================================================", 13, 10
                        db "  SOVEREIGN THERMAL DEMO - Safe Mode", 13, 10
                        db "  Controlled 30-Second Thermal Demonstration", 13, 10
                        db "  No Real I/O Stress - Simulated Thermal Cycling", 13, 10
                        db "======================================================", 13, 10, 0
    szMsgInit           db "[Demo] Initializing Thermal Oracle...", 13, 10, 0
    szMsgFailMMF        db "[Demo] Failed to open MMF. Is the Service running?", 13, 10, 0
    szMsgStart          db "[Demo] Starting 30-second safe demonstration...", 13, 10, 0
    szMsgCycle          db "[Demo] Cycle %d | Max Temp: %dC | Coolest Drive: %d (%dC) | State: %s", 13, 10, 0
    szMsgNormal         db "NORMAL", 0
    szMsgHeating        db "HEATING", 0
    szMsgCooling        db "COOLING", 0
    szMsgThrottle       db "THROTTLE", 0
    szMsgComplete       db "[Demo] Demonstration complete! System is thermally aware.", 13, 10, 0
    szMsgSwap           db "[Demo] Hot-swap: Drive %d -> Drive %d", 13, 10, 0
    
    ; MMF Layout Offsets
    OFF_SIGNATURE       equ 0
    OFF_COUNT           equ 8
    OFF_TEMPS           equ 16
    
    g_hMMF              dq 0
    g_pView             dq 0
    g_CycleCount        dq 0
    g_CurrentDrive      dd 0
    g_DemoState         dd DEMO_STATE_NORMAL
    g_SimulatedTemp     dd 40          ; Start at safe temperature
    g_StartTime         dq 0           ; Demo start timestamp
    
    fmtBuffer           db 256 dup(0)
    bytesWritten        dq 0
    stateBuffer         db 32 dup(0)

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
; Demo_InitializeThermalOracle
; ---------------------------------------------------------------------------
Demo_InitializeThermalOracle PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    lea rcx, szMsgBanner
    call PrintString
    
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
Demo_InitializeThermalOracle ENDP

; ---------------------------------------------------------------------------
; Demo_CheckThermalHeadroom
; Returns: EAX = Max Temp seen across valid drives
; ---------------------------------------------------------------------------
Demo_CheckThermalHeadroom PROC FRAME
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
Demo_CheckThermalHeadroom ENDP

; ---------------------------------------------------------------------------
; Demo_SelectCoolestDrive
; Returns: RAX = Index of coolest drive, RDX = Temp
; ---------------------------------------------------------------------------
Demo_SelectCoolestDrive PROC FRAME
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
Demo_SelectCoolestDrive ENDP

; ---------------------------------------------------------------------------
; Demo_SimulateThermalCycle
; Updates g_SimulatedTemp based on current state
; ---------------------------------------------------------------------------
Demo_SimulateThermalCycle PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov eax, g_DemoState
    
    ; State machine for thermal simulation
    cmp eax, DEMO_STATE_NORMAL
    je _state_normal
    cmp eax, DEMO_STATE_HEATING
    je _state_heating
    cmp eax, DEMO_STATE_COOLING
    je _state_cooling
    cmp eax, DEMO_STATE_THROTTLE
    je _state_throttle
    
    ; Default to normal
    mov g_DemoState, DEMO_STATE_NORMAL
    jmp _done

_state_normal:
    ; Slowly heat up from idle
    mov eax, g_SimulatedTemp
    cmp eax, 45
    jge _start_heating
    add eax, 2
    mov g_SimulatedTemp, eax
    jmp _done

_start_heating:
    mov g_DemoState, DEMO_STATE_HEATING
    jmp _done

_state_heating:
    ; Rapidly heat up
    mov eax, g_SimulatedTemp
    cmp eax, 65
    jge _start_throttle
    add eax, 5
    mov g_SimulatedTemp, eax
    jmp _done

_start_throttle:
    mov g_DemoState, DEMO_STATE_THROTTLE
    jmp _done

_state_throttle:
    ; System throttles, start cooling
    mov eax, g_SimulatedTemp
    cmp eax, 50
    jle _start_cooling
    sub eax, 3
    mov g_SimulatedTemp, eax
    jmp _done

_start_cooling:
    mov g_DemoState, DEMO_STATE_COOLING
    jmp _done

_state_cooling:
    ; Cool down to safe levels
    mov eax, g_SimulatedTemp
    cmp eax, 35
    jle _back_to_normal
    sub eax, 4
    mov g_SimulatedTemp, eax
    jmp _done

_back_to_normal:
    mov g_DemoState, DEMO_STATE_NORMAL
    mov g_SimulatedTemp, 35

_done:
    add rsp, 28h
    ret
Demo_SimulateThermalCycle ENDP

; ---------------------------------------------------------------------------
; Demo_GetStateString
; Returns: RCX = State string pointer
; ---------------------------------------------------------------------------
Demo_GetStateString PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov eax, g_DemoState
    
    cmp eax, DEMO_STATE_NORMAL
    je _normal
    cmp eax, DEMO_STATE_HEATING
    je _heating
    cmp eax, DEMO_STATE_COOLING
    je _cooling
    cmp eax, DEMO_STATE_THROTTLE
    je _throttle
    
    lea rcx, szMsgNormal
    jmp _done

_normal:
    lea rcx, szMsgNormal
    jmp _done

_heating:
    lea rcx, szMsgHeating
    jmp _done

_cooling:
    lea rcx, szMsgCooling
    jmp _done

_throttle:
    lea rcx, szMsgThrottle

_done:
    add rsp, 28h
    ret
Demo_GetStateString ENDP

; ---------------------------------------------------------------------------
; Main Entry
; ---------------------------------------------------------------------------
DemoMain PROC FRAME
    sub rsp, 68h
    .allocstack 68h
    .endprolog

    call Demo_InitializeThermalOracle
    test eax, eax
    jz _exit

    lea rcx, szMsgStart
    call PrintString

    ; Record demo start time for duration tracking
    call GetTickCount64
    mov g_StartTime, rax

_demo_loop:
    inc g_CycleCount

    ; 1. Check real thermal headroom (from actual drives)
    call Demo_CheckThermalHeadroom
    mov ebx, eax ; MaxTemp

    ; 2. Simulate thermal cycle for demonstration
    call Demo_SimulateThermalCycle

    ; 3. Select coolest drive based on real temperatures
    call Demo_SelectCoolestDrive
    mov rsi, rax ; BestDrive
    mov rdi, rdx ; BestTemp

    ; 4. Check if hot-swap is needed
    mov edx, g_CurrentDrive
    cmp rsi, rdx
    je _no_swap

    ; Perform swap!
    lea rcx, fmtBuffer
    lea rdx, szMsgSwap
    mov r8d, g_CurrentDrive
    mov r9, rsi
    call wsprintfA
    lea rcx, fmtBuffer
    call PrintString
    
    mov g_CurrentDrive, esi

_no_swap:
    ; 5. Get state string for display
    call Demo_GetStateString
    mov r12, rcx ; Save state string

    ; 6. Status report
    lea rcx, fmtBuffer
    lea rdx, szMsgCycle
    mov r8, g_CycleCount
    mov r9d, ebx ; MaxTemp (real)
    mov [rsp+20h], rsi ; BestDrive
    mov [rsp+28h], rdi ; BestTemp
    mov [rsp+30h], r12 ; State string
    call wsprintfA
    
    lea rcx, fmtBuffer
    call PrintString

    ; 7. Check if demo duration complete
    call GetTickCount64
    sub rax, g_StartTime
    cmp rax, DEMO_DURATION_MS
    jge _complete

    ; 8. Sleep for cycle interval
    mov ecx, CYCLE_INTERVAL_MS
    call Sleep
    
    jmp _demo_loop

_complete:
    lea rcx, szMsgComplete
    call PrintString

_exit:
    mov ecx, 0
    call ExitProcess

DemoMain ENDP

; ---------------------------------------------------------------------------
; Entry Point
; ---------------------------------------------------------------------------
main PROC
    sub rsp, 28h
    call DemoMain
    add rsp, 28h
    ret
main ENDP

END
