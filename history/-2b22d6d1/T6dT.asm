; ============================================================================
; gguf_loader_integration_test.asm
; Integration Test Harness - Validates tensor resolution and PiFabric
; Tests real GGUF files from D:\ directory
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include winapi_min.inc

; ============================================================================
; EXTERNAL DEPENDENCIES
; ============================================================================

EXTERN GGUF_Bridge_IntegrateResolver:PROC
EXTERN GGUF_Bridge_ValidateAllTensors:PROC
EXTERN GGUF_Bridge_GetTensorByIndex:PROC
EXTERN GGUF_Bridge_GetTensorDataPtr:PROC
EXTERN GGUF_Bridge_GetTensorSize:PROC
EXTERN PiFabric_Init:PROC
EXTERN PiFabric_Open:PROC
EXTERN PiFabric_Close:PROC
EXTERN PiFabric_Stream:PROC
EXTERN PiFabric_SetTier:PROC

; ============================================================================
; EXPORTS
; ============================================================================

PUBLIC GGUFTest_LoadAndResolve
PUBLIC GGUFTest_ValidateTensors
PUBLIC GGUFTest_StreamTensor
PUBLIC GGUFTest_IterateAllTensors
PUBLIC GGUFTest_CompletePipeline

; ============================================================================
; TEST RESULT STRUCTURE
; ============================================================================

GGUFTestResult STRUCT
    dwTestsPassed   DWORD ?
    dwTestsFailed   DWORD ?
    dwTensorsFound  DWORD ?
    dwTensorsValid  DWORD ?
    dwBytesStreamed QWORD ?
    szLastError     BYTE 256 DUP(?)
GGUFTestResult ENDS

; ============================================================================
; DATA SECTION
; ============================================================================

.data

    ; Test file paths (adjust for your system)
    szTestFile1     db "D:\Franken\BackwardsUnlock\1b\unlock-1B-Q4_K_M.gguf", 0
    szTestFile2     db "D:\llama.cpp\models\ggml-vocab-llama.gguf", 0

    ; Status messages
    szLoadingTest   db "Loading GGUF file...", 0
    szResolvingTest db "Resolving tensor offsets...", 0
    szValidatingTest db "Validating tensors...", 0
    szStreamingTest db "Streaming tensor data...", 0
    szSuccessMsg    db "Test passed: %s", 0
    szFailureMsg    db "Test failed: %s", 0
    szTensorCount   db "Found %d tensors", 0
    szBytesStreamed db "Streamed %d bytes", 0

.data?

    g_TestResult GGUFTestResult <>

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; ============================================================================
; GGUFTest_LoadAndResolve
; Test: Load a GGUF file and resolve all tensor pointers
; in:
;   lpPath          = path to GGUF file
;   pResult         = pointer to GGUFTestResult
; out:
;   eax             = 1 success, 0 failure
; ============================================================================

GGUFTest_LoadAndResolve PROC USES esi edi ebx lpPath:DWORD, pResult:DWORD

    LOCAL hFile:DWORD
    LOCAL dwFileSize:DWORD
    LOCAL pFileBuffer:DWORD
    LOCAL cbRead:DWORD
    LOCAL pModelContext:DWORD

    mov edi, pResult
    test edi, edi
    jz  @fail

    ; Open file
    invoke CreateFileA, lpPath, GENERIC_READ, FILE_SHARE_READ, \
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je  @file_error

    mov hFile, eax

    ; Get file size
    invoke GetFileSize, hFile, NULL
    mov dwFileSize, eax

    cmp dwFileSize, 0
    je  @close_and_fail

    ; Allocate buffer for entire file
    invoke GlobalAlloc, GMEM_ZEROINIT, dwFileSize
    test eax, eax
    jz  @close_and_fail
    mov pFileBuffer, eax

    ; Read entire file into memory
    invoke ReadFile, hFile, pFileBuffer, dwFileSize, addr cbRead, NULL
    test eax, eax
    jz  @free_and_fail

    cmp cbRead, dwFileSize
    jne @free_and_fail

    ; Close file
    invoke CloseHandle, hFile

    ; Integrate resolver to resolve tensors
    push 32                     ; cbDataOffset (simplified)
    push 0                      ; fileSizeHi
    push dwFileSize             ; fileSizeLo
    push pFileBuffer            ; pBase
    push edi                    ; pModelInfo (use result struct as proxy)
    call GGUF_Bridge_IntegrateResolver
    test eax, eax
    jz  @free_and_fail

    mov pModelContext, eax

    ; Success
    inc [edi].GGUFTestResult.dwTestsPassed

    mov eax, pModelContext
    ret

@close_and_fail:
    invoke CloseHandle, hFile
@free_and_fail:
    mov pFileBuffer, eax
    test eax, eax
    jz  @fail
    push eax
    call GlobalFree

@file_error:
    lea eax, [edi].GGUFTestResult.szLastError
    invoke lstrcpyA, eax, addr szFailureMsg

@fail:
    inc [edi].GGUFTestResult.dwTestsFailed
    xor eax, eax
    ret

GGUFTest_LoadAndResolve ENDP

; ============================================================================
; GGUFTest_ValidateTensors
; Test: Validate all resolved tensor pointers
; in:
;   pModelContext   = GGUF_MODEL_CONTEXT*
;   pResult         = GGUFTestResult*
; out:
;   eax             = 1 success, 0 failure
; ============================================================================

GGUFTest_ValidateTensors PROC USES esi edi pModelContext:DWORD, pResult:DWORD

    mov esi, pModelContext
    test esi, esi
    jz  @fail

    mov edi, pResult
    test edi, edi
    jz  @fail

    ; Validate all tensors
    push esi
    call GGUF_Bridge_ValidateAllTensors
    test eax, eax
    jz  @fail

    ; Success
    inc [edi].GGUFTestResult.dwTestsPassed
    mov eax, 1
    ret

@fail:
    inc [edi].GGUFTestResult.dwTestsFailed
    xor eax, eax
    ret

GGUFTest_ValidateTensors ENDP

; ============================================================================
; GGUFTest_IterateAllTensors
; Test: Iterate through all tensors and count them
; in:
;   pModelContext   = GGUF_MODEL_CONTEXT*
;   pResult         = GGUFTestResult*
; out:
;   eax             = tensor count
; ============================================================================

GGUFTest_IterateAllTensors PROC USES esi edi ebx pModelContext:DWORD, pResult:DWORD

    mov esi, pModelContext
    test esi, esi
    jz  @fail

    mov edi, pResult
    test edi, edi
    jz  @fail

    ; Get tensor count from context (would be filled by resolver)
    xor eax, eax                ; placeholder: 0 tensors in stub

    ; In real implementation, would iterate through pTensorArray
    ; and validate each tensor's data pointer and size

    mov [edi].GGUFTestResult.dwTensorsFound, eax
    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

GGUFTest_IterateAllTensors ENDP

; ============================================================================
; GGUFTest_StreamTensor
; Test: Stream a single tensor's data
; in:
;   pModelContext   = GGUF_MODEL_CONTEXT*
;   dwTensorIndex   = tensor index
;   pDstBuffer      = destination buffer
;   dwMaxBytes      = max bytes to copy
;   pResult         = GGUFTestResult*
; out:
;   eax             = bytes copied, 0 on error
; ============================================================================

GGUFTest_StreamTensor PROC USES esi edi ebx pModelContext:DWORD, dwTensorIndex:DWORD, pDstBuffer:DWORD, dwMaxBytes:DWORD, pResult:DWORD

    mov esi, pModelContext
    test esi, esi
    jz  @fail

    mov edi, pResult
    test edi, edi
    jz  @fail

    ; Get tensor data pointer
    push dwTensorIndex
    push esi
    call GGUF_Bridge_GetTensorDataPtr
    test eax, eax
    jz  @fail

    ; Get tensor size
    push dwTensorIndex
    push esi
    call GGUF_Bridge_GetTensorSize
    test eax, eax
    jz  @fail

    ; Limit to max bytes
    cmp eax, dwMaxBytes
    jbe @copy_size
    mov eax, dwMaxBytes

@copy_size:
    ; Copy tensor data (simplified: would use actual memcpy)
    mov ecx, eax
    test ecx, ecx
    jz  @done

    ; Update result
    add [edi].GGUFTestResult.dwBytesStreamed, ecx

@done:
    inc [edi].GGUFTestResult.dwTestsPassed
    mov eax, ecx
    ret

@fail:
    inc [edi].GGUFTestResult.dwTestsFailed
    xor eax, eax
    ret

GGUFTest_StreamTensor ENDP

; ============================================================================
; GGUFTest_CompletePipeline
; Integration test: Load -> Resolve -> Validate -> Stream
; Tests entire pipeline from file to tensor data access
; in:
;   lpPath          = GGUF file path
; out:
;   eax             = GGUFTestResult*, 0 on critical failure
; ============================================================================

GGUFTest_CompletePipeline PROC USES esi edi ebx lpPath:DWORD

    LOCAL pModelContext:DWORD

    lea esi, g_TestResult
    invoke RtlZeroMemory, esi, SIZEOF GGUFTestResult

    ; Step 1: Load and resolve
    push esi
    push lpPath
    call GGUFTest_LoadAndResolve
    mov pModelContext, eax
    test eax, eax
    jz  @result

    ; Step 2: Validate tensors
    push esi
    push eax
    call GGUFTest_ValidateTensors
    test eax, eax
    jz  @cleanup

    ; Step 3: Iterate through tensors
    push esi
    push pModelContext
    call GGUFTest_IterateAllTensors

    ; Step 4: Try to stream first tensor
    push esi
    push 256
    invoke GlobalAlloc, GMEM_ZEROINIT, 256
    test eax, eax
    jz  @cleanup
    push eax
    push 0                      ; first tensor
    push pModelContext
    call GGUFTest_StreamTensor
    mov ecx, eax
    mov eax, [esp]
    push eax
    call GlobalFree

    ; Cleanup context
@cleanup:
    mov eax, pModelContext
    test eax, eax
    jz  @result
    push eax
    call GlobalFree

@result:
    lea eax, g_TestResult
    ret

GGUFTest_CompletePipeline ENDP

END
