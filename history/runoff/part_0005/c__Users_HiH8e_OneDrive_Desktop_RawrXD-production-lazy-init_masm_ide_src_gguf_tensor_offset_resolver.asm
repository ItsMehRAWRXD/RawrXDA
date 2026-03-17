; ============================================================================
; gguf_tensor_offset_resolver.asm
; Tensor Offset Resolution Loop - Core Infrastructure Component
; Resolves all tensor offsets within GGUF file, validates bounds, computes sizes
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

; ============================================================================
; IMPORTS
; ============================================================================

GetProcessHeap PROTO
HeapAlloc PROTO :DWORD,:DWORD,:DWORD
HeapFree PROTO :DWORD,:DWORD,:DWORD

includelib kernel32.lib

; ============================================================================
; CONSTANTS & EXPORTS
; ============================================================================

PUBLIC GGUF_TensorOffsetResolve
PUBLIC GGUF_TensorOffsetValidate
PUBLIC GGUF_TensorSizeCompute
PUBLIC GGUF_TensorOffsetLoopAll
PUBLIC GGUF_ResolveTensorPointers
PUBLIC GGUF_ValidateTensorBounds

; GGML Type Constants
GGML_TYPE_F32       equ 0
GGML_TYPE_F16       equ 1
GGML_TYPE_Q4_0      equ 2
GGML_TYPE_Q4_1      equ 3
GGML_TYPE_Q5_0      equ 4
GGML_TYPE_Q5_1      equ 5
GGML_TYPE_Q8_0      equ 6
GGML_TYPE_Q8_1      equ 7
GGML_TYPE_Q2_K      equ 8
GGML_TYPE_Q3_K      equ 9
GGML_TYPE_Q4_K      equ 10
GGML_TYPE_Q5_K      equ 11
GGML_TYPE_Q6_K      equ 12
GGML_TYPE_I8        equ 13
GGML_TYPE_I16       equ 14
GGML_TYPE_I32       equ 15

; Size constants per type (bytes)
; Adjusted for typical quantization block sizes
GGML_SIZE_F32       equ 4
GGML_SIZE_F16       equ 2
GGML_SIZE_Q4_0      equ 1           ; 4 bits per value
GGML_SIZE_Q4_1      equ 1
GGML_SIZE_Q5_0      equ 1
GGML_SIZE_Q5_1      equ 1
GGML_SIZE_Q8_0      equ 1
GGML_SIZE_Q8_1      equ 1
GGML_SIZE_I8        equ 1
GGML_SIZE_I16       equ 2
GGML_SIZE_I32       equ 4

; ============================================================================
; STRUCTURES
; ============================================================================

GGUF_TENSOR_INFO STRUCT
    pName           DWORD ?     ; Pointer to name string
    nDims           DWORD ?     ; Number of dimensions
    dims            DWORD 4 DUP (?)  ; Dimension sizes (up to 4D)
    type            DWORD ?     ; GGML type
    offsetInFile    QWORD ?     ; Offset in file (64-bit for large files)
    pResolved       DWORD ?     ; Resolved memory pointer
    cbTensorBytes   QWORD ?     ; Computed tensor size in bytes
    dwFlags         DWORD ?     ; Flags (valid, resolved, etc.)
    reserved        DWORD ?     ; Reserved for future use
GGUF_TENSOR_INFO ENDS

GGUF_HEADER STRUCT
    magic           DWORD ?     ; 'GGUF' = 0x46554747
    version         DWORD ?     ; GGUF version
    nTensors        QWORD ?     ; Number of tensors
    nKVPairs        QWORD ?     ; Number of key-value pairs
GGUF_HEADER ENDS

GGUF_MODEL_CONTEXT STRUCT
    pBase           DWORD ?     ; Base pointer to file in memory
    cbFileSize      QWORD ?     ; Total file size
    pHeader         DWORD ?     ; Pointer to header
    pTensorMeta     DWORD ?     ; Pointer to tensor metadata array
    nTensorsCount   DWORD ?     ; Count of tensors
    pTensors        DWORD ?     ; Array of GGUF_TENSOR_INFO
    cbDataOffset    DWORD ?     ; Offset to start of tensor data section
    dwStatus        DWORD ?     ; Status flags
    dwValidated     DWORD ?     ; Validation flags
GGUF_MODEL_CONTEXT ENDS

; ============================================================================
; DATA SECTION
; ============================================================================

.data

; Lookup table for type sizes (in bytes, accounting for quantization)
g_ggmlTypeSizes DWORD \
    4,                          ; GGML_TYPE_F32 = 4 bytes
    2,                          ; GGML_TYPE_F16 = 2 bytes
    1,                          ; GGML_TYPE_Q4_0 = 1 byte (4-bit quant)
    1,                          ; GGML_TYPE_Q4_1 = 1 byte
    1,                          ; GGML_TYPE_Q5_0 = 1 byte
    1,                          ; GGML_TYPE_Q5_1 = 1 byte
    1,                          ; GGML_TYPE_Q8_0 = 1 byte
    1,                          ; GGML_TYPE_Q8_1 = 1 byte
    1,                          ; GGML_TYPE_Q2_K = 1 byte
    1,                          ; GGML_TYPE_Q3_K = 1 byte
    1,                          ; GGML_TYPE_Q4_K = 1 byte
    1,                          ; GGML_TYPE_Q5_K = 1 byte
    1,                          ; GGML_TYPE_Q6_K = 1 byte
    1,                          ; GGML_TYPE_I8 = 1 byte
    2,                          ; GGML_TYPE_I16 = 2 bytes
    4                           ; GGML_TYPE_I32 = 4 bytes

g_nTypeSizesCount equ ($ - g_ggmlTypeSizes) / 4

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; ============================================================================
; GGUF_TensorSizeCompute - Compute tensor size in bytes
; in:
;   pTensorInfo     = pointer to GGUF_TENSOR_INFO
; out:
;   EDX:EAX         = 64-bit size in bytes
; ============================================================================

GGUF_TensorSizeCompute PROC uses esi edi ebx pTensorInfo:DWORD

    mov esi, pTensorInfo
    test esi, esi
    jz  @error

    ; Get type and validate
    mov eax, [esi].GGUF_TENSOR_INFO.type
    cmp eax, g_nTypeSizesCount
    jae @error

    ; Get element size from lookup table
    mov ebx, [g_ggmlTypeSizes + eax*4]

    ; Start with element size as EDX:EAX = 1 * typeSize
    mov eax, ebx
    xor edx, edx

    ; Multiply by each dimension
    mov ecx, [esi].GGUF_TENSOR_INFO.nDims
    test ecx, ecx
    jz  @done

    xor edi, edi                ; dimension index

@dim_loop:
    cmp edi, ecx
    jge @done

    mov ebx, [esi + SIZEOF GGUF_TENSOR_INFO + edi*4]
    test ebx, ebx
    jz  @error_zero_dim

    ; EDX:EAX *= EBX (64-bit multiply)
    mov ecx, ebx
    call @mul64
    inc edi
    jmp @dim_loop

@done:
    ret

@error_zero_dim:
    xor eax, eax
    xor edx, edx
    ret

@error:
    mov eax, 0FFFFFFFFh
    mov edx, 0FFFFFFFFh
    ret

@mul64:
    ; EDX:EAX *= ECX (64-bit result)
    ; Result in EDX:EAX
    push ebx
    mov ebx, edx
    mul ecx
    mov edx, eax
    mov eax, ebx
    mul ecx
    add edx, eax
    mov eax, edx
    pop ebx
    ret

GGUF_TensorSizeCompute ENDP

; ============================================================================
; GGUF_TensorOffsetResolve - Resolve single tensor offset to memory pointer
; in:
;   pTensorInfo     = pointer to GGUF_TENSOR_INFO
;   pBaseMemory     = base address of file in memory
;   cbDataOffset    = offset to start of tensor data section
; out:
;   EAX             = resolved pointer, or 0 if invalid
; ============================================================================

GGUF_TensorOffsetResolve PROC uses esi pTensorInfo:DWORD, pBaseMemory:DWORD, cbDataOffset:DWORD

    mov esi, pTensorInfo
    test esi, esi
    jz  @error

    mov eax, pBaseMemory
    test eax, eax
    jz  @error

    ; pResolved = pBaseMemory + cbDataOffset + offsetInFile
    mov ecx, [esi].GGUF_TENSOR_INFO.offsetInFile   ; Low 32 bits
    mov edx, [esi].GGUF_TENSOR_INFO.offsetInFile + 4 ; High 32 bits (for 64-bit offsets)

    add ecx, cbDataOffset
    adc edx, 0

    add eax, ecx
    test edx, edx
    jz  @ok_resolved

    ; High part non-zero: would overflow, error
    xor eax, eax
    ret

@ok_resolved:
    mov [esi].GGUF_TENSOR_INFO.pResolved, eax
    ret

@error:
    xor eax, eax
    ret

GGUF_TensorOffsetResolve ENDP

; ============================================================================
; GGUF_TensorOffsetValidate - Validate tensor bounds against file size
; in:
;   pTensorInfo     = pointer to GGUF_TENSOR_INFO
;   cbFileSize      = total file size (high 32 bits)
;   cbFileSizeHi    = high 32 bits
;   pBaseMemory     = base pointer for relative checks
; out:
;   EAX             = 1 if valid, 0 if out of bounds
; ============================================================================

GGUF_TensorOffsetValidate PROC uses esi edi pTensorInfo:DWORD, cbFileSizeLo:DWORD, cbFileSizeHi:DWORD, pBaseMemory:DWORD

    mov esi, pTensorInfo
    test esi, esi
    jz  @fail

    ; Check: offsetInFile + tensorSize <= fileSize
    mov eax, [esi].GGUF_TENSOR_INFO.offsetInFile   ; low
    mov edx, [esi].GGUF_TENSOR_INFO.offsetInFile + 4 ; high

    ; Get tensor size
    push esi
    call GGUF_TensorSizeCompute
    mov ecx, eax                ; low size
    mov edi, edx                ; high size
    pop esi

    ; Add: (offset + size)
    add ecx, [esi].GGUF_TENSOR_INFO.offsetInFile   ; ecx += offset.low
    adc edi, [esi].GGUF_TENSOR_INFO.offsetInFile + 4 ; edi += offset.high

    ; Compare with file size
    cmp edi, cbFileSizeHi
    ja  @fail
    jb  @ok_bounds

    ; High parts equal, check low
    cmp ecx, cbFileSizeLo
    ja  @fail

@ok_bounds:
    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

GGUF_TensorOffsetValidate ENDP

; ============================================================================
; GGUF_ResolveTensorPointers - Resolve all tensor pointers in array
; MAIN TENSOR OFFSET RESOLUTION LOOP
; in:
;   pTensorArray    = pointer to array of GGUF_TENSOR_INFO
;   nTensorCount    = number of tensors
;   pBaseMemory     = base address of mapped file
;   cbDataOffset    = offset to tensor data section
;   cbFileSizeLo    = file size (low 32 bits)
;   cbFileSizeHi    = file size (high 32 bits)
; out:
;   EAX             = 1 if all resolved, 0 if any failed
; ============================================================================

GGUF_ResolveTensorPointers PROC uses esi edi ebx pTensorArray:DWORD, nTensorCount:DWORD, pBaseMemory:DWORD, cbDataOffset:DWORD, cbFileSizeLo:DWORD, cbFileSizeHi:DWORD

    mov esi, pTensorArray
    test esi, esi
    jz  @error

    mov ecx, nTensorCount
    test ecx, ecx
    jz  @success

    xor ebx, ebx                ; tensor counter

@tensor_loop:
    cmp ebx, ecx
    jge @success

    ; Resolve pointer for this tensor
    push cbFileSizeHi
    push cbFileSizeLo
    push pBaseMemory
    push esi
    call GGUF_TensorOffsetResolve
    test eax, eax
    jz  @error_during_loop

    ; Validate bounds for this tensor
    push pBaseMemory
    push cbFileSizeHi
    push cbFileSizeLo
    push esi
    call GGUF_TensorOffsetValidate
    test eax, eax
    jz  @error_during_loop

    ; Compute size for this tensor
    push esi
    call GGUF_TensorSizeCompute
    mov [esi].GGUF_TENSOR_INFO.cbTensorBytes, eax
    mov [esi].GGUF_TENSOR_INFO.cbTensorBytes + 4, edx

    ; Mark as valid and resolved
    mov dword ptr [esi].GGUF_TENSOR_INFO.dwFlags, 3  ; VALID | RESOLVED

    ; Move to next tensor
    add esi, SIZEOF GGUF_TENSOR_INFO
    inc ebx
    jmp @tensor_loop

@success:
    mov eax, 1
    ret

@error_during_loop:
    xor eax, eax
    ret

@error:
    xor eax, eax
    ret

GGUF_ResolveTensorPointers ENDP

; ============================================================================
; GGUF_TensorOffsetLoopAll - Complete resolution loop with validation
; HIGHER-LEVEL WRAPPER: validates all tensors end-to-end
; in:
;   pContext        = pointer to GGUF_MODEL_CONTEXT
; out:
;   EAX             = 1 success, 0 failure
; ============================================================================

GGUF_TensorOffsetLoopAll PROC uses esi edi ebx pContext:DWORD

    mov esi, pContext
    test esi, esi
    jz  @fail

    ; Extract fields from context
    mov eax, [esi].GGUF_MODEL_CONTEXT.pBase
    mov edx, [esi].GGUF_MODEL_CONTEXT.cbFileSize
    mov ecx, [esi].GGUF_MODEL_CONTEXT.cbFileSize + 4
    mov edi, [esi].GGUF_MODEL_CONTEXT.pTensors
    mov ebx, [esi].GGUF_MODEL_CONTEXT.nTensorsCount

    ; Call main resolution loop
    push ecx                    ; cbFileSizeHi
    push edx                    ; cbFileSizeLo
    push eax                    ; pBaseMemory
    push [esi].GGUF_MODEL_CONTEXT.cbDataOffset
    push ebx                    ; nTensorCount
    push edi                    ; pTensorArray
    call GGUF_ResolveTensorPointers

    ; Update status in context
    mov esi, pContext
    cmp eax, 0
    je  @fail_update

    mov [esi].GGUF_MODEL_CONTEXT.dwStatus, 1
    mov [esi].GGUF_MODEL_CONTEXT.dwValidated, 1
    ret

@fail_update:
    mov [esi].GGUF_MODEL_CONTEXT.dwStatus, 0
    ret

@fail:
    xor eax, eax
    ret

GGUF_TensorOffsetLoopAll ENDP

; ============================================================================
; GGUF_ValidateTensorBounds - Validate all tensor bounds
; Helper function for post-resolution validation
; in:
;   pContext        = pointer to GGUF_MODEL_CONTEXT
; out:
;   EAX             = 1 all valid, 0 any invalid
; ============================================================================

GGUF_ValidateTensorBounds PROC uses esi edi ebx pContext:DWORD

    mov esi, pContext
    test esi, esi
    jz  @fail

    mov edi, [esi].GGUF_MODEL_CONTEXT.pTensors
    mov ecx, [esi].GGUF_MODEL_CONTEXT.nTensorsCount
    mov eax, [esi].GGUF_MODEL_CONTEXT.cbFileSize
    mov edx, [esi].GGUF_MODEL_CONTEXT.cbFileSize + 4
    mov ebx, [esi].GGUF_MODEL_CONTEXT.pBase

    xor esi, esi                ; counter

@validate_loop:
    cmp esi, ecx
    jge @all_valid

    ; Validate this tensor
    push ebx
    push edx
    push eax
    push edi
    call GGUF_TensorOffsetValidate
    test eax, eax
    jz  @fail

    add edi, SIZEOF GGUF_TENSOR_INFO
    inc esi
    jmp @validate_loop

@all_valid:
    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

GGUF_ValidateTensorBounds ENDP

END
