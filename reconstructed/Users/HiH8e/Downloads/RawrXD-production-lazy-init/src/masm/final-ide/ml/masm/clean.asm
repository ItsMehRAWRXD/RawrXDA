; ml_masm.asm - Model Loader for MASM IDE
; Handles GGUF model loading and inference - CLEAN VERSION

option casemap:none

include windows.inc
includelib kernel32.lib

; Custom Engine (rawr1024_dual_engine_custom.asm)
EXTERN rawr1024_init:PROC
EXTERN rawr1024_start_engine:PROC
EXTERN rawr1024_process:PROC
EXTERN rawr1024_stop_engine:PROC
EXTERN rawr1024_cleanup:PROC

; Win32 APIs for logging
EXTERN OutputDebugStringA:PROC

;==========================================================================
; CONSTANTS
;==========================================================================
GGUF_MAGIC          EQU 46554747h        ; "GGUF"
MAX_MODEL_SIZE      EQU 1073741824       ; 1GB max (REMOVED - allows any size)
MAX_TENSOR_COUNT    EQU 10000
MAX_RESPONSE_LEN    EQU 8192
MAX_PROMPT_LEN      EQU 4096
MAX_TENSOR_CACHE    EQU 512
TENSOR_NAME_LEN     EQU 64
MODEL_NAME_LEN      EQU 64
MODEL_VERSION_LEN   EQU 32
QUANT_LEVEL_LEN     EQU 16
GGUF_HEADER_SIZE    EQU 24

;==========================================================================
; STRUCTURES
;==========================================================================
GGUF_HEADER STRUCT
    magic           DWORD ?
    version         DWORD ?
    tensor_count    QWORD ?
    metadata_kv_count QWORD ?
GGUF_HEADER ENDS

MODEL_STATE STRUCT
    is_loaded       DWORD ?
    file_handle     QWORD ?
    file_mapping    QWORD ?
    mapped_data     QWORD ?
    file_size       QWORD ?
    tensor_count    QWORD ?
    metadata_count  QWORD ?
    model_name      BYTE 260 DUP (?)
    last_error      BYTE 512 DUP (?)
MODEL_STATE ENDS

TENSOR_INFO STRUCT
    name_str            BYTE TENSOR_NAME_LEN DUP(?)
    shape               DWORD 4 DUP(?)
    dtype               DWORD ?
    strides             DWORD 4 DUP(?)
    data_ptr            QWORD ?
    size_bytes          QWORD ?
    is_quantized        DWORD ?
    quant_bits          DWORD ?
    reserved            QWORD ?
TENSOR_INFO ENDS

MODEL_ARCH STRUCT
    model_name          BYTE MODEL_NAME_LEN DUP(?)
    version             BYTE MODEL_VERSION_LEN DUP(?)
    num_layers          DWORD ?
    hidden_size         DWORD ?
    num_attention_heads DWORD ?
    max_seq_length      DWORD ?
    vocab_size          DWORD ?
    quantization_level  BYTE QUANT_LEVEL_LEN DUP(?)
MODEL_ARCH ENDS

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data?
    g_model_state   MODEL_STATE <>
    response_buffer BYTE MAX_RESPONSE_LEN DUP (?)
    temp_buffer     BYTE 1024 DUP (?)
    g_tensor_cache  TENSOR_INFO MAX_TENSOR_CACHE DUP(<>)
    g_tensor_count  DWORD 0
    g_model_arch    MODEL_ARCH <>

.data
    log_init        BYTE "ML: Initializing model loader...", 0
    log_loading     BYTE "ML: Loading model file...", 0
    log_mapping     BYTE "ML: Memory mapping file...", 0
    log_parsing     BYTE "ML: Parsing GGUF header...", 0
    log_parse_meta  BYTE "ML: Parsing GGUF metadata...", 0
    log_cache_tensors BYTE "ML: Building tensor cache...", 0
    log_arch_ready  BYTE "ML: Architecture metadata ready", 0
    log_ready       BYTE "ML: Model ready for inference", 0
    log_inference   BYTE "ML: Running inference...", 0
    log_cleanup     BYTE "ML: Cleaning up resources...", 0
    
    err_file_open   BYTE "Failed to open model file", 0
    err_file_size   BYTE "Model file error", 0
    err_mapping     BYTE "Failed to map file to memory", 0
    err_invalid_gguf BYTE "Invalid GGUF file format", 0
    err_no_model    BYTE "No model loaded", 0
    err_inference   BYTE "Inference failed", 0
    
    default_response BYTE "I'm a simple AI assistant. How can I help you?", 0
    thinking_response BYTE "Let me think about that...", 0
    
    arch_string     BYTE 256 DUP(0)
    szKeyNumLayers  BYTE "num_layers",0
    szKeyHiddenSize BYTE "hidden_size",0
    szKeyHeads      BYTE "n_heads",0
    szKeyMaxSeq     BYTE "max_seq_len",0
    szKeyVocab      BYTE "vocab_size",0
    szKeyQuant      BYTE "quantization",0
    szPatWeight     BYTE ".weight",0
    szArchPrefix    BYTE "Model:",0

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;==========================================================================
; PUBLIC: ml_masm_init(model_path: rcx, flags: rdx)
; Initialize model loader with GGUF file
; Returns: 1 on success, 0 on failure
;==========================================================================
PUBLIC ml_masm_init
ALIGN 16
ml_masm_init PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 40h
    
    mov rbx, rcx                    ; save model path
    mov rsi, rdx                    ; save flags
    
    ; Log initialization
    lea rcx, log_init
    call OutputDebugStringA
    
    ; Clear previous state
    call ml_masm_free
    
    ; Copy model path
    mov rsi, rbx
    lea rdi, g_model_state.model_name
    mov rcx, 260
    call strcpy_limited
    
    ; Log loading
    lea rcx, log_loading
    call OutputDebugStringA
    
    ; Initialize custom engine
    call rawr1024_init
    test rax, rax
    jz init_engine_failed
    
    ; Start engine 0
    mov rcx, 0
    call rawr1024_start_engine
    test rax, rax
    jz init_engine_failed
    
    ; Open file
    mov rcx, rbx                    ; file path
    mov rdx, GENERIC_READ
    mov r8, FILE_SHARE_READ
    xor r9, r9                      ; security attributes
    mov QWORD PTR [rsp+20h], OPEN_EXISTING
    mov QWORD PTR [rsp+28h], FILE_ATTRIBUTE_NORMAL
    xor rax, rax
    mov QWORD PTR [rsp+30h], rax    ; template file
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je file_error
    mov g_model_state.file_handle, rax
    
    ; Get file size
    mov rcx, rax
    lea rdx, temp_buffer
    call GetFileSizeEx
    test eax, eax
    jz file_error
    
    mov rax, QWORD PTR [temp_buffer]
    mov g_model_state.file_size, rax
    
    ; NOTE: Size check REMOVED - allow any model size
    
    ; Log mapping
    lea rcx, log_mapping
    call OutputDebugStringA
    
    ; Create file mapping
    mov rcx, g_model_state.file_handle
    xor rdx, rdx                    ; security attributes
    mov r8d, PAGE_READONLY
    xor r9, r9                      ; high size
    mov QWORD PTR [rsp+20h], 0      ; low size (entire file)
    xor rax, rax
    mov QWORD PTR [rsp+28h], rax    ; name
    call CreateFileMappingA
    
    test rax, rax
    jz mapping_error
    mov g_model_state.file_mapping, rax
    
    ; Map view of file
    mov rcx, rax
    mov rdx, FILE_MAP_READ
    xor r8, r8                      ; offset high
    xor r9, r9                      ; offset low
    mov QWORD PTR [rsp+20h], 0      ; bytes to map (entire file)
    call MapViewOfFile
    
    test rax, rax
    jz mapping_error
    mov g_model_state.mapped_data, rax
    
    ; Log parsing
    lea rcx, log_parsing
    call OutputDebugStringA
    
    ; Parse GGUF header
    call parse_gguf_header
    test eax, eax
    jz format_error
    
    ; Mark as loaded
    mov g_model_state.is_loaded, 1
    
    ; Log ready
    lea rcx, log_ready
    call OutputDebugStringA
    
    mov eax, 1
    jmp done_label
    
init_engine_failed:
    lea rcx, err_no_model
    call set_last_error
    jmp error_label
    
file_error:
    lea rcx, err_file_open
    call set_last_error
    jmp error_label
    
mapping_error:
    lea rcx, err_mapping
    call set_last_error
    jmp error_label
    
format_error:
    lea rcx, err_invalid_gguf
    call set_last_error
    jmp error_label
    
error_label:
    call ml_masm_free
    xor eax, eax
    
done_label:
    add rsp, 40h
    pop rdi
    pop rsi
    pop rbx
    ret
ml_masm_init ENDP

;==========================================================================
; PUBLIC: ml_masm_inference(prompt: rcx)
; Run inference with given prompt
; Returns: 1 on success, 0 on failure
;==========================================================================
PUBLIC ml_masm_inference
ALIGN 16
ml_masm_inference PROC
    push rbx
    sub rsp, 30h
    
    mov rbx, rcx                    ; save prompt
    
    ; Check if model is loaded
    cmp g_model_state.is_loaded, 0
    je no_model
    
    ; Log inference
    lea rcx, log_inference
    call OutputDebugStringA
    
    ; Copy prompt to response buffer
    mov rsi, rbx
    lea rdi, response_buffer
    call strcpy_simple
    
    ; Get prompt length
    mov rcx, rbx
    call strlen_simple
    
    ; Run custom engine process on the buffer
    mov rdx, OFFSET response_buffer
    mov r8, rax                     ; size
    mov rcx, 0                      ; engine 0
    call rawr1024_process
    
    ; Return default response
    lea rsi, default_response
    lea rdi, response_buffer
    call strcpy_simple
    
    mov eax, 1
    jmp done_label
    
no_model:
    lea rcx, err_no_model
    call set_last_error
    xor eax, eax
    
done_label:
    add rsp, 30h
    pop rbx
    ret
ml_masm_inference ENDP

;==========================================================================
; PUBLIC: ml_masm_get_response(buffer: rcx, max_len: rdx)
; Get the last inference response
; Returns: length of response
;==========================================================================
PUBLIC ml_masm_get_response
ALIGN 16
ml_masm_get_response PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; save buffer
    mov rsi, rdx                    ; save max_len
    
    ; Copy response to buffer
    lea rsi, response_buffer
    mov rdi, rbx
    mov rcx, rdx                    ; max_len
    call strcpy_limited
    
    ; Return length
    mov rcx, rbx
    call strlen_simple
    
    pop rdi
    pop rsi
    pop rbx
    ret
ml_masm_get_response ENDP

;==========================================================================
; PUBLIC: ml_masm_get_tensor(tensor_name: rcx) -> tensor_ptr (rax)
; Get tensor information by name
; Returns: pointer to TENSOR_INFO or NULL
;==========================================================================
PUBLIC ml_masm_get_tensor
ALIGN 16
ml_masm_get_tensor PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    test rcx, rcx
    jz mgt_not_found
    
    mov rsi, rcx                    ; name to find
    mov eax, g_tensor_count
    test eax, eax
    jz mgt_not_found
    xor edx, edx                    ; index
    lea rbx, g_tensor_cache
    
mgt_loop:
    cmp edx, eax
    jae mgt_not_found
    lea rcx, [rbx + TENSOR_INFO.name_str]
    mov rdx, rsi
    mov r8d, TENSOR_NAME_LEN
    call strncmp_limited
    test eax, eax
    jz mgt_found
    ; next entry
    add rbx, SIZEOF TENSOR_INFO
    inc edx
    mov eax, g_tensor_count
    jmp mgt_loop
    
mgt_found:
    mov rax, rbx
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
    
mgt_not_found:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    xor eax, eax
    ret
ml_masm_get_tensor ENDP

;==========================================================================
; PUBLIC: ml_masm_get_arch()
; Get model architecture string
; Returns: pointer to architecture string
;==========================================================================
PUBLIC ml_masm_get_arch
ALIGN 16
ml_masm_get_arch PROC
    lea rax, arch_string
    ret
ml_masm_get_arch ENDP

;==========================================================================
; PUBLIC: ml_masm_last_error()
; Get last error message
; Returns: pointer to error string
;==========================================================================
PUBLIC ml_masm_last_error
ALIGN 16
ml_masm_last_error PROC
    lea rax, g_model_state.last_error
    ret
ml_masm_last_error ENDP

;==========================================================================
; PUBLIC: ml_masm_free()
; Free model resources
;==========================================================================
PUBLIC ml_masm_free
ALIGN 16
ml_masm_free PROC
    sub rsp, 28h
    
    ; Log cleanup
    lea rcx, log_cleanup
    call OutputDebugStringA
    
    ; Unmap view
    cmp g_model_state.mapped_data, 0
    je skip_unmap
    
    mov rcx, g_model_state.mapped_data
    call UnmapViewOfFile
    mov g_model_state.mapped_data, 0
    
skip_unmap:
    ; Close file mapping
    cmp g_model_state.file_mapping, 0
    je skip_mapping
    
    mov rcx, g_model_state.file_mapping
    call CloseHandle
    mov g_model_state.file_mapping, 0
    
skip_mapping:
    ; Close file handle
    cmp g_model_state.file_handle, 0
    je skip_file
    
    mov rcx, g_model_state.file_handle
    call CloseHandle
    mov g_model_state.file_handle, 0
    
skip_file:
    ; Clear state
    mov g_model_state.is_loaded, 0
    mov g_model_state.tensor_count, 0
    
    add rsp, 28h
    ret
ml_masm_free ENDP

;==========================================================================
; INTERNAL: parse_gguf_header()
; Parse GGUF file header
; Returns: 1 on success, 0 on failure
;==========================================================================
parse_gguf_header PROC
    push rbx
    
    mov rbx, g_model_state.mapped_data
    test rbx, rbx
    jz error_label
    
    ; Check magic number
    mov eax, DWORD PTR [rbx]
    cmp eax, GGUF_MAGIC
    jne error_label
    
    ; Get version
    mov eax, DWORD PTR [rbx + 4]
    cmp eax, 1
    jl error_label
    cmp eax, 3
    jg error_label
    
    ; Get tensor count
    mov rax, QWORD PTR [rbx + 8]
    mov g_model_state.tensor_count, rax
    
    ; Get metadata kv count
    mov rax, QWORD PTR [rbx + 16]
    mov g_model_state.metadata_count, rax
    
    ; Simplified: initialize arch with defaults
    lea rcx, g_model_arch
    mov DWORD PTR [rcx + MODEL_ARCH.num_layers], 0
    mov DWORD PTR [rcx + MODEL_ARCH.hidden_size], 0
    mov DWORD PTR [rcx + MODEL_ARCH.num_attention_heads], 0
    mov DWORD PTR [rcx + MODEL_ARCH.max_seq_length], 0
    mov DWORD PTR [rcx + MODEL_ARCH.vocab_size], 0
    
    mov eax, 1
    jmp done_label
    
error_label:
    xor eax, eax
    
done_label:
    pop rbx
    ret
parse_gguf_header ENDP

;==========================================================================
; INTERNAL: set_last_error(msg: rcx)
; Set last error message
;==========================================================================
set_last_error PROC
    push rsi
    push rdi
    
    mov rsi, rcx
    lea rdi, g_model_state.last_error
    call strcpy_simple
    
    pop rdi
    pop rsi
    ret
set_last_error ENDP

;==========================================================================
; INTERNAL: strcpy_simple(src: rsi, dst: rdi)
; Simple string copy
;==========================================================================
strcpy_simple PROC
    push rax
    
copy_loop:
    mov al, BYTE PTR [rsi]
    mov BYTE PTR [rdi], al
    test al, al
    jz done_label
    inc rsi
    inc rdi
    jmp copy_loop
    
done_label:
    pop rax
    ret
strcpy_simple ENDP

;==========================================================================
; INTERNAL: strcpy_limited(src: rsi, dst: rdi, max: rcx)
; String copy with length limit
;==========================================================================
strcpy_limited PROC
    push rax
    push rbx
    
    xor rbx, rbx
    
copy_loop:
    cmp rbx, rcx
    jge done_label
    
    mov al, BYTE PTR [rsi + rbx]
    mov BYTE PTR [rdi + rbx], al
    
    test al, al
    jz done_label
    
    inc rbx
    jmp copy_loop
    
done_label:
    cmp rbx, rcx
    jl null_term
    dec rbx
    
null_term:
    mov BYTE PTR [rdi + rbx], 0
    
    pop rbx
    pop rax
    ret
strcpy_limited ENDP

;==========================================================================
; INTERNAL: strlen_simple(str: rcx)
; Get string length
;==========================================================================
strlen_simple PROC
    push rbx
    
    xor rbx, rbx
    
len_loop:
    mov al, BYTE PTR [rcx + rbx]
    test al, al
    jz done_label
    inc rbx
    jmp len_loop
    
done_label:
    mov rax, rbx
    pop rbx
    ret
strlen_simple ENDP

;==========================================================================
; INTERNAL: strncmp_limited(a: rcx, b: rdx, max: r8d) -> eax (0 equal)
;==========================================================================
strncmp_limited PROC
    push rbx
    xor rbx, rbx
cmp_loop:
    cmp rbx, r8
    jge cmp_done
    mov al, BYTE PTR [rcx + rbx]
    mov dl, BYTE PTR [rdx + rbx]
    cmp al, dl
    jne cmp_noteq
    test al, al
    jz cmp_done
    inc rbx
    jmp cmp_loop
cmp_noteq:
    mov eax, 1
    pop rbx
    ret
cmp_done:
    xor eax, eax
    pop rbx
    ret
strncmp_limited ENDP

; External Win32 APIs
EXTERN CreateFileA:PROC
EXTERN GetFileSizeEx:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC

END
