; RawrXD Beaconism — Intra-process message routing
; No IPC, no sockets — direct memory queues between agents
; X+4: Hotswap message types + non-blocking recv for UI

EXTERN g_hHeap:QWORD
EXTERN HeapAlloc:PROC

PUBLIC BeaconRouterInit
PUBLIC BeaconSend
PUBLIC BeaconRecv
PUBLIC TryBeaconRecv
PUBLIC RegisterAgent

; X+4 hotpatch message types (for agent slot dispatch)
MODEL_HOTSWAP_REQUEST   equ 1001h
MODEL_HOTSWAP_COMPLETE  equ 1002h

.data?
align 16
BEACON_SLOTS  equ 16
g_beacons     dq BEACON_SLOTS dup(?)
g_agent_map   dq 256 dup(?)

RING_SIZE     equ 100000h

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
    xor     edx, edx
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

BeaconSend PROC
    ; ECX=beaconID, RDX=pData, R8D=dataLen
    movsxd  rax, ecx
    lea     r10, g_beacons
    mov     r10, [r10 + rax*8]
    mov     eax, dword ptr [r10]
    add     eax, 8
    and     eax, RING_SIZE - 1
    mov     [r10 + rax], rdx
    mov     dword ptr [r10 + rax + 8], r8d
    mov     dword ptr [r10], eax
    xor     eax, eax
    ret
BeaconSend ENDP

BeaconRecv PROC
    ; ECX=beaconID, RDX=ppData, R8=pLen
    movsxd  rax, ecx
    lea     r10, g_beacons
    mov     r10, [r10 + rax*8]
@wait:
    mov     eax, dword ptr [r10]
    mov     r11d, dword ptr [r10+4]
    cmp     eax, r11d
    je      @wait
    add     r11d, 8
    and     r11d, RING_SIZE - 1
    mov     rax, [r10 + r11]
    mov     [rdx], rax
    mov     eax, dword ptr [r10 + r11 + 8]
    mov     dword ptr [r8], eax
    mov     dword ptr [r10+4], r11d
    xor     eax, eax
    ret
BeaconRecv ENDP

; TryBeaconRecv(ECX=beaconID, RDX=ppData, R8=pLen) — non-blocking
; Returns EAX=1 if message read, 0 if ring empty
TryBeaconRecv PROC
    movsxd  rax, ecx
    lea     r10, g_beacons
    mov     r10, [r10 + rax*8]
    mov     eax, dword ptr [r10]
    mov     r11d, dword ptr [r10+4]
    cmp     eax, r11d
    je      @try_none
    add     r11d, 8
    and     r11d, RING_SIZE - 1
    mov     rax, [r10 + r11]
    mov     [rdx], rax
    mov     eax, dword ptr [r10 + r11 + 8]
    mov     dword ptr [r8], eax
    mov     dword ptr [r10+4], r11d
    mov     eax, 1
    ret
@try_none:
    xor     eax, eax
    ret
TryBeaconRecv ENDP

RegisterAgent PROC
    ; ECX=agentID, EDX=beaconSlot
    movsxd  rax, ecx
    lea     rcx, g_agent_map
    mov     [rcx + rax*8], rdx
    xor     eax, eax
    ret
RegisterAgent ENDP

END
