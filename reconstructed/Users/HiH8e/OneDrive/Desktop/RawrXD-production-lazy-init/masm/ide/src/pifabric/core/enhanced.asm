; ============================================================================
; pifabric_core_enhanced.asm
; PiFabric Core with Tensor Offset Resolution + Training Support
; Integrates GGUF loader, tensor resolution, settings, and ML training
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

; ============================================================================
; EXTERNAL DEPENDENCIES
; ============================================================================

EXTERN  GGUFChain_LoadModel:PROC
EXTERN  GGUFChain_CloseModel:PROC
EXTERN  GGUFChain_StreamChunk:PROC

EXTERN  GGUF_ResolveTensorPointers:PROC
EXTERN  GGUF_ValidateTensorBounds:PROC
EXTERN  GGUF_TensorOffsetLoopAll:PROC

EXTERN  GGUFSettings_Init:PROC
EXTERN  GGUFSettings_Create:PROC
EXTERN  GGUFSettings_SetLoaderMethod:PROC
EXTERN  GGUFSettings_GetLoaderMethod:PROC
EXTERN  GGUFSettings_SetToggle:PROC
EXTERN  GGUFSettings_GetToggle:PROC

EXTERN  MLTraining_Init:PROC
EXTERN  MLTraining_CreateSession:PROC
EXTERN  MLTraining_SetLearningRate:PROC
EXTERN  MLTraining_ComputeGradients:PROC
EXTERN  MLTraining_UpdateWeights:PROC
EXTERN  MLTraining_GetLoss:PROC
EXTERN  MLTraining_GetMetrics:PROC
EXTERN  MLTraining_CloseSession:PROC

; ============================================================================
; EXPORTS
; ============================================================================

PUBLIC  PiFabric_Init
PUBLIC  PiFabric_Open
PUBLIC  PiFabric_Stream
PUBLIC  PiFabric_Close
PUBLIC  PiFabric_EnableTraining
PUBLIC  PiFabric_DisableTraining
PUBLIC  PiFabric_Train
PUBLIC  PiFabric_ConfigureSettings
PUBLIC  PiFabric_GetStatus
PUBLIC  PiFabric_ResolveTensorOffsets

; ============================================================================
; CONSTANTS
; ============================================================================

NULL                equ 0
TRUE                equ 1
FALSE               equ 0

; PiFabric flags
PIFABRIC_FLAG_TRAINING_ENABLED  equ 1
PIFABRIC_FLAG_OFFSETS_RESOLVED  equ 2
PIFABRIC_FLAG_SETTINGS_APPLIED  equ 4

; ============================================================================
; STRUCTURES
; ============================================================================

PiFabricHandle STRUCT
    hChain          DWORD ?     ; Chain handle
    pChain          DWORD ?     ; Chain pointer
    dwMethod        DWORD ?     ; Loading method
    dwChainMode     DWORD ?     ; Chain mode
    dwTier          DWORD ?     ; Performance tier
    dwFlags         DWORD ?     ; Status flags
    
    pSettings       DWORD ?     ; Loader settings
    pTrainingSession DWORD ?    ; ML training session
    dwTrainingFlags DWORD ?     ; Training-specific flags
    
    pContext        DWORD ?     ; GGUF model context
    nTensorsResolved DWORD ?    ; Number of resolved tensors
    dwOffsetStatus  DWORD ?     ; Offset resolution status
PiFabricHandle ENDS

; GGUF_MODEL_CONTEXT structure
GGUF_MODEL_CONTEXT STRUCT
    pBase           DWORD ?
    cbFileSize      QWORD ?
    pHeader         DWORD ?
    pTensorMeta     DWORD ?
    nTensorsCount   DWORD ?
    pTensors        DWORD ?
    cbDataOffset    DWORD ?
    dwStatus        DWORD ?
    dwValidated     DWORD ?
GGUF_MODEL_CONTEXT ENDS

; ============================================================================
; DATA SECTION
; ============================================================================

.data

g_dwPiFabricInitFlag dd 0               ; Global init flag
g_dwHandleCount      dd 0               ; Number of open handles

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; ============================================================================
; PiFabric_Init - Initialize PiFabric system
; out:
;   EAX             = 1 success, 0 failure
; ============================================================================

PiFabric_Init PROC

    cmp g_dwPiFabricInitFlag, 1
    je  @already_init

    ; Initialize settings system
    call GGUFSettings_Init
    test eax, eax
    jz  @fail

    ; Initialize training system
    call MLTraining_Init
    test eax, eax
    jz  @fail

    mov g_dwPiFabricInitFlag, 1
    mov eax, 1
    ret

@already_init:
    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

PiFabric_Init ENDP

; ============================================================================
; PiFabric_Open - Open GGUF model with full initialization
; in:
;   lpPath          = path to GGUF file
;   dwMethodMask    = method bitmask
;   dwChainMode     = chain mode
; out:
;   EAX             = PiFabricHandle*, or 0 on failure
; ============================================================================

PiFabric_Open PROC USES esi edi ebx lpPath:DWORD, dwMethodMask:DWORD, dwChainMode:DWORD

    call PiFabric_Init
    test eax, eax
    jz  @fail

    ; Load model using chain API
    push dwChainMode
    push dwMethodMask
    push lpPath
    call GGUFChain_LoadModel
    test eax, eax
    jz  @fail
    mov esi, eax                ; chain handle

    ; Allocate PiFabricHandle
    push SIZEOF PiFabricHandle
    push GMEM_ZEROINIT
    call GlobalAlloc
    test eax, eax
    jz  @close_chain
    mov edi, eax                ; PiFabricHandle

    ; Initialize handle
    mov [edi].PiFabricHandle.hChain, esi
    mov [edi].PiFabricHandle.pChain, esi
    mov [edi].PiFabricHandle.dwMethod, dwMethodMask
    mov [edi].PiFabricHandle.dwChainMode, dwChainMode
    mov [edi].PiFabricHandle.dwTier, 0
    mov [edi].PiFabricHandle.dwFlags, 0

    ; Create settings for this handle
    call GGUFSettings_Create
    mov [edi].PiFabricHandle.pSettings, eax

    ; Increment counter
    mov eax, g_dwHandleCount
    inc eax
    mov g_dwHandleCount, eax

    mov eax, edi
    ret

@close_chain:
    push esi
    call GGUFChain_CloseModel

@fail:
    xor eax, eax
    ret

PiFabric_Open ENDP

; ============================================================================
; PiFabric_ResolveTensorOffsets - Resolve tensor offsets and pointers
; in:
;   hFabric         = PiFabricHandle*
;   pContext        = GGUF_MODEL_CONTEXT*
; out:
;   EAX             = 1 success, 0 failure
; ============================================================================

PiFabric_ResolveTensorOffsets PROC USES esi edi hFabric:DWORD, pContext:DWORD

    mov esi, hFabric
    test esi, esi
    jz  @fail

    mov edi, pContext
    test edi, edi
    jz  @fail

    ; Call tensor offset resolution loop
    push edi
    call GGUF_TensorOffsetLoopAll
    test eax, eax
    jz  @fail

    ; Update handle
    mov [esi].PiFabricHandle.pContext, edi
    or  [esi].PiFabricHandle.dwOffsetStatus, 1

    ; Store tensor count
    mov eax, [edi].GGUF_MODEL_CONTEXT.nTensorsCount
    mov [esi].PiFabricHandle.nTensorsResolved, eax

    or  [esi].PiFabricHandle.dwFlags, PIFABRIC_FLAG_OFFSETS_RESOLVED

    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

PiFabric_ResolveTensorOffsets ENDP

; ============================================================================
; PiFabric_Stream - Stream chunk from model
; in:
;   hFabric         = PiFabricHandle*
;   pDst            = destination buffer
;   dwBytes         = bytes to read
; out:
;   EAX             = bytes read, 0 on failure
; ============================================================================

PiFabric_Stream PROC USES esi edi ebx hFabric:DWORD, pDst:DWORD, dwBytes:DWORD

    mov esi, hFabric
    test esi, esi
    jz  @fail

    mov eax, [esi].PiFabricHandle.hChain
    test eax, eax
    jz  @fail

    ; Call chain stream
    push dwBytes
    push pDst
    push eax
    call GGUFChain_StreamChunk
    ret

@fail:
    xor eax, eax
    ret

PiFabric_Stream ENDP

; ============================================================================
; PiFabric_EnableTraining - Enable training mode for model
; in:
;   hFabric         = PiFabricHandle*
;   nLayers         = number of layers in model
;   dwOptimizer     = optimizer type
;   dwLossFunction  = loss function type
; out:
;   EAX             = 1 success, 0 failure
; ============================================================================

PiFabric_EnableTraining PROC USES esi hFabric:DWORD, nLayers:DWORD, dwOptimizer:DWORD, dwLossFunction:DWORD

    mov esi, hFabric
    test esi, esi
    jz  @fail

    ; Create training session
    push dwLossFunction
    push dwOptimizer
    push nLayers
    push [esi].PiFabricHandle.pChain
    call MLTraining_CreateSession
    test eax, eax
    jz  @fail

    mov [esi].PiFabricHandle.pTrainingSession, eax
    or  [esi].PiFabricHandle.dwFlags, PIFABRIC_FLAG_TRAINING_ENABLED

    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

PiFabric_EnableTraining ENDP

; ============================================================================
; PiFabric_DisableTraining - Disable training mode
; in:
;   hFabric         = PiFabricHandle*
; ============================================================================

PiFabric_DisableTraining PROC USES esi hFabric:DWORD

    mov esi, hFabric
    test esi, esi
    jz  @done

    mov eax, [esi].PiFabricHandle.pTrainingSession
    test eax, eax
    jz  @clear_flags

    push eax
    call MLTraining_CloseSession

@clear_flags:
    and [esi].PiFabricHandle.dwFlags, NOT PIFABRIC_FLAG_TRAINING_ENABLED
    mov dword ptr [esi].PiFabricHandle.pTrainingSession, 0

@done:
    ret

PiFabric_DisableTraining ENDP

; ============================================================================
; PiFabric_Train - Execute one training iteration
; in:
;   hFabric         = PiFabricHandle*
;   pBatchInput     = input batch data
;   pBatchTarget    = target/label data
;   dwBatchSize     = batch size
; out:
;   EAX             = 1 success, 0 failure
; ============================================================================

PiFabric_Train PROC USES esi hFabric:DWORD, pBatchInput:DWORD, pBatchTarget:DWORD, dwBatchSize:DWORD

    mov esi, hFabric
    test esi, esi
    jz  @fail

    ; Check if training enabled
    mov eax, [esi].PiFabricHandle.dwFlags
    and eax, PIFABRIC_FLAG_TRAINING_ENABLED
    test eax, eax
    jz  @fail

    mov eax, [esi].PiFabricHandle.pTrainingSession
    test eax, eax
    jz  @fail

    ; Compute gradients
    push dwBatchSize
    push pBatchTarget
    push pBatchInput
    push eax
    call MLTraining_ComputeGradients
    test eax, eax
    jz  @fail

    ; Update weights
    mov eax, [esi].PiFabricHandle.pTrainingSession
    push eax
    call MLTraining_UpdateWeights
    test eax, eax
    jz  @fail

    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

PiFabric_Train ENDP

; ============================================================================
; PiFabric_ConfigureSettings - Configure loader settings
; in:
;   hFabric         = PiFabricHandle*
;   dwMethod        = loading method
;   dwToggle        = toggle flag
;   bEnable         = 1 enable, 0 disable
; out:
;   EAX             = 1 success, 0 failure
; ============================================================================

PiFabric_ConfigureSettings PROC USES esi hFabric:DWORD, dwMethod:DWORD, dwToggle:DWORD, bEnable:DWORD

    mov esi, hFabric
    test esi, esi
    jz  @fail

    mov eax, [esi].PiFabricHandle.pSettings
    test eax, eax
    jz  @fail

    ; Set loader method
    cmp dwMethod, 0FFFFFFFFh
    je  @skip_method

    push dwMethod
    push eax
    call GGUFSettings_SetLoaderMethod

@skip_method:
    ; Set toggle
    cmp dwToggle, 0FFFFFFFFh
    je  @done

    mov eax, [esi].PiFabricHandle.pSettings
    push bEnable
    push dwToggle
    push eax
    call GGUFSettings_SetToggle

@done:
    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

PiFabric_ConfigureSettings ENDP

; ============================================================================
; PiFabric_GetStatus - Get current status
; in:
;   hFabric         = PiFabricHandle*
; out:
;   EAX             = status flags (PIFABRIC_FLAG_*)
; ============================================================================

PiFabric_GetStatus PROC USES esi hFabric:DWORD

    mov esi, hFabric
    test esi, esi
    jz  @error

    mov eax, [esi].PiFabricHandle.dwFlags
    ret

@error:
    xor eax, eax
    ret

PiFabric_GetStatus ENDP

; ============================================================================
; PiFabric_Close - Close PiFabric handle
; in:
;   hFabric         = PiFabricHandle*
; ============================================================================

PiFabric_Close PROC USES esi hFabric:DWORD

    mov esi, hFabric
    test esi, esi
    jz  @done

    ; Close training session if active
    mov eax, [esi].PiFabricHandle.pTrainingSession
    test eax, eax
    jz  @close_chain

    push eax
    call MLTraining_CloseSession

@close_chain:
    mov eax, [esi].PiFabricHandle.hChain
    test eax, eax
    jz  @free_handle

    push eax
    call GGUFChain_CloseModel

@free_handle:
    push esi
    call GlobalFree

    mov eax, g_dwHandleCount
    dec eax
    mov g_dwHandleCount, eax

@done:
    xor eax, eax
    ret

PiFabric_Close ENDP

END
