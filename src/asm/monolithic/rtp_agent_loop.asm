; =============================================================================
; rtp_agent_loop.asm — x64 MASM — RTP plan→execute→verify agent loop
; ABI: RTP_AgentLoop_Run(userPrompt, outBuf, outCap, maxIters)
; =============================================================================
OPTION CASEMAP:NONE

EXTERN InferenceRouter_Generate:PROC
EXTERN RTP_StreamParser_Reset:PROC
EXTERN RTP_StreamParser_PushByte:PROC
EXTERN RTP_StreamParser_GetPacket:PROC
EXTERN RTP_DispatchPacket:PROC
EXTERN RTP_EncodeToolResultFrame:PROC
EXTERN g_rtpAgentLoopRounds:QWORD

PUBLIC RTP_AgentLoop_Run

MAX_RESP_BYTES      equ 4096
MAX_PACKET_BYTES    equ 8192
MAX_RESULT_BYTES    equ 8192

.data?
align 16
g_loopRespBuf       db MAX_RESP_BYTES dup(?)
g_loopPacketBuf     db MAX_PACKET_BYTES dup(?)
g_loopResultFrame   db MAX_RESULT_BYTES dup(?)

.code

RTP_AgentLoop_Run PROC FRAME
    ; RCX=user_prompt_utf8, RDX=out_buf, R8D=out_cap, R9D=max_iters
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
    push    r14
    .pushreg r14
    sub     rsp, 64
    .allocstack 64
    .endprolog

    mov     r12, rcx                    ; current prompt
    mov     r13, rdx                    ; out buf
    mov     r14d, r8d                   ; out cap
    mov     ebx, r9d                    ; max iters
    test    r14d, r14d
    jle     @@bad_outcap
    test    ebx, ebx
    jnz     @@iters_ok
    mov     ebx, 4
@@iters_ok:

    xor     edi, edi                    ; iter index
    xor     esi, esi                    ; tool-call rounds executed

@@loop_begin:
    cmp     edi, ebx
    jae     @@max_iters
    lock inc qword ptr [g_rtpAgentLoopRounds]

    ; Plan phase: ask model for next action/result
    mov     rcx, r12
    lea     rdx, g_loopRespBuf
    mov     r8d, MAX_RESP_BYTES - 1
    call    InferenceRouter_Generate
    cmp     eax, 1
    jl      @@infer_fail

    ; null terminate response
    lea     rcx, g_loopRespBuf
    mov     byte ptr [rcx + rax], 0
    mov     dword ptr [rsp+48], eax     ; resp len
    mov     dword ptr [rsp+44], 0       ; scan index

    ; Execute detect phase: scan response for RTP packet
    call    RTP_StreamParser_Reset
@@scan_loop:
    mov     eax, dword ptr [rsp+44]
    cmp     eax, dword ptr [rsp+48]
    jae     @@no_packet
    movzx   ecx, byte ptr [g_loopRespBuf + rax]
    call    RTP_StreamParser_PushByte
    cmp     eax, 1
    je      @@packet_ready
    inc     dword ptr [rsp+44]
    jmp     @@scan_loop

@@packet_ready:
    ; Extract packet
    lea     rcx, g_loopPacketBuf
    mov     edx, MAX_PACKET_BYTES
    lea     r8, [rsp+40]
    call    RTP_StreamParser_GetPacket
    cmp     eax, 0
    jne     @@no_packet

    ; Execute tool call (writes tool result to out buffer)
    lea     rcx, g_loopPacketBuf
    mov     edx, dword ptr [rsp+40]
    mov     r8,  r13
    mov     r9d, r14d
    call    RTP_DispatchPacket
    test    eax, eax
    jne     @@dispatch_fail
    mov     dword ptr [rsp+52], eax

    ; Verify phase: ensure tool result is non-empty
    cmp     byte ptr [r13], 0
    je      @@verify_fail

    ; Build result frame for model/tool feedback path (best effort)
    mov     ecx, edi
    mov     edx, dword ptr [rsp+52]
    mov     r8,  r13
    ; compute payload size quickly (bounded)
    xor     eax, eax
@@len_loop:
    cmp     eax, r14d
    jae     @@len_done
    cmp     byte ptr [r13 + rax], 0
    je      @@len_done
    inc     eax
    jmp     @@len_loop
@@len_done:
    mov     r9d, eax
    lea     rax, g_loopResultFrame
    mov     [rsp+20h], rax
    mov     dword ptr [rsp+28h], MAX_RESULT_BYTES
    lea     rax, [rsp+56]
    mov     [rsp+30h], rax
    call    RTP_EncodeToolResultFrame

    ; Next planning prompt becomes last tool result (closed-loop agent)
    mov     r12, r13
    inc     esi
    inc     edi
    jmp     @@loop_begin

@@no_packet:
    ; No tool call => final answer. copy response to out and finish success.
    cmp     r14d, 1
    jb      @@bad_outcap
    lea     rsi, g_loopRespBuf
    mov     rdi, r13
    mov     ecx, dword ptr [rsp+48]
    cmp     ecx, r14d
    jb      @@copy_resp
    mov     ecx, r14d
    dec     ecx
@@copy_resp:
    rep     movsb
    mov     byte ptr [rdi], 0
    test    esi, esi
    jz      @@ret_final
    mov     eax, 1                      ; success after >=1 tool round
    jmp     @@ret
@@ret_final:
    xor     eax, eax                    ; success, no tool rounds needed
    jmp     @@ret

@@infer_fail:
    mov     eax, -10
    jmp     @@ret
@@dispatch_fail:
    mov     eax, -20
    jmp     @@ret
@@verify_fail:
    mov     eax, -21
    jmp     @@ret
@@bad_outcap:
    mov     eax, -11
    jmp     @@ret
@@max_iters:
    mov     eax, -30

@@ret:
    add     rsp, 64
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RTP_AgentLoop_Run ENDP

END
