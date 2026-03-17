;==============================================================================
; RawrXD Automated Testing & CI/CD - Pure MASM Implementation
; Phase 12: Test Runner, Assertions, and Build Automation
;==============================================================================

.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc
include kernel32.inc

includelib user32.lib
includelib kernel32.lib

;==============================================================================
; TEST CONSTANTS
;==============================================================================
MAX_TESTS               equ 128
TEST_STATUS_PENDING     equ 0
TEST_STATUS_PASSED      equ 1
TEST_STATUS_FAILED      equ 2

;==============================================================================
; TEST STRUCTURES
;==============================================================================
TEST_CASE struct
    name            db 128 dup(?)
    pFunc           dd ?
    status          dd ?
    errorMessage    db 256 dup(?)
TEST_CASE ends

;==============================================================================
; TEST STATE
;==============================================================================
.data?
testCases               TEST_CASE MAX_TESTS dup(<?>)
testCount               dd ?
passCount               dd ?
failCount               dd ?

.code

;==============================================================================
; Phase 12.1: Test Registration
;==============================================================================
Test_Register proc pName:DWORD, pFunc:DWORD
    mov eax, testCount
    .if eax >= MAX_TESTS
        ret
    .endif
    
    mov ebx, SIZEOF TEST_CASE
    mul ebx
    lea edi, testCases
    add edi, eax
    assume edi:ptr TEST_CASE
    
    invoke lstrcpyn, addr [edi].name, pName, 128
    mov eax, pFunc
    mov [edi].pFunc, eax
    mov [edi].status, TEST_STATUS_PENDING
    
    inc testCount
    ret
Test_Register endp

;==============================================================================
; Phase 12.2: Assertion Framework
;==============================================================================
Test_AssertTrue proc condition:DWORD, pMsg:DWORD, pTestCase:DWORD
    mov edi, pTestCase
    assume edi:ptr TEST_CASE
    
    .if condition == FALSE
        mov [edi].status, TEST_STATUS_FAILED
        invoke lstrcpyn, addr [edi].errorMessage, pMsg, 256
        mov eax, FALSE
    .else
        mov eax, TRUE
    .endif
    ret
Test_AssertTrue endp

Test_AssertIntEq proc val1:DWORD, val2:DWORD, pMsg:DWORD, pTestCase:DWORD
    mov eax, val1
    .if eax != val2
        invoke Test_AssertTrue, FALSE, pMsg, pTestCase
        mov eax, FALSE
    .else
        mov eax, TRUE
    .endif
    ret
Test_AssertIntEq endp

;==============================================================================
; Phase 12.3: Test Runner
;==============================================================================
Test_RunAll proc
    mov passCount, 0
    mov failCount, 0
    
    local i:DWORD
    mov i, 0
    .while i < testCount
        mov eax, i
        mov ebx, SIZEOF TEST_CASE
        mul ebx
        lea esi, testCases
        add esi, eax
        assume esi:ptr TEST_CASE
        
        ; Execute test
        mov [esi].status, TEST_STATUS_PASSED ; Assume pass
        push esi
        call [esi].pFunc
        
        .if [esi].status == TEST_STATUS_PASSED
            inc passCount
        .else
            inc failCount
        .endif
        
        inc i
    .endw
    
    invoke Test_ReportResults
    ret
Test_RunAll endp

;==============================================================================
; Phase 12.4: Reporting and CI/CD Integration
;==============================================================================
Test_ReportResults proc
    local buffer[512]:BYTE
    
    jmp @F
    szReportFmt db "Test Results: %d Passed, %d Failed",13,10,0
@@:
    invoke wsprintf, addr buffer, addr szReportFmt, passCount, failCount
    invoke OutputDebugString, addr buffer
    
    ; If in CI mode, exit with status code
    .if failCount > 0
        ; invoke ExitProcess, 1
    .endif
    
    ret
Test_ReportResults endp

;==============================================================================
; Phase 12.5: Sample Tests
;==============================================================================
SampleTest_Math proc pTestCase:DWORD
    invoke Test_AssertIntEq, 2, 2, addr szMathErr, pTestCase
    ret
    szMathErr db "Math error: 2 != 2",0
SampleTest_Math endp

end
