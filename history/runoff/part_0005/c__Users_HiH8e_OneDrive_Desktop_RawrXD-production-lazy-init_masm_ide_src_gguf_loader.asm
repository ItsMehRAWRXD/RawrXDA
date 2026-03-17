; ============================================================================
; RawrXD Agentic IDE - GGUF Model Loader Implementation
; Pure MASM - Basic GGUF File Format Parser
; ============================================================================

.686
.model flat, stdcall
option casemap:none



include ..\\include\\winapi_min.inc
include ..\\include\\winapi_min.inc
; External UI handles used in this module
extern hMainWindow:DWORD
extern hStatus:DWORD

include constants.inc
include structures.inc
include macros.inc

; ============================================================================
; Constants for GGUF format
; ============================================================================

; GGUF magic numbers
GGUF_MAGIC_GGML      equ 67676D6Ch  ; "ggml" in little-endian
GGUF_MAGIC_GGMF      equ 67676D66h  ; "ggmf" in little-endian
GGUF_MAGIC_GGJT      equ 67676A74h  ; "ggjt" in little-endian
GGUF_MAGIC_GGLA      equ 67676C61h  ; "ggla" in little-endian

; GGUF version constants
GGUF_VERSION_1       equ 1
GGUF_VERSION_2       equ 2
GGUF_VERSION_3       equ 3

.data

; GGUF data types
GGUF_TYPE_UINT8      equ 0
GGUF_TYPE_INT8       equ 1
GGUF_TYPE_UINT16     equ 2
GGUF_TYPE_INT16      equ 3
GGUF_TYPE_UINT32     equ 4
GGUF_TYPE_INT32      equ 5
GGUF_TYPE_FLOAT32    equ 6
GGUF_TYPE_BOOL       equ 7
GGUF_TYPE_STRING     equ 8
GGUF_TYPE_ARRAY      equ 9
GGUF_TYPE_UINT64     equ 10
GGUF_TYPE_INT64      equ 11
GGUF_TYPE_FLOAT64    equ 12

; ============================================================================
; Structures for GGUF parsing
; ============================================================================

GGUF_HEADER struct
    magic           dd ?    ; Magic number
    version         dd ?    ; Version
    n_tensors       dq ?    ; Number of tensors
    n_kv            dq ?    ; Number of key-value pairs
GGUF_HEADER ends

GGUF_KV_PAIR struct
    key_len         dd ?    ; Length of key
    key_data        db 256 dup(?)  ; Key string (max 256 chars)
    value_type      dd ?    ; Value type
    value_len       dd ?    ; Length of value
    value_data      db 1024 dup(?) ; Value data (max 1024 bytes)
GGUF_KV_PAIR ends

GGUF_TENSOR_INFO struct
    name_len        dd ?    ; Length of tensor name
    name_data       db 256 dup(?)  ; Tensor name (max 256 chars)
    n_dims          dd ?    ; Number of dimensions
    dims            dd 8 dup(?)    ; Dimensions (max 8)
    ggml_type       dd ?    ; GGML data type
    file_offset     dq ?    ; Offset in file
GGUF_TENSOR_INFO ends

GGUF_MODEL_INFO struct
    header          GGUF_HEADER <>
    kv_pairs        dd ?    ; Pointer to key-value pairs
    tensors         dd ?    ; Pointer to tensor info
    file_handle     dd ?    ; File handle
    file_size       dd ?    ; File size
    is_valid        dd ?    ; Valid GGUF file flag
    szModelPath     db MAX_PATH_SIZE dup(?) ; Model file path
GGUF_MODEL_INFO ends

; ============================================================================
; DATA SECTION
; ============================================================================
    szGGUFExtension  db ".gguf", 0
    szModelFilter    db "GGUF Models (*.gguf)", 0, "*.gguf", 0, 
                        "All Files (*.*)", 0, "*.*", 0, 0
    szErrorInvalid   db "Invalid GGUF file format", 0
    szErrorVersion   db "Unsupported GGUF version", 0
    szErrorIO        db "File I/O error", 0
    szLoadingModel   db "Loading GGUF model...", 0
    szModelLoaded    db "Model loaded successfully", 0
    szModelError     db "Error loading model", 0
    szQueryTitle     db "GGUF Loader", 0
    szCRLF           db 13, 10, 0
    szInfoTitle      db "GGUF Model Information:", 0
    szSeparator      db "======================", 0
    szFileLabel      db "File: ", 0
    szSizeLabel      db "Size: ", 0
    szMagicLabel     db "Magic: 0x", 0
    szVersionLabel   db "Version: ", 0
    szTensorsLabel   db "Tensors: ", 0
    szKvLabel        db "Key-Value Pairs: ", 0
    szFmtBytes       db "%d bytes", 0
    szFmtHex         db "%08X", 0
    szFmtDec         db "%d", 0
    
    ; Common GGUF keys
    szKeyGeneralArch  db "general.architecture", 0
    szKeyGeneralName  db "general.name", 0
    szKeyGeneralQuant db "general.quantization_version", 0
    szKeyGeneralAlign db "general.alignment", 0
    szKeyGeneralFileT db "general.file_type", 0
    szKeyLLMContextL  db "llama.context_length", 0
    szKeyLLMEmbd     db "llama.embedding_length", 0
    szKeyLLMBlockC   db "llama.block_count", 0
    szKeyLLMFeedF    db "llama.feed_forward_length", 0
    szKeyLLMHeadC    db "llama.attention.head_count", 0
    szKeyLLMHeadCKV  db "llama.attention.head_count_kv", 0
    szKeyLLMRopeDim  db "llama.rope.dimension_count", 0
    szKeyTokenizerM  db "tokenizer.ggml.model", 0
    szKeyTokenizerN  db "tokenizer.ggml.tokens", 0
    szKeyTokenizerS  db "tokenizer.ggml.scores", 0
    szKeyTokenizerB  db "tokenizer.ggml.bos_token_id", 0
    szKeyTokenizerE  db "tokenizer.ggml.eos_token_id", 0
    szKeyTokenizerU  db "tokenizer.ggml.unknown_token_id", 0
    szKeyTokenizerS2 db "tokenizer.ggml.separator", 0

.data?
    g_pCurrentModel  dd ?    ; Pointer to current model info
    g_dwModelCount   dd ?    ; Number of loaded models

 .code

; ============================================================================
; PROCEDURES
; ============================================================================

; ============================================================================
; GGUFLoader_Init - Initialize GGUF loader system
; Returns: TRUE in eax on success
; ============================================================================
GGUFLoader_Init proc
    ; Initialize global variables
    mov g_pCurrentModel, 0
    mov g_dwModelCount, 0
    
    mov eax, TRUE  ; Success
    ret
GGUFLoader_Init endp

; ============================================================================
; GGUFLoader_LoadModel - Load a GGUF model file
; Input: pszModelPath
; Returns: Pointer to GGUF_MODEL_INFO in eax, NULL on error
; ============================================================================
GGUFLoader_LoadModel proc pszModelPath:DWORD
    LOCAL pModelInfo:DWORD
    LOCAL hFile:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL header:GGUF_HEADER
    LOCAL szError[256]:BYTE
    
    ; Allocate model info structure
    MemAlloc sizeof GGUF_MODEL_INFO
    mov pModelInfo, eax
    test eax, eax
    jz @Exit
    
    ; Initialize structure
    MemZero pModelInfo, sizeof GGUF_MODEL_INFO
    
    ; Copy model path
    mov eax, pModelInfo
    assume eax:ptr GGUF_MODEL_INFO
    invoke lstrcpyA, addr [eax].szModelPath, pszModelPath
    assume eax:nothing
    
    ; Open file
    invoke CreateFile, pszModelPath, GENERIC_READ, FILE_SHARE_READ, 
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    cmp eax, INVALID_HANDLE_VALUE
    je @FileError
    
    ; Store file handle
    mov eax, pModelInfo
    assume eax:ptr GGUF_MODEL_INFO
    mov edx, hFile
    mov [eax].file_handle, edx
    assume eax:nothing
    
    ; Get file size
    invoke GetFileSize, hFile, NULL
    mov ecx, pModelInfo
    assume ecx:ptr GGUF_MODEL_INFO
    mov [ecx].file_size, eax
    assume ecx:nothing
    
    ; Read header
    invoke ReadFile, hFile, addr header, sizeof GGUF_HEADER, addr dwBytesRead, NULL
    test eax, eax
    jz @IOError
    cmp dwBytesRead, sizeof GGUF_HEADER
    jne @IOError
    
    ; Validate magic number
    mov eax, header.magic
    cmp eax, GGUF_MAGIC_GGML
    je @MagicOK
    cmp eax, GGUF_MAGIC_GGMF
    je @MagicOK
    cmp eax, GGUF_MAGIC_GGJT
    je @MagicOK
    cmp eax, GGUF_MAGIC_GGLA
    je @MagicOK
    invoke lstrcpyA, addr szError, addr szErrorInvalid
    jmp @FormatError
@MagicOK:
    
    ; Validate version
    mov eax, header.version
    cmp eax, GGUF_VERSION_1
    jl @BadVersion
    cmp eax, GGUF_VERSION_3
    jg @BadVersion
    jmp @StoreHeader
@BadVersion:
    invoke lstrcpyA, addr szError, addr szErrorVersion
    jmp @FormatError
    
@StoreHeader:
    ; Store header
    mov eax, pModelInfo
    assume eax:ptr GGUF_MODEL_INFO
    mov ecx, header.magic
    mov [eax].header.magic, ecx
    mov ecx, header.version
    mov [eax].header.version, ecx
    mov ecx, dword ptr header.n_tensors
    mov dword ptr [eax].header.n_tensors, ecx
    mov ecx, dword ptr header.n_kv
    mov dword ptr [eax].header.n_kv, ecx
    mov [eax].is_valid, TRUE
    assume eax:nothing
    
    ; Parse key-value pairs
    mov eax, pModelInfo
    assume eax:ptr GGUF_MODEL_INFO
    mov ecx, [eax].GGUF_MODEL_INFO.header.n_kv
    assume eax:nothing
    
    push pModelInfo
    push ecx
    push hFile
    call GGUFLoader_ParseKeyValuePairs
    test eax, eax
    jz @IOError
    
    ; Parse tensor info
    mov eax, pModelInfo
    assume eax:ptr GGUF_MODEL_INFO
    mov ecx, [eax].GGUF_MODEL_INFO.header.n_tensors
    assume eax:nothing
    
    push pModelInfo
    push ecx
    push hFile
    call GGUFLoader_ParseTensorInfo
    test eax, eax
    jz @IOError
    
    ; Success
    mov eax, pModelInfo
    inc g_dwModelCount
    jmp @Exit
    
@FileError:
    invoke lstrcpyA, addr szError, addr szErrorIO
    jmp @Error
    
@IOError:
    invoke lstrcpyA, addr szError, addr szErrorIO
    jmp @Error
    
@FormatError:
    ; Fall through to error handling
    
@Error:
    ; Clean up on error
    .if hFile != INVALID_HANDLE_VALUE
        invoke CloseHandle, hFile
    .endif
    .if pModelInfo != 0
        MemFree pModelInfo
    .endif
    invoke MessageBoxA, NULL, addr szError, addr szQueryTitle, MB_OK or MB_ICONERROR
    xor eax, eax
    
@Exit:
    ret
GGUFLoader_LoadModel endp

; ============================================================================
; GGUFLoader_ParseKeyValuePairs - Parse GGUF key-value pairs
; Input: hFile, n_kv count
; Output: eax = pointer to KV buffer, NULL on error
; ============================================================================
GGUFLoader_ParseKeyValuePairs proc USES esi edi hFile:DWORD, nKVCount:DWORD, pModel:DWORD

    LOCAL pKVBuffer:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL cbKVSize:DWORD

    mov esi, pModel
    test esi, esi
    jz  @fail

    ; Allocate KV buffer (16KB max for KV section)
    mov cbKVSize, 16384
    invoke GlobalAlloc, GMEM_ZEROINIT, cbKVSize
    test eax, eax
    jz  @fail
    mov pKVBuffer, eax

    ; Store in model
    mov [esi].GGUF_MODEL_INFO.kv_pairs, pKVBuffer

    ; Read KV data from file
    invoke ReadFile, hFile, pKVBuffer, cbKVSize, addr dwBytesRead, NULL
    test eax, eax
    jz  @fail

    mov eax, pKVBuffer
    ret

@fail:
    xor eax, eax
    ret
GGUFLoader_ParseKeyValuePairs endp

; ============================================================================
; GGUFLoader_ParseTensorInfo - Parse GGUF tensor information
; Input: hFile, n_tensors count, pModel
; Output: eax = pointer to tensor array, NULL on error
; ============================================================================
GGUFLoader_ParseTensorInfo proc USES esi edi ebx hFile:DWORD, nTensorCount:DWORD, pModel:DWORD

    LOCAL pTensorBuffer:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL cbTensorSize:DWORD
    LOCAL i:DWORD

    mov esi, pModel
    test esi, esi
    jz  @fail

    ; Allocate tensor array (256 bytes per tensor info max)
    mov eax, nTensorCount
    imul eax, 256
    mov cbTensorSize, eax

    invoke GlobalAlloc, GMEM_ZEROINIT, cbTensorSize
    test eax, eax
    jz  @fail
    mov pTensorBuffer, eax

    ; Store in model
    mov [esi].GGUF_MODEL_INFO.tensors, pTensorBuffer

    ; Read tensor metadata
    invoke ReadFile, hFile, pTensorBuffer, cbTensorSize, addr dwBytesRead, NULL
    test eax, eax
    jz  @fail

    mov eax, pTensorBuffer
    ret

@fail:
    xor eax, eax
    ret
GGUFLoader_ParseTensorInfo endp

; ============================================================================
; GGUFLoader_ComputeTensorSize - Compute tensor size from dimensions
; Input: pTensorInfo (placeholder for full structure)
; Output: EDX:EAX = size in bytes
; ============================================================================

GGUFLoader_ComputeTensorSize proc USES esi edi ebx

    ; Type size table (basic GGML types)
    ; F32=4, F16=2, Q4=1, Q5=1, Q8=1, etc.
    mov eax, 4                  ; Default F32 size
    xor edx, edx
    ret

GGUFLoader_ComputeTensorSize endp

; ============================================================================
; GGUFLoader_GetModelInfo - Get information about loaded model
; Input: pModelInfo
; Output: Fills szBuffer with model information
; ============================================================================
GGUFLoader_GetModelInfo proc pModelInfo:DWORD, szBuffer:DWORD, dwBufferSize:DWORD
    LOCAL szTemp[1024]:BYTE
    LOCAL szFmtBuf[128]:BYTE
    LOCAL pModel:DWORD
    LOCAL header:GGUF_HEADER
    
    mov eax, pModelInfo
    mov pModel, eax
    assume eax:ptr GGUF_MODEL_INFO
    lea esi, [eax].header
    assume esi:ptr GGUF_HEADER
    lea edi, header
    mov ecx, sizeof GGUF_HEADER
    rep movsb
    assume esi:nothing
    assume eax:nothing
    
    ; Format model information
    invoke lstrcpyA, addr szTemp, addr szInfoTitle
    invoke lstrcatA, addr szTemp, addr szCRLF
    invoke lstrcatA, addr szTemp, addr szSeparator
    invoke lstrcatA, addr szTemp, addr szCRLF
    invoke lstrcatA, addr szTemp, addr szCRLF
    
    ; Add file path
    invoke lstrcatA, addr szTemp, addr szFileLabel
    mov eax, pModel
    assume eax:ptr GGUF_MODEL_INFO
    invoke lstrcatA, addr szTemp, addr [eax].szModelPath
    assume eax:nothing
    invoke lstrcatA, addr szTemp, addr szCRLF
    
    ; Add file size
    invoke lstrcatA, addr szTemp, addr szSizeLabel
    mov eax, pModel
    assume eax:ptr GGUF_MODEL_INFO
    invoke wsprintfA, addr szFmtBuf, addr szFmtBytes, [eax].file_size
    invoke lstrcatA, addr szTemp, addr szFmtBuf
    assume eax:nothing
    invoke lstrcatA, addr szTemp, addr szCRLF
    
    ; Add header info
    invoke lstrcatA, addr szTemp, addr szMagicLabel
    invoke wsprintfA, addr szFmtBuf, addr szFmtHex, header.magic
    invoke lstrcatA, addr szTemp, addr szFmtBuf
    invoke lstrcatA, addr szTemp, addr szCRLF
    
    invoke lstrcatA, addr szTemp, addr szVersionLabel
    invoke wsprintfA, addr szFmtBuf, addr szFmtDec, header.version
    invoke lstrcatA, addr szTemp, addr szFmtBuf
    invoke lstrcatA, addr szTemp, addr szCRLF
    
    invoke lstrcatA, addr szTemp, addr szTensorsLabel
    invoke wsprintfA, addr szFmtBuf, addr szFmtDec, header.n_tensors
    invoke lstrcatA, addr szTemp, addr szFmtBuf
    invoke lstrcatA, addr szTemp, addr szCRLF
    
    invoke lstrcatA, addr szTemp, addr szKvLabel
    invoke wsprintfA, addr szFmtBuf, addr szFmtDec, header.n_kv
    invoke lstrcatA, addr szTemp, addr szFmtBuf
    invoke lstrcatA, addr szTemp, addr szCRLF
    
    ; Copy to output buffer
    invoke lstrcpyA, szBuffer, addr szTemp
    
    ret
GGUFLoader_GetModelInfo endp

; ============================================================================
; GGUFLoader_ShowModelDialog - Show dialog to select and load GGUF model
; ============================================================================
GGUFLoader_ShowModelDialog proc
    LOCAL ofn:OPENFILENAME
    LOCAL szFile[ MAX_PATH ]:BYTE
    LOCAL pModelInfo:DWORD
    
    ; Initialize OPENFILENAME structure
    push edi
    lea edi, ofn
    mov ecx, sizeof OPENFILENAME
    xor eax, eax
    rep stosb
    pop edi
    mov ofn.lStructSize, sizeof OPENFILENAME
    mov eax, hMainWindow
    mov ofn.hwndOwner, eax
    lea eax, szFile
    mov ofn.lpstrFile, eax
    mov ofn.nMaxFile, MAX_PATH
    lea eax, szModelFilter
    mov ofn.lpstrFilter, eax
    mov ofn.nFilterIndex, 1
    mov ofn.lpstrFileTitle, NULL
    mov ofn.nMaxFileTitle, 0
    mov ofn.lpstrInitialDir, NULL
    lea eax, szLoadingModel
    mov ofn.lpstrTitle, eax
    mov ofn.Flags, OFN_PATHMUSTEXIST or OFN_FILEMUSTEXIST or OFN_EXPLORER
    
    ; Show open dialog
    invoke GetOpenFileNameA, addr ofn
    test eax, eax
    jz @Exit
    
    ; Show loading message
    invoke SetWindowTextA, hStatus, addr szLoadingModel
    
    ; Load model
    invoke GGUFLoader_LoadModel, addr szFile
    mov pModelInfo, eax
    test eax, eax
    jz @LoadError
    
    ; Store current model
    mov eax, pModelInfo
    mov g_pCurrentModel, eax
    
    ; Show success message
    invoke SetWindowTextA, hStatus, addr szModelLoaded
    invoke MessageBoxA, hMainWindow, addr szModelLoaded, addr szQueryTitle, MB_OK or MB_ICONINFORMATION
    jmp @Exit
    
@LoadError:
    invoke SetWindowTextA, hStatus, addr szModelError
    invoke MessageBoxA, hMainWindow, addr szModelError, addr szQueryTitle, MB_OK or MB_ICONERROR
    
@Exit:
    ret
GGUFLoader_ShowModelDialog endp

; ============================================================================
; GGUFLoader_GetCurrentModel - Get pointer to current loaded model
; Returns: Pointer to GGUF_MODEL_INFO in eax
; ============================================================================
GGUFLoader_GetCurrentModel proc
    mov eax, g_pCurrentModel
    ret
GGUFLoader_GetCurrentModel endp

; ============================================================================
; GGUFLoader_UnloadModel - Unload a GGUF model
; Input: pModelInfo
; ============================================================================
GGUFLoader_UnloadModel proc pModelInfo:DWORD
    LOCAL hFile:DWORD
    
    .if pModelInfo != 0
        ; Get file handle
        mov eax, pModelInfo
        assume eax:ptr GGUF_MODEL_INFO
        mov eax, [eax].file_handle
        mov hFile, eax
        assume eax:nothing
        
        ; Close file
        cmp hFile, 0
        je @SkipClose
        cmp hFile, INVALID_HANDLE_VALUE
        je @SkipClose
        invoke CloseHandle, hFile
    @SkipClose:
        
        ; Free memory
        MemFree pModelInfo
        
        ; Update current model pointer if needed
        mov eax, pModelInfo
        .if eax == g_pCurrentModel
            mov g_pCurrentModel, 0
        .endif
        
        dec g_dwModelCount
    .endif
    
    ret
GGUFLoader_UnloadModel endp

; ============================================================================
; GGUFLoader_Cleanup - Cleanup GGUF loader system
; ============================================================================
GGUFLoader_Cleanup proc
    ; Unload current model if loaded
    .if g_pCurrentModel != 0
        invoke GGUFLoader_UnloadModel, g_pCurrentModel
    .endif
    
    ret
GGUFLoader_Cleanup endp

; ============================================================================
; GGUFLoader_GetModelParameter - Get a specific model parameter
; Input: pModelInfo, pszKey
; Returns: Pointer to value in eax, NULL if not found
; ============================================================================
GGUFLoader_GetModelParameter proc pModelInfo:DWORD, pszKey:DWORD
    ; In a full implementation, this would search key-value pairs for the specified key
    ; For now, we'll just return NULL to indicate not found
    xor eax, eax
    ret
GGUFLoader_GetModelParameter endp

; ============================================================================
; GGUFLoader_EnumerateModels - Get list of loaded models
; Returns: Pointer to model list in eax, count in ecx
; ============================================================================
GGUFLoader_EnumerateModels proc
    lea eax, g_pCurrentModel
    mov ecx, g_dwModelCount
    ret
GGUFLoader_EnumerateModels endp

; ============================================================================
; Data
; ============================================================================

.data
    szModelInfoFormat db "Model: %s", 13, 10
                      db "Version: %d", 13, 10
                      db "Tensors: %d", 13, 10
                      db "KV Pairs: %d", 13, 10, 0

end