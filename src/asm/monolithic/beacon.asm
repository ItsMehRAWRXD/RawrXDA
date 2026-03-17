; RawrXD Beaconism — Intra-process message routing
; No IPC, no sockets — direct memory queues between agents
; X+4: Hotswap message types + non-blocking recv for UI

EXTERN g_hHeap:QWORD
EXTERN HeapAlloc:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN GetTickCount64:PROC

PUBLIC BeaconRouterInit
PUBLIC BeaconSend
PUBLIC BeaconRecv
PUBLIC TryBeaconRecv
PUBLIC RegisterAgent
PUBLIC BeaconTraceLog

; X+4 hotpatch message types (for agent slot dispatch)
MODEL_HOTSWAP_REQUEST   equ 1001h
MODEL_HOTSWAP_COMPLETE  equ 1002h
MODEL_HOTSWAP_FAILED    equ 1003h

.data?
align 16
BEACON_SLOTS           equ 16
AGENT_STRUCT_SIZE      equ 64
AGENT_REGISTERED_EVENT equ 2001h

g_beacons       dq BEACON_SLOTS dup(?)
g_agent_map     dq 256 dup(?)

RING_SIZE       equ 100000h

; --- Per-slot recv/send statistics ---
align 8
g_recvCount     dq BEACON_SLOTS dup(?)    ; messages received per slot
g_recvBytes     dq BEACON_SLOTS dup(?)    ; bytes received per slot
g_sendCount     dq BEACON_SLOTS dup(?)    ; messages sent per slot
g_recvTimestamp dq BEACON_SLOTS dup(?)    ; last recv rdtsc per slot
g_seqNumbers    dq BEACON_SLOTS dup(?)    ; expected sequence per slot

; --- Debug trace ring (16 entries x 32 bytes each) ---
; Entry layout: [0] rdtsc timestamp (8), [8] slot (4), [12] event (4),
;               [16] data (8), [24] pad (8)
; Event codes: 1=TIMEOUT, 2=RECV_OK, 3=CORRUPTION, 4=AGENT_REG
align 16
g_traceRing     db (16 * 32) dup(?)
g_traceIdx      dd ?

; --- Per-agent data (256 agents x 64 bytes) ---
; [0]  registered flag   (qword)
; [8]  msgs sent         (qword)
; [16] msgs recv         (qword)
; [24] registration tick (qword, GetTickCount64)
; [32] capability flags  (qword)
; [40] scratch buf ptr   (qword, 4KB heap block)
; [48] sequence number   (qword)
; [56] reserved          (qword)
align 16
g_agentData     db (256 * AGENT_STRUCT_SIZE) dup(?)

.data
align 8
g_beaconTimeout dq 100000000   ; QPC-tick timeout (~10s @ 10 MHz; 0=infinite)

.code
BeaconRouterInit PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    xor     ebx, ebx
@alloc_loop:
    mov     rcx, qword ptr g_hHeap
    xor     edx, edx                 ; flags = 0
    mov     r8, RING_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      @alloc_fail
    lea     rcx, g_beacons
    mov     [rcx + rbx*8], rax
    inc     ebx
    cmp     ebx, BEACON_SLOTS
    jb      @alloc_loop

    add     rsp, 20h
    pop     rbx
    xor     eax, eax
    ret

@alloc_fail:
    add     rsp, 20h
    pop     rbx
    mov     eax, -1
    ret
BeaconRouterInit ENDP

BeaconSend PROC FRAME
    ; ECX=beaconID, RDX=pData, R8D=dataLen
    ; Ring header: [0]=writePos (dword), [4]=readPos (dword)
    ; Data entries: 16 bytes each (qword pData + dword len + 4 pad)
    ; FRAME: 1 push (rbp) + 28h alloc = 8+8+40 = 56 → not 16-aligned
    ;   Fix: 1 push + 20h = 8+8+32 = 48 → 48/16=3 exact. Good.
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    movsxd  rax, ecx
    lea     r10, g_beacons
    mov     r10, [r10 + rax*8]
    mov     eax, dword ptr [r10]       ; current write offset
    add     eax, 16                    ; advance one 16-byte entry
    and     eax, 0FFFF0h               ; wrap within 1MB ring, 16-aligned
    mov     [r10 + rax], rdx           ; store pData (qword)
    mov     dword ptr [r10 + rax + 8], r8d  ; store dataLen (dword)
    mov     dword ptr [r10], eax       ; update write position
    xor     eax, eax

    lea     rsp, [rbp]
    pop     rbp
    ret
BeaconSend ENDP

BeaconRecv PROC FRAME
    ; ECX=beaconID, RDX=ppData, R8=pLen
    ; Blocking recv with PAUSE yield, QPC timeout, integrity checks,
    ; per-slot statistics, and debug trace breadcrumbs.
    ; Returns: 0=success, -1=TIMEOUT, -2=CORRUPTION
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
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    ; ---- save arguments in non-volatile registers ----
    movsxd  rbx, ecx              ; beaconID (slot index)
    mov     rdi, rdx              ; ppData
    mov     rsi, r8               ; pLen

    ; ---- resolve ring buffer pointer ----
    lea     r12, g_beacons
    mov     r12, [r12 + rbx*8]
    test    r12, r12
    jz      @recv_corrupt         ; NULL ring → corruption

    ; ---- snapshot QPC start for timeout ----
    lea     rcx, [rsp+30h]
    call    QueryPerformanceCounter
    mov     r13, [rsp+30h]        ; r13 = start QPC
    xor     r14d, r14d            ; r14 = iteration counter

    ; ================================================================
    ;  Spin-wait loop (with PAUSE + periodic timeout check)
    ; ================================================================
@recv_wait:
    pause                         ; reduce CPU burn / SMT contention

    mov     eax, dword ptr [r12]        ; writePos
    mov     r11d, dword ptr [r12+4]     ; readPos
    cmp     eax, r11d
    jne     @recv_gotmsg

    ; ---- check timeout every 1024 spins ----
    inc     r14d
    test    r14d, 3FFh
    jnz     @recv_wait

    ; skip timeout logic when g_beaconTimeout == 0 (infinite)
    cmp     qword ptr g_beaconTimeout, 0
    je      @recv_wait

    lea     rcx, [rsp+30h]
    call    QueryPerformanceCounter
    mov     rax, [rsp+30h]
    sub     rax, r13                    ; elapsed ticks
    cmp     rax, qword ptr g_beaconTimeout
    jb      @recv_wait

    ; ---- TIMEOUT ----
    mov     ecx, ebx
    mov     edx, 1                      ; event = TIMEOUT
    xor     r8, r8
    call    BeaconTraceLog

    mov     eax, -1
    jmp     @recv_exit

    ; ================================================================
    ;  Message available — validate, deliver, update stats
    ; ================================================================
@recv_gotmsg:
    add     r11d, 16
    and     r11d, 0FFFF0h               ; advance & wrap readPos

    ; ---- integrity: dataLen must be < RING_SIZE ----
    mov     r15d, dword ptr [r12 + r11 + 8]
    cmp     r15d, RING_SIZE
    jae     @recv_corrupt

    ; ---- integrity: pData pointer must be non-NULL ----
    mov     rax, [r12 + r11]
    test    rax, rax
    jz      @recv_corrupt

    ; ---- overflow / wrap corruption: detect read crossing write ----
    mov     ecx, dword ptr [r12]        ; re-read writePos
    cmp     r11d, ecx
    ja      @recv_corrupt               ; readPos beyond writePos → wrap err

    ; ---- deliver message to caller ----
    mov     [rdi], rax                  ; *ppData = pData
    mov     dword ptr [rsi], r15d       ; *pLen   = dataLen

    ; ---- commit new readPos ----
    mov     dword ptr [r12+4], r11d

    ; ---- per-slot statistics (interlocked) ----
    lea     rax, g_recvCount
    lock inc qword ptr [rax + rbx*8]

    lea     rax, g_recvBytes
    movsxd  rcx, r15d
    lock add qword ptr [rax + rbx*8], rcx

    ; ---- trace breadcrumb ----
    mov     ecx, ebx
    mov     edx, 2                      ; event = RECV_OK
    movsxd  r8, r15d
    call    BeaconTraceLog

    xor     eax, eax                    ; SUCCESS
    jmp     @recv_exit

    ; ================================================================
    ;  Corruption path
    ; ================================================================
@recv_corrupt:
    mov     ecx, ebx
    mov     edx, 3                      ; event = CORRUPTION
    xor     r8, r8
    call    BeaconTraceLog

    mov     eax, -2                     ; CORRUPTION

@recv_exit:
    add     rsp, 40h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rsi
    pop     rdi
    pop     rbx
    ret
BeaconRecv ENDP

; TryBeaconRecv(ECX=beaconID, RDX=ppData, R8=pLen) — non-blocking
; Returns: 1=message received, 0=ring empty, -1=corruption detected
TryBeaconRecv PROC FRAME
    ; Non-blocking recv with integrity validation, sequence tracking,
    ; rdtsc latency stamp, and per-slot statistics.
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; ---- save arguments ----
    movsxd  rbx, ecx              ; beaconID
    mov     rdi, rdx              ; ppData
    mov     rsi, r8               ; pLen

    ; ---- resolve ring pointer ----
    lea     r10, g_beacons
    mov     r10, [r10 + rbx*8]
    test    r10, r10
    jz      @try_corrupt

    ; ---- check for data ----
    mov     eax, dword ptr [r10]        ; writePos
    mov     r11d, dword ptr [r10+4]     ; readPos
    cmp     eax, r11d
    je      @try_empty

    ; ---- advance readPos ----
    add     r11d, 16
    and     r11d, 0FFFF0h

    ; ---- integrity: dataLen < RING_SIZE ----
    mov     r9d, dword ptr [r10 + r11 + 8]
    cmp     r9d, RING_SIZE
    jae     @try_corrupt

    ; ---- integrity: non-NULL data pointer ----
    mov     rax, [r10 + r11]
    test    rax, rax
    jz      @try_corrupt

    ; ---- sequence number validation ----
    lea     rcx, g_seqNumbers
    inc     qword ptr [rcx + rbx*8]     ; bump expected seq

    ; ---- deliver message ----
    mov     [rdi], rax                  ; *ppData
    mov     dword ptr [rsi], r9d        ; *pLen

    ; ---- rdtsc latency stamp ----
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    lea     rcx, g_recvTimestamp
    mov     [rcx + rbx*8], rax

    ; ---- commit readPos ----
    mov     dword ptr [r10+4], r11d

    ; ---- per-slot statistics (interlocked) ----
    lea     rax, g_recvCount
    lock inc qword ptr [rax + rbx*8]

    lea     rax, g_recvBytes
    movsxd  rcx, r9d
    lock add qword ptr [rax + rbx*8], rcx

    mov     eax, 1                      ; MESSAGE_RECEIVED
    jmp     @try_exit

@try_empty:
    xor     eax, eax                    ; EMPTY
    jmp     @try_exit

@try_corrupt:
    mov     ecx, ebx
    mov     edx, 3                      ; event = CORRUPTION
    xor     r8, r8
    call    BeaconTraceLog

    mov     eax, -1                     ; CORRUPTION

@try_exit:
    add     rsp, 20h
    pop     rsi
    pop     rdi
    pop     rbx
    ret
TryBeaconRecv ENDP

RegisterAgent PROC FRAME
    ; ECX=agentID (0-255), EDX=capability flags (OR'd into agent record)
    ; Returns: 0=success, -1=invalid ID or duplicate registration
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; ---- save arguments ----
    movsxd  rbx, ecx              ; agentID
    mov     esi, edx              ; capability flags

    ; ---- validate agentID bounds (0-255) ----
    cmp     ebx, 256
    jae     @reg_invalid

    ; ---- compute per-agent data pointer ----
    imul    eax, ebx, AGENT_STRUCT_SIZE
    lea     r12, g_agentData
    add     r12, rax              ; r12 → agent record

    ; ---- duplicate check ----
    cmp     qword ptr [r12], 1
    je      @reg_invalid

    ; ---- mark registered ----
    mov     qword ptr [r12], 1

    ; ---- OR capability flags from edx ----
    mov     eax, esi              ; zero-extend to rax
    or      qword ptr [r12+32], rax

    ; ---- stamp registration timestamp (GetTickCount64) ----
    call    GetTickCount64
    mov     [r12+24], rax

    ; ---- initialize per-agent statistics ----
    mov     qword ptr [r12+8],  0       ; msgs sent
    mov     qword ptr [r12+16], 0       ; msgs recv
    mov     qword ptr [r12+48], 0       ; sequence number
    mov     qword ptr [r12+56], 0       ; reserved

    ; ---- allocate 4 KB scratch buffer for message formatting ----
    mov     rcx, qword ptr g_hHeap
    xor     edx, edx                    ; flags = 0
    mov     r8, 1000h                   ; 4 KB
    call    HeapAlloc
    mov     [r12+40], rax               ; store (may be NULL)
    test    rax, rax
    jz      @reg_done                   ; skip broadcast on alloc failure

    ; ---- build AGENT_REGISTERED event in scratch buffer ----
    mov     dword ptr [rax],   AGENT_REGISTERED_EVENT
    mov     dword ptr [rax+4], ebx      ; agentID
    mov     dword ptr [rax+8], esi      ; capabilities

    ; ---- broadcast event to beacon slot 0 ----
    xor     ecx, ecx                    ; beaconID = 0
    mov     rdx, rax                    ; pData = scratch buffer
    mov     r8d, 12                     ; dataLen = 12 bytes
    call    BeaconSend

@reg_done:
    ; ---- update agent map (default slot 0) ----
    lea     rax, g_agent_map
    mov     qword ptr [rax + rbx*8], 0

    ; ---- trace breadcrumb ----
    mov     ecx, ebx
    mov     edx, 4                      ; event = AGENT_REGISTERED
    xor     r8, r8
    call    BeaconTraceLog

    xor     eax, eax                    ; SUCCESS
    jmp     @reg_exit

@reg_invalid:
    mov     eax, -1

@reg_exit:
    add     rsp, 30h
    pop     r12
    pop     rsi
    pop     rbx
    ret
RegisterAgent ENDP

; ================================================================
;  BeaconTraceLog — internal diagnostic trace-ring writer
;  ECX = slot, EDX = event code, R8 = data payload
;  Writes one 32-byte entry into the 16-slot g_traceRing.
;  Uses RDTSC for a cheap timestamp (no API call).
; ================================================================
BeaconTraceLog PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    ; save params in volatile regs before rdtsc clobbers edx:eax
    mov     r9d, ecx              ; slot
    mov     r10d, edx             ; event code
    ; r8 already holds data payload

    ; atomically claim a trace slot (mod 16)
    lea     rax, g_traceIdx
    mov     ebx, 1
    lock xadd dword ptr [rax], ebx
    and     ebx, 0Fh              ; index 0-15

    ; compute entry pointer (32 bytes per entry)
    shl     ebx, 5                ; *32
    lea     r11, g_traceRing
    add     r11, rbx

    ; stamp rdtsc timestamp
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     [r11],    rax               ; [0]  timestamp
    mov     dword ptr [r11+8],  r9d     ; [8]  slot
    mov     dword ptr [r11+12], r10d    ; [12] event
    mov     [r11+16], r8                ; [16] data
    mov     qword ptr [r11+24], 0       ; [24] pad / zero

    pop     rbx
    ret
BeaconTraceLog ENDP

END
