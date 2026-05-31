; =============================================================================
; NativeInferenceClient.asm — Sovereign In-Process Inference Client
; Pure x64 MASM (ml64) — Replaces HTTP/Socket layer with direct memory mapping
; =============================================================================
; Build:   ml64 /c /nologo /Fo$(OutDir)NativeInferenceClient.obj NativeInferenceClient.asm
; Link:    link ... NativeInferenceClient.obj msvcrt.lib kernel32.lib
; =============================================================================
; Exports (C linkage):
;   bool NativeInferenceClient_Initialize(const wchar_t* modelPath);
;   int64_t NativeInferenceClient_Infer(const char* prompt, char* outBuf, size_t outSize);
;   void NativeInferenceClient_Shutdown(void);
; =============================================================================

include rawrxd_win64.inc
include rawrxd_crt.inc

; ---------------------------------------------------------------------------
; Win32 API externs not yet in rawrxd_win64.inc
; ---------------------------------------------------------------------------
EXTERN CreateFileW:PROC
EXTERN GetFileSizeEx:PROC
EXTERN CreateFileMappingW:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC

; ---------------------------------------------------------------------------
; Bridge externs (ai_agent_masm_bridge.hpp — C linkage, packed structs)
; ---------------------------------------------------------------------------
EXTERN masm_ai_tensor_simd_process:PROC

; ---------------------------------------------------------------------------
; Data
; ---------------------------------------------------------------------------
.data
align 8
PUBLIC g_ModelBasePtr
PUBLIC g_ModelSize
PUBLIC g_FileHandle
PUBLIC g_MappingHandle

g_ModelBasePtr      dq 0
g_ModelSize         dq 0
g_FileHandle        dq INVALID_HANDLE_VALUE
g_MappingHandle     dq 0

sz_init_ok          db "[Sovereign] Model mapped: %llu bytes", 10, 0
sz_init_fail        db "[Sovereign] Model map failed (GLE=%lu)", 10, 0
sz_infer_no_model   db "[Sovereign] Infer called with no model loaded", 10, 0

; AiMasmInferenceContext offsets (#pragma pack(1))
AIC_MODEL_BASE      equ 0
AIC_MODEL_SIZE      equ 8
AIC_INPUT_BUF       equ 16
AIC_OUTPUT_BUF      equ 24
AIC_TENSOR_SIZE     equ 32
AIC_BATCH_SIZE      equ 40
AIC_CALLBACK        equ 44
AIC_STRUCT_SIZE     equ 52

; MasmOperationResult offsets (#pragma pack(1))
MOR_SUCCESS         equ 0

; ---------------------------------------------------------------------------
; Code
; ---------------------------------------------------------------------------
.code

; =============================================================================
; NativeInferenceClient_Initialize
;   RCX = wchar_t* modelPath
;   Returns AL = 1 on success, 0 on failure
; =============================================================================
align 16
PUBLIC NativeInferenceClient_Initialize
NativeInferenceClient_Initialize PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    sub rsp, 56
    .allocstack 56
    .endprolog

    mov rbx, rcx                        ; RBX = modelPath

    ; Tear down any previous mapping
    call NativeInferenceClient_Shutdown

    ; CreateFileW(modelPath, GENERIC_READ, FILE_SHARE_READ, NULL,
    ;             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
    mov rcx, rbx
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9, r9
    mov qword ptr [rsp+32], OPEN_EXISTING
    mov qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+48], 0
    call CreateFileW
    cmp rax, INVALID_HANDLE_VALUE
    je _init_fail
    mov g_FileHandle, rax

    ; GetFileSizeEx(hFile, &g_ModelSize)
    lea rdx, g_ModelSize
    mov rcx, g_FileHandle
    call GetFileSizeEx
    test eax, eax
    jz _init_fail

    ; CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL)
    mov rcx, g_FileHandle
    xor rdx, rdx
    mov r8d, 2                      ; PAGE_READONLY
    xor r9, r9
    mov qword ptr [rsp+32], 0
    mov qword ptr [rsp+40], 0
    mov qword ptr [rsp+48], 0
    call CreateFileMappingW
    test rax, rax
    jz _init_fail
    mov g_MappingHandle, rax

    ; MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)
    mov rcx, g_MappingHandle
    mov edx, 4                      ; FILE_MAP_READ
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call MapViewOfFile
    test rax, rax
    jz _init_fail
    mov g_ModelBasePtr, rax

    ; Log success
    mov rdx, g_ModelSize
    lea rcx, sz_init_ok
    xor eax, eax
    call printf

    mov al, 1
    jmp _init_done

_init_fail:
    call GetLastError
    mov rdx, rax
    lea rcx, sz_init_fail
    xor eax, eax
    call printf
    call NativeInferenceClient_Shutdown
    xor al, al

_init_done:
    add rsp, 56
    pop rsi
    pop rdi
    pop rbx
    ret
NativeInferenceClient_Initialize ENDP


; =============================================================================
; NativeInferenceClient_Infer
;   RCX = char* prompt
;   RDX = char* outBuf
;   R8  = size_t outSize
;   Returns RAX = bytes written, or -1 on error
; =============================================================================
align 16
PUBLIC NativeInferenceClient_Infer
NativeInferenceClient_Infer PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    push r12
    .pushreg r12
    sub rsp, 104
    .allocstack 104
    .endprolog

    mov rbx, rcx                        ; RBX = prompt
    mov rdi, rdx                        ; RDI = outBuf
    mov rsi, r8                         ; RSI = outSize

    cmp qword ptr [g_ModelBasePtr], 0
    jne _infer_has_model

    lea rcx, sz_infer_no_model
    xor eax, eax
    call printf
    mov rax, -1
    jmp _infer_done

_infer_has_model:
    ; Build AiMasmInferenceContext on stack at [rsp+48]
    lea r12, [rsp+48]
    mov rax, g_ModelBasePtr
    mov qword ptr [r12+AIC_MODEL_BASE], rax
    mov rax, g_ModelSize
    mov qword ptr [r12+AIC_MODEL_SIZE], rax
    mov qword ptr [r12+AIC_INPUT_BUF], rbx
    mov qword ptr [r12+AIC_OUTPUT_BUF], rdi
    mov qword ptr [r12+AIC_TENSOR_SIZE], rsi
    mov dword ptr [r12+AIC_BATCH_SIZE], 1
    mov qword ptr [r12+AIC_CALLBACK], 0

    ; input_size = strlen(prompt)
    mov rcx, rbx
    call strlen

    ; Call masm_ai_tensor_simd_process(context, prompt, promptLen, outBuf, outSize)
    sub rsp, 40                         ; shadow + 1 stack arg
    mov qword ptr [rsp+32], rsi         ; 5th arg = outSize
    mov r9, rdi                         ; 4th arg = outBuf
    mov r8, rax                         ; 3rd arg = promptLen
    mov rdx, rbx                        ; 2nd arg = prompt
    mov rcx, r12                        ; 1st arg = context
    call masm_ai_tensor_simd_process
    add rsp, 40

    ; Check result.success
    mov al, byte ptr [rsp+48+MOR_SUCCESS]
    test al, al
    jz _infer_fail

    ; Return full buffer size on success (backend wrote into outBuf)
    mov rax, rsi
    jmp _infer_done

_infer_fail:
    mov rax, -1

_infer_done:
    add rsp, 104
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret
NativeInferenceClient_Infer ENDP


; =============================================================================
; NativeInferenceClient_Shutdown
; =============================================================================
align 16
PUBLIC NativeInferenceClient_Shutdown
NativeInferenceClient_Shutdown PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog

    cmp qword ptr [g_ModelBasePtr], 0
    je _no_view
    mov rcx, g_ModelBasePtr
    call UnmapViewOfFile
    mov g_ModelBasePtr, 0
_no_view:

    cmp qword ptr [g_MappingHandle], 0
    je _no_mapping
    mov rcx, g_MappingHandle
    call CloseHandle
    mov g_MappingHandle, 0
_no_mapping:

    cmp qword ptr [g_FileHandle], INVALID_HANDLE_VALUE
    je _no_file
    mov rcx, g_FileHandle
    call CloseHandle
    mov g_FileHandle, INVALID_HANDLE_VALUE
_no_file:

    add rsp, 32
    pop rbx
    ret
NativeInferenceClient_Shutdown ENDP

END
