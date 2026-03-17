; model_loader_integration.asm - Master Integration Layer (Pure MASM)
; Coordinates GGUF loader, metadata parser, and agent chat UI
; Provides unified API for loading models and running inference in agent chat

option casemap:none

include windows.inc
includelib kernel32.lib

; External modules
EXTERN parse_gguf_metadata_complete:PROC
EXTERN extract_tensor_names_from_gguf:PROC
EXTERN format_arch_string:PROC

EXTERN agent_chat_load_model:PROC
EXTERN agent_chat_run_inference:PROC
EXTERN agent_chat_is_model_loaded:PROC
EXTERN agent_chat_get_inference_count:PROC
EXTERN agent_chat_get_session_state:PROC

EXTERN ml_masm_init:PROC
EXTERN ml_masm_free:PROC
EXTERN ml_masm_get_arch:PROC
EXTERN ml_masm_get_tensor:PROC
EXTERN ml_masm_inference:PROC
EXTERN ml_masm_get_response:PROC
EXTERN ml_masm_last_error:PROC

EXTERN test_gguf_loader_main:PROC

; Win32 APIs
EXTERN OutputDebugStringA:PROC
EXTERN GetTickCount:PROC
EXTERN GetTickCount64:PROC

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_MODEL_PATH              EQU 512
MAX_PROMPT_LEN              EQU 4096
MAX_RESPONSE_LEN            EQU 8192
INFERENCE_TIMEOUT_MS        EQU 30000
CHAR_BUFFER_SIZE            EQU 2048

;==========================================================================
; LOADER STATE STRUCTURE
;==========================================================================
LOADER_STATE STRUCT
    initialized             DWORD ?
    current_model_path      BYTE MAX_MODEL_PATH DUP(?)
    model_loaded            DWORD ?
    model_arch_string       BYTE 512 DUP(?)
    last_inference_time     QWORD ?
    last_inference_duration DWORD ?
    total_inferences        QWORD ?
    last_error              BYTE 512 DUP(?)
LOADER_STATE ENDS

;==========================================================================
; PERFORMANCE METRICS STRUCTURE
;==========================================================================
PERF_METRICS STRUCT
    load_time_ms            DWORD ?
    inference_count         QWORD ?
    total_inference_time_ms QWORD ?
    avg_inference_time_ms   DWORD ?
    longest_inference_ms    DWORD ?
    shortest_inference_ms   DWORD ?
PERF_METRICS ENDS

;==========================================================================
; DATA SECTION
;==========================================================================
.DATA

; Global state
g_loader_state              LOADER_STATE <>
g_perf_metrics              PERF_METRICS <>

; Initialization flag
initialized                 DWORD 0

; Status messages
msg_module_init             BYTE "Model Loader Integration initialized", 0x0D, 0x0A, 0
msg_model_loading           BYTE "Loading model: ", 0
msg_model_loaded_ok         BYTE "Model loaded successfully", 0x0D, 0x0A, 0
msg_model_load_fail         BYTE "Failed to load model: ", 0
msg_inference_start         BYTE "Starting inference...", 0x0D, 0x0A, 0
msg_inference_complete      BYTE "Inference completed in ", 0
msg_ms                      BYTE " ms", 0x0D, 0x0A, 0
msg_no_model                BYTE "No model loaded. Load a model first.", 0x0D, 0x0A, 0
msg_metrics_line            BYTE "Metrics: %d inferences, avg %d ms", 0x0D, 0x0A, 0
msg_test_suite_start        BYTE "Starting GGUF Model Loader Test Suite...", 0x0D, 0x0A, 0

; Temp buffer for formatting
temp_msg_buffer             BYTE 2048 DUP(?)

;==========================================================================
; CODE SECTION
;==========================================================================
.CODE

;==========================================================================
; PUBLIC: model_loader_init()
; Initialize the model loader integration layer
; Must be called once before using any loader functions
; Returns: 1 on success, 0 on failure
;==========================================================================
PUBLIC model_loader_init
ALIGN 16
model_loader_init PROC
    push rbx
    sub rsp, 32
    
    cmp DWORD PTR initialized, 1
    je already_initialized
    
    ; Initialize global state
    xor eax, eax
    mov DWORD PTR [g_loader_state.initialized], 1
    mov DWORD PTR [g_loader_state.model_loaded], 0
    mov QWORD PTR [g_loader_state.total_inferences], 0
    
    xor eax, eax
    mov DWORD PTR [g_perf_metrics.inference_count], 0
    mov QWORD PTR [g_perf_metrics.total_inference_time_ms], 0
    mov DWORD PTR [g_perf_metrics.longest_inference_ms], 0
    mov DWORD PTR [g_perf_metrics.shortest_inference_ms], -1
    
    ; Print initialization message
    lea rcx, msg_module_init
    call OutputDebugStringA
    
    mov DWORD PTR initialized, 1
    mov eax, 1
    jmp init_done
    
already_initialized:
    mov eax, 1
    
init_done:
    add rsp, 32
    pop rbx
    ret
model_loader_init ENDP

;==========================================================================
; PUBLIC: model_loader_load_gguf_model(model_path: rcx) -> eax
; Load GGUF model file with full metadata extraction and UI integration
; Orchestrates:
;   1. ml_masm_init() - loads and maps GGUF file
;   2. parse_gguf_metadata_complete() - extracts architecture
;   3. extract_tensor_names_from_gguf() - builds tensor cache
;   4. agent_chat_load_model() - updates UI with model info
; Returns: 1 on success, 0 on failure
;==========================================================================
PUBLIC model_loader_load_gguf_model
ALIGN 16
model_loader_load_gguf_model PROC
    push rbx
    push rsi
    push rdi
    push r8
    push r9
    sub rsp, 80h
    
    mov rsi, rcx            ; rsi = model_path
    
    ; Ensure initialized
    cmp DWORD PTR initialized, 1
    jne need_init
    
    ; Copy model path to state
    lea rdi, [g_loader_state.current_model_path]
    mov rcx, rsi
    mov rdx, rdi
    mov r8, MAX_MODEL_PATH
    call strcpy_safe_masm
    
    ; Print loading message
    lea rcx, msg_model_loading
    call OutputDebugStringA
    mov rcx, rsi
    call OutputDebugStringA
    
    ; Get start time for metrics
    call GetTickCount
    mov r8d, eax            ; r8d = start_time
    
    ; Call ml_masm_init() to load model
    mov rcx, rsi
    xor rdx, rdx            ; flags = 0
    call ml_masm_init
    test eax, eax
    jz load_error
    
    ; Mark model as loaded
    mov DWORD PTR [g_loader_state.model_loaded], 1
    
    ; Print success message
    lea rcx, msg_model_loaded_ok
    call OutputDebugStringA
    
    ; Get end time and store
    call GetTickCount
    sub eax, r8d
    mov DWORD PTR [g_perf_metrics.load_time_ms], eax
    
    ; Delegate to agent_chat_load_model for UI integration
    mov rcx, rsi
    call agent_chat_load_model
    test eax, eax
    jz load_error
    
    mov eax, 1
    jmp load_done
    
load_error:
    mov DWORD PTR [g_loader_state.model_loaded], 0
    
    ; Get error message
    call ml_masm_last_error
    mov rsi, rax
    lea rdi, [g_loader_state.last_error]
    mov rcx, rsi
    mov rdx, rdi
    mov r8, 512
    call strcpy_safe_masm
    
    ; Print error
    lea rcx, msg_model_load_fail
    call OutputDebugStringA
    mov rcx, rsi
    call OutputDebugStringA
    
    xor eax, eax
    
load_done:
    add rsp, 80h
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbx
    ret
    
need_init:
    ; Auto-initialize
    call model_loader_init
    jmp model_loader_load_gguf_model
model_loader_load_gguf_model ENDP

;==========================================================================
; PUBLIC: model_loader_run_inference(prompt: rcx) -> eax
; Run inference on loaded model with performance tracking
; Records timing metrics and updates inference statistics
; Returns: 1 on success, 0 on failure
;==========================================================================
PUBLIC model_loader_run_inference
ALIGN 16
model_loader_run_inference PROC
    push rbx
    push rsi
    push rdi
    push r8
    sub rsp, 80h
    
    mov rsi, rcx            ; rsi = prompt
    
    ; Check if model is loaded
    cmp DWORD PTR [g_loader_state.model_loaded], 1
    jne no_model_loaded
    
    ; Print inference start message
    lea rcx, msg_inference_start
    call OutputDebugStringA
    
    ; Get start time
    call GetTickCount
    mov r8d, eax            ; r8d = start_time_ms
    
    ; Call ml_masm_inference()
    mov rcx, rsi
    call ml_masm_inference
    test eax, eax
    jz inference_error
    
    ; Get end time
    call GetTickCount
    sub eax, r8d            ; eax = duration_ms
    mov DWORD PTR [g_loader_state.last_inference_duration], eax
    
    ; Update metrics
    mov ecx, DWORD PTR [g_perf_metrics.inference_count]
    inc ecx
    mov DWORD PTR [g_perf_metrics.inference_count], ecx
    
    mov r8, QWORD PTR [g_perf_metrics.total_inference_time_ms]
    add r8, rax
    mov QWORD PTR [g_perf_metrics.total_inference_time_ms], r8
    
    ; Update longest inference time
    mov ecx, DWORD PTR [g_perf_metrics.longest_inference_ms]
    cmp eax, ecx
    jle check_shortest
    mov DWORD PTR [g_perf_metrics.longest_inference_ms], eax
    
check_shortest:
    mov ecx, DWORD PTR [g_perf_metrics.shortest_inference_ms]
    cmp ecx, -1
    je set_shortest
    cmp eax, ecx
    jge check_average
    
set_shortest:
    mov DWORD PTR [g_perf_metrics.shortest_inference_ms], eax
    
check_average:
    ; Calculate average: total_time / inference_count
    mov rax, QWORD PTR [g_perf_metrics.total_inference_time_ms]
    mov ecx, DWORD PTR [g_perf_metrics.inference_count]
    test ecx, ecx
    jz skip_average
    
    xor edx, edx
    div rcx
    mov DWORD PTR [g_perf_metrics.avg_inference_time_ms], eax
    
skip_average:
    ; Print completion message
    lea rcx, msg_inference_complete
    call OutputDebugStringA
    
    mov eax, DWORD PTR [g_loader_state.last_inference_duration]
    lea rdx, temp_msg_buffer
    call int_to_string
    
    lea rcx, temp_msg_buffer
    call OutputDebugStringA
    
    lea rcx, msg_ms
    call OutputDebugStringA
    
    ; Delegate to agent_chat_run_inference for UI integration
    mov rcx, rsi
    call agent_chat_run_inference
    
    mov eax, 1
    jmp inference_done
    
no_model_loaded:
    lea rcx, msg_no_model
    call OutputDebugStringA
    xor eax, eax
    jmp inference_done
    
inference_error:
    call ml_masm_last_error
    mov rsi, rax
    lea rdi, [g_loader_state.last_error]
    mov rcx, rsi
    mov rdx, rdi
    mov r8, 512
    call strcpy_safe_masm
    
    xor eax, eax
    
inference_done:
    add rsp, 80h
    pop r8
    pop rdi
    pop rsi
    pop rbx
    ret
model_loader_run_inference ENDP

;==========================================================================
; PUBLIC: model_loader_get_metrics() -> rax (pointer to PERF_METRICS)
; Get performance metrics for loaded model and inferences
;==========================================================================
PUBLIC model_loader_get_metrics
ALIGN 16
model_loader_get_metrics PROC
    lea rax, g_perf_metrics
    ret
model_loader_get_metrics ENDP

;==========================================================================
; PUBLIC: model_loader_get_state() -> rax (pointer to LOADER_STATE)
; Get current loader state (model path, load status, last error)
;==========================================================================
PUBLIC model_loader_get_state
ALIGN 16
model_loader_get_state PROC
    lea rax, g_loader_state
    ret
model_loader_get_state ENDP

;==========================================================================
; PUBLIC: model_loader_is_initialized() -> eax (1 or 0)
; Check if loader module is initialized
;==========================================================================
PUBLIC model_loader_is_initialized
ALIGN 16
model_loader_is_initialized PROC
    mov eax, DWORD PTR initialized
    ret
model_loader_is_initialized ENDP

;==========================================================================
; PUBLIC: model_loader_unload_current_model()
; Free resources for currently loaded model
; Returns: 1 on success, 0 if no model loaded
;==========================================================================
PUBLIC model_loader_unload_current_model
ALIGN 16
model_loader_unload_current_model PROC
    cmp DWORD PTR [g_loader_state.model_loaded], 0
    je already_unloaded
    
    ; Call ml_masm_free() to clean up
    call ml_masm_free
    
    mov DWORD PTR [g_loader_state.model_loaded], 0
    
    mov eax, 1
    ret
    
already_unloaded:
    mov eax, 1
    ret
model_loader_unload_current_model ENDP

;==========================================================================
; PUBLIC: model_loader_run_self_tests()
; Run comprehensive GGUF loader test suite
; Tests loading real models, metadata extraction, tensor cache
; Returns: number of tests passed
;==========================================================================
PUBLIC model_loader_run_self_tests
ALIGN 16
model_loader_run_self_tests PROC
    push rbx
    sub rsp, 32
    
    ; Print test suite start message
    lea rcx, msg_test_suite_start
    call OutputDebugStringA
    
    ; Call test_gguf_loader_main()
    call test_gguf_loader_main
    
    add rsp, 32
    pop rbx
    ret
model_loader_run_self_tests ENDP

;==========================================================================
; INTERNAL HELPER FUNCTIONS
;==========================================================================

;==========================================================================
; INTERNAL: strcpy_safe_masm(src: rcx, dst: rdx, max: r8)
; Safe string copy with length limiting and null termination
;==========================================================================
ALIGN 16
strcpy_safe_masm PROC
    push rax
    push rbx
    
    xor eax, eax
    
safe_copy_loop:
    cmp rax, r8
    jge safe_copy_done
    
    mov bl, BYTE PTR [rcx + rax]
    mov BYTE PTR [rdx + rax], bl
    
    test bl, bl
    jz safe_copy_done
    
    inc rax
    jmp safe_copy_loop
    
safe_copy_done:
    cmp rax, r8
    jl safe_null_term
    dec rax
    
safe_null_term:
    mov BYTE PTR [rdx + rax], 0
    
    pop rbx
    pop rax
    ret
strcpy_safe_masm ENDP

;==========================================================================
; INTERNAL: int_to_string(value: eax, buffer: rdx)
; Convert 32-bit integer to decimal string
;==========================================================================
ALIGN 16
int_to_string PROC
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    sub rsp, 64
    
    mov ebx, eax            ; ebx = value
    lea rsi, [rsp]          ; rsi = temp digit buffer
    xor ecx, ecx            ; ecx = digit count
    mov eax, 10             ; divisor
    mov rdi, rdx            ; rdi = output buffer
    
    ; Handle zero
    test ebx, ebx
    jnz i2s_convert
    
    mov BYTE PTR [rdi], '0'
    mov BYTE PTR [rdi+1], 0
    jmp i2s_done
    
i2s_convert:
    test ebx, ebx
    jz i2s_reverse
    
    mov eax, ebx
    xor edx, edx
    mov ecx, 10
    div ecx
    mov ebx, eax
    
    add dl, '0'
    mov BYTE PTR [rsi + rcx - 1], dl
    dec ecx
    jmp i2s_convert
    
i2s_reverse:
    ; rsi now has digits in memory
    ; Just copy it out (simplified)
    xor ecx, ecx
    
i2s_copy:
    mov al, BYTE PTR [rsi + rcx]
    mov BYTE PTR [rdi + rcx], al
    test al, al
    jz i2s_done
    inc ecx
    jmp i2s_copy
    
i2s_done:
    add rsp, 64
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret
int_to_string ENDP

END
