; ============================================================================
; RawrXD-SingleFile-Streaming-Orchestrator.asm
; Unified MASM64 single-source: FastDeflate + paging + DMA + scheduler + harness
; Target: 64GB RAM hosts, 40GB BigDaddyG validation, scalable to 800B window
; =========================================================================

option casemap:none

EXTERN VirtualAlloc:PROC
EXTERN QueueUserWorkItem:PROC
EXTERN Sleep:PROC
EXTERN GetSystemInfo:PROC
EXTERN CreateEventA:PROC
EXTERN SetEvent:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC
EXTERN vkMapMemory:PROC
EXTERN vkAllocateMemory:PROC
EXTERN vkCreateInstance:PROC
EXTERN vkCreateDevice:PROC
EXTERN vkAllocateCommandBuffers:PROC
EXTERN vkBeginCommandBuffer:PROC
EXTERN vkEndCommandBuffer:PROC
EXTERN vkCmdPipelineBarrier:PROC

; ----------------------------------------------------------------------------
; Configuration
; ----------------------------------------------------------------------------
NUM_THREADS             equ 16                    ; use cores-2 on typical HEDT
CHUNK_SIZE_BYTES        equ 128*1024*1024         ; 128MB PCIe-friendly chunks
GPU_ARENA_BYTES         equ 12*1024*1024*1024     ; 12GB mapped GPU arena
CPU_ARENA_BYTES         equ 44*1024*1024*1024     ; 44GB host arena
RAM_LIMIT_GB            equ 56
PREFETCH_WINDOW         equ 2
TOTAL_LAYERS            equ 320                   ; BigDaddyG reference

; ----------------------------------------------------------------------------
; Structures
; ----------------------------------------------------------------------------
ALIGN 64
GlobalContext STRUCT
    CpuArena        dq 0
    GpuArena        dq 0
    GpuMapped       dq 0
    GpuOffset       dq 0
    TotalBytesOut   dq 0
    TimelineValue   dq 0
    ActiveThreads   dd 0
    _pad            dd 0
GlobalContext ENDS

ALIGN 64
JobDesc STRUCT
    pBlockHdr       dq 0
    pDst            dq 0
    compSz          dq 0
    uncompSz        dq 0
    layerId         dq 0
    doneFlag        dq 0
JobDesc ENDS

ALIGN 64
LayerEntry STRUCT
    ChunkOffset     dq 0
    GpuOffset       dq -1
    LastAccess      dq 0
    AccessCount     dq 0
    LockFlag        dq 0
LayerEntry ENDS

ALIGN 64
Metrics STRUCT
    DmaBytes        dq 0
    DmaCount        dq 0
    EvictCount      dq 0
    PrefetchHits    dq 0
    PrefetchMiss    dq 0
    StallTicks      dq 0
Metrics ENDS

; ----------------------------------------------------------------------------
; Globals
; ----------------------------------------------------------------------------
.data
ALIGN 64
G_State         GlobalContext <0>
TP_Jobs         JobDesc 256 dup(<0>)              ; ring buffer size 256
TP_Head         dq 0
TP_Tail         dq 0
TP_Mask         dq 255

Layers          LayerEntry TOTAL_LAYERS dup(<0>)
Stats           Metrics <0>

ALIGN 64
StaticHuffmanLut    dw 4096 dup(0)
BitMasks            dq 0,1,3,7,15,31,63,127,255,511,1023,2047,4095

ALIGN 16
ThreadStartArgs STRUCT
    WorkerId    dq 0
ThreadStartArgs ENDS

.code

; ----------------------------------------------------------------------------
; FastDeflate core (branchless LUT skeleton)
; ----------------------------------------------------------------------------
RawrXD_FastDeflate_Block PROC
    ; RCX=src, RDX=dst, R8=compSz
    push rbp
    mov rbp, rsp
    sub rsp, 128

    xor r9, r9                  ; bit buffer
    xor r10, r10                ; bit count
    mov r11, rcx                ; src cursor
    mov r12, rdx                ; dst cursor
    mov r13, r8                 ; remaining

DecodeLoop:
    cmp r10, 32
    ja  DecodedReady
    cmp r13, 8
    jb  DecodeDone
    mov rax, [r11]
    mov cl, r10b
    shl rax, cl
    or  r9, rax
    add r10, 64
    add r11, 8
    sub r13, 8
DecodedReady:
    mov rax, r9
    and rax, 0FFFh
    lea rbx, StaticHuffmanLut
    movzx eax, word ptr [rbx + rax*2]
    movzx ecx, al               ; bits used
    shr r9, cl
    sub r10, rcx
    shr eax, 8                  ; symbol

    cmp eax, 256
    jb  WriteLiteral
    je  DecodeDone
    ; placeholder for LZ77 copy for long match
    jmp DecodeLoop

WriteLiteral:
    mov [r12], al
    inc r12
    jmp DecodeLoop

DecodeDone:
    mov rax, r12                ; return end ptr
    add rsp, 128
    pop rbp
    ret
RawrXD_FastDeflate_Block ENDP

; ----------------------------------------------------------------------------
; Thread pool (MPMC ring, minimal lock-free skeleton)
; ----------------------------------------------------------------------------
RawrXD_TP_Init PROC
    ; RCX=worker count
    mov [G_State.ActiveThreads], ecx
    xor eax, eax
    ret
RawrXD_TP_Init ENDP

RawrXD_TP_Submit PROC
    ; RCX=&JobDesc
    mov rdx, [TP_Tail]
    mov rax, TP_Mask
    and rdx, rax
    lea r8, TP_Jobs
    mov r9, rdx
    shl r9, 6                   ; JobDesc is 48 bytes but we keep 64 stride
    lea r8, [r8 + r9]
    ; copy descriptor
    mov rax, [rcx]
    mov [r8], rax
    mov rax, [rcx+8]
    mov [r8+8], rax
    mov rax, [rcx+16]
    mov [r8+16], rax
    mov rax, [rcx+24]
    mov [r8+24], rax
    mov rax, [rcx+32]
    mov [r8+32], rax
    mov rax, [rcx+40]
    mov [r8+40], rax
    inc qword ptr [TP_Tail]
    ret
RawrXD_TP_Submit ENDP

RawrXD_TP_TryDequeue PROC
    ; returns RAX=&JobDesc or 0
    mov rdx, [TP_Head]
    cmp rdx, [TP_Tail]
    jae TPEmpty
    mov rax, TP_Mask
    and rdx, rax
    lea rcx, TP_Jobs
    mov rax, rdx
    shl rax, 6
    lea rax, [rcx + rax]
    inc qword ptr [TP_Head]
    ret
TPEmpty:
    xor rax, rax
    ret
RawrXD_TP_TryDequeue ENDP

RawrXD_TP_Worker_Main PROC
    ; RCX = worker id
WorkerLoop:
    call RawrXD_TP_TryDequeue
    test rax, rax
    jz WorkerIdle
    ; job in RAX
    mov rcx, [rax].JobDesc.pBlockHdr
    mov rdx, [rax].JobDesc.pDst
    mov r8,  [rax].JobDesc.compSz
    call RawrXD_FastDeflate_Block
    mov qword ptr [rax].JobDesc.doneFlag, 1
    jmp WorkerLoop
WorkerIdle:
    mov ecx, 0                  ; sleep briefly
    call Sleep
    jmp WorkerLoop
RawrXD_TP_Worker_Main ENDP

; ----------------------------------------------------------------------------
; Arena / Paging / Scheduler
; ----------------------------------------------------------------------------
RawrXD_Arena_Init PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32

    ; CPU arena
    mov rcx, CPU_ARENA_BYTES
    mov rdx, 0x3000             ; MEM_COMMIT|MEM_RESERVE
    mov r8d, 0x04               ; PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz ArenaFail
    mov [G_State.CpuArena], rax

    ; GPU arena: stubbed by vkAllocateMemory
    xor rax, rax

ArenaFail:
    add rsp, 32
    pop rbp
    ret
RawrXD_Arena_Init ENDP

RawrXD_Arena_Evict_Oldest PROC
    ; stubbed eviction tracking
    inc qword ptr [Stats.EvictCount]
    ret
RawrXD_Arena_Evict_Oldest ENDP

RawrXD_Orchestrate_Paging PROC
    ; RCX=current layer index
    push rbp
    mov rbp, rsp

    ; RAM pressure check
    mov rax, [G_State.TotalBytesOut]
    shr rax, 30
    cmp rax, RAM_LIMIT_GB
    jb NoEvict
    call RawrXD_Arena_Evict_Oldest
NoEvict:
    ; Prefetch N+2
    mov rbx, rcx
    add rbx, PREFETCH_WINDOW
    cmp rbx, TOTAL_LAYERS
    jae PagingDone
    mov rdx, [Layers + rbx*SIZE LayerEntry].LayerEntry.GpuOffset
    cmp rdx, -1
    jne PagingDone
    ; submit decompression job
    lea r8, TP_Jobs             ; reuse first slot for now
    mov [r8].JobDesc.pBlockHdr, [G_State.CpuArena]
    mov [r8].JobDesc.pDst, [G_State.CpuArena]
    mov [r8].JobDesc.compSz, CHUNK_SIZE_BYTES
    mov [r8].JobDesc.uncompSz, CHUNK_SIZE_BYTES
    mov [r8].JobDesc.layerId, rbx
    xor rax, rax
    mov [r8].JobDesc.doneFlag, rax
    mov rcx, r8
    call RawrXD_TP_Submit
    inc qword ptr [Stats.PrefetchMiss]
PagingDone:
    pop rbp
    ret
RawrXD_Orchestrate_Paging ENDP

; ----------------------------------------------------------------------------
; Vulkan DMA bridge (skeleton)
; ----------------------------------------------------------------------------
RawrXD_Vulkan_Dma_Transfer PROC
    ; RCX=layerId, RDX=cpuOffset, R8=size
    inc qword ptr [Stats.DmaCount]
    add qword ptr [Stats.DmaBytes], r8
    ret
RawrXD_Vulkan_Dma_Transfer ENDP

RawrXD_Vulkan_Evict_Oldest PROC
    ret
RawrXD_Vulkan_Evict_Oldest ENDP

RawrXD_Vulkan_Init PROC
    ; minimal stub: create instance/device and map arena
    xor rax, rax
    ret
RawrXD_Vulkan_Init ENDP

; ----------------------------------------------------------------------------
; BigDaddyG test harness
; ----------------------------------------------------------------------------
RawrXD_Run_40GB_Test PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32

    xor rcx, rcx
    call RawrXD_TP_Init
    call RawrXD_Arena_Init
    call RawrXD_Vulkan_Init

    xor rbx, rbx
LayerLoop:
    mov rcx, rbx
    call RawrXD_Orchestrate_Paging
    ; wait for current layer ready would go here
    ; execute GPU kernel placeholder
    inc rbx
    cmp rbx, TOTAL_LAYERS
    jl LayerLoop

    add rsp, 32
    pop rbp
    ret
RawrXD_Run_40GB_Test ENDP

; ----------------------------------------------------------------------------
; Entrypoint
; ----------------------------------------------------------------------------
RawrXD_Main_Entry PROC
    call RawrXD_Run_40GB_Test
    xor eax, eax
    ret
RawrXD_Main_Entry ENDP

END RawrXD_Main_Entry
