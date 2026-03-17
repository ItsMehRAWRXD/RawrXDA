;  ============================================================================
;  IDE_18_COLLAB.asm  –  Agent-to-Agent communication (Google A2A protocol)
;  ============================================================================
;  Implements:
;    - AgentComm_SendA2A (Google A2A protocol frame)
;    - AgentComm_RecvA2A (non-blocking dequeue)
;    - CollabSession_Start / SendOp
;  ============================================================================

include ..\agent_core\IDE_INC.ASM

; ============================================================================
;  EXPORTS
; ============================================================================
PUBLIC AgentComm_SendA2A
PUBLIC AgentComm_RecvA2A
PUBLIC CollabSession_Start
PUBLIC CollabSession_SendOp

; ============================================================================
;  CONSTANTS
; ============================================================================
MAX_MESSAGE_LEN   equ 8192
MAX_QUEUE_SIZE    equ 256
A2A_PROTO_VERSION equ 1

; ============================================================================
;  A2A MESSAGE STRUCTURE
; ============================================================================
A2A_MESSAGE STRUCT
    version     DWORD ?
    msg_type    DWORD ?            ; 0=request, 1=response, 2=notification
    sender_id   DWORD ?
    receiver_id DWORD ?
    timestamp   QWORD ?
    payload_len DWORD ?
    payload     BYTE MAX_MESSAGE_LEN dup(?)
A2A_MESSAGE ENDS

; ============================================================================
;  MESSAGE QUEUE
; ============================================================================
.data
g_msg_queue     A2A_MESSAGE MAX_QUEUE_SIZE dup(<>)
g_queue_head    DWORD 0
g_queue_tail    DWORD 0
g_queue_count   DWORD 0
g_session_active DWORD 0

szSendA2A       db "AgentComm_SendA2A", 0
szRecvA2A       db "AgentComm_RecvA2A", 0
szCollabStart   db "CollabSession_Start", 0
szCollabSend    db "CollabSession_SendOp", 0

; ============================================================================
;  CODE
; ============================================================================
.code

; ============================================================================
;  CollabSession_Start – Initialize collaboration session
; ============================================================================
CollabSession_Start PROC
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    ; Initialize queue
    mov g_queue_head, 0
    mov g_queue_tail, 0
    mov g_queue_count, 0
    mov g_session_active, 1
    
    mov hr, 0
    
    call _leave_crit
    PERF_DELTA offset start, offset dur, offset dur
    
    LOG offset szCollabStart, hr, dword ptr [dur]
    
    mov eax, hr
    ret
CollabSession_Start ENDP

; ============================================================================
;  AgentComm_SendA2A – Send A2A protocol message
;  Input:  receiverId = target agent ID
;          msgType = message type
;          pPayload = message payload
;          payloadLen = payload length
;  Output: eax = HRESULT
; ============================================================================
AgentComm_SendA2A PROC uses ebx esi edi, receiverId:DWORD, msgType:DWORD, pPayload:DWORD, payloadLen:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    LOCAL pMsg:PTR A2A_MESSAGE
    LOCAL tail:DWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    ; Check if queue is full
    mov eax, g_queue_count
    .if eax >= MAX_QUEUE_SIZE
        mov hr, E_OUTOFMEMORY
        jmp done
    .endif
    
    ; Get tail position
    mov eax, g_queue_tail
    mov tail, eax
    
    ; Calculate message pointer
    imul eax, sizeof A2A_MESSAGE
    lea esi, g_msg_queue
    add esi, eax
    mov pMsg, esi
    
    ; Fill message structure
    mov (A2A_MESSAGE PTR [esi]).version, A2A_PROTO_VERSION
    mov eax, msgType
    mov (A2A_MESSAGE PTR [esi]).msg_type, eax
    mov (A2A_MESSAGE PTR [esi]).sender_id, 0        ; TODO: Get own agent ID
    mov eax, receiverId
    mov (A2A_MESSAGE PTR [esi]).receiver_id, eax
    
    ; Set timestamp
    lea eax, (A2A_MESSAGE PTR [esi]).timestamp
    push eax
    call _perf_now
    add esp, 4
    
    ; Copy payload
    mov eax, payloadLen
    .if eax > MAX_MESSAGE_LEN
        mov eax, MAX_MESSAGE_LEN
    .endif
    mov (A2A_MESSAGE PTR [esi]).payload_len, eax
    
    .if pPayload != 0 && payloadLen > 0
        push eax                                    ; length
        push pPayload                               ; source
        lea eax, (A2A_MESSAGE PTR [esi]).payload
        push eax                                    ; destination
        call memcpy
        add esp, 12
    .endif
    
    ; Update queue pointers
    mov eax, tail
    inc eax
    .if eax >= MAX_QUEUE_SIZE
        xor eax, eax
    .endif
    mov g_queue_tail, eax
    inc g_queue_count
    
    mov hr, 0
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur, offset dur
    
    LOG offset szSendA2A, hr, dword ptr [dur]
    
    mov eax, hr
    ret
AgentComm_SendA2A ENDP

; ============================================================================
;  AgentComm_RecvA2A – Non-blocking receive
;  Input:  pMsg = output buffer for message
;          cbMsg = buffer size
;  Output: eax = 1 if message received, 0 if empty
; ============================================================================
AgentComm_RecvA2A PROC uses ebx esi edi, pMsg:PTR A2A_MESSAGE, cbMsg:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    LOCAL head:DWORD
    LOCAL result:DWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    xor eax, eax
    mov result, eax
    
    ; Check if queue is empty
    mov eax, g_queue_count
    .if eax == 0
        mov hr, 0
        jmp done
    .endif
    
    ; Get head position
    mov eax, g_queue_head
    mov head, eax
    
    ; Calculate source pointer
    imul eax, sizeof A2A_MESSAGE
    lea esi, g_msg_queue
    add esi, eax
    
    ; Copy message to output
    .if pMsg != 0 && cbMsg >= sizeof A2A_MESSAGE
        push sizeof A2A_MESSAGE
        push esi
        push pMsg
        call memcpy
        add esp, 12
        
        mov result, 1
    .endif
    
    ; Update queue pointers
    mov eax, head
    inc eax
    .if eax >= MAX_QUEUE_SIZE
        xor eax, eax
    .endif
    mov g_queue_head, eax
    dec g_queue_count
    
    mov hr, 0
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur, offset dur
    
    LOG offset szRecvA2A, hr, dword ptr [dur]
    
    mov eax, result
    ret
AgentComm_RecvA2A ENDP

; ============================================================================
;  CollabSession_SendOp – Send CRDT operation
; ============================================================================
CollabSession_SendOp PROC uses ebx, receiverId:DWORD, pOp:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    LOCAL opLen:DWORD
    
    PERF_INIT offset start
    
    .if pOp == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    ; Get operation length
    invoke lstrlenA, pOp
    mov opLen, eax
    
    ; Send as A2A message (type = 0 for request)
    invoke AgentComm_SendA2A, receiverId, 0, pOp, opLen
    mov hr, eax
    
done:
    PERF_DELTA offset start, offset dur, offset dur
    LOG offset szCollabSend, hr, dword ptr [dur]
    
    mov eax, hr
    ret
CollabSession_SendOp ENDP

; Helper: memcpy (simple implementation)
memcpy PROC uses esi edi ecx, pDest:DWORD, pSrc:DWORD, count:DWORD
    mov edi, pDest
    mov esi, pSrc
    mov ecx, count
    rep movsb
    ret
memcpy ENDP

END
