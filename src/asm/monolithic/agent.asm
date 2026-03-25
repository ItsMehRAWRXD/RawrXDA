; RawrXD Agent Core — Self-directing task execution
; X+4: ProcessOneAgentMessage dispatches MODEL_HOTSWAP_REQUEST → HotSwapModel

EXTERN BeaconSend:PROC
EXTERN BeaconRecv:PROC
EXTERN TryBeaconRecv:PROC
EXTERN RunInference:PROC
EXTERN RegisterAgent:PROC
EXTERN HotSwapModel:PROC
EXTERN g_hHeap:QWORD
EXTERN HeapAlloc:PROC
<<<<<<< HEAD
EXTERN GetTickCount64:PROC
=======
>>>>>>> origin/main

PUBLIC AgentCoreInit
PUBLIC SpawnTask
PUBLIC ProcessOneAgentMessage
<<<<<<< HEAD
PUBLIC DefaultDispatchHandler
=======
>>>>>>> origin/main

; Match beacon.asm
MODEL_HOTSWAP_REQUEST   equ 1001h
MODEL_HOTSWAP_COMPLETE  equ 1002h

.data?
align 8
<<<<<<< HEAD
g_agentState        dd ?
g_agentLastError    dd ?
g_taskQueue         dq ?

; Agent telemetry counters
align 8
g_agentMsgsProcessed  dq ?
g_agentMsgsDropped    dq ?
g_agentAvgLatency     dq ?
g_agentPeakLatency    dq ?
g_agentUptimeTicks    dq ?
g_agentBirthTick      dq ?
g_agentHeartbeat      dq ?

; Agent resource pointers
g_agentScratchBuf     dq ?
g_agentRetryQueue     dq ?

; Retry queue global tracking (mirrors heap-resident header)
g_agentRetryWritePos  dd ?
g_agentRetryReadPos   dd ?
g_agentRetryCount     dd ?
=======
g_agentState  dd ?
g_taskQueue   dq ?
>>>>>>> origin/main

.data
align 4
g_completionMsg dd MODEL_HOTSWAP_COMPLETE, 0   ; type, result (updated on hotswap)

<<<<<<< HEAD
; Priority dispatch table — 8 entries x 16 bytes
; Layout per entry: dd msgType, dd priority, dq handlerPtr
align 16
g_dispatchTable:
    dd MODEL_HOTSWAP_REQUEST, 10     ;  [0] hotswap request, pri 10
    dq 0
    dd MODEL_HOTSWAP_COMPLETE, 5     ;  [1] hotswap complete, pri 5
    dq 0
    dd 1003h, 10                     ;  [2] hotswap failed,  pri 10
    dq 0
    dd 2001h, 8                      ;  [3] task execute,    pri 8
    dq 0
    dd 2002h, 3                      ;  [4] task status qry, pri 3
    dq 0
    dd 3001h, 7                      ;  [5] inference req,   pri 7
    dq 0
    dd 3002h, 6                      ;  [6] inference res,   pri 6
    dq 0
    dd 0FFFFh, 1                     ;  [7] heartbeat ping,  pri 1
    dq 0

.const
AGENT_ID      equ 0A1h

; Agent FSM states
AGENT_STATE_IDLE          equ 0
AGENT_STATE_PROCESSING    equ 1
AGENT_STATE_WAITING_RESP  equ 2
AGENT_STATE_ERROR         equ 3

; Sizing constants
TASK_QUEUE_SIZE           equ 10000h    ; 64KB task ring buffer
TASK_RING_CAPACITY        equ 0FFFh     ; max 16-byte entry slots
SCRATCH_BUF_SIZE          equ 10000h    ; 64KB scratch buffer
RETRY_ENTRY_SIZE          equ 20h       ; 32 bytes per retry entry
RETRY_QUEUE_ENTRIES       equ 10h       ; 16 entries
RETRY_QUEUE_ALLOC         equ 210h      ; header(16) + 16*32 = 528

DISPATCH_ENTRY_SIZE       equ 10h       ; 16 bytes per dispatch entry
DISPATCH_TABLE_COUNT      equ 8

; Additional message type equates
MSG_HOTSWAP_FAILED        equ 1003h
MSG_TASK_EXECUTE          equ 2001h
MSG_TASK_STATUS_QUERY     equ 2002h
MSG_INFERENCE_REQUEST     equ 3001h
MSG_INFERENCE_RESULT      equ 3002h
MSG_HEARTBEAT_PING        equ 0FFFFh

.code
AgentCoreInit PROC FRAME
    ; ===========================================================================
    ; AgentCoreInit — Full agent subsystem initialization
    ;   Allocates task ring, scratch buffer, retry queue; seeds dispatch table,
    ;   telemetry counters, heartbeat, and birth timestamp.  Sends AGENT_ONLINE
    ;   beacon to slot 0.
    ;   Returns: RAX = 0  success, -1  allocation failure
    ; ===========================================================================
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 80h
    .allocstack 80h
    .endprolog

    ; ------------------------------------------------------------------
    ; Step 1: Register this agent with the beacon router
    ;         RegisterAgent(agentID=0xA1, beaconSlot=1)
    ; ------------------------------------------------------------------
    mov     ecx, AGENT_ID
    mov     edx, 1
    call    RegisterAgent
    test    eax, eax
    jnz     @aci_fail

    ; ------------------------------------------------------------------
    ; Step 2: Set initial FSM state -> IDLE
    ; ------------------------------------------------------------------
    mov     dword ptr g_agentState, AGENT_STATE_IDLE

    ; ------------------------------------------------------------------
    ; Step 3: Allocate task queue ring buffer (64 KB)
    ;         HeapAlloc(g_hHeap, 0, TASK_QUEUE_SIZE)
    ; ------------------------------------------------------------------
    mov     rcx, qword ptr g_hHeap
    xor     edx, edx
    mov     r8d, TASK_QUEUE_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      @aci_fail
    mov     g_taskQueue, rax
    mov     rbx, rax                        ; rbx = task ring base

    ; ------------------------------------------------------------------
    ; Step 4: Initialize ring buffer header
    ;   [+00] writePos  dd 0
    ;   [+04] readPos   dd 0
    ;   [+08] capacity  dd TASK_RING_CAPACITY
    ;   [+0C] flags     dd 0
    ; ------------------------------------------------------------------
    mov     dword ptr [rbx],     0
    mov     dword ptr [rbx+4],   0
    mov     dword ptr [rbx+8],   TASK_RING_CAPACITY
    mov     dword ptr [rbx+0Ch], 0

    ; Zero first 256 bytes past header as sentinel region
    lea     rdi, [rbx+10h]
    xor     eax, eax
    mov     ecx, 32                         ; 32 x 8 = 256 bytes
@aci_ztq:
    mov     qword ptr [rdi], rax
    lea     rdi, [rdi+8]
    dec     ecx
    jnz     @aci_ztq

    ; ------------------------------------------------------------------
    ; Step 5: Allocate scratch buffer (64 KB) for message assembly
    ; ------------------------------------------------------------------
    mov     rcx, qword ptr g_hHeap
    xor     edx, edx
    mov     r8d, SCRATCH_BUF_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      @aci_fail
    mov     g_agentScratchBuf, rax
    mov     r12, rax                        ; r12 = scratch base

    ; Zero first 64 bytes (message assembly header region)
    xor     eax, eax
    mov     qword ptr [r12],      rax
    mov     qword ptr [r12+8],    rax
    mov     qword ptr [r12+10h],  rax
    mov     qword ptr [r12+18h],  rax
    mov     qword ptr [r12+20h],  rax
    mov     qword ptr [r12+28h],  rax
    mov     qword ptr [r12+30h],  rax
    mov     qword ptr [r12+38h],  rax

    ; ------------------------------------------------------------------
    ; Step 6: Allocate retry queue (header + 16 x 32-byte entries)
    ;   Header:
    ;     [+00] writePos  dd
    ;     [+04] readPos   dd
    ;     [+08] capacity  dd
    ;     [+0C] count     dd
    ;   Entry (32 bytes):
    ;     [+00] msgType     dd
    ;     [+04] retryCount  dd
    ;     [+08] pData       dq
    ;     [+10] dataLen     dd
    ;     [+14] timestamp   dd
    ;     [+18] reserved    dq
    ; ------------------------------------------------------------------
    mov     rcx, qword ptr g_hHeap
    xor     edx, edx
    mov     r8d, RETRY_QUEUE_ALLOC
    call    HeapAlloc
    test    rax, rax
    jz      @aci_fail
    mov     g_agentRetryQueue, rax
    mov     r13, rax                        ; r13 = retry queue base

    ; Initialize retry queue header
    mov     dword ptr [r13],     0
    mov     dword ptr [r13+4],   0
    mov     dword ptr [r13+8],   RETRY_QUEUE_ENTRIES
    mov     dword ptr [r13+0Ch], 0

    ; Zero all 16 retry entries: 16 x 32 = 512 bytes = 64 qwords
    lea     rdi, [r13+10h]
    xor     eax, eax
    mov     ecx, 64
@aci_zrq:
    mov     qword ptr [rdi], rax
    lea     rdi, [rdi+8]
    dec     ecx
    jnz     @aci_zrq

    ; Mirror retry state into globals
    mov     dword ptr g_agentRetryWritePos, 0
    mov     dword ptr g_agentRetryReadPos,  0
    mov     dword ptr g_agentRetryCount,    0

    ; ------------------------------------------------------------------
    ; Step 7: Seed priority dispatch table handler pointers
    ;   Entry layout: dd msgType, dd priority, dq handlerPtr
    ;   Slot 0 -> ProcessOneAgentMessage (hotswap path)
    ;   Slots 1-7 -> DefaultDispatchHandler (stubs)
    ; ------------------------------------------------------------------
    lea     rsi, g_dispatchTable

    ; Slot 0: MODEL_HOTSWAP_REQUEST -> ProcessOneAgentMessage
    lea     rax, ProcessOneAgentMessage
    mov     qword ptr [rsi + 08h], rax

    ; Slots 1-7: default handler for remaining message types
    lea     rax, DefaultDispatchHandler
    mov     qword ptr [rsi + 18h], rax       ; slot 1
    mov     qword ptr [rsi + 28h], rax       ; slot 2
    mov     qword ptr [rsi + 38h], rax       ; slot 3
    mov     qword ptr [rsi + 48h], rax       ; slot 4
    mov     qword ptr [rsi + 58h], rax       ; slot 5
    mov     qword ptr [rsi + 68h], rax       ; slot 6
    mov     qword ptr [rsi + 78h], rax       ; slot 7

    ; ------------------------------------------------------------------
    ; Step 8: Zero all telemetry counters
    ; ------------------------------------------------------------------
    xor     eax, eax
    mov     g_agentMsgsProcessed, rax
    mov     g_agentMsgsDropped,   rax
    mov     g_agentAvgLatency,    rax
    mov     g_agentPeakLatency,   rax
    mov     g_agentUptimeTicks,   rax
    mov     dword ptr g_agentLastError, eax

    ; ------------------------------------------------------------------
    ; Step 9: Initialize heartbeat counter for liveness detection
    ; ------------------------------------------------------------------
    mov     g_agentHeartbeat, rax             ; heartbeat = 0

    ; ------------------------------------------------------------------
    ; Step 10: Stamp birth timestamp via GetTickCount64
    ; ------------------------------------------------------------------
    call    GetTickCount64
    mov     g_agentBirthTick, rax
    mov     [rsp+20h], rax                    ; stash birth tick locally

    ; ------------------------------------------------------------------
    ; Step 11: Broadcast AGENT_ONLINE beacon to slot 0
    ;   Payload (16 bytes in scratch buffer):
    ;     [+0] agentID     dd
    ;     [+4] state       dd
    ;     [+8] birthTick   dq
    ; ------------------------------------------------------------------
    mov     dword ptr [r12],     AGENT_ID
    mov     dword ptr [r12+4],   AGENT_STATE_IDLE
    mov     rax, [rsp+20h]
    mov     qword ptr [r12+8],   rax

    xor     ecx, ecx                         ; beacon slot 0
    mov     rdx, r12                          ; pData = scratch
    mov     r8d, 10h                          ; 16 bytes
    call    BeaconSend

    ; ------------------------------------------------------------------
    ; Success path
    ; ------------------------------------------------------------------
    xor     eax, eax
    jmp     @aci_epilog

@aci_fail:
    ; Mark agent as ERROR on any allocation failure
    mov     dword ptr g_agentState, AGENT_STATE_ERROR
    mov     dword ptr g_agentLastError, -1
    mov     eax, -1

@aci_epilog:
    add     rsp, 80h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
AgentCoreInit ENDP

; ---------------------------------------------------------------------------
; DefaultDispatchHandler — Dispatch-table fallback for unhandled message types
;   RCX = pMsgData (pointer to message payload)
;   EDX = msgType  (message type that had no specific handler)
;
;   Logs diagnostic to beacon slot 0, increments drop counter.
;   Returns: EAX = 0 (acknowledged)
; ---------------------------------------------------------------------------
DefaultDispatchHandler PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 60h
    .allocstack 60h
    .endprolog

    mov     ebx, edx                    ; preserve msgType
    mov     rsi, rcx                    ; preserve pMsgData

    ; --- Increment dropped-message counter ---
    lock inc qword ptr g_agentMsgsDropped

    ; --- Build 24-byte diagnostic payload on stack ---
    ;   [rsp+32] dd  0DEADh         ; sentinel ("unhandled")
    ;   [rsp+36] dd  msgType
    ;   [rsp+40] dq  pMsgData
    ;   [rsp+48] dq  timestamp (GetTickCount64)
    mov     dword ptr [rsp+20h], 0DEADh
    mov     dword ptr [rsp+24h], ebx
    mov     qword ptr [rsp+28h], rsi

    call    GetTickCount64
    mov     qword ptr [rsp+30h], rax

    ; --- BeaconSend(slot=0, pPayload, 24) to diagnostics slot ---
    xor     ecx, ecx                    ; beacon slot 0 = diagnostics
    lea     rdx, [rsp+20h]
    mov     r8d, 24
    call    BeaconSend

    xor     eax, eax                    ; acknowledged
    add     rsp, 60h
    pop     rsi
    pop     rbx
    ret
DefaultDispatchHandler ENDP

; ---------------------------------------------------------------------------
; SpawnTask — Enqueue a task into the agent task ring + beacon notify
;   ECX = taskType  (e.g. MSG_TASK_EXECUTE, MSG_INFERENCE_REQUEST)
;   RDX = pContext  (pointer to task-specific context data)
;
;   Writes a 16-byte entry into g_taskQueue ring buffer:
;     [+0] dd taskType
;     [+4] dd 0 (status = PENDING)
;     [+8] dq pContext
;   Advances writePos.  Sends beacon notify on slot 1.
;   Returns: EAX = 0 success, -1 ring full
; ---------------------------------------------------------------------------
SpawnTask PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    mov     ebx, ecx                    ; taskType
    mov     rsi, rdx                    ; pContext

    ; ---- Validate ring buffer ----
    mov     rdi, g_taskQueue
    test    rdi, rdi
    jz      @st_fail                    ; ring not allocated

    ; ---- Read ring header ----
    ;   [+0] writePos, [+4] readPos, [+8] capacity
    mov     eax, dword ptr [rdi]        ; writePos
    mov     ecx, dword ptr [rdi+4]      ; readPos
    mov     edx, dword ptr [rdi+8]      ; capacity

    ; ---- Advance write position ----
    lea     r8d, [eax + 1]
    and     r8d, edx                     ; wrap (capacity is power-of-2 mask)
    cmp     r8d, ecx
    je      @st_fail                    ; ring full

    ; ---- Compute entry address: base + 10h + writePos * 16 ----
    movsxd  rax, eax
    shl     rax, 4                      ; * 16
    lea     rcx, [rdi + rax + 10h]      ; skip 16-byte header

    ; ---- Write 16-byte task entry ----
    mov     dword ptr [rcx],     ebx    ; taskType
    mov     dword ptr [rcx + 4], 0      ; status = PENDING
    mov     qword ptr [rcx + 8], rsi    ; pContext

    ; ---- Commit new writePos ----
    mov     dword ptr [rdi], r8d

    ; ---- Beacon notify slot 1 (agent message channel) ----
    ;   Payload = the task entry we just wrote (16 bytes)
    mov     ecx, 1                      ; beacon slot 1
    mov     rdx, rcx                    ; pData = task entry
    lea     rdx, [rdi + rax + 10h]
    mov     r8d, 10h                    ; 16 bytes
    call    BeaconSend

    ; ---- Increment processed counter ----
    lock inc qword ptr g_agentMsgsProcessed

    xor     eax, eax                    ; success
    jmp     @st_done

@st_fail:
    lock inc qword ptr g_agentMsgsDropped
    mov     eax, -1

@st_done:
    add     rsp, 40h
    pop     rdi
    pop     rsi
    pop     rbx
=======
.const
AGENT_ID      equ 0A1h

.code
AgentCoreInit PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     ecx, AGENT_ID
    mov     edx, 1
    call    RegisterAgent

    mov     rcx, qword ptr g_hHeap
    xor     edx, edx
    mov     r8, 10000h
    call    HeapAlloc
    mov     g_taskQueue, rax

    add     rsp, 28h
    xor     eax, eax
    ret
AgentCoreInit ENDP

SpawnTask PROC
    ; ECX=taskType, RDX=pContext
    mov     r8d, ecx
    mov     ecx, 1
    ; BeaconSend(beaconID, pData, dataLen)
    call    BeaconSend
>>>>>>> origin/main
    ret
SpawnTask ENDP

; ProcessOneAgentMessage — poll slot 1; if MODEL_HOTSWAP_REQUEST, call HotSwapModel and notify slot 0
ProcessOneAgentMessage PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog
    ; TryBeaconRecv(1, &pData, &len)
    mov     ecx, 1
    lea     rdx, [rsp+20h]      ; ppData
    lea     r8, [rsp+28h]      ; pLen
    call    TryBeaconRecv
    test    eax, eax
    jz      @agent_done
    mov     rbx, [rsp+20h]
    mov     eax, dword ptr [rbx]
    cmp     eax, MODEL_HOTSWAP_REQUEST
    jne     @agent_done
    ; HotSwapModel(path = rbx+4, preserveKV = 1)
    lea     rcx, [rbx+4]
    mov     dl, 1
    call    HotSwapModel
    mov     dword ptr g_completionMsg+4, eax
    ; BeaconSend(0, &g_completionMsg, 8)
    mov     ecx, 0
    lea     rdx, g_completionMsg
    mov     r8d, 8
    call    BeaconSend
@agent_done:
    add     rsp, 30h
    pop     rbx
    ret
ProcessOneAgentMessage ENDP

END
