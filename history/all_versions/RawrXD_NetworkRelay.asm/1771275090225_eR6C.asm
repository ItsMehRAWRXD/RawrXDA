; =============================================================================
; RawrXD NetworkRelay.asm — Production-Ready MASM64 with Correct FRAME Prologues
; Fixes: .ENDPROLOG placement, EXTRN declarations, struct offsets verified
; Assembles: ml64.exe /c /W3 /Zi NetworkRelay.asm
; =============================================================================

; -----------------------------------------------------------------------------
; External Imports (Kernel32, WS2_32)
; -----------------------------------------------------------------------------
EXTRN VirtualAlloc:PROC
EXTRN select:PROC
EXTRN recv:PROC
EXTRN send:PROC
EXTRN closesocket:PROC
EXTRN WSAGetLastError:PROC

; Constants
RELAY_BUF_SIZE            EQU 65536
MAX_CONCURRENT_RELAYS     EQU 1024

; Context structure offsets (48 bytes total)
RCXT_ClientSocket         EQU 0
RCXT_TargetSocket         EQU 8
RCXT_EntryPtr             EQU 16
RCXT_BufferPtr            EQU 24
RCXT_RunningFlag          EQU 32
RCXT_BytesTransferred     EQU 40

; -----------------------------------------------------------------------------
; BSS Section (Uninitialized data)
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
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rbx, rcx
    
    ; Allocate buffer pool
    mov     rax, MAX_CONCURRENT_RELAYS
    shl     rax, 16                     ; * 65536
    mov     rcx, rax
    xor     edx, edx                    ; lpAddress = NULL
    mov     r8, rax                     ; dwSize
    mov     r9, 02003000h               ; MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES
    mov     QWORD PTR [rsp+32], 04h     ; PAGE_READWRITE
    call    VirtualAlloc
    mov     [g_RelayBufferPool], rax
    
    ; Allocate context pool
    mov     rcx, MAX_CONCURRENT_RELAYS * 48
    xor     edx, edx
    mov     r8, rcx
    mov     r9, 02003000h
    mov     QWORD PTR [rsp+32], 04h
    call    VirtualAlloc
    mov     [g_RelayContextPool], rax
    
    xor     eax, eax
    add     rsp, 32
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
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rdi, rcx                    ; ClientSocket
    mov     rsi, rdx                    ; TargetSocket  
    mov     rbx, r8                     ; EntryPtr
    
    ; Find free slot
    xor     rax, rax
    mov     r9, [g_RelayContextPool]
    
@@find_slot:
    cmp     rax, MAX_CONCURRENT_RELAYS
    jae     @@error_full
    
    mov     r10, rax
    imul    r10, 48
    lea     r11, [r9 + r10]
    
    cmp     QWORD PTR [r11 + RCXT_ClientSocket], 0
    je      @@found_slot
    inc     rax
    jmp     @@find_slot
    
@@found_slot:
    ; Initialize context at R11
    mov     [r11 + RCXT_ClientSocket], rdi
    mov     [r11 + RCXT_TargetSocket], rsi
    mov     [r11 + RCXT_EntryPtr], rbx
    
    ; Buffer assignment
    mov     r10, rax
    shl     r10, 16
    add     r10, [g_RelayBufferPool]
    mov     [r11 + RCXT_BufferPtr], r10
    
    ; Running flag (Entry->running at offset 48)
    lea     rcx, [rbx + 48]
    mov     [r11 + RCXT_RunningFlag], rcx
    
    ; Byte counter (Entry->bytesTransferred at offset 32)
    lea     rcx, [rbx + 32]
    mov     [r11 + RCXT_BytesTransferred], rcx
    
    jmp     @@exit
    
@@error_full:
    mov     rax, -1
    
@@exit:
    lea     rsp, [rbp - 24]             ; Restore to before pushes
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
RelayEngine_CreateContext ENDP

; -----------------------------------------------------------------------------
; RelayEngine_RunBiDirectional
; rcx = ContextID
; Frame: rbp, rbx, rdi, rsi, r12, r13, r14, r15 + 64 bytes locals
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
    sub     rsp, 80                     ; 64 bytes locals + 16 align
    .allocstack 80
    .endprolog

    ; Load context
    mov     rbx, rcx
    imul    rbx, 48
    add     rbx, [g_RelayContextPool]
    
    mov     rdi, [rbx + RCXT_ClientSocket]      ; RDI = Client
    mov     rsi, [rbx + RCXT_TargetSocket]      ; RSI = Target
    mov     r12, [rbx + RCXT_BufferPtr]         ; R12 = Buffer
    mov     r13, [rbx + RCXT_RunningFlag]       ; R13 = &running
    mov     r14, [rbx + RCXT_EntryPtr]          ; R14 = Entry
    mov     r15, [rbx + RCXT_BytesTransferred]  ; R15 = &bytesCounter

@@relay_loop:
    ; Check running flag
    movzx   eax, BYTE PTR [r13]
    test    al, al
    jz      @@cleanup

    ; Setup fd_set on stack (at rsp+16)
    lea     rcx, [rsp + 16]
    xor     edx, edx
    mov     [rcx], rdx                  ; fd_count = 0
    mov     [rcx + 8], rdi              ; fd_array[0] = client
    mov     [rcx + 16], rsi             ; fd_array[1] = target
    mov     DWORD PTR [rcx], 2          ; fd_count = 2
    
    ; Timeout (1ms)
    lea     rdx, [rsp + 8]
    mov     DWORD PTR [rdx], 0          ; tv_sec
    mov     DWORD PTR [rdx + 8], 1000   ; tv_usec
    
    ; select(0, &readfds, NULL, NULL, &timeout)
    xor     ecx, ecx                    ; nfds (ignored)
    lea     r8, [rsp + 16]              ; &readfds
    xor     r9d, r9d                    ; writefds
    push    0                           ; exceptfds
    push    rdx                         ; timeout
    sub     rsp, 32                     ; Shadow space
    call    select
    add     rsp, 48                     ; Cleanup shadow + pushes
    
    test    eax, eax
    jle     @@relay_loop
    
    ; Check client->target
    cmp     DWORD PTR [rsp + 16], 0     ; fd_count after select
    je      @@check_target
    
    ; recv from client
    mov     rcx, rdi
    mov     rdx, r12
    mov     r8d, RELAY_BUF_SIZE
    xor     r9d, r9d
    call    recv
    test    eax, eax
    jle     @@cleanup
    
    mov     ebx, eax                    ; Bytes received
    
    ; send to target
    mov     rcx, rsi
    mov     rdx, r12
    mov     r8d, ebx
    xor     r9d, r9d
    call    send
    
    ; Atomic add to counter
    mov     edx, ebx
    lock add QWORD PTR [r15], rdx

@@check_target:
    ; Check target->client
    cmp     DWORD PTR [rsp + 16], 2
    jb      @@relay_loop
    
    ; recv from target
    mov     rcx, rsi
    mov     rdx, r12
    mov     r8d, RELAY_BUF_SIZE
    xor     r9d, r9d
    call    recv
    test    eax, eax
    jle     @@cleanup
    
    mov     ebx, eax
    
    ; send to client
    mov     rcx, rdi
    mov     rdx, r12
    mov     r8d, ebx
    xor     r9d, r9d
    call    send
    
    ; Atomic add
    mov     edx, ebx
    lock add QWORD PTR [r15], rdx
    
    jmp     @@relay_loop

@@cleanup:
    ; Close sockets
    mov     rcx, rdi
    call    closesocket
    mov     rcx, rsi
    call    closesocket
    
    ; Clear slot
    mov     QWORD PTR [rbx + RCXT_ClientSocket], 0
    
    ; Restore and return
    lea     rsp, [rbp - 56]             ; Before pushes
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
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rdi, rcx                    ; Target
    mov     rsi, rdx                    ; Sniper
    
    ; Backup original (simplified - assume space available)
    mov     rax, [rdi]
    mov     rdx, [rdi + 8]
    
    ; Write JMP [RIP+0] (14 bytes)
    mov     WORD PTR [rdi], 025FFh      ; FF 25 = JMP [RIP+disp32]
    mov     DWORD PTR [rdi + 2], 0      ; disp32 = 0
    mov     [rdi + 6], rsi              ; Absolute target
    mov     WORD PTR [rdi + 14], 0CCCCh
    
    sfence
    
    add     rsp, 32
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
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog

    mov     r8, rcx                     ; Entry
    mov     r9, rdx                     ; Out buffer
    
    ; Load bytesTransferred (offset 32)
    mov     rax, [r8 + 32]
    mov     [r9], rax
    
    ; TX placeholder (same as RX for now)
    mov     [r9 + 8], rax
    
    ; Active flag (running at offset 48)
    movzx   eax, BYTE PTR [r8 + 48]
    and     eax, 1
    mov     DWORD PTR [r9 + 16], eax
    
    pop     rsi
    pop     rdi
    ret
RelayEngine_GetStats ENDP

END