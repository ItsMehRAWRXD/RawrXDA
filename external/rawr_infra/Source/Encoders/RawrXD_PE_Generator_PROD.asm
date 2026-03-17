; ================================================================================
; RawrXD PE Generator - Production Implementation
; Simplified, streamlined PE generation in pure x64 assembly
; ================================================================================

OPTION CASEMAP:NONE
OPTION DOTNAME

; External Windows API references
EXTERNDEF __imp_VirtualAlloc:QWORD
EXTERNDEF __imp_VirtualFree:QWORD
EXTERNDEF __imp_CreateFileA:QWORD
EXTERNDEF __imp_WriteFile:QWORD
EXTERNDEF __imp_CloseHandle:QWORD

; ================================================================================
; CONSTANTS
; ================================================================================

PE_MAGIC                EQU 5A4Dh          ; "MZ"
PE_SIGNATURE            EQU 00004550h      ; "PE\0\0"
MACHINE_AMD64           EQU 8664h
OPTIONAL_HDR64_MAGIC    EQU 20Bh

IMAGE_FILE_EXECUTABLE_IMAGE EQU 0002h
IMAGE_FILE_LARGE_ADDRESS_AWARE EQU 0020h
IMAGE_SUBSYSTEM_WINDOWS_CUI EQU 3
IMAGE_DLLCHARACTERISTICS_NX_COMPAT EQU 0100h
IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE EQU 0040h

IMAGE_SCN_CNT_CODE          EQU 00000020h
IMAGE_SCN_CNT_INITIALIZED_DATA EQU 00000040h
IMAGE_SCN_MEM_EXECUTE       EQU 20000000h
IMAGE_SCN_MEM_READ          EQU 40000000h
IMAGE_SCN_MEM_WRITE         EQU 80000000h

; Encoder constants
ENCODER_XOR_KEY         EQU 0DEADBEEFCAFEBABEh
ENCODER_ROL_BITS        EQU 7

.code

ALIGN 16

; ================================================================================
; PeGen_Initialize - Initialize context and allocate bufferntext
; RDX = BufferSize
; Returns EAX = 1 (success) or 0 (failure)
PeGen_Initialize PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    .PUSHREG r12
    .PUSHREG r13
    .ENDPROLOG
    
    mov     r12, rcx
    mov     r13, rdx
    
    ; Zero context (sizeof PEGENCTX = 112 bytes)
    mov     rdi, r12
    mov     rcx, 112
    xor     eax, eax
    rep     stosb
    
    ; Allocate buffer RWX
    xor     rcx, rcx
    mov     rdx, r13
    mov     r8, 3000h
    mov     r9, 40h
    call    qword ptr [__imp_VirtualAlloc]
    
    test    rax, rax
    jz      @@fail

    ; Store in context
    mov     [r12], rax      ; pBuffer
    mov     [r12+8], r13    ; BufferSize
    mov     [r12+16], rax   ; CurrentOffset
    mov     [r12+24], rax   ; DosHeaderOffset

    ; Setup encoder key with RDTSC
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     rcx, ENCODER_XOR_KEY
    xor     rax, rcx
    mov     [r12+64], rax

    mov     eax, 1
    jmp     @@done
    
@@fail:
    xor     eax, eax
    
@@done:
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PeGen_Initialize ENDP

; ================================================================================
; PeGen_CreateDosHeader - Create MZ/DOS header
; RCX = pContext
; Returns EAX = 1 (success)
; ================================================================================
PeGen_CreateDosHeader PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    .ENDPROLOG

    mov     rbx, rcx
    mov     rdi, [rbx+24]
    mov     word ptr [rdi], PE_MAGIC
    mov     word ptr [rdi+2], 0090h
    mov     word ptr [rdi+4], 0003h
    mov     word ptr [rdi+6], 0000h
    mov     word ptr [rdi+8], 0004h
    mov     word ptr [rdi+10], 0000h
    mov     word ptr [rdi+12], 0FFFFh
    mov     word ptr [rdi+14], 0000h
    mov     word ptr [rdi+16], 00B8h
    mov     word ptr [rdi+18], 0000h
    mov     word ptr [rdi+20], 0000h
    mov     word ptr [rdi+22], 0000h
    mov     word ptr [rdi+24], 0040h
    mov     word ptr [rdi+26], 0000h
    xor     eax, eax
    mov     [rdi+28], eax
    mov     [rdi+32], eax
    mov     [rdi+36], eax
    mov     [rdi+40], eax
    mov     [rdi+44], eax
    mov     dword ptr [rdi+60], 40h
    mov     byte ptr [rdi+64], 0EBh
    mov     byte ptr [rdi+65], 02h
    mov     rax, [rbx+24]
    add     rax, 64 + 16
    mov     [rbx+16], rax

    mov     eax, 1
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PeGen_CreateDosHeader ENDP

; ================================================================================
; PeGen_CreateNtHeaders - Create NT/PE32+ headers
; RCX = pContext
; RDX = ImageBase
; R8  = Subsystem (3=Console, 2=GUI)
; Returns EAX = 1 (success)
; ================================================================================
PeGen_CreateNtHeaders PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    .PUSHREG r12
    .PUSHREG r13
    .PUSHREG r14
    .ENDPROLOG

    mov     rbx, rcx
    mov     r12, rdx
    mov     r13d, r8d
    mov     rdi, [rbx+16]
    add     rdi, 7
    and     rdi, 0FFFFFFFFFFFFFFF8h
    mov     [rbx+16], rdi
    mov     dword ptr [rdi], PE_SIGNATURE
    add     rdi, 4
    mov     [rbx+32], rdi
    mov     word ptr [rdi], MACHINE_AMD64
    mov     word ptr [rdi+2], 0
    mov     dword ptr [rdi+4], 0
    mov     dword ptr [rdi+8], 0
    mov     dword ptr [rdi+12], 0
    mov     word ptr [rdi+16], 0
    mov     word ptr [rdi+18], IMAGE_FILE_EXECUTABLE_IMAGE OR IMAGE_FILE_LARGE_ADDRESS_AWARE
    add     rdi, 20
    mov     [rbx+40], rdi
    mov     word ptr [rdi], OPTIONAL_HDR64_MAGIC
    mov     byte ptr [rdi+2], 0
    mov     byte ptr [rdi+3], 0
    mov     dword ptr [rdi+4], 0
    mov     dword ptr [rdi+8], 0
    mov     dword ptr [rdi+12], 0
    mov     dword ptr [rdi+16], 0
    mov     dword ptr [rdi+20], 1000h
    mov     [rdi+24], r12
    mov     dword ptr [rdi+32], 1000h
    mov     dword ptr [rdi+36], 200h
    mov     word ptr [rdi+40], 6
    mov     word ptr [rdi+42], 0
    mov     word ptr [rdi+44], 0
    mov     word ptr [rdi+46], 0
    mov     word ptr [rdi+48], 0
    mov     word ptr [rdi+50], 0
    mov     dword ptr [rdi+52], 0
    mov     dword ptr [rdi+56], 10000h
    mov     dword ptr [rdi+60], 400h
    mov     dword ptr [rdi+64], 0
    mov     word ptr [rdi+68], r13w
    mov     word ptr [rdi+70], IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE OR IMAGE_DLLCHARACTERISTICS_NX_COMPAT
    mov     qword ptr [rdi+72], 100000h
    mov     qword ptr [rdi+80], 10000h
    mov     qword ptr [rdi+88], 100000h
    mov     qword ptr [rdi+96], 10000h
    mov     dword ptr [rdi+104], 0
    mov     dword ptr [rdi+108], 16

    ; Data directories
    add     rdi, 112
    mov     [rbx+48], rdi

    mov     rcx, 128
    xor     eax, eax
    rep     stosb

    ; Update CurrentOffset
    mov     [rbx+16], rdi
    add     rdi, 7
    and     rdi, 0FFFFFFFFFFFFFFF8h
    mov     [rbx+56], rdi

    mov     eax, 1
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PeGen_CreateNtHeaders ENDP

; ================================================================================
; Encoder_XOR - XOR encode data
; ================================================================================
Encoder_XOR PROC FRAME
    push    rsi
    push    rbx
    .PUSHREG rsi
    .PUSHREG rbx
    .ENDPROLOG
    
    mov     rsi, rcx
    mov     rbx, rdx
    mov     r9, r8
    
    shr     rbx, 3
    jz      @@done
    
@@loop:
    mov     rax, [rsi]
    xor     rax, r9
    mov     [rsi], rax
    add     rsi, 8
    dec     rbx
    jnz     @@loop
    
@@done:
    pop     rbx
    pop     rsi
    ret
Encoder_XOR ENDP

; ================================================================================
; Encoder_Rotate - ADD + ROR encode
; ================================================================================
Encoder_Rotate PROC FRAME
    push    rsi
    push    rbx
    push    r12
    .PUSHREG rsi
    .PUSHREG rbx
    .PUSHREG r12
    .ENDPROLOG
    
    mov     rsi, rcx
    mov     rbx, rdx
    mov     r12, r8
    
    shr     rbx, 3
    jz      @@done
    
@@loop:
    mov     rax, [rsi]
    add     rax, r12
    ror     rax, ENCODER_ROL_BITS
    mov     [rsi], rax
    add     rsi, 8
    dec     rbx
    jnz     @@loop
    
@@done:
    pop     r12
    pop     rbx
    pop     rsi
    ret
Encoder_Rotate ENDP

; ================================================================================
; PeGen_Finalize - Finalize PE structure
; RCX = pContext, RDX = EntryPointRVA
; Returns EAX = 1 (success)
; ================================================================================
PeGen_Finalize PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    .ENDPROLOG

    mov     rbx, rcx
    mov     esi, edx
    mov     rdi, [rbx+40]
    mov     [rdi+16], esi
    mov     dword ptr [rdi+56], 10000h
    mov     dword ptr [rdi+60], 400h

    mov     eax, 1
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PeGen_Finalize ENDP

; ================================================================================
; PeGen_Cleanup - Free context buffer
; RCX = pContext
; ================================================================================
PeGen_Cleanup PROC FRAME
    push    rbx
    .PUSHREG rbx
    .ENDPROLOG
    
    mov     rbx, rcx
    mov     rcx, [rbx]
    test    rcx, rcx
    jz      @@done
    
    xor     edx, edx
    mov     r8d, 8000h
    call    qword ptr [__imp_VirtualFree]
    
@@done:
    pop     rbx
    ret
PeGen_Cleanup ENDP

; ================================================================================
; EXPORTS
; ================================================================================

PUBLIC PeGen_Initialize
PUBLIC PeGen_CreateDosHeader
PUBLIC PeGen_CreateNtHeaders
PUBLIC PeGen_Finalize
PUBLIC PeGen_Cleanup
PUBLIC Encoder_XOR
PUBLIC Encoder_Rotate

END

