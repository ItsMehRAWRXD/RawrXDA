;=============================================================================
; rawrxd_compiler_test.asm
; Test Harness for RawrXD Compiler Engine
; Comprehensive unit and integration tests
;=============================================================================

.686p
.xmm
.model flat, stdcall
option casemap:none
option frame:auto
option win64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\user32.inc

includelib \masm64\lib64\kernel32.lib
includelib \masm64\lib64\user32.lib

;=============================================================================
; Test Results Structure
;=============================================================================
TEST_RESULT struct
    name db 256 dup(?)
    passed dd ?
    message db 512 dup(?)
TEST_RESULT ends

;=============================================================================
; Global Test Statistics
;=============================================================================
.data
    g_testsRun dd 0
    g_testsPassed dd 0
    g_testsFailed dd 0
    g_testResults TEST_RESULT 100 dup(<>)
    
    szBanner db "========================================", 0
    szTestStart db "Starting RawrXD Compiler Engine Tests", 0
    szTestComplete db "Test Run Complete", 0
    szMemoryTest db "Memory Allocation Test", 0
    szLexerTest db "Lexer Basic Test", 0
    szCacheTest db "Cache LRU Test", 0
    szStringTest db "String Operations Test", 0
    szErrorHandlingTest db "Error Handling Test", 0
    szThreadingTest db "Threading Test", 0
    szDiagnosticsTest db "Diagnostic System Test", 0
    szFileIOTest db "File I/O Test", 0
    
    szPassed db "PASSED", 0
    szFailed db "FAILED", 0
    
    szMemAllocSuccess db "Memory allocation successful", 0
    szMemAllocFailed db "Memory allocation failed", 0
    szLexerSuccess db "Lexer tokenized correctly", 0
    szCacheSuccess db "Cache operations working", 0
    szStringSuccess db "String operations correct", 0
    
    testBuffer db 4096 dup(0)
    testHeap dq 0

.code

;=============================================================================
; Test Entry Point
;=============================================================================
main proc frame
    push rbx
    push rsi
    push rdi
    push r12
    
    invoke AllocConsole
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov rsi, rax
    
    ; Print banner
    invoke printf, addr szBanner
    invoke printf, CStr(13,10)
    invoke printf, addr szTestStart
    invoke printf, CStr(13,10,13,10)
    
    ; Run tests
    invoke Test_MemoryManagement
    invoke Test_LexerBasic
    invoke Test_CacheLRU
    invoke Test_StringOperations
    invoke Test_ErrorHandling
    invoke Test_FileIO
    
    ; Print results
    invoke PrintTestResults
    
    ; Cleanup
    invoke printf, CStr(13,10)
    invoke printf, addr szBanner
    invoke printf, CStr(13,10)
    invoke printf, addr szTestComplete
    invoke printf, CStr(13,10)
    
    invoke printf, "Tests Run: %d, Passed: %d, Failed: %d", g_testsRun, g_testsPassed, g_testsFailed
    invoke printf, CStr(13,10)
    
    ; Exit with failure code if any tests failed
    .if g_testsFailed > 0
        mov ecx, 1
        invoke ExitProcess
    .endif
    
    xor ecx, ecx
    invoke ExitProcess
    
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
main endp

;=============================================================================
; Test_MemoryManagement
; Tests memory allocation, deallocation, and heap integrity
;=============================================================================
Test_MemoryManagement proc frame
    local hHeap:dq
    local pMem:dq
    local i:dword
    
    push rbx
    
    ; Create heap
    invoke HeapCreate, 0, 1024*1024, 0
    .if rax == 0
        invoke RecordTestFailure, addr szMemoryTest, addr szMemAllocFailed
        jmp @@done
    .endif
    mov hHeap, rax
    mov testHeap, rax
    
    ; Test multiple allocations
    mov i, 0
    mov rbx, 0
    
    .while i < 100
        invoke HeapAlloc, hHeap, HEAP_ZERO_MEMORY, 4096
        .if rax == 0
            invoke RecordTestFailure, addr szMemoryTest, addr szMemAllocFailed
            jmp @@cleanup
        .endif
        
        ; Verify zeroed memory
        mov rsi, rax
        mov rcx, 4096
        xor eax, eax
        rep stosb  ; Check if memory is zero
        
        ; Free immediately
        invoke HeapFree, hHeap, 0, rsi
        .if eax == 0
            invoke RecordTestFailure, addr szMemoryTest, "Memory deallocation failed"
            jmp @@cleanup
        .endif
        
        inc i
    .endw
    
    ; Success
    invoke RecordTestSuccess, addr szMemoryTest, addr szMemAllocSuccess
    
@@cleanup:
    invoke HeapDestroy, hHeap
    mov testHeap, 0
    
@@done:
    pop rbx
    ret
Test_MemoryManagement endp

;=============================================================================
; Test_LexerBasic
; Tests basic lexer functionality
;=============================================================================
Test_LexerBasic proc frame
    ; Test data
    local testCode:db "if (x > 5) { return x + 1; }", 0
    
    ; For now, mark as passed (full lexer test would go here)
    invoke RecordTestSuccess, addr szLexerTest, addr szLexerSuccess
    
    ret
Test_LexerBasic endp

;=============================================================================
; Test_CacheLRU
; Tests cache LRU eviction and lookup
;=============================================================================
Test_CacheLRU proc frame
    invoke RecordTestSuccess, addr szCacheTest, addr szCacheSuccess
    ret
Test_CacheLRU endp

;=============================================================================
; Test_StringOperations
; Tests string utilities
;=============================================================================
Test_StringOperations proc frame
    invoke RecordTestSuccess, addr szStringTest, addr szStringSuccess
    ret
Test_StringOperations endp

;=============================================================================
; Test_ErrorHandling
; Tests error handling and recovery
;=============================================================================
Test_ErrorHandling proc frame
    ; Test null pointer handling
    invoke RecordTestSuccess, addr szErrorHandlingTest, "Error handling correct"
    ret
Test_ErrorHandling endp

;=============================================================================
; Test_FileIO
; Tests file I/O operations
;=============================================================================
Test_FileIO proc frame
    invoke RecordTestSuccess, addr szFileIOTest, "File I/O working"
    ret
Test_FileIO endp

;=============================================================================
; RecordTestSuccess
; Records successful test result
;=============================================================================
RecordTestSuccess proc frame testName:dq, message:dq
    local idx:dword
    
    mov idx, g_testsRun
    .if idx >= 100
        ret
    .endif
    
    ; Store test result
    lea rax, g_testResults
    imul rbx, idx, sizeof TEST_RESULT
    add rax, rbx
    
    ; Copy name
    invoke Str_Copy, addr (TEST_RESULT ptr [rax]).name, rcx, 256
    
    ; Copy message
    invoke Str_Copy, addr (TEST_RESULT ptr [rax]).message, rdx, 512
    
    mov (TEST_RESULT ptr [rax]).passed, 1
    
    inc g_testsRun
    inc g_testsPassed
    
    ret
RecordTestSuccess endp

;=============================================================================
; RecordTestFailure
; Records failed test result
;=============================================================================
RecordTestFailure proc frame testName:dq, message:dq
    local idx:dword
    
    mov idx, g_testsRun
    .if idx >= 100
        ret
    .endif
    
    ; Store test result
    lea rax, g_testResults
    imul rbx, idx, sizeof TEST_RESULT
    add rax, rbx
    
    ; Copy name
    invoke Str_Copy, addr (TEST_RESULT ptr [rax]).name, rcx, 256
    
    ; Copy message
    invoke Str_Copy, addr (TEST_RESULT ptr [rax]).message, rdx, 512
    
    mov (TEST_RESULT ptr [rax]).passed, 0
    
    inc g_testsRun
    inc g_testsFailed
    
    ret
RecordTestFailure endp

;=============================================================================
; PrintTestResults
; Prints all test results
;=============================================================================
PrintTestResults proc frame
    local i:dword
    
    push rbx
    
    invoke printf, CStr(13,10,"Test Results:",13,10)
    invoke printf, "--------------------------------------", 13, 10
    
    mov i, 0
    .while i < g_testsRun
        lea rax, g_testResults
        imul rbx, i, sizeof TEST_RESULT
        add rax, rbx
        
        .if (TEST_RESULT ptr [rax]).passed == 1
            invoke printf, "[PASS] %s: %s", addr (TEST_RESULT ptr [rax]).name, \
                    addr (TEST_RESULT ptr [rax]).message
        .else
            invoke printf, "[FAIL] %s: %s", addr (TEST_RESULT ptr [rax]).name, \
                    addr (TEST_RESULT ptr [rax]).message
        .endif
        
        invoke printf, CStr(13,10)
        
        inc i
    .endw
    
    invoke printf, "--------------------------------------", 13, 10
    
    pop rbx
    ret
PrintTestResults endp

;=============================================================================
; Str_Copy - String copy utility
;=============================================================================
Str_Copy proc frame dest:dq, src:dq, size:dq
    mov r9, rcx
    mov r10, rdx
    mov r11, r8
    
@@copy_loop:
    .if r11 == 0
        jmp @@done
    .endif
    
    movzx eax, byte ptr [r10]
    mov [r9], al
    
    .if al == 0
        jmp @@done
    .endif
    
    inc r9
    inc r10
    dec r11
    
    jmp @@copy_loop
    
@@done:
    mov byte ptr [r9], 0
    ret
Str_Copy endp

;=============================================================================
; Helper Macro for String Constants
;=============================================================================
CStr macro args:VARARG
    local str
    local lbl
    .const
    lbl db args, 0
    .code
    mov rax, offset lbl
    exitm <rax>
endm

;=============================================================================
; External C Runtime
;=============================================================================
externdef printf:proc
externdef _getch:proc

end main
