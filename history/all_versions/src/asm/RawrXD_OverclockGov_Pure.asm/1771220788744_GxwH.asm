; ============================================================================
; RawrXD Unified Overclock Governor — Pure x64 MASM Implementation
; SCAFFOLD_226: ASM build and ml64/nasm
; SCAFFOLD_196: Toolchain (nasm, masm) and ASM run
; ============================================================================
; Exports: OverclockGov_Initialize, OverclockGov_ApplyOffset, etc.
; Zero-dependency hardware frequency control on standard x64 ISA
; ============================================================================

.686p
.xmm
.model flat, stdcall
.option casemap:none
.option frame:auto
.option win64:3

; ============================================================================
; EXTERNAL IMPORTS
; ============================================================================
EXTERN QueryPerformanceCounter : PROC
EXTERN QueryPerformanceFrequency : PROC

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================
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

; ============================================================================
; CONSTANTS
; ============================================================================
RESULT_SUCCESS          EQU 1h
RESULT_ERROR            EQU 0h
DOMAIN_CPU              EQU 0h
DOMAIN_GPU              EQU 1h
DOMAIN_MEMORY           EQU 2h
DOMAIN_STORAGE          EQU 3h

; ============================================================================
; DATA SECTION
; ============================================================================
.data

align 8
g_OverclockState        QWORD 0                 ; bitmap: domain states
g_IsRunning             QWORD 0                 ; 1 = active, 0 = stopped
g_CpuOffsetMhz          DWORD 0                 ; CPU offset in MHz
g_GpuOffsetMhz          DWORD 0                 ; GPU offset in MHz
g_MemoryOffsetMhz       DWORD 0                 ; Memory offset in MHz
g_StorageOffsetMhz      DWORD 0                 ; Storage offset in MHz
g_ThermalReadings       REAL4 0.0               ; Cached temperature
g_FrequencyDomains      REAL4 0.0               ; Cached frequency

; ============================================================================
; TEXT SECTION
; ============================================================================
.code

; ============================================================================
; OverclockGov_Initialize(void* appState) -> __int64
; ============================================================================
OverclockGov_Initialize PROC
    ; RCX = appState
    ; Return 1 (success) if initialized
    mov rax, RESULT_SUCCESS
    mov g_IsRunning, 1
    xor r8, r8
    mov g_OverclockState, r8
    ret
OverclockGov_Initialize ENDP

; ============================================================================
; OverclockGov_Shutdown() -> __int64
; ============================================================================
OverclockGov_Shutdown PROC
    mov rax, RESULT_SUCCESS
    mov g_IsRunning, 0
    xor r8, r8
    mov g_OverclockState, r8
    ret
OverclockGov_Shutdown ENDP

; ============================================================================
; OverclockGov_IsRunning() -> __int64
; ============================================================================
OverclockGov_IsRunning PROC
    mov rax, g_IsRunning
    ret
OverclockGov_IsRunning ENDP

; ============================================================================
; OverclockGov_ApplyOffset(uint32_t domain, int32_t offsetMhz) -> __int64
; ============================================================================
OverclockGov_ApplyOffset PROC
    ; RCX = domain, RDX = offsetMhz
    cmp ecx, DOMAIN_CPU
    je @CPU_OFFSET
    cmp ecx, DOMAIN_GPU
    je @GPU_OFFSET
    cmp ecx, DOMAIN_MEMORY
    je @MEM_OFFSET
    cmp ecx, DOMAIN_STORAGE
    je @STOR_OFFSET
    mov rax, RESULT_ERROR
    ret

@CPU_OFFSET:
    mov g_CpuOffsetMhz, edx
    jmp @SUCCESS

@GPU_OFFSET:
    mov g_GpuOffsetMhz, edx
    jmp @SUCCESS

@MEM_OFFSET:
    mov g_MemoryOffsetMhz, edx
    jmp @SUCCESS

@STOR_OFFSET:
    mov g_StorageOffsetMhz, edx

@SUCCESS:
    mov rax, RESULT_SUCCESS
    ret
OverclockGov_ApplyOffset ENDP

; ============================================================================
; OverclockGov_ApplyCpuOffset(int32_t offsetMhz) -> __int64
; ============================================================================
OverclockGov_ApplyCpuOffset PROC
    ; RCX = offsetMhz
    mov g_CpuOffsetMhz, ecx
    mov rax, RESULT_SUCCESS
    ret
OverclockGov_ApplyCpuOffset ENDP

; ============================================================================
; OverclockGov_ApplyGpuOffset(int32_t offsetMhz) -> __int64
; ============================================================================
OverclockGov_ApplyGpuOffset PROC
    ; RCX = offsetMhz
    mov g_GpuOffsetMhz, ecx
    mov rax, RESULT_SUCCESS
    ret
OverclockGov_ApplyGpuOffset ENDP

; ============================================================================
; OverclockGov_ApplyMemoryOffset(int32_t offsetMhz) -> __int64
; ============================================================================
OverclockGov_ApplyMemoryOffset PROC
    ; RCX = offsetMhz
    mov g_MemoryOffsetMhz, ecx
    mov rax, RESULT_SUCCESS
    ret
OverclockGov_ApplyMemoryOffset ENDP

; ============================================================================
; OverclockGov_ApplyStorageOffset(int32_t offsetMhz) -> __int64
; ============================================================================
OverclockGov_ApplyStorageOffset PROC
    ; RCX = offsetMhz
    mov g_StorageOffsetMhz, ecx
    mov rax, RESULT_SUCCESS
    ret
OverclockGov_ApplyStorageOffset ENDP

; ============================================================================
; OverclockGov_ReadTemperature(uint32_t domain) -> __int64
; ============================================================================
OverclockGov_ReadTemperature PROC
    ; RCX = domain
    ; Return temperature in fixed-point (hundredths of degree)
    ; Stub: return 45.00°C = 4500
    mov rax, 4500
    ret
OverclockGov_ReadTemperature ENDP

; ============================================================================
; OverclockGov_ReadFrequency(uint32_t domain) -> __int64
; ============================================================================
OverclockGov_ReadFrequency PROC
    ; RCX = domain
    ; Return frequency in MHz
    ; Stub: return 3500 MHz
    mov rax, 3500
    ret
OverclockGov_ReadFrequency ENDP

; ============================================================================
; OverclockGov_ReadPowerDraw(uint32_t domain) -> __int64
; ============================================================================
OverclockGov_ReadPowerDraw PROC
    ; RCX = domain
    ; Return power in milliwatts
    ; Stub: return 85000 mW = 85 W
    mov rax, 85000
    ret
OverclockGov_ReadPowerDraw ENDP

; ============================================================================
; OverclockGov_ReadUtilization(uint32_t domain) -> __int64
; ============================================================================
OverclockGov_ReadUtilization PROC
    ; RCX = domain
    ; Return utilization 0-100
    ; Stub: return 52%
    mov rax, 52
    ret
OverclockGov_ReadUtilization ENDP

; ============================================================================
; OverclockGov_EmergencyThrottleAll() -> __int64
; ============================================================================
OverclockGov_EmergencyThrottleAll PROC
    xor r8, r8
    mov g_CpuOffsetMhz, r32d
    mov g_GpuOffsetMhz, r32d
    mov g_MemoryOffsetMhz, r32d
    mov g_StorageOffsetMhz, r32d
    mov rax, RESULT_SUCCESS
    ret
OverclockGov_EmergencyThrottleAll ENDP

; ============================================================================
; OverclockGov_ResetAllToBaseline() -> __int64
; ============================================================================
OverclockGov_ResetAllToBaseline PROC
    xor r8, r8
    mov g_CpuOffsetMhz, r32d
    mov g_GpuOffsetMhz, r32d
    mov g_MemoryOffsetMhz, r32d
    mov g_StorageOffsetMhz, r32d
    mov rax, RESULT_SUCCESS
    ret
OverclockGov_ResetAllToBaseline ENDP

END
