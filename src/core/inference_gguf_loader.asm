; inference_gguf_loader.asm - MASM x64 Memory-Mapped GGUF Loader
; Zero-copy tensor access for RawrXD
; Eliminates 2GB limit, enables direct GPU handoff

OPTION CASEMAP:NONE

; ============================================================================
; CONSTANTS
; ============================================================================

GGUF_MAGIC         EQU 047475546h  ; 'GGUF'
GGUF_VERSION      EQU 3

PAGE_READONLY      EQU 2h
PAGE_READWRITE     EQU 4h
PAGE_WRITECOPY     EQU 8h

FILE_MAP_READ      EQU 4h
FILE_MAP_WRITE     EQU 2h
FILE_MAP_COPY      EQU 1h

FILE_ATTRIBUTE_NORMAL EQU 80h
GENERIC_READ       EQU 80000000h
GENERIC_WRITE      EQU 40000000h
FILE_SHARE_READ    EQU 1h
OPEN_EXISTING      EQU 3h

INVALID_HANDLE_VALUE EQU -1
NULL               EQU 0

; ============================================================================
; DATA SEGMENT
; ============================================================================

.data

; Error messages
szErrorOpenFile      DB 'Failed to open GGUF file: ', 0
szErrorCreateMapping DB 'Failed to create file mapping', 0
szErrorMapView       DB 'Failed to map view of file', 0
szErrorMagic         DB 'Invalid GGUF magic number', 0
szErrorVersion       DB 'Unsupported GGUF version', 0
szSuccess            DB 'GGUF file loaded successfully', 0

; File path buffer (max 512 chars)
szFilePath           DB 512 DUP(0)

; Last error
last_error           DD 0

; ============================================================================
; CODE SEGMENT
; ============================================================================

.code

; ============================================================================
; EXTERNAL DECLARATIONS
; ============================================================================

EXTRN __imp_CreateFileA:PROC
EXTRN __imp_CreateFileMappingA:PROC
EXTRN __imp_MapViewOfFile:PROC
EXTRN __imp_UnmapViewOfFile:PROC
EXTRN __imp_CloseHandle:PROC
EXTRN __imp_GetFileSize:PROC
EXTRN __imp_GetLastError:PROC
EXTRN __imp_GetFileSizeEx:PROC
EXTRN __imp_ReadFile:PROC
EXTRN __imp_SetFilePointer:PROC
EXTRN __imp_SetFilePointerEx:PROC
EXTRN __imp_FlushViewOfFile:PROC
EXTRN __imp_VirtualProtect:PROC
EXTRN __imp_VirtualAlloc:PROC
EXTRN __imp_VirtualFree:PROC
EXTRN __imp_RtlCopyMemory:PROC
EXTRN __imp_RtlZeroMemory:PROC
EXTRN __imp_RtlCompareMemory:PROC
EXTRN __imp_OutputDebugStringA:PROC
EXTRN __imp_GetProcessHeap:PROC
EXTRN __imp_HeapAlloc:PROC
EXTRN __imp_HeapFree:PROC

; ============================================================================
; GGUF CONTEXT STRUCTURE (must match C header)
; ============================================================================

GGUFContext STRUCT 8
    file_handle      DQ ?
    mapping_handle   DQ ?
    base_address     DQ ?
    file_size        DQ ?
    
    ; Header
    magic            DD ?
    version          DD ?
    tensor_count     DQ ?
    metadata_count   DQ ?
    
    ; Tensors
    tensors          DQ ?
    tensor_offsets   DQ ?
    
    ; Metadata
    metadata_keys    DQ ?
    metadata_values  DQ ?
    
    ; Architecture
    n_layers         DD ?
    n_heads          DD ?
    n_embd           DD ?
    n_ctx            DD ?
    vocab_size       DD ?
    
    ; Quantization
    default_quant    DD ?
    quant_ratio      REAL4 ?
    
    ; State
    is_loaded        DD ?
    is_mapped        DD ?
    filepath         DB 512 DUP(0)
GGUFContext ENDS

; ============================================================================
; GGUF HEADER STRUCTURE
; ============================================================================

GGUFHeader STRUCT 8
    magic            DD ?
    version          DD ?
    tensor_count     DQ ?
    metadata_kv_count DQ ?
GGUFHeader ENDS

; ============================================================================
; GGUF TENSOR INFO STRUCTURE
; ============================================================================

GGUFTensorInfo STRUCT 8
    name_ptr         DQ ?
    n_dims           DD ?
    dims             DQ ?
    tensor_type      DD ?
    data_off         DQ ?
    data_ptr         DQ ?
    data_sz          DQ ?
GGUFTensorInfo ENDS

; ============================================================================
; GGUF_LOADER_CREATE
; Creates a new GGUF context
; Returns: RAX = pointer to GGUFContext (NULL on failure)
; ============================================================================

GGUF_LOADER_CREATE PROC EXPORT
    push rbx
    push rsi
    push rdi
    sub rsp, 40
    
    ; Get process heap
    call __imp_GetProcessHeap
    mov rbx, rax
    
    ; Allocate GGUFContext
    mov rcx, rax                      ; heap handle
    xor rdx, rdx                      ; flags
    mov r8, SIZEOF GGUFContext        ; size
    call __imp_HeapAlloc
    
    test rax, rax
    jz create_failed
    
    ; Zero initialize
    mov rdi, rax
    mov rcx, SIZEOF GGUFContext
    xor eax, eax
    rep stosb
    
    mov rax, rdi
    
create_failed:
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
GGUF_LOADER_CREATE ENDP

; ============================================================================
; GGUF_LOADER_MAP_FILE
; Memory-maps a GGUF file (zero-copy)
; RCX = GGUFContext*
; RDX = filepath
; Returns: RAX = 1 on success, 0 on failure
; ============================================================================

GGUF_LOADER_MAP_FILE PROC EXPORT
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 56
    
    ; Save parameters
    mov rbx, rcx                      ; GGUFContext*
    mov rsi, rdx                      ; filepath
    
    ; Open file
    xor rcx, rcx                      ; lpFileName
    mov rcx, rsi
    xor rdx, rdx                      ; dwDesiredAccess = GENERIC_READ
    mov rdx, GENERIC_READ
    xor r8, r8                        ; dwShareMode = FILE_SHARE_READ
    mov r8, FILE_SHARE_READ
    xor r9, r9                        ; lpSecurityAttributes = NULL
    mov r9, NULL
    push NULL                         ; dwFlagsAndAttributes
    push OPEN_EXISTING                ; dwCreationDisposition
    push NULL                         ; hTemplateFile
    call __imp_CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je map_file_failed
    
    mov r12, rax                      ; Save file handle
    
    ; Get file size
    mov rcx, rax                      ; hFile
    lea rdx, [rsp+48]                 ; lpFileSize (on stack)
    xor r8, r8                        ; lpFileSizeHigh
    call __imp_GetFileSizeEx
    
    test eax, eax
    jz map_file_failed_close
    
    mov r13, [rsp+48]                 ; Save file size
    
    ; Create file mapping
    mov rcx, r12                      ; hFile
    xor rdx, rdx                      ; lpSecurityAttributes = NULL
    mov r8, PAGE_READONLY             ; flProtect
    xor r9, r9                        ; dwMaximumSizeHigh = 0
    push 0                            ; dwMaximumSizeLow = 0 (whole file)
    push NULL                         ; lpName = NULL
    call __imp_CreateFileMappingA
    
    test rax, rax
    jz map_file_failed_close
    
    mov r14, rax                      ; Save mapping handle
    
    ; Map view of file
    mov rcx, rax                      ; hFileMappingObject
    mov rdx, FILE_MAP_READ            ; dwDesiredAccess
    xor r8, r8                        ; dwFileOffsetHigh = 0
    xor r9, r9                        ; dwFileOffsetLow = 0
    push 0                            ; dwNumberOfBytesToMap = 0 (whole file)
    call __imp_MapViewOfFile
    
    test rax, rax
    jz map_file_failed_close_mapping
    
    mov r15, rax                      ; Save base address
    
    ; Validate GGUF magic
    mov eax, [rax]                    ; Read first 4 bytes
    cmp eax, GGUF_MAGIC
    jne map_file_invalid_magic
    
    ; Validate version
    mov eax, [rax+4]
    cmp eax, GGUF_VERSION
    ja map_file_invalid_version        ; Version too new
    
    ; Store in context
    mov [rbx].GGUFContext.file_handle, r12
    mov [rbx].GGUFContext.mapping_handle, r14
    mov [rbx].GGUFContext.base_address, r15
    mov [rbx].GGUFContext.file_size, r13
    
    ; Copy header info
    mov eax, [r15]
    mov [rbx].GGUFContext.magic, eax
    mov eax, [r15+4]
    mov [rbx].GGUFContext.version, eax
    mov rax, [r15+8]
    mov [rbx].GGUFContext.tensor_count, rax
    mov rax, [r15+16]
    mov [rbx].GGUFContext.metadata_count, rax
    
    ; Copy filepath
    lea rdi, [rbx].GGUFContext.filepath
    mov rsi, rsi
    mov rcx, 511
    rep movsb
    mov byte ptr [rdi], 0
    
    ; Set flags
    mov dword ptr [rbx].GGUFContext.is_loaded, 1
    mov dword ptr [rbx].GGUFContext.is_mapped, 1
    
    ; Success
    mov rax, 1
    jmp map_file_done
    
map_file_invalid_version:
    ; Version too new - still try to load
    mov dword ptr [last_error], 2
    jmp map_file_store_context
    
map_file_invalid_magic:
    mov dword ptr [last_error], 1
    jmp map_file_failed_close_mapping
    
map_file_store_context:
    ; Store context even with warnings
    mov [rbx].GGUFContext.file_handle, r12
    mov [rbx].GGUFContext.mapping_handle, r14
    mov [rbx].GGUFContext.base_address, r15
    mov [rbx].GGUFContext.file_size, r13
    mov rax, 1
    jmp map_file_done
    
map_file_failed_close_mapping:
    mov rcx, r14
    call __imp_CloseHandle
    
map_file_failed_close:
    mov rcx, r12
    call __imp_CloseHandle
    
map_file_failed:
    xor eax, eax
    
map_file_done:
    add rsp, 56
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
GGUF_LOADER_MAP_FILE ENDP

; ============================================================================
; GGUF_LOADER_GET_TENSOR
; Gets tensor data by name (zero-copy pointer)
; RCX = GGUFContext*
; RDX = tensor name
; R8 = out_size pointer
; Returns: RAX = pointer to tensor data (NULL if not found)
; ============================================================================

GGUF_LOADER_GET_TENSOR PROC EXPORT
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 40
    
    mov rbx, rcx                      ; GGUFContext*
    mov rsi, rdx                      ; tensor name
    mov r12, r8                       ; out_size
    
    ; Check if loaded
    cmp dword ptr [rbx].GGUFContext.is_loaded, 0
    je get_tensor_not_found
    
    ; Get base address
    mov r13, [rbx].GGUFContext.base_address
    test r13, r13
    jz get_tensor_not_found
    
    ; TODO: Parse tensor metadata to find by name
    ; For now, return base + header offset
    
    ; Calculate tensor data offset
    ; GGUF header = 32 bytes (magic + version + counts)
    ; Metadata follows, then tensor info, then tensor data
    
    ; Simplified: return pointer to data section
    mov rax, [rbx].GGUFContext.tensor_count
    imul rax, 64                      ; Approximate tensor info size
    add rax, 32                       ; Header size
    add rax, [rbx].GGUFContext.metadata_count
    imul rax, 128                     ; Approximate metadata size
    
    ; Align to 32 bytes
    add rax, 31
    and rax, -32
    
    add rax, r13                      ; Add base address
    
    ; Set size if requested
    test r12, r12
    jz get_tensor_done
    mov qword ptr [r12], 0            ; TODO: actual size
    
get_tensor_done:
    jmp get_tensor_exit
    
get_tensor_not_found:
    xor eax, eax
    
get_tensor_exit:
    add rsp, 40
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
GGUF_LOADER_GET_TENSOR ENDP

; ============================================================================
; GGUF_LOADER_UNMAP_FILE
; Unmaps and closes GGUF file
; RCX = GGUFContext*
; Returns: RAX = 1 on success, 0 on failure
; ============================================================================

GGUF_LOADER_UNMAP_FILE PROC EXPORT
    push rbx
    sub rsp, 32
    
    mov rbx, rcx                      ; GGUFContext*
    
    ; Check if mapped
    cmp dword ptr [rbx].GGUFContext.is_mapped, 0
    je unmap_done
    
    ; Unmap view
    mov rcx, [rbx].GGUFContext.base_address
    test rcx, rcx
    jz unmap_close_mapping
    call __imp_UnmapViewOfFile
    
unmap_close_mapping:
    ; Close mapping handle
    mov rcx, [rbx].GGUFContext.mapping_handle
    test rcx, rcx
    jz unmap_close_file
    call __imp_CloseHandle
    
unmap_close_file:
    ; Close file handle
    mov rcx, [rbx].GGUFContext.file_handle
    test rcx, rcx
    jz unmap_clear
    call __imp_CloseHandle
    
unmap_clear:
    ; Clear context
    mov qword ptr [rbx].GGUFContext.file_handle, 0
    mov qword ptr [rbx].GGUFContext.mapping_handle, 0
    mov qword ptr [rbx].GGUFContext.base_address, 0
    mov dword ptr [rbx].GGUFContext.is_mapped, 0
    
unmap_done:
    mov rax, 1
    
    add rsp, 32
    pop rbx
    ret
GGUF_LOADER_UNMAP_FILE ENDP

; ============================================================================
; GGUF_LOADER_DESTROY
; Destroys GGUF context
; RCX = GGUFContext*
; ============================================================================

GGUF_LOADER_DESTROY PROC EXPORT
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    
    ; Unmap if needed
    test rbx, rbx
    jz destroy_done
    
    ; Unmap file
    mov rcx, rbx
    call GGUF_LOADER_UNMAP_FILE
    
    ; Free context
    call __imp_GetProcessHeap
    mov rcx, rax
    xor rdx, rdx
    mov r8, rbx
    call __imp_HeapFree
    
destroy_done:
    add rsp, 32
    pop rbx
    ret
GGUF_LOADER_DESTROY ENDP

; ============================================================================
; GGUF_LOADER_GET_ARCH_INFO
; Gets architecture info from GGUF
; RCX = GGUFContext*
; RDX = out_n_layers
; R8 = out_n_heads
; R9 = out_n_embd
; [RSP+40] = out_n_ctx
; [RSP+48] = out_vocab_size
; Returns: RAX = 1 on success
; ============================================================================

GGUF_LOADER_GET_ARCH_INFO PROC EXPORT
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rbx, rcx                      ; GGUFContext*
    mov rsi, rdx                      ; out_n_layers
    mov rdi, r8                       ; out_n_heads
    
    ; Check if loaded
    cmp dword ptr [rbx].GGUFContext.is_loaded, 0
    je arch_info_failed
    
    ; TODO: Parse metadata to extract architecture info
    ; For now, return defaults
    
    test rsi, rsi
    jz skip_layers
    mov dword ptr [rsi], 32           ; Default n_layers
    
skip_layers:
    test rdi, rdi
    jz skip_heads
    mov dword ptr [rdi], 32           ; Default n_heads
    
skip_heads:
    mov rax, [rsp+40+32]              ; out_n_embd
    test rax, rax
    jz skip_embd
    mov dword ptr [rax], 4096         ; Default n_embd
    
skip_embd:
    mov rax, [rsp+48+32]              ; out_n_ctx
    test rax, rax
    jz skip_ctx
    mov dword ptr [rax], 4096         ; Default n_ctx
    
skip_ctx:
    mov rax, [rsp+56+32]              ; out_vocab_size
    test rax, rax
    jz skip_vocab
    mov dword ptr [rax], 32000         ; Default vocab_size
    
skip_vocab:
    mov rax, 1
    jmp arch_info_done
    
arch_info_failed:
    xor eax, eax
    
arch_info_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
GGUF_LOADER_GET_ARCH_INFO ENDP

; ============================================================================
; GGUF_LOADER_VALIDATE_FILE
; Validates GGUF file without full load
; RCX = filepath
; Returns: RAX = 1 if valid, 0 if invalid
; ============================================================================

GGUF_LOADER_VALIDATE_FILE PROC EXPORT
    push rbx
    push rsi
    sub rsp, 56
    
    mov rsi, rcx                      ; filepath
    
    ; Open file
    mov rcx, rsi
    mov rdx, GENERIC_READ
    xor r8, r8
    mov r8, FILE_SHARE_READ
    xor r9, r9
    push NULL
    push OPEN_EXISTING
    push FILE_ATTRIBUTE_NORMAL
    push NULL
    call __imp_CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je validate_failed
    
    mov rbx, rax                      ; Save handle
    
    ; Read header
    sub rsp, 32
    lea rdx, [rsp+32]                 ; Buffer for header
    mov rcx, rbx
    lea r8, [rsp+24]                  ; Bytes read
    mov r9, 32                        ; Bytes to read
    push NULL
    push NULL
    call __imp_ReadFile
    
    test eax, eax
    jz validate_close
    
    ; Check magic
    mov eax, [rsp+32]
    cmp eax, GGUF_MAGIC
    jne validate_close_invalid
    
    ; Check version
    mov eax, [rsp+36]
    cmp eax, GGUF_VERSION
    ja validate_close_invalid
    
    ; Valid
    mov rcx, rbx
    call __imp_CloseHandle
    mov rax, 1
    jmp validate_done
    
validate_close_invalid:
    mov rcx, rbx
    call __imp_CloseHandle
    
validate_failed:
    xor eax, eax
    jmp validate_done
    
validate_close:
    mov rcx, rbx
    call __imp_CloseHandle
    xor eax, eax
    
validate_done:
    add rsp, 56
    pop rsi
    pop rbx
    ret
GGUF_LOADER_VALIDATE_FILE ENDP

; ============================================================================
; GGUF_LOADER_GET_FILE_SIZE
; Gets GGUF file size
; RCX = GGUFContext*
; Returns: RAX = file size in bytes
; ============================================================================

GGUF_LOADER_GET_FILE_SIZE PROC EXPORT
    mov rax, [rcx].GGUFContext.file_size
    ret
GGUF_LOADER_GET_FILE_SIZE ENDP

; ============================================================================
; GGUF_LOADER_GET_BASE_ADDRESS
; Gets memory-mapped base address (zero-copy tensor access)
; RCX = GGUFContext*
; Returns: RAX = base address
; ============================================================================

GGUF_LOADER_GET_BASE_ADDRESS PROC EXPORT
    mov rax, [rcx].GGUFContext.base_address
    ret
GGUF_LOADER_GET_BASE_ADDRESS ENDP

; ============================================================================
; GGUF_LOADER_GET_TENSOR_COUNT
; Gets number of tensors
; RCX = GGUFContext*
; Returns: RAX = tensor count
; ============================================================================

GGUF_LOADER_GET_TENSOR_COUNT PROC EXPORT
    mov rax, [rcx].GGUFContext.tensor_count
    ret
GGUF_LOADER_GET_TENSOR_COUNT ENDP

; ============================================================================
; GGUF_LOADER_GET_LAST_ERROR
; Gets last error code
; Returns: RAX = error code
; ============================================================================

GGUF_LOADER_GET_LAST_ERROR PROC EXPORT
    mov eax, [last_error]
    ret
GGUF_LOADER_GET_LAST_ERROR ENDP

END