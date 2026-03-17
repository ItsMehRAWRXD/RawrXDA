;==============================================================================
; RawrXD GGUF Inference Engine - x64 Assembly
;==============================================================================

OPTION CASEMAP:NONE

;==============================================================================
; Constants
;==============================================================================

GGUF_MAGIC equ 0x46554747
GGUF_VERSION equ 3

;==============================================================================
; External Functions
;==============================================================================

EXTERN CreateFileA:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSizeEx:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memcpy:PROC
EXTERN TlsAlloc:PROC
EXTERN TlsFree:PROC

;==============================================================================
; Globals
;==============================================================================

.DATA?

gTlsIndex DWORD ?
gModelCache QWORD ?
gRopeTableSin QWORD ?
gRopeTableCos QWORD ?
gExpTable QWORD ?
gLogTable QWORD ?
gTempBuffer QWORD ?

;==============================================================================
; DllMain
;==============================================================================

.CODE

DllMain PROC hInstance:QWORD, dwReason:QWORD, lpReserved:QWORD

    mov rax, rdx
    mov ecx, dword ptr [rax]
    
    cmp rcx, 1
    je @@attach
    cmp rcx, 0
    je @@detach
    mov eax, 1
    ret
    
@@attach:
    mov eax, 1
    ret
    
@@detach:
    mov eax, 1
    ret

DllMain ENDP

;==============================================================================
; Simple test procedure
;==============================================================================

LoadModelNative PROC

    ; RCX = pCtx, RDX = pFilePath
    mov rbx, rcx
    mov r12, rdx
    
    ; Open file
    mov rcx, r12
    mov edx, 80000000h
    mov r8d, 1
    xor r9, r9
    call CreateFileA
    
    cmp rax, -1
    je @@err
    mov r13, rax
    
    ; Get file size
    mov rcx, r13
    lea rdx, [rsp]
    call GetFileSizeEx
    test eax, eax
    jz @@err
    
    ; Create mapping
    mov rcx, r13
    xor rdx, rdx
    mov r8d, 2
    xor r9, r9
    call CreateFileMappingA
    
    test rax, rax
    jz @@err
    mov r15, rax
    
    ; Map view
    mov rcx, r15
    mov edx, 4
    xor r8, r8
    xor r9, r9
    call MapViewOfFile
    
    test rax, rax
    jz @@err
    mov rsi, rax
    
    ; Validate GGUF magic at offset 0
    cmp DWORD PTR [rsi], 0x46554747
    jne @@err
    
    ; Store handles in context
    mov QWORD PTR [rbx], r13
    mov QWORD PTR [rbx+8], r15
    mov QWORD PTR [rbx+16], rsi
    
    mov eax, 1
    ret
    
@@err:
    xor eax, eax
    ret

LoadModelNative ENDP

END
