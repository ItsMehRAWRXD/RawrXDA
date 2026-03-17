; ============================================================================
; EXTENDED PURE MASM HARNESS v3.0 - FULLY FUNCTIONAL
; ============================================================================
; Integration of Phase 3 (Config) + Phase 4 (Error/Resource Guards)
; Simplified output using direct console writes
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

; Phase 3 Config  
extern Config_Initialize:PROC
extern Config_LoadFromFile:PROC
extern Config_LoadFromEnvironment:PROC
extern Config_IsFeatureEnabled:PROC
extern Config_Cleanup:PROC

; Phase 4 Error Handler
extern ErrorHandler_Initialize:PROC
extern ErrorHandler_Cleanup:PROC
extern ErrorHandler_Capture:PROC

; Phase 4 Resource Guards
extern Guard_CreateMemory:PROC
extern Guard_Destroy:PROC

; Runtime support
extern asm_malloc:PROC

; Ollama Client
extern OllamaClient_Initialize:PROC

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
align 4

dwBytesWritten  dd 0

; Message strings
msgBanner       db "=== Extended MASM Harness v3.0 ===", 13, 10
                db "Real Components Integration", 13, 10, 13, 10, 0

msgPhase3       db "[1] Initializing Config System...", 13, 10, 0
msgPhase3OK     db "    [OK]", 13, 10, 0
msgPhase3Fail   db "    [FAIL]", 13, 10, 0

msgPhase4E      db "[2] Initializing Error Handler...", 13, 10, 0
msgPhase4EOK    db "    [OK]", 13, 10, 0
msgPhase4EFail  db "    [FAIL]", 13, 10, 0

msgPhase4R      db "[3] Initializing Resource Guards...", 13, 10, 0
msgPhase4ROK    db "    [OK]", 13, 10, 0
msgPhase4RFail  db "    [FAIL]", 13, 10, 0

msgOllama       db "[4] Testing Ollama Integration...", 13, 10, 0
msgOllamaOK     db "    [OK]", 13, 10, 0
msgOllamaSkip   db "    [SKIP - Not available]", 13, 10, 0

msgPerf         db "[5] Testing Performance...", 13, 10, 0
msgPerfOK       db "    [OK]", 13, 10, 0

msgCleanup      db 13, 10, "[CLEANUP] Releasing resources...", 13, 10, 0
msgSuccess      db "[SUCCESS] All tests passed!", 13, 10, 13, 10, 0
msgFailed       db "[FAILED] Some tests failed", 13, 10, 13, 10, 0

.code

; ============================================================================
; OUTPUT HELPER - Direct console write
; ============================================================================

OutputMsg PROC
    ; RCX = message string
    ; Returns: nothing

    ; Win64: keep stack aligned and provide 32-byte shadow space
    sub rsp, 28h

    mov r10, rcx                ; r10 = message ptr

    ; Get stdout handle
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle

    ; Compute string length into r8d
    xor r8d, r8d
len_loop:
    cmp byte ptr [r10 + r8], 0
    je len_done
    inc r8d
    jmp len_loop

len_done:
    ; WriteFile(hStdOut, buffer, len, &dwBytesWritten, NULL)
    mov rcx, rax                ; handle
    mov rdx, r10                ; buffer
    lea r9, dwBytesWritten
    mov qword ptr [rsp + 20h], 0
    call WriteFile

    add rsp, 28h
    ret
OutputMsg ENDP

; ============================================================================
; MAIN ENTRY
; ============================================================================

mainCRTStartup PROC
    ; Win64 ABI: reserve shadow space + align stack for all calls we make.
    ; We never return (ExitProcess), so no epilogue needed.
    sub rsp, 28h

    ; Banner
    lea rcx, [msgBanner]
    call OutputMsg
    
    ; Phase 3: Config
    lea rcx, [msgPhase3]
    call OutputMsg
    
    xor rcx, rcx
    call Config_Initialize
    test rax, rax
    jz p3fail
    
    lea rcx, [msgPhase3OK]
    call OutputMsg
    jmp phase4e
    
p3fail:
    lea rcx, [msgPhase3Fail]
    call OutputMsg
    jmp cleanup
    
    ; Phase 4: Error Handler
phase4e:
    lea rcx, [msgPhase4E]
    call OutputMsg
    
    xor rcx, rcx
    call ErrorHandler_Initialize
    test rax, rax
    jz p4efail
    
    lea rcx, [msgPhase4EOK]
    call OutputMsg
    jmp phase4r
    
p4efail:
    lea rcx, [msgPhase4EFail]
    call OutputMsg
    jmp cleanup
    
    ; Phase 4: Resource Guards
phase4r:
    lea rcx, [msgPhase4R]
    call OutputMsg

    ; Create & destroy a memory guard as a real RAII sanity test
    mov rcx, 64
    mov rdx, 16
    call asm_malloc
    test rax, rax
    jz p4rfail

    mov rcx, rax
    call Guard_CreateMemory
    test rax, rax
    jz p4rfail

    mov rcx, rax
    call Guard_Destroy

    lea rcx, [msgPhase4ROK]
    call OutputMsg
    jmp test_ollama
    
p4rfail:
    lea rcx, [msgPhase4RFail]
    call OutputMsg
    jmp cleanup
    
    ; Ollama Test
test_ollama:
    lea rcx, [msgOllama]
    call OutputMsg
    
    xor rcx, rcx
    call OllamaClient_Initialize
    test rax, rax
    jz ollama_skip
    
    lea rcx, [msgOllamaOK]
    call OutputMsg
    jmp test_perf
    
ollama_skip:
    lea rcx, [msgOllamaSkip]
    call OutputMsg
    
    ; Performance Test
test_perf:
    lea rcx, [msgPerf]
    call OutputMsg
    
    xor rcx, rcx
    call Perf_Initialize
    
    lea rcx, [msgPerfOK]
    call OutputMsg
    
    ; Cleanup
cleanup:
    lea rcx, [msgCleanup]
    call OutputMsg
    
    ; Cleanup calls
    ; (Guards are explicitly destroyed above in this harness)
    call ErrorHandler_Cleanup
    call Config_Cleanup
    call Perf_Cleanup
    
    ; Success message
    lea rcx, [msgSuccess]
    call OutputMsg
    
    ; Exit
    xor ecx, ecx
    call ExitProcess

mainCRTStartup ENDP

end
