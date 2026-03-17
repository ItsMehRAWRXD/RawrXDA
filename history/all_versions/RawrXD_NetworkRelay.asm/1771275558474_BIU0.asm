; =============================================================================
; RawrXD NetworkRelay.asm ? High-Performance Socket Relay Engine
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
RELAY_BUF_SIZE            EQU 65536       ; 64KB relay buffer
MAX_CONCURRENT_RELAYS     EQU 1024

; -----------------------------------------------------------------------------
; Structure Definitions (MASM syntax - no nested types)
; -----------------------------------------------------------------------------
; Relay Context Structure (48 bytes)
RCXT_ClientSocket         EQU 0           ; 8 bytes
RCXT_TargetSocket         EQU 8           ; 8 bytes
RCXT_EntryPtr             EQU 16          ; 8 bytes
RCXT_BufferPtr            EQU 24          ; 8 bytes
RCXT_RunningFlag          EQU 32          ; 8 bytes (pointer)
RCXT_BytesTransferred     EQU 40          ; 8 bytes (pointer)
SIZEOF_RCXT               EQU 48

; PortForwardEntry offsets (matching C++ layout)
ENTRY_LocalPort           EQU 0
ENTRY_RemotePort          EQU 2
ENTRY_Label               EQU 8           ; std::string starts here (ptr + size + cap)
ENTRY_Protocol            EQU 32
ENTRY_LocalAddress        EQU 56
ENTRY_ForwardAddress      EQU 80
ENTRY_Active              EQU 104
ENTRY_BytesTransferred    EQU 112         ; uint64_t
ENTRY_Running             EQU 120         ; std::atomic<bool> (1 byte)

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

; -----------------------------------------------------------------------------
; RelayEngine_Init
; rcx = PoolSize
; -----------------------------------------------------------------------------
PUBLIC RelayEngine_Init
RelayEngine_Init PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    
    ; Calculate buffer pool size (64KB per connection)
    mov     rax, MAX_CONCURRENT_RELAYS
    shl     rax, 16                     ; * 65536
    mov     rcx, rax
    call    AllocateLargePagesMemory
    mov     [g_RelayBufferPool], rax
    
    ; Allocate context pool
    mov     rcx, MAX_CONCURRENT_RELAYS * SIZEOF_RCXT
    call    AllocateLargePagesMemory
    mov     [g_RelayContextPool], rax
    
    xor     eax, eax
    
    add     rsp, 40
    pop     rbx
    ret
RelayEngine_Init ENDP

; -----------------------------------------------------------------------------
; RelayEngine_CreateContext
; rcx = ClientSocket, rdx = TargetSocket, r8 = EntryPtr
; Returns: ContextID or -1
; -----------------------------------------------------------------------------
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

    mov     rdi, rcx                    ; Client
    mov     rsi, rdx                    ; Target
    mov     rbx, r8                     ; Entry
    
    ; Find free slot
    xor     rax, rax
    mov     r9, [g_RelayContextPool]
    
@@find_slot:
    cmp     rax, MAX_CONCURRENT_RELAYS
    jae     @@error_full
    
    mov     r10, rax
    imul    r10, SIZEOF_RCXT
    lea     r11, [r9 + r10]             ; Context pointer
    
    cmp     QWORD PTR [r11 + RCXT_ClientSocket], 0
    je      @@found_slot
    inc     rax
    jmp     @@find_slot
    
@@found_slot:
    ; Initialize context
    mov     [r11 + RCXT_ClientSocket], rdi
    mov     [r11 + RCXT_TargetSocket], rsi
    mov     [r11 + RCXT_EntryPtr], rbx
    
    ; Calculate buffer pointer (slot * 64KB)
    mov     r10, rax
    shl     r10, 16
    add     r10, [g_RelayBufferPool]
    mov     [r11 + RCXT_BufferPtr], r10
    
    ; Set pointers into Entry structure
    lea     rcx, [rbx + ENTRY_Running]
    mov     [r11 + RCXT_RunningFlag], rcx
    lea     rcx, [rbx + ENTRY_BytesTransferred]
    mov     [r11 + RCXT_BytesTransferred], rcx
    
    jmp     @@exit
    
@@error_full:
    mov     rax, -1
    
@@exit:
    lea     rsp, [rbp - 24]             ; Restore rsp before popping
    pop     rsi
    pop     rdi
    pop     rbx
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
    sub     rsp, 88                     ; Local space + shadow space for calls
    .allocstack 88
    .endprolog

    ; Load context
    mov     rbx, rcx
    imul    rbx, SIZEOF_RCXT
    add     rbx, [g_RelayContextPool]
    
    mov     rdi, [rbx + RCXT_ClientSocket]      ; RDI = Client
    mov     rsi, [rbx + RCXT_TargetSocket]      ; RSI = Target
    mov     r12, [rbx + RCXT_BufferPtr]         ; R12 = Buffer
    mov     r13, [rbx + RCXT_RunningFlag]       ; R13 = &running
    mov     r14, [rbx + RCXT_EntryPtr]          ; R14 = Entry
    mov     r15, [rbx + RCXT_BytesTransferred]  ; R15 = &bytesTransferred

    ; Initialize empty select counter
    mov     DWORD PTR [rsp + 80], 0     ; empty_count = 0

@@relay_loop:
    ; Check running flag (relaxed load)
    mov     al, [r13]
    test    al, al
    jz      @@cleanup

    ; Adaptive timeout: 1ms → 100ms based on consecutive empty selects
    inc     DWORD PTR [rsp + 80]        ; empty_count++
    cmp     DWORD PTR [rsp + 80], 100
    jb      @@short_timeout
    mov     DWORD PTR [r9 + 8], 100000  ; 100ms backoff
    jmp     @@do_select
@@short_timeout:
    mov     DWORD PTR [r9 + 8], 1000    ; 1ms normal
@@do_select:
    
    ; select(nfds, readfds, writefds, exceptfds, timeout)
    xor     ecx, ecx                    ; nfds (ignored on Windows)
    lea     rdx, [rsp + 64]             ; readfds
    xor     r8d, r8d                    ; writefds = null
    mov     QWORD PTR [rsp + 32], 0     ; exceptfds = null (5th param)
    lea     rax, [rsp + 48]
    mov     QWORD PTR [rsp + 40], rax   ; timeout (6th param)
    call    select
    
    test    eax, eax
    jle     @@relay_loop                ; Timeout or error
    
    ; Reset empty count on activity
    mov     DWORD PTR [rsp + 80], 0
    
    ; recv from client
    mov     rcx, rdi                    ; socket
    mov     rdx, r12                    ; buffer
    mov     r8d, RELAY_BUF_SIZE         ; len
    xor     r9d, r9d                    ; flags
    call    recv
    
    test    eax, eax
    jle     @@check_target              ; Error or would block
    
    mov     ebx, eax                    ; bytes received
    
    ; send to target
    mov     rcx, rsi                    ; socket
    mov     rdx, r12                    ; buffer
    mov     r8d, ebx                    ; len
    xor     r9d, r9d                    ; flags
    call    send
    
    ; Atomic add to byte counter
    movsxd  rdx, ebx
    lock add QWORD PTR [r15], rdx
    
@@check_target:
    ; recv from target
    mov     rcx, rsi                    ; socket
    mov     rdx, r12                    ; buffer
    mov     r8d, RELAY_BUF_SIZE         ; len
    xor     r9d, r9d                    ; flags
    call    recv
    
    test    eax, eax
    jle     @@relay_loop                ; Error or would block
    
    mov     ebx, eax
    
    ; send to client
    mov     rcx, rdi                    ; socket
    mov     rdx, r12                    ; buffer
    mov     r8d, ebx                    ; len
    xor     r9d, r9d                    ; flags
    call    send
    
    ; Atomic add to byte counter
    movsxd  rdx, ebx
    lock add QWORD PTR [r15], rdx
    
    jmp     @@relay_loop

@@cleanup:
    ; Close sockets
    mov     rcx, rdi
    call    closesocket
    mov     rcx, rsi
    call    closesocket
    
    ; Clear context slot
    mov     QWORD PTR [rbx + RCXT_ClientSocket], 0
    
    ; Restore and return
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

    mov     rdi, rcx                    ; Target to patch
    mov     rsi, rdx                    ; Sniper function
    
    ; Backup original 16 bytes
    mov     rax, [rdi]
    mov     rdx, [rdi + 8]
    
    ; Write hotpatch: JMP [RIP+0] (FF 25 00 00 00 00) + 8-byte addr
    mov     WORD PTR [rdi], 025FFh      ; JMP [RIP+0]
    mov     DWORD PTR [rdi + 2], 0      ; Displacement 0
    mov     [rdi + 6], rsi              ; Absolute address
    mov     WORD PTR [rdi + 14], 9090h  ; NOP padding (instead of INT3 for safety)
    
    ; Memory fence
    sfence
    
    pop     rsi
    pop     rdi
    ret
NetworkInstallSnipe ENDP

; -----------------------------------------------------------------------------
; AllocateLargePagesMemory
; rcx = Size
; Returns: pointer in RAX, or NULL
; -----------------------------------------------------------------------------
AllocateLargePagesMemory PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    
    ; Try large pages first
    xor     ecx, ecx                    ; lpAddress = NULL
    xor     edx, edx                    ; (unused, but clear for safety)
    mov     r8, rbx                     ; dwSize
    mov     r9, 02003000h               ; flAllocationType = MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES
    mov     QWORD PTR [rsp + 32], 04h   ; flProtect = PAGE_READWRITE
    call    VirtualAlloc
    
    test    rax, rax
    jnz     @@done
    
    ; Fallback to normal pages without MEM_LARGE_PAGES
    xor     ecx, ecx                    ; lpAddress = NULL
    xor     edx, edx
    mov     r8, rbx                     ; dwSize
    mov     r9, 03000h                  ; MEM_RESERVE | MEM_COMMIT
    mov     QWORD PTR [rsp + 32], 04h   ; PAGE_READWRITE
    call    VirtualAlloc
    
@@done:
    add     rsp, 40
    pop     rbx
    ret
AllocateLargePagesMemory ENDP

; -----------------------------------------------------------------------------
; RelayEngine_GetStats
; rcx = EntryPtr, rdx = OutStats
; -----------------------------------------------------------------------------
PUBLIC RelayEngine_GetStats
RelayEngine_GetStats PROC FRAME
    .endprolog                          ; No stack allocation needed
    
    mov     r8, rcx                     ; Entry
    mov     r9, rdx                     ; Out buffer
    
    ; Atomic load of bytes transferred
    mov     rax, [r8 + ENTRY_BytesTransferred]
    mov     [r9], rax
    
    ; Placeholder for tx (would need separate counter for true duplex)
    mov     [r9 + 8], rax
    
    ; Active flag
    movzx   eax, BYTE PTR [r8 + ENTRY_Running]
    and     eax, 1
    mov     DWORD PTR [r9 + 16], eax
    
    ret
RelayEngine_GetStats ENDP

END
