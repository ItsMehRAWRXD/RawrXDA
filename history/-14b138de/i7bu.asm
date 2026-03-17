;==========================================================================
; masm_inference_engine.asm - Pure MASM Inference Engine
; ==========================================================================
; Replaces inference_engine.cpp and transformer_inference.cpp.
; Coordinates model loading, tokenization, and inference.
;==========================================================================

option casemap:none

include windows.inc

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN ml_masm_init:PROC
EXTERN ml_masm_inference:PROC
EXTERN tokenizer_init:PROC
EXTERN tokenizer_encode:PROC
EXTERN masm_detect_failure:PROC
EXTERN masm_puppeteer_correct_response:PROC
EXTERN console_log:PROC

;==========================================================================
; DATA SECTION
;==========================================================================
.data
    szEngineInit        BYTE "InferenceEngine: Initializing MASM core...", 0
    szModelLoading      BYTE "InferenceEngine: Loading model: %s", 0
    szInferenceStart    BYTE "InferenceEngine: Starting inference for prompt...", 0
    szFailureDetected   BYTE "InferenceEngine: Hallucination detected! Correcting...", 0
    
    ; State
    g_engine_ready      DWORD 0
    
.data?
    token_buffer        QWORD 1024 dup(?)  ; Token buffer
    correction_result   BYTE 256 dup(?)    ; CorrectionResult structure

.code

;==========================================================================
; inference_engine_init()
;==========================================================================
PUBLIC inference_engine_init
inference_engine_init PROC
    sub rsp, 32
    
    lea rcx, szEngineInit
    call console_log
    
    call ml_masm_init
    call tokenizer_init
    
    mov DWORD PTR g_engine_ready, 1
    
    add rsp, 32
    ret
inference_engine_init ENDP

;==========================================================================
; inference_engine_run(prompt: rcx) -> rax (response_ptr)
;==========================================================================
PUBLIC inference_engine_run
inference_engine_run PROC
    push rbx
    push rsi
    sub rsp, 32
    
    mov rsi, rcx        ; prompt
    
    lea rcx, szInferenceStart
    call console_log
    
    ; 1. Tokenize
    mov rcx, rsi
    lea rdx, token_buffer
    call tokenizer_encode
    test rax, rax
    jz inference_fail
    
    ; 2. Inference
    lea rcx, token_buffer
    call ml_masm_inference
    mov rbx, rax        ; response
    test rbx, rbx
    jz inference_fail
    
    ; 3. Failure Detection
    mov rcx, rbx
    call masm_detect_failure
    test rax, rax
    jz inference_success
    
    ; 4. Correction
    lea rcx, szFailureDetected
    call console_log
    
    mov rcx, rbx        ; failure result
    xor rdx, rdx        ; mode = 0 (default)
    lea r8, correction_result
    call masm_puppeteer_correct_response
    test rax, rax
    jz inference_fail
    
    ; Use corrected response
    mov rbx, QWORD PTR [correction_result + 8]  ; corrected_response_ptr
    jmp inference_success
    
inference_fail:
    xor rbx, rbx
    
inference_success:
    mov rax, rbx
    add rsp, 32
    pop rsi
    pop rbx
    ret
inference_engine_run ENDP

END
