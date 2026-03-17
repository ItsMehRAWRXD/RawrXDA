;==========================================================================
; test_config_integration.asm - Integration test for configuration system
;==========================================================================

option casemap:none

; External declarations
EXTERN Config_Initialize:PROC
EXTERN Config_GetModelPath:PROC
EXTERN Config_GetApiEndpoint:PROC
EXTERN Config_IsFeatureEnabled:PROC
EXTERN Config_EnableFeature:PROC
EXTERN Config_DisableFeature:PROC
EXTERN Config_IsProduction:PROC
EXTERN Config_Cleanup:PROC
EXTERN ExitProcess:PROC
EXTERN OutputDebugStringA:PROC

; Feature flags
FEATURE_GPU_ACCELERATION    EQU 00000001h
FEATURE_DISTRIBUTED_TRACE   EQU 00000002h
FEATURE_ADVANCED_METRICS    EQU 00000004h
FEATURE_HOTPATCH_DYNAMIC    EQU 00000008h

.data
    szTestStart         BYTE "=== Configuration Integration Test Start ===", 13, 10, 0
    szTestPass          BYTE "[PASS] ", 0
    szTestFail          BYTE "[FAIL] ", 0
    szTestInit          BYTE "Config_Initialize", 13, 10, 0
    szTestGetModelPath  BYTE "Config_GetModelPath", 13, 10, 0
    szTestGetApiEndpoint BYTE "Config_GetApiEndpoint", 13, 10, 0
    szTestFeatureFlags  BYTE "Feature flags enable/disable", 13, 10, 0
    szTestProdCheck     BYTE "Config_IsProduction (should be false)", 13, 10, 0
    szTestEnd           BYTE "=== Configuration Integration Test Complete ===", 13, 10, 0
    szAllPassed         BYTE "All tests PASSED!", 13, 10, 0
    szSomeFailed        BYTE "Some tests FAILED!", 13, 10, 0

.data?
    test_count          DWORD ?
    test_passed         DWORD ?

.code

;==========================================================================
; Main test entry point
;==========================================================================
main PROC
    SUB rsp, 40
    
    ; Initialize test counters
    MOV DWORD PTR test_count, 0
    MOV DWORD PTR test_passed, 0
    
    ; Print test start
    LEA rcx, szTestStart
    CALL OutputDebugStringA
    
    ; Test 1: Config_Initialize
    LEA rcx, szTestInit
    CALL run_test
    CALL Config_Initialize
    TEST rax, rax
    JZ test1_fail
    CALL mark_pass
    JMP test2
    
test1_fail:
    CALL mark_fail
    
test2:
    ; Test 2: Config_GetModelPath
    LEA rcx, szTestGetModelPath
    CALL run_test
    CALL Config_GetModelPath
    TEST rax, rax
    JZ test2_fail
    CALL mark_pass
    JMP test3
    
test2_fail:
    CALL mark_fail
    
test3:
    ; Test 3: Config_GetApiEndpoint
    LEA rcx, szTestGetApiEndpoint
    CALL run_test
    CALL Config_GetApiEndpoint
    TEST rax, rax
    JZ test3_fail
    CALL mark_pass
    JMP test4
    
test3_fail:
    CALL mark_fail
    
test4:
    ; Test 4: Feature flags
    LEA rcx, szTestFeatureFlags
    CALL run_test
    
    ; Enable GPU feature
    MOV ecx, FEATURE_GPU_ACCELERATION
    CALL Config_EnableFeature
    
    ; Check if enabled
    MOV ecx, FEATURE_GPU_ACCELERATION
    CALL Config_IsFeatureEnabled
    TEST rax, rax
    JZ test4_fail
    
    ; Disable GPU feature
    MOV ecx, FEATURE_GPU_ACCELERATION
    CALL Config_DisableFeature
    
    ; Check if disabled
    MOV ecx, FEATURE_GPU_ACCELERATION
    CALL Config_IsFeatureEnabled
    TEST rax, rax
    JNZ test4_fail
    
    CALL mark_pass
    JMP test5
    
test4_fail:
    CALL mark_fail
    
test5:
    ; Test 5: Production check
    LEA rcx, szTestProdCheck
    CALL run_test
    CALL Config_IsProduction
    ; Should NOT be production by default
    TEST rax, rax
    JNZ test5_fail
    CALL mark_pass
    JMP cleanup_tests
    
test5_fail:
    CALL mark_fail
    
cleanup_tests:
    ; Clean up
    CALL Config_Cleanup
    
    ; Print test end
    LEA rcx, szTestEnd
    CALL OutputDebugStringA
    
    ; Check results
    MOV eax, DWORD PTR test_passed
    MOV ecx, DWORD PTR test_count
    CMP eax, ecx
    JE all_passed
    
    LEA rcx, szSomeFailed
    CALL OutputDebugStringA
    MOV ecx, 1  ; Exit code 1 for failure
    JMP exit_test
    
all_passed:
    LEA rcx, szAllPassed
    CALL OutputDebugStringA
    XOR ecx, ecx  ; Exit code 0 for success
    
exit_test:
    ADD rsp, 40
    CALL ExitProcess
main ENDP

;==========================================================================
; Helper: Print test name
;==========================================================================
run_test PROC
    PUSH rcx
    INC DWORD PTR test_count
    POP rcx
    CALL OutputDebugStringA
    RET
run_test ENDP

;==========================================================================
; Helper: Mark test as passed
;==========================================================================
mark_pass PROC
    INC DWORD PTR test_passed
    LEA rcx, szTestPass
    CALL OutputDebugStringA
    RET
mark_pass ENDP

;==========================================================================
; Helper: Mark test as failed
;==========================================================================
mark_fail PROC
    LEA rcx, szTestFail
    CALL OutputDebugStringA
    RET
mark_fail ENDP

END
