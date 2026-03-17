;==============================================================================
; test_real_model_loading.asm - Integration Test: Real GGUF Model Loading
; ==============================================================================
; Tests ml_masm.asm with actual model file operations using config system
; to locate models and control feature flags
;==============================================================================

option casemap:none

include d:\RawrXD-production-lazy-init\masm\masm_master_include.asm

; External dependencies
EXTERN ml_masm_load_model:PROC
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN GetTickCount64:PROC
EXTERN OutputDebugStringA:PROC
EXTERN wsprintfA:PROC

.data
    ; Test messages
    szTestStart         BYTE "Real Model Loading Test Starting...", 13, 10, 0
    szTestPass          BYTE "✅ Test Passed: Model loaded successfully", 13, 10, 0
    szTestFail          BYTE "❌ Test Failed: Model loading failed", 13, 10, 0
    szConfigError       BYTE "❌ Configuration system initialization failed", 13, 10, 0
    szLoadStart         BYTE "Loading model from configured path...", 13, 10, 0
    szLoadComplete      BYTE "Model loading complete", 13, 10, 0
    szModelPath         BYTE "Model path: ", 0
    szElapsedTime       BYTE "Elapsed time: %lld ms", 13, 10, 0
    szFormatBuf         BYTE 512 DUP (?)
    
    ; Model file names to try
    szModel1            BYTE "models/llama2-7b.gguf", 0
    szModel2            BYTE "models/mistral-7b.gguf", 0
    szModel3            BYTE "models/neural-chat.gguf", 0

.code

;==============================================================================
; Test_RealModelLoading()
;==============================================================================
PUBLIC Test_RealModelLoading
ALIGN 16
Test_RealModelLoading PROC
    PUSH rbx
    PUSH rsi
    SUB rsp, 64
    
    ; Output test start
    LEA rcx, [szTestStart]
    CALL OutputDebugStringA
    
    ; Initialize configuration system
    CALL Config_Initialize
    TEST rax, rax
    JZ test_config_fail
    
    ; Get configured model path
    CALL Config_GetModelPath
    MOV rsi, rax        ; Store path
    TEST rax, rax
    JZ test_config_fail
    
    ; Output model path
    LEA rcx, [szModelPath]
    MOV rdx, rsi
    LEA r8, [szFormatBuf]
    CALL wsprintfA
    LEA rcx, [szFormatBuf]
    CALL OutputDebugStringA
    
    ; Record start time
    CALL GetTickCount64
    MOV [rsp], rax      ; Save start time
    
    ; Output loading message
    LEA rcx, [szLoadStart]
    CALL OutputDebugStringA
    
    ; Attempt to load model using configured path
    MOV rcx, rsi        ; model_path
    CALL ml_masm_load_model
    MOV rbx, rax        ; Save result
    
    ; Record end time
    CALL GetTickCount64
    MOV rcx, rax
    SUB rcx, [rsp]      ; Calculate elapsed time
    
    ; Format and output elapsed time
    LEA rdx, [szFormatBuf]
    LEA r8, [szElapsedTime]
    MOV r9, rcx
    CALL wsprintfA
    LEA rcx, [szFormatBuf]
    CALL OutputDebugStringA
    
    ; Check result
    TEST rbx, rbx
    JZ test_model_fail
    
    ; Output success
    LEA rcx, [szTestPass]
    CALL OutputDebugStringA
    
    MOV eax, 0          ; Test passed
    JMP test_cleanup
    
test_model_fail:
    ; Output failure
    LEA rcx, [szTestFail]
    CALL OutputDebugStringA
    
    MOV eax, 1          ; Test failed
    JMP test_cleanup
    
test_config_fail:
    ; Output config error
    LEA rcx, [szConfigError]
    CALL OutputDebugStringA
    
    MOV eax, -1         ; Configuration error
    
test_cleanup:
    ; Free model if loaded
    TEST rbx, rbx
    JZ skip_model_free
    
    MOV rcx, rbx
    CALL asm_free
    
skip_model_free:
    ADD rsp, 64
    POP rsi
    POP rbx
    RET
Test_RealModelLoading ENDP

;==============================================================================
; Entry point for testing
;==============================================================================
PUBLIC main
main PROC
    SUB rsp, 32
    
    CALL Test_RealModelLoading
    
    ; Set exit code
    MOV ecx, eax
    
    ADD rsp, 32
    RET
main ENDP

END
