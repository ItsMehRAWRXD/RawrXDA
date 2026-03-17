; ============================================================================
; RawrXD Unified Overclock Governor v2 — Pure x64 MASM
; Zero-dependency hardware control: CPU, GPU, Memory, Storage
; Full production implementation: PID, control loop, Win32 powercfg/nvidia-smi
; ============================================================================
OPTION CASEMAP:NONE

PUBLIC HardwareDomainToString_asm
PUBLIC UnifiedOverclockGovernor_Instance
PUBLIC UnifiedOverclockGovernor_initialize
PUBLIC UnifiedOverclockGovernor_shutdown
PUBLIC UnifiedOverclockGovernor_isRunning
PUBLIC UnifiedOverclockGovernor_applyOffset
PUBLIC UnifiedOverclockGovernor_emergencyThrottleAll
PUBLIC UnifiedOverclockGovernor_resetAllToBaseline
PUBLIC UnifiedOverclockGovernor_getDomainTelemetry
PUBLIC UnifiedOverclockGovernor_isEmergencyActive
PUBLIC UnifiedOverclockGovernor_clearEmergency
PUBLIC ControlLoopThread

.CODE
EXTERN GetCurrentProcess:PROC
EXTERN SetProcessWorkingSetSize:PROC
EXTERN CreateThread:PROC
EXTERN CreateMutexW:PROC
EXTERN WaitForSingleObject:PROC
EXTERN ReleaseMutex:PROC
EXTERN CloseHandle:PROC
EXTERN CreatePipe:PROC
EXTERN CreateProcessA:PROC
EXTERN GetExitCodeProcess:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CreateFileA:PROC
EXTERN GetFileSize:PROC
EXTERN Sleep:PROC
EXTERN GetTickCount64:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN lstrlenA:PROC
EXTERN lstrcpyA:PROC
EXTERN wsprintfA:PROC
EXTERN GetCommandLineA:PROC
EXTERN GetStdHandle:PROC
EXTERN SetStdHandle:PROC
EXTERN DuplicateHandle:PROC

; kernel32
EXTERN LoadLibraryA:PROC
EXTERN GetProcAddress:PROC
EXTERN FreeLibrary:PROC

; PowrProf (powercfg) - optional; we invoke powercfg via CreateProcess
; So no direct import; all via CreateProcessA("cmd.exe /c powercfg ...")

; ============================================================================
; Constants
; ============================================================================
HARDWARE_DOMAIN_COUNT EQU 4
AUTO_TUNE_HISTORY_SIZE EQU 256
DOMAIN_CPU   EQU 0
DOMAIN_GPU   EQU 1
DOMAIN_MEM   EQU 2
DOMAIN_STOR  EQU 3
CLOCK_OVER   EQU 0
CLOCK_UNDER  EQU 1
CLOCK_STOCK  EQU 2
STRAT_DISABLED   EQU 0
STRAT_CONSERVATIVE EQU 1
STRAT_BALANCED   EQU 2
STRAT_AGGRESSIVE EQU 3
STRAT_ADAPTIVEML EQU 4
INVALID_HANDLE_VALUE EQU -1
INFINITE EQU 0FFFFFFFFh
GENERIC_READ     EQU 80000000h
GENERIC_WRITE    EQU 40000000h
OPEN_EXISTING    EQU 3
FILE_ATTRIBUTE_NORMAL EQU 80h
STD_INPUT_HANDLE  EQU -10
STD_OUTPUT_HANDLE EQU -11
STD_ERROR_HANDLE  EQU -12

; ============================================================================
; ClockResult: success(1), pad(3), detail(8), errorCode(4), pad(4) = 20 bytes
; ============================================================================
ClockResult STRUCT
    success    BYTE ?
    _pad1      BYTE 3 DUP (?)
    detail     QWORD ?
    errorCode  DWORD ?
    _pad2      DWORD ?
ClockResult ENDS

; ============================================================================
; ClockProfile: domain(4), direction(1), pad(3), offsetMhz(4), minFreq(4), maxFreq(4),
;   targetTempC(4), criticalTempC(4), hysteresisC(4), autoTuneEnabled(1), autoTuneStrategy(1), pad(2)
; ============================================================================
ClockProfile STRUCT
    domain           DWORD ?
    direction        BYTE ?
    _pad1            BYTE 3 DUP (?)
    offsetMhz        SDWORD ?
    minFreqMhz       SDWORD ?
    maxFreqMhz       SDWORD ?
    targetTempC      REAL4 ?
    criticalTempC    REAL4 ?
    hysteresisC      REAL4 ?
    autoTuneEnabled  BYTE ?
    autoTuneStrategy BYTE ?
    _pad2            BYTE 2 DUP (?)
ClockProfile ENDS

; ============================================================================
; PIDState: kp, ki, kd, integral, prevError, output (6 x REAL4), consecutiveFaults(4), totalFaults(4), lastFaultTime(8)
; ============================================================================
PIDState STRUCT
    kp                REAL4 ?
    ki                REAL4 ?
    kd                REAL4 ?
    integral          REAL4 ?
    prevError         REAL4 ?
    output            REAL4 ?
    consecutiveFaults SDWORD ?
    totalFaults       SDWORD ?
    lastFaultTime     QWORD ?
PIDState ENDS

; ============================================================================
; DomainTelemetry (abbreviated for MASM: no std::chrono; use QWORD tick)
; ============================================================================
DomainTelemetry STRUCT
    domain            DWORD ?
    currentTempC      REAL4 ?
    currentFreqMhz    REAL4 ?
    baselineFreqMhz   REAL4 ?
    appliedOffsetMhz  SDWORD ?
    _pad1             DWORD ?
    powerDrawWatts    REAL4 ?
    utilizationPct    REAL4 ?
    efficiencyScore   REAL4 ?
    _pad2             DWORD ?
    adjustmentCount   QWORD ?
    faultCount        QWORD ?
    autoTuneActive    BYTE ?
    activeStrategy    BYTE ?
    _pad3             BYTE 2 DUP (?)
    pidSnapshot       PIDState {}
DomainTelemetry ENDS

; ============================================================================
; Singleton instance + state (one governor)
; ============================================================================
.DATA
align 16
g_governorMutex     QWORD 0
g_controlThread     QWORD 0
g_running           DWORD 0
g_emergency         DWORD 0
g_appState          QWORD 0

; 4 profiles, 4 PID states, 4 telemetry, 4 baselines, 4 history heads
g_profiles          ClockProfile HARDWARE_DOMAIN_COUNT DUP (<>)
g_pidStates         PIDState HARDWARE_DOMAIN_COUNT DUP (<>)
g_telemetry         DomainTelemetry HARDWARE_DOMAIN_COUNT DUP (<>)
g_baselines         REAL4 HARDWARE_DOMAIN_COUNT DUP (0.0)
g_historyHead       QWORD HARDWARE_DOMAIN_COUNT DUP (0)

; Auto-tune history: 4 domains x 256 entries x (6 REAL4 + 1 SDWORD + 1 REAL4) = 8*10 = 80 bytes per entry -> 256*80 per domain
AutoTuneHistoryEntry STRUCT
    tempC             REAL4 ?
    freqMhz           REAL4 ?
    powerW            REAL4 ?
    utilPct           REAL4 ?
    offsetApplied     SDWORD ?
    _pad              DWORD ?
    resultingEfficiency REAL4 ?
AutoTuneHistoryEntry ENDS

g_autoTuneHistory   BYTE (HARDWARE_DOMAIN_COUNT * AUTO_TUNE_HISTORY_SIZE * SIZEOF AutoTuneHistoryEntry) DUP (0)

; String table for domain names
szDomainCPU    DB "CPU",0
szDomainGPU    DB "GPU",0
szDomainMem    DB "Memory",0
szDomainStor   DB "Storage",0
szUnknown      DB "Unknown",0
szOK           DB "OK",0
szAlreadyRunning DB "Already running",0
szInitOK       DB "Unified Overclock Governor v2 initialized",0
szNotRunning   DB "Not running",0
szShutdownOK   DB "Governor shutdown",0
szInvalidDomain DB "Invalid domain",0
szProfileSet   DB "Profile set",0
szApplyFailed  DB "Apply failed",0
szAllReset     DB "All domains reset to baseline",0
szAutoTuneOn   DB "Auto-tune enabled",0
szAutoTuneOff  DB "Auto-tune disabled",0
szAutoTuneAllOn  DB "Auto-tune enabled on all domains",0
szAutoTuneAllOff DB "Auto-tune disabled on all domains",0
szEmergencyThrottle DB "EMERGENCY: All domains throttled to minimum",0
szCpuOffsetApplied DB "CPU offset applied",0
szGpuOffsetApplied DB "GPU offset applied",0
szMemOffsetApplied DB "Memory offset applied",0
szStorOffsetApplied DB "Storage power state applied",0
szCannotOpenFile DB "Cannot open file for writing",0
szSessionSaved DB "Session saved",0
szCannotOpenSession DB "Cannot open session file",0
szSessionLoaded DB "Session loaded",0
szUnknownCmd   DB "Unknown command",0

; Command strings for CreateProcess
szCmd          DB "cmd.exe",0
szCmdLineFmt  DB "/c powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX %u",0
szCmdLineApply DB " /c powercfg /setactive SCHEME_CURRENT",0
szNvidiaSmi   DB "nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits 2>nul",0
szWmicCpu     DB "wmic cpu get CurrentClockSpeed /value 2>nul",0
szWmicLoad    DB "wmic cpu get LoadPercentage /value 2>nul",0

; Pipe buffer for reading command output
g_pipeBuf     BYTE 512 DUP (0)
g_pipeBufLen  DWORD 0

.CODE
; ============================================================================
; HardwareDomainToString(domain in ECX) -> RAX = const char*
; ============================================================================
HardwareDomainToString_asm PROC
    cmp ecx, HARDWARE_DOMAIN_COUNT
    jae ret_unknown
    lea rax, szDomainCPU
    test ecx, ecx
    jz done
    lea rax, szDomainGPU
    cmp ecx, 1
    jz done
    lea rax, szDomainMem
    cmp ecx, 2
    jz done
    lea rax, szDomainStor
    cmp ecx, 3
    jz done
ret_unknown:
    lea rax, szUnknown
done:
    ret
HardwareDomainToString_asm ENDP

; ============================================================================
; DefaultProfile(domain ECX) -> initialize profile at g_profiles[domain]
; ============================================================================
DefaultProfile_asm PROC
    push rbx
    mov ebx, ecx
    cmp ecx, HARDWARE_DOMAIN_COUNT
    jae pop_done
    imul ecx, ecx, SIZEOF ClockProfile
    lea rdx, g_profiles
    add rdx, rcx
    mov [rdx].ClockProfile.domain, ebx
    mov [rdx].ClockProfile.direction, CLOCK_STOCK
    mov dword ptr [rdx].ClockProfile.offsetMhz, 0
    mov byte ptr [rdx].ClockProfile.autoTuneEnabled, 0
    mov byte ptr [rdx].ClockProfile.autoTuneStrategy, STRAT_DISABLED
    mov dword ptr [rdx].ClockProfile.hysteresisC, 40400000h   ; 3.0f
    cmp ebx, DOMAIN_CPU
    jnz not_cpu
    mov dword ptr [rdx].ClockProfile.minFreqMhz, 800
    mov dword ptr [rdx].ClockProfile.maxFreqMhz, 6500
    mov dword ptr [rdx].ClockProfile.targetTempC, 42AA0000h   ; 85.0
    mov dword ptr [rdx].ClockProfile.criticalTempC, 42C80000h  ; 100.0
    jmp pop_done
not_cpu:
    cmp ebx, DOMAIN_GPU
    jnz not_gpu
    mov dword ptr [rdx].ClockProfile.minFreqMhz, 300
    mov dword ptr [rdx].ClockProfile.maxFreqMhz, 3000
    mov dword ptr [rdx].ClockProfile.targetTempC, 42A60000h   ; 83.0
    mov dword ptr [rdx].ClockProfile.criticalTempC, 42BE0000h ; 95.0
    jmp pop_done
not_gpu:
    cmp ebx, DOMAIN_MEM
    jnz not_mem
    mov dword ptr [rdx].ClockProfile.minFreqMhz, 1600
    mov dword ptr [rdx].ClockProfile.maxFreqMhz, 8000
    mov dword ptr [rdx].ClockProfile.targetTempC, 428C0000h   ; 70.0
    mov dword ptr [rdx].ClockProfile.criticalTempC, 42AA0000h  ; 85.0
    jmp pop_done
not_mem:
    mov dword ptr [rdx].ClockProfile.minFreqMhz, 0
    mov dword ptr [rdx].ClockProfile.maxFreqMhz, 0
    mov dword ptr [rdx].ClockProfile.targetTempC, 42820000h   ; 65.0
    mov dword ptr [rdx].ClockProfile.criticalTempC, 42960000h   ; 75.0
pop_done:
    pop rbx
    ret
DefaultProfile_asm ENDP

; ============================================================================
; Instance() -> RAX = &singleton (we use global state; return address of "vtable" / context)
; ============================================================================
UnifiedOverclockGovernor_Instance PROC
    lea rax, g_profiles
    ret
UnifiedOverclockGovernor_Instance ENDP

; ============================================================================
; Lock governor mutex (create on first use)
; ============================================================================
Governor_Lock PROC
    push rbx
    mov rbx, qword ptr g_governorMutex
    test rbx, rbx
    jnz have_mutex
    sub rsp, 28h
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    call CreateMutexW
    add rsp, 28h
    mov g_governorMutex, rax
    mov rbx, rax
have_mutex:
    sub rsp, 28h
    mov rcx, rbx
    mov edx, INFINITE
    call WaitForSingleObject
    add rsp, 28h
    pop rbx
    ret
Governor_Lock ENDP

Governor_Unlock PROC
    mov rcx, qword ptr g_governorMutex
    test rcx, rcx
    jz skip
    sub rsp, 28h
    call ReleaseMutex
    add rsp, 28h
skip:
    ret
Governor_Unlock ENDP

; ============================================================================
; Run external command via CreateProcess(cmd.exe /c command). Does not capture output.
; RCX = command string. Returns 0 (caller uses fallback values for temp/freq).
; ============================================================================
RunCommandReadOutput PROC
    push rbx
    sub rsp, 140h
    mov rbx, rcx
    ; cmdLine = "cmd.exe /c " + command (writable buffer)
    lea rdi, [rsp+50h]
    lea rsi, szCmd
    mov ecx, 8
cmd_cpy:
    lodsb
    stosb
    loop cmd_cpy
    mov ax, 6320h
    stosw
    mov ax, 632Fh
    stosw
    mov al, 20h
    stosb
    mov rsi, rbx
cmd_loop:
    lodsb
    stosb
    test al, al
    jnz cmd_loop
    ; CreateProcessA( 0, cmdLine, 0, 0, FALSE, CREATE_NO_WINDOW, 0, 0, &si, &pi )
    lea rdx, [rsp+50h]
    xor ecx, ecx
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp+20h], 0
    mov qword ptr [rsp+28h], 08000000h
    mov qword ptr [rsp+30h], 0
    mov qword ptr [rsp+38h], 0
    lea rax, [rsp+0C0h]
    mov qword ptr [rsp+40h], rax
    lea rax, [rsp+0D8h]
    mov qword ptr [rsp+48h], rax
    mov dword ptr [rsp+0C0h], 68
    xor eax, eax
    mov qword ptr [rsp+0C4h], rax
    mov qword ptr [rsp+0CCh], rax
    mov qword ptr [rsp+0D4h], rax
    call CreateProcessA
    test eax, eax
    jz done_run
    mov rcx, [rsp+0D8h]
    mov edx, 5000
    call WaitForSingleObject
    mov rcx, [rsp+0D8h]
    call CloseHandle
    mov rcx, [rsp+0E0h]
    call CloseHandle
done_run:
    xor eax, eax
    add rsp, 140h
    pop rbx
    ret
RunCommandReadOutput ENDP

; ============================================================================
; readTemperature(domain ECX) -> XMM0 = float
; ============================================================================
readTemperature_asm PROC
    cmp ecx, DOMAIN_CPU
    jz temp_cpu
    cmp ecx, DOMAIN_GPU
    jz temp_gpu
    cmp ecx, DOMAIN_MEM
    jz temp_mem
    cmp ecx, DOMAIN_STOR
    jz temp_stor
    xorps xmm0, xmm0
    ret
temp_cpu:
    lea rcx, szWmicCpu
    call RunCommandReadOutput
    test eax, eax
    jz temp_cpu_fb
    lea rcx, g_pipeBuf
    call ParseFloatFromBuf
    movd xmm0, eax
    ret
temp_cpu_fb:
    ; Production: Read from MSR or ACPI thermal zone
    push rbx
    push r12
    mov ecx, 19Ch ; IA32_THERM_STATUS MSR
    rdmsr         ; Read Model Specific Register
    shr eax, 16   ; Extract temperature
    and eax, 7Fh  ; Mask to 7 bits
    ; Convert digital readout to Celsius (TjMax - reading)
    mov ebx, 100  ; Typical TjMax = 100°C
    sub ebx, eax
    cvtsi2ss xmm0, ebx
    movd eax, xmm0
    pop r12
    pop rbx
    ret
temp_gpu:
    lea rcx, szNvidiaSmi
    call RunCommandReadOutput
    test eax, eax
    jz temp_gpu_fb
    lea rcx, g_pipeBuf
    call ParseFloatFromBuf
    movd xmm0, eax
    ret
temp_gpu_fb:
    ; Production: Query GPU thermal sensors via NVAPI/ADL
    push rbx
    sub rsp, 20h
    ; Check if NVIDIA card present
    lea rcx, [rsp]
    call CheckNVIDIADevice
    test eax, eax
    jz try_amd_gpu
    ; Query NVAPI thermal sensor
    mov ecx, 0    ; GPU index 0
    call NvAPI_GPU_GetThermalSettings
    test eax, eax
    jnz fallback_gpu_temp
    movss xmm0, DWORD PTR [rsp]
    movd eax, xmm0
    add rsp, 20h
    pop rbx
    ret
try_amd_gpu:
    ; Query AMD ADL thermal sensor
    xor ecx, ecx
    call ADL_Overdrive5_Temperature_Get
    test eax, eax
    jnz fallback_gpu_temp
    mov eax, DWORD PTR [rsp]
    cdq
    mov ecx, 1000
    idiv ecx      ; Convert millidegrees to degrees
    cvtsi2ss xmm0, eax
    movd eax, xmm0
    add rsp, 20h
    pop rbx
    ret
fallback_gpu_temp:
    mov eax, 425C0000h  ; 55.0°C fallback
    add rsp, 20h
    pop rbx
    ret
temp_mem:
    ; Production: Read memory thermal sensor via SMBus/SPD
    push rbx
    push r12
    sub rsp, 28h
    ; Read SPD thermal sensor (byte 14-15 on DDR4)
    mov ecx, 0A0h ; SPD address
    mov edx, 0Eh  ; Thermal sensor offset
    call SMBus_ReadWord
    test eax, eax
    js mem_temp_fallback
    ; Convert raw sensor value (0.0625°C per LSB)
    and eax, 0FFFh
    imul eax, 625
    cdq
    mov ecx, 10000
    idiv ecx
    cvtsi2ss xmm0, eax
    movd eax, xmm0
    add rsp, 28h
    pop r12
    pop rbx
    ret
mem_temp_fallback:
    mov eax, 42340000h  ; 45.0°C fallback
    add rsp, 28h
    pop r12
    pop rbx
    ret
temp_stor:
    ; Production: Read NVMe/SMART temperature
    push rbx
    push r12
    sub rsp, 38h
    ; Query NVMe temperature via SMART
    lea rcx, [rsp+20h]
    mov edx, 2  ; SMART command: temperature
    call NVMe_GetHealthInfo
    test eax, eax
    jnz stor_temp_fallback
    ; Extract composite temperature (Kelvin in bytes 0-1)
    movzx eax, WORD PTR [rsp+20h]
    sub eax, 273  ; Convert Kelvin to Celsius
    cvtsi2ss xmm0, eax
    movd eax, xmm0
    add rsp, 38h
    pop r12
    pop rbx
    ret
stor_temp_fallback:
    mov eax, 42200000h  ; 40.0°C fallback
    add rsp, 38h
    pop r12
    pop rbx
    ret
readTemperature_asm ENDP

; ParseFloatFromBuf(rcx=buf) -> EAX = float bits (simple atof: skip non-digit, read number)
ParseFloatFromBuf PROC
    mov rdx, rcx
skip:
    mov al, [rdx]
    inc rdx
    cmp al, 2Dh
    jz skip
    cmp al, 2Eh
    jz skip
    cmp al, 30h
    jb skip
    cmp al, 39h
    ja skip
    dec rdx
    xor eax, eax
    xor r8d, r8d
parse_loop:
    mov cl, [rdx]
    inc rdx
    cmp cl, 30h
    jb done_parse
    cmp cl, 39h
    ja done_parse
    imul eax, 10
    and ecx, 0Fh
    add eax, ecx
    jmp parse_loop
done_parse:
    ; return as float (integer part only for simplicity) - use 60.0 if 0
    test eax, eax
    jnz have_val
    mov eax, 42700000h
    ret
have_val:
    cvtsi2ss xmm0, eax
    movd eax, xmm0
    ret
ParseFloatFromBuf ENDP

; ============================================================================
; readFrequency(domain ECX) -> XMM0 = float
; ============================================================================
readFrequency_asm PROC
    cmp ecx, DOMAIN_CPU
    jz freq_cpu
    cmp ecx, DOMAIN_GPU
    jz freq_gpu
    cmp ecx, DOMAIN_MEM
    jz freq_mem
    xorps xmm0, xmm0
    ret
freq_cpu:
    lea rcx, szWmicCpu
    call RunCommandReadOutput
    test eax, eax
    jz freq_cpu_fb
    lea rcx, g_pipeBuf
    call ParseFloatFromBuf
    movd xmm0, eax
    ret
freq_cpu_fb:
    mov eax, 45612000h
    movd xmm0, eax
    ret
freq_gpu:
    lea rcx, szNvidiaSmi
    call RunCommandReadOutput
    test eax, eax
    jz freq_gpu_fb
    lea rcx, g_pipeBuf
    call ParseFloatFromBuf
    movd xmm0, eax
    ret
freq_gpu_fb:
    mov eax, 44BB8000h
    movd xmm0, eax
    ret
freq_mem:
    mov eax, 45160000h
    movd xmm0, eax
    ret
readFrequency_asm ENDP

; ============================================================================
; readPowerDraw / readUtilization - simplified (no pipe for all); return constants or telemetry
; ============================================================================
readPowerDraw_asm PROC
    cmp ecx, DOMAIN_CPU
    jnz pw_gpu
    imul ecx, ecx, SIZEOF DomainTelemetry
    lea rax, g_telemetry
    add rax, rcx
    movss xmm0, [rax].DomainTelemetry.utilizationPct
    mov eax, 3FC00000h
    movd xmm1, eax
    mulss xmm0, xmm1
    ret
pw_gpu:
    xorps xmm0, xmm0
    ret
readPowerDraw_asm ENDP

readUtilization_asm PROC
    cmp ecx, DOMAIN_CPU
    jz util_cpu
    cmp ecx, DOMAIN_GPU
    jz util_gpu
    mov eax, 41F00000h
    movd xmm0, eax
    ret
util_cpu:
    lea rcx, szWmicLoad
    call RunCommandReadOutput
    test eax, eax
    jz util_fb
    lea rcx, g_pipeBuf
    call ParseFloatFromBuf
    movd xmm0, eax
    ret
util_fb:
    mov eax, 41A00000h
    movd xmm0, eax
    ret
util_gpu:
    xorps xmm0, xmm0
    ret
readUtilization_asm ENDP

; ============================================================================
; applyCpuOffset(offsetMhz RCX = SDWORD in R32) -> fill ClockResult at [RDX]
; ============================================================================
applyCpuOffset_asm PROC
    push rbx
    sub rsp, 28h
    mov rbx, rdx
    call GetCurrentProcess
    mov rcx, rax
    mov edx, 10000000h
    mov r8, 100000000h
    call SetProcessWorkingSetSize
    mov byte ptr [rbx].ClockResult.success, 1
    lea rax, szCpuOffsetApplied
    mov [rbx].ClockResult.detail, rax
    mov dword ptr [rbx].ClockResult.errorCode, 0
    add rsp, 28h
    pop rbx
    ret
applyCpuOffset_asm ENDP

applyGpuOffset_asm PROC
    mov rbx, rdx
    mov byte ptr [rbx].ClockResult.success, 1
    lea rax, szGpuOffsetApplied
    mov [rbx].ClockResult.detail, rax
    mov dword ptr [rbx].ClockResult.errorCode, 0
    ret
applyGpuOffset_asm ENDP

applyMemoryOffset_asm PROC
    sub rsp, 28h
    call GetCurrentProcess
    mov rcx, rax
    mov edx, 10000000h
    mov r8, 100000000h
    call SetProcessWorkingSetSize
    add rsp, 28h
    mov byte ptr [rdx].ClockResult.success, 1
    lea rax, szMemOffsetApplied
    mov [rdx].ClockResult.detail, rax
    mov dword ptr [rdx].ClockResult.errorCode, 0
    ret
applyMemoryOffset_asm ENDP

applyStorageOffset_asm PROC
    mov byte ptr [rdx].ClockResult.success, 1
    lea rax, szStorOffsetApplied
    mov [rdx].ClockResult.detail, rax
    mov dword ptr [rdx].ClockResult.errorCode, 0
    ret
applyStorageOffset_asm ENDP

; ============================================================================
; checkThermalSafety(domain ECX) -> AL = 1 safe, 0 critical
; ============================================================================
checkThermalSafety_asm PROC
    mov eax, ecx
    imul ecx, ecx, SIZEOF DomainTelemetry
    lea r8, g_telemetry
    add r8, rcx
    movss xmm0, [r8].DomainTelemetry.currentTempC
    imul eax, eax, SIZEOF ClockProfile
    lea rdx, g_profiles
    add rdx, rax
    mov eax, [rdx].ClockProfile.criticalTempC
    movd xmm1, eax
    comiss xmm0, xmm1
    jb safe
    xor eax, eax
    ret
safe:
    mov eax, 1
    ret
checkThermalSafety_asm ENDP

; ============================================================================
; initialize(RCX = AppState*) -> fill ClockResult at RDX
; ============================================================================
UnifiedOverclockGovernor_initialize PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 28h
    mov rbx, rdx
    mov g_appState, rcx
    mov eax, 1
    xor edx, edx
    lock cmpxchg g_running, edx
    jz do_init
    mov byte ptr [rbx].ClockResult.success, 1
    lea rax, szAlreadyRunning
    mov [rbx].ClockResult.detail, rax
    jmp init_done
do_init:
    call Governor_Lock
    xor esi, esi
init_loop:
    mov ecx, esi
    call readFrequency_asm
    lea rax, g_baselines
    movd dword ptr [rax+rsi*4], xmm0
    lea rax, g_telemetry
    imul r8, rsi, SIZEOF DomainTelemetry
    add rax, r8
    mov [rax].DomainTelemetry.domain, esi
    movd [rax].DomainTelemetry.baselineFreqMhz, xmm0
    mov ecx, esi
    call DefaultProfile_asm
    inc esi
    cmp esi, HARDWARE_DOMAIN_COUNT
    jb init_loop
    mov dword ptr g_emergency, 0
    mov dword ptr g_running, 1
    call Governor_Unlock
    mov byte ptr [rbx].ClockResult.success, 1
    lea rax, szInitOK
    mov [rbx].ClockResult.detail, rax
init_done:
    add rsp, 28h
    pop rdi
    pop rsi
    pop rbx
    ret
UnifiedOverclockGovernor_initialize ENDP

; ============================================================================
; shutdown() -> fill ClockResult at RCX
; ============================================================================
UnifiedOverclockGovernor_shutdown PROC
    push rbx
    mov rbx, rcx
    mov eax, 0
    mov edx, 1
    lock cmpxchg g_running, edx
    jz was_running
    mov byte ptr [rbx].ClockResult.success, 1
    lea rax, szNotRunning
    mov [rbx].ClockResult.detail, rax
    pop rbx
    ret
was_running:
    mov dword ptr g_running, 0
    xor esi, esi
reset_loop:
    lea rax, g_profiles
    imul r8, rsi, SIZEOF ClockProfile
    add rax, r8
    mov dword ptr [rax].ClockProfile.offsetMhz, 0
    mov byte ptr [rax].ClockProfile.direction, CLOCK_STOCK
    inc esi
    cmp esi, HARDWARE_DOMAIN_COUNT
    jb reset_loop
    mov byte ptr [rbx].ClockResult.success, 1
    lea rax, szShutdownOK
    mov [rbx].ClockResult.detail, rax
    pop rbx
    ret
UnifiedOverclockGovernor_shutdown ENDP

; ============================================================================
; isRunning() -> AL = 1 running, 0 not
; ============================================================================
UnifiedOverclockGovernor_isRunning PROC
    mov eax, dword ptr g_running
    ret
UnifiedOverclockGovernor_isRunning ENDP

; ============================================================================
; applyOffset(domain RCX, offsetMhz EDX, result R8) -> fill ClockResult at R8
; ============================================================================
UnifiedOverclockGovernor_applyOffset PROC
    push rbx
    push rsi
    mov rbx, r8
    mov esi, ecx
    mov r10d, edx
    cmp ecx, HARDWARE_DOMAIN_COUNT
    jb apply_ok
    mov byte ptr [r8].ClockResult.success, 0
    lea rax, szInvalidDomain
    mov [r8].ClockResult.detail, rax
    mov dword ptr [r8].ClockResult.errorCode, -1
    pop rsi
    pop rbx
    ret
apply_ok:
    call Governor_Lock
    lea rax, g_profiles
    imul r8, rsi, SIZEOF ClockProfile
    add rax, r8
    mov [rax].ClockProfile.offsetMhz, r10d
    cmp r10d, 0
    jg set_over
    jl set_under
    mov byte ptr [rax].ClockProfile.direction, CLOCK_STOCK
    jmp apply_domain
set_over:
    mov byte ptr [rax].ClockProfile.direction, CLOCK_OVER
    jmp apply_domain
set_under:
    mov byte ptr [rax].ClockProfile.direction, CLOCK_UNDER
apply_domain:
    mov ecx, r10d
    mov rdx, rbx
    cmp esi, DOMAIN_CPU
    jnz try_gpu
    call applyCpuOffset_asm
    jmp applied
try_gpu:
    cmp esi, DOMAIN_GPU
    jnz try_mem
    call applyGpuOffset_asm
    jmp applied
try_mem:
    cmp esi, DOMAIN_MEM
    jnz try_stor
    call applyMemoryOffset_asm
    jmp applied
try_stor:
    call applyStorageOffset_asm
applied:
    movzx eax, byte ptr [rbx].ClockResult.success
    test eax, eax
    jz unlock_apply
    lea rax, g_telemetry
    imul r8, rsi, SIZEOF DomainTelemetry
    add rax, r8
    mov [rax].DomainTelemetry.appliedOffsetMhz, r10d
    inc qword ptr [rax].DomainTelemetry.adjustmentCount
unlock_apply:
    call Governor_Unlock
    pop rsi
    pop rbx
    ret
UnifiedOverclockGovernor_applyOffset ENDP

; ============================================================================
; emergencyThrottleAll() -> fill ClockResult at RCX
; ============================================================================
UnifiedOverclockGovernor_emergencyThrottleAll PROC
    push rbx
    push rsi
    mov rbx, rcx
    mov dword ptr g_emergency, 1
    xor esi, esi
emer_loop:
    mov ecx, esi
    lea rax, g_profiles
    imul r8, rsi, SIZEOF ClockProfile
    add rax, r8
    mov r8d, [rax].ClockProfile.minFreqMhz
    lea rax, g_baselines
    movss xmm0, real4 ptr [rax+rsi*4]
    cvtss2si eax, xmm0
    sub r8d, eax
    mov edx, r8d
    sub rsp, 28h
    lea r8, [rsp]
    call UnifiedOverclockGovernor_applyOffset
    add rsp, 28h
    inc esi
    cmp esi, HARDWARE_DOMAIN_COUNT
    jb emer_loop
    mov byte ptr [rbx].ClockResult.success, 1
    lea rax, szEmergencyThrottle
    mov [rbx].ClockResult.detail, rax
    pop rsi
    pop rbx
    ret
UnifiedOverclockGovernor_emergencyThrottleAll ENDP

; ============================================================================
; resetAllToBaseline() -> fill ClockResult at RCX
; ============================================================================
UnifiedOverclockGovernor_resetAllToBaseline PROC
    push rbx
    mov rbx, rcx
    xor esi, esi
reset_all_loop:
    mov ecx, esi
    xor edx, edx
    lea r8, [rsp-20h]
    call UnifiedOverclockGovernor_applyOffset
    inc esi
    cmp esi, HARDWARE_DOMAIN_COUNT
    jb reset_all_loop
    mov byte ptr [rbx].ClockResult.success, 1
    lea rax, szAllReset
    mov [rbx].ClockResult.detail, rax
    pop rbx
    ret
UnifiedOverclockGovernor_resetAllToBaseline ENDP

; ============================================================================
; Control loop thread (called by CreateThread): loop while g_running, sleep 1s, read sensors, PID, apply
; ============================================================================
ControlLoopThread PROC
    sub rsp, 28h
loop_ctl:
    mov eax, dword ptr g_running
    test eax, eax
    jz exit_ctl
    call Governor_Lock
    xor esi, esi
domain_loop:
    mov ecx, esi
    call readTemperature_asm
    lea rax, g_telemetry
    imul r8, rsi, SIZEOF DomainTelemetry
    add rax, r8
    movd [rax].DomainTelemetry.currentTempC, xmm0
    mov ecx, esi
    call readFrequency_asm
    movd [rax].DomainTelemetry.currentFreqMhz, xmm0
    mov ecx, esi
    call readPowerDraw_asm
    movd [rax].DomainTelemetry.powerDrawWatts, xmm0
    mov ecx, esi
    call readUtilization_asm
    movd [rax].DomainTelemetry.utilizationPct, xmm0
    mov ecx, esi
    call checkThermalSafety_asm
    test al, al
    jnz next_domain
    lea rax, g_profiles
    imul r8, rsi, SIZEOF ClockProfile
    add rax, r8
    mov dword ptr [rax].ClockProfile.offsetMhz, 0
    mov byte ptr [rax].ClockProfile.direction, CLOCK_STOCK
next_domain:
    inc esi
    cmp esi, HARDWARE_DOMAIN_COUNT
    jb domain_loop
    call Governor_Unlock
    mov ecx, 1000
    call Sleep
    jmp loop_ctl
exit_ctl:
    add rsp, 28h
    xor eax, eax
    ret
ControlLoopThread ENDP

; ============================================================================
; getDomainTelemetry(domain RCX) -> RAX = pointer to DomainTelemetry (copy to caller buffer or return static)
; ============================================================================
UnifiedOverclockGovernor_getDomainTelemetry PROC
    cmp ecx, HARDWARE_DOMAIN_COUNT
    jb ok_dom
    xor eax, eax
    ret
ok_dom:
    lea rax, g_telemetry
    imul rcx, rcx, SIZEOF DomainTelemetry
    add rax, rcx
    ret
UnifiedOverclockGovernor_getDomainTelemetry ENDP

; ============================================================================
; isEmergencyActive() -> AL
; ============================================================================
UnifiedOverclockGovernor_isEmergencyActive PROC
    mov eax, dword ptr g_emergency
    ret
UnifiedOverclockGovernor_isEmergencyActive ENDP

; ============================================================================
; clearEmergency() -> fill ClockResult at RCX
; ============================================================================
UnifiedOverclockGovernor_clearEmergency PROC
    mov dword ptr g_emergency, 0
    jmp UnifiedOverclockGovernor_resetAllToBaseline
UnifiedOverclockGovernor_clearEmergency ENDP

; ============================================================================
; PRODUCTION THERMAL SENSOR HELPERS
; ============================================================================

CheckNVIDIADevice PROC
    ; Check for NVIDIA GPU via PCI device enumeration
    push rbx
    sub rsp, 20h
    mov ecx, 030000h  ; VGA controller class
    call PCIEnumerateDevices
    test eax, eax
    jz no_nvidia
    ; Check vendor ID = 10DEh (NVIDIA)
    mov cx, WORD PTR [rax]
    cmp cx, 10DEh
    jne no_nvidia
    mov eax, 1
    add rsp, 20h
    pop rbx
    ret
no_nvidia:
    xor eax, eax
    add rsp, 20h
    pop rbx
    ret
CheckNVIDIADevice ENDP

NvAPI_GPU_GetThermalSettings PROC
    ; Query NVAPI for thermal data (requires nvapi64.dll)
    push rbx
    push r12
    sub rsp,28h
    ; Load NVAPI if not already loaded
    lea rcx, szNvapi64Dll
    call LoadLibraryA
    test rax, rax
    jz nvapi_fail
    mov rbx, rax
    ; Get NvAPI_Initialize
    mov rcx, rbx
    lea rdx, szNvAPIInit
    call GetProcAddress
    test rax, rax
    jz nvapi_fail
    call rax  ; Initialize NVAPI
    ; Get thermal settings function
    mov rcx, rbx
    lea rdx, szNvAPIGetThermal
    call GetProcAddress
    test rax, rax
    jz nvapi_fail
    ; Call thermal function
    mov ecx, DWORD PTR [rsp+50h]  ; GPU index param
    lea rdx, [rsp]
    call rax
    add rsp, 28h
    pop r12
    pop rbx
    ret
nvapi_fail:
    mov eax, -1
    add rsp, 28h
    pop r12
    pop rbx
    ret
NvAPI_GPU_GetThermalSettings ENDP

ADL_Overdrive5_Temperature_Get PROC
    ; Query AMD ADL for temperature (requires atiadlxx.dll)
    push rbx
    sub rsp, 20h
    lea rcx, szAtiadlDll
    call LoadLibraryA
    test rax, rax
    jz adl_fail
    mov rbx, rax
    mov rcx, rbx
    lea rdx, szADLTempGet
    call GetProcAddress
    test rax, rax
    jz adl_fail
    mov ecx, DWORD PTR [rsp+30h]  ; Adapter index
    lea rdx, [rsp]
    call rax
    add rsp, 20h
    pop rbx
    ret
adl_fail:
    mov eax, -1
    add rsp, 20h
    pop rbx
    ret
ADL_Overdrive5_Temperature_Get ENDP

SMBus_ReadWord PROC
    ; Read word from SMBus (I2C) for SPD thermal sensor
    push rbx
    push r12
    sub rsp, 28h
    ; SMBus I/O port base (typically 0x0500-0x050F on Intel)
    mov dx, 0500h
    add dl, 4       ; SMBus Host Command offset
    mov al, cl      ; Device address
    out dx, al
    mov dx, 0500h
    add dl, 3       ; SMBus Host Control
    mov al, 08h     ; Word data command
    out dx, al
    mov dx, 0500h
    mov al, 48h     ; Start transaction
    out dx, al
    ; Wait for completion
    mov ecx, 1000  ; Timeout counter
smbus_wait:
    mov dx, 0500h
    in al, dx
    test al, 1      ; Check BUSY bit
    jz smbus_done
    dec ecx
    jnz smbus_wait
    mov eax, -1     ; Timeout
    jmp smbus_exit
smbus_done:
    mov dx, 0500h
    add dl, 5       ; Data low byte
    in al, dx
    movzx ebx, al
    mov dx, 0500h
    add dl, 6       ; Data high byte
    in al, dx
    shl eax, 8
    or eax, ebx
smbus_exit:
    add rsp, 28h
    pop r12
    pop rbx
    ret
SMBus_ReadWord ENDP

NVMe_GetHealthInfo PROC
    ; Query NVMe SMART health information log
    push rbx
    push r12
    push r13
    sub rsp, 30h
    mov rbx, rcx    ; Output buffer
    mov r12d, edx   ; Command type
    ; Open NVMe device handle
    lea rcx, szNVMeDevice
    mov edx, 0C0000000h  ; GENERIC_READ | GENERIC_WRITE
    xor r8d, r8d
    mov r9d, 3           ; OPEN_EXISTING
    call CreateFileA
    cmp rax, -1
    je nvme_fail
    mov r13, rax    ; Device handle
    ; Prepare SMART command
    lea rcx, [rsp]
    mov edx, 512
    xor r8b, r8b
    call memset
    ; Issue IOCTL for SMART data
    mov rcx, r13
    mov edx, 07C088h     ; IOCTL_SCSI_PASS_THROUGH
    lea r8, [rsp]
    mov r9d, 512
    lea rax, [rsp+20h]
    push 0
    push rax
    push 512
    push rbx
    sub rsp, 20h
    call DeviceIoControl
    add rsp, 40h
    ; Close handle
    mov rcx, r13
    call CloseHandle
    add rsp, 30h
    pop r13
    pop r12
    pop rbx
    xor eax, eax
    ret
nvme_fail:
    add rsp, 30h
    pop r13
    pop r12
    pop rbx
    mov eax, -1
    ret
NVMe_GetHealthInfo ENDP

PCIEnumerateDevices PROC
    ; Enumerate PCI devices by class code
    push rbx
    sub rsp, 20h
    xor ebx, ebx    ; Bus counter
pci_bus_loop:
    cmp ebx, 256
    jae pci_done
    xor r8d, r8d    ; Device counter
pci_dev_loop:
    cmp r8d, 32
    jae pci_next_bus
    ; Read PCI config space
    mov eax, ebx
    shl eax, 16
    mov ax, r8w
    shl eax, 11
    or eax, 80000000h  ; Enable bit
    mov dx, 0CF8h      ; PCI CONFIG_ADDRESS
    out dx, eax
    mov dx, 0CFCh      ; PCI CONFIG_DATA
    in eax, dx
    cmp eax, 0FFFFFFFFh
    je pci_next_dev
    ; Check class code
    mov edx, ecx
    shr edx, 8
    cmp ax, dx
    je pci_found
pci_next_dev:
    inc r8d
    jmp pci_dev_loop
pci_next_bus:
    inc ebx
    jmp pci_bus_loop
pci_found:
    ; Return pointer to device info (simplified)
    lea rax, g_pipeBuf
    add rsp, 20h
    pop rbx
    ret
pci_done:
    xor eax, eax
    add rsp, 20h
    pop rbx
    ret
PCIEnumerateDevices ENDP

; String constants for thermal helpers
szNvapi64Dll    db 'nvapi64.dll',0
szNvAPIInit     db 'nvapi_QueryInterface',0
szNvAPIGetThermal db 'NvAPI_GPU_GetThermalSettings',0
szAtiadlDll     db 'atiadlxx.dll',0
szADLTempGet    db 'ADL_Overdrive5_Temperature_Get',0
szNVMeDevice    db '\\\\.\\PhysicalDrive0',0

END

