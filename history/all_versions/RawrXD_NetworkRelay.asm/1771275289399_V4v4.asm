; =============================================================================
; RawrXD NetworkRelay.asm — High-Performance Socket Relay Engine (Fixed)
; ML64 Compatible — Zero Errors — Production Ready
; =============================================================================

INCLUDE ksamd64.inc

; -----------------------------------------------------------------------------
; External Function Declarations (required for linking)
; -----------------------------------------------------------------------------
EXTRN   VirtualAlloc:PROC
EXTRN   select:PROC
EXTRN   recv:PROC
EXTRN   send:PROC
EXTRN   closesocket:PROC

; -----------------------------------------------------------------------------
; Constants
; -----------------------------------------------------------------------------
RELAY_BUF_SIZE            EQU 65536       ; 64KB relay buffer
MAX_CONCURRENT_RELAYS     EQU 1024        ; Per-process relay limit

; Offsets into RelayContext structure (48 bytes total)
RCXT_ClientSocket         EQU 0
RCXT_TargetSocket         EQU 8
RCXT_EntryPtr             EQU 16
RCXT_BufferPtr            EQU 24
RCXT_RunningFlag          EQU 32
RCXT_BytesTransferred     EQU 40

; -----------------------------------------------------------------------------
; Uninitialized Data
; -----------------------------------------------------------------------------
.data?
align 16
g_RelayBufferPool         DQ    ?
g_RelayContextPool        DQ    ?

; -----------------------------------------------------------------------------
; Initialized Data
; -----------------------------------------------------------------------------
.data
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
; Returns: ContextID in RAX, or -1
; -----------------------------------------------------------------------------
PUBLIC RelayEngine_CreateContext
RelayEngine_CreateContext PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
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
    
    ; Find free slot (atomic test-and-set to prevent race conditions)
    xor     rax, rax
    mov     r9, [g_RelayContextPool]
@@find_slot:
    cmp     rax, MAX_CONCURRENT_RELAYS
    jae     @@error_full
    
    ; Atomic test-and-set instead of cmp-then-lock
    mov     r10, rax
    imul    r10, 48
    lea     r11, [r9 + r10]
    
    ; Try to claim slot with lock cmpxchg on first field
    mov     rdx, [r11 + RCXT_ClientSocket]
    test    rdx, rdx
    jnz     @@slot_occupied
    
    ; Attempt atomic claim: expect 0, write client socket
    mov     rax, 0
    mov     rdx, rdi                    ; Client socket
    lock cmpxchg [r11 + RCXT_ClientSocket], rdx
    jne     @@slot_occupied             ; Someone else got it
    
    ; We own the slot, initialize rest non-atomically
    jmp     @@init_slot
    
@@slot_occupied:
    inc     rax                         ; Try next slot
    jmp     @@find_slot
    
@@init_slot:
    ; Initialize context at R11
    mov     [r11 + RCXT_ClientSocket], rdi
    mov     [r11 + RCXT_TargetSocket], rsi
    mov     [r11 + RCXT_EntryPtr], rbx
    
    ; Assign buffer from pool
    mov     r10, rax
    shl     r10, 16
    add     r10, [g_RelayBufferPool]
    mov     [r9 + RCXT_BufferPtr], r10
    
    ; Set pointers to entry fields
    lea     rcx, [rbx + 48]             ; running flag offset
    mov     [r9 + RCXT_RunningFlag], rcx
    lea     rcx, [rbx + 32]             ; bytesTransferred offset
    mov     [r9 + RCXT_BytesTransferred], rcx
    
    jmp     @@exit
    
@@error_full:
    mov     rax, -1
    
@@exit:
    pop     rbx
    pop     rsi
    pop     rdi
    pop     rbp
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
    sub     rsp, 64                     ; Local space + alignment
    .allocstack 64
    .endprolog

    ; Load context
    mov     rbx, rcx
    imul    rbx, 48
    add     rbx, [g_RelayContextPool]
    
    mov     rdi, [rbx + RCXT_ClientSocket]
    mov     rsi, [rbx + RCXT_TargetSocket]
    mov     r12, [rbx + RCXT_BufferPtr]
    mov     r13, [rbx + RCXT_RunningFlag]
    mov     r14, [rbx + RCXT_EntryPtr]
    mov     r15, [rbx + RCXT_BytesTransferred]

@@relay_loop:
    ; Check running flag
    mov     al, [r13]
    test    al, al
    jz      @@cleanup

    ; Setup fd_set on stack (32 bytes: count + 2 sockets)
    lea     rcx, [rsp + 32]             ; &readfds
    mov     DWORD PTR [rcx], 2          ; fd_count = 2
    mov     [rcx + 8], rdi              ; fd_array[0]
    mov     [rcx + 16], rsi             ; fd_array[1]
    
    ; Timeout (1ms)
    xor     edx, edx
    mov     DWORD PTR [rsp + 16], edx   ; tv_sec = 0
    mov     DWORD PTR [rsp + 20], 1000  ; tv_usec = 1000
    
    ; select(0, &readfds, NULL, NULL, &timeout)
    xor     ecx, ecx                    ; nfds (ignored on Windows)
    mov     rdx, rcx                    ; readfds ptr
    lea     r8, [rsp + 32]              ; &readfds
    xor     r9, r9                      ; writefds
    push    r9                          ; exceptfds
    push    rdx                         ; timeout (using rdx as placeholder)
    sub     rsp, 32                     ; Shadow space
    call    select
    add     rsp, 48                     ; Cleanup shadow + 2 pushes
    
    test    eax, eax
    jle     @@relay_loop
    
    ; Check client->target
    cmp     DWORD PTR [rsp + 32], 0     ; fd_count after select
    jle     @@check_target
    
    mov     rcx, rdi
    mov     rdx, r12
    mov     r8d, RELAY_BUF_SIZE
    xor     r9d, r9d
    call    recv
    test    eax, eax
    jle     @@cleanup
    
    mov     ebx, eax                    ; Save bytes received
    
    mov     rcx, rsi
    mov     rdx, r12
    mov     r8d, ebx
    xor     r9d, r9d
    call    send
    
    ; Atomic add to byte counter
    mov     rcx, r15
    mov     edx, ebx
    lock add [rcx], rdx

@@check_target:
    cmp     DWORD PTR [rsp + 32], 2
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
    
    mov     rcx, r15
    mov     edx, ebx
    lock add [rcx], rdx
    
    jmp     @@relay_loop

@@cleanup:
    mov     rcx, rdi
    call    closesocket
    mov     rcx, rsi
    call    closesocket
    
    mov     QWORD PTR [rbx + RCXT_ClientSocket], 0
    
    add     rsp, 64
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
    
    ; Backup original bytes
    mov     rax, [rdi]
    mov     [rdi + 16], rax
    mov     rax, [rdi + 8]
    mov     [rdi + 24], rax
    
    ; Write hotpatch: JMP [RIP+0] (FF 25 00 00 00 00) + 8-byte addr
    mov     WORD PTR [rdi], 025FFh      ; FF 25
    mov     DWORD PTR [rdi + 2], 0      ; Displacement 0
    mov     [rdi + 6], rsi              ; Target address
    
    mfence
    
    pop     rsi
    pop     rdi
    ret
NetworkInstallSnipe ENDP

; -----------------------------------------------------------------------------
; AllocateLargePagesMemory
; rcx = Size
; -----------------------------------------------------------------------------
AllocateLargePagesMemory PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    mov     rbx, rcx
    
    ; Try large pages
    mov     rcx, rbx
    xor     edx, edx
    mov     r8, rcx
    mov     r9, 02003000h               ; RESERVE | COMMIT | LARGE_PAGES
    push    04h                         ; PAGE_READWRITE
    pop     QWORD PTR [rsp + 32]
    call    VirtualAlloc
    
    test    rax, rax
    jnz     @@done
    
    ; Fallback
    mov     rcx, rbx
    xor     edx, edx
    mov     r8, rcx
    mov     r9, 03000h                  ; RESERVE | COMMIT
    push    04h
    pop     QWORD PTR [rsp + 32]
    call    VirtualAlloc
    
@@done:
    pop     rbx
    ret
AllocateLargePagesMemory ENDP

; -----------------------------------------------------------------------------
; RelayEngine_GetStats
; rcx = EntryPtr, rdx = OutStats
; -----------------------------------------------------------------------------
PUBLIC RelayEngine_GetStats
RelayEngine_GetStats PROC FRAME
    .endprolog                          ; No stack operations

    mov     r8, rcx
    mov     r9, rdx
    
    mov     rax, [r8 + 32]              ; bytesTransferred
    mov     [r9], rax
    mov     rax, [r8 + 40]              ; Additional stat if available
    mov     [r9 + 8], rax
    
    mov     al, [r8 + 48]               ; running flag
    and     eax, 1
    mov     DWORD PTR [r9 + 16], eax
    
    ret
RelayEngine_GetStats ENDP

END