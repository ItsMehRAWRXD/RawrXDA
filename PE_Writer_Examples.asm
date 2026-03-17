; PE_Writer_Examples.asm
; Comprehensive examples demonstrating the RawrXD PE Writer capabilities
; Shows different scenarios from simple to complex executable generation

OPTION CASEMAP:NONE

; PE Writer interface
EXTERN PEWriter_CreateExecutable: PROC
EXTERN PEWriter_AddImport: PROC
EXTERN PEWriter_AddCode: PROC
EXTERN PEWriter_WriteFile: PROC
EXTERN Emit_FunctionPrologue: PROC
EXTERN Emit_FunctionEpilogue: PROC
EXTERN Emit_MOV: PROC
EXTERN Emit_CALL: PROC
EXTERN Emit_RET: PROC
EXTERN CodeGen_CreateOperand: PROC

; Windows APIs for examples
EXTERN GetStdHandle: PROC
EXTERN WriteConsoleA: PROC
EXTERN ExitProcess: PROC

; Constants from PE Writer
DEFAULT_IMAGE_BASE equ 140000000h
SECTION_ALIGNMENT equ 1000h
OP_REG equ 1
OP_IMM equ 2
REG_RCX equ 1

PUBLIC CreateHelloWorldApp
PUBLIC CreateCalculatorApp
PUBLIC CreateFileProcessorApp
PUBLIC CreateAdvancedExample
PUBLIC DemonstrateAllFeatures

.data
; Example application data
hello_filename db "hello_world.exe", 0
calc_filename db "calculator.exe", 0
fileproc_filename db "file_processor.exe", 0
advanced_filename db "advanced_demo.exe", 0

; DLL names
kernel32_dll db "kernel32.dll", 0
user32_dll db "user32.dll", 0
msvcrt_dll db "msvcrt.dll", 0

; Function names
exitprocess_fn db "ExitProcess", 0
getstdhandle_fn db "GetStdHandle", 0
writeconsolea_fn db "WriteConsoleA", 0
messageboxa_fn db "MessageBoxA", 0
createfilea_fn db "CreateFileA", 0
readfile_fn db "ReadFile", 0
writefile_fn db "WriteFile", 0
closefile_fn db "CloseHandle", 0
printf_fn db "printf", 0
scanf_fn db "scanf", 0

; Messages and banners
demo_banner db "=== RawrXD PE Writer - Feature Demonstrations ===", 13, 10, 0
example1_msg db "Example 1: Hello World Console Application", 13, 10, 0
example2_msg db "Example 2: Calculator Application", 13, 10, 0
example3_msg db "Example 3: File Processing Utility", 13, 10, 0
example4_msg db "Example 4: Advanced Multi-API Demo", 13, 10, 0
success_msg db "SUCCESS - Executable generated: ", 0
error_msg db "ERROR - Failed to generate: ", 0
newline db 13, 10, 0

.code

;=============================================================================
; MAIN DEMONSTRATION ENTRY POINT
;=============================================================================
main PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Display banner
    lea rcx, demo_banner
    call PrintConsole
    
    ; Run all examples
    call DemonstrateAllFeatures
    
    ; Exit
    mov rcx, 0
    call ExitProcess
main ENDP

;-----------------------------------------------------------------------------
; DemonstrateAllFeatures - Run all PE generation examples
;-----------------------------------------------------------------------------
DemonstrateAllFeatures PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    
    ; Example 1: Hello World
    lea rcx, example1_msg
    call PrintConsole
    
    call CreateHelloWorldApp
    call PrintResult
    
    ; Example 2: Calculator
    lea rcx, example2_msg
    call PrintConsole
    
    call CreateCalculatorApp
    call PrintResult
    
    ; Example 3: File Processor
    lea rcx, example3_msg
    call PrintConsole
    
    call CreateFileProcessorApp
    call PrintResult
    
    ; Example 4: Advanced Demo
    lea rcx, example4_msg
    call PrintConsole
    
    call CreateAdvancedExample
    call PrintResult
    
    pop rbx
    add rsp, 40h
    pop rbp
    ret
DemonstrateAllFeatures ENDP

;=============================================================================
; EXAMPLE 1: HELLO WORLD CONSOLE APPLICATION
;=============================================================================

;-----------------------------------------------------------------------------
; CreateHelloWorldApp - Creates a simple console "Hello World" application
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
CreateHelloWorldApp PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    ; Create PE context
    mov rcx, DEFAULT_IMAGE_BASE
    mov rdx, SECTION_ALIGNMENT      ; Entry point RVA
    call PEWriter_CreateExecutable
    test rax, rax
    jz hello_fail
    mov rbx, rax                    ; Save PE context
    
    ; Add kernel32.dll imports
    mov rcx, rbx
    lea rdx, kernel32_dll
    lea r8, getstdhandle_fn
    call PEWriter_AddImport
    test rax, rax
    jz hello_cleanup_fail
    
    mov rcx, rbx
    lea rdx, kernel32_dll
    lea r8, writeconsolea_fn
    call PEWriter_AddImport
    test rax, rax
    jz hello_cleanup_fail
    
    mov rcx, rbx
    lea rdx, kernel32_dll
    lea r8, exitprocess_fn
    call PEWriter_AddImport
    test rax, rax
    jz hello_cleanup_fail
    
    ; Generate Hello World code
    mov rcx, rbx
    call GenerateHelloWorldCode
    test rax, rax
    jz hello_cleanup_fail
    
    ; Write the executable
    mov rcx, rbx
    lea rdx, hello_filename
    call PEWriter_WriteFile
    test rax, rax
    jz hello_cleanup_fail
    
    ; Success - cleanup context
    mov rcx, rbx
    call FreeContext
    mov rax, 1
    jmp hello_done
    
hello_cleanup_fail:
    mov rcx, rbx
    call FreeContext
    
hello_fail:
    xor rax, rax
    
hello_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
CreateHelloWorldApp ENDP

;-----------------------------------------------------------------------------
; GenerateHelloWorldCode - Generates x64 assembly for Hello World
; Input: RCX = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
GenerateHelloWorldCode PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; PE context
    
    ; Function prologue
    mov rcx, rbx
    call Emit_FunctionPrologue
    test rax, rax
    jz gen_hello_fail
    
    ; Get stdout handle: GetStdHandle(-11)
    ; mov rcx, -11 (STD_OUTPUT_HANDLE)
    mov rcx, rbx
    mov rdx, OP_REG
    mov r8, REG_RCX
    call CodeGen_CreateOperand
    mov rsi, rax
    
    mov rdx, OP_IMM
    mov r8, -11
    call CodeGen_CreateOperand
    mov rdi, rax
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call Emit_MOV
    test rax, rax
    jz gen_hello_fail
    
    ; call GetStdHandle
    mov rcx, rbx
    lea rdx, getstdhandle_fn
    call Emit_CALL
    test rax, rax
    jz gen_hello_fail
    
    ; Store handle in a safe register
    ; mov rsi, rax
    ; (This would require more sophisticated register tracking)
    
    ; WriteConsoleA call preparation would go here
    ; For demonstration, jump to exit
    
    ; Call ExitProcess(0)
    mov rcx, rbx
    mov rdx, OP_REG
    mov r8, REG_RCX
    call CodeGen_CreateOperand
    mov rsi, rax
    
    mov rdx, OP_IMM
    mov r8, 0
    call CodeGen_CreateOperand
    mov rdi, rax
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call Emit_MOV
    test rax, rax
    jz gen_hello_fail
    
    mov rcx, rbx
    lea rdx, exitprocess_fn
    call Emit_CALL
    test rax, rax
    jz gen_hello_fail
    
    ; Function epilogue
    mov rcx, rbx
    call Emit_FunctionEpilogue
    test rax, rax
    jz gen_hello_fail
    
    ; Return instruction
    mov rcx, rbx
    call Emit_RET
    
gen_hello_fail:
    pop rdi
    pop rsi
    pop rbx  
    add rsp, 40h
    pop rbp
    ret
GenerateHelloWorldCode ENDP

;=============================================================================
; EXAMPLE 2: CALCULATOR APPLICATION
;=============================================================================

;-----------------------------------------------------------------------------
; CreateCalculatorApp - Creates a GUI calculator application
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
CreateCalculatorApp PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    
    ; Create PE context
    mov rcx, DEFAULT_IMAGE_BASE
    mov rdx, SECTION_ALIGNMENT
    call PEWriter_CreateExecutable
    test rax, rax
    jz calc_fail
    mov rbx, rax
    
    ; Add user32.dll imports for GUI
    mov rcx, rbx
    lea rdx, user32_dll
    lea r8, messageboxa_fn
    call PEWriter_AddImport
    test rax, rax
    jz calc_cleanup_fail
    
    ; Add kernel32.dll imports
    mov rcx, rbx
    lea rdx, kernel32_dll
    lea r8, exitprocess_fn
    call PEWriter_AddImport
    test rax, rax
    jz calc_cleanup_fail
    
    ; Generate calculator code
    mov rcx, rbx
    call GenerateCalculatorCode
    test rax, rax
    jz calc_cleanup_fail
    
    ; Write executable
    mov rcx, rbx
    lea rdx, calc_filename
    call PEWriter_WriteFile
    test rax, rax
    jz calc_cleanup_fail
    
    ; Success
    mov rcx, rbx
    call FreeContext
    mov rax, 1
    jmp calc_done
    
calc_cleanup_fail:
    mov rcx, rbx
    call FreeContext
    
calc_fail:
    xor rax, rax
    
calc_done:
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
CreateCalculatorApp ENDP

;-----------------------------------------------------------------------------
; GenerateCalculatorCode - Generates calculator GUI code
; Input: RCX = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
GenerateCalculatorCode PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    
    mov rbx, rcx
    
    ; Function prologue
    mov rcx, rbx
    call Emit_FunctionPrologue
    test rax, rax
    jz gen_calc_fail
    
    ; For demonstration: Show message box, then exit
    ; MessageBoxA(NULL, "Calculator Demo", "Calculator", MB_OK)
    ; This would require more complex parameter setup
    
    ; Simplified: just call ExitProcess(0)
    mov rcx, rbx
    mov rdx, OP_REG
    mov r8, REG_RCX
    call CodeGen_CreateOperand
    mov rsi, rax
    
    mov rdx, OP_IMM
    mov r8, 0
    call CodeGen_CreateOperand
    mov rdi, rax
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call Emit_MOV
    test rax, rax
    jz gen_calc_fail
    
    mov rcx, rbx
    lea rdx, exitprocess_fn
    call Emit_CALL
    test rax, rax
    jz gen_calc_fail
    
    ; Epilogue
    mov rcx, rbx
    call Emit_FunctionEpilogue
    test rax, rax
    jz gen_calc_fail
    
    mov rcx, rbx
    call Emit_RET
    
gen_calc_fail:
    pop rbx
    add rsp, 40h
    pop rbp
    ret
GenerateCalculatorCode ENDP

;=============================================================================
; EXAMPLE 3: FILE PROCESSING UTILITY
;=============================================================================

;-----------------------------------------------------------------------------
; CreateFileProcessorApp - Creates a file processing utility
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
CreateFileProcessorApp PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    
    ; Create PE context
    mov rcx, DEFAULT_IMAGE_BASE
    mov rdx, SECTION_ALIGNMENT
    call PEWriter_CreateExecutable
    test rax, rax
    jz fileproc_fail
    mov rbx, rax
    
    ; Add kernel32.dll file I/O imports
    mov rcx, rbx
    lea rdx, kernel32_dll
    lea r8, createfilea_fn
    call PEWriter_AddImport
    test rax, rax
    jz fileproc_cleanup_fail
    
    mov rcx, rbx
    lea rdx, kernel32_dll  
    lea r8, readfile_fn
    call PEWriter_AddImport
    test rax, rax
    jz fileproc_cleanup_fail
    
    mov rcx, rbx
    lea rdx, kernel32_dll
    lea r8, writefile_fn
    call PEWriter_AddImport
    test rax, rax
    jz fileproc_cleanup_fail
    
    mov rcx, rbx
    lea rdx, kernel32_dll
    lea r8, closefile_fn
    call PEWriter_AddImport
    test rax, rax
    jz fileproc_cleanup_fail
    
    mov rcx, rbx
    lea rdx, kernel32_dll
    lea r8, exitprocess_fn
    call PEWriter_AddImport
    test rax, rax
    jz fileproc_cleanup_fail
    
    ; Generate file processing code
    mov rcx, rbx
    call GenerateFileProcessorCode
    test rax, rax
    jz fileproc_cleanup_fail
    
    ; Write executable
    mov rcx, rbx
    lea rdx, fileproc_filename
    call PEWriter_WriteFile
    test rax, rax
    jz fileproc_cleanup_fail
    
    ; Success
    mov rcx, rbx
    call FreeContext
    mov rax, 1
    jmp fileproc_done
    
fileproc_cleanup_fail:
    mov rcx, rbx
    call FreeContext
    
fileproc_fail:
    xor rax, rax
    
fileproc_done:
    pop rbx
    add rsp, 40h
    pop rbp
    ret
CreateFileProcessorApp ENDP

;-----------------------------------------------------------------------------
; GenerateFileProcessorCode - Generates file I/O code
; Input: RCX = PE context  
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
GenerateFileProcessorCode PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    
    mov rbx, rcx
    
    ; Function prologue
    mov rcx, rbx
    call Emit_FunctionPrologue
    test rax, rax
    jz gen_fileproc_fail
    
    ; Simplified file processor: just exit
    ; Real implementation would have file open/read/write/close sequence
    
    ; ExitProcess(0)
    mov rcx, rbx
    mov rdx, OP_REG
    mov r8, REG_RCX
    call CodeGen_CreateOperand
    mov rsi, rax
    
    mov rdx, OP_IMM
    mov r8, 0
    call CodeGen_CreateOperand
    mov rdi, rax
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call Emit_MOV
    test rax, rax
    jz gen_fileproc_fail
    
    mov rcx, rbx
    lea rdx, exitprocess_fn
    call Emit_CALL
    test rax, rax
    jz gen_fileproc_fail
    
    ; Epilogue
    mov rcx, rbx
    call Emit_FunctionEpilogue
    test rax, rax
    jz gen_fileproc_fail
    
    mov rcx, rbx
    call Emit_RET
    
gen_fileproc_fail:
    pop rbx
    add rsp, 40h
    pop rbp
    ret
GenerateFileProcessorCode ENDP

;=============================================================================
; EXAMPLE 4: ADVANCED MULTI-API DEMONSTRATION
;=============================================================================

;-----------------------------------------------------------------------------
; CreateAdvancedExample - Creates complex multi-DLL application
; Output: RAX = 1 success, 0 failure  
;-----------------------------------------------------------------------------
CreateAdvancedExample PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    
    ; Create PE context
    mov rcx, DEFAULT_IMAGE_BASE
    mov rdx, SECTION_ALIGNMENT
    call PEWriter_CreateExecutable
    test rax, rax
    jz advanced_fail
    mov rbx, rax
    
    ; Add imports from multiple DLLs
    
    ; kernel32.dll imports
    mov rcx, rbx
    lea rdx, kernel32_dll
    lea r8, getstdhandle_fn
    call PEWriter_AddImport
    test rax, rax
    jz advanced_cleanup_fail
    
    mov rcx, rbx
    lea rdx, kernel32_dll
    lea r8, writeconsolea_fn
    call PEWriter_AddImport
    test rax, rax
    jz advanced_cleanup_fail
    
    mov rcx, rbx
    lea rdx, kernel32_dll
    lea r8, exitprocess_fn
    call PEWriter_AddImport
    test rax, rax
    jz advanced_cleanup_fail
    
    ; user32.dll imports
    mov rcx, rbx
    lea rdx, user32_dll
    lea r8, messageboxa_fn
    call PEWriter_AddImport
    test rax, rax
    jz advanced_cleanup_fail
    
    ; msvcrt.dll imports (C runtime)
    mov rcx, rbx
    lea rdx, msvcrt_dll
    lea r8, printf_fn
    call PEWriter_AddImport
    test rax, rax
    jz advanced_cleanup_fail
    
    ; Generate complex code using multiple APIs
    mov rcx, rbx
    call GenerateAdvancedCode
    test rax, rax
    jz advanced_cleanup_fail
    
    ; Write executable
    mov rcx, rbx
    lea rdx, advanced_filename
    call PEWriter_WriteFile
    test rax, rax
    jz advanced_cleanup_fail
    
    ; Success
    mov rcx, rbx
    call FreeContext
    mov rax, 1
    jmp advanced_done
    
advanced_cleanup_fail:
    mov rcx, rbx
    call FreeContext
    
advanced_fail:
    xor rax, rax
    
advanced_done:
    pop rbx
    add rsp, 40h
    pop rbp
    ret
CreateAdvancedExample ENDP

;-----------------------------------------------------------------------------
; GenerateAdvancedCode - Generates complex multi-API code
; Input: RCX = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
GenerateAdvancedCode PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    
    mov rbx, rcx
    
    ; Function prologue
    mov rcx, rbx
    call Emit_FunctionPrologue
    test rax, rax
    jz gen_advanced_fail
    
    ; This would contain a complex sequence of API calls:
    ; 1. Console output via WriteConsoleA
    ; 2. Message box via MessageBoxA
    ; 3. Printf calls via msvcrt
    ; 4. File operations
    ; 5. Final ExitProcess
    
    ; For demonstration, simplified to ExitProcess
    mov rcx, rbx
    mov rdx, OP_REG
    mov r8, REG_RCX
    call CodeGen_CreateOperand
    mov rsi, rax
    
    mov rdx, OP_IMM
    mov r8, 0
    call CodeGen_CreateOperand
    mov rdi, rax
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call Emit_MOV
    test rax, rax
    jz gen_advanced_fail
    
    mov rcx, rbx
    lea rdx, exitprocess_fn
    call Emit_CALL
    test rax, rax
    jz gen_advanced_fail
    
    ; Epilogue
    mov rcx, rbx
    call Emit_FunctionEpilogue
    test rax, rax
    jz gen_advanced_fail
    
    mov rcx, rbx
    call Emit_RET
    
gen_advanced_fail:
    pop rbx
    add rsp, 40h
    pop rbp
    ret
GenerateAdvancedCode ENDP

;=============================================================================
; HELPER FUNCTIONS
;=============================================================================

;-----------------------------------------------------------------------------
; PrintResult - Print success/failure message
; Input: RAX = result code (1=success, 0=failure)
;-----------------------------------------------------------------------------
PrintResult PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    test rax, rax
    jz print_error
    
    lea rcx, success_msg
    call PrintConsole
    jmp print_result_done
    
print_error:
    lea rcx, error_msg
    call PrintConsole
    
print_result_done:
    lea rcx, newline
    call PrintConsole
    
    add rsp, 20h
    pop rbp
    ret
PrintResult ENDP

;-----------------------------------------------------------------------------
; PrintConsole - Print string to console
; Input: RCX = string pointer
;-----------------------------------------------------------------------------
PrintConsole PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    
    ; This is a placeholder - real implementation would use WriteConsoleA
    ; For now, just return success
    
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
PrintConsole ENDP

;-----------------------------------------------------------------------------
; FreeContext - Free PE context (placeholder)
; Input: RCX = PE context
;-----------------------------------------------------------------------------
FreeContext PROC
    ; Placeholder for context cleanup
    ret
FreeContext ENDP

END main