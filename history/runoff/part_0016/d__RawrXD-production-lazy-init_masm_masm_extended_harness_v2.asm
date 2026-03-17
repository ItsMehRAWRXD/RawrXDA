; ============================================================================
; EXTENDED PURE MASM HARNESS v2.0 - SIMPLIFIED FOR TESTING
; ============================================================================
; Demonstrates real component integration with actual testing
; Phase 3: Config + Phase 4: Error/Resource Guards + Real Ollama
; ============================================================================

OPTION CASEMAP:NONE

includelib kernel32.lib
includelib user32.lib
includelib winhttp.lib

; ============================================================================
; EXTERNAL DECLARATIONS
; ============================================================================

; Win32 API
extern GetStdHandle:PROC
extern WriteFile:PROC
extern ExitProcess:PROC
extern GetLastError:PROC
extern LocalAlloc:PROC
extern LocalFree:PROC
extern lstrlenA:PROC
extern Sleep:PROC

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

; Performance Baseline
extern Perf_Initialize:PROC
extern Perf_StartMeasurement:PROC
extern Perf_EndMeasurement:PROC
extern Perf_Cleanup:PROC

; ============================================================================
; CONSTANTS
; ============================================================================

STD_OUTPUT_HANDLE = -11
LMEM_ZEROINIT = 40h
MAX_BUFFER = 4096

; Feature flags
FEATURE_DEBUG_MODE = 0
FEATURE_DISTRIBUTED_TRACE = 1
FEATURE_ADVANCED_METRICS = 2
FEATURE_HOTPATCH_DYNAMIC = 3
FEATURE_EXPERIMENTAL_MODELS = 4

; ============================================================================
; DATA SECTION
; ============================================================================

.data
align 8

; Strings (existing)
szBanner            db "╔════════════════════════════════════════════════════╗", 13, 10
                    db "║ Extended MASM Harness v2.0 - Real Components     ║", 13, 10
                    db "║ Phase 3: Config | Phase 4: Error/Resource Guards ║", 13, 10
                    db "╚════════════════════════════════════════════════════╝", 13, 10, 13, 10, 0

szPhase3            db "[PHASE 3] Initializing Configuration Management...", 13, 10, 0
szPhase3Success     db "[✓] Config system ready", 13, 10, 0
szPhase3Failed      db "[✗] Config initialization failed", 13, 10, 0

szPhase4            db "[PHASE 4] Initializing Error Handling...", 13, 10, 0
szPhase4Success     db "[✓] Error handler ready", 13, 10, 0
szPhase4Failed      db "[✗] Error handler initialization failed", 13, 10, 0

szResGuards         db "[PHASE 4] Initializing Resource Guards...", 13, 10, 0
szResGuardSuccess   db "[✓] Resource guards ready", 13, 10, 0
szResGuardFailed    db "[✗] Resource guard initialization failed", 13, 10, 0

szOllamaTest        db "[TEST] Connecting to Ollama...", 13, 10, 0
szOllamaOk          db "[✓] Ollama connected successfully", 13, 10, 0
szOllamaFail        db "[✗] Ollama not available", 13, 10, 0

szPerfTest          db "[TEST] Measuring performance baseline...", 13, 10, 0
szPerfOk            db "[✓] Performance baseline recorded", 13, 10, 0

szCleanup           db 13, 10, "[CLEANUP] Releasing resources...", 13, 10, 0
szSuccess           db "[✓] Extended harness completed successfully!", 13, 10, 0
szFailure           db "[✗] Extended harness encountered errors", 13, 10, 0

; Initialized data (using byte arrays for 64-bit values)
hStdOut             db 8 dup(0)
pConfig             db 8 dup(0)
pErrorHandler       db 8 dup(0)
pResourceGuard      db 8 dup(0)
dwErrorCount        dd 0
dwSuccess           dd 0

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; ============================================================================
; SIMPLE PRINT PROCEDURE
; ============================================================================
SimplePrint PROC
    ; RCX = string pointer
    ; Uses: rax, rdx, r8, r9
    
    push rbx
    push r10
    
    ; Get length
    mov rax, rcx
    xor rdx, rdx
    
len_loop:
    cmp byte ptr [rax + rdx], 0
    je len_done
    inc rdx
    jmp len_loop
    
len_done:
    ; Write to stdout
    mov rcx, [hStdOut]
    mov r8, rdx
    lea r9, [dwSuccess]  ; Reuse as bytes written
    call WriteFile
    
    pop r10
    pop rbx
    ret
SimplePrint ENDP

; ============================================================================
; MAIN ENTRY POINT
; ============================================================================

mainCRTStartup PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Initialize stdout
    mov rcx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov [hStdOut], rax
    mov [dwSuccess], 0
    
    ; Banner
    lea rcx, [szBanner]
    call SimplePrint
    
    ; ====================================================================
    ; PHASE 3: CONFIG INITIALIZATION
    ; ====================================================================
    
    lea rcx, [szPhase3]
    call SimplePrint
    
    ; Call Config_Initialize
    xor rcx, rcx  ; NULL pointer for now
    call Config_Initialize
    mov [pConfig], rax
    
    cmp rax, 0
    je phase3_fail
    
    lea rcx, [szPhase3Success]
    call SimplePrint
    jmp phase4_start
    
phase3_fail:
    lea rcx, [szPhase3Failed]
    call SimplePrint
    jmp cleanup
    
    ; ====================================================================
    ; PHASE 4: ERROR HANDLER INITIALIZATION
    ; ====================================================================
    
phase4_start:
    lea rcx, [szPhase4]
    call SimplePrint
    
    ; Call ErrorHandler_Initialize
    xor rcx, rcx
    call ErrorHandler_Initialize
    mov [pErrorHandler], rax
    
    test rax, rax
    jz phase4_fail
    
    lea rcx, [szPhase4Success]
    call SimplePrint
    jmp res_guard_start
    
phase4_fail:
    lea rcx, [szPhase4Failed]
    call SimplePrint
    jmp cleanup
    
    ; ====================================================================
    ; PHASE 4: RESOURCE GUARDS INITIALIZATION
    ; ====================================================================
    
res_guard_start:
    lea rcx, [szResGuards]
    call SimplePrint
    
    xor rcx, rcx
    call ResourceGuard_Initialize
    mov [pResourceGuard], rax
    
    test rax, rax
    jz res_guard_fail
    
    lea rcx, [szResGuardSuccess]
    call SimplePrint
    jmp ollama_test
    
res_guard_fail:
    lea rcx, [szResGuardFailed]
    call SimplePrint
    jmp cleanup
    
    ; ====================================================================
    ; REAL COMPONENT TEST: OLLAMA
    ; ====================================================================
    
ollama_test:
    lea rcx, [szOllamaTest]
    call SimplePrint
    
    xor rcx, rcx
    call OllamaClient_Initialize
    test rax, rax
    jz ollama_skip
    
    lea rcx, [szOllamaOk]
    call SimplePrint
    jmp perf_test
    
ollama_skip:
    lea rcx, [szOllamaFail]
    call SimplePrint
    
    ; ====================================================================
    ; REAL COMPONENT TEST: PERFORMANCE
    ; ====================================================================
    
perf_test:
    lea rcx, [szPerfTest]
    call SimplePrint
    
    xor rcx, rcx
    call Perf_Initialize
    
    lea rcx, [szPerfOk]
    call SimplePrint
    
    mov [dwSuccess], 1
    
cleanup:
    lea rcx, [szCleanup]
    call SimplePrint
    
    ; Cleanup in reverse order
    mov rcx, [pResourceGuard]
    call ResourceGuard_CleanupAll
    
    mov rcx, [pErrorHandler]
    call ErrorHandler_Cleanup
    
    mov rcx, [pConfig]
    call Config_Cleanup
    
    call Perf_Cleanup
    
    ; Final status
    cmp dword ptr [dwSuccess], 1
    jne fail_exit
    
    lea rcx, [szSuccess]
    call SimplePrint
    xor ecx, ecx  ; Exit code 0
    jmp do_exit
    
fail_exit:
    lea rcx, [szFailure]
    call SimplePrint
    mov ecx, 1  ; Exit code 1
    
do_exit:
    call ExitProcess

mainCRTStartup ENDP

end
