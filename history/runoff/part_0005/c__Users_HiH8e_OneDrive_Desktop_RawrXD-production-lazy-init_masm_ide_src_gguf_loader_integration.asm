; ============================================================================
; GGUF_LOADER_INTEGRATION.ASM - Integration bridge for tensor resolver
; Adds the missing resolver calls to your existing GGUF loader
; NO EXISTING CODE REMOVED - PURE ADDITION
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc

includelib \masm32\lib\kernel32.lib

; External resolver functions
extern GGUF_ResolverComplete:proc
extern GGUF_ComputeDataSectionOffset:proc
extern GGUF_ResolveTensorPointers:proc
extern GGUF_ValidateTensorIntegrity:proc
extern GGUF_PopulateModelStruct:proc

; External from your existing loader
extern GGUFLoader_ParseHeader:proc
extern GGUFLoader_ParseKVMetadata:proc
extern GGUFLoader_ParseTensorMetadata:proc
extern GGUFLoader_LoadFileToMemory:proc

.data
    ; Debug messages
    szResolverSuccess db "Tensor resolver: SUCCESS - All tensors resolved",0
    szResolverFailed db "Tensor resolver: FAILED - Check offsets/bounds",0
    szDataSectionAt db "Data section starts at offset: 0x%08X",0
    szTensorResolved db "Tensor %d resolved: ptr=0x%08X size=%d bytes",0

.code

; ============================================================================
; GGUF_LoaderWithResolver - COMPLETE LOADER WITH RESOLVER INTEGRATED
; This is the master function that calls your existing loader + new resolver
; DROP THIS INTO YOUR EXISTING LOADER RIGHT AFTER TENSOR METADATA PARSING
; ============================================================================
public GGUF_LoaderWithResolver
GGUF_LoaderWithResolver proc uses ebx esi edi, \
    pszFilePath:DWORD, \
    pModel:DWORD
    
    LOCAL base_ptr:DWORD
    LOCAL file_size:DWORD
    LOCAL header_size:DWORD
    LOCAL kv_size:DWORD
    LOCAL tensor_meta_size:DWORD
    LOCAL tensor_array_ptr:DWORD
    LOCAL tensor_count:DWORD
    LOCAL data_section_ptr:DWORD
    
    ; ========================================================================
    ; STEP 1: Load entire GGUF file into memory (YOUR EXISTING CODE)
    ; ========================================================================
    invoke GGUFLoader_LoadFileToMemory, pszFilePath
    test eax, eax
    jz @LoadFailed
    mov base_ptr, eax
    
    ; Get file size (assume stored in your loader's global or passed back)
    ; TODO: Replace this with actual file size from your loader
    mov file_size, 0             ; REPLACE WITH ACTUAL VALUE
    
    ; ========================================================================
    ; STEP 2: Parse header (YOUR EXISTING CODE)
    ; ========================================================================
    invoke GGUFLoader_ParseHeader, base_ptr, pModel
    test eax, eax
    jz @ParseFailed
    mov header_size, eax         ; Assuming header size returned
    
    ; ========================================================================
    ; STEP 3: Parse KV metadata (YOUR EXISTING CODE)
    ; ========================================================================
    mov eax, base_ptr
    add eax, header_size
    invoke GGUFLoader_ParseKVMetadata, eax, pModel
    test eax, eax
    jz @ParseFailed
    mov kv_size, eax             ; Assuming KV size returned
    
    ; ========================================================================
    ; STEP 4: Parse tensor metadata (YOUR EXISTING CODE)
    ; ========================================================================
    mov eax, base_ptr
    add eax, header_size
    add eax, kv_size
    invoke GGUFLoader_ParseTensorMetadata, eax, pModel
    test eax, eax
    jz @ParseFailed
    mov tensor_meta_size, eax    ; Assuming tensor meta size returned
    
    ; Get tensor count and array pointer from model
    ; TODO: Replace with actual field offsets from your model struct
    mov edi, pModel
    mov eax, [edi+8]             ; model.tensor_count
    mov tensor_count, eax
    mov eax, [edi+12]            ; model.tensor_array_ptr
    mov tensor_array_ptr, eax
    
    ; ========================================================================
    ; STEP 5: CALL THE NEW RESOLVER (DROPS IN HERE)
    ; ========================================================================
    invoke GGUF_ResolverComplete, \
           pModel, base_ptr, header_size, kv_size, tensor_meta_size, \
           tensor_array_ptr, tensor_count, file_size
    test eax, eax
    jz @ResolverFailed
    
    ; ========================================================================
    ; SUCCESS - Model is now fully loaded and ready
    ; ========================================================================
    mov eax, pModel
    ret
    
@LoadFailed:
    xor eax, eax
    ret
    
@ParseFailed:
    ; Free memory if needed
    xor eax, eax
    ret
    
@ResolverFailed:
    ; Log error
    xor eax, eax
    ret
    
GGUF_LoaderWithResolver endp

; ============================================================================
; GGUF_PatchExistingLoader - ONE-LINE FIX for your existing loader
; Call this function RIGHT AFTER your tensor metadata parsing completes
; This is the minimal integration point
; ============================================================================
public GGUF_PatchExistingLoader
GGUF_PatchExistingLoader proc uses ebx esi edi, \
    pModel:DWORD
    
    LOCAL base_ptr:DWORD
    LOCAL header_size:DWORD
    LOCAL kv_size:DWORD
    LOCAL tensor_meta_size:DWORD
    LOCAL tensor_array_ptr:DWORD
    LOCAL tensor_count:DWORD
    LOCAL file_size:DWORD
    
    ; Extract values from model struct (YOUR STRUCT LAYOUT)
    mov edi, pModel
    mov eax, [edi]               ; model.base_ptr
    mov base_ptr, eax
    mov eax, [edi+16]            ; model.file_size
    mov file_size, eax
    mov eax, [edi+8]             ; model.tensor_count
    mov tensor_count, eax
    mov eax, [edi+12]            ; model.tensor_array_ptr
    mov tensor_array_ptr, eax
    
    ; TODO: Get header_size, kv_size, tensor_meta_size from your loader
    ; These should be computed/stored during parsing
    mov header_size, 0           ; REPLACE WITH ACTUAL
    mov kv_size, 0               ; REPLACE WITH ACTUAL
    mov tensor_meta_size, 0      ; REPLACE WITH ACTUAL
    
    ; Call resolver
    invoke GGUF_ResolverComplete, \
           pModel, base_ptr, header_size, kv_size, tensor_meta_size, \
           tensor_array_ptr, tensor_count, file_size
    
    ret
GGUF_PatchExistingLoader endp

; ============================================================================
; GGUF_IntegrationTest - Test resolver with real Ollama model
; ============================================================================
public GGUF_IntegrationTest
GGUF_IntegrationTest proc uses ebx esi edi
    LOCAL szTestPath[260]:BYTE
    LOCAL model:BYTE[1024]
    
    ; Test with 350M model
    invoke lstrcpy, addr szTestPath, offset szOllamaModel350M
    
    ; Load model
    invoke GGUF_LoaderWithResolver, addr szTestPath, addr model
    test eax, eax
    jz @TestFailed
    
    ; Validate model loaded correctly
    mov edi, eax
    mov eax, [edi+8]             ; Check tensor_count > 0
    test eax, eax
    jz @TestFailed
    
    mov eax, [edi+12]            ; Check tensor_array_ptr != NULL
    test eax, eax
    jz @TestFailed
    
    ; Success
    mov eax, TRUE
    ret
    
@TestFailed:
    xor eax, eax
    ret
    
.data
    szOllamaModel350M db "D:\Franken\BackwardsUnlock\350m\unlock-350M-Q3_K_M.gguf",0
    szOllamaModel1B db "D:\Franken\BackwardsUnlock\1b\unlock-1B-Q4_K_M.gguf",0
    szLlamaVocab db "D:\llama.cpp\models\ggml-vocab-llama.gguf",0
    
.code
GGUF_IntegrationTest endp

; ============================================================================
; GGUF_LogTensorInfo - Debug helper to verify tensor resolution
; ============================================================================
public GGUF_LogTensorInfo
GGUF_LogTensorInfo proc uses ebx esi edi, \
    tensor_array_ptr:DWORD, \
    tensor_count:DWORD
    
    LOCAL i:DWORD
    LOCAL szBuffer[256]:BYTE
    
    mov i, 0
    
@LogLoop:
    mov eax, i
    cmp eax, tensor_count
    jge @LogDone
    
    ; Get tensor[i]
    imul eax, 64                 ; sizeof GGUF_TENSOR_INFO
    mov esi, tensor_array_ptr
    add esi, eax
    
    ; Log: "Tensor %d: ptr=0x%08X size=%d"
    mov eax, [esi+32]            ; data_ptr
    mov ecx, [esi+36]            ; byte_size
    
    ; TODO: Add actual logging/printf
    ; For now, just validate pointers are non-zero
    test eax, eax
    jz @LogFailed
    test ecx, ecx
    jz @LogFailed
    
    inc i
    jmp @LogLoop
    
@LogDone:
    mov eax, TRUE
    ret
    
@LogFailed:
    xor eax, eax
    ret
    
GGUF_LogTensorInfo endp

end
