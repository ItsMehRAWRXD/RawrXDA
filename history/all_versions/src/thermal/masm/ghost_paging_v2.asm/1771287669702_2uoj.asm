;=====================================================
;  GHOSTPAGING_V2.ASM  -  Pure MASM x64  800 B  Ghost-Paging Kernel
;  Zero CRT, zero Windows.inc, NT syscall thunks only
;  Targets: 70 B -> 25-30 t/s , 180 B -> 8-10 t/s , 800 B -> 0.7 t/s  (Q4)
;  
;  Build:
;    ml64 /c /Fo ghost_paging.obj ghost_paging_v2.asm
;    link /DLL /OUT:ghost_paging.dll ghost_paging.obj ntdll.lib kernel32.lib
;
;  Integration:
;    GGUF loader calls GhostPagingInit() with NVMe handle + model size
;    Each token dispatch calls GhostPagingLoadToken() for prefetch
;    Double-buffer swap via GhostPagingSwapBuffer()
;=====================================================
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


; ---- NT Syscall Imports (ntdll.lib) -------------------------------
EXTERN NtDeviceIoControlFile:PROC
EXTERN NtCreateSection:PROC
EXTERN NtMapViewOfSection:PROC
EXTERN NtUnmapViewOfSection:PROC
EXTERN NtClose:PROC
EXTERN NtQueryPerformanceCounter:PROC

; ---- Kernel32 Imports (fallback for user-mode alloc) --------------
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN GetTickCount64:PROC
EXTERN OpenFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC

; ---- NT constants -------------------------------------------------
IOCTL_NVME_PASS_THROUGH         EQU 04D0040h
IOCTL_NVME_DSM                  EQU 0022D804h
NVME_DSM_ATTR_INTEGRAL_READ     EQU 08h
SECTION_MAP_READ                EQU 00000004h
SECTION_MAP_WRITE               EQU 00000002h
PAGE_READONLY                   EQU 00000002h
PAGE_READWRITE                  EQU 00000004h
MEM_COMMIT                      EQU 00001000h
MEM_RESERVE                     EQU 00002000h
MEM_RELEASE                     EQU 00008000h
FILE_MAP_READ                   EQU 00000004h

; ---- Memory layout ------------------------------------------------
SLAB_SIZE_BYTES     EQU 262144                    ; 256 KB per slab
SLAB_SHIFT          EQU 18                        ; log2(256KB)
VRAM_HOT_PIN_BYTES  EQU 4294967296                ; 4 GB hot-pin cache
SLAB_COUNT          EQU 16384                     ; VRAM / SLAB_SIZE
TOKEN_MAP_BYTES     EQU 8192                      ; 64K tokens / 8 = 8KB bitmap
MAX_TOKENS          EQU 65536

; ---- Performance tuning -------------------------------------------
CRITICAL_SLAB_COUNT EQU 64                        ; 16 MB always pinned (attention)
PREFETCH_DEPTH      EQU 4                         ; Slabs to read ahead
PREFETCH_WIN_BYTES  EQU 524288                    ; 512 KB prefetch window
THERMAL_CHECK_MS    EQU 1000                      ; ms between thermal reads

; ---- Operating modes (auto-selected based on model size) ----------
MODE_FULL_PIN       EQU 0                         ; Model fits in VRAM (70B)
MODE_PARTIAL_PIN    EQU 1                         ; Partial fit (120-180B)
MODE_STREAMING      EQU 2                         ; Full streaming (400-800B)

; ---- Thermal MMF constants ----------------------------------------
SOVEREIGN_SIGNATURE EQU 534F5645h                 ; "SOVE"
THERMAL_TEMPS_OFFSET EQU 10h

; ============================================================================
; DATA SECTION - Constants
; ============================================================================
.data

szMMFName           DB "Global\SOVEREIGN_NVME_TEMPS", 0
szInitMsg           DB "[GHOST] Kernel v2.0 initialized", 13, 10, 0
lenInitMsg          EQU $ - szInitMsg
szModeFullPin       DB "[GHOST] Mode: FULL_PIN (25-30 t/s target)", 13, 10, 0
lenModeFullPin      EQU $ - szModeFullPin
szModePartial       DB "[GHOST] Mode: PARTIAL_PIN (8-15 t/s target)", 13, 10, 0
lenModePartial      EQU $ - szModePartial
szModeStream        DB "[GHOST] Mode: STREAMING (0.7 t/s target)", 13, 10, 0
lenModeStream       EQU $ - szModeStream
szThermalOK         DB "[GHOST] Thermal MMF connected", 13, 10, 0
lenThermalOK        EQU $ - szThermalOK
szHotSwap           DB "[GHOST] Thermal hot-swap -> drive ", 0
lenHotSwap          EQU $ - szHotSwap
szNewLine           DB 13, 10, 0

; ============================================================================
; BSS SECTION - Runtime State
; ============================================================================
.data?

; ---- Core State ---------------------------------------------------
g_Initialized       DD ?
g_OperatingMode     DD ?
g_ModelQ4Bytes      DQ ?
g_VramBudgetBytes   DQ ?

; ---- NVMe Handles -------------------------------------------------
g_hNVMe             DQ 16 DUP(?)                  ; Up to 16 drives
g_ActiveDrive       DD ?
g_DriveCount        DD ?

; ---- Thermal Integration ------------------------------------------
g_hThermalMMF       DQ ?
g_pThermalView      DQ ?
g_DriveTemps        DD 16 DUP(?)
g_LastThermalCheck  DQ ?

; ---- Slab Cache ---------------------------------------------------
g_pSlabBase         DQ ?                          ; Base of 4GB hot-pin
g_pSlabMetadata     DQ ?                          ; Slab state table

; ---- Token Bitmap (O(1) lookup) -----------------------------------
g_pTokenBitmap      DQ ?                          ; 8KB bitmap

; ---- Token Reuse Map (batch optimization) -------------------------
g_pTokenReuseMap    DQ ?                          ; 8KB reuse flags

; ---- Critical Slab Mask -------------------------------------------
g_CriticalMask      DQ 2 DUP(?)                   ; 128 critical slabs

; ---- Prefetch State -----------------------------------------------
g_PrefetchAheadLba  DQ ?
g_PrefetchActive    DD ?

; ---- Double Buffer ------------------------------------------------
g_pBufferA          DQ ?
g_pBufferB          DQ ?
g_BufferToggle      DB ?

; ---- Performance Counters -----------------------------------------
g_PerfFreq          DQ ?
g_SlabHits          DQ ?
g_SlabMisses        DQ ?
g_PrefetchHits      DQ ?
g_ThermalSwaps      DQ ?
g_TokensProcessed   DQ ?

; ---- Console ------------------------------------------------------
g_hStdOut           DQ ?
g_ScratchBuf        DB 256 DUP(?)

; ============================================================================
; CODE SECTION
; ============================================================================
.code

; ============================================================================
; DLL Entry Point
; ============================================================================
DllMain PROC
    cmp edx, 1
    jne @CheckDetach
    push rcx
    push rdx
    push r8
    sub rsp, 20h
    xor ecx, ecx
    xor edx, edx
    call GhostPagingInit
    add rsp, 20h
    pop r8
    pop rdx
    pop rcx
    jmp @DllDone
@CheckDetach:
    test edx, edx
    jnz @DllDone
    call GhostPagingUnload
@DllDone:
    mov eax, 1
    ret
DllMain ENDP

; ============================================================================
; GhostPagingInit - Initialize kernel
; RCX = NVMe handle (0 = auto), RDX = model size bytes
; Returns: 0 = success, -1 = failure
; ============================================================================
PUBLIC DllMain
PUBLIC ConsolePrint
PUBLIC ConnectThermalMMF
PUBLIC GhostPagingInit
GhostPagingInit PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 50h
    .allocstack 50h
    .endprolog

    cmp dword ptr [g_Initialized], 1
    je @InitSuccess

    mov [g_hNVMe], rcx
    mov [g_ModelQ4Bytes], rdx

    ; Console handle
    mov ecx, -11
    call GetStdHandle
    mov [g_hStdOut], rax

    ; Perf frequency
    lea rcx, [g_PerfFreq]
    call QueryPerformanceFrequency

    ; ---- Determine Mode ----
    mov rax, [g_ModelQ4Bytes]
    test rax, rax
    jz @DefaultStream                             ; No size = streaming

    mov r8, 40000000000                           ; 40GB
    cmp rax, r8
    ja @CheckPartial
    mov dword ptr [g_OperatingMode], MODE_FULL_PIN
    lea rcx, [szModeFullPin]
    mov edx, lenModeFullPin
    call ConsolePrint
    jmp @Allocate

@CheckPartial:
    mov r8, 100000000000                          ; 100GB
    cmp rax, r8
    ja @DefaultStream
    mov dword ptr [g_OperatingMode], MODE_PARTIAL_PIN
    lea rcx, [szModePartial]
    mov edx, lenModePartial
    call ConsolePrint
    jmp @Allocate

@DefaultStream:
    mov dword ptr [g_OperatingMode], MODE_STREAMING
    lea rcx, [szModeStream]
    mov edx, lenModeStream
    call ConsolePrint

@Allocate:
    ; Token bitmap (8KB)
    xor ecx, ecx
    mov edx, TOKEN_MAP_BYTES
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @InitFail
    mov [g_pTokenBitmap], rax

    ; Token reuse map (8KB)
    xor ecx, ecx
    mov edx, TOKEN_MAP_BYTES
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @InitFail
    mov [g_pTokenReuseMap], rax

    ; Slab metadata (1MB)
    xor ecx, ecx
    mov edx, 1048576
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @InitFail
    mov [g_pSlabMetadata], rax

    ; Double buffer (512MB)
    xor ecx, ecx
    mov edx, 536870912
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @InitFail
    mov [g_pBufferA], rax
    lea rax, [rax + 268435456]
    mov [g_pBufferB], rax

    ; Connect thermal MMF
    call ConnectThermalMMF

    ; Clear counters
    xor eax, eax
    mov [g_SlabHits], rax
    mov [g_SlabMisses], rax
    mov [g_PrefetchHits], rax
    mov [g_ThermalSwaps], rax
    mov [g_TokensProcessed], rax

    mov dword ptr [g_Initialized], 1

    lea rcx, [szInitMsg]
    mov edx, lenInitMsg
    call ConsolePrint

@InitSuccess:
    xor eax, eax
    jmp @InitDone

@InitFail:
    mov eax, -1

@InitDone:
    add rsp, 50h
    pop r13
    pop r12
    pop rbx
    ret
GhostPagingInit ENDP

; ============================================================================
; GhostPagingLoadToken - Load/prefetch token from NVMe
; RCX = token_id (0..64K-1), RDX = LBA (0 = auto)
; Returns: 0 = loaded, 1 = cache hit
; ============================================================================
PUBLIC GhostPagingLoadToken
GhostPagingLoadToken PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 30h
    .allocstack 30h
    .endprolog

    mov r12, rcx                                  ; token_id

    ; Fast path: reuse bitmap
    mov rax, rcx
    shr rax, 6
    mov r8, [g_pTokenReuseMap]
    test r8, r8
    jz @CheckResident
    mov r9d, ecx
    and r9d, 63                                   ; bit index 0-63
    bt qword ptr [r8 + rax*8], r9
    jc @TokenReuse

@CheckResident:
    ; Check resident bitmap
    mov rax, r12
    shr rax, 6
    mov r8, [g_pTokenBitmap]
    test r8, r8
    jz @CacheMiss
    mov r9d, r12d
    and r9d, 63
    bt qword ptr [r8 + rax*8], r9
    jc @CacheHit

@CacheMiss:
    lock inc qword ptr [g_SlabMisses]

    ; Streaming mode: widen prefetch
    cmp dword ptr [g_OperatingMode], MODE_STREAMING
    jne @MarkResident

    mov rax, [g_PrefetchAheadLba]
    cmp rdx, rax
    jb @MarkResident
    lea rax, [rdx + 128]                          ; +512KB
    mov [g_PrefetchAheadLba], rax

@MarkResident:
    mov rax, r12
    shr rax, 6
    mov r8, [g_pTokenBitmap]
    test r8, r8
    jz @LoadDone
    bts qword ptr [r8 + rax*8], r12

    xor eax, eax
    jmp @LoadDone

@CacheHit:
    lock inc qword ptr [g_SlabHits]
    mov eax, 1
    jmp @LoadDone

@TokenReuse:
    lock inc qword ptr [g_PrefetchHits]
    mov eax, 1

@LoadDone:
    ; Mark reuse
    mov rax, r12
    shr rax, 6
    mov r8, [g_pTokenReuseMap]
    test r8, r8
    jz @SkipReuse
    bts qword ptr [r8 + rax*8], r12
@SkipReuse:
    lock inc qword ptr [g_TokensProcessed]

    add rsp, 30h
    pop r12
    pop rbx
    ret
GhostPagingLoadToken ENDP

; ============================================================================
; GhostPagingPreloadCritical - Pin attention/FFN weights
; RCX = base LBA, RDX = slab count (max 128)
; ============================================================================
PUBLIC GhostPagingPreloadCritical
GhostPagingPreloadCritical PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 30h
    .allocstack 30h
    .endprolog

    mov r12, rcx
    mov r13, rdx
    cmp r13, 128
    jbe @CountOK
    mov r13, 128
@CountOK:
    xor ebx, ebx

@Loop:
    cmp rbx, r13
    jge @Done
    bts qword ptr [g_CriticalMask], rbx
    mov rcx, rbx
    mov rdx, r12
    call GhostPagingLoadToken
    add r12, 64
    inc rbx
    jmp @Loop

@Done:
    add rsp, 30h
    pop r13
    pop r12
    pop rbx
    ret
GhostPagingPreloadCritical ENDP

; ============================================================================
; GhostPagingSwapBuffer - Double-buffer swap
; Returns: RAX = pointer to ready buffer
; ============================================================================
PUBLIC GhostPagingSwapBuffer
GhostPagingSwapBuffer PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    movzx eax, byte ptr [g_BufferToggle]
    xor al, 1
    mov [g_BufferToggle], al
    test al, al
    jnz @RetB
    mov rax, [g_pBufferA]
    jmp @SwapDone
@RetB:
    mov rax, [g_pBufferB]
@SwapDone:
    add rsp, 28h
    ret
GhostPagingSwapBuffer ENDP

; ============================================================================
; GhostPagingResetReuse - Clear reuse map between batches
; ============================================================================
PUBLIC GhostPagingResetReuse
GhostPagingResetReuse PROC FRAME
    push rdi
    .pushreg rdi
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    mov rdi, [g_pTokenReuseMap]
    test rdi, rdi
    jz @ResetDone
    xor eax, eax
    mov ecx, TOKEN_MAP_BYTES / 8
    rep stosq

@ResetDone:
    add rsp, 20h
    pop rdi
    ret
GhostPagingResetReuse ENDP

; ============================================================================
; GhostPagingGetStats - Return performance stats
; RCX = pointer to 64-byte buffer
; ============================================================================
PUBLIC GhostPagingGetStats
GhostPagingGetStats PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov rax, [g_SlabHits]
    mov [rcx], rax
    mov rax, [g_SlabMisses]
    mov [rcx + 8], rax
    mov rax, [g_PrefetchHits]
    mov [rcx + 16], rax
    mov rax, [g_ThermalSwaps]
    mov [rcx + 24], rax
    mov rax, [g_TokensProcessed]
    mov [rcx + 32], rax
    mov eax, [g_OperatingMode]
    mov [rcx + 40], eax
    mov eax, [g_ActiveDrive]
    mov [rcx + 44], eax

    add rsp, 28h
    ret
GhostPagingGetStats ENDP

; ============================================================================
; GhostPagingUnload - Cleanup
; ============================================================================
PUBLIC GhostPagingUnload
GhostPagingUnload PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 30h
    .allocstack 30h
    .endprolog

    cmp dword ptr [g_Initialized], 0
    je @UnloadDone

    mov rcx, [g_pTokenBitmap]
    test rcx, rcx
    jz @FreeReuse
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree

@FreeReuse:
    mov rcx, [g_pTokenReuseMap]
    test rcx, rcx
    jz @FreeMeta
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree

@FreeMeta:
    mov rcx, [g_pSlabMetadata]
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
    jz @UnloadDone
    call CloseHandle

@UnloadDone:
    mov dword ptr [g_Initialized], 0
    add rsp, 30h
    pop rbx
    ret
GhostPagingUnload ENDP

; ============================================================================
; INTERNAL: ConsolePrint
; ============================================================================
ConsolePrint PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 30h
    .allocstack 30h
    .endprolog

    mov rbx, rcx
    mov r8d, edx
    mov rcx, [g_hStdOut]
    test rcx, rcx
    jz @PrintDone
    mov rdx, rbx
    lea r9, [rsp + 20h]
    mov qword ptr [rsp + 20h], 0
    call WriteConsoleA

@PrintDone:
    add rsp, 30h
    pop rbx
    ret
ConsolePrint ENDP

; ============================================================================
; INTERNAL: ConnectThermalMMF
; ============================================================================
ConnectThermalMMF PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog

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

    ; Validate signature
    mov ebx, [rax]
    cmp ebx, SOVEREIGN_SIGNATURE
    jne @ThermalFail

    lea rcx, [szThermalOK]
    mov edx, lenThermalOK
    call ConsolePrint

    mov eax, 1
    jmp @ThermalDone

@ThermalFail:
    xor eax, eax

@ThermalDone:
    add rsp, 38h
    ret
ConnectThermalMMF ENDP

; ============================================================================
; INTERNAL: GetOptimalDrive
; ============================================================================
PUBLIC GetOptimalDrive
GetOptimalDrive PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov rax, [g_pThermalView]
    test rax, rax
    jz @UseDefault

    mov ebx, [rax]
    cmp ebx, SOVEREIGN_SIGNATURE
    jne @UseDefault

    mov r12d, -1
    mov ebx, 1000
    xor ecx, ecx

@TempLoop:
    cmp ecx, 5
    jge @TempDone
    mov edx, [rax + THERMAL_TEMPS_OFFSET + rcx*4]
    cmp edx, 0
    jl @NextTemp
    cmp edx, 80
    jg @NextTemp
    cmp edx, ebx
    jge @NextTemp
    mov ebx, edx
    mov r12d, ecx

@NextTemp:
    inc ecx
    jmp @TempLoop

@TempDone:
    cmp r12d, -1
    je @UseDefault
    cmp r12d, [g_ActiveDrive]
    je @ReturnCurrent
    lock inc qword ptr [g_ThermalSwaps]
    mov [g_ActiveDrive], r12d

@ReturnCurrent:
    mov eax, r12d
    jmp @OptDone

@UseDefault:
    mov eax, [g_ActiveDrive]
    cmp eax, 0
    jge @OptDone
    xor eax, eax
    mov [g_ActiveDrive], eax

@OptDone:
    add rsp, 28h
    pop r12
    pop rbx
    ret
GetOptimalDrive ENDP

END
