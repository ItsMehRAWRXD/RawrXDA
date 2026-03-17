; ============================================================================
; GGUF_CHAIN_API.ASM - GGUF Loading/Streaming/Wrapper Chain API Engine
; Unified API that cycles loading methods and stacks them for performance
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

; Win32 API
CreateFileA PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
ReadFile PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
SetFilePointer PROTO :DWORD,:DWORD,:DWORD,:DWORD
CloseHandle PROTO :DWORD
GetFileSize PROTO :DWORD,:DWORD
CreateFileMappingA PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
MapViewOfFile PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
UnmapViewOfFile PROTO :DWORD
HeapAlloc PROTO :DWORD,:DWORD,:DWORD
HeapFree PROTO :DWORD,:DWORD,:DWORD
GetProcessHeap PROTO
GetTickCount PROTO

; Cloud API
CloudAPI_Init PROTO :DWORD,:DWORD,:DWORD
CloudAPI_DownloadModel PROTO :DWORD,:DWORD

includelib kernel32.lib

; External loaders
DiscLoader_Init PROTO
DiscLoader_OpenModel PROTO :DWORD,:DWORD
DiscLoader_ReadSector PROTO :DWORD,:DWORD
DiscLoader_CloseModel PROTO :DWORD

ReverseQuantize_Init PROTO
ReverseQuantize_CompressToFit PROTO :DWORD,:DWORD

PiRam_EnableHalving PROTO :DWORD
PiRam_CompressGGUF PROTO :DWORD
PiRam_PerfectCircleFwd PROTO :DWORD,:DWORD
PiRam_PerfectCircleInv PROTO :DWORD,:DWORD

; ============================================================================
; CONSTANTS
; ============================================================================
NULL                equ 0
TRUE                equ 1
FALSE               equ 0
INVALID_HANDLE_VALUE equ -1
GENERIC_READ        equ 80000000h
FILE_SHARE_READ     equ 1h
OPEN_EXISTING       equ 3h
PAGE_READONLY       equ 2h
FILE_MAP_READ       equ 4h
HEAP_ZERO_MEMORY    equ 8h

; Loading methods
METHOD_DISC         equ 0       ; Disc-based streaming (CD-ROM style)
METHOD_MEMORY       equ 1       ; Full memory load
METHOD_MMAP         equ 2       ; Memory-mapped file
METHOD_HYBRID       equ 3       ; Hybrid disc+memory
METHOD_AUTO         equ 4       ; Auto-select best method
METHOD_CLOUD        equ 5       ; Cloud fetch then local load

; Chain modes
CHAIN_SEQUENTIAL    equ 0       ; Try methods sequentially
CHAIN_PARALLEL      equ 1       ; Stack methods in parallel
CHAIN_ADAPTIVE      equ 2       ; Adapt based on performance

; Performance tiers
TIER_FAST           equ 0       ; <1s load time
TIER_NORMAL         equ 1       ; 1-5s load time
TIER_SLOW           equ 2       ; >5s load time

; ============================================================================
; EXPORTS
; ============================================================================
PUBLIC GGUFChain_Init
PUBLIC GGUFChain_LoadModel
PUBLIC GGUFChain_StreamChunk
PUBLIC GGUFChain_CloseModel
PUBLIC GGUFChain_SetMethod
PUBLIC GGUFChain_SetChainMode
PUBLIC GGUFChain_GetPerformance
PUBLIC GGUFChain_CycleMethod
PUBLIC GGUFChain_StackMethods
PUBLIC GGUFChain_LoadModelCloud

; ============================================================================
; CHAIN HANDLE STRUCTURE
; ============================================================================
CHAIN_HANDLE STRUCT
    hFile           DWORD ?     ; File handle
    hMapping        DWORD ?     ; Memory mapping handle
    pMappedView     DWORD ?     ; Mapped view pointer
    hDisc           DWORD ?     ; Disc loader handle
    dwFileSize      DWORD ?     ; File size
    dwCurrentMethod DWORD ?     ; Active loading method
    dwChainMode     DWORD ?     ; Chain mode
    dwMethodMask    DWORD ?     ; Bitmask of available methods
    dwPerfTier      DWORD ?     ; Performance tier
    dwLoadTime      DWORD ?     ; Last load time (ms)
    pBuffer         DWORD ?     ; Memory buffer
    dwBufferSize    DWORD ?     ; Buffer size
    dwFlags         DWORD ?     ; Flags
    dwReserved      DWORD ?     ; Reserved
CHAIN_HANDLE ENDS

; ============================================================================
; DATA
; ============================================================================
.data
    g_ChainHandles  CHAIN_HANDLE 128 dup(<>)
    g_ActiveChains  dd 0
    g_DefaultMethod dd METHOD_AUTO
    g_DefaultChainMode dd CHAIN_ADAPTIVE
    
    ; Performance tracking
    g_MethodStats   dd METHOD_AUTO+1 dup(0)  ; Load counts per method
    g_MethodTimes   dd METHOD_AUTO+1 dup(0)  ; Avg times per method
    
.data?
    g_hHeap         dd ?
    g_Initialized   dd ?

; ============================================================================
; CODE
; ============================================================================
.code

; ============================================================================
; GGUFChain_Init - Initialize chain API engine
; Returns: EAX = 1 success
; ============================================================================
GGUFChain_Init PROC
    cmp g_Initialized, 1
    je @already_init
    
    invoke GetProcessHeap
    mov g_hHeap, eax
    
    ; Initialize subsystems
    invoke DiscLoader_Init
    invoke ReverseQuantize_Init
    invoke PiRam_EnableHalving, TRUE
    
    mov g_Initialized, 1
    mov eax, 1
    ret

@already_init:
    mov eax, 1
    ret
GGUFChain_Init ENDP

; ============================================================================
; GGUFChain_SelectMethod - Select optimal loading method
; dwFileSize: File size
; Returns: EAX = METHOD_* constant
; ============================================================================
GGUFChain_SelectMethod PROC dwFileSize:DWORD
    push ebx
    
    mov eax, dwFileSize
    
    ; <4MB: Full memory load
    cmp eax, 400000h
    jb @use_memory
    
    ; <256MB: Memory-mapped
    cmp eax, 10000000h
    jb @use_mmap
    
    ; <4GB: Hybrid disc+memory
    cmp eax, 0F0000000h
    jb @use_hybrid
    
    ; >4GB: Pure disc streaming
    mov eax, METHOD_DISC
    jmp @done

@use_hybrid:
    mov eax, METHOD_HYBRID
    jmp @done
    
@use_mmap:
    mov eax, METHOD_MMAP
    jmp @done
    
@use_memory:
    mov eax, METHOD_MEMORY

@done:
    pop ebx
    ret
GGUFChain_SelectMethod ENDP

; ============================================================================
; GGUFChain_LoadModel - Load model using chain API
; pPath: File path, dwMethod: METHOD_* (or METHOD_AUTO)
; Returns: EAX = chain handle (non-zero), 0 on fail
; ============================================================================
GGUFChain_LoadModel PROC pPath:DWORD, dwMethod:DWORD
    LOCAL hFile:DWORD
    LOCAL fileSize:DWORD
    LOCAL sizeHigh:DWORD
    LOCAL selectedMethod:DWORD
    LOCAL startTime:DWORD
    
    push esi
    push edi
    
    ; Start performance timer
    invoke GetTickCount
    mov startTime, eax
    
    ; Open file
    invoke CreateFileA, pPath, GENERIC_READ, FILE_SHARE_READ, NULL, \
            OPEN_EXISTING, 0, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @fail
    mov hFile, eax
    
    ; Get file size
    invoke GetFileSize, hFile, addr sizeHigh
    mov fileSize, eax
    
    ; Select method (auto or specified)
    mov eax, dwMethod
    cmp eax, METHOD_AUTO
    jne @use_specified
    
    invoke GGUFChain_SelectMethod, fileSize
    mov selectedMethod, eax
    jmp @method_selected
    
@use_specified:
    mov selectedMethod, eax

@method_selected:
    ; Find free chain handle
    mov esi, offset g_ChainHandles
    mov edi, 0
@find_slot:
    cmp edi, 128
    jge @fail_slot
    
    cmp [esi].CHAIN_HANDLE.hFile, 0
    je @slot_found
    
    add esi, sizeof CHAIN_HANDLE
    inc edi
    jmp @find_slot

@slot_found:
    ; Initialize chain handle
    mov eax, hFile
    mov [esi].CHAIN_HANDLE.hFile, eax
    mov dword ptr [esi].CHAIN_HANDLE.hMapping, 0
    mov dword ptr [esi].CHAIN_HANDLE.pMappedView, 0
    mov dword ptr [esi].CHAIN_HANDLE.hDisc, 0
    mov eax, fileSize
    mov [esi].CHAIN_HANDLE.dwFileSize, eax
    mov eax, selectedMethod
    mov [esi].CHAIN_HANDLE.dwCurrentMethod, eax
    mov eax, g_DefaultChainMode
    mov [esi].CHAIN_HANDLE.dwChainMode, eax
    mov dword ptr [esi].CHAIN_HANDLE.dwMethodMask, 0Fh  ; All methods available
    
    ; Execute loading based on method
    mov eax, selectedMethod
    cmp eax, METHOD_DISC
    je @load_disc
    cmp eax, METHOD_MEMORY
    je @load_memory
    cmp eax, METHOD_MMAP
    je @load_mmap
    cmp eax, METHOD_HYBRID
    je @load_hybrid
    
    ; Default: memory load
    jmp @load_memory

@load_disc:
    ; Disc-based streaming
    invoke DiscLoader_OpenModel, pPath, 2  ; INT8 quantization
    mov [esi].CHAIN_HANDLE.hDisc, eax
    jmp @load_complete

@load_memory:
    ; Full memory load
    invoke HeapAlloc, g_hHeap, HEAP_ZERO_MEMORY, fileSize
    test eax, eax
    jz @fail_memory
    mov [esi].CHAIN_HANDLE.pBuffer, eax
    mov eax, fileSize
    mov [esi].CHAIN_HANDLE.dwBufferSize, eax
    
    ; Read file into buffer
    invoke ReadFile, hFile, [esi].CHAIN_HANDLE.pBuffer, fileSize, addr fileSize, NULL
    jmp @load_complete

@load_mmap:
    ; Memory-mapped file
    invoke CreateFileMappingA, hFile, NULL, PAGE_READONLY, 0, 0, NULL
    test eax, eax
    jz @fail_mmap
    mov [esi].CHAIN_HANDLE.hMapping, eax
    
    invoke MapViewOfFile, eax, FILE_MAP_READ, 0, 0, 0
    test eax, eax
    jz @fail_mmap
    mov [esi].CHAIN_HANDLE.pMappedView, eax
    jmp @load_complete

@load_hybrid:
    ; Hybrid: mmap for metadata, disc for tensors
    invoke CreateFileMappingA, hFile, NULL, PAGE_READONLY, 0, 10000h, NULL
    test eax, eax
    jz @use_disc_only
    mov [esi].CHAIN_HANDLE.hMapping, eax
    
    invoke MapViewOfFile, eax, FILE_MAP_READ, 0, 0, 10000h
    mov [esi].CHAIN_HANDLE.pMappedView, eax
    
@use_disc_only:
    invoke DiscLoader_OpenModel, pPath, 2
    mov [esi].CHAIN_HANDLE.hDisc, eax

@load_complete:
    ; Calculate load time
    invoke GetTickCount
    sub eax, startTime
    mov [esi].CHAIN_HANDLE.dwLoadTime, eax
    
    ; Determine performance tier
    cmp eax, 1000
    jb @tier_fast
    cmp eax, 5000
    jb @tier_normal
    mov dword ptr [esi].CHAIN_HANDLE.dwPerfTier, TIER_SLOW
    jmp @tier_set
@tier_normal:
    mov dword ptr [esi].CHAIN_HANDLE.dwPerfTier, TIER_NORMAL
    jmp @tier_set
@tier_fast:
    mov dword ptr [esi].CHAIN_HANDLE.dwPerfTier, TIER_FAST
@tier_set:
    
    ; Update stats
    mov eax, selectedMethod
    lea ecx, g_MethodStats
    inc dword ptr [ecx + eax*4]
    
    inc g_ActiveChains
    
    ; Return handle (1-based)
    mov eax, edi
    inc eax
    jmp @exit

@fail_mmap:
@fail_memory:
@fail_slot:
    invoke CloseHandle, hFile
@fail:
    xor eax, eax

@exit:
    pop edi
    pop esi
    ret
GGUFChain_LoadModel ENDP

; ============================================================================
; GGUFChain_StreamChunk - Stream chunk from model (wrapper)
; hChain: Chain handle, pBuffer: Output buffer, dwSize: Chunk size
; Returns: EAX = bytes read
; ============================================================================
GGUFChain_StreamChunk PROC hChain:DWORD, pBuffer:DWORD, dwSize:DWORD
    LOCAL pHandle:DWORD
    
    push esi
    
    ; Validate and get handle
    mov eax, hChain
    test eax, eax
    jz @stream_fail
    dec eax
    cmp eax, 128
    jge @stream_fail
    
    mov esi, offset g_ChainHandles
    imul eax, sizeof CHAIN_HANDLE
    add esi, eax
    mov pHandle, esi
    
    ; Route to appropriate streaming method
    mov eax, [esi].CHAIN_HANDLE.dwCurrentMethod
    cmp eax, METHOD_DISC
    je @stream_disc
    cmp eax, METHOD_MEMORY
    je @stream_memory
    cmp eax, METHOD_MMAP
    je @stream_mmap
    
    ; Default: fail
    jmp @stream_fail

@stream_disc:
    invoke DiscLoader_ReadSector, [esi].CHAIN_HANDLE.hDisc, pBuffer
    jmp @stream_done

@stream_memory:
    ; Copy from buffer
    mov ecx, dwSize
    mov edi, pBuffer
    mov esi, pHandle
    mov esi, [esi].CHAIN_HANDLE.pBuffer
    rep movsb
    mov eax, dwSize
    jmp @stream_done

@stream_mmap:
    ; Copy from mapped view
    mov ecx, dwSize
    mov edi, pBuffer
    mov esi, pHandle
    mov esi, [esi].CHAIN_HANDLE.pMappedView
    rep movsb
    mov eax, dwSize
    jmp @stream_done

@stream_fail:
    xor eax, eax

@stream_done:
    pop esi
    ret
GGUFChain_StreamChunk ENDP

; ============================================================================
; GGUFChain_CloseModel - Close chain handle and cleanup
; hChain: Chain handle
; Returns: EAX = 1 success
; ============================================================================
GGUFChain_CloseModel PROC hChain:DWORD
    mov eax, hChain
    test eax, eax
    jz @close_fail
    dec eax
    cmp eax, 128
    jge @close_fail
    
    mov ecx, offset g_ChainHandles
    imul eax, sizeof CHAIN_HANDLE
    add ecx, eax
    
    ; Close disc handle
    mov eax, [ecx].CHAIN_HANDLE.hDisc
    test eax, eax
    jz @no_disc
    invoke DiscLoader_CloseModel, eax
@no_disc:
    
    ; Unmap view
    mov eax, [ecx].CHAIN_HANDLE.pMappedView
    test eax, eax
    jz @no_view
    invoke UnmapViewOfFile, eax
@no_view:
    
    ; Close mapping
    mov eax, [ecx].CHAIN_HANDLE.hMapping
    test eax, eax
    jz @no_mapping
    invoke CloseHandle, eax
@no_mapping:
    
    ; Free buffer
    mov eax, [ecx].CHAIN_HANDLE.pBuffer
    test eax, eax
    jz @no_buffer
    invoke HeapFree, g_hHeap, 0, eax
@no_buffer:
    
    ; Close file
    mov eax, [ecx].CHAIN_HANDLE.hFile
    test eax, eax
    jz @no_file
    invoke CloseHandle, eax
@no_file:
    
    ; Zero handle
    mov dword ptr [ecx].CHAIN_HANDLE.hFile, 0
    
    dec g_ActiveChains
    mov eax, 1
    ret

@close_fail:
    xor eax, eax
    ret
GGUFChain_CloseModel ENDP

; ============================================================================
; GGUFChain_CycleMethod - Cycle to next loading method
; hChain: Chain handle
; Returns: EAX = new method
; ============================================================================
GGUFChain_CycleMethod PROC hChain:DWORD
    mov eax, hChain
    test eax, eax
    jz @cycle_fail
    dec eax
    cmp eax, 128
    jge @cycle_fail
    
    mov ecx, offset g_ChainHandles
    imul eax, sizeof CHAIN_HANDLE
    add ecx, eax
    
    ; Get current and cycle
    mov eax, [ecx].CHAIN_HANDLE.dwCurrentMethod
    inc eax
    cmp eax, METHOD_AUTO
    jbe @cycle_ok
    mov eax, METHOD_DISC
@cycle_ok:
    mov [ecx].CHAIN_HANDLE.dwCurrentMethod, eax
    ret

@cycle_fail:
    mov eax, -1
    ret
GGUFChain_CycleMethod ENDP

; ============================================================================
; GGUFChain_StackMethods - Enable parallel method stacking
; hChain: Chain handle, dwMethodMask: Bitmask of methods to stack
; Returns: EAX = 1 success
; ============================================================================
GGUFChain_StackMethods PROC hChain:DWORD, dwMethodMask:DWORD
    mov eax, hChain
    test eax, eax
    jz @stack_fail
    dec eax
    cmp eax, 128
    jge @stack_fail
    
    mov ecx, offset g_ChainHandles
    imul eax, sizeof CHAIN_HANDLE
    add ecx, eax
    
    mov eax, dwMethodMask
    mov [ecx].CHAIN_HANDLE.dwMethodMask, eax
    mov dword ptr [ecx].CHAIN_HANDLE.dwChainMode, CHAIN_PARALLEL
    
    mov eax, 1
    ret

@stack_fail:
    xor eax, eax
    ret
GGUFChain_StackMethods ENDP

; ============================================================================
; GGUFChain_LoadModelCloud - Download from cloud then load locally
; pModelName: model identifier
; pDestPath: local path to write
; dwProvider: CLOUD_PROVIDER_* constant
; pAuthToken/pProviderUrl: optional (can be NULL)
; Returns: EAX = chain handle, 0 on fail
; ============================================================================
GGUFChain_LoadModelCloud PROC pModelName:DWORD, pDestPath:DWORD, dwProvider:DWORD, pAuthToken:DWORD, pProviderUrl:DWORD
    push ebx
    push esi
    push edi

    ; init cloud provider
    push dwProvider
    push pAuthToken
    push pProviderUrl
    call CloudAPI_Init
    test eax, eax
    jz @fail

    ; download model
    push pDestPath
    push pModelName
    call CloudAPI_DownloadModel
    test eax, eax
    jz @fail

    ; load locally using auto method
    push METHOD_AUTO
    push pDestPath
    call GGUFChain_LoadModel
    test eax, eax
    jz @fail
    jmp @done

@fail:
    xor eax, eax

@done:
    pop edi
    pop esi
    pop ebx
    ret
GGUFChain_LoadModelCloud ENDP

; ============================================================================
; GGUFChain_SetMethod - Set loading method
; dwMethod: METHOD_* constant
; Returns: EAX = 1 success
; ============================================================================
GGUFChain_SetMethod PROC dwMethod:DWORD
    mov eax, dwMethod
    cmp eax, METHOD_AUTO
    ja @invalid
    mov g_DefaultMethod, eax
    mov eax, 1
    ret
@invalid:
    xor eax, eax
    ret
GGUFChain_SetMethod ENDP

; ============================================================================
; GGUFChain_SetChainMode - Set chain mode
; dwMode: CHAIN_* constant
; Returns: EAX = 1 success
; ============================================================================
GGUFChain_SetChainMode PROC dwMode:DWORD
    mov eax, dwMode
    cmp eax, CHAIN_ADAPTIVE
    ja @invalid
    mov g_DefaultChainMode, eax
    mov eax, 1
    ret
@invalid:
    xor eax, eax
    ret
GGUFChain_SetChainMode ENDP

; ============================================================================
; GGUFChain_GetPerformance - Get performance statistics
; hChain: Chain handle, pPerfData: Output buffer (16 bytes)
; Buffer format:
;   [0-3]   Current method
;   [4-7]   Load time (ms)
;   [8-11]  Performance tier
;   [12-15] Method mask
; Returns: EAX = 1 success
; ============================================================================
GGUFChain_GetPerformance PROC hChain:DWORD, pPerfData:DWORD
    push esi
    
    mov eax, hChain
    test eax, eax
    jz @perf_fail
    dec eax
    cmp eax, 128
    jge @perf_fail
    
    mov ecx, offset g_ChainHandles
    imul eax, sizeof CHAIN_HANDLE
    add ecx, eax
    
    mov esi, pPerfData
    test esi, esi
    jz @perf_fail
    
    mov eax, [ecx].CHAIN_HANDLE.dwCurrentMethod
    mov [esi], eax
    mov eax, [ecx].CHAIN_HANDLE.dwLoadTime
    mov [esi+4], eax
    mov eax, [ecx].CHAIN_HANDLE.dwPerfTier
    mov [esi+8], eax
    mov eax, [ecx].CHAIN_HANDLE.dwMethodMask
    mov [esi+12], eax
    
    mov eax, 1
    jmp @perf_exit

@perf_fail:
    xor eax, eax

@perf_exit:
    pop esi
    ret
GGUFChain_GetPerformance ENDP

END
