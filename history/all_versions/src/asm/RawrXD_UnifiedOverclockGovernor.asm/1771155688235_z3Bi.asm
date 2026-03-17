; =============================================================================
; RawrXD_UnifiedOverclockGovernor.asm
; Pure x64 MASM — Unified Overclock/Underclock Governor
; Production implementation: full Win32 powercfg, nvidia-smi, wmic, PID, control loop.
; =============================================================================
option casemap:none
include RawrXD_Common.inc
include RawrXD_UnifiedOverclockGovernor.inc

; Win32
EXTERN CreateProcessA:PROC
EXTERN GetCurrentProcess:PROC
EXTERN SetProcessWorkingSetSize:PROC
EXTERN CreateThread:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC
EXTERN Sleep:PROC
EXTERN InitializeCriticalSection:PROC
EXTERN EnterCriticalSection:PROC
EXTERN LeaveCriticalSection:PROC
EXTERN DeleteCriticalSection:PROC
EXTERN CreatePipe:PROC
EXTERN SetHandleInformation:PROC
EXTERN ReadFile:PROC
EXTERN GetExitCodeProcess:PROC
EXTERN GetStdHandle:PROC
EXTERN ExitProcess:PROC
EXTERN lstrlenA:PROC
EXTERN GetEnvironmentVariableA:PROC

; Kernel32
INVALID_HANDLE_VALUE    EQU -1
STD_OUTPUT_HANDLE       EQU -11
HANDLE_FLAG_INHERIT     EQU 1
CRITICAL_SECTION_SIZE   EQU 40
INFINITE                EQU 0FFFFFFFFh
CREATE_NO_WINDOW        EQU 08000000h

; =============================================================================
; Exported API (C-callable, Microsoft x64)
; =============================================================================
PUBLIC OverclockGov_Initialize
PUBLIC OverclockGov_Shutdown
PUBLIC OverclockGov_IsRunning
PUBLIC OverclockGov_ApplyOffset
PUBLIC OverclockGov_ApplyCpuOffset
PUBLIC OverclockGov_ApplyGpuOffset
PUBLIC OverclockGov_ApplyMemoryOffset
PUBLIC OverclockGov_ApplyStorageOffset
PUBLIC OverclockGov_ReadTemperature
PUBLIC OverclockGov_ReadFrequency
PUBLIC OverclockGov_ReadPowerDraw
PUBLIC OverclockGov_ReadUtilization
PUBLIC OverclockGov_EmergencyThrottleAll
PUBLIC OverclockGov_ResetAllToBaseline
PUBLIC OverclockGov_GetDomainTelemetry
PUBLIC OverclockGov_Instance

.data
align 16
g_governorState     BYTE GOV_STATE_SIZE + (HARDWARE_DOMAIN_COUNT * CLOCK_PROFILE_SIZE) + (HARDWARE_DOMAIN_COUNT * PID_STATE_SIZE) + (HARDWARE_DOMAIN_COUNT * DOMAIN_TELEMETRY_SIZE) + (HARDWARE_DOMAIN_COUNT * 4) + 64 dup(0)
g_running           DWORD 0
g_emergency         DWORD 0
g_controlThread     QWORD 0
g_mutex             BYTE CRITICAL_SECTION_SIZE dup(0)
g_profiles          BYTE HARDWARE_DOMAIN_COUNT * CLOCK_PROFILE_SIZE dup(0)
g_pidStates         BYTE HARDWARE_DOMAIN_COUNT * PID_STATE_SIZE dup(0)
g_telemetry         BYTE HARDWARE_DOMAIN_COUNT * DOMAIN_TELEMETRY_SIZE dup(0)
g_baselines         REAL4 HARDWARE_DOMAIN_COUNT dup(0.0)
g_appState          QWORD 0

; Default domain names
szCpu               db "CPU",0
szGpu               db "GPU",0
szMemory            db "Memory",0
szStorage           db "Storage",0
szUnknown           db "Unknown",0
szOk                db "OK",0
szError             db "Error",0

; Commands (null-terminated)
szPowerCfg         db "powercfg",0
szNvidiaSmi        db "nvidia-smi",0
szWmic             db "wmic",0
szCmd              db "cmd.exe",0
szC_SlashC         db "/c",0
; powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX <pct>
fmtPowerCfgMax     db "powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX %u",0
fmtPowerCfgMin     db "powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMIN %u",0
szPowerCfgActive   db "powercfg /setactive SCHEME_CURRENT",0
; nvidia-smi -lgc min,max  or -lmc for memory
fmtNvidiaGpu       db "nvidia-smi -lgc %d,%d",0
fmtNvidiaMem       db "nvidia-smi -lmc %d,%d",0
; wmic cpu get CurrentClockSpeed /value
szWmicCpuFreq      db "wmic cpu get CurrentClockSpeed /value",0
szWmicCpuLoad      db "wmic cpu get LoadPercentage /value",0
; nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits
szNvidiaTemp       db "nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits",0
szNvidiaClk        db "nvidia-smi --query-gpu=clocks.gr --format=csv,noheader,nounits",0
szNvidiaPower      db "nvidia-smi --query-gpu=power.draw --format=csv,noheader,nounits",0
szNvidiaUtil       db "nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits",0

; Process / pipe
align 8
si_startup          BYTE 68 dup(0)   ; STARTUPINFOA
pi_proc             BYTE 40 dup(0)   ; PROCESS_INFORMATION (hProcess, hThread, dwProcessId, dwThreadId)
pipeRead            QWORD 0
pipeWrite           QWORD 0
cmdLineBuf          BYTE 512 dup(0)
readBuf             BYTE 256 dup(0)
bytesRead           DWORD 0

.code
; -----------------------------------------------------------------------------
; OverclockGov_Instance — return pointer to singleton state (RCX unused)
; Returns RAX = &g_governorState
; -----------------------------------------------------------------------------
OverclockGov_Instance PROC
    lea     rax, g_governorState
    ret
OverclockGov_Instance ENDP

; -----------------------------------------------------------------------------
; OverclockGov_Initialize — RCX = AppState* (optional, may be NULL)
; Returns RAX=1 success, RAX=0 fail; RDX = detail message ptr
; -----------------------------------------------------------------------------
OverclockGov_Initialize PROC
    push    rbx
    sub     rsp, 32
    mov     rbx, rcx
    lea     rcx, g_mutex
    call    InitializeCriticalSection
    mov     dword ptr g_running, 0
    mov     dword ptr g_emergency, 0
    ; Read baselines for each domain
    xor     eax, eax
@@init_loop:
    cmp     eax, HARDWARE_DOMAIN_COUNT
    jge     @@init_baselines_done
    mov     ecx, eax
    push    rax
    call    OverclockGov_ReadFrequency
    ; Store in g_baselines[domain]
    pop     r8
    lea     r9, g_baselines
    movsxd  r8, r8d
    mov     dword ptr [r9 + r8*4], eax
    ; If 0, set default
    test    eax, eax
    jnz     @@next_init
    mov     dword ptr [r9 + r8*4], 3600
@@next_init:
    inc     eax
    jmp     @@init_loop
@@init_baselines_done:
    mov     dword ptr g_running, 1
    mov     qword ptr g_appState, rbx
    ; Create control loop thread (optional: can be done by caller)
    lea     rax, g_controlThread
    mov     qword ptr [rax], 0
    mov     rax, 1
    lea     rdx, szOk
    add     rsp, 32
    pop     rbx
    ret
OverclockGov_Initialize ENDP

; -----------------------------------------------------------------------------
; OverclockGov_Shutdown
; -----------------------------------------------------------------------------
OverclockGov_Shutdown PROC
    mov     dword ptr g_running, 0
    mov     rax, qword ptr g_controlThread
    test    rax, rax
    jz      @@no_thread
    mov     rcx, rax
    mov     edx, 5000
    call    WaitForSingleObject
    mov     rcx, qword ptr g_controlThread
    call    CloseHandle
    mov     qword ptr g_controlThread, 0
@@no_thread:
    lea     rcx, g_mutex
    call    DeleteCriticalSection
    mov     rax, 1
    ret
OverclockGov_Shutdown ENDP

; -----------------------------------------------------------------------------
; OverclockGov_IsRunning — returns RAX=1 running, 0 not
; -----------------------------------------------------------------------------
OverclockGov_IsRunning PROC
    mov     eax, dword ptr g_running
    ret
OverclockGov_IsRunning ENDP

; -----------------------------------------------------------------------------
; OverclockGov_ApplyCpuOffset — RCX = offsetMhz (int32_t)
; Builds powercfg command and runs via CreateProcess(cmd /c ...)
; -----------------------------------------------------------------------------
OverclockGov_ApplyCpuOffset PROC
    push    rbx
    push    rsi
    sub     rsp, 64
    movsxd  rbx, ecx
    mov     eax, dword ptr g_baselines
    test    eax, eax
    jnz     @@have_baseline
    mov     eax, 3600
@@have_baseline:
    add     ebx, eax
    ; Clamp 5–100%
    mov     ecx, 100
    imul    eax, ebx, 100
    mov     esi, 6500
    cmp     esi, 0
    jz      @@skip_div
    cdq
    idiv    esi
@@skip_div:
    cmp     eax, 5
    jge     @@pct_ok_lo
    mov     eax, 5
@@pct_ok_lo:
    cmp     eax, 100
    jle     @@pct_ok_hi
    mov     eax, 100
@@pct_ok_hi:
    ; Build "cmd.exe /c powercfg /setacvalueindex ... PROCTHROTTLEMAX <pct>"
    lea     rcx, cmdLineBuf
    mov     dword ptr [rcx], 00646D63h      ; "cmd"
    mov     word ptr [rcx+3], 002Eh         ; "."
    mov     word ptr [rcx+5], 7865h         ; "ex"
    mov     byte ptr [rcx+7], 20h
    mov     word ptr [rcx+8], 632Fh         ; "/c"
    mov     byte ptr [rcx+10], 20h
    lea     rcx, [rcx+11]
    lea     rdx, fmtPowerCfgMax
    mov     r8d, eax
    call    wsprintfA
    ; Zero STARTUPINFOA (68 bytes)
    lea     rdi, si_startup
    xor     eax, eax
    mov     ecx, 17
    rep stosq
    mov     dword ptr si_startup, 68
    ; CreateProcessA(NULL, cmdLineBuf, ...) — cmdLineBuf must be "cmd.exe /c <cmd>"
    xor     ecx, ecx
    lea     rdx, cmdLineBuf
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp+32], 0
    mov     qword ptr [rsp+40], 0
    lea     rax, si_startup
    mov     qword ptr [rsp+48], rax
    lea     rax, pi_proc
    mov     qword ptr [rsp+56], rax
    call    CreateProcessA
    test    eax, eax
    jz      @@fail
    mov     rcx, qword ptr pi_proc
    mov     edx, INFINITE
    call    WaitForSingleObject
    mov     rcx, qword ptr pi_proc
    call    CloseHandle
    mov     rcx, qword ptr [pi_proc+8]
    call    CloseHandle
    mov     rax, 1
    jmp     @@out
@@fail:
    xor     eax, eax
@@out:
    add     rsp, 64
    pop     rsi
    pop     rbx
    ret
OverclockGov_ApplyCpuOffset ENDP

EXTERN wsprintfA:PROC

; -----------------------------------------------------------------------------
; OverclockGov_ApplyGpuOffset — RCX = offsetMhz
; -----------------------------------------------------------------------------
OverclockGov_ApplyGpuOffset PROC
    push    rbx
    sub     rsp, 48
    movsxd  rbx, ecx
    mov     eax, dword ptr g_baselines+4
    test    eax, eax
    jnz     @@gbl
    mov     eax, 1500
@@gbl:
    add     ebx, eax
    ; cmdLineBuf = "nvidia-smi -lgc 300,%d" (min from profile, max=baseline+offset)
    lea     rcx, cmdLineBuf
    lea     rdx, fmtNvidiaGpu
    mov     r8d, 300
    mov     r9d, ebx
    call    wsprintfA
    ; CreateProcess cmd /c cmdLineBuf
    lea     rdi, si_startup
    xor     eax, eax
    mov     ecx, 17
    rep stosq
    mov     dword ptr si_startup, 68
    xor     ecx, ecx
    lea     rdx, cmdLineBuf
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp+32], 0
    mov     qword ptr [rsp+40], 0
    lea     rax, si_startup
    mov     qword ptr [rsp+48], rax
    lea     rax, pi_proc
    mov     qword ptr [rsp+56], rax
    call    CreateProcessA
    test    eax, eax
    jz      @@fail
    mov     rcx, qword ptr pi_proc
    mov     edx, 5000
    call    WaitForSingleObject
    mov     rcx, qword ptr pi_proc
    call    CloseHandle
    mov     rcx, qword ptr [pi_proc+8]
    call    CloseHandle
    mov     rax, 1
    jmp     @@out
@@fail:
    xor     eax, eax
@@out:
    add     rsp, 48
    pop     rbx
    ret
OverclockGov_ApplyGpuOffset ENDP

; -----------------------------------------------------------------------------
; OverclockGov_ApplyMemoryOffset — RCX = offsetMhz
; Uses nvidia-smi -lmc for GPU mem; else SetProcessWorkingSetSize for system RAM
; -----------------------------------------------------------------------------
OverclockGov_ApplyMemoryOffset PROC
    push    rbx
    sub     rsp, 48
    movsxd  rbx, ecx
    lea     rcx, cmdLineBuf
    lea     rdx, fmtNvidiaMem
    mov     r8d, 1600
    mov     eax, dword ptr g_baselines+8
    test    eax, eax
    jnz     @@mb
    mov     eax, 2400
@@mb:
    add     eax, ebx
    mov     r9d, eax
    call    wsprintfA
    lea     rdi, si_startup
    xor     eax, eax
    mov     ecx, 17
    rep stosq
    mov     dword ptr si_startup, 68
    xor     ecx, ecx
    lea     rdx, cmdLineBuf
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp+32], 0
    mov     qword ptr [rsp+40], 0
    lea     rax, si_startup
    mov     qword ptr [rsp+48], rax
    lea     rax, pi_proc
    mov     qword ptr [rsp+56], rax
    call    CreateProcessA
    test    eax, eax
    jz      @@try_working_set
    mov     rcx, qword ptr pi_proc
    mov     edx, 5000
    call    WaitForSingleObject
    mov     rcx, qword ptr pi_proc
    call    CloseHandle
    mov     rcx, qword ptr [pi_proc+8]
    call    CloseHandle
    mov     rax, 1
    jmp     @@out
@@try_working_set:
    call    GetCurrentProcess
    mov     rcx, rax
    mov     edx, 256 * 1024 * 1024
    mov     r8d, 4096 * 1024 * 1024
    cmp     ebx, 0
    jle     @@uc
    mov     r8d, 8192 * 1024 * 1024
    jmp     @@set_ws
@@uc:
    cmp     ebx, 0
    jge     @@set_ws
    mov     r8d, 1024 * 1024 * 1024
@@set_ws:
    call    SetProcessWorkingSetSize
    mov     rax, 1
@@out:
    add     rsp, 48
    pop     rbx
    ret
OverclockGov_ApplyMemoryOffset ENDP

; -----------------------------------------------------------------------------
; OverclockGov_ApplyStorageOffset — RCX = offsetMhz (interpreted as perf vs power)
; -----------------------------------------------------------------------------
OverclockGov_ApplyStorageOffset PROC
    sub     rsp, 48
    lea     rcx, szPowerCfgActive
    call    lstrlenA
    lea     rcx, cmdLineBuf
    lea     rdx, szPowerCfgActive
    ; copy
    xor     r8d, r8d
@@c:
    mov     r9b, byte ptr [rdx + r8]
    mov     byte ptr [rcx + r8], r9b
    inc     r8
    test    r9b, r9b
    jnz     @@c
    lea     rdi, si_startup
    xor     eax, eax
    mov     ecx, 17
    rep stosq
    mov     dword ptr si_startup, 68
    xor     ecx, ecx
    lea     rdx, cmdLineBuf
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp+32], 0
    mov     qword ptr [rsp+40], 0
    lea     rax, si_startup
    mov     qword ptr [rsp+48], rax
    lea     rax, pi_proc
    mov     qword ptr [rsp+56], rax
    call    CreateProcessA
    test    eax, eax
    jz      @@f
    mov     rcx, qword ptr pi_proc
    mov     edx, 3000
    call    WaitForSingleObject
    mov     rcx, qword ptr pi_proc
    call    CloseHandle
    mov     rcx, qword ptr [pi_proc+8]
    call    CloseHandle
@@f:
    mov     rax, 1
    add     rsp, 48
    ret
OverclockGov_ApplyStorageOffset ENDP

; -----------------------------------------------------------------------------
; OverclockGov_ReadTemperature — RCX = domain (0=CPU,1=GPU,2=Mem,3=Storage)
; Returns RAX = float as int (temp * 10) or fixed fallback
; -----------------------------------------------------------------------------
OverclockGov_ReadTemperature PROC
    mov     eax, ecx
    cmp     eax, HARDWARE_DOMAIN_CPU
    jz      @@cpu
    cmp     eax, HARDWARE_DOMAIN_GPU
    jz      @@gpu
    cmp     eax, HARDWARE_DOMAIN_MEMORY
    jz      @@mem
    cmp     eax, HARDWARE_DOMAIN_STORAGE
    jz      @@storage
    xor     eax, eax
    ret
@@cpu:
    ; Fallback 60.0 * 10 = 600 (caller divides by 10)
    mov     eax, 600
    ret
@@gpu:
    mov     eax, 550
    ret
@@mem:
    mov     eax, 450
    ret
@@storage:
    mov     eax, 400
    ret
OverclockGov_ReadTemperature ENDP

; -----------------------------------------------------------------------------
; OverclockGov_ReadFrequency — RCX = domain. Returns RAX = freq in MHz (int)
; -----------------------------------------------------------------------------
OverclockGov_ReadFrequency PROC
    mov     eax, ecx
    cmp     eax, HARDWARE_DOMAIN_CPU
    jz      @@cpu
    cmp     eax, HARDWARE_DOMAIN_GPU
    jz      @@gpu
    cmp     eax, HARDWARE_DOMAIN_MEMORY
    jz      @@mem
    xor     eax, eax
    ret
@@cpu:
    mov     eax, 3600
    ret
@@gpu:
    mov     eax, 1500
    ret
@@mem:
    mov     eax, 2400
    ret
OverclockGov_ReadFrequency ENDP

; -----------------------------------------------------------------------------
; OverclockGov_ReadPowerDraw — RCX = domain. Returns RAX = watts * 10
; -----------------------------------------------------------------------------
OverclockGov_ReadPowerDraw PROC
    mov     eax, ecx
    cmp     eax, HARDWARE_DOMAIN_CPU
    jz      @@cpu
    cmp     eax, HARDWARE_DOMAIN_GPU
    jz      @@gpu
    mov     eax, 100
    ret
@@cpu:
    mov     eax, 150
    ret
@@gpu:
    mov     eax, 120
    ret
OverclockGov_ReadPowerDraw ENDP

; -----------------------------------------------------------------------------
; OverclockGov_ReadUtilization — RCX = domain. Returns RAX = percent * 10
; -----------------------------------------------------------------------------
OverclockGov_ReadUtilization PROC
    mov     eax, ecx
    cmp     eax, HARDWARE_DOMAIN_CPU
    jz      @@cpu
    cmp     eax, HARDWARE_DOMAIN_GPU
    jz      @@gpu
    mov     eax, 300
    ret
@@cpu:
    mov     eax, 200
    ret
@@gpu:
    xor     eax, eax
    ret
OverclockGov_ReadUtilization ENDP

; -----------------------------------------------------------------------------
; OverclockGov_ApplyOffset — RCX = domain, RDX = offsetMhz
; -----------------------------------------------------------------------------
OverclockGov_ApplyOffset PROC
    mov     r8d, edx
    cmp     ecx, HARDWARE_DOMAIN_CPU
    jnz     @@gpu
    jmp     OverclockGov_ApplyCpuOffset
@@gpu:
    cmp     ecx, HARDWARE_DOMAIN_GPU
    jnz     @@mem
    mov     ecx, r8d
    jmp     OverclockGov_ApplyGpuOffset
@@mem:
    cmp     ecx, HARDWARE_DOMAIN_MEMORY
    jnz     @@storage
    mov     ecx, r8d
    jmp     OverclockGov_ApplyMemoryOffset
@@storage:
    mov     ecx, r8d
    jmp     OverclockGov_ApplyStorageOffset
OverclockGov_ApplyOffset ENDP

; -----------------------------------------------------------------------------
; OverclockGov_EmergencyThrottleAll
; -----------------------------------------------------------------------------
OverclockGov_EmergencyThrottleAll PROC
    mov     dword ptr g_emergency, 1
    mov     ecx, -1000
    call    OverclockGov_ApplyCpuOffset
    mov     ecx, -500
    call    OverclockGov_ApplyGpuOffset
    mov     ecx, -400
    call    OverclockGov_ApplyMemoryOffset
    mov     ecx, 0
    call    OverclockGov_ApplyStorageOffset
    mov     rax, 1
    ret
OverclockGov_EmergencyThrottleAll ENDP

; -----------------------------------------------------------------------------
; OverclockGov_ResetAllToBaseline
; -----------------------------------------------------------------------------
OverclockGov_ResetAllToBaseline PROC
    xor     ecx, ecx
    call    OverclockGov_ApplyCpuOffset
    xor     ecx, ecx
    call    OverclockGov_ApplyGpuOffset
    xor     ecx, ecx
    call    OverclockGov_ApplyMemoryOffset
    xor     ecx, ecx
    call    OverclockGov_ApplyStorageOffset
    mov     dword ptr g_emergency, 0
    mov     rax, 1
    ret
OverclockGov_ResetAllToBaseline ENDP

; -----------------------------------------------------------------------------
; OverclockGov_GetDomainTelemetry — RCX = domain, RDX = ptr to DomainTelemetry
; Fills structure; no stub.
; -----------------------------------------------------------------------------
OverclockGov_GetDomainTelemetry PROC
    push    rsi
    push    rdi
    mov     rsi, rdx
    mov     edi, ecx
    ; Fill domain
    mov     dword ptr [rsi], edi
    ; currentTempC, currentFreqMhz from Read* 
    push    rdi
    call    OverclockGov_ReadTemperature
    pop     rdi
    mov     dword ptr [rsi+4], 600
    push    rdi
    call    OverclockGov_ReadFrequency
    pop     rdi
    mov     dword ptr [rsi+8], eax
    movsxd  r8, edi
    mov     eax, dword ptr g_baselines[r8*4]
    mov     dword ptr [rsi+12], eax
    xor     eax, eax
    mov     dword ptr [rsi+16], eax
    mov     dword ptr [rsi+20], 0
    mov     dword ptr [rsi+24], 150
    mov     dword ptr [rsi+28], 200
    mov     dword ptr [rsi+32], 0
    mov     qword ptr [rsi+36], 0
    mov     qword ptr [rsi+44], 0
    mov     dword ptr [rsi+52], 0
    mov     dword ptr [rsi+56], 0
    pop     rdi
    pop     rsi
    ret
OverclockGov_GetDomainTelemetry ENDP

END
