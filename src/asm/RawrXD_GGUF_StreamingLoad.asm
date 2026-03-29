; RawrXD_GGUF_StreamingLoad.asm
; Fixes ERROR_NOT_ENOUGH_MEMORY by using chunked mapping

INCLUDE ksamd64.inc
INCLUDE rawrxd_win64.inc

;=============================================================================
; DATA SECTION
;=============================================================================
.DATA
GGUF_FLAG_FORCE_CHUNKED     EQU 0x0001
FILE_FLAG_SEQUENTIAL_SCAN   EQU 0x08000000
GENERIC_READ                EQU 0x80000000
OPEN_EXISTING               EQU 3
FILE_MAP_READ               EQU 4
PAGE_READONLY               EQU 2
PAGE_READWRITE              EQU 4

GGUFStreamingContext STRUCT 128
    hFile           DQ ?
    hMap            DQ ?
    fileSize        DQ ?
    mapGranularity  DQ ?
    headerView      DQ ?
    tensorCount     DD ?
GGUFStreamingContext ENDS

TensorInfo STRUCT 32
    fileOffset      DQ ?
    dataSize        DQ ?
TensorInfo ENDS

.CODE

;=============================================================================
; STUBS
;=============================================================================
GGUF_ParseHeader_Streaming PROC
    mov eax, 1
    ret
GGUF_ParseHeader_Streaming ENDP

GGUF_BuildTensorIndex_Streaming PROC
    ret
GGUF_BuildTensorIndex_Streaming ENDP

GGUF_LookupTensorInfo PROC
    ret
GGUF_LookupTensorInfo ENDP

GGUF_CheckTensorCache PROC
    xor eax, eax
    ret
GGUF_CheckTensorCache ENDP

GGUF_EvictOldestTensor PROC
    ret
GGUF_EvictOldestTensor ENDP

GGUF_AddToTensorCache PROC
    ret
GGUF_AddToTensorCache ENDP

;=============================================================================
; GGUF_LoadModel_Streaming
; Loads large GGUF without full memory map - uses streaming chunks
;=============================================================================
GGUF_LoadModel_Streaming PROC EXPORT FRAME
    ; rcx = filePath (wchar_t*), rdx = flags
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    .allocstack 40
    .endprolog
    
    mov r12, rcx                    ; filePath
    mov r13d, edx                   ; flags
    
    ; Open file with sequential scan hint (tells OS we're streaming)
    mov rcx, r12
    mov edx, GENERIC_READ
    xor r8d, r8d                    ; no share mode needed
    xor r9d, r9d                    ; security
    push 0                          ; hTemplateFile
    push FILE_FLAG_SEQUENTIAL_SCAN  ; flags (HINT: we're streaming)
    push OPEN_EXISTING              ; creation
    push 0                          ; lpSecurityAttributes
    sub rsp, 32
    call CreateFileW
    add rsp, 56
    cmp rax, -1                     ; INVALID_HANDLE_VALUE
    je @@error_file
    
    mov rbx, rax                    ; rbx = hFile
    
    ; Get file size
    lea rdx, [rsp+8]
    mov rcx, rbx
    call GetFileSizeEx
    test eax, eax
    jz @@error_handle
    
    mov rsi, [rsp+8]                ; rsi = file size (low)
    mov rdi, [rsp+16]               ; rdi = file size (high) - ignore for now
    
    ; For files > 2GB or when RAM is constrained, use chunked mapping
    cmp rsi, 2 * 1024 * 1024 * 1024 ; 2GB threshold
    ja @@chunked_load
    cmp r13d, GGUF_FLAG_FORCE_CHUNKED
    je @@chunked_load
    
    ; Small file: Standard memory map
    jmp @@standard_map
    
@@chunked_load:
    ; === CHUNKED LOADING for large models ===
    
    ; Create file mapping (reserved, not committed)
    mov rcx, rbx                    ; hFile
    xor edx, edx                    ; security
    mov r8d, 2                      ; protect = PAGE_READONLY
    mov r9, rsi                     ; max size = file size
    call CreateFileMappingW
    test rax, rax
    jz @@error_handle
    mov rdi, rax                    ; rdi = hMap
    
    ; Allocate streaming context
    mov ecx, SIZEOF GGUFStreamingContext
    call malloc
    test rax, rax
    jz @@error_map
    mov r12, rax
    
    ; Initialize streaming state
    mov [r12].GGUFStreamingContext.hFile, rbx
    mov [r12].GGUFStreamingContext.hMap, rdi
    mov [r12].GGUFStreamingContext.fileSize, rsi
    mov [r12].GGUFStreamingContext.mapGranularity, 65536 ; 64KB granularity
    
    ; Map ONLY the header (first 1MB) to read metadata
    xor ecx, ecx                    ; lpAddress (system chooses)
    mov edx, 1024 * 1024            ; dwBytes = 1MB header
    xor r8d, r8d                    ; dwFlags = 0
    mov r9d, FILE_MAP_READ          ; dwDesiredAccess
    push 0                          ; file offset high
    push 0                          ; file offset low
    push rdi                        ; hFileMappingObject
    sub rsp, 32
    call MapViewOfFile
    add rsp, 56
    test rax, rax
    jz @@error_stream_ctx
    
    mov [r12].GGUFStreamingContext.headerView, rax
    
    ; Parse header, tensor count, etc.
    mov rcx, [r12].GGUFStreamingContext.headerView
    call GGUF_ParseHeader_Streaming
    mov [r12].GGUFStreamingContext.tensorCount, eax
    
    ; Unmap header (we'll remap tensors on-demand)
    mov rcx, [r12].GGUFStreamingContext.headerView
    call UnmapViewOfFile
    ; Set up tensor index (in-memory directory of tensor locations)
    mov rcx, r12                   ; rcx = GGUFStreamingContext*

    call GGUF_BuildTensorIndex_Streaming
    
    ; Return streaming context (not a full map)
    mov rax, r12
    jmp @@done_chunked
    
@@standard_map:
    ; Traditional full mapping for small files
    ; ... (existing code)
    
@@error_stream_ctx:
    mov rcx, r12
    call free
    
@@error_map:
    mov rcx, rdi
    call CloseHandle
    
@@error_handle:
    mov rcx, rbx
    call CloseHandle
    
@@error_file:
    xor eax, eax
    
@@done_chunked:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
GGUF_LoadModel_Streaming ENDP

;=============================================================================
; GGUF_GetTensor_Streaming
; Maps tensor on-demand, unmaps when evicted
;=============================================================================
GGUF_GetTensor_Streaming PROC EXPORT FRAME
    ; rcx = GGUFStreamingContext*, rdx = tensorIndex, r8 = outputBuffer
    
    push rbx
    push rsi
    push rdi
    .allocstack 24
    .endprolog
    
    mov rbx, rcx
    mov esi, edx
    
    ; Get tensor info from index
    mov rcx, rbx
    mov edx, esi
    call GGUF_LookupTensorInfo        ; Returns: rax = info ptr
    
    ; Calculate file offset for this tensor
    mov rdi, [rax].TensorInfo.fileOffset
    
    ; Round down to granularity boundary
    mov r8, rdi
    and r8, NOT 65535                 ; Align to 64KB
    
    ; Calculate view size (tensor size + offset adjustment)
    mov r9, [rax].TensorInfo.dataSize
    add r9, rdi
    sub r9, r8                        ; + alignment padding
    add r9, 65535
    and r9, NOT 65535                 ; Round up to granularity
    
    ; Check cache - is this tensor already mapped?
    mov rcx, rbx
    mov edx, esi
    call GGUF_CheckTensorCache
    test rax, rax
    jnz @@cached                      ; Already in cache
    
    ; Evict oldest cached tensor if cache is full
    mov rcx, rbx
    call GGUF_EvictOldestTensor
    
    ; Map this tensor's region
    ; r8 contains aligned file offset; preserve for offset low push
    mov r10, r8                       ; save aligned file offset
    mov rdx, r9                       ; dwBytes
    xor r8d, r8d                      ; dwFlags
    mov r9d, FILE_MAP_READ
    push 0                            ; offset high
    push r10d                         ; offset low (from saved r8)
    push [rbx].GGUFStreamingContext.hMap
    sub rsp, 32
    call MapViewOfFile
    add rsp, 56
    test rax, rax
    jz @@error
    
    ; Add to cache
    mov rcx, rbx
    mov edx, esi
    mov r8, rax                       ; view base
    mov r9, r9                        ; view size
    call GGUF_AddToTensorCache
    
    ; Calculate actual tensor data pointer (view base + offset adjustment)
    mov rdx, rdi
    sub rdx, r8                       ; offset within view
    add rax, rdx                      ; rax = tensor data ptr
    
@@cached:
@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
    
@@error:
    xor eax, eax
    jmp @@done
GGUF_GetTensor_Streaming ENDP

END