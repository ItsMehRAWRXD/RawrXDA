;-------------------------------------------------------------------------------
; RawrXD_Tensor_Distributor.asm
; MASM64 - High-performance tensor sharding and distribution kernel
; Phase 13: 800B Distributed Sharding Engine (TITAN) - Swarm Protocol v1.2
;-------------------------------------------------------------------------------

option casemap:none

; External Swarm Link prototypes
extern RawrXD_Swarm_SyncTensorShard : proc

.code

;-------------------------------------------------------------------------------
; Swarm Protocol v1.2 Header Structure (56 bytes)
; offset 0:  Magic(DD)           - 0x5854494A (TITAN)
; offset 4:  Version(DW)         - 0x0102
; offset 8:  ShardID(DQ)         - Unique shard index
; offset 16: TensorID(DQ)        - Parent model/tensor identifier
; offset 24: ShardOffset(DQ)     - Offset within the main tensor
; offset 32: TotalElements(DQ)   - Total elements of the parent tensor
; offset 40: ShardSize(DQ)       - Size of this shard in bytes
; offset 48: Flags/Checksum(DQ)  - Transmission flags and data integrity
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; RawrXD_Tensor_SliceAndDistribute
; RCX = Ptr to Full Model Tensor (Source)
; RDX = Total Tensor Size (800B scale)
; R8  = Number of Nodes (Cluster Size)
; R9  = Socket Array (Array of node socket handles)
; Returns: RAX = Number of successfully distributed shards
;-------------------------------------------------------------------------------
RawrXD_Tensor_SliceAndDistribute proc
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 128            ; Shadow space + 56-byte Header + alignment

    mov r12, rcx            ; r12 = Tensor Base
    mov r13, rdx            ; r13 = Total Size
    mov r14, r8             ; r14 = Node Count
    mov r15, r9             ; r15 = Socket Array Ptr

    test r14, r14           ; Check for zero nodes
    jz @error_exit
    test r12, r12           ; Check for null buffer
    jz @error_exit

    ; 1. Calculate Shard Size (Aligned to 64 bytes for AVX-512)
    xor rdx, rdx
    mov rax, r13
    div r14                 ; rax = Base ShardSize
    add rax, 63             ; Round up to 64-byte alignment
    and rax, -64
    mov rbx, rax            ; rbx = Aligned ShardSize

    xor rsi, rsi            ; rsi = Current Node Index
    xor rdi, rdi            ; rdi = Successfully Sent Count

@distribute_loop:
    cmp rsi, r14
    jae @finish

    ; Check socket validity
    mov r11, [r15 + rsi*8]
    test r11, r11
    jz @next_node

    ; 2. Initialize Swarm Protocol v1.2 Header on Stack
    lea rdx, [rsp + 40]     ; rdx = Pointer to 56-byte header structure

    ; offset 0: Magic(DD) - TITAN (0x5854494A)
    mov dword ptr [rdx + 0], 5854494Ah
    ; offset 4: Version(DW) - 1.2
    mov word ptr [rdx + 4], 0102h
    ; offset 8: ShardID(DQ)
    mov qword ptr [rdx + 8], rsi
    ; offset 16: TensorID(DQ) - Using 0x800B-FEED for the 800B model
    mov qword ptr [rdx + 16], 0800BFEEDh
    ; offset 24: ShardOffset(DQ) = NodeIndex * ShardSize
    mov rax, rsi
    mul rbx
    mov qword ptr [rdx + 24], rax
    ; offset 32: TotalElements(DQ) - Mapping Total Size / 2 (assuming FP16)
    mov rax, r13
    shr rax, 1
    mov qword ptr [rdx + 32], rax
    ; offset 40: ShardSize(DQ)
    mov qword ptr [rdx + 40], rbx
    ; offset 48: Flags/Checksum(DQ) - 0x1 (ACTIVE)
    mov qword ptr [rdx + 48], 1

    ; 3. Call Sync Kernel
    ; RCX = Socket handle
    ; RDX = Header Ptr
    ; R8  = Data Ptr
    ; R9  = Direction (0 = Send)
    mov rcx, r11            ; Current socket handle
    mov rax, qword ptr [rdx + 24] ; Get ShardOffset
    add rax, r12            ; Data source = Base + Offset
    mov r8, rax             ; r8 = Source Data Buffer
    xor r9, r9              ; Direction: SEND
    
    call RawrXD_Swarm_SyncTensorShard
    
    test rax, rax
    js @next_node           ; Skip count if result is negative (failure)
    inc rdi

@next_node:
    inc rsi
    jmp @distribute_loop

@finish:
    mov rax, rdi            ; Return success count
    
@exit:
    add rsp, 128
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret

@error_exit:
    xor rax, rax
    jmp @exit

RawrXD_Tensor_SliceAndDistribute endp

end
