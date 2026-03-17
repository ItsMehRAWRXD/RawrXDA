; ============================================================================
; gguf_loader_tensor_bridge.asm
; Integration bridge between GGUF loader and tensor offset resolver
; Connects parsed GGUF data to tensor offset resolution pipeline
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include winapi_min.inc

; ============================================================================
; EXTERNAL DEPENDENCIES
; ============================================================================

; From gguf_tensor_offset_resolver.asm
EXTERN GGUF_ResolveTensorPointers:PROC
EXTERN GGUF_TensorOffsetLoopAll:PROC
EXTERN GGUF_ValidateTensorBounds:PROC

; From gguf_loader.asm
EXTERN GGUFLoader_GetCurrentModel:PROC

; ============================================================================
; EXPORTS
; ============================================================================

PUBLIC GGUF_Bridge_ResolveAllTensors
PUBLIC GGUF_Bridge_ValidateAllTensors
PUBLIC GGUF_Bridge_PopulateModelContext
PUBLIC GGUF_Bridge_IntegrateResolver

; ============================================================================
; STRUCTURES - Replicate for bridge compatibility
; ============================================================================

GGUF_TENSOR_INFO STRUCT
    pName           DWORD ?
    nDims           DWORD ?
    dims            DWORD 4 DUP (?)
    type            DWORD ?
    offsetInFile    QWORD ?
    pResolved       DWORD ?
    cbTensorBytes   QWORD ?
    dwFlags         DWORD ?
    reserved        DWORD ?
GGUF_TENSOR_INFO ENDS

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

GGUF_MODEL_INFO STRUCT
    header          DWORD 4 DUP (?)
    kv_pairs        DWORD ?
    tensors         DWORD ?
    file_handle     DWORD ?
    file_size       DWORD ?
    is_valid        DWORD ?
    szModelPath     BYTE 256 DUP (?)
GGUF_MODEL_INFO ENDS

; ============================================================================
; DATA SECTION
; ============================================================================

.data

    ; Status messages
    szResolving     db "Resolving tensor offsets...", 0
    szResolved      db "Tensor offsets resolved successfully", 0
    szResolveFail   db "Failed to resolve tensor offsets", 0
    szValidating    db "Validating tensor bounds...", 0
    szValidated     db "All tensors validated", 0
    szValidateFail  db "Tensor validation failed", 0

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; ============================================================================
; GGUF_Bridge_PopulateModelContext
; Converts GGUF_MODEL_INFO into GGUF_MODEL_CONTEXT for resolver
; in:
;   pModelInfo      = pointer to GGUF_MODEL_INFO
;   pBase           = base address of mapped file
;   cbDataOffset    = offset to tensor data section
; out:
;   eax             = pointer to GGUF_MODEL_CONTEXT, or 0 on error
; ============================================================================

GGUF_Bridge_PopulateModelContext PROC USES esi edi ebx pModelInfo:DWORD, pBase:DWORD, cbDataOffset:DWORD, cbFileSizeLo:DWORD, cbFileSizeHi:DWORD

    mov esi, pModelInfo
    test esi, esi
    jz  @fail

    ; Allocate context structure
    invoke GlobalAlloc, GMEM_ZEROINIT, SIZEOF GGUF_MODEL_CONTEXT
    test eax, eax
    jz  @fail
    mov edi, eax

    ; Populate context fields
    mov ebx, pBase
    mov [edi].GGUF_MODEL_CONTEXT.pBase, ebx

    mov ebx, cbFileSizeLo
    mov [edi].GGUF_MODEL_CONTEXT.cbFileSize, ebx
    mov ebx, cbFileSizeHi
    mov [edi].GGUF_MODEL_CONTEXT.cbFileSize + 4, ebx

    mov ebx, pBase
    mov [edi].GGUF_MODEL_CONTEXT.pHeader, ebx

    ; Calculate tensor meta offset (after header and KV pairs)
    mov ebx, pBase
    add ebx, 24                 ; Assume header is 24 bytes (simplified)
    mov [edi].GGUF_MODEL_CONTEXT.pTensorMeta, ebx

    mov ebx, [esi].GGUF_MODEL_INFO.tensors
    mov [edi].GGUF_MODEL_CONTEXT.pTensors, ebx

    ; Get tensor count from header
    mov ebx, [esi].GGUF_MODEL_INFO.header
    mov [edi].GGUF_MODEL_CONTEXT.nTensorsCount, ebx

    mov ebx, cbDataOffset
    mov [edi].GGUF_MODEL_CONTEXT.cbDataOffset, ebx

    mov [edi].GGUF_MODEL_CONTEXT.dwStatus, 0
    mov [edi].GGUF_MODEL_CONTEXT.dwValidated, 0

    mov eax, edi
    ret

@fail:
    xor eax, eax
    ret

GGUF_Bridge_PopulateModelContext ENDP

; ============================================================================
; GGUF_Bridge_ResolveAllTensors
; Main resolution entry point from loader
; in:
;   pContext        = pointer to GGUF_MODEL_CONTEXT
; out:
;   eax             = 1 success, 0 failure
; ============================================================================

GGUF_Bridge_ResolveAllTensors PROC USES esi edi ebx pContext:DWORD

    mov esi, pContext
    test esi, esi
    jz  @fail

    ; Call the main tensor offset resolution loop
    push esi
    call GGUF_TensorOffsetLoopAll

    ; Update status in context if successful
    test eax, eax
    jz  @fail

    mov esi, pContext
    mov [esi].GGUF_MODEL_CONTEXT.dwStatus, 1

    mov eax, 1
    ret

@fail:
    mov esi, pContext
    mov [esi].GGUF_MODEL_CONTEXT.dwStatus, 0
    xor eax, eax
    ret

GGUF_Bridge_ResolveAllTensors ENDP

; ============================================================================
; GGUF_Bridge_ValidateAllTensors
; Validate all tensor pointers and bounds
; in:
;   pContext        = pointer to GGUF_MODEL_CONTEXT
; out:
;   eax             = 1 all valid, 0 any invalid
; ============================================================================

GGUF_Bridge_ValidateAllTensors PROC USES esi edi ebx pContext:DWORD

    mov esi, pContext
    test esi, esi
    jz  @fail

    ; Call validation loop
    push esi
    call GGUF_ValidateTensorBounds

    ; Update validation flag in context
    test eax, eax
    jz  @fail

    mov esi, pContext
    mov [esi].GGUF_MODEL_CONTEXT.dwValidated, 1

    mov eax, 1
    ret

@fail:
    mov esi, pContext
    mov [esi].GGUF_MODEL_CONTEXT.dwValidated, 0
    xor eax, eax
    ret

GGUF_Bridge_ValidateAllTensors ENDP

; ============================================================================
; GGUF_Bridge_IntegrateResolver
; Complete integration: context creation + resolution + validation
; Called AFTER loader has parsed header, KV pairs, and tensor metadata
; in:
;   pModelInfo      = pointer to GGUF_MODEL_INFO (populated by loader)
;   pBase           = base address of file in memory
;   cbDataOffset    = offset to tensor data section
;   cbFileSizeLo    = file size (low 32 bits)
;   cbFileSizeHi    = file size (high 32 bits)
; out:
;   eax             = pointer to GGUF_MODEL_CONTEXT if success, 0 on error
; Side effect: model is ready for tensor access if eax != 0
; ============================================================================

GGUF_Bridge_IntegrateResolver PROC USES esi edi ebx pModelInfo:DWORD, pBase:DWORD, cbDataOffset:DWORD, cbFileSizeLo:DWORD, cbFileSizeHi:DWORD

    mov esi, pModelInfo
    test esi, esi
    jz  @fail

    ; Step 1: Create context structure
    push cbFileSizeHi
    push cbFileSizeLo
    push cbDataOffset
    push pBase
    push esi
    call GGUF_Bridge_PopulateModelContext
    test eax, eax
    jz  @fail
    mov edi, eax

    ; Step 2: Resolve all tensor offsets
    push edi
    call GGUF_Bridge_ResolveAllTensors
    test eax, eax
    jz  @cleanup_fail

    ; Step 3: Validate all tensor bounds
    push edi
    call GGUF_Bridge_ValidateAllTensors
    test eax, eax
    jz  @cleanup_fail

    ; Success: return context
    mov eax, edi
    ret

@cleanup_fail:
    ; Free context on failure
    push edi
    call GlobalFree

@fail:
    xor eax, eax
    ret

GGUF_Bridge_IntegrateResolver ENDP

; ============================================================================
; Helper: Get tensor by index from context
; in:
;   pContext        = pointer to GGUF_MODEL_CONTEXT
;   dwIndex         = tensor index
; out:
;   eax             = pointer to GGUF_TENSOR_INFO, 0 if invalid
; ============================================================================

GGUF_Bridge_GetTensorByIndex PROC USES esi pContext:DWORD, dwIndex:DWORD

    mov esi, pContext
    test esi, esi
    jz  @fail

    mov eax, dwIndex
    cmp eax, [esi].GGUF_MODEL_CONTEXT.nTensorsCount
    jae @fail

    ; Calculate pointer: pTensors + (index * SIZEOF GGUF_TENSOR_INFO)
    mov edx, SIZEOF GGUF_TENSOR_INFO
    mul edx
    add eax, [esi].GGUF_MODEL_CONTEXT.pTensors

    ret

@fail:
    xor eax, eax
    ret

GGUF_Bridge_GetTensorByIndex ENDP

; ============================================================================
; Helper: Get tensor data pointer from context/index
; in:
;   pContext        = pointer to GGUF_MODEL_CONTEXT
;   dwIndex         = tensor index
; out:
;   eax             = resolved data pointer, 0 if invalid or unresolved
; ============================================================================

GGUF_Bridge_GetTensorDataPtr PROC USES esi pContext:DWORD, dwIndex:DWORD

    mov esi, pContext
    test esi, esi
    jz  @fail

    push dwIndex
    push esi
    call GGUF_Bridge_GetTensorByIndex
    test eax, eax
    jz  @fail

    mov esi, eax
    mov eax, [esi].GGUF_TENSOR_INFO.pResolved
    ret

@fail:
    xor eax, eax
    ret

GGUF_Bridge_GetTensorDataPtr ENDP

; ============================================================================
; Helper: Get tensor size from context/index
; in:
;   pContext        = pointer to GGUF_MODEL_CONTEXT
;   dwIndex         = tensor index
; out:
;   EDX:EAX         = 64-bit tensor size in bytes
; ============================================================================

GGUF_Bridge_GetTensorSize PROC USES esi pContext:DWORD, dwIndex:DWORD

    mov esi, pContext
    test esi, esi
    jz  @fail

    push dwIndex
    push esi
    call GGUF_Bridge_GetTensorByIndex
    test eax, eax
    jz  @fail

    mov esi, eax
    mov eax, [esi].GGUF_TENSOR_INFO.cbTensorBytes
    mov edx, [esi].GGUF_TENSOR_INFO.cbTensorBytes + 4

    ret

@fail:
    xor eax, eax
    xor edx, edx
    ret

GGUF_Bridge_GetTensorSize ENDP

END
