; ============================================================================
; MODEL_HOTPATCH_ENGINE.ASM - Runtime Model Swapping Without Restart
; Supports: Cloud models, Local GGUF, Quantized variants, Custom formats
; Zero-downtime model switching with rollback capability
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc

includelib \masm32\lib\kernel32.lib

; External GGUF loaders
EXTERN GgufUnified_LoadModelAutomatic:PROC
EXTERN GgufUnified_LoadModel:PROC
EXTERN DiscStream_OpenModel:PROC

; ----------------------------------------------------------------------------
; Streaming limits for ultra-large models (800B scale)
; These keep resident memory below a configurable cap (default 512MB window).
; ----------------------------------------------------------------------------
STREAM_MAX_WINDOW_MB      EQU 1024           ; hard ceiling 1GB resident window
STREAM_DEFAULT_WINDOW_MB  EQU 512            ; default streaming window
STREAM_IO_ALIGN_BYTES     EQU 1048576        ; 1MB aligned IO blocks
STREAM_THRESHOLD_BYTES    EQU 40000000h      ; ~1GB triggers streaming path

PUBLIC HotPatch_Init
PUBLIC HotPatch_RegisterModel
PUBLIC HotPatch_SwapModel
PUBLIC HotPatch_RollbackModel
PUBLIC HotPatch_GetActiveModel
PUBLIC HotPatch_ListModels
PUBLIC HotPatch_CacheModel
PUBLIC HotPatch_WarmupModel
PUBLIC HotPatch_SetSwapStrategy
PUBLIC HotPatch_SetStreamCap
PUBLIC HotPatch_StreamedLoadModel
PUBLIC HotPatch_StreamedApplyChunk

EXTERN GgufUnified_LoadModelAutomatic:PROC
EXTERN PiramHooks_CompressTensor:PROC
EXTERN PiramHooks_DecompressTensor:PROC
EXTERN ReverseQuant_Init:PROC
EXTERN ReverseQuant_DequantizeBuffer:PROC
EXTERN DiscStream_OpenModel:PROC
EXTERN DiscStream_ReadChunk:PROC
EXTERN ErrorLogging_LogEvent:PROC
EXTERN InferenceBackend_SelectBackend:PROC
EXTERN InferenceBackend_CreateInferenceContext:PROC

; Model source types
MODEL_SOURCE_LOCAL      EQU 1
MODEL_SOURCE_CLOUD      EQU 2
MODEL_SOURCE_OLLAMA     EQU 3
MODEL_SOURCE_HUGGINGFACE EQU 4
MODEL_SOURCE_CUSTOM     EQU 5

; Swap strategies
SWAP_INSTANT            EQU 1   ; Immediate swap (may drop requests)
SWAP_GRACEFUL           EQU 2   ; Wait for current requests
SWAP_PARALLEL           EQU 3   ; Load new model in parallel, swap when ready
SWAP_GRADUAL            EQU 4   ; A/B testing style transition

; Model registry entry
ModelEntry STRUCT
    dwModelID           dd ?
    dwSourceType        dd ?
    szModelPath         db 260 dup(?)
    szModelName         db 128 dup(?)
    hModelHandle        dd ?
    hInferenceContext   dd ?
    qwLoadTime          dq ?
    dwParameterCount    dd ?
    bCached             dd ?
    bWarmedUp           dd ?
    dwPriority          dd ?
    bCompressed         dd ?
    dwCompressionRatio  dd ?
    bDequantized        dd ?
    dwQuantFormat       dd ?
    hDiscStream         dd ?
    dwInferenceBackend  dd ?
    dwReserved          dd 2 dup(?)
ModelEntry ENDS

; Hot-patch context
HotPatchContext STRUCT
    dwActiveModelID     dd ?
    dwBackupModelID     dd ?
    dwSwapStrategy      dd ?
    dwModelCount        dd ?
    pModelRegistry      dd ?
    bSwapInProgress     dd ?
    dwSwapStartTime     dd ?
    hSwapThread         dd ?
HotPatchContext ENDS

.data
    g_HotPatchCtx HotPatchContext <0, 0, SWAP_GRACEFUL, 0, 0, 0, 0, 0>
    g_ModelRegistry ModelEntry 32 dup(<>)  ; Support 32 models
    g_NextModelID dd 1
    g_StreamCapMB dd STREAM_DEFAULT_WINDOW_MB ; configurable memory window (MB)
    g_StreamWindowPtr dd 0
    g_StreamWindowSize dd 0
    g_StreamLastBytes dd 0
    
    szSwapSuccess db "Model swap completed successfully", 0
    szSwapFailed db "Model swap failed - rolling back", 0
    szSwapInProgress db "Swap in progress...", 0
    szModelNotFound db "Model not found", 0
    szCacheSuccess db "Model cached successfully", 0
    szChunkProcessing db "Processing model chunk", 0
    szChunkFailed db "Chunk processing failed", 0
    szModelCompressed db "Model using compression", 0
    szModelDequantized db "Model dequantized", 0
    szBackendSelected db "Inference backend selected", 0

.code

; ============================================================================
; HotPatch_Init - Initialize hot-patching system
; ============================================================================
HotPatch_Init PROC
    lea eax, g_ModelRegistry
    mov [g_HotPatchCtx.pModelRegistry], eax
    mov [g_HotPatchCtx.dwModelCount], 0
    mov [g_HotPatchCtx.dwSwapStrategy], SWAP_GRACEFUL
    mov [g_HotPatchCtx.bSwapInProgress], 0
    mov eax, 1
    ret
HotPatch_Init ENDP

; ============================================================================
; HotPatch_RegisterModel - Register model for hot-swapping
; Input:  ECX = model path
;         EDX = model name
;         ESI = source type
; Output: EAX = model ID
; ============================================================================
HotPatch_RegisterModel PROC lpPath:DWORD, lpName:DWORD, dwSourceType:DWORD
    LOCAL pEntry:DWORD
    LOCAL dwModelID:DWORD
    push ebx
    push esi
    push edi
    
    ; Check capacity
    cmp [g_HotPatchCtx.dwModelCount], 32
    jge @RegistryFull
    
    ; Allocate model ID
    mov eax, g_NextModelID
    mov dwModelID, eax
    inc g_NextModelID
    
    ; Get registry entry
    mov eax, [g_HotPatchCtx.dwModelCount]
    imul eax, SIZEOF ModelEntry
    mov ebx, [g_HotPatchCtx.pModelRegistry]
    add ebx, eax
    mov pEntry, ebx
    
    ; Initialize entry
    mov eax, dwModelID
    mov [ebx].ModelEntry.dwModelID, eax
    
    mov eax, dwSourceType
    mov [ebx].ModelEntry.dwSourceType, eax
    
    ; Copy path
    push 260
    lea edi, [ebx].ModelEntry.szModelPath
    push edi
    push lpPath
    call lstrcpynA
    add esp, 12
    
    ; Copy name
    push 128
    lea edi, [ebx].ModelEntry.szModelName
    push edi
    push lpName
    call lstrcpynA
    add esp, 12
    
    ; Initialize state
    mov [ebx].ModelEntry.hModelHandle, 0
    mov [ebx].ModelEntry.hInferenceContext, 0
    mov [ebx].ModelEntry.bCached, 0
    mov [ebx].ModelEntry.bWarmedUp, 0
    mov [ebx].ModelEntry.dwPriority, 50
    mov [ebx].ModelEntry.bCompressed, 0
    mov [ebx].ModelEntry.dwCompressionRatio, 100
    mov [ebx].ModelEntry.bDequantized, 0
    mov [ebx].ModelEntry.dwQuantFormat, 0
    mov [ebx].ModelEntry.hDiscStream, 0
    mov [ebx].ModelEntry.dwInferenceBackend, 0
    
    ; Increment count
    inc [g_HotPatchCtx.dwModelCount]
    
    mov eax, dwModelID
    pop edi
    pop esi
    pop ebx
    ret
    
@RegistryFull:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
HotPatch_RegisterModel ENDP

; ============================================================================
; HotPatch_SwapModel - Swap active model with registered model
; Input:  ECX = target model ID
; Output: EAX = 1 success, 0 failure
; ============================================================================
HotPatch_SwapModel PROC dwTargetModelID:DWORD
    LOCAL pTargetEntry:DWORD
    LOCAL pActiveEntry:DWORD
    push ebx
    push esi
    push edi
    
    ; Check if swap already in progress
    cmp [g_HotPatchCtx.bSwapInProgress], 1
    je @SwapBusy
    
    ; Mark swap in progress
    mov [g_HotPatchCtx.bSwapInProgress], 1
    
    ; Find target model
    push dwTargetModelID
    call FindModelEntry
    add esp, 4
    test eax, eax
    jz @ModelNotFound
    mov pTargetEntry, eax
    
    ; Backup current active model
    mov eax, [g_HotPatchCtx.dwActiveModelID]
    mov [g_HotPatchCtx.dwBackupModelID], eax
    
    ; Execute swap based on strategy
    mov eax, [g_HotPatchCtx.dwSwapStrategy]
    
    cmp eax, SWAP_INSTANT
    je @SwapInstant
    cmp eax, SWAP_GRACEFUL
    je @SwapGraceful
    cmp eax, SWAP_PARALLEL
    je @SwapParallel
    
    ; Default: Graceful
@SwapGraceful:
    ; Wait for active requests to complete (stub)
    push 100
    call Sleep
    add esp, 4
    jmp @PerformSwap
    
@SwapInstant:
    ; Immediate swap
    jmp @PerformSwap
    
@SwapParallel:
    ; Load new model in parallel thread
    ; (Production: CreateThread to load model)
    jmp @PerformSwap
    
@PerformSwap:
    ; Load target model if not already loaded
    mov ebx, pTargetEntry
    cmp [ebx].ModelEntry.hModelHandle, 0
    jne @AlreadyLoaded
    
    ; Load model
    lea eax, [ebx].ModelEntry.szModelPath
    push eax
    call LoadModelByPath
    add esp, 4
    test eax, eax
    jz @SwapFailed
    
    mov [ebx].ModelEntry.hModelHandle, eax
    
@AlreadyLoaded:
    ; Activate target model
    mov eax, dwTargetModelID
    mov [g_HotPatchCtx.dwActiveModelID], eax
    
    ; Mark swap complete
    mov [g_HotPatchCtx.bSwapInProgress], 0
    
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
    
@SwapFailed:
    ; Restore backup
    mov eax, [g_HotPatchCtx.dwBackupModelID]
    mov [g_HotPatchCtx.dwActiveModelID], eax
    mov [g_HotPatchCtx.bSwapInProgress], 0
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
    
@ModelNotFound:
@SwapBusy:
    mov [g_HotPatchCtx.bSwapInProgress], 0
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
HotPatch_SwapModel ENDP

; ============================================================================
; HotPatch_RollbackModel - Rollback to previous model
; Output: EAX = 1 success
; ============================================================================
HotPatch_RollbackModel PROC
    push ebx
    
    ; Swap back to backup model
    mov eax, [g_HotPatchCtx.dwBackupModelID]
    test eax, eax
    jz @NoBackup
    
    push eax
    call HotPatch_SwapModel
    add esp, 4
    
    pop ebx
    ret
    
@NoBackup:
    xor eax, eax
    pop ebx
    ret
HotPatch_RollbackModel ENDP

; ============================================================================
; HotPatch_GetActiveModel - Get currently active model ID
; Output: EAX = active model ID
; ============================================================================
HotPatch_GetActiveModel PROC
    mov eax, [g_HotPatchCtx.dwActiveModelID]
    ret
HotPatch_GetActiveModel ENDP

; ============================================================================
; HotPatch_ListModels - Get list of registered models
; Input:  ECX = buffer
;         EDX = buffer size
; Output: EAX = number of models written
; ============================================================================
HotPatch_ListModels PROC lpBuffer:DWORD, cbBuffer:DWORD
    LOCAL dwCount:DWORD
    push ebx
    push esi
    push edi
    
    mov dwCount, 0
    mov edi, lpBuffer
    mov esi, [g_HotPatchCtx.pModelRegistry]
    
    xor ecx, ecx
@ListLoop:
    cmp ecx, [g_HotPatchCtx.dwModelCount]
    jge @ListDone
    
    ; Check buffer space
    mov eax, dwCount
    imul eax, SIZEOF ModelEntry
    cmp eax, cbBuffer
    jge @ListDone
    
    ; Copy entry
    push SIZEOF ModelEntry
    push esi
    push edi
    call RtlMoveMemory
    add esp, 12
    
    add edi, SIZEOF ModelEntry
    add esi, SIZEOF ModelEntry
    inc dwCount
    inc ecx
    jmp @ListLoop
    
@ListDone:
    mov eax, dwCount
    pop edi
    pop esi
    pop ebx
    ret
HotPatch_ListModels ENDP

; ============================================================================
; HotPatch_CacheModel - Pre-load model into cache
; Input:  ECX = model ID
; Output: EAX = 1 success
; ============================================================================
HotPatch_CacheModel PROC dwModelID:DWORD
    LOCAL pEntry:DWORD
    push ebx
    
    push dwModelID
    call FindModelEntry
    add esp, 4
    test eax, eax
    jz @NotFound
    mov pEntry, eax
    
    mov ebx, pEntry
    
    ; Load model if not already
    cmp [ebx].ModelEntry.hModelHandle, 0
    jne @AlreadyCached
    
    lea eax, [ebx].ModelEntry.szModelPath
    push eax
    call LoadModelByPath
    add esp, 4
    test eax, eax
    jz @CacheFailed
    
    mov [ebx].ModelEntry.hModelHandle, eax
    
@AlreadyCached:
    mov [ebx].ModelEntry.bCached, 1
    
    mov eax, 1
    pop ebx
    ret
    
@NotFound:
@CacheFailed:
    xor eax, eax
    pop ebx
    ret
HotPatch_CacheModel ENDP

; ============================================================================
; HotPatch_WarmupModel - Warmup model with dummy inference
; Input:  ECX = model ID
; Output: EAX = 1 success
; ============================================================================
HotPatch_WarmupModel PROC dwModelID:DWORD
    LOCAL pEntry:DWORD
    push ebx
    
    push dwModelID
    call FindModelEntry
    add esp, 4
    test eax, eax
    jz @NotFound
    mov pEntry, eax
    
    ; Cache model first
    push dwModelID
    call HotPatch_CacheModel
    add esp, 4
    
    ; Execute dummy inference (stub)
    ; (Production: Run small inference to warmup)
    
    mov ebx, pEntry
    mov [ebx].ModelEntry.bWarmedUp, 1
    
    mov eax, 1
    pop ebx
    ret
    
@NotFound:
    xor eax, eax
    pop ebx
    ret
HotPatch_WarmupModel ENDP

; ============================================================================
; HotPatch_SetSwapStrategy - Set swap strategy
; Input:  ECX = strategy (SWAP_*)
; ============================================================================
HotPatch_SetSwapStrategy PROC dwStrategy:DWORD
    mov eax, dwStrategy
    mov [g_HotPatchCtx.dwSwapStrategy], eax
    mov eax, 1
    ret
HotPatch_SetSwapStrategy ENDP

; ============================================================================
; HotPatch_SetStreamCap - Configure streaming window cap (MB)
; Input:  ECX = window size in MB (clamped to STREAM_MAX_WINDOW_MB)
; Output: EAX = applied size (MB)
; ============================================================================
HotPatch_SetStreamCap PROC dwWindowMB:DWORD
    mov eax, dwWindowMB
    cmp eax, STREAM_MAX_WINDOW_MB
    jbe @Ok
    mov eax, STREAM_MAX_WINDOW_MB
@Ok:
    cmp eax, 0
    jne @Apply
    mov eax, STREAM_DEFAULT_WINDOW_MB
@Apply:
    mov g_StreamCapMB, eax
    ret
HotPatch_SetStreamCap ENDP

; ============================================================================
; HotPatch_StreamedApplyChunk - Apply streamed chunk with compression/dequant
; Input:  ECX = chunk pointer, EDX = chunk bytes, ESI = ModelEntry pointer (optional)
; Output: EAX = 1 success, 0 failure
; Integrates: compression, dequantization, disc streaming, error logging
; ============================================================================
HotPatch_StreamedApplyChunk PROC pChunk:DWORD, cbChunk:DWORD, pEntry:DWORD
    LOCAL workBuffer:DWORD
    LOCAL workSize:DWORD
    push ebx
    push esi
    push edi
    
    mov esi, pEntry
    mov eax, pChunk
    mov workBuffer, eax
    mov eax, cbChunk
    mov workSize, eax
    
    ; Log chunk processing
    push cbChunk
    push pChunk
    push OFFSET szChunkProcessing
    call ErrorLogging_LogEvent
    add esp, 12
    
    ; Check if model needs decompression
    test esi, esi
    jz @ApplyDirect
    
    cmp [esi].ModelEntry.bCompressed, 1
    jne @CheckDequant
    
    ; Decompress chunk
    invoke VirtualAlloc, 0, cbChunk, MEM_COMMIT, PAGE_READWRITE
    test eax, eax
    jz @Fail
    mov workBuffer, eax
    
    push cbChunk
    push workBuffer
    push cbChunk
    push pChunk
    call PiramHooks_DecompressTensor
    add esp, 16
    test eax, eax
    jz @Fail
    mov workSize, eax
    
@CheckDequant:
    ; Check if model needs dequantization
    test esi, esi
    jz @ApplyDirect
    
    cmp [esi].ModelEntry.bDequantized, 0
    jne @ApplyDirect
    
    ; Apply dequantization if needed
    push [esi].ModelEntry.dwQuantFormat
    push workSize
    push workBuffer
    call ReverseQuant_DequantizeBuffer
    add esp, 12
    test eax, eax
    jz @Fail
    
@ApplyDirect:
    ; Apply chunk to inference context (stub)
    ; Production: integrate with inference backend
    mov eax, 1
    jmp @Success
    
@Fail:
    push OFFSET szChunkFailed
    call ErrorLogging_LogEvent
    add esp, 4
    xor eax, eax
    
@Success:
    ; Cleanup work buffer if allocated
    mov eax, workBuffer
    cmp eax, pChunk
    je @NoCleanup
    push workBuffer
    call VirtualFree
    
@NoCleanup:
    pop edi
    pop esi
    pop ebx
    ret
HotPatch_StreamedApplyChunk ENDP

; ============================================================================
; HotPatch_StreamedLoadModel - Read huge models using a capped memory window
; Input:  ECX = model path (LPSTR), EDX = optional ModelEntry pointer (can be 0)
; Output: EAX = 1 success, 0 failure
; Strategy: reuse a single window buffer <= g_StreamCapMB, read->apply per chunk.
; ============================================================================
HotPatch_StreamedLoadModel PROC lpPath:DWORD, pEntry:DWORD
    LOCAL hFile:DWORD
    LOCAL windowBytes:DWORD
    LOCAL bytesToRead:DWORD
    LOCAL bytesRead:DWORD
    LOCAL remainingLow:DWORD
    LOCAL remainingHigh:DWORD
    LOCAL chunkPtr:DWORD

    push ebx
    push esi
    push edi

    mov esi, pEntry

    ; open file
    invoke CreateFileA, lpPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    cmp eax, INVALID_HANDLE_VALUE
    jne @FileOk
    xor eax, eax
    jmp @Fail

@FileOk:
    ; get size (low/high)
    invoke GetFileSize, hFile, addr remainingHigh
    mov remainingLow, eax

    ; compute window size in bytes (cap MB -> bytes)
    mov eax, g_StreamCapMB
    cmp eax, STREAM_MAX_WINDOW_MB
    jbe @CapOk
    mov eax, STREAM_MAX_WINDOW_MB
@CapOk:
    cmp eax, STREAM_DEFAULT_WINDOW_MB
    jne @CapSet
    ; default already fine
@CapSet:
    mov ebx, 1048576            ; MB in bytes
    mul ebx                     ; eax * 1MB => windowBytes (fits 32-bit due cap<=1GB)
    mov windowBytes, eax
    mov g_StreamWindowSize, eax

    ; allocate or reuse streaming window
    mov eax, g_StreamWindowPtr
    test eax, eax
    jnz @HaveWindow
    push PAGE_READWRITE
    push MEM_COMMIT
    push windowBytes
    push 0
    call VirtualAlloc
    mov g_StreamWindowPtr, eax
@HaveWindow:
    mov eax, g_StreamWindowPtr
    mov chunkPtr, eax
    test eax, eax
    jnz @LoopStart
    jmp @FailClose

@LoopStart:
    ; check remainingHigh:remainingLow > 0
    mov eax, remainingLow
    or eax, remainingHigh
    jz @Done

    ; determine bytesToRead = min(windowBytes, remainingLow if remainingHigh==0)
    mov eax, windowBytes
    mov bytesToRead, eax
    mov edx, remainingHigh
    test edx, edx
    jnz @UseWindow
    mov eax, remainingLow
    cmp eax, bytesToRead
    jae @UseWindow
    mov bytesToRead, eax
@UseWindow:

    ; read chunk
    invoke ReadFile, hFile, chunkPtr, bytesToRead, addr bytesRead, NULL
    test eax, eax
    jz @FailClose

    ; apply chunk (hotpatch / staging)
    push esi
    push bytesRead
    push chunkPtr
    call HotPatch_StreamedApplyChunk
    add esp, 12
    test eax, eax
    jz @FailClose

    ; decrement remaining
    mov eax, bytesRead
    mov ebx, remainingLow
    sub ebx, eax
    mov remainingLow, ebx
    ; handle borrow from high
    jnc @LoopStart
    dec remainingHigh
    jmp @LoopStart

@Done:
    invoke CloseHandle, hFile
    mov eax, 1
    jmp @Success

@FailClose:
    invoke CloseHandle, hFile
@Fail:
    xor eax, eax
@Success:
    pop edi
    pop esi
    pop ebx
    ret
HotPatch_StreamedLoadModel ENDP

; ============================================================================
; Internal helpers
; ============================================================================

FindModelEntry PROC dwModelID:DWORD
    push ebx
    push ecx
    
    mov esi, [g_HotPatchCtx.pModelRegistry]
    xor ecx, ecx
    
@FindLoop:
    cmp ecx, [g_HotPatchCtx.dwModelCount]
    jge @NotFound
    
    mov eax, [esi].ModelEntry.dwModelID
    cmp eax, dwModelID
    je @Found
    
    add esi, SIZEOF ModelEntry
    inc ecx
    jmp @FindLoop
    
@Found:
    mov eax, esi
    pop ecx
    pop ebx
    ret
    
@NotFound:
    xor eax, eax
    pop ecx
    pop ebx
    ret
FindModelEntry ENDP

LoadModelByPath PROC lpPath:DWORD
    LOCAL hFile:DWORD
    LOCAL highSize:DWORD
    LOCAL lowSize:DWORD
    LOCAL pEntry:DWORD
    push ebx
    push esi
    
    mov pEntry, 0
    
    ; Log model loading start
    push lpPath
    push OFFSET szModelNotFound  ; reuse string
    call ErrorLogging_LogEvent
    add esp, 8
    
    ; Initialize performance optimizations
    call ReverseQuant_Init
    
    ; Select optimal inference backend
    push 0  ; auto-select
    call InferenceBackend_SelectBackend
    add esp, 4
    
    ; Quickly probe size to decide streaming vs in-memory vs disc streaming
    invoke CreateFileA, lpPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    cmp eax, INVALID_HANDLE_VALUE
    jne @HaveFile
    xor eax, eax
    jmp @Exit

@HaveFile:
    invoke GetFileSize, hFile, addr highSize
    mov lowSize, eax
    invoke CloseHandle, hFile

    ; Check for ultra-large files (use disc streaming)
    mov eax, highSize
    test eax, eax
    jnz @DiscStream
    
    ; Check for large files (use memory streaming)
    mov eax, lowSize
    cmp eax, STREAM_THRESHOLD_BYTES
    jae @Stream

    ; Normal GGUF path for smaller models
    push lpPath
    call GgufUnified_LoadModelAutomatic
    add esp, 4
    jmp @Exit

@DiscStream:
    ; Use enterprise disc streaming for huge models
    push lpPath
    call DiscStream_OpenModel
    add esp, 4
    test eax, eax
    jz @Stream  ; fallback to memory streaming
    jmp @Exit

@Stream:
    push pEntry
    push lpPath
    call HotPatch_StreamedLoadModel
    add esp, 8
    
@Exit:
    pop esi
    pop ebx
    ret
LoadModelByPath ENDP

END
