;-------------------------------------------------------------------------------
; p2p_shard_replicate.asm
; MASM64 - Phase 17: Autonomous P2P Shard Replication
; Part of the 800B Sovereign Engine Mesh
;
; CONSTRAINTS:
; 1. Pure x64 MASM (Zero-CRT).
; 2. Protocol: Swarm Protocol v1.2 (56-byte headers).
; 3. Logic: Implement node-to-node replication of tensor shards.
;-------------------------------------------------------------------------------

option casemap:none

; External Winsock2 / Win32 prototypes
extern send        : proc
extern recv        : proc
extern GetTickCount64 : proc

; Swarm Protocol Constants
SWARM_HEADER_MAGIC  equ 5854494Ah     ; 'TITAN' (0x5854494A)
SWARM_HEADER_SIZE   equ 56
REPLICATE_TIMEOUT   equ 5000           ; 5 seconds timeout for replication

; Struct: SwarmTensorShard (56 bytes)
; offset 0:  Magic (4 bytes)
; offset 4:  Version (4 bytes)
; offset 8:  ShardID (8 bytes)
; offset 16: TensorID (8 bytes)
; offset 24: ShardOffset (8 bytes)
; offset 32: TotalElements (8 bytes)
; offset 40: ShardSize (8 bytes) - Bytes of actual weight data
; offset 48: Flags/Checksum (8 bytes)

.code

;-------------------------------------------------------------------------------
; RawrXD_P2P_ReplicateShard
; This procedure facilitates direct transfer of a tensor shard from a source
; node to a target node.
;
; RCX = Source Node Socket (The node that HAS the shard)
; RDX = Target Node Socket (The node that NEEDS the shard)
; R8  = Pointer to SwarmTensorShard header
; R9  = Pointer to Local Scratch Buffer (optional)
;
; Returns: RAX = 1 (Success), 0 (Partial/Error), -1 (Socket Failure)
;-------------------------------------------------------------------------------
RawrXD_P2P_ReplicateShard proc
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 80             ; Shadow space + locals (including temp header)

    mov r12, rcx            ; Source Socket
    mov r13, rdx            ; Target Socket
    mov r14, r8             ; Shard Header Ptr
    
    ; 1. Validate Header Magic
    mov eax, [r14]
    cmp eax, SWARM_HEADER_MAGIC
    jne @proto_error

    ; 2. Phase A: Trigger Source Node to SEND the shard
    mov rsi, [r14 + 40]     ; ShardSize (Total bytes to move)
    test rsi, rsi
    jz @success_exit

    ; --- STEP 1: FORWARD HEADER TO TARGET ---
    mov rcx, r13            ; Target Socket
    mov rdx, r14            ; Header Buffer
    mov r8, SWARM_HEADER_SIZE
    xor r9, r9              ; Flags
    sub rsp, 32
    call send
    add rsp, 32
    cmp rax, SWARM_HEADER_SIZE
    jne @conn_failure

    ; --- STEP 2: PUMP DATA FROM SOURCE TO TARGET ---
    lea r15, [rbp - 48]     ; Temporary 48-byte chunk buffer on stack
    xor rbx, rbx            ; Total Bytes Moved

@pump_loop:
    cmp rbx, rsi
    jae @success_exit

    ; Calculate remaining
    mov r8, rsi
    sub r8, rbx
    cmp r8, 48              ; Cap at chunk size
    jbe @do_recv
    mov r8, 48

@do_recv:
    mov rcx, r12            ; Source Socket
    mov rdx, r15            ; Local Buffer
    ; r8 is already set
    xor r9, r9              ; Flags
    sub rsp, 32
    call recv
    add rsp, 32
    
    cmp rax, 0
    jle @conn_failure       ; Source disconnected or error
    
    mov rdi, rax            ; Bytes received this chunk

    ; Forward to Target
    mov rcx, r13            ; Target Socket
    mov rdx, r15            ; Local Buffer
    mov r8, rdi             ; Bytes to send
    xor r9, r9
    sub rsp, 32
    call send
    add rsp, 32

    cmp rax, rdi
    jne @conn_failure       ; Target disconnected or partial write fail

    add rbx, rax            ; Total moved
    jmp @pump_loop

@success_exit:
    mov rax, 1
    jmp @cleanup

@proto_error:
    xor rax, rax
    jmp @cleanup

@conn_failure:
    mov rax, -1

@cleanup:
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
RawrXD_P2P_ReplicateShard endp

END
