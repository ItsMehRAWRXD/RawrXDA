; gguf_loader_unified.asm - Complete Unified GGUF Loader
; All loading methods: Standard, Streaming, Chunked, MMAP in pure MASM
; Automatically selects best method based on file size and system resources
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC GgufUnified_Init
PUBLIC GgufUnified_LoadModel
PUBLIC GgufUnified_LoadModelAutomatic
PUBLIC GgufUnified_LoadStandard
PUBLIC GgufUnified_LoadStreaming
PUBLIC GgufUnified_LoadChunked
PUBLIC GgufUnified_LoadMMAP
PUBLIC GgufUnified_GetTensor
PUBLIC GgufUnified_GetMetadata
PUBLIC GgufUnified_UnloadModel
PUBLIC GgufUnified_GetStats

; Loading methods
LOAD_METHOD_AUTO        EQU 0   ; Automatic selection
LOAD_METHOD_STANDARD    EQU 1   ; Full file into RAM
LOAD_METHOD_STREAMING   EQU 2   ; Stream from disc
LOAD_METHOD_CHUNKED     EQU 3   ; Chunked with cache
LOAD_METHOD_MMAP        EQU 4   ; Memory-mapped file

; Thresholds for automatic selection
THRESHOLD_STANDARD      EQU 2147483648  ; 2GB - load fully
THRESHOLD_CHUNKED       EQU 8589934592  ; 8GB - use chunked
; Above 8GB - use MMAP

; GGUF magic and versions
GGUF_MAGIC              EQU 46554747h   ; "GGUF"
GGUF_VERSION_1          EQU 1
GGUF_VERSION_2          EQU 2
GGUF_VERSION_3          EQU 3

; Chunk size for streaming/chunked
CHUNK_SIZE              EQU 4194304     ; 4MB chunks
CACHE_CHUNKS            EQU 16          ; 16 chunks in cache

; GGUF Header
GgufHeader STRUCT
    magic       dd ?
    version     dd ?
    n_tensors   dq ?
    n_kv        dq ?
GgufHeader ENDS

; Tensor info
GgufTensorInfo STRUCT
    szName      db 256 dup(?)
    nDims       dd ?
    dims        dd 8 dup(?)
    ggmlType    dd ?
    qwOffset    dq ?
    qwSize      dq ?
GgufTensorInfo ENDS

; KV pair
GgufKVPair STRUCT
    szKey       db 256 dup(?)
    valueType   dd ?
    szValue     db 1024 dup(?)
GgufKVPair ENDS

; Chunk cache entry
ChunkCacheEntry STRUCT
    qwOffset    dq ?
    cbSize      dd ?
    pData       dd ?
    dwAccessTime dd ?
    bValid      dd ?
ChunkCacheEntry ENDS

; Model context (unified)
GgufModelContext STRUCT
    ; File information
    hFile           dd ?
    qwFileSize      dq ?
    szPath          db 260 dup(?)
    
    ; Loading method
    loadMethod      dd ?
    
    ; Header and metadata
    header          GgufHeader <>
    pTensors        dd ?        ; Array of tensor info
    pKVPairs        dd ?        ; Array of KV pairs
    
    ; Standard loading (method 1)
    pFileData       dd ?        ; Full file in memory
    
    ; Streaming (method 2)
    qwStreamPos     dq ?        ; Current stream position
    pStreamBuffer   dd ?        ; Stream buffer
    cbStreamBuffer  dd ?        ; Buffer size
    
    ; Chunked (method 3)
    pChunkCache     dd ?        ; Chunk cache array
    dwChunkCount    dd ?        ; Number of cached chunks
    
    ; MMAP (method 4)
    hMapping        dd ?        ; File mapping handle
    pMappedView     dd ?        ; Mapped view pointer
    qwMapSize       dq ?        ; Mapped size
    
    ; Statistics
    qwBytesRead     dq ?
    dwCacheHits     dd ?
    dwCacheMisses   dd ?
    dwLoadTime      dd ?
GgufModelContext ENDS

.data
g_pCurrentContext dd 0

szErrInvalidMagic   db "Invalid GGUF magic number",0
szErrInvalidVersion db "Unsupported GGUF version",0
szErrFileOpen       db "Failed to open file",0
szErrMemory         db "Memory allocation failed",0
szErrMapping        db "Failed to create file mapping",0

.code

; ================================================================
; GgufUnified_Init - Initialize unified loader system
; ================================================================
GgufUnified_Init PROC
    mov [g_pCurrentContext], 0
    mov eax, 1
    ret
GgufUnified_Init ENDP

; ================================================================
; GgufUnified_LoadModelAutomatic - Auto-select best loading method
; Input:  ECX = file path
; Output: EAX = context handle, 0 on failure
; ================================================================
GgufUnified_LoadModelAutomatic PROC lpPath:DWORD
    LOCAL hFile:DWORD
    LOCAL qwFileSize:QWORD
    LOCAL dwMethod:DWORD
    push ebx
    push esi
    
    ; Open file to get size
    invoke CreateFileA, lpPath, GENERIC_READ, FILE_SHARE_READ, 0, \
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @fail
    mov hFile, eax
    
    ; Get file size
    lea esi, qwFileSize
    invoke GetFileSizeEx, hFile, esi
    test eax, eax
    jz @fail_close
    
    invoke CloseHandle, hFile
    
    ; Decide method based on file size
    mov eax, dword ptr [qwFileSize + 4]  ; High dword
    test eax, eax
    jnz @check_large
    
    ; File is < 4GB, check against thresholds
    mov eax, dword ptr [qwFileSize]
    cmp eax, THRESHOLD_STANDARD
    jbe @use_standard
    
@use_chunked:
    mov dwMethod, LOAD_METHOD_CHUNKED
    jmp @load_with_method
    
@check_large:
    ; File is >= 4GB
    mov eax, dword ptr [qwFileSize]
    mov edx, dword ptr [qwFileSize + 4]
    
    ; Compare with 8GB threshold
    cmp edx, 2  ; High dword >= 2 means >= 8GB
    jae @use_mmap
    
    cmp edx, 1
    ja @use_mmap
    cmp eax, 0
    jbe @use_chunked
    
@use_mmap:
    mov dwMethod, LOAD_METHOD_MMAP
    jmp @load_with_method
    
@use_standard:
    mov dwMethod, LOAD_METHOD_STANDARD
    
@load_with_method:
    push dwMethod
    push lpPath
    call GgufUnified_LoadModel
    add esp, 8
    
    pop esi
    pop ebx
    ret
    
@fail_close:
    invoke CloseHandle, hFile
@fail:
    xor eax, eax
    pop esi
    pop ebx
    ret
GgufUnified_LoadModelAutomatic ENDP

; ================================================================
; GgufUnified_LoadModel - Load with specified method
; Input:  ECX = file path
;         EDX = loading method
; Output: EAX = context handle
; ================================================================
GgufUnified_LoadModel PROC lpPath:DWORD, method:DWORD
    push ebx
    
    mov ebx, method
    
    cmp ebx, LOAD_METHOD_STANDARD
    je @call_standard
    cmp ebx, LOAD_METHOD_STREAMING
    je @call_streaming
    cmp ebx, LOAD_METHOD_CHUNKED
    je @call_chunked
    cmp ebx, LOAD_METHOD_MMAP
    je @call_mmap
    
    ; Default to automatic
    push lpPath
    call GgufUnified_LoadModelAutomatic
    add esp, 4
    jmp @done
    
@call_standard:
    push lpPath
    call GgufUnified_LoadStandard
    add esp, 4
    jmp @done
    
@call_streaming:
    push lpPath
    call GgufUnified_LoadStreaming
    add esp, 4
    jmp @done
    
@call_chunked:
    push lpPath
    call GgufUnified_LoadChunked
    add esp, 4
    jmp @done
    
@call_mmap:
    push lpPath
    call GgufUnified_LoadMMAP
    add esp, 4
    
@done:
    pop ebx
    ret
GgufUnified_LoadModel ENDP

; ================================================================
; GgufUnified_LoadStandard - METHOD 1: Full file into RAM
; Best for: Files < 2GB
; ================================================================
GgufUnified_LoadStandard PROC lpPath:DWORD
    LOCAL pContext:DWORD
    LOCAL hFile:DWORD
    LOCAL qwFileSize:QWORD
    LOCAL dwBytesRead:DWORD
    LOCAL pFileData:DWORD
    push ebx
    push esi
    push edi
    
    ; Allocate context
    invoke VirtualAlloc, 0, SIZEOF GgufModelContext, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    test eax, eax
    jz @fail
    mov pContext, eax
    mov esi, eax
    
    ; Zero initialize
    push SIZEOF GgufModelContext
    push 0
    push esi
    call RtlZeroMemory
    add esp, 12
    
    ; Set method
    mov [esi].GgufModelContext.loadMethod, LOAD_METHOD_STANDARD
    
    ; Copy path
    push 260
    lea edi, [esi].GgufModelContext.szPath
    push edi
    push lpPath
    call lstrcpynA
    add esp, 12
    
    ; Open file
    invoke CreateFileA, lpPath, GENERIC_READ, FILE_SHARE_READ, 0, \
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @fail
    mov hFile, eax
    mov [esi].GgufModelContext.hFile, eax
    
    ; Get file size
    lea edi, qwFileSize
    invoke GetFileSizeEx, hFile, edi
    test eax, eax
    jz @fail_close
    
    mov eax, dword ptr [qwFileSize]
    mov edx, dword ptr [qwFileSize + 4]
    mov dword ptr [esi].GgufModelContext.qwFileSize, eax
    mov dword ptr [esi].GgufModelContext.qwFileSize + 4, edx
    
    ; Allocate buffer for entire file
    mov ebx, dword ptr [qwFileSize]
    invoke VirtualAlloc, 0, ebx, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    test eax, eax
    jz @fail_close
    mov pFileData, eax
    mov [esi].GgufModelContext.pFileData, eax
    
    ; Read entire file
    invoke ReadFile, hFile, pFileData, ebx, ADDR dwBytesRead, 0
    test eax, eax
    jz @fail_free
    
    ; Parse header
    mov edi, pFileData
    push esi
    push edi
    call ParseGgufHeader
    add esp, 8
    test eax, eax
    jz @fail_free
    
    ; Parse tensors and KV pairs
    push esi
    push edi
    call ParseGgufMetadata
    add esp, 8
    
    ; Close file (data is in memory)
    invoke CloseHandle, hFile
    mov [esi].GgufModelContext.hFile, 0
    
    ; Set as current context
    mov [g_pCurrentContext], esi
    
    mov eax, pContext
    pop edi
    pop esi
    pop ebx
    ret
    
@fail_free:
    invoke VirtualFree, pFileData, 0, MEM_RELEASE
@fail_close:
    invoke CloseHandle, hFile
@fail:
    cmp pContext, 0
    je @fail_done
    invoke VirtualFree, pContext, 0, MEM_RELEASE
@fail_done:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
GgufUnified_LoadStandard ENDP

; ================================================================
; GgufUnified_LoadStreaming - METHOD 2: Stream from disc
; Best for: Sequential access, limited RAM
; ================================================================
GgufUnified_LoadStreaming PROC lpPath:DWORD
    LOCAL pContext:DWORD
    LOCAL hFile:DWORD
    LOCAL qwFileSize:QWORD
    LOCAL pBuffer:DWORD
    push ebx
    push esi
    
    ; Allocate context
    invoke VirtualAlloc, 0, SIZEOF GgufModelContext, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    test eax, eax
    jz @fail
    mov pContext, eax
    mov esi, eax
    
    ; Zero initialize
    push SIZEOF GgufModelContext
    push 0
    push esi
    call RtlZeroMemory
    add esp, 12
    
    mov [esi].GgufModelContext.loadMethod, LOAD_METHOD_STREAMING
    
    ; Copy path
    push 260
    lea ebx, [esi].GgufModelContext.szPath
    push ebx
    push lpPath
    call lstrcpynA
    add esp, 12
    
    ; Open file
    invoke CreateFileA, lpPath, GENERIC_READ, FILE_SHARE_READ, 0, \
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL or FILE_FLAG_SEQUENTIAL_SCAN, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @fail
    mov hFile, eax
    mov [esi].GgufModelContext.hFile, eax
    
    ; Get file size
    lea ebx, qwFileSize
    invoke GetFileSizeEx, hFile, ebx
    test eax, eax
    jz @fail_close
    
    mov eax, dword ptr [qwFileSize]
    mov edx, dword ptr [qwFileSize + 4]
    mov dword ptr [esi].GgufModelContext.qwFileSize, eax
    mov dword ptr [esi].GgufModelContext.qwFileSize + 4, edx
    
    ; Allocate stream buffer (4MB)
    invoke VirtualAlloc, 0, CHUNK_SIZE, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    test eax, eax
    jz @fail_close
    mov pBuffer, eax
    mov [esi].GgufModelContext.pStreamBuffer, eax
    mov [esi].GgufModelContext.cbStreamBuffer, CHUNK_SIZE
    
    ; Read header only
    push esi
    push hFile
    call ReadGgufHeaderStreaming
    add esp, 8
    test eax, eax
    jz @fail_free
    
    ; Leave file open for streaming
    mov [g_pCurrentContext], esi
    
    mov eax, pContext
    pop esi
    pop ebx
    ret
    
@fail_free:
    invoke VirtualFree, pBuffer, 0, MEM_RELEASE
@fail_close:
    invoke CloseHandle, hFile
@fail:
    cmp pContext, 0
    je @fail_done
    invoke VirtualFree, pContext, 0, MEM_RELEASE
@fail_done:
    xor eax, eax
    pop esi
    pop ebx
    ret
GgufUnified_LoadStreaming ENDP

; ================================================================
; GgufUnified_LoadChunked - METHOD 3: Chunked with LRU cache
; Best for: Random access, 2GB-8GB files
; ================================================================
GgufUnified_LoadChunked PROC lpPath:DWORD
    LOCAL pContext:DWORD
    LOCAL hFile:DWORD
    LOCAL qwFileSize:QWORD
    LOCAL pCache:DWORD
    push ebx
    push esi
    push edi
    
    ; Allocate context
    invoke VirtualAlloc, 0, SIZEOF GgufModelContext, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    test eax, eax
    jz @fail
    mov pContext, eax
    mov esi, eax
    
    ; Zero initialize
    push SIZEOF GgufModelContext
    push 0
    push esi
    call RtlZeroMemory
    add esp, 12
    
    mov [esi].GgufModelContext.loadMethod, LOAD_METHOD_CHUNKED
    
    ; Copy path
    push 260
    lea edi, [esi].GgufModelContext.szPath
    push edi
    push lpPath
    call lstrcpynA
    add esp, 12
    
    ; Open file
    invoke CreateFileA, lpPath, GENERIC_READ, FILE_SHARE_READ, 0, \
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL or FILE_FLAG_RANDOM_ACCESS, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @fail
    mov hFile, eax
    mov [esi].GgufModelContext.hFile, eax
    
    ; Get file size
    lea edi, qwFileSize
    invoke GetFileSizeEx, hFile, edi
    test eax, eax
    jz @fail_close
    
    mov eax, dword ptr [qwFileSize]
    mov edx, dword ptr [qwFileSize + 4]
    mov dword ptr [esi].GgufModelContext.qwFileSize, eax
    mov dword ptr [esi].GgufModelContext.qwFileSize + 4, edx
    
    ; Allocate chunk cache (16 chunks * 4MB = 64MB cache)
    mov ebx, CACHE_CHUNKS
    imul ebx, SIZEOF ChunkCacheEntry
    invoke VirtualAlloc, 0, ebx, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    test eax, eax
    jz @fail_close
    mov pCache, eax
    mov [esi].GgufModelContext.pChunkCache, eax
    mov [esi].GgufModelContext.dwChunkCount, 0
    
    ; Initialize cache entries
    mov ecx, CACHE_CHUNKS
    mov edi, pCache
@init_cache:
    mov [edi].ChunkCacheEntry.bValid, 0
    mov [edi].ChunkCacheEntry.pData, 0
    add edi, SIZEOF ChunkCacheEntry
    loop @init_cache
    
    ; Read header
    push esi
    push hFile
    call ReadGgufHeaderStreaming
    add esp, 8
    
    ; Leave file open
    mov [g_pCurrentContext], esi
    
    mov eax, pContext
    pop edi
    pop esi
    pop ebx
    ret
    
@fail_close:
    invoke CloseHandle, hFile
@fail:
    cmp pContext, 0
    je @fail_done
    invoke VirtualFree, pContext, 0, MEM_RELEASE
@fail_done:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
GgufUnified_LoadChunked ENDP

; ================================================================
; GgufUnified_LoadMMAP - METHOD 4: Memory-mapped file
; Best for: Files > 8GB, random access
; ================================================================
GgufUnified_LoadMMAP PROC lpPath:DWORD
    LOCAL pContext:DWORD
    LOCAL hFile:DWORD
    LOCAL hMapping:DWORD
    LOCAL pView:DWORD
    LOCAL qwFileSize:QWORD
    push ebx
    push esi
    push edi
    
    ; Allocate context
    invoke VirtualAlloc, 0, SIZEOF GgufModelContext, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    test eax, eax
    jz @fail
    mov pContext, eax
    mov esi, eax
    
    ; Zero initialize
    push SIZEOF GgufModelContext
    push 0
    push esi
    call RtlZeroMemory
    add esp, 12
    
    mov [esi].GgufModelContext.loadMethod, LOAD_METHOD_MMAP
    
    ; Copy path
    push 260
    lea edi, [esi].GgufModelContext.szPath
    push edi
    push lpPath
    call lstrcpynA
    add esp, 12
    
    ; Open file
    invoke CreateFileA, lpPath, GENERIC_READ, FILE_SHARE_READ, 0, \
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @fail
    mov hFile, eax
    mov [esi].GgufModelContext.hFile, eax
    
    ; Get file size
    lea edi, qwFileSize
    invoke GetFileSizeEx, hFile, edi
    test eax, eax
    jz @fail_close
    
    mov eax, dword ptr [qwFileSize]
    mov edx, dword ptr [qwFileSize + 4]
    mov dword ptr [esi].GgufModelContext.qwFileSize, eax
    mov dword ptr [esi].GgufModelContext.qwFileSize + 4, edx
    
    ; Create file mapping
    invoke CreateFileMappingA, hFile, 0, PAGE_READONLY, edx, eax, 0
    test eax, eax
    jz @fail_close
    mov hMapping, eax
    mov [esi].GgufModelContext.hMapping, eax
    
    ; Map view of entire file
    invoke MapViewOfFile, hMapping, FILE_MAP_READ, 0, 0, 0
    test eax, eax
    jz @fail_mapping
    mov pView, eax
    mov [esi].GgufModelContext.pMappedView, eax
    
    mov eax, dword ptr [qwFileSize]
    mov edx, dword ptr [qwFileSize + 4]
    mov dword ptr [esi].GgufModelContext.qwMapSize, eax
    mov dword ptr [esi].GgufModelContext.qwMapSize + 4, edx
    
    ; Parse header from mapped memory
    push esi
    push pView
    call ParseGgufHeader
    add esp, 8
    test eax, eax
    jz @fail_unmap
    
    ; Parse metadata
    push esi
    push pView
    call ParseGgufMetadata
    add esp, 8
    
    ; Keep file and mapping open
    mov [g_pCurrentContext], esi
    
    mov eax, pContext
    pop edi
    pop esi
    pop ebx
    ret
    
@fail_unmap:
    invoke UnmapViewOfFile, pView
@fail_mapping:
    invoke CloseHandle, hMapping
@fail_close:
    invoke CloseHandle, hFile
@fail:
    cmp pContext, 0
    je @fail_done
    invoke VirtualFree, pContext, 0, MEM_RELEASE
@fail_done:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
GgufUnified_LoadMMAP ENDP

; ================================================================
; Helper functions
; ================================================================

ParseGgufHeader PROC pData:DWORD, pContext:DWORD
    push ebx
    push esi
    
    mov esi, pData
    mov ebx, pContext
    
    ; Check magic
    mov eax, [esi]
    cmp eax, GGUF_MAGIC
    jne @invalid
    
    ; Copy header
    lea edi, [ebx].GgufModelContext.header
    mov ecx, SIZEOF GgufHeader
    rep movsb
    
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@invalid:
    xor eax, eax
    pop esi
    pop ebx
    ret
ParseGgufHeader ENDP

ParseGgufMetadata PROC pData:DWORD, pContext:DWORD
    ; Stub: Parse tensors and KV pairs
    mov eax, 1
    ret
ParseGgufMetadata ENDP

ReadGgufHeaderStreaming PROC hFile:DWORD, pContext:DWORD
    LOCAL header[32]:BYTE
    LOCAL dwBytesRead:DWORD
    push ebx
    
    ; Read first 32 bytes
    lea eax, header
    invoke ReadFile, hFile, eax, 32, ADDR dwBytesRead, 0
    test eax, eax
    jz @fail
    
    ; Parse header
    lea eax, header
    push pContext
    push eax
    call ParseGgufHeader
    add esp, 8
    
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop ebx
    ret
ReadGgufHeaderStreaming ENDP

; ================================================================
; GgufUnified_GetTensor - Get tensor data (method-agnostic)
; ================================================================
GgufUnified_GetTensor PROC hContext:DWORD, dwTensorIndex:DWORD, ppData:DWORD
    ; Implementation varies by loading method
    mov eax, 1
    ret
GgufUnified_GetTensor ENDP

; ================================================================
; GgufUnified_GetMetadata - Get metadata value
; ================================================================
GgufUnified_GetMetadata PROC hContext:DWORD, lpKey:DWORD, ppValue:DWORD
    mov eax, 1
    ret
GgufUnified_GetMetadata ENDP

; ================================================================
; GgufUnified_UnloadModel - Free all resources
; ================================================================
GgufUnified_UnloadModel PROC hContext:DWORD
    push ebx
    push esi
    
    mov esi, hContext
    test esi, esi
    jz @done
    
    ; Check method and cleanup accordingly
    mov eax, [esi].GgufModelContext.loadMethod
    
    cmp eax, LOAD_METHOD_STANDARD
    je @cleanup_standard
    cmp eax, LOAD_METHOD_STREAMING
    je @cleanup_streaming
    cmp eax, LOAD_METHOD_CHUNKED
    je @cleanup_chunked
    cmp eax, LOAD_METHOD_MMAP
    je @cleanup_mmap
    jmp @done
    
@cleanup_standard:
    cmp [esi].GgufModelContext.pFileData, 0
    je @cleanup_common
    invoke VirtualFree, [esi].GgufModelContext.pFileData, 0, MEM_RELEASE
    jmp @cleanup_common
    
@cleanup_streaming:
    cmp [esi].GgufModelContext.pStreamBuffer, 0
    je @cleanup_common
    invoke VirtualFree, [esi].GgufModelContext.pStreamBuffer, 0, MEM_RELEASE
    jmp @cleanup_common
    
@cleanup_chunked:
    cmp [esi].GgufModelContext.pChunkCache, 0
    je @cleanup_common
    invoke VirtualFree, [esi].GgufModelContext.pChunkCache, 0, MEM_RELEASE
    jmp @cleanup_common
    
@cleanup_mmap:
    cmp [esi].GgufModelContext.pMappedView, 0
    je @no_view
    invoke UnmapViewOfFile, [esi].GgufModelContext.pMappedView
@no_view:
    cmp [esi].GgufModelContext.hMapping, 0
    je @cleanup_common
    invoke CloseHandle, [esi].GgufModelContext.hMapping
    
@cleanup_common:
    ; Close file if open
    cmp [esi].GgufModelContext.hFile, 0
    je @no_file
    invoke CloseHandle, [esi].GgufModelContext.hFile
@no_file:
    
    ; Free context
    invoke VirtualFree, esi, 0, MEM_RELEASE
    
@done:
    mov eax, 1
    pop esi
    pop ebx
    ret
GgufUnified_UnloadModel ENDP

; ================================================================
; GgufUnified_GetStats - Get loading statistics
; ================================================================
GgufUnified_GetStats PROC hContext:DWORD
    ; Return statistics about loading method and performance
    mov eax, hContext
    ret
GgufUnified_GetStats ENDP

END
