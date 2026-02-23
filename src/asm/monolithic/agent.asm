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

PUBLIC AgentCoreInit
PUBLIC SpawnTask
PUBLIC ProcessOneAgentMessage

; Match beacon.asm
MODEL_HOTSWAP_REQUEST   equ 1001h
MODEL_HOTSWAP_COMPLETE  equ 1002h

.data?
align 8
g_agentState  dd ?
g_taskQueue   dq ?

.data
align 4
g_completionMsg dd MODEL_HOTSWAP_COMPLETE, 0   ; type, result (updated on hotswap)

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
