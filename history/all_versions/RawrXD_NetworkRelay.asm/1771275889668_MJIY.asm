; =============================================================================
; RawrXD NetworkRelay.asm - High-Performance Socket Relay Engine
; Corrected for ML64 compatibility with proper unwind codes
; =============================================================================

INCLUDE ksamd64.inc

; -----------------------------------------------------------------------------
; External Function Declarations (required for linking)
; -----------------------------------------------------------------------------
EXTRN   recv:PROC
EXTRN   send:PROC
EXTRN   select:PROC
EXTRN   closesocket:PROC
EXTRN   VirtualAlloc:PROC
EXTRN   __imp_RtlIpv4StringToAddressA:QWORD

; -----------------------------------------------------------------------------
; Constants
; -----------------------------------------------------------------------------
RELAY_BUF_SIZE            EQU 65536
MAX_CONCURRENT_RELAYS     EQU 1024

; -----------------------------------------------------------------------------
; Structure Definitions (flat EQU offsets)
; -----------------------------------------------------------------------------
RCXT_ClientSocket         EQU 0
RCXT_TargetSocket         EQU 8
RCXT_EntryPtr             EQU 16
RCXT_BufferPtr            EQU 24
RCXT_RunningFlag          EQU 32
RCXT_BytesTransferred     EQU 40
SIZEOF_RCXT               EQU 48

ENTRY_LocalPort           EQU 0
ENTRY_RemotePort          EQU 2
ENTRY_Label               EQU 8
ENTRY_Protocol            EQU 32
ENTRY_LocalAddress        EQU 56
ENTRY_ForwardAddress      EQU 80
ENTRY_Active              EQU 104
ENTRY_BytesTransferred    EQU 112
ENTRY_Running             EQU 120

; -----------------------------------------------------------------------------
; Data Section
; -----------------------------------------------------------------------------
.data?
align 16
g_RelayBufferPool         DQ    ?
g_RelayContextPool        DQ    ?

.data
g_RelayPoolLock           DQ    0

; -----------------------------------------------------------------------------
; Code Section
; -----------------------------------------------------------------------------
.code

; =============================================================================
; PUBLIC API
; =============================================================================

PUBLIC RelayEngine_Init
RelayEngine_Init PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx

    mov     rax, MAX_CONCURRENT_RELAYS
    shl     rax, 16
    mov     rcx, rax
    call    AllocateLargePagesMemory
    mov     [g_RelayBufferPool], rax

    mov     rcx, MAX_CONCURRENT_RELAYS * SIZEOF_RCXT
    call    AllocateLargePagesMemory
    mov     [g_RelayContextPool], rax

    xor     eax, eax

    add     rsp, 40
    pop     rbx
    ret
RelayEngine_Init ENDP

PUBLIC RelayEngine_CreateContext
RelayEngine_CreateContext PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rdi, rcx
    mov     rsi, rdx
    mov     rbx, r8

    xor     rax, rax
    mov     r9, [g_RelayContextPool]

@@find_slot:
    cmp     rax, MAX_CONCURRENT_RELAYS
    jae     @@error_full

    mov     r10, rax
    imul    r10, SIZEOF_RCXT
    lea     r11, [r9 + r10]

    cmp     QWORD PTR [r11 + RCXT_ClientSocket], 0
    je      @@found_slot
    inc     rax
    jmp     @@find_slot

@@found_slot:
    mov     [r11 + RCXT_ClientSocket], rdi
    mov     [r11 + RCXT_TargetSocket], rsi
    mov     [r11 + RCXT_EntryPtr], rbx

    mov     r10, rax
    shl     r10, 16
    add     r10, [g_RelayBufferPool]
    mov     [r11 + RCXT_BufferPtr], r10

    lea     rcx, [rbx + ENTRY_Running]
    mov     [r11 + RCXT_RunningFlag], rcx
    lea     rcx, [rbx + ENTRY_BytesTransferred]
    mov     [r11 + RCXT_BytesTransferred], rcx

    jmp     @@exit

@@error_full:
    mov     rax, -1

@@exit:
    lea     rsp, [rbp - 24]
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
RelayEngine_CreateContext ENDP

PUBLIC RelayEngine_RunBiDirectional
RelayEngine_RunBiDirectional PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 88
    .allocstack 88
    .endprolog

    mov     rbx, rcx
    imul    rbx, SIZEOF_RCXT
    add     rbx, [g_RelayContextPool]

    mov     rdi, [rbx + RCXT_ClientSocket]
    mov     rsi, [rbx + RCXT_TargetSocket]
    mov     r12, [rbx + RCXT_BufferPtr]
    mov     r13, [rbx + RCXT_RunningFlag]
    mov     r14, [rbx + RCXT_EntryPtr]
    mov     r15, [rbx + RCXT_BytesTransferred]

@@relay_loop:
    mov     al, [r13]
    test    al, al
    jz      @@cleanup

    lea     rcx, [rsp + 64]
    mov     DWORD PTR [rcx], 2
    mov     [rcx + 8], rdi
    mov     [rcx + 16], rsi

    lea     r9, [rsp + 48]
    mov     DWORD PTR [r9], 0
    mov     DWORD PTR [r9 + 8], 1000

    xor     ecx, ecx
    lea     rdx, [rsp + 64]
    xor     r8d, r8d
    mov     QWORD PTR [rsp + 32], 0
    lea     rax, [rsp + 48]
    mov     QWORD PTR [rsp + 40], rax
    call    select

    test    eax, eax
    jle     @@relay_loop

    mov     rcx, rdi
    mov     rdx, r12
    mov     r8d, RELAY_BUF_SIZE
    xor     r9d, r9d
    call    recv

    test    eax, eax
    jle     @@check_target

    mov     ebx, eax

    mov     rcx, rsi
    mov     rdx, r12
    mov     r8d, ebx
    xor     r9d, r9d
    call    send

    movsxd  rdx, ebx
    lock add QWORD PTR [r15], rdx

@@check_target:
    mov     rcx, rsi
    mov     rdx, r12
    mov     r8d, RELAY_BUF_SIZE
    xor     r9d, r9d
    call    recv

    test    eax, eax
    jle     @@relay_loop

    mov     ebx, eax

    mov     rcx, rdi
    mov     rdx, r12
    mov     r8d, ebx
    xor     r9d, r9d
    call    send

    movsxd  rdx, ebx
    lock add QWORD PTR [r15], rdx

    jmp     @@relay_loop

@@cleanup:
    mov     rcx, rdi
    call    closesocket
    mov     rcx, rsi
    call    closesocket

    mov     QWORD PTR [rbx + RCXT_ClientSocket], 0

    lea     rsp, [rbp - 56]
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
RelayEngine_RunBiDirectional ENDP

PUBLIC NetworkInstallSnipe
NetworkInstallSnipe PROC FRAME
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rdi, rcx
    mov     rsi, rdx

    mov     rax, [rdi]
    mov     rdx, [rdi + 8]

    mov     WORD PTR [rdi], 025FFh
    mov     DWORD PTR [rdi + 2], 0
    mov     [rdi + 6], rsi
    mov     WORD PTR [rdi + 14], 9090h

    sfence

    pop     rsi
    pop     rdi
    ret
NetworkInstallSnipe ENDP

AllocateLargePagesMemory PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx

    xor     ecx, ecx
    xor     edx, edx
    mov     r8, rbx
    mov     r9, 02003000h
    mov     QWORD PTR [rsp + 32], 04h
    call    VirtualAlloc

    test    rax, rax
    jnz     @@done

    xor     ecx, ecx
    xor     edx, edx
    mov     r8, rbx
    mov     r9, 03000h
    mov     QWORD PTR [rsp + 32], 04h
    call    VirtualAlloc

@@done:
    add     rsp, 40
    pop     rbx
    ret
AllocateLargePagesMemory ENDP

PUBLIC RelayEngine_GetStats
RelayEngine_GetStats PROC FRAME
    .endprolog

    mov     r8, rcx
    mov     r9, rdx

    mov     rax, [r8 + ENTRY_BytesTransferred]
    mov     [r9], rax

    mov     [r9 + 8], rax

    movzx   eax, BYTE PTR [r8 + ENTRY_Running]
    and     eax, 1
    mov     DWORD PTR [r9 + 16], eax

    ret
RelayEngine_GetStats ENDP

END