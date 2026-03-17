; =============================================================================
; RawrXD NetworkRelay.asm — Corrected FRAME Prologues & Unwind Information
; ml64 compatible with proper .xdata directives
; =============================================================================

INCLUDE ksamd64.inc

; -----------------------------------------------------------------------------
; External Imports
; -----------------------------------------------------------------------------
EXTRN   VirtualAlloc:PROC
EXTRN   select:PROC
EXTRN   recv:PROC
EXTRN   send:PROC
EXTRN   closesocket:PROC

; -----------------------------------------------------------------------------
; Constants
; -----------------------------------------------------------------------------
RELAY_BUF_SIZE            EQU 65536
RELAY_IOVEC_ALIGNMENT     EQU 64
MAX_CONCURRENT_RELAYS     EQU 1024

; Offsets into RelayContext structure (48 bytes)
RCXT_ClientSocket         EQU 0
RCXT_TargetSocket         EQU 8
RCXT_EntryPtr             EQU 16
RCXT_BufferPtr            EQU 24
RCXT_RunningFlag          EQU 32
RCXT_BytesTransferred     EQU 40

; PortForwardEntry offsets (from C++ struct)
ENTRY_BytesOffset         EQU 32
ENTRY_RunningOffset       EQU 48

; -----------------------------------------------------------------------------
; Data Section
; -----------------------------------------------------------------------------
.data?
align 4096
g_RelayBufferPool         DQ    ?
g_RelayContextPool        DQ    ?
g_RelayPoolLock           DQ    0

; -----------------------------------------------------------------------------
; Code Section
; -----------------------------------------------------------------------------
.code

; =============================================================================
; PUBLIC API
; =============================================================================

; -----------------------------------------------------------------------------
; RelayEngine_Init
; rcx = PoolSize
; -----------------------------------------------------------------------------
PUBLIC RelayEngine_Init
RelayEngine_Init PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    mov     rbx, rcx
    
    ; Allocate relay buffer pool
    mov     rax, MAX_CONCURRENT_RELAYS
    shl     rax, 16                     ; * 65536
    mov     rcx, rax
    call    AllocateLargePagesMemory
    mov     [g_RelayBufferPool], rax
    
    ; Allocate context pool
    mov     rcx, MAX_CONCURRENT_RELAYS * 48
    call    AllocateLargePagesMemory
    mov     [g_RelayContextPool], rax
    
    xor     eax, eax
    pop     rbx
    ret
RelayEngine_Init ENDP

; -----------------------------------------------------------------------------
; RelayEngine_CreateContext
; rcx = ClientSocket, rdx = TargetSocket, r8 = EntryPtr
; Returns: ContextID (RAX) or -1
; -----------------------------------------------------------------------------
PUBLIC RelayEngine_CreateContext
RelayEngine_CreateContext PROC FRAME
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    push    rbx
    .pushreg rbx
    .endprolog

    mov     rdi, rcx                    ; Client
    mov     rsi, rdx                    ; Target
    mov     rbx, r8                     ; Entry
    
    ; Find free slot
    xor     rax, rax
@@find_slot:
    cmp     rax, MAX_CONCURRENT_RELAYS
    jae     @@error_full
    
    mov     r9, [g_RelayContextPool]
    mov     r10, rax
    imul    r10, 48
    add     r9, r10
    
    cmp     QWORD PTR [r9], 0           ; Check if slot free
    je      @@found_slot
    inc     rax
    jmp     @@find_slot
    
@@found_slot:
    ; Initialize context
    mov     [r9.RCXT_ClientSocket], rdi
    mov     [r9.RCXT_TargetSocket], rsi
    mov     [r9.RCXT_EntryPtr], rbx
    
    ; Assign buffer
    mov     r10, rax
    shl     r10, 16
    add     r10, [g_RelayBufferPool]
    mov     [r9.RCXT_BufferPtr], r10
    
    ; Set running flag pointer
    lea     rcx, [rbx + ENTRY_RunningOffset]
    mov     [r9.RCXT_RunningFlag], rcx
    mov     [r9.RCXT_BytesTransferred], rbx
    
    jmp     @@exit
    
@@error_full:
    mov     rax, -1
    
@@exit:
    pop     rbx
    pop     rsi
    pop     rdi
    ret
RelayEngine_CreateContext ENDP

; -----------------------------------------------------------------------------
; RelayEngine_RunBiDirectional
; rcx = ContextID
; -----------------------------------------------------------------------------
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
    sub     rsp, 80                     ; 64 bytes local + 16 align
    .allocstack 80
    .endprolog

    ; Load context
    mov     rbx, rcx
    imul    rbx, 48
    add     rbx, [g_RelayContextPool]
    
    mov     rdi, [rbx.RCXT_ClientSocket]
    mov     rsi, [rbx.RCXT_TargetSocket]
    mov     r12, [rbx.RCXT_BufferPtr]
    mov     r13, [rbx.RCXT_RunningFlag]
    mov     r14, [rbx.RCXT_EntryPtr]

@@relay_loop:
    ; Check running flag
    mov     al, [r13]
    test    al, al
    jz      @@cleanup

    ; Setup fd_set on stack (at rbp-64)
    lea     rcx, [rsp + 16]             ; &readfds
    xor     edx, edx
    mov     [rcx], rdx
    mov     [rcx + 8], rdi              ; fd_array[0]
    mov     [rcx + 16], rsi             ; fd_array[1]
    mov     DWORD PTR [rcx], 2          ; fd_count
    
    ; Timeout (1ms)
    lea     rdx, [rsp]
    mov     QWORD PTR [rdx], 0
    mov     DWORD PTR [rdx + 8], 1000
    
    ; select()
    xor     ecx, ecx                    ; nfds
    mov     rdx, rcx
    lea     r8, [rsp + 16]              ; readfds
    xor     r9, r9                      ; writefds
    mov     QWORD PTR [rsp + 32], 0     ; exceptfds
    mov     QWORD PTR [rsp + 40], rdx   ; timeout
    call    select
    
    test    eax, eax
    jle     @@relay_loop
    
    ; Check client->target
    mov     eax, DWORD PTR [rsp + 16]
    test    eax, eax
    jz      @@check_target
    
    ; recv from client
    mov     rcx, rdi
    mov     rdx, r12
    mov     r8d, RELAY_BUF_SIZE
    xor     r9d, r9d
    call    recv
    test    eax, eax
    jle     @@cleanup
    
    mov     ebx, eax
    
    ; send to target
    mov     rcx, rsi
    mov     rdx, r12
    mov     r8d, ebx
    xor     r9d, r9d
    call    send
    
    ; Atomic add bytes
    mov     rcx, r14
    add     rcx, ENTRY_BytesOffset
    mov     edx, ebx
    lock add [rcx], rdx

@@check_target:
    ; Check target->client
    cmp     DWORD PTR [rsp + 16], 2
    jb      @@relay_loop
    
    mov     rcx, rsi
    mov     rdx, r12
    mov     r8d, RELAY_BUF_SIZE
    xor     r9d, r9d
    call    recv
    test    eax, eax
    jle     @@cleanup
    
    mov     ebx, eax
    
    mov     rcx, rdi
    mov     rdx, r12
    mov     r8d, ebx
    xor     r9d, r9d
    call    send
    
    mov     rcx, r14
    add     rcx, ENTRY_BytesOffset
    mov     edx, ebx
    lock add [rcx], rdx
    
    jmp     @@relay_loop

@@cleanup:
    mov     rcx, rdi
    call    closesocket
    mov     rcx, rsi
    call    closesocket
    
    mov     QWORD PTR [rbx.RCXT_ClientSocket], 0
    
    lea     rsp, [rbp - 56]             ; Restore to non-volatile save area
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

; -----------------------------------------------------------------------------
; NetworkInstallSnipe
; rcx = TargetAddress, rdx = SniperFunction
; -----------------------------------------------------------------------------
PUBLIC NetworkInstallSnipe
NetworkInstallSnipe PROC FRAME
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rdi, rcx
    mov     rsi, rdx
    
    ; Backup original code
    mov     rax, [rdi]
    mov     [rdi + 16], rax
    mov     rax, [rdi + 8]
    mov     [rdi + 24], rax
    
    ; Write hotpatch: JMP [RIP+0] (14 bytes)
    mov     WORD PTR [rdi], 025FFh      ; FF 25 00 00 00 00
    mov     DWORD PTR [rdi + 2], 0
    mov     [rdi + 6], rsi
    mov     WORD PTR [rdi + 14], 0CCCCh
    
    sfence
    
    pop     rsi
    pop     rdi
    ret
NetworkInstallSnipe ENDP

; -----------------------------------------------------------------------------
; RelayEngine_GetStats
; rcx = EntryPtr, rdx = OutStats
; -----------------------------------------------------------------------------
PUBLIC RelayEngine_GetStats
RelayEngine_GetStats PROC FRAME
    .endprolog                          ; No stack manipulation
    
    mov     r8, rcx
    mov     r9, rdx
    
    mov     rax, [r8 + ENTRY_BytesOffset]
    mov     [r9], rax
    mov     QWORD PTR [r9 + 8], rax
    
    movzx   eax, BYTE PTR [r8 + ENTRY_RunningOffset]
    mov     DWORD PTR [r9 + 16], eax
    
    ret
RelayEngine_GetStats ENDP

; =============================================================================
; INTERNAL HELPERS
; =============================================================================

AllocateLargePagesMemory PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40                     ; Shadow space + align
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    
    ; Try large pages
    mov     rcx, rbx
    xor     edx, edx
    mov     r8, rcx
    mov     r9, 02003000h               ; RESERVE | COMMIT | LARGE_PAGES
    mov     QWORD PTR [rsp + 32], 4     ; PAGE_READWRITE
    call    VirtualAlloc
    
    test    rax, rax
    jnz     @@done
    
    ; Fallback to normal
    mov     rcx, rbx
    xor     edx, edx
    mov     r8, rcx
    mov     r9, 03000h
    mov     QWORD PTR [rsp + 32], 4
    call    VirtualAlloc
    
@@done:
    add     rsp, 40
    pop     rbx
    ret
AllocateLargePagesMemory ENDP

END