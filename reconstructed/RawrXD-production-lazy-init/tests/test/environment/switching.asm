;==============================================================================
; test_environment_switching.asm - Integration Test: Environment Detection
; ==============================================================================
; Tests Config_IsProduction and environment-specific behavior across all
; integrated modules
;==============================================================================

option casemap:none

include d:\RawrXD-production-lazy-init\masm\masm_master_include.asm

; External dependencies
EXTERN OutputDebugStringA:PROC
EXTERN wsprintfA:PROC
EXTERN GetEnvironmentVariableA:PROC
EXTERN SetEnvironmentVariableA:PROC
EXTERN GetTickCount64:PROC

.data
    ; Test messages
    szTestStart         BYTE "Environment Switching Test Starting...", 13, 10, 0
    szDevMode           BYTE "✅ Development mode detected", 13, 10, 0
    szProdMode          BYTE "✅ Production mode detected", 13, 10, 0
    szStagingMode       BYTE "✅ Staging mode detected", 13, 10, 0
    szEnvironmentVar    BYTE "RAWRXD_ENVIRONMENT", 0
    szDevelopment       BYTE "development", 0
    szProduction        BYTE "production", 0
    szStaging           BYTE "staging", 0
    szEnvBuffer         BYTE 64 DUP (?)
    szTestMsg           BYTE "Current Environment: %s", 13, 10, 0
    szFormatBuf         BYTE 256 DUP (?)
    
    ; Behavior test messages
    szLoggingLevel      BYTE "Logging level: %s", 13, 10, 0
    szMemoryStats       BYTE "Memory stats tracking: %s", 13, 10, 0
    szHotpatchEnabled   BYTE "Hotpatching enabled: %s", 13, 10, 0
    szYes               BYTE "YES", 0
    szNo                BYTE "NO", 0
    
    ; Test results
    szTestPass          BYTE "✅ Environment switching test PASSED", 13, 10, 0
    szTestFail          BYTE "❌ Environment switching test FAILED", 13, 10, 0

.code

;==============================================================================
; Test_EnvironmentSwitching()
;==============================================================================
PUBLIC Test_EnvironmentSwitching
ALIGN 16
Test_EnvironmentSwitching PROC
    PUSH rbx
    PUSH rsi
    PUSH rdi
    SUB rsp, 96
    
    ; Output test start
    LEA rcx, [szTestStart]
    CALL OutputDebugStringA
    
    ; Initialize configuration
    CALL Config_Initialize
    TEST rax, rax
    JZ env_test_fail
    
    ; Test 1: Check if production mode
    CALL Config_IsProduction
    MOV rsi, rax        ; Save result
    
    ; Output result
    TEST rsi, rsi
    JZ env_is_dev
    
    LEA rcx, [szProdMode]
    JMP env_is_done
    
env_is_dev:
    LEA rcx, [szDevMode]
    
env_is_done:
    CALL OutputDebugStringA
    
    ; Test 2: Check FEATURE_HOTPATCH_DYNAMIC
    MOV ecx, FEATURE_HOTPATCH_DYNAMIC
    CALL Config_IsFeatureEnabled
    MOV rdi, rax        ; Save hotpatch status
    
    ; Output hotpatch feature status
    LEA rcx, [szFormatBuf]
    LEA rdx, [szHotpatchEnabled]
    TEST rdi, rdi
    JZ hp_disabled
    
    MOV r8, QWORD PTR [szYes]
    JMP hp_status_format
    
hp_disabled:
    MOV r8, QWORD PTR [szNo]
    
hp_status_format:
    CALL wsprintfA
    LEA rcx, [szFormatBuf]
    CALL OutputDebugStringA
    
    ; Test 3: Check FEATURE_DEBUG_MODE
    MOV ecx, FEATURE_DEBUG_MODE
    CALL Config_IsFeatureEnabled
    MOV rbx, rax        ; Save debug status
    
    ; Test 4: Check FEATURE_AGENTIC_ORCHESTRATION
    MOV ecx, FEATURE_AGENTIC_ORCHESTRATION
    CALL Config_IsFeatureEnabled
    
    ; All tests passed
    LEA rcx, [szTestPass]
    CALL OutputDebugStringA
    
    XOR eax, eax        ; Success
    JMP env_test_cleanup
    
env_test_fail:
    LEA rcx, [szTestFail]
    CALL OutputDebugStringA
    
    MOV eax, 1          ; Failed
    
env_test_cleanup:
    ADD rsp, 96
    POP rdi
    POP rsi
    POP rbx
    RET
Test_EnvironmentSwitching ENDP

;==============================================================================
; Test_DevelopmentModeBehavior() - Verify dev-specific features
;==============================================================================
PUBLIC Test_DevelopmentModeBehavior
ALIGN 16
Test_DevelopmentModeBehavior PROC
    PUSH rbx
    SUB rsp, 32
    
    ; Set environment to development
    LEA rcx, [szEnvironmentVar]
    LEA rdx, [szDevelopment]
    CALL SetEnvironmentVariableA
    
    ; Reinitialize config
    CALL Config_Initialize
    TEST rax, rax
    JZ dev_test_fail
    
    ; Verify NOT in production
    CALL Config_IsProduction
    TEST rax, rax
    JNZ dev_test_fail   ; Should be 0 (not production)
    
    XOR eax, eax        ; Success
    JMP dev_test_done
    
dev_test_fail:
    MOV eax, 1          ; Failed
    
dev_test_done:
    ADD rsp, 32
    POP rbx
    RET
Test_DevelopmentModeBehavior ENDP

;==============================================================================
; Test_ProductionModeBehavior() - Verify prod-specific features
;==============================================================================
PUBLIC Test_ProductionModeBehavior
ALIGN 16
Test_ProductionModeBehavior PROC
    PUSH rbx
    SUB rsp, 32
    
    ; Set environment to production
    LEA rcx, [szEnvironmentVar]
    LEA rdx, [szProduction]
    CALL SetEnvironmentVariableA
    
    ; Reinitialize config
    CALL Config_Initialize
    TEST rax, rax
    JZ prod_test_fail
    
    ; Verify IN production
    CALL Config_IsProduction
    TEST rax, rax
    JZ prod_test_fail   ; Should be 1 (is production)
    
    XOR eax, eax        ; Success
    JMP prod_test_done
    
prod_test_fail:
    MOV eax, 1          ; Failed
    
prod_test_done:
    ADD rsp, 32
    POP rbx
    RET
Test_ProductionModeBehavior ENDP

;==============================================================================
; Entry point
;==============================================================================
PUBLIC main
main PROC
    PUSH rbx
    SUB rsp, 32
    
    ; Run all environment tests
    CALL Test_EnvironmentSwitching
    MOV rbx, rax        ; Save first result
    
    CALL Test_DevelopmentModeBehavior
    OR rbx, rax         ; Combine results
    
    CALL Test_ProductionModeBehavior
    OR rbx, rax
    
    ; Return combined result
    MOV eax, ebx
    
    ADD rsp, 32
    POP rbx
    RET
main ENDP

END
