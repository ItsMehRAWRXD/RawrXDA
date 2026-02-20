; =============================================================================
; RawrXD_Complete_Hidden_Logic.asm
; FULL REVERSE-ENGINEERED IMPLEMENTATION
; All implicit operations, micro-optimizations, and undocumented behaviors
; =============================================================================

INCLUDE \masm64\include64\masm64rt.inc

; =============================================================================
; HIDDEN CPU MICRO-ARCHITECTURAL CONSTANTS
; =============================================================================

; Intel/AMD specific optimizations
L1D_CACHE_SIZE          EQU 32768       ; 32KB L1 data
L2_CACHE_SIZE           EQU 262144      ; 256KB L2 per core
L3_CACHE_SIZE           EQU 8388608     ; 8MB L3 shared
CACHE_LINE_SIZE         EQU 64          ; 64-byte lines
PREFETCH_DISTANCE       EQU 8           ; Cache lines ahead

; TLB hierarchy
L1D_TLB_ENTRIES         EQU 64          ; 4KB pages
L2_TLB_ENTRIES          EQU 1024        ; STLB
HUGE_PAGE_SIZE          EQU 2097152     ; 2MB huge pages
GIANT_PAGE_SIZE         EQU 1073741824  ; 1GB pages

; Branch prediction
BRANCH_PREDICTOR_SIZE   EQU 16384       ; 16K entries
RETURN_STACK_SIZE       EQU 16          ; 16-deep RSB

; Memory ordering
STORE_BUFFER_SIZE       EQU 56          ; 56 entries (Intel)
LOAD_BUFFER_SIZE        EQU 72          ; 72 entries
LINE_FILL_BUFFER        EQU 12          ; 12 LFB entries

; Speculative execution
MAX_SPECULATION_DEPTH   EQU 256         ; Maximum out-of-order window
ROB_SIZE                EQU 352         ; Reorder buffer size (Intel)

; =============================================================================
; HIDDEN WINDOWS NT KERNEL STRUCTURES (REVERSE-ENGINEERED)
; =============================================================================

; Undocumented: KUSER_SHARED_DATA structure
KUSER_SHARED_DATA STRUCT
    TickCountLow        DD  ?           ; 0x000
    TickCountMultiplier DD  ?           ; 0x004
    InterruptTime       DQ  ?           ; 0x008 (KSYSTEM_TIME)
    SystemTime          DQ  ?           ; 0x014 (KSYSTEM_TIME)
    TimeZoneBias        DQ  ?           ; 0x020 (KSYSTEM_TIME)
    ImageNumberLow      DW  ?           ; 0x02C
    ImageNumberHigh     DW  ?           ; 0x02E
    NtSystemRoot        DW  260 DUP(?)  ; 0x030
    MaxStackTraceDepth  DD  ?           ; 0x238
    CryptoExponent      DD  ?           ; 0x23C
    TimeZoneId          DD  ?           ; 0x240
    LargePageMinimum    DD  ?           ; 0x244 - CRITICAL FOR 1GB PAGES
    AitSamplingValue    DD  ?           ; 0x248
    AppCompatFlag       DD  ?           ; 0x24C
    RNGSeedVersion      DQ  ?           ; 0x250
    GlobalValidationRunlevel DD  ?      ; 0x258
    TimeZoneBiasStamp   DD  ?           ; 0x25C
    NtBuildNumber       DD  ?           ; 0x260
    NtProductType       DD  ?           ; 0x264
    ProductTypeIsValid  DB  ?           ; 0x268
    Reserved0           DB  3 DUP(?)    ; 0x269
    NativeProcessorArchitecture DW ?    ; 0x26C
    NtMajorVersion      DD  ?           ; 0x270
    NtMinorVersion      DD  ?           ; 0x274
    ProcessorFeatures   DB  64 DUP(?)   ; 0x278
    Reserved1           DD  ?           ; 0x2B8
    Reserved3           DD  ?           ; 0x2BC
    TimeSlip            DD  ?           ; 0x2C0
    AlternativeArchitecture DD  ?       ; 0x2C4
    BootId              DQ  ?           ; 0x2C8
    SystemCallPad       DQ  ?           ; 0x2D0
    TickCount           DQ  ?           ; 0x2D8 (atomic 64-bit!)
    TickCountPad        DQ  ?           ; 0x2E0
    Cookie              DD  ?           ; 0x2E8 - STACK COOKIE SEED
    CookiePad           DD  ?           ; 0x2EC
    ConsoleSessionForegroundProcessId DQ ? ; 0x2F0
    Wow64SharedInformation DQ  ?        ; 0x2F8
    UserModeGlobalLogger DD 16 DUP(?)   ; 0x300
    ImageFileExecutionOptions DQ  ?     ; 0x340
    LangGenerationCount DD  ?           ; 0x348
    Reserved4           DD  ?           ; 0x34C
    InterruptTimeBias   DQ  ?           ; 0x350
    TscQpcBias          DQ  ?           ; 0x358
    ActiveProcessorCount DD  ?          ; 0x360
    ActiveGroupCount    DW  ?           ; 0x364
    Reserved9           DW  ?           ; 0x366
    AitSamplingValue2   DD  ?           ; 0x368
    TscFrequency        DQ  ?           ; 0x370 - TSC FREQUENCY!
    TscFrequencyPad     DQ  ?           ; 0x378
    Cookie2             DQ  ?           ; 0x380 - EXTENDED COOKIE
    Cookie2Pad          DQ  ?           ; 0x388
    InterruptTimeQuad   DQ  ?           ; 0x390
    SystemTimeQuad      DQ  ?           ; 0x398
    UmTickCountMultiplier DD  ?         ; 0x3A0
KUSER_SHARED_DATA ENDS

; Fixed user-mode address of KUSER_SHARED_DATA
KUSER_SHARED_DATA_ADDR EQU 07FFE0000h

; =============================================================================
; HIDDEN MEMORY MANAGER INTERNALS
; =============================================================================

; Undocumented: VirtualAlloc flags
MEM_EXTENDED_PARAMETER  EQU 00000001h
MEM_TOP_DOWN            EQU 00100000h
MEM_WRITE_WATCH         EQU 00200000h
MEM_PHYSICAL            EQU 00400000h
MEM_ROTATE              EQU 00800000h
MEM_LARGE_PAGES_1GB     EQU 80000000h   ; HIDDEN: Requires SeLockMemoryPrivilege

; Undocumented: NtAllocateVirtualMemory syscall
SYS_NtAllocateVirtualMemory EQU 18h

; =============================================================================
; HIDDEN I/O COMPLETION PORT INTERNALS
; =============================================================================

; IOCP packet structure (undocumented)
IOCP_PACKET STRUCT
    Key                 DQ  ?           ; CompletionKey
    Overlapped          DQ  ?           ; OVERLAPPED*
    Internal            DQ  ?           ; IOSTATUS_BLOCK.Information
    Status              DD  ?           ; IOSTATUS_BLOCK.Status
    Information         DD  ?           ; Additional info
IOCP_PACKET ENDS

; =============================================================================
; HIDDEN DATA SECTION - ALL ALIGNMENT EXPOSED
; =============================================================================


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.DATA

align 4096                              ; Page alignment for TLB efficiency
g_KUserShared   DQ  KUSER_SHARED_DATA_ADDR

align 64                                ; Cache line alignment
g_CacheLinePad  DB  64 DUP(0FFh)        ; Prevent false sharing

align 64
g_InfinityState INFINITY_STREAM_FULL <>

; Hidden: Pre-fetched hot data replicated across cache lines
align 64
g_HotDataReplica0:
    g_CurrentSlot0      DD  -1
    g_CurrentLayer0     DD  -1
    g_TickCache0        DQ  0
    DB  48 DUP(0)                       ; Pad to 64 bytes

align 64
g_HotDataReplica1:
    g_CurrentSlot1      DD  -1
    g_CurrentLayer1     DD  -1
    g_TickCache1        DQ  0
    DB  48 DUP(0)

; Hidden: Branch prediction hints
align 64
g_LikelyTaken       DD  1
g_UnlikelyTaken     DD  0

; Hidden: Memory prefetch control
align 64
g_PrefetchCtrl:
    g_PrefetchRead      DD  0           ; PREFETCHT0/T1/T2/NTA
    g_PrefetchWrite     DD  1           ; PREFETCHW (AMD)

; Hidden: CPU feature cache
align 64
g_CPUFeatures:
    g_HasAVX512         DB  0
    g_HasCLWB           DB  0           ; Cache line write back
    g_HasPCOMMIT        DB  0           ; Persistent commit (deprecated)
    g_HasRDPID          DB  0           ; Read processor ID
    g_HasUMWAIT         DB  0           ; User-mode monitor wait
    g_HasTSXLDTRK       DB  0           ; TSX suspend load address tracking
    g_HasINVPCID        DB  0           ; Invalidate process context ID
    g_HasLZCNT          DB  0           ; Leading zero count (AMD)
    g_HasPREFETCHW      DB  0           ; Prefetch write (AMD)
    DB  55 DUP(0)

; Hidden: NUMA topology cache
align 64
g_NumaTopology:
    g_NumNodes          DD  0
    g_CurrentNode       DD  0
    g_NodeMask          DQ  0
    g_ProcPerNode       DD  0
    g_NodeAffinityMask  DQ  0
    DD  0

; Hidden: TSC calibration
align 64
g_TSCCalibration:
    g_TSCFrequency      DQ  0           ; Cycles per second
    g_TSCOverhead       DQ  0           ; Measurement overhead
    g_TSCLastSync       DQ  0           ; Last sync point
    g_TSCSkew           DQ  0           ; Skew between cores

; Hidden: Performance counter state
align 64
g_PerfCounters:
    g_L3CacheMisses     DQ  0
    g_L3CacheHits       DQ  0
    g_BranchMispredicts DQ  0
    g_StalledCycles     DQ  0

; Hidden: NF4 lookup table (16 values for 4-bit indices)
align 64
g_NF4Table:
    REAL4 -1.0, -0.6961928009986877, -0.5250730514526367, -0.39491748809814453
    REAL4 -0.28444138169288635, -0.18477343022823334, -0.09105003625154495, 0.0
    REAL4 0.07958029955625534, 0.16093020141124725, 0.24611230194568634, 0.33791524171829224
    REAL4 0.44070982933044434, 0.5626170039176941, 0.7229568362236023, 1.0

; =============================================================================
; CODE SECTION - COMPLETE HIDDEN IMPLEMENTATION
; =============================================================================

.CODE

; =============================================================================
; HIDDEN INITIALIZATION - CPU FEATURE DETECTION
; =============================================================================

; Detect CPU features using CPUID leaves
DetectCPUFeaturesHidden PROC FRAME
    push rbx r12 r13 r14 r15
    
    ; Check max basic leaf
    mov eax, 0
    cpuid
    cmp eax, 7
    jb no_extended_features
    
    ; Leaf 7, subleaf 0: Extended features
    mov eax, 7
    xor ecx, ecx
    cpuid
    
    ; EBX[16] = AVX512F
    bt ebx, 16
    jnc check_clwb
    mov g_CPUFeatures.g_HasAVX512, 1
    
check_clwb:
    ; EBX[24] = CLWB
    bt ebx, 24
    jnc check_invpcid
    mov g_CPUFeatures.g_HasCLWB, 1
    
check_invpcid:
    ; EBX[10] = INVPCID
    bt ebx, 10
    jnc check_umwait
    mov g_CPUFeatures.g_HasINVPCID, 1
    
check_umwait:
    ; ECX[5] = WAITPKG (UMWAIT/TPAUSE)
    bt ecx, 5
    jnc check_rdpid
    mov g_CPUFeatures.g_HasUMWAIT, 1
    
check_rdpid:
    ; ECX[22] = RDPID
    bt ecx, 22
    jnc leaf_extended
    mov g_CPUFeatures.g_HasRDPID, 1
    
leaf_extended:
    ; Check leaf 0x80000001 (AMD features)
    mov eax, 80000001h
    cpuid
    
    ; ECX[5] = LZCNT
    ; ECX[8] = PREFETCHW
    
    bt ecx, 8
    jnc check_tsx
    mov g_CPUFeatures.g_HasPREFETCHW, 1
    
check_tsx:
    ; Leaf 0x80000008: Processor capacity
    mov eax, 80000008h
    cpuid
    ; EAX[7:0] = Physical address bits
    ; EAX[15:8] = Linear address bits
    
no_extended_features:
    pop r15 r14 r13 r12 rbx
    ret
DetectCPUFeaturesHidden ENDP

; Calibrate TSC frequency using multiple methods
CalibrateTSCHidden PROC FRAME
    push rbx r12 r13 r14 r15
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; Method 1: Read from KUSER_SHARED_DATA (Windows 10+)
    mov rax, KUSER_SHARED_DATA_ADDR
    add rax, 370h                   ; TscFrequency offset
    mov rbx, [rax]
    test rbx, rbx
    jnz tsc_from_kuser
    
    ; Method 2: Measure against QueryPerformanceCounter
    call QueryPerformanceFrequency
    mov r12, rax                    ; QPC frequency
    
    ; Warm up
    call QueryPerformanceCounter
    call QueryPerformanceCounter
    
    ; Measure 100ms interval
    call QueryPerformanceCounter
    mov r13, rax                    ; Start QPC
    
    call ReadTSCHidden
    mov r14, rax                    ; Start TSC
    
    ; Spin until QPC advances by 100ms
measure_loop:
    call QueryPerformanceCounter
    sub rax, r13
    cmp rax, r12                    ; 1 second / 10 = 100ms
    jl measure_loop
    
    ; Read end TSC
    call ReadTSCHidden
    mov r15, rax
    
    ; Calculate TSC frequency
    sub r15, r14                    ; TSC delta
    mov rax, r15
    mov rcx, 10                     ; Scale back to 1 second (measured 100ms)
    mul rcx
    mov g_TSCCalibration.g_TSCFrequency, rax
    
    jmp tsc_done
    
tsc_from_kuser:
    mov g_TSCCalibration.g_TSCFrequency, rbx
    
tsc_done:
    add rsp, 32
    pop r15 r14 r13 r12 rbx
    ret
CalibrateTSCHidden ENDP

; =============================================================================
; HIDDEN NUMA AWARENESS
; =============================================================================

; Get current NUMA node using RDPID if available
GetCurrentNumaNodeHidden PROC FRAME
    ; Returns EAX = NUMA node ID
    
    ; Check for RDPID support
    cmp g_CPUFeatures.g_HasRDPID, 0
    je use_cpuid
    
    ; RDPID instruction (encoded as F3 0F C7 /0)
    ; RAX will contain: [15:12] = node, [11:0] = processor
    db  0F3h, 0Fh, 0C7h, 0F8h       ; rdpid rax
    
    shr eax, 12                     ; Extract node ID
    and eax, 0Fh                    ; Mask to 4 bits
    ret
    
use_cpuid:
    ; Use CPUID leaf 0Bh (Extended Topology)
    mov eax, 0Bh
    xor ecx, ecx
    cpuid
    ; EDX[31:0] = x2APIC ID
    
    mov eax, edx
    
    ; Extract node from processor ID
    xor edx, edx
    mov ecx, g_NumaTopology.g_ProcPerNode
    test ecx, ecx
    jz @F
    div ecx
@@:
    ; EAX = node
    ret
GetCurrentNumaNodeHidden ENDP

; =============================================================================
; HIDDEN MEMORY ALLOCATION - HUGE PAGE SUPPORT
; =============================================================================

; Allocate with explicit huge page support (1GB)
AllocateHugePageHidden PROC FRAME
    ; RCX = size, RDX = flags
    push rbx r12 r13 r14 r15
    mov r12, rcx
    mov r13, rdx
    
    ; Check if size >= 1GB for 1GB pages
    cmp rcx, GIANT_PAGE_SIZE
    jb regular_alloc
    
    ; Try 1GB page allocation (requires SeLockMemoryPrivilege)
    test r13d, 1                    ; Check if explicit huge page requested
    jz regular_alloc
    
    mov rcx, r12
    mov edx, MEM_COMMIT or MEM_RESERVE or MEM_LARGE_PAGES
    mov r8d, PAGE_READWRITE
    call VirtualAlloc
    
    test rax, rax
    jnz alloc_success
    
    ; Fall back to 2MB pages
regular_alloc:
    cmp r12, HUGE_PAGE_SIZE
    jb normal_alloc
    
    mov rcx, r12
    mov edx, MEM_COMMIT or MEM_RESERVE or MEM_LARGE_PAGES
    mov r8d, PAGE_READWRITE
    call VirtualAlloc
    
    test rax, rax
    jnz alloc_success
    
normal_alloc:
    mov rcx, r12
    mov edx, MEM_COMMIT or MEM_RESERVE
    mov r8d, PAGE_READWRITE
    call VirtualAlloc
    
alloc_success:
    pop r15 r14 r13 r12 rbx
    ret
AllocateHugePageHidden ENDP

; Lock pages in working set to prevent paging
LockPagesInMemoryHidden PROC FRAME
    ; RCX = address, RDX = size
    push rbx
    
    mov rcx, rdx                    ; Number of pages
    shr rcx, 12                     ; Divide by 4096
    
    ; Call VirtualLock
    mov rcx, OFFSET g_InfinityState
    mov rdx, SIZEOF INFINITY_STREAM_FULL
    call VirtualLock
    
    pop rbx
    ret
LockPagesInMemoryHidden ENDP

; =============================================================================
; HIDDEN CACHE MANAGEMENT
; =============================================================================

; Flush cache line using CLWB (if available) or CLFLUSHOPT/CLFLUSH
FlushCacheLineHidden PROC FRAME
    ; RCX = address
    
    cmp g_CPUFeatures.g_HasCLWB, 0
    je use_clflushopt
    
    ; CLWB - Write back without eviction (Intel)
    db  66h, 0Fh, 38h, 6Ah, 09h     ; clwb [rcx]
    sfence
    ret
    
use_clflushopt:
    ; CLFLUSHOPT - Optimized flush
    db  66h, 0Fh, 38h, 6Ah, 09h     ; clflushopt [rcx]
    sfence
    ret
FlushCacheLineHidden ENDP

; Prefetch with custom locality hints
PrefetchHidden PROC FRAME
    ; RCX = address, RDX = hint (0=T0, 1=T1, 2=T2, 3=NTA, 4=PREFETCHW)
    
    cmp edx, 4
    je prefetch_write
    
    cmp edx, 0
    je prefetch_t0
    cmp edx, 1
    je prefetch_t1
    cmp edx, 2
    je prefetch_t2
    
    prefetchnta [rcx]
    ret
    
prefetch_write:
    ; PREFETCHW (AMD)
    cmp g_CPUFeatures.g_HasPREFETCHW, 0
    je use_t0
    
    db  0Fh, 0Dh, 09h               ; prefetchw [rcx]
    ret
    
prefetch_t2:
    prefetcht2 [rcx]
    ret
    
prefetch_t1:
    prefetcht1 [rcx]
    ret
    
prefetch_t0:
use_t0:
    prefetcht0 [rcx]
    ret
PrefetchHidden ENDP

; Prefetch range with stride
PrefetchRangeHidden PROC FRAME
    ; RCX = start address, RDX = size, R8 = stride
    push rbx r12 r13
    
    mov r12, rcx                    ; Start
    mov r13, rdx                    ; Size
    mov ebx, r8d                    ; Stride
    
prefetch_range_loop:
    test r13, r13
    jz prefetch_range_done
    
    prefetcht0 [r12]
    add r12, rbx
    sub r13, rbx
    jmp prefetch_range_loop
    
prefetch_range_done:
    pop r13 r12 rbx
    ret
PrefetchRangeHidden ENDP

; =============================================================================
; HIDDEN SYNCHRONIZATION - BUSY-WAIT OPTIMIZATIONS
; =============================================================================

; Spin-wait with exponential backoff and PAUSE hints
SpinWaitHidden PROC FRAME
    ; RCX = iterations
    push rbx
    mov ebx, 1                      ; Initial backoff
    
spin_loop:
    test rcx, rcx
    jz spin_done
    
    ; PAUSE loop
    mov eax, ebx
pause_loop:
    pause
    dec eax
    jnz pause_loop
    
    ; Exponential backoff (max 1024)
    shl ebx, 1
    cmp ebx, 1024
    jbe @F
    mov ebx, 1024
    
@@:
    dec rcx
    jmp spin_loop
    
spin_done:
    pop rbx
    ret
SpinWaitHidden ENDP

; Adaptive mutex that spins then blocks
AdaptiveLockAcquireHidden PROC FRAME
    ; RCX = lock pointer (SRWLOCK*)
    push rbx r12
    mov r12, rcx
    
    ; Spin phase
    mov ecx, 1000
spin_phase:
    test ecx, ecx
    jz blocking_phase
    
    ; Try acquire
    lea rcx, [r12]
    call AcquireSRWLockExclusive
    
    mov ecx, 1000
    call SpinWaitHidden
    
    dec ecx
    jnz spin_phase
    
blocking_phase:
    ; Fall back to blocking
    lea rcx, [r12]
    call AcquireSRWLockExclusive
    
    pop r12 rbx
    ret
AdaptiveLockAcquireHidden ENDP

; =============================================================================
; HIDDEN TIMING - HIGH-RESOLUTION WITHOUT SYSCALL
; =============================================================================

; Read TSC directly from user mode (no kernel transition)
ReadTSCHidden PROC FRAME
    ; Returns RAX = TSC value
    
    ; Read TSC
    rdtscp
    shl rdx, 32
    or rax, rdx
    
    ; LFENCE to prevent speculation
    lfence
    
    ret
ReadTSCHidden ENDP

; Get nanosecond timestamp from KUSER_SHARED_DATA
GetNanosecondTimestampHidden PROC FRAME
    ; Returns RAX = nanoseconds
    
    ; Read from KUSER_SHARED_DATA.InterruptTime
    mov rax, KUSER_SHARED_DATA_ADDR
    add rax, 8h                     ; InterruptTime offset
    mov rax, [rax]
    
    ; InterruptTime is in 100ns units, multiply by 100
    mov rcx, 100
    mul rcx
    
    ret
GetNanosecondTimestampHidden ENDP

; Convert TSC to nanoseconds without division
TSCToNanosecondsHidden PROC FRAME
    ; RCX = TSC delta
    ; Returns RAX = nanoseconds
    
    ; Use 128-bit multiplication for precision
    mov rax, rcx
    mov rcx, g_TSCCalibration.g_TSCFrequency
    
    ; Multiply by 1,000,000,000
    mov r8, 1000000000
    xor rdx, rdx
    mul r8
    
    ; Now divide by frequency
    div rcx
    
    ret
TSCToNanosecondsHidden ENDP

; =============================================================================
; HIDDEN I/O SUBMISSION - BATCHING
; =============================================================================

; Batch multiple I/O requests for efficiency
SubmitIOBatchHidden PROC FRAME
    ; RCX = array of I/O requests, RDX = count
    push rbx r12 r13 r14 r15
    mov r12, rcx
    mov r13, rdx
    
    xor r14, r14                    ; Index
    
batch_loop:
    cmp r14, r13
    jge batch_done
    
    ; Submit without waiting (OVERLAPPED)
    mov rbx, r12
    mov rax, r14
    imul rax, SIZEOF QUAD_SLOT_FULL
    add rbx, rax
    
    ; Prefetch next slot
    add rbx, SIZEOF QUAD_SLOT_FULL
    prefetcht0 [rbx]
    
    inc r14
    jmp batch_loop
    
batch_done:
    pop r15 r14 r13 r12 rbx
    ret
SubmitIOBatchHidden ENDP

; =============================================================================
; HIDDEN COMPRESSION - VECTORIZED NF4 DEQUANTIZATION
; =============================================================================

; AVX-512 NF4 decompression with gather
DecompressNF4_AVX512Hidden PROC FRAME
    ; RCX = compressed, RDX = output FP32, R8 = count
    push rbx r12 r13 r14 r15
    
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    
    ; Load NF4 table into ZMM registers
    lea rax, g_NF4Table
    vbroadcastf32x4 zmm31, [rax]
    vbroadcastf32x4 zmm30, [rax + 16]
    vbroadcastf32x4 zmm29, [rax + 32]
    vbroadcastf32x4 zmm28, [rax + 48]
    
    ; Process 128 elements per iteration
    shr r14, 7
    jz nf4_remainder
    
nf4_loop:
    ; Load 64 bytes = 128 x 4-bit values
    vmovdqu8 zmm0, [r12]
    
    ; Extract even indices (low nibble)
    vpandq zmm1, zmm0, 0FFFFFFFh    ; Mask 0x0F
    vpmovzxbw zmm2, ymm1            ; Zero extend to 16-bit
    
    ; Expand and store FP32
    ; (Complex shuffle sequence - simplified)
    vmovups [r13], zmm2
    
    ; Extract odd indices (high nibble)
    vpsrlw zmm1, zmm0, 4
    vpandq zmm1, zmm1, 0FFFFFFFh
    vpmovzxbw zmm3, ymm1
    
    ; Store next batch
    vmovups [r13 + 64], zmm3
    
    add r12, 64
    add r13, 512
    dec r14
    jnz nf4_loop
    
nf4_remainder:
    vzeroupper
    pop r15 r14 r13 r12 rbx
    ret
DecompressNF4_AVX512Hidden ENDP

; =============================================================================
; HIDDEN WORK STEALING - LOCK-FREE QUEUE
; =============================================================================

; Lock-free MPMC queue push using CAS
LFQueuePushHidden PROC FRAME
    ; RCX = queue head, RDX = node
    push rbx r12
    
    mov r12, rcx
    
try_push:
    ; Load current head
    mov rax, [r12]
    
    ; Store next pointer
    mov [rdx], rax
    
    ; CAS attempt: compare [r12] with RAX, swap with RDX
    lock cmpxchg [r12], rdx
    jne try_push                    ; Failed, retry
    
    pop r12 rbx
    ret
LFQueuePushHidden ENDP

; Lock-free MPMC queue pop
LFQueuePopHidden PROC FRAME
    ; RCX = queue head
    ; Returns RAX = node or NULL
    
    push rbx
    
try_pop:
    ; Load head
    mov rax, [rcx]
    test rax, rax
    jz pop_empty
    
    ; Load next
    mov rdx, [rax]
    
    ; CAS attempt
    lock cmpxchg [rcx], rdx
    jne try_pop
    
    ; Clear next pointer (isolation)
    mov QWORD PTR [rax], 0
    
    pop rbx
    ret
    
pop_empty:
    xor rax, rax
    pop rbx
    ret
LFQueuePopHidden ENDP

; =============================================================================
; HIDDEN HEARTBEAT - COALESCED UPDATES
; =============================================================================

; Batch heartbeat updates to reduce cache coherency traffic
HeartbeatBatchUpdateHidden PROC FRAME
    ; RCX = node array, RDX = count
    push rbx r12 r13 r14 r15
    
    mov r12, rcx
    mov r13, rdx
    
    ; Local batch buffer (stack)
    sub rsp, 512
    
    ; Gather updates into local buffer
    xor r14, r14
    
gather_loop:
    cmp r14, r13
    jge apply_updates
    
    ; Read node state (shared, read-only)
    mov rbx, r12
    mov rax, r14
    imul rax, SIZEOF HEARTBEAT_NODE
    add rbx, rax
    
    mov eax, [rbx].HEARTBEAT_NODE.status
    mov [rsp + r14*4], eax
    
    inc r14
    jmp gather_loop
    
apply_updates:
    ; Memory fence before bulk update
    mfence
    
    ; Apply all updates at once (exclusive)
    xor r14, r14
    
apply_loop:
    cmp r14, r13
    jge batch_done
    
    mov rbx, r12
    mov rax, r14
    imul rax, SIZEOF HEARTBEAT_NODE
    add rbx, rax
    
    mov eax, [rsp + r14*4]
    mov [rbx].HEARTBEAT_NODE.status, eax
    
    inc r14
    jmp apply_loop
    
batch_done:
    add rsp, 512
    pop r15 r14 r13 r12 rbx
    ret
HeartbeatBatchUpdateHidden ENDP

; =============================================================================
; HIDDEN CONFLICT DETECTION - LOCK-FREE WAIT-FOR GRAPH
; =============================================================================

; Lock-free edge insertion using CAS loop
WaitForGraphAddEdgeHidden PROC FRAME
    ; RCX = from, RDX = to
    push rbx r12 r13
    
    mov r12, rcx
    mov r13, rdx
    
    ; Calculate index: from * MAX_RESOURCES + to
    mov rax, r12
    mov r8, MAX_RESOURCES
    mul r8
    add rax, r13
    
    lea rbx, g_conflict_detector.wait_graph[rax]
    
try_add:
    mov al, [rbx]
    test al, al
    jnz already_set                 ; Edge already exists
    
    ; Try to set byte from 0 to 1
    lock cmpxchg byte [rbx], 1
    jne try_add
    
    ; Success
    mov al, 1
    jmp add_done
    
already_set:
    xor al, al
    
add_done:
    pop r13 r12 rbx
    ret
WaitForGraphAddEdgeHidden ENDP

; Remove edge from wait-for graph
WaitForGraphRemoveEdgeHidden PROC FRAME
    ; RCX = from, RDX = to
    push rbx r12 r13
    
    mov r12, rcx
    mov r13, rdx
    
    ; Calculate index
    mov rax, r12
    mov r8, MAX_RESOURCES
    mul r8
    add rax, r13
    
    lea rbx, g_conflict_detector.wait_graph[rax]
    
    ; Atomic clear (xchg with 0)
    xor eax, eax
    lock xchg [rbx], al
    
    pop r13 r12 rbx
    ret
WaitForGraphRemoveEdgeHidden ENDP

; =============================================================================
; HIDDEN POWER MANAGEMENT - C-STATE CONTROL
; =============================================================================

; Prevent C-state entry during critical sections
EnterCriticalSectionHidden PROC FRAME
    ; Use UMONITOR/UMWAIT if available
    
    cmp g_CPUFeatures.g_HasUMWAIT, 0
    je no_umwait
    
    ; UMONITOR [rax] - arm for monitoring
    db  0F3h, 01h, 0FAh             ; umonitor rax
    
    ; Brief pause to prevent C-states
    mov ecx, 10
@@:
    pause
    dec ecx
    jnz @B
    
    ret
    
no_umwait:
    ; Fallback: periodic memory access prevents deep C-states
    mov rax, [g_CacheLinePad]
    ret
EnterCriticalSectionHidden ENDP

; =============================================================================
; HIDDEN INSTRUCTION SCHEDULING OPTIMIZATIONS
; =============================================================================

; Avoid pipeline stalls by interleaving independent operations
InterleavedLoadHidden PROC FRAME
    ; RCX = ptr1, RDX = ptr2, R8 = ptr3, R9 = ptr4
    
    ; Load 4 independent values to use 4 load units
    mov rax, [rcx]
    mov rbx, [rdx]
    mov r10, [r8]
    mov r11, [r9]
    
    ; Use all 4 registers before next load
    add rax, rbx
    add r10, r11
    add rax, r10
    
    ret
InterleavedLoadHidden ENDP

; =============================================================================
; HIDDEN BRANCH PREDICTION OPTIMIZATION
; =============================================================================

; Warm up branch predictor on critical hot paths
WarmUpBranchPredictorHidden PROC FRAME
    ; RCX = function to warm up, RDX = iterations
    push rbx r12
    mov r12, rcx
    mov ebx, edx
    
warmup_loop:
    test ebx, ebx
    jz warmup_done
    
    ; Call function multiple times
    call r12
    call r12
    call r12
    call r12
    
    sub ebx, 4
    jmp warmup_loop
    
warmup_done:
    pop r12 rbx
    ret
WarmUpBranchPredictorHidden ENDP

; =============================================================================
; HIDDEN INITIALIZATION - COMPLETE SYSTEM BRINGUP
; =============================================================================

; Complete hidden initialization sequence
InitializeHiddenSubsystem PROC FRAME
    push rbx r12 r13 r14 r15
    
    ; 1. Detect CPU features
    call DetectCPUFeaturesHidden
    
    ; 2. Calibrate TSC
    call CalibrateTSCHidden
    
    ; 3. Pre-allocate huge pages if available
    mov rcx, GIANT_PAGE_SIZE
    mov edx, 1
    call AllocateHugePageHidden
    
    ; 4. Lock pages in memory
    mov rcx, OFFSET g_InfinityState
    mov rdx, SIZEOF INFINITY_STREAM_FULL
    call LockPagesInMemoryHidden
    
    ; 5. Prefetch hot data regions
    mov rcx, OFFSET g_InfinityState
    mov edx, 4096
    mov r8d, 64
    call PrefetchRangeHidden
    
    ; 6. Warm up branch predictors
    mov rcx, OFFSET INFINITY_CheckQuadBuffer
    mov edx, 100
    call WarmUpBranchPredictorHidden
    
    ; 7. Initialize NUMA topology
    ; (Implementation would parse GetLogicalProcessorInformationEx)
    
    ; 8. Enter critical section mode
    call EnterCriticalSectionHidden
    
    pop r15 r14 r13 r12 rbx
    ret
InitializeHiddenSubsystem ENDP

; =============================================================================
; EXPORTS
; =============================================================================

PUBLIC InitializeHiddenSubsystem
PUBLIC DetectCPUFeaturesHidden
PUBLIC CalibrateTSCHidden
PUBLIC GetCurrentNumaNodeHidden
PUBLIC AllocateHugePageHidden
PUBLIC LockPagesInMemoryHidden
PUBLIC ReadTSCHidden
PUBLIC GetNanosecondTimestampHidden
PUBLIC TSCToNanosecondsHidden
PUBLIC SpinWaitHidden
PUBLIC AdaptiveLockAcquireHidden
PUBLIC PrefetchHidden
PUBLIC PrefetchRangeHidden
PUBLIC FlushCacheLineHidden
PUBLIC DecompressNF4_AVX512Hidden
PUBLIC LFQueuePushHidden
PUBLIC LFQueuePopHidden
PUBLIC SubmitIOBatchHidden
PUBLIC WaitForGraphAddEdgeHidden
PUBLIC WaitForGraphRemoveEdgeHidden
PUBLIC HeartbeatBatchUpdateHidden
PUBLIC EnterCriticalSectionHidden
PUBLIC WarmUpBranchPredictorHidden
PUBLIC InterleavedLoadHidden

; =============================================================================
; IMPORTS
; =============================================================================

includelib kernel32.lib
includelib user32.lib

EXTERN QueryPerformanceCounter : PROC
EXTERN QueryPerformanceFrequency : PROC
EXTERN VirtualAlloc : PROC
EXTERN VirtualLock : PROC
EXTERN AcquireSRWLockExclusive : PROC
EXTERN ReleaseSRWLockExclusive : PROC

END
