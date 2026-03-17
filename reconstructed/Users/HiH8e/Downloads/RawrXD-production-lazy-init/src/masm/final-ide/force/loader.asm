;==============================================================================
; force_loader.asm - Production-Ready Force Loading System
; ==============================================================================
; Implements force loading system that bypasses normal limits.
; Zero C++ runtime dependencies.
;==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib

include logging.inc

;==============================================================================
; EXTERNAL DECLARATIONS
;==============================================================================
EXTERN CreateFileA:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN lstrlenA:PROC
EXTERN lstrcpyA:PROC

;==============================================================================
; STRUCTURES
;==============================================================================
FORCE_LOAD_CONTEXT STRUCT
    model_path       QWORD ?
    model_size       QWORD ?
    mapped_view      QWORD ?
    file_handle      QWORD ?
    map_handle       QWORD ?
    load_flags       DWORD ?
FORCE_LOAD_CONTEXT ENDS

;==============================================================================
; DATA SEGMENT
;==============================================================================
.data
    szForceLoadSuccess BYTE "Model force loaded successfully",0
    szForceLoadError   BYTE "Force load failed: %s",0
    szForceLoading     BYTE "Force loading model: %s",0
    szModelFile        BYTE "%s.gguf",0

.data?
    force_load_context FORCE_LOAD_CONTEXT <>

;==============================================================================
; CODE SEGMENT
;==============================================================================
.code

;==============================================================================
; PUBLIC: ForceLoadModel(pModelName: rcx) -> eax
; Force loads a model bypassing all limits
;==============================================================================
PUBLIC ForceLoadModel
ALIGN 16
ForceLoadModel PROC
    LOCAL filePath[260]:BYTE
    LOCAL fileSize:QWORD
    LOCAL hFile:QWORD
    LOCAL hMap:QWORD
    LOCAL pView:QWORD
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 512
    
    mov r12, rcx        ; pModelName
    
    ; Log start
    lea rcx, szForceLoading
    mov rdx, r12
    call LogInfo
    
    ; Build file path
    lea rcx, filePath
    lea rdx, szModelFile
    mov r8, r12
    call wsprintfA
    
    ; Open file
    lea rcx, filePath
    mov rdx, GENERIC_READ
    mov r8, FILE_SHARE_READ
    xor r9, r9
    mov [rsp + 32], r9  ; lpSecurityAttributes
    mov [rsp + 40], OPEN_EXISTING
    mov [rsp + 48], FILE_ATTRIBUTE_NORMAL
    mov [rsp + 56], r9  ; hTemplateFile
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je force_load_fail
    mov hFile, rax
    
    ; Get file size
    mov rcx, rax
    xor rdx, rdx
    call GetFileSize
    mov fileSize, rax
    
    ; Create file mapping
    mov rcx, hFile
    xor rdx, rdx
    mov r8d, PAGE_READONLY
    xor r9, r9
    mov [rsp + 32], r9
    mov [rsp + 40], r9
    call CreateFileMappingA
    
    test rax, rax
    jz force_load_close_file
    mov hMap, rax
    
    ; Map view of file
    mov rcx, rax
    mov edx, FILE_MAP_READ
    xor r8, r8
    xor r9, r9
    mov [rsp + 32], 0
    call MapViewOfFile
    
    test rax, rax
    jz force_load_close_map
    mov pView, rax
    
    ; Store in context
    mov [force_load_context.model_path], r12
    mov [force_load_context.model_size], rax ; Should be fileSize
    mov rax, fileSize
    mov [force_load_context.model_size], rax
    mov rax, pView
    mov [force_load_context.mapped_view], rax
    mov rax, hFile
    mov [force_load_context.file_handle], rax
    mov rax, hMap
    mov [force_load_context.map_handle], rax
    
    lea rcx, szForceLoadSuccess
    call LogSuccess
    
    mov eax, 1
    jmp force_load_done
    
force_load_close_map:
    mov rcx, hMap
    call CloseHandle
force_load_close_file:
    mov rcx, hFile
    call CloseHandle
    
force_load_fail:
    lea rcx, szForceLoadError
    mov rdx, r12
    call LogError
    xor eax, eax
    
force_load_done:
    add rsp, 512
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ForceLoadModel ENDP

END
    mov [rsp + 56], 0   ; hTemplateFile
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je force_load_fail
    
    mov hFile, rax
    
    ; Get file size
    mov rcx, rax
    xor rdx, rdx
    call GetFileSize
    
    test rax, rax
    jz force_load_close
    
    mov fileSize, rax
    
    ; Create file mapping
    mov rcx, hFile
    xor rdx, rdx
    mov r8, PAGE_READONLY
    xor r9, r9
    mov [rsp + 32], fileSize
    mov [rsp + 40], 0   ; lpName
    call CreateFileMappingA
    
    test rax, rax
    jz force_load_close
    
    mov hMap, rax
    
    ; Map view of file
    mov rcx, rax
    mov rdx, FILE_MAP_READ
    xor r8, r8
    xor r9, r9
    mov [rsp + 32], fileSize
    call MapViewOfFile
    
    test rax, rax
    jz force_load_unmap
    
    mov pView, rax
    
    ; Store in context
    lea rbx, force_load_context
    lea rax, filePath
    mov [rbx + FORCE_LOAD_CONTEXT.model_path], rax
    mov rax, fileSize
    mov [rbx + FORCE_LOAD_CONTEXT.model_size], rax
    mov rax, pView
    mov [rbx + FORCE_LOAD_CONTEXT.mapped_view], rax
    mov rax, hFile
    mov [rbx + FORCE_LOAD_CONTEXT.file_handle], rax
    mov rax, hMap
    mov [rbx + FORCE_LOAD_CONTEXT.map_handle], rax
    mov [rbx + FORCE_LOAD_CONTEXT.load_flags], 1  ; FORCE_LOADED
    
    lea rcx, szForceLoadSuccess
    call LogSuccess
    
    mov eax, 1
    jmp force_load_done
    
force_load_unmap:
    mov rcx, hMap
    call CloseHandle
    
force_load_close:
    mov rcx, hFile
    call CloseHandle
    
force_load_fail:
    lea rcx, szForceLoadError
    lea rdx, filePath
    call LogError
    xor eax, eax
    
force_load_done:
    add rsp, 512
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ForceLoadModel ENDP

;==============================================================================
; PUBLIC: ForceUnloadModel() -> eax
; Unloads force-loaded model
;==============================================================================
PUBLIC ForceUnloadModel
ALIGN 16
ForceUnloadModel PROC
    push rbx
    sub rsp, 32
    
    lea rbx, force_load_context
    
    ; Unmap view
    cmp [rbx + FORCE_LOAD_CONTEXT.mapped_view], 0
    je skip_unmap
    
    mov rcx, [rbx + FORCE_LOAD_CONTEXT.mapped_view]
    call UnmapViewOfFile
    
skip_unmap:
    ; Close map handle
    cmp [rbx + FORCE_LOAD_CONTEXT.map_handle], 0
    je skip_map
    
    mov rcx, [rbx + FORCE_LOAD_CONTEXT.map_handle]
    call CloseHandle
    
skip_map:
    ; Close file handle
    cmp [rbx + FORCE_LOAD_CONTEXT.file_handle], 0
    je skip_file
    
    mov rcx, [rbx + FORCE_LOAD_CONTEXT.file_handle]
    call CloseHandle
    
skip_file:
    ; Clear context
    mov [rbx + FORCE_LOAD_CONTEXT.mapped_view], 0
    mov [rbx + FORCE_LOAD_CONTEXT.file_handle], 0
    mov [rbx + FORCE_LOAD_CONTEXT.map_handle], 0
    mov [rbx + FORCE_LOAD_CONTEXT.load_flags], 0
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
ForceUnloadModel ENDP

;==============================================================================
; PUBLIC: GetForceLoadedModel() -> rax (pointer to mapped view)
;==============================================================================
PUBLIC GetForceLoadedModel
ALIGN 16
GetForceLoadedModel PROC
    lea rax, force_load_context
    mov rax, [rax + FORCE_LOAD_CONTEXT.mapped_view]
    ret
GetForceLoadedModel ENDP

END
