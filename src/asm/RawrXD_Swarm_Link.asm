;-------------------------------------------------------------------------------
; RawrXD_Swarm_Link.asm
; MASM64 - Zero-dependency distributed inference kernel
; Foundation for 800B Swarm (Phase 14)
;-------------------------------------------------------------------------------

option casemap:none

; External Winsock2 prototypes
extern socket      : proc
extern connect     : proc
extern send        : proc
extern recv        : proc
extern closesocket : proc
extern WSASetLastError : proc
extern WSAGetLastError : proc
extern CreateIoCompletionPort : proc
extern GetQueuedCompletionStatus : proc
extern PostQueuedCompletionStatus : proc
extern ReadFile : proc
extern WSARecv : proc
extern WSASend : proc

.data
    WSA_AF_INET    equ 2
    WSA_SOCK_STREAM equ 1
    WSA_IPPROTO_TCP equ 6

.code

;-------------------------------------------------------------------------------
; RawrXD_Swarm_InitializeNode
; RCX = IPv4 Address (Network Byte Order)
; RDX = Port
; Returns: Socket Handle or INVALID_SOCKET (-1)
;-------------------------------------------------------------------------------
RawrXD_Swarm_InitializeNode proc
    push rbp
    mov rbp, rsp
    sub rsp, 48             ; Shadow space + locals

    mov r8, WSA_IPPROTO_TCP
    mov rdx, WSA_SOCK_STREAM
    mov rcx, WSA_AF_INET
    call socket             ; socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)

    cmp rax, -1
    je @exit_error

@exit_error:
    add rsp, 48
    pop rbp
    ret
RawrXD_Swarm_InitializeNode endp

;-------------------------------------------------------------------------------
; RawrXD_Swarm_SyncTensorShard
; RCX = Socket handle (Cluster node)
; RDX = Ptr to SwarmTensorShard struct header
; R8  = Ptr to Local Data Buffer (Shard source/target)
; R9  = Sync Direction (0=Send/Push, 1=Receive/Pull)
; Returns: RAX=1 (Success), 0 (Protocol Error), -1 (Network Error)
;-------------------------------------------------------------------------------
RawrXD_Swarm_SyncTensorShard proc
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 64             ; Shadow space + alignment

    mov r12, rcx            ; Node Socket
    mov r13, rdx            ; Shard Header
    mov r14, r8             ; Data Buffer
    mov r15, r9             ; Direction (0=PUSH, 1=PULL)

    ; 1. Direct Memory Handshake: Verify Header Magic 'SWRM'
    cmp dword ptr [r13], 4D525753h ; 'SWRM' (LE)
    jne @proto_err

    ; 2. Determine Sync Operation
    test r15, r15
    jnz @pull_shard

@push_shard:
    ; --- STEP A: SEND HEADER ---
    mov rcx, r12            ; Socket
    mov rdx, r13            ; Buffer (Header)
    mov r8, 56              ; sizeof(SwarmTensorShard)
    xor r9, r9              ; Flags
    sub rsp, 32
    call send
    add rsp, 32
    cmp rax, 56
    jne @net_err

    ; --- STEP B: SEND PAYLOAD ---
    mov r8, [r13 + 40]      ; SwarmTensorShard.Size
    test r8, r8
    jz @push_done

    mov rcx, r12            ; Socket
    mov rdx, r14            ; Data Buffer
    xor r9, r9
    sub rsp, 32
    call send
    add rsp, 32
    cmp rax, 0
    jle @net_err

@push_done:
    mov rax, 1
    jmp @exit

@pull_shard:
    ; --- STEP B: RECEIVE PAYLOAD ---
    ; NOTE: Header is ALREADY RECEIVED by the caller (swarm_link_test.cpp)
    ; when performing a PULL, so we skip STEP A (Header Receive) here.
    
    mov rsi, [r13 + 40]     ; SwarmTensorShard.Size
    test rsi, rsi
    jz @pull_done

    mov rdi, r14            ; Local Buffer
    xor rbx, rbx            ; Received counter

@recv_loop:
    cmp rbx, rsi
    jae @pull_done
    
    mov rcx, r12
    mov rdx, rdi
    add rdx, rbx            ; Buffer + offset
    mov r8, rsi
    sub r8, rbx             ; Remaining
    xor r9, r9
    sub rsp, 32
    call recv
    add rsp, 32
    
    cmp rax, 0
    jle @net_err            
    
    add rbx, rax
    jmp @recv_loop

@pull_done:
    mov rax, 1
    jmp @exit

@proto_err:
    xor rax, rax
    jmp @exit

@net_err:
    mov rax, -1

@exit:
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
RawrXD_Swarm_SyncTensorShard endp

;-------------------------------------------------------------------------------
; RawrXD_Swarm_SendBuffer
;-------------------------------------------------------------------------------
RawrXD_Swarm_SendBuffer proc
    sub rsp, 40
    call send
    add rsp, 40
    ret
RawrXD_Swarm_SendBuffer endp

;-------------------------------------------------------------------------------
; RawrXD_Swarm_RecvBuffer
;-------------------------------------------------------------------------------
RawrXD_Swarm_RecvBuffer proc
    sub rsp, 40
    call recv
    add rsp, 40
    ret
RawrXD_Swarm_RecvBuffer endp

;-------------------------------------------------------------------------------
; RawrXD_Swarm_CloseNode
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; RawrXD_Swarm_StreamWeightsAsync
; RCX = Model Source Handle (MMAP or File)
; RDX = Target Node Socket
; R8  = Shard Offset in Model
; R9  = Shard Size to Stream
; [RSP+40] = IOCP Handle
; Returns: RAX=1 (Queued), 0 (Error)
;-------------------------------------------------------------------------------
RawrXD_Swarm_StreamWeightsAsync proc
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 80             ; Shadow + Locals + Overlapped

    mov r12, rcx            ; File Handle
    mov r13, rdx            ; Socket
    mov r14, r8             ; Offset
    mov r15, r9             ; Size
    
    ; 1. Construct Overlapped structure on stack for async push
    ; [rsp+48] to [rsp+72] (simple overlapped stub)
    lea rdi, [rsp+48]
    xor eax, eax
    mov ecx, 32
    rep stosb               ; Zero Overlapped

    mov rax, r14            ; Offset
    mov [rsp+48+8], rax     ; InternalHigh/Offset
    
    ; 2. Execute Async Transmit
    ; Optimization: In a real Llama-70B swarm, we would use TransmitFile 
    ; but for zero-dep we'll use a high-speed loop or PostQueuedCompletionStatus
    ; to signal a worker thread to pump via SyncTensorShard kernels.
    
    mov r8, r15             ; Bytes transferred
    mov rdx, 0DEADBEEFh     ; Completion Key (Shard Sync)
    mov rcx, [rbp+48]       ; IOCP Handle (from caller stack)
    xor r9, r9              ; Overlapped
    call PostQueuedCompletionStatus
    
    test eax, eax
    jz @async_fail
    
    mov rax, 1
    jmp @async_exit

@async_fail:
    xor rax, rax

@async_exit:
    add rsp, 80
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
RawrXD_Swarm_StreamWeightsAsync endp

RawrXD_Swarm_CloseNode proc
    sub rsp, 40
    call closesocket
    add rsp, 40
    ret
RawrXD_Swarm_CloseNode endp

end