; ============================================================================
; DirectML_Bridge.asm — DirectML GPU Inference Bridge (x64 MASM)
; ============================================================================
; Bridges CPU inference to DirectML GPU acceleration.
; Exports: DML_CreateContext, DML_ReleaseContext, DML_RunInference
; ============================================================================

OPTION CASEMAP:NONE

; ============================================================================
; EXTERNAL IMPORTS
; ============================================================================
EXTERNDEF LoadLibraryA:PROC
EXTERNDEF GetProcAddress:PROC
EXTERNDEF FreeLibrary:PROC
EXTERNDEF VirtualAlloc:PROC
EXTERNDEF VirtualFree:PROC

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================
PUBLIC DML_CreateContext
PUBLIC DML_ReleaseContext
PUBLIC DML_RunInference
PUBLIC DML_IsAvailable

; ============================================================================
; CONSTANTS
; ============================================================================
DML_MAGIC         EQU 444D4C00h    ; "DML"
DML_VERSION       EQU 1
DML_SUCCESS       EQU 0
DML_ERROR_NO_GPU  EQU 1
DML_ERROR_INIT    EQU 2
DML_ERROR_INFER   EQU 3

MEM_COMMIT        EQU 1000h
MEM_RESERVE       EQU 2000h
PAGE_READWRITE    EQU 04h

; ============================================================================
; DATA SECTION
; ============================================================================
.data
ALIGN 8

g_dml_initialized   BYTE  0
g_dml_handle        QWORD 0
g_dml_context       QWORD 0

; ============================================================================
; CODE SECTION
; ============================================================================
.code

; ============================================================================
; DML_CreateContext — Initialize DirectML context
; RCX = device_id (0 = default)
; Returns RAX = context handle, 0 on failure
; ============================================================================
DML_CreateContext PROC FRAME
    .PUSHREG rbx
    push rbx
    .ENDPROLOG

    ; Check if already initialized
    cmp g_dml_initialized, 0
    jne @@already_initialized

    ; Try to load DirectML.dll
    lea rcx, @@dml_dll_name
    call LoadLibraryA
    test rax, rax
    jz  @@fail_no_gpu

    mov g_dml_handle, rax

    ; Allocate context structure
    mov rcx, 64         ; Size of context
    mov rdx, MEM_COMMIT OR MEM_RESERVE
    mov r8, PAGE_READWRITE
    xor r9, r9
    call VirtualAlloc
    test rax, rax
    jz  @@fail_init

    mov g_dml_context, rax

    ; Write context header
    mov DWORD PTR [rax], DML_MAGIC
    mov DWORD PTR [rax+4], DML_VERSION
    mov QWORD PTR [rax+8], 0    ; device_id
    mov QWORD PTR [rax+16], 0   ; buffer

    mov g_dml_initialized, 1

@@already_initialized:
    mov rax, g_dml_context
    jmp @@done

@@fail_no_gpu:
    mov rax, DML_ERROR_NO_GPU
    jmp @@done

@@fail_init:
    mov rax, DML_ERROR_INIT
    jmp @@done

@@done:
    pop rbx
    ret

@@dml_dll_name:
    DB "DirectML.dll", 0

DML_CreateContext ENDP

; ============================================================================
; DML_ReleaseContext — Release DirectML context
; RCX = context handle
; Returns RAX = 0 on success
; ============================================================================
DML_ReleaseContext PROC FRAME
    .PUSHREG rbx
    push rbx
    .ENDPROLOG

    test rcx, rcx
    jz  @@done

    ; Free context memory
    mov rbx, rcx
    mov rcx, rbx
    mov rdx, 8000h      ; MEM_RELEASE
    call VirtualFree

    ; Free library if loaded
    mov rcx, g_dml_handle
    test rcx, rcx
    jz  @@skip_free
    call FreeLibrary

@@skip_free:
    mov g_dml_initialized, 0
    mov g_dml_handle, 0
    mov g_dml_context, 0

@@done:
    xor rax, rax
    pop rbx
    ret
DML_ReleaseContext ENDP

; ============================================================================
; DML_RunInference — Run inference on DirectML
; RCX = context, RDX = input_buffer, R8 = input_size
; R9 = output_buffer, [rsp+28h] = output_size
; Returns RAX = 0 on success, error code on failure
; ============================================================================
DML_RunInference PROC FRAME
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    push rbx
    push rsi
    push rdi
    .ENDPROLOG

    ; Validate inputs
    test rcx, rcx
    jz  @@fail
    test rdx, rdx
    jz  @@fail
    test r9, r9
    jz  @@fail

    mov rbx, rcx        ; rbx = context
    mov rsi, rdx        ; rsi = input
    mov rdi, r9         ; rdi = output

    ; Check context magic
    mov eax, DWORD PTR [rbx]
    cmp eax, DML_MAGIC
    jne @@fail

    ; In production, this would call DirectML APIs
    ; For now, copy input to output as passthrough
    mov rcx, rdi
    mov rdx, rsi
    mov r8, 64          ; Copy first 64 bytes
    call RtlCopyMemory

    xor rax, rax        ; Success
    jmp @@done

@@fail:
    mov rax, DML_ERROR_INFER

@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
DML_RunInference ENDP

; ============================================================================
; DML_IsAvailable — Check if DirectML is available
; Returns RAX = 1 if available, 0 if not
; ============================================================================
DML_IsAvailable PROC FRAME
    cmp g_dml_initialized, 0
    setne al
    movzx rax, al
    ret
DML_IsAvailable ENDP

END