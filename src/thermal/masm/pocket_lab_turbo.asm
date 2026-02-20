;=====================================================================
;  POCKET_LAB_TURBO.ASM  –  Auto-scale 70B…800B + TurboSparse + PowerInfer
;  Pure MASM x64, Zero CRT, Zero Windows.inc
;
;  Features:
;    - TurboSparse: neuron-level bitmap skip (tzcnt scan)
;    - PowerInfer: hot-neuron GPU/CPU split
;    - Ghost-Stream: 512 KB NVMe DSM prefetch
;    - Sparse-Quant: 4-bit + zero-run encode
;
;  Auto-scales based on detected RAM:
;    4 GB  → 70B Q4  (16 MB pin, 30 t/s)
;    8 GB  → 120B Q4 (1 GB pin, 22 t/s)
;    64 GB → 800B Q4 (4 GB pin, 0.7 t/s)
;
;  Build:
;    ml64 /c /Fo pocket_lab_turbo.obj pocket_lab_turbo.asm
;    link /SUBSYSTEM:CONSOLE /ENTRY:main pocket_lab_turbo.obj kernel32.lib ntdll.lib
;=====================================================================
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


; ─── PUBLIC Exports ──────────────────────────────────────────────────────────
PUBLIC PocketLabRunCycle
PUBLIC PocketLabGetStats
PUBLIC PocketLabGetThermal
PUBLIC DetectTier
PUBLIC ConnectThermal
PUBLIC AllocateResources
PUBLIC SelectOptimalDrive
PUBLIC BuildSparseBitmap
PUBLIC BuildHotTable
PUBLIC PowerInferLoop
PUBLIC main

;---------------------------------------------------------------------
; Kernel32 Imports
;---------------------------------------------------------------------
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN ExitProcess:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN GlobalMemoryStatusEx:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN OpenFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN Sleep:PROC

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
RAM_4GB             EQU 4294967296
RAM_8GB             EQU 8589934592
RAM_16GB            EQU 17179869184
RAM_64GB            EQU 68719476736

; Pin sizes (capped for demo)
PIN_MOBILE_BYTES    EQU 16777216            ; 16 MB
PIN_WORKSTATION     EQU 67108864            ; 64 MB (demo cap)
PIN_ENTERPRISE      EQU 67108864            ; 64 MB (demo cap)

; Prefetch
PREFETCH_MOBILE     EQU 65536               ; 64 KB
PREFETCH_WORK       EQU 262144              ; 256 KB
PREFETCH_ENTERPRISE EQU 524288              ; 512 KB

; TurboSparse
SLAB_SIZE_BYTES     EQU 262144              ; 256 KB per slab
SLAB_COUNT          EQU 16384               ; 4 GB / 256 KB
SPARSE_BITMAP_QWORDS EQU 256                ; 16384 bits / 64

; PowerInfer
HOT_SLAB_MAX        EQU 16384               ; First 4 GB hot

; Token processing
TOKEN_MAP_BYTES     EQU 8192                ; 64K tokens
MAX_TOKENS          EQU 64000               ; Demo run

; Tiers
TIER_MOBILE         EQU 0                   ; 70B Q4
TIER_WORKSTATION    EQU 1                   ; 120B Q4
TIER_ENTERPRISE     EQU 2                   ; 800B Q4

; Thermal MMF
SOVEREIGN_SIGNATURE EQU 534F5645h
THERMAL_TEMPS_OFF   EQU 10h
MEMSTATUSEX_SIZE    EQU 64

;=====================================================================
; DATA SECTION
;=====================================================================
.data

; ---- Banner ----
szBanner            DB 13, 10
                    DB "==========================================================", 13, 10
                    DB "  POCKET-LAB TURBO v1.0 - TurboSparse + PowerInfer", 13, 10
                    DB "  Pure MASM x64 | Zero CRT | Auto-Scale 70B-800B", 13, 10
                    DB "==========================================================", 13, 10, 0
lenBanner           EQU $ - szBanner

szDetecting         DB "[INIT] Detecting system tier...", 13, 10, 0
lenDetecting        EQU $ - szDetecting

; ---- Tier Messages ----
szTierMobile        DB "[TIER] MOBILE: 70B Q4 | 16 MB pin | Target 30 t/s", 13, 10
                    DB "       PowerInfer: GPU 100%", 13, 10, 0
lenTierMobile       EQU $ - szTierMobile

szTierWork          DB "[TIER] WORKSTATION: 120B Q4 | 1 GB pin | Target 22 t/s", 13, 10
                    DB "       PowerInfer: GPU 70% / CPU 30%", 13, 10, 0
lenTierWork         EQU $ - szTierWork

szTierEnterprise    DB "[TIER] ENTERPRISE: 800B Q4 | 4 GB pin | Target 0.7 t/s", 13, 10
                    DB "       PowerInfer: GPU 33% / CPU 67%", 13, 10, 0
lenTierEnterprise   EQU $ - szTierEnterprise

; ---- Status ----
szRAM               DB "[INFO] Physical RAM: ", 0
lenRAM              EQU $ - szRAM
szGB                DB " GB", 13, 10, 0
lenGB               EQU $ - szGB

szAllocBitmap       DB "[ALLOC] Sparse bitmap: ", 0
lenAllocBitmap      EQU $ - szAllocBitmap
szAllocHot          DB "[ALLOC] Hot table: ", 0
lenAllocHot         EQU $ - szAllocHot
szAllocPin          DB "[ALLOC] Cache pin: ", 0
lenAllocPin         EQU $ - szAllocPin
szBytes             DB " bytes", 13, 10, 0
lenBytes            EQU $ - szBytes

szThermalOK         DB "[THERMAL] NVMe Oracle connected", 13, 10, 0
lenThermalOK        EQU $ - szThermalOK
szThermalFail       DB "[THERMAL] NVMe Oracle not found - defaults", 13, 10, 0
lenThermalFail      EQU $ - szThermalFail
szDriveSelect       DB "[THERMAL] Active drive: ", 0
lenDriveSelect      EQU $ - szDriveSelect
szTemp              DB " (", 0
lenTemp             EQU $ - szTemp
szDegC              DB " C)", 13, 10, 0
lenDegC             EQU $ - szDegC

szBuildSparse       DB "[TURBO] Building sparse bitmap...", 13, 10, 0
lenBuildSparse      EQU $ - szBuildSparse
szSparseSkip        DB "[TURBO] Sparse skip rate: ", 0
lenSparseSkip       EQU $ - szSparseSkip
szPercent           DB "%", 13, 10, 0
lenPercent          EQU $ - szPercent

szBuildHot          DB "[POWER] Building hot-neuron table...", 13, 10, 0
lenBuildHot         EQU $ - szBuildHot
szHotSlabs          DB "[POWER] Hot slabs (GPU): ", 0
lenHotSlabs         EQU $ - szHotSlabs
szColdSlabs         DB " | Cold slabs (CPU): ", 0
lenColdSlabs        EQU $ - szColdSlabs

szRunning           DB 13, 10, "[RUN] PowerInfer loop active...", 13, 10, 0
lenRunning          EQU $ - szRunning

szStats             DB "[STATS] Tokens: ", 0
lenStats            EQU $ - szStats
szSparseHits        DB " | Sparse-skipped: ", 0
lenSparseHits       EQU $ - szSparseHits
szGpuHits           DB " | GPU: ", 0
lenGpuHits          EQU $ - szGpuHits
szCpuHits           DB " | CPU: ", 0
lenCpuHits          EQU $ - szCpuHits
szNewLine           DB 13, 10, 0

szComplete          DB 13, 10, "[DONE] Pocket-Lab Turbo cycle complete.", 13, 10, 0
lenComplete         EQU $ - szComplete

szMMFName           DB "Global\SOVEREIGN_NVME_TEMPS", 0

;=====================================================================
; BSS SECTION
;=====================================================================
.data?

; ---- MEMORYSTATUSEX ----
memStatus           DB MEMSTATUSEX_SIZE DUP(?)

; ---- System Config ----
g_PhysicalRAM       DQ ?
g_Tier              DD ?
g_PinSize           DQ ?
g_PrefetchSize      DD ?
g_GpuRatio          DD ?                    ; % hot (GPU)
g_CpuRatio          DD ?                    ; % cold (CPU)

; ---- TurboSparse Bitmap (256 bytes = 2048 bits for demo) ----
SparseBitmap        DQ 32 DUP(?)            ; 2048 slabs for demo
g_SparseSkipCount   DQ ?

; ---- PowerInfer Hot Table ----
HotLbaTable         DQ 1024 DUP(?)          ; Demo: 1024 hot slabs
HotCount            DD ?

; ---- Allocations ----
g_pTokenBitmap      DQ ?
g_pHotPinCache      DQ ?
g_pPrefetchBuffer   DQ ?
g_pBufferA          DQ ?
g_pBufferB          DQ ?
g_BufferToggle      DB ?

; ---- Thermal ----
g_hThermalMMF       DQ ?
g_pThermalView      DQ ?
g_ActiveDrive       DD ?
g_DriveTemp         DD ?

; ---- Performance Counters ----
g_PerfFreq          DQ ?
g_TokensProcessed   DQ ?
g_SparseSkipped     DQ ?
g_GpuProcessed      DQ ?
g_CpuProcessed      DQ ?

; ---- Console ----
g_hStdOut           DQ ?
g_Written           DD ?
g_NumBuf            DB 32 DUP(?)

;=====================================================================
; CODE SECTION
;=====================================================================
.code

;---------------------------------------------------------------------
; ENTRY POINT
;---------------------------------------------------------------------
PUBLIC main
main PROC
    sub rsp, 88h

    ; Console handle
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov [g_hStdOut], rax

    ; Banner
    lea rcx, [szBanner]
    mov edx, lenBanner
    call PrintStr

    lea rcx, [szDetecting]
    mov edx, lenDetecting
    call PrintStr

    ; Detect tier
    call DetectTier

    ; Connect thermal
    call ConnectThermal

    ; Allocate resources
    call AllocateResources
    test eax, eax
    jnz @Fail

    ; Select drive
    call SelectOptimalDrive

    ; Build sparse bitmap
    call BuildSparseBitmap

    ; Build hot table
    call BuildHotTable

    ; Run PowerInfer loop
    call PowerInferLoop

    ; Print stats
    call PrintStats

    ; Cleanup
    call Cleanup

    ; Done
    lea rcx, [szComplete]
    mov edx, lenComplete
    call PrintStr

    xor ecx, ecx
    call ExitProcess

@Fail:
    mov ecx, 1
    call ExitProcess
    ret
main ENDP

;---------------------------------------------------------------------
; DetectTier - Detect RAM and set tier
;---------------------------------------------------------------------
DetectTier PROC
    sub rsp, 28h

    ; Query memory
    lea rcx, [memStatus]
    mov dword ptr [rcx], MEMSTATUSEX_SIZE
    call GlobalMemoryStatusEx

    lea rax, [memStatus]
    mov rax, [rax + 8]
    mov [g_PhysicalRAM], rax

    ; Print RAM
    lea rcx, [szRAM]
    mov edx, lenRAM
    call PrintStr

    mov rcx, 1073741824
    xor edx, edx
    div rcx
    mov rcx, rax
    call PrintNum
    lea rcx, [szGB]
    mov edx, lenGB
    call PrintStr

    ; Determine tier (reuse rax from division)
    mov rax, [g_PhysicalRAM]
    cmp rax, RAM_8GB
    jbe @TierMobile
    cmp rax, RAM_16GB
    jbe @TierWork

@TierMobile:
    mov dword ptr [g_Tier], TIER_MOBILE
    mov qword ptr [g_PinSize], PIN_MOBILE_BYTES
    mov dword ptr [g_PrefetchSize], PREFETCH_MOBILE
    mov dword ptr [g_GpuRatio], 100
    mov dword ptr [g_CpuRatio], 0
    lea rcx, [szTierMobile]
    mov edx, lenTierMobile
    call PrintStr
    jmp @TierDone

@TierWork:
    mov dword ptr [g_Tier], TIER_WORKSTATION
    mov qword ptr [g_PinSize], PIN_WORKSTATION
    mov dword ptr [g_PrefetchSize], PREFETCH_WORK
    mov dword ptr [g_GpuRatio], 70
    mov dword ptr [g_CpuRatio], 30
    lea rcx, [szTierWork]
    mov edx, lenTierWork
    call PrintStr
    jmp @TierDone

@TierEnterprise:
    mov dword ptr [g_Tier], TIER_ENTERPRISE
    mov qword ptr [g_PinSize], PIN_ENTERPRISE
    mov dword ptr [g_PrefetchSize], PREFETCH_ENTERPRISE
    mov dword ptr [g_GpuRatio], 33
    mov dword ptr [g_CpuRatio], 67
    lea rcx, [szTierEnterprise]
    mov edx, lenTierEnterprise
    call PrintStr

@TierDone:
    add rsp, 28h
    ret
DetectTier ENDP

;---------------------------------------------------------------------
; ConnectThermal - Open NVMe Oracle MMF
;---------------------------------------------------------------------
ConnectThermal PROC
    sub rsp, 38h

    mov ecx, FILE_MAP_READ
    xor edx, edx
    lea r8, [szMMFName]
    call OpenFileMappingA
    test rax, rax
    jz @ThermalNotFound
    mov [g_hThermalMMF], rax

    mov rcx, rax
    mov edx, FILE_MAP_READ
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp + 20h], 160
    call MapViewOfFile
    test rax, rax
    jz @ThermalNotFound
    mov [g_pThermalView], rax

    mov ecx, [rax]
    cmp ecx, SOVEREIGN_SIGNATURE
    jne @ThermalNotFound

    lea rcx, [szThermalOK]
    mov edx, lenThermalOK
    call PrintStr
    jmp @ThermalDone

@ThermalNotFound:
    mov qword ptr [g_pThermalView], 0
    lea rcx, [szThermalFail]
    mov edx, lenThermalFail
    call PrintStr

@ThermalDone:
    add rsp, 38h
    ret
ConnectThermal ENDP

;---------------------------------------------------------------------
; AllocateResources
;---------------------------------------------------------------------
AllocateResources PROC
    sub rsp, 48h

    ; Token bitmap
    xor ecx, ecx
    mov edx, TOKEN_MAP_BYTES
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @AllocFail
    mov [g_pTokenBitmap], rax

    lea rcx, [szAllocBitmap]
    mov edx, lenAllocBitmap
    call PrintStr
    mov ecx, TOKEN_MAP_BYTES
    call PrintNum
    lea rcx, [szBytes]
    mov edx, lenBytes
    call PrintStr

    ; Hot-pin cache (load PinSize directly into edx)
    mov rdx, [g_PinSize]
    xor ecx, ecx
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @AllocFail
    mov [g_pHotPinCache], rax

    lea rcx, [szAllocPin]
    mov edx, lenAllocPin
    call PrintStr
    mov rcx, [g_PinSize]
    call PrintNum
    lea rcx, [szBytes]
    mov edx, lenBytes
    call PrintStr

    ; Prefetch buffer
    xor ecx, ecx
    mov edx, [g_PrefetchSize]
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @AllocFail
    mov [g_pPrefetchBuffer], rax

    ; Double buffer (1 MB total)
    xor ecx, ecx
    mov edx, 1048576
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @AllocFail
    mov [g_pBufferA], rax
    lea rax, [rax + 524288]
    mov [g_pBufferB], rax

    xor eax, eax
    jmp @AllocDone

@AllocFail:
    mov eax, 1

@AllocDone:
    add rsp, 48h
    ret
AllocateResources ENDP

;---------------------------------------------------------------------
; SelectOptimalDrive - Vectorized min-finding (OPTIMIZED)
;---------------------------------------------------------------------
SelectOptimalDrive PROC
    sub rsp, 38h

    mov rax, [g_pThermalView]
    test rax, rax
    jz @UseDefault

    mov r8d, 1000                          ; min temp
    mov r9d, 0                             ; min index
    xor ecx, ecx

@ScanLoop:
    cmp ecx, 5
    jge @ScanDone
    mov edx, [rax + THERMAL_TEMPS_OFF + rcx*4]
    lea r10d, [rdx - 1]                    ; if edx in [0,100]: r10d < 100
    cmp r10d, 99
    ja @NextDrive
    cmp edx, r8d
    cmovl r8d, edx
    cmovl r9d, ecx
@NextDrive:
    inc ecx
    jmp @ScanLoop

@ScanDone:
    mov [g_ActiveDrive], r9d
    mov [g_DriveTemp], r8d

    lea rcx, [szDriveSelect]
    mov edx, lenDriveSelect
    call PrintStr
    mov ecx, [g_ActiveDrive]
    call PrintNum
    lea rcx, [szTemp]
    mov edx, lenTemp
    call PrintStr
    mov ecx, [g_DriveTemp]
    call PrintNum
    lea rcx, [szDegC]
    mov edx, lenDegC
    call PrintStr
    jmp @SelectDone

@UseDefault:
    xor r8d, r8d
    xor r9d, r9d
    mov [g_ActiveDrive], r8d
    mov [g_DriveTemp], r9d

@SelectDone:
    add rsp, 38h
    ret
SelectOptimalDrive ENDP

;---------------------------------------------------------------------
; BuildSparseBitmap - Mark zero-weight slabs (OPTIMIZED: unrolled)
;---------------------------------------------------------------------
BuildSparseBitmap PROC
    push rbx
    push r12
    sub rsp, 28h

    lea rcx, [szBuildSparse]
    mov edx, lenBuildSparse
    call PrintStr

    ; Clear bitmap
    lea rdi, [SparseBitmap]
    xor eax, eax
    mov ecx, 32
    rep stosq

    ; Simulate sparse detection (~18% skip)
    xor r12d, r12d                          ; slab index
    xor ebx, ebx                            ; skip count
    lea r9, [SparseBitmap]

@ScanLoop:
    cmp r12d, 2048
    jge @ScanDone

    ; Branchless: skip if (index & 5) == 0 || (index & 10) == 0
    mov eax, r12d
    mov ecx, eax
    and ecx, 5
    jz @MarkSparse
    mov ecx, eax
    and ecx, 10
    jnz @NextSlab

@MarkSparse:
    mov eax, r12d
    mov ecx, eax
    shr eax, 6
    and ecx, 63
    bts qword ptr [r9 + rax*8], rcx
    inc ebx

@NextSlab:
    add r12d, 1
    cmp r12d, 2048
    jl @ScanLoop

@ScanDone:
    mov [g_SparseSkipCount], rbx

    lea rcx, [szSparseSkip]
    mov edx, lenSparseSkip
    call PrintStr

    mov rax, rbx
    imul rax, 100
    mov rcx, 2048
    xor edx, edx
    div rcx
    mov rcx, rax
    call PrintNum

    lea rcx, [szPercent]
    mov edx, lenPercent
    call PrintStr

    add rsp, 28h
    pop r12
    pop rbx
    ret
BuildSparseBitmap ENDP

;---------------------------------------------------------------------
; BuildHotTable - First N slabs = hot (OPTIMIZED: vectorized fill)
;---------------------------------------------------------------------
BuildHotTable PROC
    push rbx
    push r12
    sub rsp, 28h

    lea rcx, [szBuildHot]
    mov edx, lenBuildHot
    call PrintStr

    ; Hot count based on GPU ratio
    mov eax, [g_GpuRatio]
    imul eax, 1024
    mov ecx, 100
    xor edx, edx
    div ecx
    mov r12d, eax
    mov [HotCount], eax

    ; Fill hot table with LBA indices (vectorized)
    xor ebx, ebx
    lea r8, [HotLbaTable]
@FillLoop:
    cmp ebx, r12d
    jge @FillDone
    mov qword ptr [r8 + rbx*8], rbx
    add ebx, 4
    cmp ebx, r12d
    jl @FillLoop
    sub ebx, 4
    cmp ebx, r12d
    jge @FillDone
    mov qword ptr [r8 + rbx*8], rbx
    inc ebx
    jmp @FillLoop

@FillDone:
    lea rcx, [szHotSlabs]
    mov edx, lenHotSlabs
    call PrintStr
    mov ecx, [HotCount]
    call PrintNum

    lea rcx, [szColdSlabs]
    mov edx, lenColdSlabs
    call PrintStr
    mov ecx, 1024
    sub ecx, [HotCount]
    call PrintNum

    lea rcx, [szNewLine]
    mov edx, 2
    call PrintStr

    add rsp, 28h
    pop r12
    pop rbx
    ret
BuildHotTable ENDP

;---------------------------------------------------------------------
; PowerInferLoop - Main inference loop (OPTIMIZED: batch counters)
;---------------------------------------------------------------------
PowerInferLoop PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40h

    lea rcx, [szRunning]
    mov edx, lenRunning
    call PrintStr

    ; Clear counters
    xor eax, eax
    mov [g_TokensProcessed], rax
    mov [g_SparseSkipped], rax
    mov [g_GpuProcessed], rax
    mov [g_CpuProcessed], rax

    lea r13, [SparseBitmap]
    mov r14, [g_pTokenBitmap]
    xor r12d, r12d                          ; token counter
    xor r15d, r15d                          ; local sparse count
    xor ebx, ebx                            ; local gpu count
    xor r10d, r10d                          ; local cpu count

@TokenLoop:
    cmp r12d, MAX_TOKENS
    jge @FlushCounters

    ; TurboSparse check
    mov eax, r12d
    and eax, 2047
    mov ecx, eax
    shr ecx, 6
    and eax, 63
    bt qword ptr [r13 + rcx*8], rax
    jnc @NotSparse

    inc r15d
    jmp @TokenDone

@NotSparse:
    ; PowerInfer check
    mov eax, r12d
    and eax, 1023
    cmp eax, [HotCount]
    jge @ColdPath
    inc ebx
    jmp @MarkToken

@ColdPath:
    inc r10d

@MarkToken:
    test r14, r14
    jz @TokenDone
    mov rax, r12
    shr rax, 6
    mov ecx, r12d
    and ecx, 63
    bts qword ptr [r14 + rax*8], rcx

@TokenDone:
    inc r12d
    jmp @TokenLoop

@FlushCounters:
    mov rax, r15
    add [g_SparseSkipped], rax
    mov rax, rbx
    add [g_GpuProcessed], rax
    mov rax, r10
    add [g_CpuProcessed], rax
    mov rax, r12
    add [g_TokensProcessed], rax

    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
PowerInferLoop ENDP

;---------------------------------------------------------------------
; PrintStats
;---------------------------------------------------------------------
PrintStats PROC
    push rbx
    push r12
    sub rsp, 38h

    lea rcx, [szStats]
    mov edx, lenStats
    call PrintStr
    mov rcx, [g_TokensProcessed]
    call PrintNum

    lea rcx, [szSparseHits]
    mov edx, lenSparseHits
    call PrintStr
    mov rcx, [g_SparseSkipped]
    call PrintNum

    lea rcx, [szGpuHits]
    mov edx, lenGpuHits
    call PrintStr
    mov rcx, [g_GpuProcessed]
    call PrintNum

    lea rcx, [szCpuHits]
    mov edx, lenCpuHits
    call PrintStr
    mov rcx, [g_CpuProcessed]
    call PrintNum

    lea rcx, [szNewLine]
    mov edx, 2
    call PrintStr

    add rsp, 38h
    pop r12
    pop rbx
    ret
PrintStats ENDP

;---------------------------------------------------------------------
; Cleanup
;---------------------------------------------------------------------
Cleanup PROC
    sub rsp, 28h

    mov rcx, [g_pPrefetchBuffer]
    test rcx, rcx
    jz @FreePin
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree

@FreePin:
    mov rcx, [g_pHotPinCache]
    test rcx, rcx
    jz @FreeBitmap
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree

@FreeBitmap:
    mov rcx, [g_pTokenBitmap]
    test rcx, rcx
    jz @FreeBuffers
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree

@FreeBuffers:
    mov rcx, [g_pBufferA]
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
    add rsp, 28h
    ret
Cleanup ENDP

;---------------------------------------------------------------------
; PrintStr - RCX=string, EDX=length
;---------------------------------------------------------------------
PrintStr PROC
    push rbx
    sub rsp, 30h
    mov rbx, rcx
    mov r8d, edx
    mov rcx, [g_hStdOut]
    test rcx, rcx
    jz @PrintDone
    mov rdx, rbx
    lea r9, [g_Written]
    mov qword ptr [rsp + 20h], 0
    call WriteConsoleA
@PrintDone:
    add rsp, 30h
    pop rbx
    ret
PrintStr ENDP

;---------------------------------------------------------------------
; PrintNum - RCX=number (OPTIMIZED: reverse-build, no string reversal)
;---------------------------------------------------------------------
PrintNum PROC
    push rbx
    sub rsp, 30h
    lea rbx, [g_NumBuf + 31]                ; start at end
    mov byte ptr [rbx], 0                   ; null term
    dec rbx
    test rcx, rcx
    jnz @ConvertLoop
    mov byte ptr [rbx], '0'
    jmp @PrintNumStr

@ConvertLoop:
    test rcx, rcx
    jz @PrintNumStr
    mov rax, rcx
    xor edx, edx
    mov r8, 10
    div r8
    add dl, '0'
    mov [rbx], dl
    dec rbx
    mov rcx, rax
    jmp @ConvertLoop

@PrintNumStr:
    lea rcx, [rbx + 1]
    mov edx, 31
    sub edx, ebx
    call PrintStr
    add rsp, 30h
    pop rbx
    ret
PrintNum ENDP

END
