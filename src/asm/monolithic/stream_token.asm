; stream_token.asm — Token streaming bridge for Ollama/UI
OPTION CASEMAP:NONE

EXTERN BeaconSend:PROC
EXTERN GetTickCount64:PROC

PUBLIC StreamToken_SetDispatch
PUBLIC StreamToken_ClearDispatch
PUBLIC StreamToken_Callback
PUBLIC StreamToken_GetStats
PUBLIC g_streamTokenSeq
PUBLIC g_streamTokenDropped

STREAM_TOKEN_SLOT_COUNT equ 64
STREAM_TOKEN_SLOT_MASK  equ 63
STREAM_TOKEN_TEXT_MAX   equ 240
STREAM_TOKEN_SLOT_SIZE  equ 288

.data
align 8
g_streamDispatchFn   dq 0
g_streamDispatchCtx  dq 0
g_streamBeaconSlot   dd 9
g_streamEnabled      dd 1
g_streamTokenSeq     dq 0
g_streamTokenDropped dq 0
g_streamWriteIdx     dd 0
g_streamPad          dd 0

align 16
g_streamRing         db (STREAM_TOKEN_SLOT_COUNT * STREAM_TOKEN_SLOT_SIZE) dup(0)

.code

StreamToken_SetDispatch PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     g_streamDispatchFn, rcx
    mov     g_streamDispatchCtx, rdx
    test    r8d, r8d
    jz      @std_keep_slot
    mov     g_streamBeaconSlot, r8d
@std_keep_slot:
    mov     g_streamEnabled, 1
    mov     eax, 1

    add     rsp, 28h
    ret
StreamToken_SetDispatch ENDP

StreamToken_ClearDispatch PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     g_streamDispatchFn, 0
    mov     g_streamDispatchCtx, 0
    mov     g_streamEnabled, 0
    mov     eax, 1

    add     rsp, 28h
    ret
StreamToken_ClearDispatch ENDP

; RCX = pTokenText, EDX = tokenLen, R8D = tokenId, R9 = flags
; Returns: EAX = copied token length, -1 on failure
StreamToken_Callback PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
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
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    cmp     g_streamEnabled, 0
    je      @st_fail
    test    rcx, rcx
    jz      @st_fail
    test    edx, edx
    jle     @st_fail

    mov     r12, rcx                    ; pTokenText
    mov     r13d, edx                   ; tokenLen
    cmp     r13d, STREAM_TOKEN_TEXT_MAX - 1
    jle     @st_len_ok
    mov     r13d, STREAM_TOKEN_TEXT_MAX - 1
@st_len_ok:

    mov     eax, g_streamWriteIdx
    inc     dword ptr g_streamWriteIdx
    and     eax, STREAM_TOKEN_SLOT_MASK
    imul    rax, STREAM_TOKEN_SLOT_SIZE
    lea     rbx, g_streamRing
    add     rbx, rax                    ; rbx = slot ptr

    mov     dword ptr [rbx + 0], r8d    ; tokenId
    mov     dword ptr [rbx + 4], r13d   ; tokenLen

    mov     rax, 1
    lock xadd qword ptr g_streamTokenSeq, rax
    inc     rax
    mov     qword ptr [rbx + 8], rax    ; sequence

    call    GetTickCount64
    mov     qword ptr [rbx + 16], rax   ; timestamp
    mov     qword ptr [rbx + 24], r9    ; flags
    mov     rax, g_streamDispatchCtx
    mov     qword ptr [rbx + 32], rax   ; context

    lea     rdi, [rbx + 40]             ; text payload
    mov     rsi, r12
    mov     ecx, r13d
    rep     movsb
    mov     byte ptr [rdi], 0

    ; BeaconSend(slot, pData, payloadLen)
    mov     ecx, g_streamBeaconSlot
    mov     rdx, rbx
    mov     r8d, STREAM_TOKEN_SLOT_SIZE
    call    BeaconSend

    ; Optional direct callback into C++ sink
    mov     rax, g_streamDispatchFn
    test    rax, rax
    jz      @st_ret_ok

    lea     rcx, [rbx + 40]             ; token text
    mov     edx, dword ptr [rbx + 4]    ; token len
    mov     r8d, dword ptr [rbx + 0]    ; token id
    mov     r9,  qword ptr [rbx + 32]   ; callback context
    call    rax

@st_ret_ok:
    mov     eax, r13d
    jmp     @st_ret

@st_fail:
    lock inc qword ptr g_streamTokenDropped
    mov     eax, -1

@st_ret:
    add     rsp, 20h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
StreamToken_Callback ENDP

; RCX = pSeqOut (qword*), RDX = pDroppedOut (qword*)
; Returns EAX = 1
StreamToken_GetStats PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    test    rcx, rcx
    jz      @sgs_skip_seq
    mov     rax, g_streamTokenSeq
    mov     [rcx], rax
@sgs_skip_seq:

    test    rdx, rdx
    jz      @sgs_done
    mov     rax, g_streamTokenDropped
    mov     [rdx], rax

@sgs_done:
    mov     eax, 1
    add     rsp, 28h
    ret
StreamToken_GetStats ENDP

END
