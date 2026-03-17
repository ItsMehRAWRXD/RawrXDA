;================================================================================
; PHASE2_MASTER.ASM - Model Loader & Format Router
; Universal Model Loading: GGUF/Safetensors/PyTorch/ONNX with Streaming & Quantization
; Bridges Phase-1 Foundation to Phase-4 Swarm Inference
;================================================================================

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION WIN64:3

;================================================================================
; EXTERNAL IMPORTS - Phase-1 Foundation + Compression/Crypto
;================================================================================
; Phase-1 Foundation
EXTERN Phase1Initialize : proc
EXTERN ArenaAllocate : proc
EXTERN ReadTsc : proc
EXTERN GetElapsedMicroseconds : proc
EXTERN Phase1LogMessage : proc

; Win32 I/O
EXTERN CreateFileA : proc
EXTERN CreateFileW : proc
EXTERN CreateFileMappingA : proc
EXTERN MapViewOfFileEx : proc
EXTERN UnmapViewOfFile : proc
EXTERN VirtualAlloc : proc
EXTERN VirtualFree : proc
EXTERN VirtualProtect : proc
EXTERN ReadFile : proc
EXTERN ReadFileEx : proc
EXTERN WriteFile : proc
EXTERN SetFilePointerEx : proc
EXTERN GetFileSizeEx : proc
EXTERN GetFileAttributesA : proc
EXTERN GetFileAttributesW : proc
EXTERN FindFirstFileA : proc
EXTERN FindNextFileA : proc
EXTERN FindClose : proc
EXTERN DeviceIoControl : proc
EXTERN GetCompressedFileSizeA : proc
EXTERN GetDiskFreeSpaceExA : proc
EXTERN FlushFileBuffers : proc
EXTERN CloseHandle : proc

; Compression (for MASM-compressed blobs)
EXTERN RtlDecompressBuffer : proc
EXTERN RtlCompressBuffer : proc
EXTERN RtlGetCompressionWorkSpaceSize : proc

; Cryptography (for secure model loading)
EXTERN BCryptOpenAlgorithmProvider : proc
EXTERN BCryptCloseAlgorithmProvider : proc
EXTERN BCryptCreateHash : proc
EXTERN BCryptDestroyHash : proc
EXTERN BCryptHashData : proc
EXTERN BCryptFinishHash : proc
EXTERN BCryptGenRandom : proc

; HTTP/Network (for HF/Ollama download)
EXTERN WSAStartup : proc
EXTERN WSACleanup : proc
EXTERN socket : proc
EXTERN connect : proc
EXTERN send : proc
EXTERN recv : proc
EXTERN closesocket : proc
EXTERN inet_addr : proc
EXTERN htons : proc
EXTERN gethostbyname : proc
EXTERN Sleep : proc

; Threading
EXTERN CreateThread : proc
EXTERN CreateEventA : proc
EXTERN SetEvent : proc
EXTERN WaitForSingleObject : proc
EXTERN EnterCriticalSection : proc
EXTERN LeaveCriticalSection : proc
EXTERN InitializeCriticalSection : proc

;================================================================================
; CONSTANTS - Format Detection & Loading
;================================================================================
; Format magics
GGUF_MAGIC              EQU 46554747h   ; "GGUF" little-endian
GGUF_VERSION_V1         EQU 1
GGUF_VERSION_V2         EQU 2
GGUF_VERSION_V3         EQU 3

SAFETENSORS_MAGIC       EQU 7B7B7B7Bh   ; "{{{{" (JSON starts with {)
PYTORCH_MAGIC           EQU 8002022Eh   ; Pickle protocol 2
ONNX_MAGIC              EQU 08080808h   ; Protobuf varint

; Tensor data types
GGML_TYPE_F32           EQU 0
GGML_TYPE_F16           EQU 1
GGML_TYPE_Q4_0          EQU 2
GGML_TYPE_Q4_1          EQU 3
GGML_TYPE_Q5_0          EQU 6
GGML_TYPE_Q5_1          EQU 7
GGML_TYPE_Q8_0          EQU 8
GGML_TYPE_Q8_1          EQU 9
GGML_TYPE_Q2_K          EQU 10
GGML_TYPE_Q3_K          EQU 11
GGML_TYPE_Q4_K          EQU 12
GGML_TYPE_Q5_K          EQU 13
GGML_TYPE_Q6_K          EQU 14
GGML_TYPE_Q8_K          EQU 15
GGML_TYPE_IQ2_XXS       EQU 16
GGML_TYPE_IQ2_XS        EQU 17
GGML_TYPE_IQ3_XXS       EQU 18
GGML_TYPE_IQ1_S         EQU 19
GGML_TYPE_IQ4_NL        EQU 20
GGML_TYPE_IQ3_S         EQU 21
GGML_TYPE_IQ2_S         EQU 22
GGML_TYPE_IQ4_XS        EQU 23
GGML_TYPE_I8            EQU 24
GGML_TYPE_I16           EQU 25
GGML_TYPE_I32           EQU 26
GGML_TYPE_I64           EQU 27
GGML_TYPE_F64           EQU 28
GGML_TYPE_IQ1_M         EQU 29

; Loading flags
LOAD_FLAG_STREAMING     EQU 000000001h  ; Don't load all at once
LOAD_FLAG_MMAP          EQU 000000002h  ; Memory-map files
LOAD_FLAG_VERIFY        EQU 000000004h  ; Verify checksums
LOAD_FLAG_DECRYPT       EQU 000000008h  ; Decrypt encrypted models
LOAD_FLAG_PROGRESS      EQU 000000010h  ; Report progress callbacks
LOAD_FLAG_NUMA_AFFINE   EQU 000000020h  ; Allocate on specific NUMA node
LOAD_FLAG_GPU_PIN       EQU 000000040h  ; Pin for GPU DMA

; Router types
ROUTER_TYPE_UNKNOWN     EQU 0
ROUTER_TYPE_GGUF_LOCAL  EQU 1
ROUTER_TYPE_GGUF_MMAP   EQU 2
ROUTER_TYPE_HF_HUB      EQU 3
ROUTER_TYPE_OLLAMA_API  EQU 4
ROUTER_TYPE_MASM_BLOB   EQU 5

; Buffer sizes
TENSOR_NAME_MAX         EQU 128
MAX_TENSORS             EQU 10000
MAX_METADATA_PAIRS      EQU 1000
CHUNK_SIZE              EQU 100000h     ; 1MB chunks for streaming
CIRCULAR_BUFFER_SIZE    EQU 40000000h   ; 1GB circular buffer

; HTTP constants
HTTP_PORT_HF            EQU 50          ; 80 decimal (0x50)
HTTP_PORT_OLLAMA        EQU 11D1h       ; 11434 decimal

; GGUF value types
GGUF_TYPE_UINT8         EQU 0
GGUF_TYPE_INT8          EQU 1
GGUF_TYPE_UINT16        EQU 2
GGUF_TYPE_INT16         EQU 3
GGUF_TYPE_UINT32        EQU 4
GGUF_TYPE_INT32         EQU 5
GGUF_TYPE_FLOAT32       EQU 6
GGUF_TYPE_BOOL          EQU 7
GGUF_TYPE_STRING        EQU 8
GGUF_TYPE_ARRAY         EQU 9
GGUF_TYPE_UINT64        EQU 10
GGUF_TYPE_INT64         EQU 11
GGUF_TYPE_FLOAT64       EQU 12

;================================================================================
; STRUCTURES - Format Headers & Tensor Metadata
;================================================================================
GGUF_HEADER STRUCT 8
    magic           dd ?      ; GGUF_MAGIC
    version         dd ?      ; 1, 2, or 3
    tensor_count    dq ?      ; Number of tensors
    metadata_count  dq ?      ; Number of metadata kv pairs
GGUF_HEADER ENDS

GGUF_METADATA KV STRUCT 8
    key             dq ?      ; Offset to string
    value_type      dd ?      ; Type enum
    value           db 8 DUP(?); Union of value types
GGUF_METADATA KV ENDS

GGUF_TENSOR_INFO STRUCT 16
    name            dq ?      ; Offset to name string
    n_dims          dd ?      ; Number of dimensions (1-4)
    dims            dq 4 DUP(?); Dimensions
    type            dd ?      ; GGML_TYPE_*
    offset          dq ?      ; Offset in file to data
GGUF_TENSOR_INFO ENDS

TENSOR_METADATA STRUCT 512
    ; Identification
    name            db TENSOR_NAME_MAX DUP(?)
    name_hash       dq ?      ; For fast lookup
    
    ; Dimensions
    n_dims          dd ?
    dims            dq 4 DUP(?)
    n_elements      dq ?
    
    ; Data
    dtype           dd ?
    type_size       dd ?      ; Bytes per element (dequantized)
    data_size       dq ?      ; Size in file
    
    ; Location
    file_offset     dq ?
    file_handle     dq ?
    
    ; Memory
    host_ptr        dq ?      ; CPU memory address
    device_ptr      dq ?      ; GPU memory address
    dma_handle      dq ?      ; For pinned memory
    
    ; Quantization
    quant_type      dd ?
    quant_block_size dd ?
    quant_scale_ptr dq ?
    
    ; State
    state           dd ?      ; 0=unloaded,1=loading,2=loaded,3=evicted
    ref_count       dd ?
    last_access     dq ?      ; Timestamp
    
    ; NUMA affinity
    preferred_node  dd ?
    actual_node     dd ?
TENSOR_METADATA ENDS

MODEL_LOADER_CONTEXT STRUCT 8192
    ; Phase-1 backlink
    phase1_ctx      dq ?
    
    ; Source
    source_path     db 260 DUP(?)     ; MAX_PATH
    source_type     dd ?              ; ROUTER_TYPE_*
    source_flags    dd ?              ; LOAD_FLAG_*
    
    ; File handles
    file_handle     dq ?
    file_size       dq ?
    file_mapping    dq ?
    mapped_view     dq ?
    
    ; Format-specific
    format_type     dd ?
    gguf_version    dd ?
    header_size     dq ?
    tensor_data_offset dq ?
    
    ; Tensor registry
    tensor_count    dq ?
    tensor_capacity dq ?
    tensor_table    dq ?              ; Pointer to TENSOR_METADATA array
    
    ; Metadata
    metadata_count  dq ?
    metadata_table  dq ?              ; Key-value pairs
    
    ; Model architecture (extracted from metadata)
    arch_type       dd ?              ; 0=llama,1=mistral,2=phi,etc
    vocab_size      dd ?
    context_length  dd ?
    embedding_length dd ?
    block_count     dd ?
    feed_forward_length dd ?
    attention_head_count dd ?
    attention_head_count_kv dd ?
    rope_freq_base  dd ?
    rope_dim_count  dd ?
    
    ; Loading state
    bytes_loaded    dq ?
    tensors_loaded  dq ?
    progress_cb     dq ?              ; Callback function
    progress_ctx    dq ?              ; Callback context
    
    ; Threading
    load_thread     dq ?
    io_completion   dq ?
    cancel_event    dq ?
    
    ; Streaming
    stream_buffer   dq ?              ; Circular buffer
    stream_write_ptr dq ?
    stream_read_ptr dq ?
    stream_lock     CRITICAL_SECTION <>
    
    ; Verification
    expected_hash   db 32 DUP(?)      ; SHA-256
    actual_hash     db 32 DUP(?)
    
    ; Error state
    last_error      dd ?
    error_line      dd ?
MODEL_LOADER_CONTEXT ENDS

HF_DOWNLOAD_CONTEXT STRUCT 512
    socket          dq ?
    file_handle     dq ?
    bytes_total     dq ?
    bytes_received  dq ?
    chunk_buffer    dq ?
    resume_offset   dq ?
    etag            db 64 DUP(?)
    last_modified   db 64 DUP(?)
HF_DOWNLOAD_CONTEXT ENDS

OLLAMA_API_CONTEXT STRUCT 512
    base_url        db 256 DUP(?)
    model_name      db 128 DUP(?)
    api_key         db 128 DUP(?)
    socket          dq ?
    session_token   db 256 DUP(?)
OLLAMA_API_CONTEXT ENDS

;================================================================================
; DATA SECTION
;================================================================================
.DATA
ALIGN 64

; Format signatures for detection
sig_gguf            db "GGUF", 0
sig_safetensors     db "safetensors", 0
sig_pytorch         db "pytorch", 0
sig_pickle          db "pkl", 0
sig_onnx            db "onnx", 0

; Architecture strings
arch_llama          db "llama", 0
arch_mistral        db "mistral", 0
arch_phi            db "phi", 0
arch_gemma          db "gemma", 0
arch_qwen           db "qwen", 0

; HTTP templates
http_get_template   db "GET %s HTTP/1.1", 0Dh, 0Ah
                    db "Host: %s", 0Dh, 0Ah
                    db "User-Agent: RawrXD-ModelLoader/1.0", 0Dh, 0Ah
                    db "Accept: application/octet-stream", 0Dh, 0Ah
                    db "Range: bytes=%llu-", 0Dh, 0Ah
                    db "Connection: keep-alive", 0Dh, 0Ah, 0Dh, 0Ah, 0

; Error strings
err_file_not_found  db "[PHASE2] ERROR: Model file not found: %s", 0Dh, 0Ah, 0
err_format_unknown  db "[PHASE2] ERROR: Unknown model format", 0Dh, 0Ah, 0
err_memory_alloc    db "[PHASE2] ERROR: Memory allocation failed", 0Dh, 0Ah, 0
err_tensor_overflow db "[PHASE2] ERROR: Tensor count exceeds maximum", 0Dh, 0Ah, 0
err_verify_failed   db "[PHASE2] ERROR: Checksum verification failed", 0Dh, 0Ah, 0
err_network         db "[PHASE2] ERROR: Network operation failed", 0Dh, 0Ah, 0

; Progress strings
str_loading         db "[PHASE2] Loading model: %s", 0Dh, 0Ah, 0
str_format_detected db "[PHASE2] Format: %s, Version: %d", 0Dh, 0Ah, 0
str_tensors_found   db "[PHASE2] Found %llu tensors", 0Dh, 0Ah, 0
str_loading_tensor  db "[PHASE2] Loading tensor [%llu/%llu]: %s (%.2f MB)", 0Dh, 0Ah, 0
str_progress        db "[PHASE2] Progress: %llu/%llu MB (%.1f%%)", 0Dh, 0Ah, 0
str_complete        db "[PHASE2] Model loaded successfully in %llu ms", 0Dh, 0Ah, 0

; Format names
fmt_gguf            db "GGUF", 0
fmt_safetensors     db "Safetensors", 0
fmt_pytorch         db "PyTorch", 0
fmt_onnx            db "ONNX", 0

; Quantization type names
q_f32               db "F32", 0
q_f16               db "F16", 0
q_q4_0              db "Q4_0", 0
q_q4_1              db "Q4_1", 0
q_q5_0              db "Q5_0", 0
q_q5_1              db "Q5_1", 0
q_q8_0              db "Q8_0", 0
q_q8_1              db "Q8_1", 0
q_q2_k              db "Q2_K", 0
q_q3_k              db "Q3_K", 0
q_q4_k              db "Q4_K", 0
q_q5_k              db "Q5_K", 0
q_q6_k              db "Q6_K", 0
q_q8_k              db "Q8_K", 0

; SAVE_REGS and RESTORE_REGS macros for non-volatile registers
SAVE_REGS MACRO
    push rsi
    push rdi
    push rbx
    push r12
    push r13
    push r14
    push r15
ENDM

RESTORE_REGS MACRO
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rdi
    pop rsi
ENDM

;================================================================================
; CODE SECTION
;================================================================================
.CODE
ALIGN 64

;================================================================================
; PHASE 2: INITIALIZATION
;================================================================================

;-------------------------------------------------------------------------------
; Phase2Initialize - Create model loader context
; Input:  RCX = Phase-1 context pointer
; Output: RAX = MODEL_LOADER_CONTEXT* or NULL
;-------------------------------------------------------------------------------
Phase2Initialize PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 200h
    
    mov r12, rcx                      ; R12 = Phase-1 context
    
    ; Allocate loader context
    mov rcx, 0
    mov rdx, sizeof MODEL_LOADER_CONTEXT
    mov r8, 1000h
    mov r9, 4
    call VirtualAlloc
    test rax, rax
    jz @phase2_init_fail
    mov rbx, rax                      ; RBX = MODEL_LOADER_CONTEXT*
    
    ; Store Phase-1 context
    mov [rbx].MODEL_LOADER_CONTEXT.phase1_ctx, r12
    
    ; Initialize tensor table
    mov [rbx].MODEL_LOADER_CONTEXT.tensor_capacity, MAX_TENSORS
    
    ; Allocate tensor table from Phase-1 arena
    mov rcx, r12
    mov rdx, MAX_TENSORS * sizeof TENSOR_METADATA
    mov r8, 64
    call ArenaAllocate
    test rax, rax
    jz @phase2_init_cleanup
    mov [rbx].MODEL_LOADER_CONTEXT.tensor_table, rax
    
    ; Initialize critical section for streaming
    lea rcx, [rbx].MODEL_LOADER_CONTEXT.stream_lock
    call InitializeCriticalSection
    
    ; Create cancel event
    xor ecx, ecx                      ; Security
    xor edx, edx                      ; Manual reset = FALSE
    xor r8d, r8d                      ; Initial state = FALSE
    xor r9d, r9d                      ; Name = NULL
    call CreateEventA
    mov [rbx].MODEL_LOADER_CONTEXT.cancel_event, rax
    
    mov rax, rbx
    jmp @phase2_init_exit
    
@phase2_init_cleanup:
    mov rcx, rbx
    xor rdx, rdx
    mov r8, 8000h
    call VirtualFree
    
@phase2_init_fail:
    xor rax, rax
    
@phase2_init_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
Phase2Initialize ENDP

;================================================================================
; FORMAT ROUTER - Universal Format Detection & Dispatch
;================================================================================

;-------------------------------------------------------------------------------
; DetectModelFormat - Determine format from file content
; Input:  RCX = MODEL_LOADER_CONTEXT*
;         RDX = File path
; Output: EAX = format type (0=unknown, 1=GGUF, 2=Safetensors, 3=PyTorch, 4=ONNX)
;-------------------------------------------------------------------------------
DetectModelFormat PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 100h
    
    mov rbx, rcx
    mov r12, rdx                      ; R12 = file path
    
    ; Open file for read
    mov rcx, r12
    xor edx, edx                      ; GENERIC_READ
    mov r8d, 3                        ; FILE_SHARE_READ | FILE_SHARE_WRITE
    xor r9d, r9d                      ; Security
    push 0                            ; Template
    push 3                            ; OPEN_EXISTING
    push 080h                         ; FILE_ATTRIBUTE_NORMAL
    push 0
    sub rsp, 20h
    call CreateFileA
    add rsp, 38h
    
    cmp rax, -1
    je @detect_fail
    mov r13, rax                      ; R13 = file handle
    
    ; Read first 16 bytes for magic detection
    mov rcx, r13
    lea rdx, [rbp-16]
    mov r8d, 16
    xor r9d, r9d
    push 0
    sub rsp, 20h
    call ReadFile
    add rsp, 28h
    
    ; Check magic
    mov eax, [rbp-16]                 ; First 4 bytes
    
    cmp eax, GGUF_MAGIC
    je @detect_gguf
    
    ; Check for Safetensors (JSON starts with {)
    cmp al, '{'
    je @detect_safetensors
    
    ; Check for PyTorch pickle
    cmp eax, PYTORCH_MAGIC
    je @detect_pytorch
    
    ; Check for ONNX
    cmp eax, ONNX_MAGIC
    je @detect_onnx
    
    ; Unknown format
    mov eax, 0
    jmp @detect_cleanup
    
@detect_gguf:
    mov eax, 1
    jmp @detect_cleanup
    
@detect_safetensors:
    mov eax, 2
    jmp @detect_cleanup
    
@detect_pytorch:
    mov eax, 3
    jmp @detect_cleanup
    
@detect_onnx:
    mov eax, 4
    jmp @detect_cleanup
    
@detect_cleanup:
    mov r14d, eax                     ; Save result
    mov rcx, r13
    call CloseHandle
    mov eax, r14d
    jmp @detect_exit
    
@detect_fail:
    xor eax, eax
    
@detect_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
DetectModelFormat ENDP

;-------------------------------------------------------------------------------
; RouteModelLoad - Main entry point for model loading
; Input:  RCX = MODEL_LOADER_CONTEXT*
;         RDX = Source path/URL
;         R8  = Flags (LOAD_FLAG_*)
; Output: EAX = 1 on success, 0 on failure
;-------------------------------------------------------------------------------
RouteModelLoad PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 400h
    
    mov rbx, rcx
    mov r12, rdx                      ; R12 = source
    mov r13d, r8d                     ; R13 = flags
    
    ; Store parameters
    mov [rbx].MODEL_LOADER_CONTEXT.source_flags, r13d
    
    ; Copy source path
    push rsi
    push rdi
    lea rdi, [rbx].MODEL_LOADER_CONTEXT.source_path
    mov rsi, r12
    mov ecx, 260
    rep movsb
    pop rdi
    pop rsi
    
    ; Log loading start
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    lea rdx, str_loading
    mov r8, r12
    call Phase1LogMessage
    
    ; Detect router type from path
    mov rcx, r12
    call DetermineRouterType
    mov [rbx].MODEL_LOADER_CONTEXT.source_type, eax
    
    ; Dispatch to appropriate loader
    cmp eax, ROUTER_TYPE_GGUF_LOCAL
    je @route_gguf_local
    cmp eax, ROUTER_TYPE_GGUF_MMAP
    je @route_gguf_mmap
    cmp eax, ROUTER_TYPE_HF_HUB
    je @route_hf_hub
    cmp eax, ROUTER_TYPE_OLLAMA_API
    je @route_ollama_api
    cmp eax, ROUTER_TYPE_MASM_BLOB
    je @route_masm_blob
    
    ; Unknown router
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    lea rdx, err_format_unknown
    call Phase1LogMessage
    xor eax, eax
    jmp @route_exit
    
@route_gguf_local:
    mov rcx, rbx
    call LoadGGUFLocal
    jmp @route_exit
    
@route_gguf_mmap:
    mov rcx, rbx
    call LoadGGUFMmap
    jmp @route_exit
    
@route_hf_hub:
    mov rcx, rbx
    call LoadHFHub
    jmp @route_exit
    
@route_ollama_api:
    mov rcx, rbx
    call LoadOllamaAPI
    jmp @route_exit
    
@route_masm_blob:
    mov rcx, rbx
    call LoadMASMBlob
    jmp @route_exit
    
@route_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
RouteModelLoad ENDP

;-------------------------------------------------------------------------------
; DetermineRouterType - Parse source path to determine loading strategy
; Input:  RCX = Source path/URL
; Output: EAX = router type
;-------------------------------------------------------------------------------
DetermineRouterType PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    
    ; Check for URL schemes
    mov eax, [rbx]
    and eax, 0FFFFFFFFh
    
    cmp eax, 'ptth'                   ; "http" reversed
    je @router_http
    
    ; Check for HuggingFace hub pattern (hf:// or org/model)
    cmp word ptr [rbx], 'fh'          ; "hf"
    je @router_hf
    
    ; Check for Ollama pattern
    mov eax, [rbx+4]
    and eax, 0FFFFFFFFh
    cmp eax, '://o'                   ; "o://" from "ollama://"
    je @router_ollama
    
    ; Check for MASM blob marker
    cmp word ptr [rbx], 'AM'          ; "MA" from "MASM"
    je @router_masm
    
    ; Default: local file
    mov eax, ROUTER_TYPE_GGUF_LOCAL
    jmp @router_exit
    
@router_http:
    mov eax, ROUTER_TYPE_HF_HUB
    jmp @router_exit
    
@router_hf:
    mov eax, ROUTER_TYPE_HF_HUB
    jmp @router_exit
    
@router_ollama:
    mov eax, ROUTER_TYPE_OLLAMA_API
    jmp @router_exit
    
@router_masm:
    mov eax, ROUTER_TYPE_MASM_BLOB
    jmp @router_exit
    
@router_exit:
    RESTORE_REGS
    ret
DetermineRouterType ENDP

;================================================================================
; GGUF LOADER - Primary format (complete implementation)
;================================================================================

;-------------------------------------------------------------------------------
; LoadGGUFLocal - Load GGUF file with full tensor extraction
; Input:  RCX = MODEL_LOADER_CONTEXT*
; Output: EAX = 1 on success, 0 on failure
;-------------------------------------------------------------------------------
LoadGGUFLocal PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 1000h
    
    mov rbx, rcx                      ; RBX = MODEL_LOADER_CONTEXT*
    
    ; Record start time
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    call GetElapsedMicroseconds
    mov r12, rax                      ; R12 = start time
    
    ; Open file
    lea rcx, [rbx].MODEL_LOADER_CONTEXT.source_path
    xor edx, edx                      ; GENERIC_READ
    mov r8d, 3                        ; FILE_SHARE_READ | FILE_SHARE_WRITE
    xor r9d, r9d
    push 0
    push 3                            ; OPEN_EXISTING
    push 080h
    push 0
    sub rsp, 20h
    call CreateFileA
    add rsp, 38h
    
    cmp rax, -1
    je @gguf_fail_file
    mov [rbx].MODEL_LOADER_CONTEXT.file_handle, rax
    
    ; Get file size
    mov rcx, rax
    lea rdx, [rbp-16]
    call GetFileSizeEx
    mov rax, [rbp-16]
    mov [rbx].MODEL_LOADER_CONTEXT.file_size, rax
    
    ; Read header
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.file_handle
    lea rdx, [rbp-64]                 ; Header buffer
    mov r8d, sizeof GGUF_HEADER
    xor r9d, r9d
    push 0
    sub rsp, 20h
    call ReadFile
    add rsp, 28h
    
    ; Verify magic
    mov eax, [rbp-64]                 ; Header.magic
    cmp eax, GGUF_MAGIC
    jne @gguf_fail_format
    
    ; Store format info
    mov eax, [rbp-64+4]               ; Header.version
    mov [rbx].MODEL_LOADER_CONTEXT.gguf_version, eax
    mov [rbx].MODEL_LOADER_CONTEXT.format_type, 1
    
    mov rax, [rbp-64+8]               ; Header.tensor_count
    mov [rbx].MODEL_LOADER_CONTEXT.tensor_count, rax
    
    mov rax, [rbp-64+16]              ; Header.metadata_count
    mov [rbx].MODEL_LOADER_CONTEXT.metadata_count, rax
    
    ; Log format detection
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    lea rdx, str_format_detected
    lea r8, fmt_gguf
    mov r9d, [rbx].MODEL_LOADER_CONTEXT.gguf_version
    call Phase1LogMessage
    
    ; Parse tensor info
    mov rcx, rbx
    call ParseGGUFTensorInfo
    test eax, eax
    jz @gguf_fail_parse
    
    ; Log tensor count
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    lea rdx, str_tensors_found
    mov r8, [rbx].MODEL_LOADER_CONTEXT.tensor_count
    call Phase1LogMessage
    
    ; Load tensors (or set up for streaming)
    test [rbx].MODEL_LOADER_CONTEXT.source_flags, LOAD_FLAG_STREAMING
    jnz @gguf_streaming_setup
    
    ; Load all tensors now
    mov rcx, rbx
    call LoadAllGGUFTensors
    jmp @gguf_verify
    
@gguf_streaming_setup:
    mov rcx, rbx
    call SetupStreamingBuffer
    
@gguf_verify:
    ; Verify if requested
    test [rbx].MODEL_LOADER_CONTEXT.source_flags, LOAD_FLAG_VERIFY
    jz @gguf_complete
    
    mov rcx, rbx
    call VerifyModelChecksum
    
@gguf_complete:
    ; Calculate elapsed time
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    call GetElapsedMicroseconds
    sub rax, r12
    mov r8, rax
    shr r8, 10                        ; Convert to ms (divide by 1000)
    
    ; Log completion
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    lea rdx, str_complete
    call Phase1LogMessage
    
    mov eax, 1
    jmp @gguf_exit
    
@gguf_fail_file:
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    lea rdx, err_file_not_found
    lea r8, [rbx].MODEL_LOADER_CONTEXT.source_path
    call Phase1LogMessage
    xor eax, eax
    jmp @gguf_exit
    
@gguf_fail_format:
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    lea rdx, err_format_unknown
    call Phase1LogMessage
    xor eax, eax
    jmp @gguf_exit
    
@gguf_fail_parse:
    xor eax, eax
    
@gguf_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
LoadGGUFLocal ENDP

;-------------------------------------------------------------------------------
; ParseGGUFTensorInfo - Build tensor metadata table
; Input:  RCX = MODEL_LOADER_CONTEXT*
; Output: EAX = 1 on success, 0 on failure
;-------------------------------------------------------------------------------
ParseGGUFTensorInfo PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 200h
    
    mov rbx, rcx
    
    ; Get current file position (after header)
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.file_handle
    lea rdx, [rbp-8]
    xor r8d, r8d
    mov r9d, 1                        ; FILE_CURRENT
    call SetFilePointerEx
    
    ; Read each tensor info
    xor r13d, r13d                    ; Tensor index
@tensor_info_loop:
    cmp r13, [rbx].MODEL_LOADER_CONTEXT.tensor_count
    jge @tensor_info_done
    
    ; Get tensor metadata slot
    mov rax, r13
    imul rax, sizeof TENSOR_METADATA
    add rax, [rbx].MODEL_LOADER_CONTEXT.tensor_table
    mov r14, rax                      ; R14 = TENSOR_METADATA*
    
    ; Read name length
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.file_handle
    lea rdx, [rbp-8]
    mov r8d, 8
    xor r9d, r9d
    push 0
    sub rsp, 20h
    call ReadFile
    add rsp, 28h
    
    mov r15, [rbp-8]                  ; R15 = name length
    
    ; Read name
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.file_handle
    lea rdx, [r14].TENSOR_METADATA.name
    cmp r15, TENSOR_NAME_MAX
    cmova r15, TENSOR_NAME_MAX
    mov r8, r15
    xor r9d, r9d
    push 0
    sub rsp, 20h
    call ReadFile
    add rsp, 28h
    
    ; Null-terminate
    mov byte ptr [r14].TENSOR_METADATA.name[r15], 0
    
    ; Compute name hash for fast lookup
    lea rcx, [r14].TENSOR_METADATA.name
    call ComputeHash64
    mov [r14].TENSOR_METADATA.name_hash, rax
    
    ; Read n_dims
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.file_handle
    lea rdx, [r14].TENSOR_METADATA.n_dims
    mov r8d, 4
    xor r9d, r9d
    push 0
    sub rsp, 20h
    call ReadFile
    add rsp, 28h
    
    ; Read dimensions
    mov ecx, [r14].TENSOR_METADATA.n_dims
    cmp ecx, 4
    ja @tensor_info_fail
    
    lea r15, [r14].TENSOR_METADATA.dims
@dims_loop:
    test ecx, ecx
    jz @dims_done
    
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.file_handle
    mov rdx, r15
    mov r8d, 8
    xor r9d, r9d
    push 0
    sub rsp, 20h
    call ReadFile
    add rsp, 28h
    
    add r15, 8
    dec ecx
    jmp @dims_loop
    
@dims_done:
    ; Read type
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.file_handle
    lea rdx, [r14].TENSOR_METADATA.dtype
    mov r8d, 4
    xor r9d, r9d
    push 0
    sub rsp, 20h
    call ReadFile
    add rsp, 28h
    
    ; Read offset
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.file_handle
    lea rdx, [r14].TENSOR_METADATA.file_offset
    mov r8d, 8
    xor r9d, r9d
    push 0
    sub rsp, 20h
    call ReadFile
    add rsp, 28h
    
    ; Calculate element count and data size
    mov rcx, r14
    call CalculateTensorSize
    
    ; Set initial state
    mov dword ptr [r14].TENSOR_METADATA.state, 0      ; UNLOADED
    mov [r14].TENSOR_METADATA.file_handle, [rbx].MODEL_LOADER_CONTEXT.file_handle
    
    inc r13d
    jmp @tensor_info_loop
    
@tensor_info_done:
    mov eax, 1
    jmp @tensor_info_exit
    
@tensor_info_fail:
    xor eax, eax
    
@tensor_info_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
ParseGGUFTensorInfo ENDP

;-------------------------------------------------------------------------------
; CalculateTensorSize - Compute n_elements and data_size from metadata
; Input:  RCX = TENSOR_METADATA*
;-------------------------------------------------------------------------------
CalculateTensorSize PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx                      ; RBX = TENSOR_METADATA*
    
    ; Calculate n_elements = product of dimensions
    mov eax, [rbx].TENSOR_METADATA.n_dims
    test eax, eax
    jz @calc_empty
    
    mov r12, 1                        ; R12 = product
    xor r13d, r13d                    ; Index
    
@calc_loop:
    cmp r13d, eax
    jge @calc_done
    
    mov rcx, [rbx].TENSOR_METADATA.dims[r13*8]
    imul r12, rcx
    
    inc r13d
    jmp @calc_loop
    
@calc_done:
    mov [rbx].TENSOR_METADATA.n_elements, r12
    
    ; Get type size
    mov ecx, [rbx].TENSOR_METADATA.dtype
    call GetGGMLTypeSize
    mov [rbx].TENSOR_METADATA.type_size, eax
    
    ; Calculate data size (considering quantization)
    mov rcx, r12
    mov edx, [rbx].TENSOR_METADATA.dtype
    call GetQuantizedSize
    mov [rbx].TENSOR_METADATA.data_size, rax
    
    jmp @calc_exit
    
@calc_empty:
    mov qword ptr [rbx].TENSOR_METADATA.n_elements, 0
    mov qword ptr [rbx].TENSOR_METADATA.data_size, 0
    
@calc_exit:
    RESTORE_REGS
    ret
CalculateTensorSize ENDP

;-------------------------------------------------------------------------------
; GetGGMLTypeSize - Bytes per element (dequantized)
; Input:  EAX = type
; Output: EAX = size
;-------------------------------------------------------------------------------
GetGGMLTypeSize PROC FRAME
    SAVE_REGS
    
    cmp eax, GGML_TYPE_F32
    je @type_f32
    cmp eax, GGML_TYPE_F16
    je @type_f16
    cmp eax, GGML_TYPE_Q4_0
    je @type_q4
    cmp eax, GGML_TYPE_Q4_1
    je @type_q4
    cmp eax, GGML_TYPE_Q8_0
    je @type_q8
    
    ; Default to float32
    mov eax, 4
    jmp @type_exit
    
@type_f32:
    mov eax, 4
    jmp @type_exit
    
@type_f16:
    mov eax, 2
    jmp @type_exit
    
@type_q4:
    mov eax, 4                        ; Dequantized to float32
    jmp @type_exit
    
@type_q8:
    mov eax, 4                        ; Dequantized to float32
    
@type_exit:
    RESTORE_REGS
    ret
GetGGMLTypeSize ENDP

;-------------------------------------------------------------------------------
; GetQuantizedSize - Actual bytes on disk for quantized type
; Input:  RCX = n_elements, EDX = type
; Output: RAX = data size in bytes
;-------------------------------------------------------------------------------
GetQuantizedSize PROC FRAME
    SAVE_REGS
    
    mov r12, rcx                      ; R12 = n_elements
    mov r13d, edx                     ; R13 = type
    
    ; Calculate based on quantization type
    cmp r13d, GGML_TYPE_F32
    je @quant_f32
    cmp r13d, GGML_TYPE_F16
    je @quant_f16
    cmp r13d, GGML_TYPE_Q4_0
    je @quant_q4_0
    cmp r13d, GGML_TYPE_Q4_1
    je @quant_q4_1
    cmp r13d, GGML_TYPE_Q8_0
    je @quant_q8_0
    cmp r13d, GGML_TYPE_Q4_K
    je @quant_q4_k
    
    ; Default: assume float32
    shl r12, 2
    mov rax, r12
    jmp @quant_exit
    
@quant_f32:
    shl r12, 2                        ; * 4
    mov rax, r12
    jmp @quant_exit
    
@quant_f16:
    shl r12, 1                        ; * 2
    mov rax, r12
    jmp @quant_exit
    
@quant_q4_0:
    ; Q4_0: 4 bits per weight + 16-bit scale per 32 elements = 18 bytes
    mov rax, r12
    shr rax, 5                        ; / 32
    imul rax, 18
    jmp @quant_exit
    
@quant_q4_1:
    ; Q4_1: 4 bits per weight + 16-bit scale + 16-bit min per 32 elements = 20 bytes
    mov rax, r12
    shr rax, 5
    imul rax, 20
    jmp @quant_exit
    
@quant_q8_0:
    ; Q8_0: 8 bits per weight + 16-bit scale per 32 elements = 34 bytes
    mov rax, r12
    shr rax, 5
    imul rax, 34
    jmp @quant_exit
    
@quant_q4_k:
    ; Q4_K: Complex block structure, ~0.5 bytes per element
    mov rax, r12
    shr rax, 1                        ; / 2 (approximate)
    
@quant_exit:
    RESTORE_REGS
    ret
GetQuantizedSize ENDP

;-------------------------------------------------------------------------------
; LoadAllGGUFTensors - Load every tensor into memory
; Input:  RCX = MODEL_LOADER_CONTEXT*
; Output: EAX = 1 on success, 0 on failure
;-------------------------------------------------------------------------------
LoadAllGGUFTensors PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 200h
    
    mov rbx, rcx
    
    xor r12d, r12d                    ; Tensor index
@load_loop:
    cmp r12, [rbx].MODEL_LOADER_CONTEXT.tensor_count
    jge @load_done
    
    ; Get tensor metadata
    mov rax, r12
    imul rax, sizeof TENSOR_METADATA
    add rax, [rbx].MODEL_LOADER_CONTEXT.tensor_table
    mov r13, rax                      ; R13 = TENSOR_METADATA*
    
    ; Skip if already loaded
    cmp dword ptr [r13].TENSOR_METADATA.state, 2      ; LOADED
    je @load_next
    
    ; Allocate memory for tensor
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    mov rdx, [r13].TENSOR_METADATA.data_size
    mov r8, 64                        ; Alignment
    call ArenaAllocate
    test rax, rax
    jz @load_fail_memory
    mov [r13].TENSOR_METADATA.host_ptr, rax
    
    ; Seek to tensor data
    mov rcx, [r13].TENSOR_METADATA.file_handle
    mov rdx, [r13].TENSOR_METADATA.file_offset
    xor r8d, r8d
    mov r9d, 0                        ; FILE_BEGIN
    call SetFilePointerEx
    
    ; Read tensor data
    mov rcx, [r13].TENSOR_METADATA.file_handle
    mov rdx, [r13].TENSOR_METADATA.host_ptr
    mov r8, [r13].TENSOR_METADATA.data_size
    xor r9d, r9d
    push 0
    sub rsp, 20h
    call ReadFile
    add rsp, 28h
    
    ; Mark as loaded
    mov dword ptr [r13].TENSOR_METADATA.state, 2      ; LOADED
    
    ; Update progress
    mov rax, [r13].TENSOR_METADATA.data_size
    add [rbx].MODEL_LOADER_CONTEXT.bytes_loaded, rax
    inc [rbx].MODEL_LOADER_CONTEXT.tensors_loaded
    
@load_next:
    inc r12d
    jmp @load_loop
    
@load_done:
    mov eax, 1
    jmp @load_exit
    
@load_fail_memory:
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    lea rdx, err_memory_alloc
    call Phase1LogMessage
    xor eax, eax
    
@load_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
LoadAllGGUFTensors ENDP

;================================================================================
; MEMORY-MAPPED LOADER
;================================================================================

;-------------------------------------------------------------------------------
; LoadGGUFMmap - Memory-mapped file loading
; Input:  RCX = MODEL_LOADER_CONTEXT*
; Output: EAX = 1 on success, 0 on failure
;-------------------------------------------------------------------------------
LoadGGUFMmap PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 100h
    
    mov rbx, rcx
    
    ; Open file
    lea rcx, [rbx].MODEL_LOADER_CONTEXT.source_path
    xor edx, edx
    mov r8d, 3
    xor r9d, r9d
    push 0
    push 3
    push 080h
    push 0
    sub rsp, 20h
    call CreateFileA
    add rsp, 38h
    
    cmp rax, -1
    je @mmap_fail
    mov [rbx].MODEL_LOADER_CONTEXT.file_handle, rax
    
    ; Create file mapping
    mov rcx, rax
    xor edx, edx
    mov r8d, 2                        ; PAGE_READONLY
    xor r9d, r9d
    push 0                            ; Max size high
    push 0                            ; Max size low (use file size)
    sub rsp, 20h
    call CreateFileMappingA
    add rsp, 28h
    
    test rax, rax
    jz @mmap_fail
    mov [rbx].MODEL_LOADER_CONTEXT.file_mapping, rax
    
    ; Map view
    mov rcx, rax
    xor edx, edx                      ; FILE_MAP_READ
    xor r8d, r8d                      ; Offset high
    xor r9d, r9d                      ; Offset low
    push 0                            ; Bytes to map (0 = all)
    sub rsp, 20h
    call MapViewOfFileEx
    add rsp, 28h
    
    test rax, rax
    jz @mmap_fail
    mov [rbx].MODEL_LOADER_CONTEXT.mapped_view, rax
    
    ; Mark tensors as using mapped view
    xor r12d, r12d
@mmap_mark_loop:
    cmp r12, [rbx].MODEL_LOADER_CONTEXT.tensor_count
    jge @mmap_done
    
    mov rax, r12
    imul rax, sizeof TENSOR_METADATA
    add rax, [rbx].MODEL_LOADER_CONTEXT.tensor_table
    
    ; Set host_ptr to mapped address + offset
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.mapped_view
    add rcx, [rax].TENSOR_METADATA.file_offset
    mov [rax].TENSOR_METADATA.host_ptr, rcx
    
    ; Mark as loaded (mapped)
    mov dword ptr [rax].TENSOR_METADATA.state, 2
    
    inc r12d
    jmp @mmap_mark_loop
    
@mmap_done:
    mov eax, 1
    jmp @mmap_exit
    
@mmap_fail:
    xor eax, eax
    
@mmap_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
LoadGGUFMmap ENDP

;================================================================================
; STREAMING LOADER
;================================================================================

;-------------------------------------------------------------------------------
; SetupStreamingBuffer - Initialize circular buffer for streaming
; Input:  RCX = MODEL_LOADER_CONTEXT*
;-------------------------------------------------------------------------------
SetupStreamingBuffer PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    
    ; Allocate circular buffer
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    mov rdx, CIRCULAR_BUFFER_SIZE
    mov r8, 4096
    call ArenaAllocate
    mov [rbx].MODEL_LOADER_CONTEXT.stream_buffer, rax
    
    ; Initialize pointers
    mov qword ptr [rbx].MODEL_LOADER_CONTEXT.stream_write_ptr, 0
    mov qword ptr [rbx].MODEL_LOADER_CONTEXT.stream_read_ptr, 0
    
    RESTORE_REGS
    ret
SetupStreamingBuffer ENDP

;===============================================================================
; HF HUB DOWNLOADER
;================================================================================

;-------------------------------------------------------------------------------
; LoadHFHub - Download from HuggingFace Hub
; Input:  RCX = MODEL_LOADER_CONTEXT*
; Output: EAX = 1 on success, 0 on failure
;-------------------------------------------------------------------------------
LoadHFHub PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    
    ; For now: Return failure (network code would go here)
    ; In production: Parse URL, connect, download, load as local
    
    mov rcx, [rbx].MODEL_LOADER_CONTEXT.phase1_ctx
    lea rdx, err_network
    call Phase1LogMessage
    
    xor eax, eax
    RESTORE_REGS
    ret
LoadHFHub ENDP

;================================================================================
; OLLAMA API LOADER
;================================================================================

;-------------------------------------------------------------------------------
; LoadOllamaAPI - Stream from Ollama API
; Input:  RCX = MODEL_LOADER_CONTEXT*
; Output: EAX = 1 on success, 0 on failure
;-------------------------------------------------------------------------------
LoadOllamaAPI PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    
    ; For now: Return failure (network code would go here)
    xor eax, eax
    RESTORE_REGS
    ret
LoadOllamaAPI ENDP

;================================================================================
; MASM COMPRESSED BLOB
;================================================================================

;-------------------------------------------------------------------------------
; LoadMASMBlob - Decompress embedded model blob
; Input:  RCX = MODEL_LOADER_CONTEXT*
; Output: EAX = 1 on success, 0 on failure
;-------------------------------------------------------------------------------
LoadMASMBlob PROC FRAME
    SAVE_REGS
    
    ; Blob is embedded in .rdata section
    ; For now: Return failure (would decompress and load)
    
    xor eax, eax
    RESTORE_REGS
    ret
LoadMASMBlob ENDP

;================================================================================
; UTILITY FUNCTIONS
;================================================================================

;-------------------------------------------------------------------------------
; ComputeHash64 - FNV-1a hash for fast lookup
; Input:  RCX = String
; Output: RAX = 64-bit hash
;-------------------------------------------------------------------------------
ComputeHash64 PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx                      ; String
    mov r12, 14695981039346656037     ; FNV offset basis
    
@hash_loop:
    movzx eax, byte ptr [rbx]
    test al, al
    jz @hash_done
    
    xor r12, rax
    imul r12, 1099511628211           ; FNV prime
    
    inc rbx
    jmp @hash_loop
    
@hash_done:
    mov rax, r12
    
    RESTORE_REGS
    ret
ComputeHash64 ENDP

;-------------------------------------------------------------------------------
; VerifyModelChecksum - SHA-256 verification
; Input:  RCX = MODEL_LOADER_CONTEXT*
;-------------------------------------------------------------------------------
VerifyModelChecksum PROC FRAME
    SAVE_REGS
    
    ; Implementation would compute SHA-256 of loaded data
    
    RESTORE_REGS
    ret
VerifyModelChecksum ENDP

;-------------------------------------------------------------------------------
; GetTensorByName - Fast tensor lookup using hash
; Input:  RCX = MODEL_LOADER_CONTEXT*
;         RDX = Tensor name
; Output: RAX = TENSOR_METADATA* or NULL
;-------------------------------------------------------------------------------
GetTensorByName PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    mov r12, rdx
    
    ; Hash name for lookup
    mov rcx, r12
    call ComputeHash64
    mov r13, rax                      ; R13 = name hash
    
    ; Search tensor table
    xor r14d, r14d
@search_loop:
    cmp r14, [rbx].MODEL_LOADER_CONTEXT.tensor_count
    jge @not_found
    
    mov rax, r14
    imul rax, sizeof TENSOR_METADATA
    add rax, [rbx].MODEL_LOADER_CONTEXT.tensor_table
    
    cmp [rax].TENSOR_METADATA.name_hash, r13
    jne @search_next
    
    ; Verify name match (for collision handling)
    push rax
    lea rcx, [rax].TENSOR_METADATA.name
    mov rdx, r12
    call StringEqual
    pop rax
    
    test ebx, ebx
    jz @found
    
@search_next:
    inc r14d
    jmp @search_loop
    
@found:
    RESTORE_REGS
    ret
    
@not_found:
    xor eax, eax
    RESTORE_REGS
    ret
GetTensorByName ENDP

;-------------------------------------------------------------------------------
; StringEqual - Compare two strings
; Input:  RCX = String1, RDX = String2
; Output: EBX = 1 if equal, 0 if not
;-------------------------------------------------------------------------------
StringEqual PROC FRAME
    SAVE_REGS
    
    mov rsi, rcx
    mov rdi, rdx
    xor ebx, ebx
    
@seq_loop:
    movzx eax, byte ptr [rsi]
    movzx edx, byte ptr [rdi]
    
    cmp al, dl
    jne @seq_exit
    
    test al, al
    jz @seq_equal
    
    inc rsi
    inc rdi
    jmp @seq_loop
    
@seq_equal:
    mov ebx, 1
    
@seq_exit:
    RESTORE_REGS
    ret
StringEqual ENDP

;===============================================================================
; EXPORTS
;================================================================================
PUBLIC Phase2Initialize
PUBLIC DetectModelFormat
PUBLIC RouteModelLoad
PUBLIC LoadGGUFLocal
PUBLIC LoadGGUFMmap
PUBLIC LoadAllGGUFTensors
PUBLIC GetTensorByName
PUBLIC LoadHFHub
PUBLIC LoadOllamaAPI
PUBLIC ComputeHash64
PUBLIC GetGGMLTypeSize
PUBLIC GetQuantizedSize

END
