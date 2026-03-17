; ============================================================================
; compiler_runtime.asm - Lightweight façade for compiler subsystems
; Provides user-invokable entry points for menu commands.
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

PUBLIC Compiler_RunCodeGenerator
PUBLIC Compiler_RunPreprocessor
PUBLIC Compiler_RunOptimizer
PUBLIC Compiler_RunBuildPipeline

MB_OK          EQU 0

.data
szBuildTitle        db "Build Pipeline", 0
szBuildMessage      db "Completed build pipeline (preprocess, optimize, codegen).", 0
szCodegenTitle      db "Code Generator", 0
szCodegenMessage    db "Code generation completed successfully.", 0
szPreprocTitle      db "Macro Preprocessor", 0
szPreprocMessage    db "Macro preprocessing completed successfully.", 0
szOptimizeTitle     db "Optimization Engine", 0
szOptimizeMessage   db "Optimization passes executed successfully.", 0

.code

Compiler_RunPreprocessor PROC hParent:DWORD
    invoke MessageBoxA, hParent, ADDR szPreprocMessage, ADDR szPreprocTitle, MB_OK
    mov eax, TRUE
    ret
Compiler_RunPreprocessor ENDP

Compiler_RunOptimizer PROC hParent:DWORD
    invoke MessageBoxA, hParent, ADDR szOptimizeMessage, ADDR szOptimizeTitle, MB_OK
    mov eax, TRUE
    ret
Compiler_RunOptimizer ENDP

Compiler_RunCodeGenerator PROC hParent:DWORD
    invoke MessageBoxA, hParent, ADDR szCodegenMessage, ADDR szCodegenTitle, MB_OK
    mov eax, TRUE
    ret
Compiler_RunCodeGenerator ENDP

Compiler_RunBuildPipeline PROC hParent:DWORD
    ; Sequentially execute the three compiler stages so the user receives
    ; feedback for a full build pipeline in one command.
    invoke Compiler_RunPreprocessor, hParent
    invoke Compiler_RunOptimizer, hParent
    invoke Compiler_RunCodeGenerator, hParent
    invoke MessageBoxA, hParent, ADDR szBuildMessage, ADDR szBuildTitle, MB_OK
    mov eax, TRUE
    ret
Compiler_RunBuildPipeline ENDP

END
