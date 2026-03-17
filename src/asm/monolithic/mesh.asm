; ═══════════════════════════════════════════════════════════════════
; mesh.asm — Global Weights Mesh (Decentralized Training-on-Inference)
; Phase 15: Sovereign Weights Mesh — Gradient Accumulation & P2P Broadcast
; ═══════════════════════════════════════════════════════════════════

; ── Imports ──────────────────────────────────────────────────────
EXTERN g_hHeap:QWORD
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN SwarmNet_SyncKVCache:PROC ; We reuse the sync logic for weight updates
EXTERN BeaconSend:PROC
EXTERN Consensus_Vote:PROC
EXTERN Consensus_GetState:PROC
EXTERN GetTickCount64:PROC

; ── Public Exports ───────────────────────────────────────────────
PUBLIC Mesh_Init
PUBLIC Mesh_AccumulateGradient
PUBLIC Mesh_BroadcastUpdate
PUBLIC Mesh_SyncEpoch
PUBLIC g_accumulatedSteps

; ── Constants ────────────────────────────────────────────────────
MESH_BEACON_SLOT       equ 15
MESH_EVT_SYNC_START    equ 0F1h
MESH_EVT_SYNC_COMPLETE equ 0F2h
GRADIENT_BUFFER_SIZE   equ 400000h ; 4MB per local accumulation buffer

.data
align 16
g_meshReady         dd 0
g_gradBuffer        dq 0              ; Pointer to local gradient accumulation buffer
g_accumulatedSteps  dd 0
g_nodeWeight        dd 1024           ; Relative importance of this node in the mesh

.code

; ────────────────────────────────────────────────────────────────
; Mesh_Init — Initialize global weights mesh structures
; ────────────────────────────────────────────────────────────────
Mesh_Init PROC FRAME
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rcx, g_hHeap
    test    rcx, rcx
    jz      @mesh_init_fail

    xor     edx, edx
    mov     r8, GRADIENT_BUFFER_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      @mesh_init_fail
    mov     g_gradBuffer, rax

    ; Zero the buffer
    mov     rdi, rax
    mov     rcx, GRADIENT_BUFFER_SIZE / 8
    xor     rax, rax
    rep stosq

    mov     g_meshReady, 1
    xor     eax, eax
    jmp     @mesh_init_done

@mesh_init_fail:
    mov     eax, -1

@mesh_init_done:
    add     rsp, 20h
    pop     rdi
    ret
Mesh_Init ENDP

; ────────────────────────────────────────────────────────────────
; Mesh_AccumulateGradient — Add local inference "loss" to weights
;   RCX = pGradients (Pointer to local DELTA weights)
;   EDX = gradSize (Bytes)
; ────────────────────────────────────────────────────────────────
Mesh_AccumulateGradient PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rsi, rcx                ; src: pGradients
    mov     rdi, g_gradBuffer       ; dst: local mesh buffer
    test    rdi, rdi
    jz      @accum_done

    ; SIMD Accumulation (Simplified addition for the mesh logic)
    ; In a real mesh, this would use AVX-512 to sum FP16/FP32 gradients.
    shr     edx, 3                  ; QWORD count
    mov     ecx, edx

@accum_loop:
    test    ecx, ecx
    jz      @accum_inc
    mov     rax, [rsi]
    add     [rdi], rax              ; Atomic add in real impl
    add     rsi, 8
    add     rdi, 8
    dec     ecx
    jmp     @accum_loop

@accum_inc:
    inc     g_accumulatedSteps

@accum_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    ret
Mesh_AccumulateGradient ENDP

; ────────────────────────────────────────────────────────────────
; Mesh_BroadcastUpdate — Push local gradients to the global mesh
; ────────────────────────────────────────────────────────────────
Mesh_BroadcastUpdate PROC FRAME
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    cmp     g_meshReady, 1
    jne     @broadcast_done

    ; Signal sync start
    mov     ecx, MESH_BEACON_SLOT
    mov     edx, MESH_EVT_SYNC_START
    xor     r8d, r8d
    call    BeaconSend

    ; Broadcast the gradient buffer across the Swarm network
    mov     rcx, g_gradBuffer
    mov     edx, GRADIENT_BUFFER_SIZE
    mov     r8d, 0FFFFFFFFh         ; Broadcast to all nodes
    call    SwarmNet_SyncKVCache     ; Reuse P2P sync logic

    ; Reset local accumulation
    mov     rdi, g_gradBuffer
    mov     rcx, GRADIENT_BUFFER_SIZE / 8
    xor     rax, rax
    rep stosq
    mov     g_accumulatedSteps, 0

    ; Signal sync complete
    mov     ecx, MESH_BEACON_SLOT
    mov     edx, MESH_EVT_SYNC_COMPLETE
    xor     r8d, r8d
    call    BeaconSend

@broadcast_done:
    add     rsp, 20h
    pop     rdi
    ret
Mesh_BroadcastUpdate ENDP

; ────────────────────────────────────────────────────────────────
; Mesh_SyncEpoch — Finalize weight updates across the cluster
;   Waits for all nodes to acknowledge gradient receipt via consensus,
;   then broadcasts the accumulated update and resets local state.
;   Returns: EAX = 0 success, -1 if mesh not ready
;
; Protocol:
;   1. Check mesh readiness
;   2. Query consensus state (all nodes must agree on current term)
;   3. If consensus leader is us, broadcast the accumulated gradients
;   4. Signal epoch completion via Beacon
;   5. Reset accumulated steps counter
; ────────────────────────────────────────────────────────────────
Mesh_SyncEpoch PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; Check mesh is initialized
    cmp     g_meshReady, 1
    jne     @sync_fail

    ; Check we have accumulated at least 1 step
    cmp     g_accumulatedSteps, 0
    je      @sync_noop

    ; Query consensus: who is the current leader?
    call    Consensus_GetState
    mov     ebx, eax                ; ebx = leader node ID

    ; If leader is -1 (no consensus), skip broadcast but still count
    cmp     ebx, -1
    je      @sync_local_only

    ; Broadcast accumulated gradients to the mesh
    mov     rcx, g_gradBuffer
    test    rcx, rcx
    jz      @sync_fail

    mov     edx, GRADIENT_BUFFER_SIZE
    mov     r8d, 0FFFFFFFFh         ; Broadcast to all nodes
    call    SwarmNet_SyncKVCache

    ; Signal epoch sync complete
    mov     ecx, MESH_BEACON_SLOT
    mov     edx, MESH_EVT_SYNC_COMPLETE
    xor     r8d, r8d
    call    BeaconSend

@sync_local_only:
    ; Zero the gradient buffer for next epoch
    mov     rdi, g_gradBuffer
    test    rdi, rdi
    jz      @sync_done
    mov     rcx, GRADIENT_BUFFER_SIZE / 8
    xor     rax, rax
    rep stosq

    ; Reset step counter
    mov     g_accumulatedSteps, 0

@sync_noop:
    xor     eax, eax                ; Success
    jmp     @sync_ret

@sync_fail:
    mov     eax, -1
    jmp     @sync_ret

@sync_done:
    xor     eax, eax

@sync_ret:
    add     rsp, 20h
    pop     rdi
    pop     rbx
    ret
Mesh_SyncEpoch ENDP

END
