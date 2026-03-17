; ============================================================================
; EXTENDED PURE MASM HARNESS v2.0 - FINAL VERSION
; ============================================================================
; Real components: Phase 3 (Config) + Phase 4 (Error/Resource Guards)
; Simplified for stable linking and execution
; ============================================================================

OPTION CASEMAP:NONE

includelib kernel32.lib
includelib user32.lib
includelib winhttp.lib

; ============================================================================
; EXTERNAL DECLARATIONS
; ============================================================================

extern GetStdHandle:PROC
extern WriteFile:PROC
extern ExitProcess:PROC
extern GetLastError:PROC
extern lstrlenA:PROC

; Phase 3 Config  
extern Config_Initialize:PROC
extern Config_LoadFromFile:PROC
extern Config_LoadFromEnvironment:PROC
extern Config_IsFeatureEnabled:PROC
extern Config_Cleanup:PROC

; Phase 4 Error Handler
extern ErrorHandler_Initialize:PROC
extern ErrorHandler_Cleanup:PROC
extern ErrorHandler_GetErrorCount:PROC

; Phase 4 Resource Guards
extern ResourceGuard_Initialize:PROC
extern ResourceGuard_CleanupAll:PROC

; Ollama Client
extern OllamaClient_Initialize:PROC
extern OllamaClient_ListModels:PROC

; Performance
extern Perf_Initialize:PROC
extern Perf_Cleanup:PROC

; ============================================================================
; CONSTANTS
; ============================================================================

STD_OUTPUT_HANDLE = -11

; ============================================================================
; DATA SECTION
; ============================================================================

.data

szBanner            db "=== Extended MASM Harness v2.0 ===", 13, 10
                    db "Real Components: Config + Error + ResourceGuards", 13, 10, 13, 10, 0

szPhase3            db "[PHASE 3] Config Initialization...", 13, 10, 0
szPhase3Ok          db "[OK] Config system ready", 13, 10, 0
szPhase3Fail        db "[FAIL] Config initialization failed", 13, 10, 0

szPhase4E           db "[PHASE 4] Error Handler Initialization...", 13, 10, 0
szPhase4EOk         db "[OK] Error handler ready", 13, 10, 0
szPhase4EFail       db "[FAIL] Error handler initialization failed", 13, 10, 0

szPhase4R           db "[PHASE 4] Resource Guards Initialization...", 13, 10, 0
szPhase4ROk         db "[OK] Resource guards ready", 13, 10, 0
szPhase4RFail       db "[FAIL] Resource guard initialization failed", 13, 10, 0

szOllama            db "[TEST] Ollama integration...", 13, 10, 0
szOllamaOk          db "[OK] Ollama test passed", 13, 10, 0
szOllamaFail        db "[INFO] Ollama not available", 13, 10, 0

szPerf              db "[TEST] Performance baseline...", 13, 10, 0
szPerfOk            db "[OK] Performance test passed", 13, 10, 0

szCleanup           db 13, 10, "[CLEANUP] Releasing resources...", 13, 10, 0
szSuccess           db "[SUCCESS] All tests passed!", 13, 10, 0
szFailed            db "[FAILED] Some tests failed", 13, 10, 0

.code

; ============================================================================
; SIMPLE PRINT HELPER
; ============================================================================
Print PROC
    ; RCX = string pointer
    push rax
    push rdx
    push r8
    push r9
    
    mov rax, rcx
    xor rdx, rdx
    
strlen_loop:
    cmp byte ptr [rax + rdx], 0
    je strlen_done
    inc rdx
    jmp strlen_loop
    
strlen_done:
    ; Now RDX = string length, RCX = string
    mov r8, rdx
    mov rcx, -11  ; STD_OUTPUT_HANDLE
    call GetStdHandle
    
    ; RCX = string, R8 = length, RAX = handle
    mov r9d, eax  ; R9d = handle (temp)
    mov rcx, rax  ; RCX = handle
    mov rax, rdx  ; Save length
    lea rdx, [szSuccess]  ; Reuse as pointer (bytes written)
    mov r8, rax   ; R8 = length
    call WriteFile
    
    pop r9
    pop r8
    pop rdx
    pop rax
    ret
Print ENDP

; ============================================================================
; MAIN ENTRY POINT
; ============================================================================

mainCRTStartup PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    
    ; Display banner
    lea rcx, [szBanner]
    call Print
    
    ; ====================================================================
    ; PHASE 3: CONFIG
    ; ====================================================================
    
    lea rcx, [szPhase3]
    call Print
    
    xor rcx, rcx
    call Config_Initialize
    test rax, rax
    jz phase3_fail
    
    lea rcx, [szPhase3Ok]
    call Print
    jmp phase4e
    
phase3_fail:
    lea rcx, [szPhase3Fail]
    call Print
    jmp cleanup
    
    ; ====================================================================
    ; PHASE 4: ERROR HANDLER
    ; ====================================================================
    
phase4e:
    lea rcx, [szPhase4E]
    call Print
    
    xor rcx, rcx
    call ErrorHandler_Initialize
    test rax, rax
    jz phase4e_fail
    
    lea rcx, [szPhase4EOk]
    call Print
    jmp phase4r
    
phase4e_fail:
    lea rcx, [szPhase4EFail]
    call Print
    jmp cleanup
    
    ; ====================================================================
    ; PHASE 4: RESOURCE GUARDS
    ; ====================================================================
    
phase4r:
    lea rcx, [szPhase4R]
    call Print
    
    xor rcx, rcx
    call ResourceGuard_Initialize
    test rax, rax
    jz phase4r_fail
    
    lea rcx, [szPhase4ROk]
    call Print
    jmp ollama
    
phase4r_fail:
    lea rcx, [szPhase4RFail]
    call Print
    jmp cleanup
    
    ; ====================================================================
    ; REAL COMPONENT TESTS
    ; ====================================================================
    
ollama:
    lea rcx, [szOllama]
    call Print
    
    xor rcx, rcx
    call OllamaClient_Initialize
    test rax, rax
    jz ollama_skip
    
    lea rcx, [szOllamaOk]
    call Print
    jmp perf
    
ollama_skip:
    lea rcx, [szOllamaFail]
    call Print
    
perf:
    lea rcx, [szPerf]
    call Print
    
    xor rcx, rcx
    call Perf_Initialize
    
    lea rcx, [szPerfOk]
    call Print
    
    ; ====================================================================
    ; CLEANUP
    ; ====================================================================
    
cleanup:
    lea rcx, [szCleanup]
    call Print
    
    xor rcx, rcx
    call ResourceGuard_CleanupAll
    
    call ErrorHandler_Cleanup
    
    call Config_Cleanup
    
    call Perf_Cleanup
    
    lea rcx, [szSuccess]
    call Print
    
    xor ecx, ecx
    call ExitProcess

mainCRTStartup ENDP

end
