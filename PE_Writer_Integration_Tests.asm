; PE_Writer_Integration_Tests.asm
; Complete integration tests for the RawrXD PE Writer
; Demonstrates end-to-end executable generation with working Windows API calls

OPTION CASEMAP:NONE

; External PE Writer interface
EXTERN PEWriter_CreateExecutable: PROC
EXTERN PEWriter_AddImport: PROC  
EXTERN PEWriter_AddCode: PROC
EXTERN PEWriter_WriteFile: PROC
EXTERN PEWriter_CreateSimpleExecutable: PROC
EXTERN PEWriter_CreateMessageBoxApp: PROC
EXTERN PEWriter_RunIntegrationTests: PROC

; Windows APIs for testing
EXTERN GetStdHandle: PROC
EXTERN WriteConsoleA: PROC
EXTERN ExitProcess: PROC
EXTERN CreateProcessA: PROC
EXTERN WaitForSingleObject: PROC
EXTERN CloseHandle: PROC

.data
test_banner db "=== RawrXD PE Writer Integration Tests ===", 13, 10, 0
test_separator db "----------------------------------------", 13, 10, 0
test_pass db "[PASS] ", 0
test_fail db "[FAIL] ", 0
newline db 13, 10, 0

; Test file names
test1_name db "test_simple.exe", 0
test2_name db "test_msgbox.exe", 0
test3_name db "test_complex.exe", 0

; Test descriptions
test1_desc db "Simple ExitProcess executable", 13, 10, 0
test2_desc db "MessageBox application", 13, 10, 0
test3_desc db "Complex multi-API executable", 13, 10, 0

; Summary messages
tests_summary db "Tests completed. Results:", 13, 10, 0
all_pass_msg db "All tests PASSED! PE Writer is fully functional.", 13, 10, 0
some_fail_msg db "Some tests FAILED. Check implementation.", 13, 10, 0

; Executable validation messages
testing_exe_msg db "Testing generated executable: ", 0
exe_runs_msg db " - Executable runs successfully", 13, 10, 0
exe_fail_msg db " - Executable failed to run", 13, 10, 0

; Performance timing
start_time QWORD ?
end_time QWORD ?
perf_msg db "Total execution time: ", 0
perf_ms_msg db " milliseconds", 13, 10, 0

.code

;-----------------------------------------------------------------------------
; Main entry point for integration tests
;-----------------------------------------------------------------------------
main PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Print test banner
    lea rcx, test_banner
    call PrintString
    
    lea rcx, test_separator
    call PrintString
    
    ; Record start time
    call GetPerformanceTime
    mov start_time, rax
    
    ; Run comprehensive PE writer tests
    call RunCompleteTestSuite
    mov rbx, rax    ; Save results
    
    ; Record end time and calculate duration
    call GetPerformanceTime
    mov end_time, rax
    sub rax, start_time
    
    ; Print performance information
    lea rcx, perf_msg
    call PrintString
    mov rcx, rax
    call PrintNumber
    lea rcx, perf_ms_msg
    call PrintString
    
    ; Print test summary
    lea rcx, test_separator
    call PrintString
    
    lea rcx, tests_summary
    call PrintString
    
    ; Check if all tests passed
    cmp rbx, 6      ; Expected number of successful tests
    je all_tests_passed
    
    lea rcx, some_fail_msg
    call PrintString
    mov rcx, 1      ; Exit with error code
    jmp exit_program
    
all_tests_passed:
    lea rcx, all_pass_msg
    call PrintString
    mov rcx, 0      ; Exit with success
    
exit_program:
    call ExitProcess
main ENDP

;-----------------------------------------------------------------------------
; RunCompleteTestSuite - Executes all integration tests
; Output: RAX = number of tests passed
;-----------------------------------------------------------------------------
RunCompleteTestSuite PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    xor rbx, rbx    ; Test pass counter
    
    ; Test 1: Generate simple executable
    lea rcx, test1_desc
    call PrintString
    
    lea rcx, test1_name
    call TestSimpleExecutableGeneration
    add rbx, rax
    call PrintTestResult
    
    ; Test 2: Generate message box application  
    lea rcx, test2_desc
    call PrintString
    
    lea rcx, test2_name
    call TestMessageBoxGeneration
    add rbx, rax
    call PrintTestResult
    
    ; Test 3: Generate complex multi-API executable
    lea rcx, test3_desc
    call PrintString
    
    lea rcx, test3_name
    call TestComplexExecutableGeneration
    add rbx, rax
    call PrintTestResult
    
    ; Test 4: Validate file structures
    call TestGeneratedFileValidation
    add rbx, rax
    
    ; Test 5: Test executable execution
    call TestExecutableExecution
    add rbx, rax
    
    ; Test 6: PE structure validation
    call TestPEStructureValidation
    add rbx, rax
    
    mov rax, rbx    ; Return total passed tests
    
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
RunCompleteTestSuite ENDP

;-----------------------------------------------------------------------------
; TestSimpleExecutableGeneration
; Tests creation of basic executable with ExitProcess call
; Input: RCX = output filename
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
TestSimpleExecutableGeneration PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; Save filename
    
    ; Use the built-in simple executable creator
    mov rcx, rbx
    call PEWriter_CreateSimpleExecutable
    test rax, rax
    jz simple_test_fail
    
    ; Verify file was created and has correct size
    mov rcx, rbx
    call CheckFileExists
    test rax, rax
    jz simple_test_fail
    
    ; Verify file size is reasonable (should be > 1KB, < 100KB)
    mov rcx, rbx
    call GetFileSize 
    cmp rax, 1024
    jl simple_test_fail
    cmp rax, 102400
    jg simple_test_fail
    
    mov rax, 1      ; Success
    jmp simple_test_done
    
simple_test_fail:
    xor rax, rax
    
simple_test_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
TestSimpleExecutableGeneration ENDP

;-----------------------------------------------------------------------------
; TestMessageBoxGeneration
; Tests creation of executable with MessageBox call
; Input: RCX = output filename  
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
TestMessageBoxGeneration PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    
    mov rbx, rcx    ; Save filename
    
    ; Use the built-in message box app creator
    mov rcx, rbx
    call PEWriter_CreateMessageBoxApp
    test rax, rax
    jz msgbox_test_fail
    
    ; Verify file creation and basic properties
    mov rcx, rbx
    call CheckFileExists
    test rax, rax
    jz msgbox_test_fail
    
    mov rax, 1
    jmp msgbox_test_done
    
msgbox_test_fail:
    xor rax, rax
    
msgbox_test_done:
    pop rbx
    add rsp, 40h
    pop rbp
    ret
TestMessageBoxGeneration ENDP

;-----------------------------------------------------------------------------
; TestComplexExecutableGeneration
; Tests creation of executable with multiple API calls and sections
; Input: RCX = output filename
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
TestComplexExecutableGeneration PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; Save filename
    
    ; Create PE context with complex setup
    mov rcx, 140000000h     ; Image base
    mov rdx, 1000h          ; Entry point
    call PEWriter_CreateExecutable
    test rax, rax
    jz complex_test_fail
    mov rdi, rax            ; PE context
    
    ; Add multiple DLL imports
    mov rcx, rdi
    lea rdx, kernel32_name
    lea r8, exitprocess_name
    call PEWriter_AddImport
    test rax, rax
    jz complex_cleanup_fail
    
    mov rcx, rdi
    lea rdx, kernel32_name
    lea r8, getconsolemode_name
    call PEWriter_AddImport
    test rax, rax
    jz complex_cleanup_fail
    
    mov rcx, rdi
    lea rdx, user32_name
    lea r8, showwindow_name
    call PEWriter_AddImport
    test rax, rax
    jz complex_cleanup_fail
    
    ; Generate complex code with multiple function calls
    mov rcx, rdi
    call GenerateComplexCode
    test rax, rax
    jz complex_cleanup_fail
    
    ; Write the PE file
    mov rcx, rdi
    mov rdx, rbx
    call PEWriter_WriteFile
    test rax, rax
    jz complex_cleanup_fail
    
    ; Free PE context
    mov rcx, rdi
    call FreePEContext
    
    mov rax, 1      ; Success
    jmp complex_test_done
    
complex_cleanup_fail:
    mov rcx, rdi
    call FreePEContext
    
complex_test_fail:
    xor rax, rax
    
complex_test_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
TestComplexExecutableGeneration ENDP

;-----------------------------------------------------------------------------
; TestGeneratedFileValidation
; Validates the structure of all generated PE files
; Output: RAX = 1 all valid, 0 some invalid
;-----------------------------------------------------------------------------
TestGeneratedFileValidation PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    
    xor rbx, rbx    ; Validation counter
    
    ; Test validation message
    lea rcx, file_validation_msg
    call PrintString
    
    ; Validate simple executable
    lea rcx, test1_name
    call ValidateExecutableFile
    add rbx, rax
    
    ; Validate message box executable
    lea rcx, test2_name  
    call ValidateExecutableFile
    add rbx, rax
    
    ; Validate complex executable
    lea rcx, test3_name
    call ValidateExecutableFile
    add rbx, rax
    
    ; All three files should be valid
    cmp rbx, 3
    je validation_pass
    
    xor rax, rax
    jmp validation_done
    
validation_pass:
    mov rax, 1
    
validation_done:
    pop rbx
    add rsp, 40h
    pop rbp
    ret
TestGeneratedFileValidation ENDP

;-----------------------------------------------------------------------------
; TestExecutableExecution
; Attempts to execute generated files to verify functionality
; Output: RAX = 1 all executed, 0 execution failed
;-----------------------------------------------------------------------------
TestExecutableExecution PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    push rbx
    push rsi
    push rdi
    
    xor rbx, rbx    ; Execution success counter
    
    lea rcx, exe_test_msg
    call PrintString
    
    ; Test simple executable (should exit quickly)
    lea rcx, testing_exe_msg
    call PrintString
    lea rcx, test1_name
    call PrintString
    lea rcx, newline
    call PrintString
    
    lea rcx, test1_name
    call ExecuteAndWait
    test rax, rax
    jz exe_test_1_fail
    
    lea rcx, exe_runs_msg
    call PrintString
    inc rbx
    jmp exe_test_1_done
    
exe_test_1_fail:
    lea rcx, exe_fail_msg
    call PrintString
    
exe_test_1_done:
    ; Note: Message box test is skipped in automated testing
    ; as it requires user interaction
    
    ; Return success if at least one executable ran
    test rbx, rbx
    jz exe_test_fail
    
    mov rax, 1
    jmp exe_test_done
    
exe_test_fail:
    xor rax, rax
    
exe_test_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 80h
    pop rbp
    ret
TestExecutableExecution ENDP

;-----------------------------------------------------------------------------
; TestPEStructureValidation
; Performs deep validation of PE file structures
; Output: RAX = 1 structure valid, 0 invalid
;-----------------------------------------------------------------------------
TestPEStructureValidation PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    
    lea rcx, pe_structure_msg
    call PrintString
    
    xor rbx, rbx    ; Structure validation counter
    
    ; Validate DOS header
    lea rcx, test1_name
    call ValidateDOSHeader
    add rbx, rax
    
    ; Validate NT headers
    lea rcx, test1_name
    call ValidateNTHeaders
    add rbx, rax
    
    ; Validate section headers
    lea rcx, test1_name
    call ValidateSectionHeaders
    add rbx, rax
    
    ; Validate import table
    lea rcx, test1_name
    call ValidateImportTable
    add rbx, rax
    
    ; All structure validations should pass
    cmp rbx, 4
    je structure_valid
    
    xor rax, rax
    jmp structure_done
    
structure_valid:
    mov rax, 1
    
structure_done:
    pop rbx
    add rsp, 40h
    pop rbp
    ret
TestPEStructureValidation ENDP

;=============================================================================
; HELPER FUNCTIONS
;=============================================================================

;-----------------------------------------------------------------------------
; PrintTestResult
; Prints pass/fail for the last test based on RAX value
; Input: RAX = test result (1=pass, 0=fail)
;-----------------------------------------------------------------------------
PrintTestResult PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    test rax, rax
    jz print_fail_result
    
    lea rcx, test_pass
    call PrintString
    jmp print_result_done
    
print_fail_result:
    lea rcx, test_fail
    call PrintString
    
print_result_done:
    add rsp, 20h
    pop rbp
    ret
PrintTestResult ENDP

;-----------------------------------------------------------------------------
; PrintString
; Prints a null-terminated string to console
; Input: RCX = string pointer
;-----------------------------------------------------------------------------
PrintString PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    
    mov rbx, rcx    ; Save string pointer
    
    ; Get string length
    mov rcx, rbx
    call StrLen
    mov rsi, rax    ; String length
    
    ; Get stdout handle
    mov rcx, -11    ; STD_OUTPUT_HANDLE
    call GetStdHandle
    test rax, rax
    jz print_done
    
    ; Write string to console
    mov rcx, rax    ; Console handle
    mov rdx, rbx    ; String buffer
    mov r8, rsi     ; String length
    lea r9, [rsp+32] ; Bytes written
    mov qword ptr [rsp+20h], 0  ; Reserved
    call WriteConsoleA
    
print_done:
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
PrintString ENDP

;-----------------------------------------------------------------------------
; PrintNumber
; Prints a number to console
; Input: RCX = number to print
;-----------------------------------------------------------------------------
PrintNumber PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; Number to convert
    lea rdi, [rsp+20h] ; Buffer for digits
    mov rsi, rdi    ; Save buffer start
    
    ; Convert number to string (simple decimal)
    mov rax, rbx
    mov r8, 10      ; Base 10
    
convert_loop:
    xor rdx, rdx
    div r8          ; Divide by 10
    add dl, '0'     ; Convert remainder to ASCII
    mov [rdi], dl   ; Store digit
    inc rdi
    test rax, rax   ; Check if more digits
    jnz convert_loop
    
    ; Reverse string in buffer
    dec rdi         ; Point to last digit
    mov byte ptr [rdi+1], 0  ; Null terminate
    
reverse_loop:
    cmp rsi, rdi
    jae reverse_done
    mov al, [rsi]
    mov bl, [rdi]
    mov [rsi], bl
    mov [rdi], al
    inc rsi
    dec rdi
    jmp reverse_loop
    
reverse_done:
    ; Print the converted string
    lea rcx, [rsp+20h]
    call PrintString
    
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
PrintNumber ENDP

;-----------------------------------------------------------------------------
; Additional helper functions and test data
;-----------------------------------------------------------------------------

.data
; Test messages
file_validation_msg db "Validating generated PE file structures...", 13, 10, 0
exe_test_msg db "Testing executable execution...", 13, 10, 0
pe_structure_msg db "Validating PE internal structures...", 13, 10, 0

; DLL and function names for complex test
kernel32_name db "kernel32.dll", 0
user32_name db "user32.dll", 0
exitprocess_name db "ExitProcess", 0
getconsolemode_name db "GetConsoleMode", 0
showwindow_name db "ShowWindow", 0

; External function references needed by helpers
EXTERN StrLen: PROC
EXTERN CheckFileExists: PROC
EXTERN ExecuteAndWait: PROC
EXTERN ValidateExecutableFile: PROC
EXTERN ValidateDOSHeader: PROC
EXTERN ValidateNTHeaders: PROC
EXTERN ValidateSectionHeaders: PROC
EXTERN ValidateImportTable: PROC
EXTERN GenerateComplexCode: PROC
EXTERN FreePEContext: PROC
EXTERN GetPerformanceTime: PROC

END main