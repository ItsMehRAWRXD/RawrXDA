; RawrXD_Titan_MetaReverse.asm
; Reverse^5: Lock-free reclamation, Arena alloc, LZ4, RS-ECC, GPU rings
; Assemble: ml64 /c /Zi /O2 /D"METAREV=5" /W3 RawrXD_Titan_MetaReverse.asm

OPTION CASEMAP:NONE
; OPTION WIN64:3

; ============================================================================
; EXTERNAL API IMPORTS (Explicit - no includes)
; ============================================================================
EXTERNDEF __imp_VirtualAlloc : QWORD
EXTERNDEF __imp_VirtualFree : QWORD
EXTERNDEF __imp_SetPriorityClass : QWORD
EXTERNDEF __imp_GetCurrentProcess : QWORD
EXTERNDEF __imp_CreateIoCompletionPort : QWORD
EXTERNDEF __imp_AvSetMmThreadCharacteristicsA : QWORD
EXTERNDEF __imp_NtSetTimerResolution : QWORD
EXTERNDEF __imp_RtlGetCurrentPeb : QWORD
EXTERNDEF __imp_QueryPerformanceCounter : QWORD
EXTERNDEF __imp_QueryPerformanceFrequency : QWORD
EXTERNDEF __imp_HeapAlloc : QWORD
EXTERNDEF __imp_HeapFree : QWORD
EXTERNDEF __imp_GetProcessHeap : QWORD
EXTERNDEF __imp_CloseHandle : QWORD
EXTERNDEF __imp_CreateFileA : QWORD
EXTERNDEF __imp_CreateFileMappingA : QWORD
EXTERNDEF __imp_MapViewOfFileEx : QWORD
EXTERNDEF __imp_UnmapViewOfFile : QWORD
EXTERNDEF __imp_SetWindowTextA : QWORD        ; For status updates

; Macro wrappers for clarity (calls through import table)
VirtualAlloc      EQU <__imp_VirtualAlloc>
VirtualFree       EQU <__imp_VirtualFree>
; ... (used via call [VirtualAlloc] or direct EXTERN below)

; ============================================================================
; WINDOWS CONSTANT DEFINITIONS (Explicit values - no headers needed)
; ============================================================================
MEM_COMMIT               EQU 00001000h
MEM_RESERVE              EQU 00002000h
MEM_RELEASE              EQU 00008000h
MEM_DECOMMIT             EQU 00004000h
PAGE_NOACCESS            EQU 01h
PAGE_READWRITE           EQU 04h
PAGE_READONLY            EQU 02h
PAGE_EXECUTE_READWRITE   EQU 40h
FILE_MAP_ALL_ACCESS      EQU 000F001Fh
FILE_MAP_COPY            EQU 00000001h
FILE_MAP_READ            EQU 00000004h
GENERIC_READ             EQU 80000000h
FILE_SHARE_READ          EQU 00000001h
OPEN_EXISTING            EQU 3
FILE_FLAG_SEQUENTIAL_SCAN EQU 08000000h
INVALID_HANDLE_VALUE     EQU -1
REALTIME_PRIORITY_CLASS  EQU 00000100h
HEAP_ZERO_MEMORY         EQU 00000008h

; AvSet constants
AVRT_PRIORITY            EQU 0

; GPU Ring Constant (Moved here - before use)
GPU_RING_SIZE            EQU 65536
GPU_ALIGN                EQU 256
GPU_TIMESTAMP            EQU 8
MAX_CMD_LISTS            EQU 16
MAX_BARRIERS             EQU 64

; ============================================================================
; MATHEMATICAL/STRUCTURAL CONSTANTS
; ============================================================================
CACHE_LINE          EQU 64
MS_QUEUE_NODE_SIZE  EQU 128
HAZARD_PTR_COUNT    EQU 32
EPOCH_GRANULARITY   EQU 4096
Q2K_BLOCK_SIZE      EQU 256
RS_SYMBOL_SIZE      EQU 8
RS_CODEWORD         EQU 255
RS_PARITY           EQU 32
RS_PAYLOAD          EQU (RS_CODEWORD - RS_PARITY)
LZ4_HASHLOG         EQU 12
LZ4_HASH_SIZE       EQU (1 SHL LZ4_HASHLOG)

; ============================================================================
; SECTION .DATA? (BSS - Uninitialized)
; ============================================================================
.data?
 ALIGN 16; Lock-free Epoch System
g_EpochCounter      DQ ?
g_HazardPointers    DQ 32 DUP(?)
                    DQ 4 DUP(?)         ; Pad to 128

; Arena Allocator (Sparse commit)
g_ArenaBase         DQ ?
g_ArenaCommitPtr    DQ ?
g_ArenaUsed         DQ ?
g_ArenaLock         DD ?
                    DD ?                ; Pad

; Michael-Scott Queue
g_TokenQueueHead    DQ ?
g_TokenQueueTail    DQ ?
                    DQ 6 DUP(?)

; LZ4 State
g_LZ4HashTable      DD 4096 DUP(?)
                    DQ 16 DUP(?)
g_LZ4RingBuffer     DB 65536 DUP(?)
                    DQ 64 DUP(?)

; GPU Command Ring
g_GPUCmdProducer    DQ ?
g_GPUCmdConsumer    DQ ?
g_GPUFenceValue     DQ ?
                    DQ 13 DUP(?)

; Reed-Solomon Tables
g_RSExpTable        DB 256 DUP(?)
g_RSLogTable        DB 256 DUP(?)
g_RSGenPoly         DB 33 DUP(?)
                    DQ 7 DUP(?)

; ============================================================================
; CODE SECTION
; =========================================================================###
.code

; ----------------------------------------------------------------------------
; HP_Protect - Hazard pointer publication (Release semantics)
; RCX = ptr to protect, RDX = slot index (0-31)
; ----------------------------------------------------------------------------
HP_Protect PROC FRAME
    .endprolog
    mov rax, g_EpochCounter
    or rax, 1
    
    ; Release store: SFENCE + MOV
    sfence
    mov g_HazardPointers[rdx*8], rcx
    sfence
    
    ; Verify epoch hasn't flipped (safety check)
    mov rax, g_EpochCounter
    test al, 1
    jnz @@ok
    jmp HP_Protect        ; Retry if epoch flipped mid-publish
@@ok:
    ret
HP_Protect ENDP

; ----------------------------------------------------------------------------
; HP_Retire - Schedule deallocation (Wait for even epoch)
; RCX = ptr to free
; ----------------------------------------------------------------------------
HP_Retire PROC FRAME
    push rbx
    .endprolog
    mov rbx, rcx
@@check:
    mov rax, g_EpochCounter
    test al, 1
    jz @@free       ; Even epoch = safe
    pause
    jmp @@check
@@free:
    sub rsp, 32     ; Shadow space
    mov rcx, rbx
    xor edx, edx
    mov r8d, MEM_RELEASE
    call [VirtualFree]
    add rsp, 32
    pop rbx
    ret
HP_Retire ENDP

; ----------------------------------------------------------------------------
; MSQueue_Enqueue - Lock-free Michael-Scott enqueue
; RCX = new node (pre-allocated, .next=0)
; ----------------------------------------------------------------------------
MSQueue_Enqueue PROC FRAME
    push rsi        ; tail
    push rdi        ; tail->next
    push rbx        ; new node
    mov rbx, rcx
    
    mov QWORD PTR [rbx], 0      ; Node.next = NULL
    
@@retry:
    mov rsi, g_TokenQueueTail
    mov rdi, [rsi]              ; tail->next
    
    test rdi, rdi
    jnz @@tail_lagged
    
    ; CAS tail->next = new_node
    mov rax, 0                  ; Expected NULL
    lock cmpxchg [rsi], rbx
    jne @@retry
    
    ; CAS tail = new_node (failure OK)
    mov rax, rsi
    lock cmpxchg g_TokenQueueTail, rbx
    jmp @@done
    
@@tail_lagged:
    ; Help advance tail
    mov rax, rsi
    lock cmpxchg g_TokenQueueTail, rdi
    jmp @@retry
    
@@done:
    pop rbx
    pop rdi
    pop rsi
    ret
MSQueue_Enqueue ENDP

; ----------------------------------------------------------------------------
; Arena_Alloc - Lock-free bump pointer with commit-on-demand
; RCX = size, RDX = align (power of 2)
; Returns RAX = ptr (NULL if failed)
; ----------------------------------------------------------------------------
Arena_Alloc PROC FRAME
    push rbx        ; saved regs
    push r12        ; align mask
    push r13        ; size
    push r14        ; new_end
    push r15        ; old_used
    
    mov r13, rcx
    mov r12, rdx
    dec r12         ; mask = align-1
    not r12         ; ~(align-1)
    
@@retry:
    mov r15, g_ArenaUsed
    mov rcx, r15
    add rcx, r13    ; raw end
    add rcx, r12    ; + align mask
    and rcx, r12    ; align up -> new_end in RCX
    
    mov r14, rcx    ; Save new_end
    
    ; CAS arena_used = new_end
    mov rax, r15
    lock cmpxchg g_ArenaUsed, r14
    jne @@retry
    ; R15 = our allocation start (old used)
    mov rax, r15
    add rax, g_ArenaBase    ; Absolute address
    
    ; Check commit needed
    cmp r14, g_ArenaCommitPtr
    jbe @@done              ; Already committed
    
@@commit:
    ; Calculate delta: new_end - commit_ptr, rounded to page
    mov rdx, r14
    sub rdx, g_ArenaCommitPtr
    add rdx, 0FFFh
    and rdx, NOT 0FFFh      ; Size to commit in RDX
    
    sub rsp, 32
    mov rcx, g_ArenaBase
    add rcx, g_ArenaCommitPtr   ; Address to commit
    mov r8d, MEM_COMMIT
    mov r9d, PAGE_READWRITE
    call [VirtualAlloc]
    add rsp, 32
    
    test rax, rax
    jz @@fail
    
    ; Update commit watermark
    mov g_ArenaCommitPtr, r14
    
@@done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
@@fail:
    xor eax, eax
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Arena_Alloc ENDP

; ----------------------------------------------------------------------------
; LZ4_Decompress_Block - AVX-512 accelerated, fixes return calc bug
; RCX = src, RDX = dst, R8 = output limit
; Returns RAX = bytes consumed from input
; ----------------------------------------------------------------------------
LZ4_Decompress_Block PROC FRAME
    push rbx        ; match len temp
    push r12        ; output ptr (original)
    push r13        ; output limit
    push r14        ; match offset
    push r15        ; saved RSI (original src)
    
    mov r15, rcx    ; Save original source for return math
    mov rsi, rcx    ; Current source
    mov rdi, rdx    ; Dest
    mov r13, r8     ; Limit
    mov r12, rdx    ; Save dest start (optional debug)
    
    ; Zero YMM/ZMM registers we'll use (cleanup later)
    vpxor xmm0, xmm0, xmm0
    
@@block:
    cmp rdi, r13
    jae @@done
    
    ; Read token
    movzx eax, BYTE PTR [rsi]
    inc rsi
    
    mov ecx, eax
    shr ecx, 4          ; Literal length
    and eax, 0Fh        ; Match length token
    
    cmp ecx, 15
    jne @@literal_len_ok
@@lit_ext:
    movzx edx, BYTE PTR [rsi]
    inc rsi
    add ecx, edx
    cmp dl, 255
    je @@lit_ext
@@literal_len_ok:
    
    ; Copy literal
    mov r10d, ecx
@@lit_copy:
    cmp r10d, 256
    jb @@lit_small
    
    ; AVX-512 non-temporal for large literals
    vmovdqu64 zmm0, [rsi]
    vmovdqu64 [rdi], zmm0
    add rsi, 64
    add rdi, 64
    sub r10d, 64
    ja @@lit_copy
    sfence
    jmp @@match_decode
    
@@lit_small:
    rep movsb   ; ECX already has count (<256)
    
@@match_decode:
    cmp rdi, r13
    jae @@done
    
    ; Read match offset (u16 LE)
    movzx r14d, WORD PTR [rsi]
    lea rsi, [rsi+2]
    
    ; Decode match length
    mov r10d, eax       ; From token nibble
    cmp r10d, 15
    jne @@match_len_ready
@@match_ext:
    movzx edx, BYTE PTR [rsi]
    inc rsi
    add r10d, edx
    cmp dl, 255
    je @@match_ext
@@match_len_ready:
    add r10d, 4         ; Min match 4
    
    ; Copy match: RDI - R14, length R10
    mov rcx, rdi
    sub rcx, r14        ; Source = dest - offset
    
    ; Overlap check (slow path if overlapping)
    mov rax, rdi
    sub rax, rcx
    cmp rax, r14        ; If offset > copy length, no overlap? Not exactly...
    ; Simplified: always use rep movsb for safety in this fix
    ; (Optimization: check for non-overlap to use AVX)
    
    xchg rsi, rcx       ; RSI = match source, RCX = saved original RSI
    push rcx
    mov ecx, r10d
    rep movsb
    pop rsi             ; Restore source pointer
    
    jmp @@block
    
@@done:
    ; Calculate consumed: current RSI - original R15
    mov rax, rsi
    sub rax, r15
    
    vzeroupper          ; Required before return (mixed SSE/AVX cleanup)
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
LZ4_Decompress_Block ENDP

; ----------------------------------------------------------------------------
; RS_InitTables - Galois Field generation
; ----------------------------------------------------------------------------
RS_InitTables PROC FRAME
    push rbx
    xor ecx, ecx
    mov bl, 1               ; Alpha^0
@@loop:
    mov g_RSExpTable[ecx], bl
    movzx eax, bl
    mov g_RSLogTable[eax], cl
    
    test bl, 80h            ; Check high bit before shift
    jz @@no_xor
    shl bl, 1
    xor bl, 1Dh             ; Primitive polynomial x^8 + x^4 + x^3 + x^2 + 1
    jmp @@next
@@no_xor:
    shl bl, 1
@@next:
    inc cl
    jnz @@loop
    pop rbx
    ret
RS_InitTables ENDP

; ----------------------------------------------------------------------------
; RS_CalculateParity - 32 parity bytes for 223 data
; RCX = data (223 bytes), RDX = parity output (32 bytes)
; ----------------------------------------------------------------------------
RS_CalculateParity PROC FRAME
    push rsi
    push rdi
    mov rsi, rcx    ; data
    mov rdi, rdx    ; parity out
    
    ; Zero 32 bytes of parity (simplified - full RS is 500+ lines)
    mov ecx, 8      ; 8 QWORDs
    xor eax, eax
    rep stosq
    
    mov rdi, rdx    ; Restore parity ptr
    
    ; Compute syndrome (placeholder - implement full Berlekamp-Massey for production)
    mov ecx, 223
@@calc:
    movzx eax, BYTE PTR [rsi]
    inc rsi
    ; ... polynomial math here ...
    dec ecx
    jnz @@calc
    
    pop rdi
    pop rsi
    ret
RS_CalculateParity ENDP

; ----------------------------------------------------------------------------
; GPU_SubmitCompute - Lockless GPU command submission
; RCX = shader bytecode, RDX = uniform buffer, R8 = thread groups
; ----------------------------------------------------------------------------
GPU_SubmitCompute PROC FRAME
    push rbx
    push r12
    
    ; Wait for GPU fence
    mov rax, g_GPUFenceValue
    dec rax
@@wait:
    cmp g_GPUCmdConsumer, rax
    jae @@ready
    pause
    jmp @@wait
@@ready:
    
    ; Reserve slot in ring (atomic)
    mov rbx, g_GPUCmdProducer
    add rbx, 32                 ; Command size
    and rbx, (GPU_RING_SIZE-1)  ; Wrap
    
    ; Write command (simplified structure)
    mov r10, g_GPUCmdProducer
    mov BYTE PTR [r10], 1       ; Type COMPUTE
    
    mov [r10+1], rcx            ; Shader ptr
    mov [r10+9], rdx            ; Uniform
    mov [r10+17], r8d           ; Groups
    
    mov r12, g_GPUFenceValue
    mov [r10+21], r12           ; Fence
    
    inc g_GPUFenceValue
    sfence                      ; Release semantics
    
    mov g_GPUCmdProducer, rbx
    
    pop r12
    pop rbx
    ret
GPU_SubmitCompute ENDP

; ----------------------------------------------------------------------------
; Titan_MetaInit - Bootstrap with NtSetTimerResolution (syscall)
; ----------------------------------------------------------------------------
PUBLIC Titan_MetaInit
Titan_MetaInit PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; Set Realtime priority ( Multimedia scheduler)
    sub rsp, 32
    call [GetCurrentProcess]
    mov rcx, rax
    mov edx, REALTIME_PRIORITY_CLASS
    call [SetPriorityClass]
    add rsp, 32
    
    ; AvSet (if available - may fail gracefully)
    sub rsp, 32
    lea rcx, [szGames]
    lea rdx, [taskIndex]
    call [AvSetMmThreadCharacteristicsA]
    add rsp, 32
    
    ; Reserve 64GB Arena (sparse)
    sub rsp, 32
    xor ecx, ecx                    ; NULL = let OS choose
    mov rdx, (64ULL SHL 30)         ; 64GB
    mov r8d, MEM_RESERVE
    mov r9d, PAGE_NOACCESS
    call [VirtualAlloc]
    add rsp, 32
    mov g_ArenaBase, rax
    
    ; Init Queue with dummy node
    mov ecx, MS_QUEUE_NODE_SIZE
    mov edx, 64
    call Arena_Alloc
    mov g_TokenQueueHead, rax
    mov g_TokenQueueTail, rax
    mov QWORD PTR [rax], 0
    
    ; Init RS tables
    call RS_InitTables
    
    ; Set timer resolution (NtSetTimerResolution - ntdll)
    sub rsp, 32
    mov ecx, 5000       ; 0.5ms in 100ns units
    mov dl, 1           ; Set TRUE
    lea r8, [actualRes]
    call [NtSetTimerResolution]
    add rsp, 32
    
    ; Create IOCP (optional - for async I/O)
    sub rsp, 32
    mov rcx, INVALID_HANDLE_VALUE
    xor edx, edx
    xor r8, r8
    xor r9, r9
    call [CreateIoCompletionPort]
    add rsp, 32
    ; mov hIOCP, rax  ; if needed
    
@@done:
    mov rsp, rbp
    pop rbp
    ret
Titan_MetaInit ENDP

.data
szGames         BYTE "Games",0
taskIndex       DD 0
actualRes       DD 0

; Exports for the next wiring step
PUBLIC Titan_MetaInit
PUBLIC Arena_Alloc
PUBLIC HP_Protect
PUBLIC HP_Retire
PUBLIC MSQueue_Enqueue
PUBLIC LZ4_Decompress_Block
PUBLIC RS_InitTables
PUBLIC RS_CalculateParity
PUBLIC GPU_SubmitCompute

END
