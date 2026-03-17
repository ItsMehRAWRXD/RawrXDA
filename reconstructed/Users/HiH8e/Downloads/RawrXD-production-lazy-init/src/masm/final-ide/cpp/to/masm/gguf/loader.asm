; gguf_loader_masm.asm
; Pure MASM x64 - GGUF Loader (converted from C++ GGUFLoaderQt class)
; GGUF model file parsing and tensor loading

option casemap:none

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memset:PROC
EXTERN memcpy:PROC
EXTERN strlen:PROC
EXTERN strcpy:PROC
EXTERN sprintf:PROC
EXTERN console_log:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC

; GGUF constants
MAX_TENSORS EQU 1000
MAX_METADATA_KEYS EQU 500
MAX_TOKENIZER_METADATA EQU 100
GGUF_MAGIC EQU 0x46554747          ; "GGUF" in little-endian
GGUF_VERSION EQU 3

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; GGUF_TENSOR - Tensor metadata
GGUF_TENSOR STRUCT
    name QWORD ?                    ; Tensor name
    offset QWORD ?                  ; File offset
    size QWORD ?                    ; Tensor size in bytes
    dimensions QWORD ?              ; Array of dimensions
    dimensionCount DWORD ?          ; Number of dimensions
    dtype DWORD ?                   ; Data type
    quantization DWORD ?            ; Quantization type
ENDS

; GGUF_METADATA - Metadata key-value pair
GGUF_METADATA STRUCT
    key QWORD ?                     ; Metadata key
    value QWORD ?                   ; Metadata value
    valueType DWORD ?               ; Value type
ENDS

; GGUF_LOADER - Loader state
GGUF_LOADER STRUCT
    filePath QWORD ?                ; Model file path
    fileHandle QWORD ?              ; File handle
    fileSize QWORD ?                ; File size
    
    tensors QWORD ?                 ; Array of GGUF_TENSOR
    tensorCount DWORD ?             ; Number of tensors
    maxTensors DWORD ?              ; Capacity
    
    metadata QWORD ?                ; Array of GGUF_METADATA
    metadataCount DWORD ?           ; Number of metadata entries
    maxMetadata DWORD ?             ; Capacity
    
    tokenizerMetadata QWORD ?       ; Tokenizer metadata array
    tokenizerCount DWORD ?          ; Tokenizer metadata count
    
    isOpen BYTE ?                   ; Whether file is open
    isValid BYTE ?                  ; Whether GGUF is valid
    
    ; Statistics
    totalTensorsLoaded QWORD ?
    totalBytesLoaded QWORD ?
    totalErrors DWORD ?
ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szLoaderCreated DB "[GGUF_LOADER] Created for: %s", 0
    szFileOpened DB "[GGUF_LOADER] Opened file: %s (%lld bytes)", 0
    szFileFailed DB "[GGUF_LOADER] Failed to open: %s", 0
    szTensorLoaded DB "[GGUF_LOADER] Loaded tensor: %s (%lld bytes)", 0
    szTensorFailed DB "[GGUF_LOADER] Failed to load tensor: %s", 0
    szMetadataRetrieved DB "[GGUF_LOADER] Retrieved metadata: %s", 0
    szUnsupportedQuant DB "[GGUF_LOADER] Unsupported quantization: %s", 0

; Data types
DTYPE_F32 EQU 0
DTYPE_F16 EQU 1
DTYPE_Q8 EQU 2
DTYPE_Q4 EQU 3
DTYPE_I32 EQU 4

; Quantization types
Q_TYPE_Q8 EQU 0
Q_TYPE_Q4 EQU 1
Q_TYPE_Q2 EQU 2
Q_TYPE_F16 EQU 3
Q_TYPE_F32 EQU 4

.code

; ============================================================================
; PUBLIC API
; ============================================================================

; gguf_loader_create(RCX = filePath)
; Create GGUF loader
; Returns: RAX = pointer to GGUF_LOADER
PUBLIC gguf_loader_create
gguf_loader_create PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = filePath
    
    ; Allocate loader
    mov rcx, SIZEOF GGUF_LOADER
    call malloc
    mov r9, rax
    
    ; Allocate tensors array
    mov rcx, MAX_TENSORS
    imul rcx, SIZEOF GGUF_TENSOR
    call malloc
    mov [r9 + GGUF_LOADER.tensors], rax
    
    ; Allocate metadata array
    mov rcx, MAX_METADATA_KEYS
    imul rcx, SIZEOF GGUF_METADATA
    call malloc
    mov [r9 + GGUF_LOADER.metadata], rax
    
    ; Allocate tokenizer metadata
    mov rcx, MAX_TOKENIZER_METADATA
    imul rcx, SIZEOF GGUF_METADATA
    call malloc
    mov [r9 + GGUF_LOADER.tokenizerMetadata], rax
    
    ; Initialize
    mov [r9 + GGUF_LOADER.filePath], rbx
    mov [r9 + GGUF_LOADER.tensorCount], 0
    mov [r9 + GGUF_LOADER.maxTensors], MAX_TENSORS
    mov [r9 + GGUF_LOADER.metadataCount], 0
    mov [r9 + GGUF_LOADER.maxMetadata], MAX_METADATA_KEYS
    mov [r9 + GGUF_LOADER.tokenizerCount], 0
    mov byte [r9 + GGUF_LOADER.isOpen], 0
    mov byte [r9 + GGUF_LOADER.isValid], 0
    mov [r9 + GGUF_LOADER.totalTensorsLoaded], 0
    mov [r9 + GGUF_LOADER.totalBytesLoaded], 0
    mov [r9 + GGUF_LOADER.totalErrors], 0
    
    ; Log
    lea rcx, [szLoaderCreated]
    mov rdx, rbx
    call console_log
    
    mov rax, r9
    pop rbx
    ret
gguf_loader_create ENDP

; ============================================================================

; gguf_loader_open(RCX = loader)
; Open GGUF file
; Returns: RAX = 1 if successful, 0 if failed
PUBLIC gguf_loader_open
gguf_loader_open PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = loader
    
    ; Check if already open
    cmp byte [rbx + GGUF_LOADER.isOpen], 1
    je .already_open
    
    ; Open file
    mov rcx, [rbx + GGUF_LOADER.filePath]
    mov rdx, 0x80000000             ; GENERIC_READ
    mov r8, 1                       ; FILE_SHARE_READ
    mov r9, 0                       ; lpSecurityAttributes
    mov r10, 3                      ; OPEN_EXISTING
    mov r11, 0x80                   ; FILE_ATTRIBUTE_NORMAL
    mov r12, 0                      ; hTemplateFile
    call CreateFileA
    
    cmp rax, 0xFFFFFFFFFFFFFFFF     ; INVALID_HANDLE_VALUE
    je .open_failed
    
    mov [rbx + GGUF_LOADER.fileHandle], rax
    
    ; Get file size
    mov rcx, rax
    call GetFileSize
    mov [rbx + GGUF_LOADER.fileSize], rax
    
    ; Validate GGUF magic
    mov rcx, [rbx + GGUF_LOADER.fileHandle]
    lea rdx, [magicBuffer]
    mov r8, 4                       ; Read 4 bytes
    mov r9, 0                       ; lpNumberOfBytesRead
    call ReadFile
    
    mov eax, [magicBuffer]
    cmp eax, GGUF_MAGIC
    jne .invalid_gguf
    
    mov byte [rbx + GGUF_LOADER.isOpen], 1
    mov byte [rbx + GGUF_LOADER.isValid], 1
    
    ; Log
    lea rcx, [szFileOpened]
    mov rdx, [rbx + GGUF_LOADER.filePath]
    mov r8, [rbx + GGUF_LOADER.fileSize]
    call console_log
    
    mov rax, 1
    pop rbx
    ret
    
.already_open:
    mov rax, 1
    pop rbx
    ret
    
.open_failed:
.invalid_gguf:
    lea rcx, [szFileFailed]
    mov rdx, [rbx + GGUF_LOADER.filePath]
    call console_log
    
    xor rax, rax
    pop rbx
    ret
gguf_loader_open ENDP

; ============================================================================

; gguf_loader_is_open(RCX = loader)
; Check if loader is open
; Returns: RAX = 1 if open, 0 if not
PUBLIC gguf_loader_is_open
gguf_loader_is_open PROC
    movzx eax, byte [rcx + GGUF_LOADER.isOpen]
    ret
gguf_loader_is_open ENDP

; ============================================================================

; gguf_loader_get_param(RCX = loader, RDX = key, R8 = defaultValue)
; Get metadata parameter
; Returns: RAX = parameter value
PUBLIC gguf_loader_get_param
gguf_loader_get_param PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = loader
    mov r9, rdx                     ; r9 = key
    mov r10, r8                     ; r10 = defaultValue
    
    ; Check if open
    cmp byte [rbx + GGUF_LOADER.isOpen], 1
    jne .not_open
    
    ; Search metadata
    mov r11, [rbx + GGUF_LOADER.metadata]
    mov r12d, [rbx + GGUF_LOADER.metadataCount]
    xor r13d, r13d
    
.search_metadata:
    cmp r13d, r12d
    jge .key_not_found
    
    mov r14, r11
    mov r15, r13
    imul r15, SIZEOF GGUF_METADATA
    add r14, r15
    
    mov rcx, [r14 + GGUF_METADATA.key]
    mov rdx, r9
    call strcmp
    cmp eax, 0
    je .key_found
    
    inc r13d
    jmp .search_metadata
    
.key_found:
    mov rax, [r14 + GGUF_METADATA.value]
    
    ; Log
    lea rcx, [szMetadataRetrieved]
    mov rdx, r9
    call console_log
    
    pop rbx
    ret
    
.key_not_found:
.not_open:
    mov rax, r10                    ; Return default value
    pop rbx
    ret
gguf_loader_get_param ENDP

; ============================================================================

; gguf_loader_inflate_weight(RCX = loader, RDX = tensorName)
; Load and decompress tensor
; Returns: RAX = tensor data (malloc'd), 0 on error
PUBLIC gguf_loader_inflate_weight
gguf_loader_inflate_weight PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = loader
    mov rsi, rdx                    ; rsi = tensorName
    
    ; Check if open
    cmp byte [rbx + GGUF_LOADER.isOpen], 1
    jne .not_open
    
    ; Find tensor
    mov r8, [rbx + GGUF_LOADER.tensors]
    mov r9d, [rbx + GGUF_LOADER.tensorCount]
    xor r10d, r10d
    
.find_tensor:
    cmp r10d, r9d
    jge .tensor_not_found
    
    mov r11, r8
    mov r12, r10
    imul r12, SIZEOF GGUF_TENSOR
    add r11, r12
    
    mov rcx, [r11 + GGUF_TENSOR.name]
    mov rdx, rsi
    call strcmp
    cmp eax, 0
    je .tensor_found
    
    inc r10d
    jmp .find_tensor
    
.tensor_found:
    ; Allocate buffer for tensor data
    mov rcx, [r11 + GGUF_TENSOR.size]
    call malloc
    mov r13, rax                    ; r13 = data buffer
    
    ; Read tensor data
    mov rcx, [rbx + GGUF_LOADER.fileHandle]
    mov rdx, [r11 + GGUF_TENSOR.offset]
    mov r8, 0                       ; FILE_BEGIN
    call SetFilePointer
    
    mov rcx, [rbx + GGUF_LOADER.fileHandle]
    mov rdx, r13
    mov r8, [r11 + GGUF_TENSOR.size]
    mov r9, 0                       ; lpNumberOfBytesRead
    call ReadFile
    
    ; Update statistics
    inc qword [rbx + GGUF_LOADER.totalTensorsLoaded]
    mov rax, [r11 + GGUF_TENSOR.size]
    add [rbx + GGUF_LOADER.totalBytesLoaded], rax
    
    ; Log
    lea rcx, [szTensorLoaded]
    mov rdx, rsi
    mov r8, [r11 + GGUF_TENSOR.size]
    call console_log
    
    mov rax, r13                    ; Return data buffer
    pop rsi
    pop rbx
    ret
    
.tensor_not_found:
.not_open:
    lea rcx, [szTensorFailed]
    mov rdx, rsi
    call console_log
    
    xor rax, rax
    pop rsi
    pop rbx
    ret
gguf_loader_inflate_weight ENDP

; ============================================================================

; gguf_loader_tensor_names(RCX = loader, RDX = namesBuffer)
; Get list of tensor names
; Returns: RAX = number of tensors
PUBLIC gguf_loader_tensor_names
gguf_loader_tensor_names PROC
    mov r8, [rcx + GGUF_LOADER.tensors]
    mov r9d, [rcx + GGUF_LOADER.tensorCount]
    xor r10d, r10d
    
.copy_names:
    cmp r10d, r9d
    jge .names_copied
    
    mov r11, r8
    mov r12, r10
    imul r12, SIZEOF GGUF_TENSOR
    add r11, r12
    
    mov rax, [r11 + GGUF_TENSOR.name]
    mov [rdx + r10 * 8], rax        ; Store pointer in buffer
    
    inc r10d
    jmp .copy_names
    
.names_copied:
    mov eax, r9d                    ; Return tensor count
    ret
gguf_loader_tensor_names ENDP

; ============================================================================

; gguf_loader_has_unsupported_quantization(RCX = loader)
; Check for unsupported quantization types
; Returns: RAX = 1 if unsupported, 0 if all supported
PUBLIC gguf_loader_has_unsupported_quantization
gguf_loader_has_unsupported_quantization PROC
    push rbx
    
    mov rbx, rcx
    
    ; Check tensors for unsupported quantization
    mov r8, [rbx + GGUF_LOADER.tensors]
    mov r9d, [rbx + GGUF_LOADER.tensorCount]
    xor r10d, r10d
    
.check_quantization:
    cmp r10d, r9d
    jge .all_supported
    
    mov r11, r8
    mov r12, r10
    imul r12, SIZEOF GGUF_TENSOR
    add r11, r12
    
    mov eax, [r11 + GGUF_TENSOR.quantization]
    cmp eax, Q_TYPE_Q2              ; Q2 is unsupported
    je .unsupported_found
    
    inc r10d
    jmp .check_quantization
    
.unsupported_found:
    lea rcx, [szUnsupportedQuant]
    mov rdx, [r11 + GGUF_TENSOR.name]
    call console_log
    
    mov rax, 1
    pop rbx
    ret
    
.all_supported:
    xor rax, rax
    pop rbx
    ret
gguf_loader_has_unsupported_quantization ENDP

; ============================================================================

; gguf_loader_get_statistics(RCX = loader, RDX = statsBuffer)
; Get loader statistics
PUBLIC gguf_loader_get_statistics
gguf_loader_get_statistics PROC
    mov [rdx + 0], qword [rcx + GGUF_LOADER.totalTensorsLoaded]
    mov [rdx + 8], qword [rcx + GGUF_LOADER.totalBytesLoaded]
    mov [rdx + 16], dword [rcx + GGUF_LOADER.totalErrors]
    ret
gguf_loader_get_statistics ENDP

; ============================================================================

; gguf_loader_close(RCX = loader)
; Close GGUF file
PUBLIC gguf_loader_close
gguf_loader_close PROC
    push rbx
    
    mov rbx, rcx
    
    ; Check if open
    cmp byte [rbx + GGUF_LOADER.isOpen], 1
    jne .already_closed
    
    ; Close file
    mov rcx, [rbx + GGUF_LOADER.fileHandle]
    call CloseHandle
    
    mov byte [rbx + GGUF_LOADER.isOpen], 0
    
.already_closed:
    pop rbx
    ret
gguf_loader_close ENDP

; ============================================================================

; gguf_loader_destroy(RCX = loader)
; Free GGUF loader
PUBLIC gguf_loader_destroy
gguf_loader_destroy PROC
    push rbx
    
    mov rbx, rcx
    
    ; Close file if open
    mov rcx, rbx
    call gguf_loader_close
    
    ; Free tensors array
    mov r10, [rbx + GGUF_LOADER.tensors]
    mov r11d, [rbx + GGUF_LOADER.tensorCount]
    xor r12d, r12d
    
.free_tensors:
    cmp r12d, r11d
    jge .tensors_freed
    
    mov r13, r10
    mov r14, r12
    imul r14, SIZEOF GGUF_TENSOR
    add r13, r14
    
    mov rcx, [r13 + GGUF_TENSOR.name]
    cmp rcx, 0
    je .skip_tensor_name
    call free
    
.skip_tensor_name:
    inc r12d
    jmp .free_tensors
    
.tensors_freed:
    mov rcx, [rbx + GGUF_LOADER.tensors]
    cmp rcx, 0
    je .skip_tensors_array
    call free
    
.skip_tensors_array:
    ; Free metadata array
    mov rcx, [rbx + GGUF_LOADER.metadata]
    cmp rcx, 0
    je .skip_metadata
    call free
    
.skip_metadata:
    ; Free tokenizer metadata
    mov rcx, [rbx + GGUF_LOADER.tokenizerMetadata]
    cmp rcx, 0
    je .skip_tokenizer
    call free
    
.skip_tokenizer:
    ; Free file path
    mov rcx, [rbx + GGUF_LOADER.filePath]
    cmp rcx, 0
    je .skip_filepath
    call free
    
.skip_filepath:
    ; Free loader
    mov rcx, rbx
    call free
    
    pop rbx
    ret
gguf_loader_destroy ENDP

; ============================================================================

.data
    magicBuffer DWORD ?

END
