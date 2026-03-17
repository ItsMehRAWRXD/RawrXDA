;=====================================================================
;  POCKET_LAB_TURBO_DLL.ASM  –  Exportable DLL version
;  Pure MASM x64, Zero CRT, C-callable exports
;
;  Exports:
;    PocketLabInit()          - Initialize kernel, returns 0=ok
;    PocketLabGetThermal()    - Fill ThermalSnapshot struct
;    PocketLabRunCycle()      - Run one inference cycle
;    PocketLabGetStats()      - Get performance counters
;
;  Build:
;    ml64 /c /Fo pocket_lab_turbo_dll.obj pocket_lab_turbo_dll.asm
;    link /DLL /OUT:pocket_lab_turbo.dll /EXPORT:PocketLabInit /EXPORT:PocketLabGetThermal /EXPORT:PocketLabRunCycle /EXPORT:PocketLabGetStats pocket_lab_turbo_dll.obj kernel32.lib ntdll.lib
;=====================================================================
OPTION CASEMAP:NONE

;---------------------------------------------------------------------
; Kernel32 Imports
;---------------------------------------------------------------------
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN GlobalMemoryStatusEx:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN OpenFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC

;---------------------------------------------------------------------
; Constants
;---------------------------------------------------------------------
STD_OUTPUT_HANDLE   EQU -11
MEM_COMMIT          EQU 00001000h
MEM_RESERVE         EQU 00002000h
MEM_RELEASE         EQU 00008000h
PAGE_READWRITE      EQU 00000004h
FILE_MAP_READ       EQU 00000004h

; RAM thresholds
RAM_8GB             EQU 8589934592
RAM_16GB            EQU 17179869184

; Pin sizes (capped for demo)
PIN_MOBILE_BYTES    EQU 16777216
PIN_WORKSTATION     EQU 67108864
PIN_ENTERPRISE      EQU 67108864

; Prefetch
PREFETCH_MOBILE     EQU 65536
PREFETCH_WORK       EQU 262144
PREFETCH_ENTERPRISE EQU 524288

; TurboSparse
SPARSE_BITMAP_QWORDS EQU 32

; Tiers
TIER_MOBILE         EQU 0
TIER_WORKSTATION    EQU 1
TIER_ENTERPRISE     EQU 2

; Thermal MMF
SOVEREIGN_SIGNATURE EQU 534F5645h
THERMAL_TEMPS_OFF   EQU 10h
MEMSTATUSEX_SIZE    EQU 64

; ThermalSnapshot offsets (matches C struct)
; double t0,t1,t2,t3,t4 (5 * 8 = 40 bytes)
; uint tier (4 bytes)
; uint sparseSkipPct (4 bytes)
; uint gpuSplit (4 bytes)
SNAP_T0             EQU 0
SNAP_T1             EQU 8
SNAP_T2             EQU 16
SNAP_T3             EQU 24
SNAP_T4             EQU 32
SNAP_TIER           EQU 40
SNAP_SPARSE         EQU 44
SNAP_GPU            EQU 48

;=====================================================================
; DATA SECTION
;=====================================================================
.data

szMMFName           DB "Global\SOVEREIGN_NVME_TEMPS", 0

;=====================================================================
; BSS SECTION
;=====================================================================
.data?

; ---- MEMORYSTATUSEX ----
memStatus           DB MEMSTATUSEX_SIZE DUP(?)

; ---- System Config ----
g_Initialized       DD ?
g_PhysicalRAM       DQ ?
g_Tier              DD ?
g_PinSize           DQ ?
g_PrefetchSize      DD ?
g_GpuRatio          DD ?
g_CpuRatio          DD ?

; ---- TurboSparse Bitmap ----
SparseBitmap        DQ SPARSE_BITMAP_QWORDS DUP(?)
g_SparseSkipPct     DD ?

; ---- Allocations ----
g_pTokenBitmap      DQ ?
g_pHotPinCache      DQ ?

; ---- Thermal ----
g_hThermalMMF       DQ ?
g_pThermalView      DQ ?
g_DriveTemps        DD 5 DUP(?)             ; Temps for 5 drives

; ---- Performance Counters ----
g_PerfFreq          DQ ?
g_TokensProcessed   DQ ?
g_SparseSkipped     DQ ?
g_GpuProcessed      DQ ?
g_CpuProcessed      DQ ?

; ---- Console ----
g_hStdOut           DQ ?
g_Written           DD ?

;=====================================================================
; CODE SECTION
;=====================================================================
.code

;---------------------------------------------------------------------
; _DllMainCRTStartup - DLL Entry Point (standard name for /NOENTRY)
;---------------------------------------------------------------------
PUBLIC _DllMainCRTStartup
_DllMainCRTStartup PROC
    ; RCX = hInstDLL, RDX = fdwReason, R8 = lpvReserved
    cmp edx, 1                              ; DLL_PROCESS_ATTACH
    jne @CheckDetach
    ; Auto-init on load
    push rcx
    push rdx
    push r8
    sub rsp, 20h
    call PocketLabInit
    add rsp, 20h
    pop r8
    pop rdx
    pop rcx
    jmp @DllDone
@CheckDetach:
    test edx, edx                           ; DLL_PROCESS_DETACH
    jnz @DllDone
    push rcx
    push rdx
    push r8
    sub rsp, 20h
    call InternalCleanup
    add rsp, 20h
    pop r8
    pop rdx
    pop rcx
@DllDone:
    mov eax, 1
    ret
_DllMainCRTStartup ENDP

;---------------------------------------------------------------------
; PocketLabInit - Initialize kernel
; Returns: 0 = success, -1 = failure
;---------------------------------------------------------------------
PUBLIC PocketLabInit
PocketLabInit PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 40h
    .allocstack 40h
    .endprolog

    ; Already initialized?
    cmp dword ptr [g_Initialized], 1
    je @InitSuccess

    ; Get console handle
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov [g_hStdOut], rax

    ; Detect RAM and set tier
    call DetectTierInternal

    ; Connect thermal MMF
    call ConnectThermalInternal

    ; Allocate resources
    call AllocateInternal
    test eax, eax
    jnz @InitFail

    ; Build sparse bitmap
    call BuildSparseInternal

    ; Read initial temps
    call ReadTempsInternal

    mov dword ptr [g_Initialized], 1

@InitSuccess:
    xor eax, eax
    jmp @InitDone

@InitFail:
    mov eax, -1

@InitDone:
    add rsp, 40h
    pop rbx
    ret
PocketLabInit ENDP

;---------------------------------------------------------------------
; PocketLabGetThermal - Fill ThermalSnapshot struct
; RCX = pointer to ThermalSnapshot
;---------------------------------------------------------------------
PUBLIC PocketLabGetThermal
PocketLabGetThermal PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov r12, rcx                            ; Save output pointer

    ; Refresh temps from MMF
    call ReadTempsInternal

    ; Fill t0-t4 as doubles
    xor ebx, ebx
@TempLoop:
    cmp ebx, 5
    jge @FillRest

    mov eax, [g_DriveTemps + rbx*4]
    ; Convert int32 to double
    cvtsi2sd xmm0, eax
    mov rax, rbx
    shl rax, 3                              ; * 8 for double offset
    movsd qword ptr [r12 + rax], xmm0

    inc ebx
    jmp @TempLoop

@FillRest:
    ; tier
    mov eax, [g_Tier]
    mov [r12 + SNAP_TIER], eax

    ; sparseSkipPct
    mov eax, [g_SparseSkipPct]
    mov [r12 + SNAP_SPARSE], eax

    ; gpuSplit
    mov eax, [g_GpuRatio]
    mov [r12 + SNAP_GPU], eax

    add rsp, 28h
    pop r12
    pop rbx
    ret
PocketLabGetThermal ENDP

;---------------------------------------------------------------------
; PocketLabRunCycle - Run one inference cycle
; RCX = token count to process
; Returns: tokens actually processed
;---------------------------------------------------------------------
PUBLIC PocketLabRunCycle
PocketLabRunCycle PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 30h
    .allocstack 30h
    .endprolog

    mov r13, rcx                            ; token count
    test r13, r13
    jz @CycleDone

    ; Clear counters
    xor eax, eax
    mov [g_TokensProcessed], rax
    mov [g_SparseSkipped], rax
    mov [g_GpuProcessed], rax
    mov [g_CpuProcessed], rax

    xor r12d, r12d                          ; token counter

@TokenLoop:
    cmp r12, r13
    jge @CycleDone

    ; TurboSparse check
    mov eax, r12d
    and eax, 2047
    mov ecx, eax
    shr ecx, 6
    mov r8d, eax
    and r8d, 63
    lea r9, [SparseBitmap]
    bt qword ptr [r9 + rcx*8], r8
    jnc @NotSparse

    lock inc qword ptr [g_SparseSkipped]
    jmp @TokenDone

@NotSparse:
    ; PowerInfer split
    mov eax, r12d
    and eax, 1023
    mov ecx, [g_GpuRatio]
    imul ecx, 1024
    mov edx, 100
    push rax
    mov eax, ecx
    xor edx, edx
    mov ecx, 100
    div ecx
    mov ecx, eax
    pop rax
    cmp eax, ecx
    jge @ColdPath

    lock inc qword ptr [g_GpuProcessed]
    jmp @TokenDone

@ColdPath:
    lock inc qword ptr [g_CpuProcessed]

@TokenDone:
    lock inc qword ptr [g_TokensProcessed]
    inc r12
    jmp @TokenLoop

@CycleDone:
    mov rax, [g_TokensProcessed]
    add rsp, 30h
    pop r13
    pop r12
    pop rbx
    ret
PocketLabRunCycle ENDP

;---------------------------------------------------------------------
; PocketLabGetStats - Get performance counters
; RCX = pointer to 64-byte buffer
; Layout: [tokens:8][sparse:8][gpu:8][cpu:8][tier:4][spare:28]
;---------------------------------------------------------------------
PUBLIC PocketLabGetStats
PocketLabGetStats PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov rax, [g_TokensProcessed]
    mov [rcx], rax
    mov rax, [g_SparseSkipped]
    mov [rcx + 8], rax
    mov rax, [g_GpuProcessed]
    mov [rcx + 16], rax
    mov rax, [g_CpuProcessed]
    mov [rcx + 24], rax
    mov eax, [g_Tier]
    mov [rcx + 32], eax

    add rsp, 28h
    ret
PocketLabGetStats ENDP

;=====================================================================
; INTERNAL FUNCTIONS
;=====================================================================

;---------------------------------------------------------------------
; DetectTierInternal - Detect RAM and configure tier
;---------------------------------------------------------------------
DetectTierInternal PROC
    sub rsp, 28h

    lea rcx, [memStatus]
    mov dword ptr [rcx], MEMSTATUSEX_SIZE
    call GlobalMemoryStatusEx

    lea rax, [memStatus]
    mov rax, [rax + 8]
    mov [g_PhysicalRAM], rax

    mov rcx, RAM_8GB
    cmp rax, rcx
    jbe @TierMobile

    mov rcx, RAM_16GB
    cmp rax, rcx
    jbe @TierWork

    jmp @TierEnterprise

@TierMobile:
    mov dword ptr [g_Tier], TIER_MOBILE
    mov rcx, PIN_MOBILE_BYTES
    mov [g_PinSize], rcx
    mov dword ptr [g_PrefetchSize], PREFETCH_MOBILE
    mov dword ptr [g_GpuRatio], 100
    mov dword ptr [g_CpuRatio], 0
    jmp @TierDone

@TierWork:
    mov dword ptr [g_Tier], TIER_WORKSTATION
    mov rcx, PIN_WORKSTATION
    mov [g_PinSize], rcx
    mov dword ptr [g_PrefetchSize], PREFETCH_WORK
    mov dword ptr [g_GpuRatio], 70
    mov dword ptr [g_CpuRatio], 30
    jmp @TierDone

@TierEnterprise:
    mov dword ptr [g_Tier], TIER_ENTERPRISE
    mov rcx, PIN_ENTERPRISE
    mov [g_PinSize], rcx
    mov dword ptr [g_PrefetchSize], PREFETCH_ENTERPRISE
    mov dword ptr [g_GpuRatio], 33
    mov dword ptr [g_CpuRatio], 67

@TierDone:
    add rsp, 28h
    ret
DetectTierInternal ENDP

;---------------------------------------------------------------------
; ConnectThermalInternal - Open NVMe Oracle MMF
;---------------------------------------------------------------------
ConnectThermalInternal PROC
    sub rsp, 38h

    mov ecx, FILE_MAP_READ
    xor edx, edx
    lea r8, [szMMFName]
    call OpenFileMappingA
    test rax, rax
    jz @ThermalFail
    mov [g_hThermalMMF], rax

    mov rcx, rax
    mov edx, FILE_MAP_READ
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp + 20h], 160
    call MapViewOfFile
    test rax, rax
    jz @ThermalFail
    mov [g_pThermalView], rax

    mov ecx, [rax]
    cmp ecx, SOVEREIGN_SIGNATURE
    jne @ThermalFail

    mov eax, 1
    jmp @ThermalDone

@ThermalFail:
    mov qword ptr [g_pThermalView], 0
    xor eax, eax

@ThermalDone:
    add rsp, 38h
    ret
ConnectThermalInternal ENDP

;---------------------------------------------------------------------
; ReadTempsInternal - Read current temps from MMF
;---------------------------------------------------------------------
ReadTempsInternal PROC
    sub rsp, 28h

    mov rax, [g_pThermalView]
    test rax, rax
    jz @UseDefaults

    ; Copy 5 temps
    xor ecx, ecx
@ReadLoop:
    cmp ecx, 5
    jge @ReadDone
    mov edx, [rax + THERMAL_TEMPS_OFF + rcx*4]
    mov [g_DriveTemps + rcx*4], edx
    inc ecx
    jmp @ReadLoop

@UseDefaults:
    ; Default temps if MMF not available
    mov dword ptr [g_DriveTemps], 35
    mov dword ptr [g_DriveTemps + 4], 35
    mov dword ptr [g_DriveTemps + 8], 35
    mov dword ptr [g_DriveTemps + 12], 35
    mov dword ptr [g_DriveTemps + 16], 35

@ReadDone:
    add rsp, 28h
    ret
ReadTempsInternal ENDP

;---------------------------------------------------------------------
; AllocateInternal - Allocate buffers
;---------------------------------------------------------------------
AllocateInternal PROC
    sub rsp, 38h

    ; Token bitmap (8KB)
    xor ecx, ecx
    mov edx, 8192
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @AllocFail
    mov [g_pTokenBitmap], rax

    ; Hot-pin cache
    mov rax, [g_PinSize]
    xor ecx, ecx
    mov edx, eax
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @AllocFail
    mov [g_pHotPinCache], rax

    xor eax, eax
    jmp @AllocDone

@AllocFail:
    mov eax, 1

@AllocDone:
    add rsp, 38h
    ret
AllocateInternal ENDP

;---------------------------------------------------------------------
; BuildSparseInternal - Build sparse bitmap (~18% skip)
;---------------------------------------------------------------------
BuildSparseInternal PROC
    push rbx
    push r12
    sub rsp, 28h

    ; Clear bitmap
    lea rdi, [SparseBitmap]
    xor eax, eax
    mov ecx, SPARSE_BITMAP_QWORDS
    rep stosq

    xor r12d, r12d
    xor ebx, ebx

@ScanLoop:
    cmp r12d, 2048
    jge @ScanDone

    ; Mark sparse if (index % 6) == 0 || (index % 11) == 0
    mov eax, r12d
    xor edx, edx
    mov ecx, 6
    div ecx
    test edx, edx
    jz @MarkSparse

    mov eax, r12d
    xor edx, edx
    mov ecx, 11
    div ecx
    test edx, edx
    jnz @NextSlab

@MarkSparse:
    mov eax, r12d
    shr eax, 6
    mov ecx, r12d
    and ecx, 63
    lea r9, [SparseBitmap]
    bts qword ptr [r9 + rax*8], rcx
    inc ebx

@NextSlab:
    inc r12d
    jmp @ScanLoop

@ScanDone:
    ; Calculate skip percentage
    mov eax, ebx
    imul eax, 100
    mov ecx, 2048
    xor edx, edx
    div ecx
    mov [g_SparseSkipPct], eax

    add rsp, 28h
    pop r12
    pop rbx
    ret
BuildSparseInternal ENDP

;---------------------------------------------------------------------
; InternalCleanup - Free resources on DLL unload
;---------------------------------------------------------------------
InternalCleanup PROC
    sub rsp, 28h

    cmp dword ptr [g_Initialized], 0
    je @CleanupDone

    mov rcx, [g_pHotPinCache]
    test rcx, rcx
    jz @FreeBitmap
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree

@FreeBitmap:
    mov rcx, [g_pTokenBitmap]
    test rcx, rcx
    jz @UnmapThermal
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree

@UnmapThermal:
    mov rcx, [g_pThermalView]
    test rcx, rcx
    jz @CloseThermal
    call UnmapViewOfFile

@CloseThermal:
    mov rcx, [g_hThermalMMF]
    test rcx, rcx
    jz @CleanupDone
    call CloseHandle

@CleanupDone:
    mov dword ptr [g_Initialized], 0
    add rsp, 28h
    ret
InternalCleanup ENDP

END
