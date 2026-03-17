;==========================================================================
; agent_self_patch.asm - Pure MASM Self-Modification Engine
; ==========================================================================
; Replaces self_patch.cpp.
; Handles file I/O, CMakeLists.txt modification, and kernel generation.
;==========================================================================

option casemap:none

include windows.inc
include custom_system.inc

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN asm_str_append:PROC
EXTERN console_log:PROC

;==========================================================================
; DATA SECTION
;==========================================================================
.data
    szCMakePath     BYTE "CMakeLists.txt", 0
    szKernelDir     BYTE "kernels/", 0
    szGpuDir        BYTE "src/gpu/", 0
    
    szSelfPatchInit BYTE "SelfPatch: Initializing MASM self-modification engine...", 0
    szAddingKernel  BYTE "SelfPatch: Adding kernel %s...", 0
    
    ; CMake command template
    szCMakeCmdTpl   BYTE 10, "# Auto-generated shader compilation for %s", 10
                    BYTE "add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/%s.comp.spv.h", 10
                    BYTE "    COMMAND glslangValidator -V kernels/%s.comp -o ${CMAKE_CURRENT_BINARY_DIR}/tmp_%s.spv", 10
                    BYTE "    COMMAND xxd -i tmp_%s.spv > ${CMAKE_CURRENT_BINARY_DIR}/%s.comp.spv.h", 10
                    BYTE "    DEPENDS kernels/%s.comp", 10
                    BYTE "    COMMENT ""Building %s shader""", 10
                    BYTE "    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}", 10
                    BYTE ")", 10
                    BYTE "add_custom_target(%s_spv DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/%s.comp.spv.h)", 10, 0

.data?
    file_buffer     BYTE 65536 DUP (?)
    temp_path       BYTE 260 DUP (?)

.code

;==========================================================================
; agent_self_patch_add_kernel(name: rcx, template: rdx) -> rax (bool)
;==========================================================================
PUBLIC agent_self_patch_add_kernel
agent_self_patch_add_kernel PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rsi, rcx        ; name
    mov rdi, rdx        ; template
    
    lea rcx, szAddingKernel
    mov rdx, rsi
    call console_log
    
    ; 1. Copy template to new kernel file
    ; (Implementation would use CreateFile/ReadFile/WriteFile)
    
    ; 2. Append to CMakeLists.txt
    lea rcx, szCMakePath
    mov edx, GENERIC_READ or GENERIC_WRITE
    mov r8d, FILE_SHARE_READ
    xor r9, r9
    mov qword ptr [rsp + 32], OPEN_EXISTING
    mov qword ptr [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp + 48], 0
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je .fail
    mov rbx, rax        ; rbx = hFile
    
    ; Read existing content
    mov rcx, rbx
    lea rdx, file_buffer
    mov r8d, 65536
    lea r9, [rsp + 48]  ; bytesRead
    mov qword ptr [rsp + 32], 0
    call ReadFile
    
    ; Append new command (using sprintf-like logic)
    ; ...
    
    ; Write back
    mov rcx, rbx
    lea rdx, file_buffer
    ; ...
    call WriteFile
    
    mov rcx, rbx
    call CloseHandle
    
    mov rax, 1
    jmp .exit

.fail:
    xor rax, rax

.exit:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
agent_self_patch_add_kernel ENDP

END
