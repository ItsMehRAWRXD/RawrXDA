;================================================================================
; PHASE1_MASTER.ASM - Foundation & Bootstrap Layer
; Core Infrastructure: Memory Management, Hardware Detection, Kernel Primitives
; This is the bedrock that Phases 2-5 build upon
;================================================================================

OPTION CASEMAP:NONE
OPTION WIN64:3

;================================================================================
; EXTERNAL IMPORTS - NT Kernel + Hardware Abstraction
;================================================================================
; NTDLL (Native API)
extern NtAllocateVirtualMemory : proc
extern NtFreeVirtualMemory : proc
extern NtProtectVirtualMemory : proc
extern NtQuerySystemInformation : proc
extern NtQueryPerformanceCounter : proc
extern RtlGetVersion : proc
extern RtlCopyMemory : proc
extern RtlZeroMemory : proc
extern RtlCompareMemory : proc

; Kernel32 (Win32 API)
extern GetProcAddress : proc
extern GetModuleHandleA : proc
extern LoadLibraryA : proc
extern VirtualAlloc : proc
extern VirtualFree : proc
extern VirtualProtect : proc
extern VirtualLock : proc
extern VirtualUnlock : proc
extern GetSystemInfo : proc
extern GetLogicalProcessorInformation : proc
extern GetPhysicallyInstalledSystemMemory : proc
extern GlobalMemoryStatusEx : proc
extern QueryPerformanceCounter : proc
extern QueryPerformanceFrequency : proc
extern GetTickCount64 : proc
extern GetCurrentProcess : proc
extern GetCurrentProcessId : proc
extern GetCurrentThreadId : proc
extern SetThreadAffinityMask : proc
extern SetThreadPriority : proc
extern GetThreadPriority : proc
extern Sleep : proc
extern SleepConditionVariableCS : proc
extern WakeConditionVariable : proc
extern WakeAllConditionVariable : proc
extern InitializeConditionVariable : proc
extern InitializeCriticalSection : proc
extern InitializeCriticalSectionAndSpinCount : proc
extern InitializeSRWLock : proc
extern AcquireSRWLockExclusive : proc
extern ReleaseSRWLockExclusive : proc
extern EnterCriticalSection : proc
extern LeaveCriticalSection : proc
extern TryEnterCriticalSection : proc
extern DeleteCriticalSection : proc
extern CreateEventA : proc
extern CreateEventW : proc
extern SetEvent : proc
extern ResetEvent : proc
extern WaitForSingleObject : proc
extern WaitForMultipleObjects : proc
extern CreateSemaphoreA : proc
extern ReleaseSemaphore : proc
extern CreateMutexA : proc
extern ReleaseMutex : proc
extern CreateThread : proc
extern TerminateThread : proc
extern SuspendThread : proc
extern ResumeThread : proc
extern GetThreadContext : proc
extern SetThreadContext : proc
extern FlushInstructionCache : proc
extern GetStdHandle : proc
extern WriteConsoleA : proc
extern IsProcessorFeaturePresent : proc

;================================================================================
; CONSTANTS - Hardware & Architecture
;================================================================================
; CPU Features (CPUID)
CPUID_ECX_SSE3          EQU 000000001h
CPUID_ECX_PCLMULQDQ     EQU 000000002h
CPUID_ECX_MONITOR       EQU 000000008h
CPUID_ECX_SSSE3         EQU 000000200h
CPUID_ECX_FMA           EQU 000001000h
CPUID_ECX_CMPXCHG16B    EQU 000002000h
CPUID_ECX_SSE41         EQU 000080000h
CPUID_ECX_SSE42         EQU 000100000h
CPUID_ECX_MOVBE         EQU 000400000h
CPUID_ECX_POPCNT        EQU 000800000h
CPUID_ECX_AES           EQU 002000000h
CPUID_ECX_XSAVE         EQU 004000000h
CPUID_ECX_OSXSAVE       EQU 008000000h
CPUID_ECX_AVX           EQU 010000000h
CPUID_ECX_F16C          EQU 020000000h
CPUID_ECX_RDRAND        EQU 040000000h

CPUID_EDX_FPU           EQU 000000001h
CPUID_EDX_VME           EQU 000000002h
CPUID_EDX_DE            EQU 000000004h
CPUID_EDX_PSE           EQU 000000008h
CPUID_EDX_TSC           EQU 000000010h
CPUID_EDX_MSR           EQU 000000020h
CPUID_EDX_PAE           EQU 000000040h
CPUID_EDX_MCE           EQU 000000080h
CPUID_EDX_CX8           EQU 000000100h
CPUID_EDX_APIC          EQU 000000200h
CPUID_EDX_SEP           EQU 000008000h
CPUID_EDX_MTRR          EQU 000010000h
CPUID_EDX_PGE           EQU 000020000h
CPUID_EDX_MCA           EQU 000040000h
CPUID_EDX_CMOV          EQU 000080000h
CPUID_EDX_PAT           EQU 000100000h
CPUID_EDX_PSE36         EQU 000200000h
CPUID_EDX_PSN           EQU 000400000h
CPUID_EDX_CLFSH         EQU 000800000h
CPUID_EDX_DS            EQU 002000000h
CPUID_EDX_ACPI          EQU 004000000h
CPUID_EDX_MMX           EQU 008000000h
CPUID_EDX_FXSR          EQU 010000000h
CPUID_EDX_SSE           EQU 020000000h
CPUID_EDX_SSE2          EQU 040000000h
CPUID_EDX_SS            EQU 080000000h

; Extended CPUID (EAX=7, ECX=0)
CPUID_EBX_FSGSBASE      EQU 000000001h
CPUID_EBX_SGX           EQU 000000004h
CPUID_EBX_BMI1          EQU 000000008h
CPUID_EBX_HLE           EQU 000000010h
CPUID_EBX_AVX2          EQU 000000020h
CPUID_EBX_SMEP          EQU 000000080h
CPUID_EBX_BMI2          EQU 000000100h
CPUID_EBX_ERMS          EQU 000000200h
CPUID_EBX_INVPCID       EQU 000000400h
CPUID_EBX_RTM           EQU 000000800h
CPUID_EBX_PQM           EQU 000001000h
CPUID_EBX_MPX           EQU 000004000h
CPUID_EBX_PQE           EQU 000010000h
CPUID_EBX_AVX512F       EQU 000100000h
CPUID_EBX_AVX512DQ      EQU 000200000h
CPUID_EBX_RDSEED        EQU 000400000h
CPUID_EBX_ADX           EQU 000800000h
CPUID_EBX_SMAP          EQU 001000000h
CPUID_EBX_AVX512IFMA    EQU 002000000h
CPUID_EBX_CLFLUSHOPT    EQU 008000000h
CPUID_EBX_CLWB          EQU 010000000h
CPUID_EBX_AVX512PF      EQU 040000000h
CPUID_EBX_AVX512ER      EQU 080000000h
CPUID_EBX_AVX512CD      EQU 100000000h
CPUID_EBX_SHA           EQU 200000000h
CPUID_EBX_AVX512BW      EQU 400000000h
CPUID_EBX_AVX512VL      EQU 800000000h

; Memory
PAGE_SIZE               EQU 1000h
LARGE_PAGE_SIZE         EQU 200000h
HUGE_PAGE_SIZE          EQU 40000000h
CACHE_LINE_SIZE         EQU 40h

; NUMA
MAX_NUMA_NODES          EQU 64
MAX_PROCESSORS          EQU 1024

; Memory allocation flags
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
MEM_LARGE_PAGES         EQU 20000000h
PAGE_READWRITE          EQU 4
PAGE_NOACCESS           EQU 1

; Thread priority
THREAD_PRIORITY_HIGHEST EQU 2

; Standard handles
STD_OUTPUT_HANDLE       EQU -11

;================================================================================
; STRUCTURES - Hardware Topology & Capability Detection
;================================================================================
CPU_CAPABILITIES STRUCT 64
    ; Vendor
    vendor_id               db 12 DUP(?)
    brand_string            db 48 DUP(?)
    
    ; Basic features
    family                  dd ?
    model                   dd ?
    stepping                dd ?
    
    ; Core topology
    physical_cores          dd ?
    logical_cores           dd ?
    threads_per_core        dd ?
    numa_nodes              dd ?
    
    ; Cache hierarchy
    l1d_cache_size          dd ?
    l1i_cache_size          dd ?
    l2_cache_size           dd ?
    l3_cache_size           dd ?
    l1d_cacheline           dd ?
    l1i_cacheline           dd ?
    l2_cacheline            dd ?
    l3_cacheline            dd ?
    
    ; Feature flags
    has_sse                 dd ?
    has_sse2                dd ?
    has_sse3                dd ?
    has_ssse3               dd ?
    has_sse41               dd ?
    has_sse42               dd ?
    has_avx                 dd ?
    has_avx2                dd ?
    has_avx512f             dd ?
    has_avx512dq            dd ?
    has_avx512bw            dd ?
    has_avx512vl            dd ?
    has_avx512cd            dd ?
    has_avx512er            dd ?
    has_avx512pf            dd ?
    has_fma                 dd ?
    has_f16c                dd ?
    has_popcnt              dd ?
    has_lzcnt               dd ?
    has_bmi1                dd ?
    has_bmi2                dd ?
    has_aes                 dd ?
    has_sha                 dd ?
    has_rdrand              dd ?
    has_rdseed              dd ?
    has_tsc                 dd ?
    has_tsc_deadline        dd ?
    has_invariant_tsc       dd ?
    
    ; Extended state
    xsave_area_size         dd ?
    max_xsave_size          dd ?
    has_xsaveopt            dd ?
    has_xsaves              dd ?
    has_xsavec              dd ?
    
    ; Performance
    tsc_frequency_hz        dq ?
    nominal_frequency_mhz   dd ?
    max_frequency_mhz       dd ?
CPU_CAPABILITIES ENDS

MEMORY_ARENA STRUCT 64
    base_address            dq ?
    current_offset          dq ?
    committed_size          dq ?
    reserved_size           dq ?
    block_size              dq ?
    flags                   dd ?
    numa_node               dd ?
    lock                    dq ?  ; SRWLOCK
MEMORY_ARENA ENDS

NUMA_NODE_INFO STRUCT 32
    node_number             dd ?
    processor_mask          dq ?
    memory_size             dq ?
    free_memory             dq ?
    arena_count             dd ?
    padding                 dd ?
NUMA_NODE_INFO ENDS

HARDWARE_TOPOLOGY STRUCT 4096
    cpu                     CPU_CAPABILITIES <>
    
    ; NUMA
    numa_node_count         dd ?
    padding1                dd ?
    numa_nodes              dq MAX_NUMA_NODES DUP(?)
    
    ; Memory
    total_physical_memory   dq ?
    available_memory        dq ?
    total_virtual_memory    dq ?
    available_virtual       dq ?
    
    ; System
    page_size               dq ?
    allocation_granularity  dq ?
    processor_count         dd ?
    active_processor_mask   dq ?
    
    ; Capabilities detected
    has_large_pages         dd ?
    has_huge_pages          dd ?
    has_numa                dd ?
    has_processor_groups    dd ?
HARDWARE_TOPOLOGY ENDS

PHASE1_CONTEXT STRUCT 8192
    ; Hardware detection results
    topology                HARDWARE_TOPOLOGY <>
    
    ; Memory management
    system_arena            MEMORY_ARENA <>
    numa_arenas             dq MAX_NUMA_NODES DUP(?)
    
    ; Thread pool primitives
    worker_thread_count     dd ?
    io_thread_count         dd ?
    padding1                dd ?
    completion_port         dq ?
    
    ; Synchronization primitives pool
    cs_pool                 dq ?
    event_pool              dq ?
    semaphore_pool          dq ?
    
    ; Performance monitoring
    perf_frequency          dq ?
    perf_counter_start      dq ?
    
    ; Initialization state
    initialized             dd ?
    init_flags              dd ?
    
    ; Error handling
    last_error_code         dd ?
    padding2                dd ?
    error_handler           dq ?
    
    ; Logging
    log_buffer              dq ?
    log_write_ptr           dq ?
    log_capacity            dq ?
    
    ; Reserved for expansion
    reserved                db 4096 DUP(?)
PHASE1_CONTEXT ENDS

THREAD_POOL_TASK STRUCT 64
    next                    dq ?
    callback                dq ?
    context                 dq ?
    priority                dd ?
    flags                   dd ?
    submit_time             dq ?
    target_numa_node        dd ?
    processor_affinity      dd ?
THREAD_POOL_TASK ENDS

THREAD_POOL_WORKER STRUCT 128
    thread_handle           dq ?
    thread_id               dd ?
    state                   dd ?
    current_task            dq ?
    task_count              dq ?
    numa_node               dd ?
    ideal_processor         dd ?
    worker_arena            MEMORY_ARENA <>
THREAD_POOL_WORKER ENDS

;================================================================================
; DATA SECTION
;================================================================================
.DATA
ALIGN 4096

; CPU vendor strings
vendor_intel            db "GenuineIntel", 0
vendor_amd              db "AuthenticAMD", 0

; Error strings
err_no_avx              db "[PHASE1] ERROR: AVX not supported", 0Dh, 0Ah, 0
err_no_xsave            db "[PHASE1] ERROR: XSAVE not supported", 0Dh, 0Ah, 0
err_memory_init         db "[PHASE1] ERROR: Memory initialization failed", 0Dh, 0Ah, 0
err_numa_unavailable    db "[PHASE1] WARNING: NUMA not available", 0Dh, 0Ah, 0

; Feature strings
str_detecting           db "[PHASE1] Detecting hardware capabilities...", 0Dh, 0Ah, 0
str_cpu_vendor          db "[PHASE1] CPU Vendor: %s", 0Dh, 0Ah, 0
str_cpu_cores           db "[PHASE1] Cores: %d physical, %d logical", 0Dh, 0Ah, 0
str_avx512              db "[PHASE1] AVX-512: F=%d DQ=%d BW=%d VL=%d", 0Dh, 0Ah, 0
str_memory              db "[PHASE1] Memory: %llu GB total, %llu GB available", 0Dh, 0Ah, 0
str_numa                db "[PHASE1] NUMA nodes: %d", 0Dh, 0Ah, 0
str_init_complete       db "[PHASE1] Initialization complete", 0Dh, 0Ah, 0

; Feature bit names for logging
str_sse                 db "SSE", 0
str_sse2                db "SSE2", 0
str_avx                 db "AVX", 0
str_avx2                db "AVX2", 0
str_avx512              db "AVX-512", 0
str_fma                 db "FMA", 0

; Global Phase1 context pointer
g_Phase1Context         dq 0
g_Phase1Initialized     dd 0

;================================================================================
; CODE SECTION
;================================================================================
.CODE
ALIGN 64

;================================================================================
; PHASE 1: INITIALIZATION
;================================================================================

;-------------------------------------------------------------------------------
; Phase1Initialize - Bootstrap the entire system
; Input:  RCX = Initialization flags
; Output: RAX = PHASE1_CONTEXT* or NULL
;-------------------------------------------------------------------------------
Phase1Initialize PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 200h
    
    mov r12d, ecx                     ; R12 = init flags
    
    ; Check if already initialized
    cmp g_Phase1Initialized, 0
    jne @phase1_already_init
    
    ; Allocate Phase1 context
    xor ecx, ecx
    mov edx, sizeof PHASE1_CONTEXT
    mov r8d, MEM_RESERVE + MEM_COMMIT
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @phase1_init_fail
    mov rbx, rax                      ; RBX = PHASE1_CONTEXT*
    
    ; Zero the structure
    mov rcx, rbx
    mov edx, sizeof PHASE1_CONTEXT
    call RtlZeroMemory
    
    mov [rbx].PHASE1_CONTEXT.init_flags, r12d
    mov g_Phase1Context, rbx
    
    ; Step 1: Detect CPU capabilities
    mov rcx, rbx
    call DetectCpuCapabilities
    test eax, eax
    jz @phase1_init_cleanup
    
    ; Step 2: Detect memory topology
    mov rcx, rbx
    call DetectMemoryTopology
    test eax, eax
    jz @phase1_init_cleanup
    
    ; Step 3: Initialize memory arenas
    mov rcx, rbx
    call InitializeMemoryArenas
    test eax, eax
    jz @phase1_init_cleanup
    
    ; Step 4: Initialize synchronization primitives
    mov rcx, rbx
    call InitializeSynchronizationPrimitives
    
    ; Step 5: Calibrate performance counters
    mov rcx, rbx
    call CalibratePerformanceCounters
    
    ; Step 6: Initialize thread pool infrastructure
    mov rcx, rbx
    call InitializeThreadPoolInfrastructure
    
    ; Mark as initialized
    mov dword ptr [rbx].PHASE1_CONTEXT.initialized, 1
    mov g_Phase1Initialized, 1
    
    ; Log completion
    mov rcx, rbx
    lea rdx, str_init_complete
    call Phase1LogMessage
    
    mov rax, rbx
    jmp @phase1_init_exit
    
@phase1_already_init:
    mov rax, g_Phase1Context
    jmp @phase1_init_exit
    
@phase1_init_cleanup:
    mov rcx, rbx
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree
    
@phase1_init_fail:
    xor eax, eax
    
@phase1_init_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
Phase1Initialize ENDP

;================================================================================
; HARDWARE DETECTION
;================================================================================

;-------------------------------------------------------------------------------
; DetectCpuCapabilities - Full CPUID enumeration
;-------------------------------------------------------------------------------
DetectCpuCapabilities PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 100h
    
    mov rbx, rcx                      ; RBX = PHASE1_CONTEXT*
    lea r12, [rbx].PHASE1_CONTEXT.topology.cpu
    
    ; Log start
    mov rcx, rbx
    lea rdx, str_detecting
    call Phase1LogMessage
    
    ; Get vendor ID (CPUID 0)
    xor eax, eax
    cpuid
    mov [r12].CPU_CAPABILITIES.vendor_id, ebx
    mov DWORD PTR [r12].CPU_CAPABILITIES.vendor_id+4, edx
    mov DWORD PTR [r12].CPU_CAPABILITIES.vendor_id+8, ecx
    
    ; Get brand string (CPUID 80000002h-80000004h)
    mov eax, 80000002h
    cpuid
    mov [r12].CPU_CAPABILITIES.brand_string, eax
    mov DWORD PTR [r12].CPU_CAPABILITIES.brand_string+4, ebx
    mov DWORD PTR [r12].CPU_CAPABILITIES.brand_string+8, ecx
    mov DWORD PTR [r12].CPU_CAPABILITIES.brand_string+12, edx
    
    mov eax, 80000003h
    cpuid
    mov [r12].CPU_CAPABILITIES.brand_string+16, eax
    mov DWORD PTR [r12].CPU_CAPABILITIES.brand_string+20, ebx
    mov DWORD PTR [r12].CPU_CAPABILITIES.brand_string+24, ecx
    mov DWORD PTR [r12].CPU_CAPABILITIES.brand_string+28, edx
    
    mov eax, 80000004h
    cpuid
    mov [r12].CPU_CAPABILITIES.brand_string+32, eax
    mov DWORD PTR [r12].CPU_CAPABILITIES.brand_string+36, ebx
    mov DWORD PTR [r12].CPU_CAPABILITIES.brand_string+40, ecx
    mov DWORD PTR [r12].CPU_CAPABILITIES.brand_string+44, edx
    
    ; Get features (CPUID 1)
    mov eax, 1
    xor ecx, ecx
    cpuid
    
    ; Extract family/model/stepping
    mov r13d, eax
    shr r13d, 8
    and r13d, 0Fh
    mov [r12].CPU_CAPABILITIES.family, r13d
    
    mov r13d, eax
    shr r13d, 4
    and r13d, 0Fh
    mov [r12].CPU_CAPABILITIES.model, r13d
    
    mov r13d, eax
    and r13d, 0Fh
    mov [r12].CPU_CAPABILITIES.stepping, r13d
    
    ; Check features in ECX
    test ecx, CPUID_ECX_SSE3
    setnz byte ptr [r12].CPU_CAPABILITIES.has_sse3
    test ecx, CPUID_ECX_SSSE3
    setnz byte ptr [r12].CPU_CAPABILITIES.has_ssse3
    test ecx, CPUID_ECX_SSE41
    setnz byte ptr [r12].CPU_CAPABILITIES.has_sse41
    test ecx, CPUID_ECX_SSE42
    setnz byte ptr [r12].CPU_CAPABILITIES.has_sse42
    test ecx, CPUID_ECX_AVX
    setnz byte ptr [r12].CPU_CAPABILITIES.has_avx
    test ecx, CPUID_ECX_FMA
    setnz byte ptr [r12].CPU_CAPABILITIES.has_fma
    test ecx, CPUID_ECX_POPCNT
    setnz byte ptr [r12].CPU_CAPABILITIES.has_popcnt
    test ecx, CPUID_ECX_AES
    setnz byte ptr [r12].CPU_CAPABILITIES.has_aes
    test ecx, CPUID_ECX_XSAVE
    setnz byte ptr [r12].CPU_CAPABILITIES.has_xsaveopt
    
    ; Check features in EDX
    test edx, CPUID_EDX_TSC
    setnz byte ptr [r12].CPU_CAPABILITIES.has_tsc
    test edx, CPUID_EDX_SSE
    setnz byte ptr [r12].CPU_CAPABILITIES.has_sse
    test edx, CPUID_EDX_SSE2
    setnz byte ptr [r12].CPU_CAPABILITIES.has_sse2
    
    ; Get extended features (CPUID 7, ECX=0)
    mov eax, 7
    xor ecx, ecx
    cpuid
    
    test ebx, CPUID_EBX_AVX2
    setnz byte ptr [r12].CPU_CAPABILITIES.has_avx2
    test ebx, CPUID_EBX_BMI1
    setnz byte ptr [r12].CPU_CAPABILITIES.has_bmi1
    test ebx, CPUID_EBX_BMI2
    setnz byte ptr [r12].CPU_CAPABILITIES.has_bmi2
    test ebx, CPUID_EBX_AVX512F
    setnz byte ptr [r12].CPU_CAPABILITIES.has_avx512f
    test ebx, CPUID_EBX_AVX512DQ
    setnz byte ptr [r12].CPU_CAPABILITIES.has_avx512dq
    test ebx, CPUID_EBX_AVX512BW
    setnz byte ptr [r12].CPU_CAPABILITIES.has_avx512bw
    test ebx, CPUID_EBX_AVX512VL
    setnz byte ptr [r12].CPU_CAPABILITIES.has_avx512vl
    test ebx, CPUID_EBX_AVX512CD
    setnz byte ptr [r12].CPU_CAPABILITIES.has_avx512cd
    test ebx, CPUID_EBX_AVX512ER
    setnz byte ptr [r12].CPU_CAPABILITIES.has_avx512er
    test ebx, CPUID_EBX_AVX512PF
    setnz byte ptr [r12].CPU_CAPABILITIES.has_avx512pf
    test ebx, CPUID_EBX_SHA
    setnz byte ptr [r12].CPU_CAPABILITIES.has_sha
    test ebx, CPUID_EBX_RDSEED
    setnz byte ptr [r12].CPU_CAPABILITIES.has_rdseed
    
    ; Get core topology (CPUID Bh)
    mov eax, 0Bh
    xor ecx, ecx
    cpuid
    
    ; EBX = logical processors at this level
    mov [r12].CPU_CAPABILITIES.logical_cores, ebx
    
    ; Calculate physical cores from logical cores
    cmp ebx, 0
    je @cpu_cores_unknown
    mov [r12].CPU_CAPABILITIES.physical_cores, ebx
    mov dword ptr [r12].CPU_CAPABILITIES.threads_per_core, 1
    jmp @cpu_cores_done
    
@cpu_cores_unknown:
    ; Fallback: assume physical = logical
    mov [r12].CPU_CAPABILITIES.physical_cores, 1
    mov dword ptr [r12].CPU_CAPABILITIES.threads_per_core, 1
    
@cpu_cores_done:
    ; Measure TSC frequency
    mov rcx, r12
    call MeasureTscFrequency
    
    ; Verify minimum requirements (AVX)
    cmp byte ptr [r12].CPU_CAPABILITIES.has_avx, 0
    je @cpu_no_avx
    
    mov eax, 1
    jmp @cpu_detect_done
    
@cpu_no_avx:
    mov rcx, rbx
    lea rdx, err_no_avx
    call Phase1LogMessage
    xor eax, eax
    
@cpu_detect_done:
    mov rsp, rbp
    RESTORE_REGS
    ret
DetectCpuCapabilities ENDP

;-------------------------------------------------------------------------------
; MeasureTscFrequency - Calibrate TSC using QueryPerformanceCounter
;-------------------------------------------------------------------------------
MeasureTscFrequency PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 40h
    
    mov rbx, rcx                      ; RBX = CPU_CAPABILITIES*
    
    ; Warm up RDTSC
    rdtsc
    rdtsc
    
    ; Get start values
    lea rcx, [rbp-8]
    call QueryPerformanceCounter
    mov r12, [rbp-8]                  ; R12 = QPC start
    
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov r13, rax                      ; R13 = TSC start
    
    ; Spin for ~10ms
    mov ecx, 10
    call Sleep
    
    ; Get end values
    lea rcx, [rbp-8]
    call QueryPerformanceCounter
    mov r14, [rbp-8]                  ; R14 = QPC end
    
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov r15, rax                      ; R15 = TSC end
    
    ; Calculate frequency
    sub r15, r13                      ; TSC delta
    sub r14, r12                      ; QPC delta
    
    ; Get QPF
    lea rcx, [rbp-10h]
    call QueryPerformanceFrequency
    mov rax, [rbp-10h]                ; RAX = QPF
    
    ; Avoid division by zero
    test r14, r14
    jz @tsc_skip_calc
    
    ; TSC_freq = (TSC_delta * QPF) / QPC_delta
    mov rcx, r15
    mul rcx                           ; RDX:RAX = TSC_delta * QPF
    div r14                           ; RAX = TSC frequency
    
    mov [rbx].CPU_CAPABILITIES.tsc_frequency_hz, rax
    
    ; Calculate nominal frequency in MHz
    mov rcx, 1000000
    xor edx, edx
    div rcx
    mov [rbx].CPU_CAPABILITIES.nominal_frequency_mhz, eax
    
@tsc_skip_calc:
    mov rsp, rbp
    RESTORE_REGS
    ret
MeasureTscFrequency ENDP

;-------------------------------------------------------------------------------
; DetectMemoryTopology - NUMA and memory layout detection
;-------------------------------------------------------------------------------
DetectMemoryTopology PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 1000h
    
    mov rbx, rcx                      ; RBX = PHASE1_CONTEXT*
    lea r12, [rbx].PHASE1_CONTEXT.topology
    
    ; Get system info
    lea rcx, [rbp-100h]
    call GetSystemInfo
    
    mov eax, [rbp-100h]               ; dwPageSize
    mov [r12].HARDWARE_TOPOLOGY.page_size, rax
    
    mov eax, [rbp-100h+28]            ; dwAllocationGranularity
    mov [r12].HARDWARE_TOPOLOGY.allocation_granularity, rax
    
    mov eax, [rbp-100h+32]            ; dwNumberOfProcessors
    mov [r12].HARDWARE_TOPOLOGY.processor_count, eax
    
    ; Get memory status
    mov dword ptr [rbp-200h], 40      ; sizeof MEMORYSTATUSEX
    lea rcx, [rbp-200h]
    call GlobalMemoryStatusEx
    
    mov rax, [rbp-200h+8]             ; ullTotalPhys
    mov [r12].HARDWARE_TOPOLOGY.total_physical_memory, rax
    
    mov rax, [rbp-200h+16]            ; ullAvailPhys
    mov [r12].HARDWARE_TOPOLOGY.available_memory, rax
    
    ; NUMA detection (simplified - detect node count)
    mov eax, [r12].HARDWARE_TOPOLOGY.processor_count
    mov [r12].HARDWARE_TOPOLOGY.numa_node_count, 1  ; Assume single node
    
    mov eax, 1
    
    mov rsp, rbp
    RESTORE_REGS
    ret
DetectMemoryTopology ENDP

;================================================================================
; MEMORY MANAGEMENT
;================================================================================

;-------------------------------------------------------------------------------
; InitializeMemoryArenas - Setup bump allocators
;-------------------------------------------------------------------------------
InitializeMemoryArenas PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 200h
    
    mov rbx, rcx                      ; RBX = PHASE1_CONTEXT*
    
    ; Initialize system arena
    lea r12, [rbx].PHASE1_CONTEXT.system_arena
    
    ; Reserve 256MB for system arena
    xor ecx, ecx
    mov edx, 10000000h                ; 256MB
    mov r8d, MEM_RESERVE + MEM_COMMIT
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @arena_init_fail
    
    mov [r12].MEMORY_ARENA.base_address, rax
    mov qword ptr [r12].MEMORY_ARENA.reserved_size, 10000000h
    mov qword ptr [r12].MEMORY_ARENA.committed_size, 10000000h
    mov qword ptr [r12].MEMORY_ARENA.block_size, 1000h
    mov dword ptr [r12].MEMORY_ARENA.numa_node, -1
    mov qword ptr [r12].MEMORY_ARENA.current_offset, 0
    
    mov eax, 1
    jmp @arena_init_exit
    
@arena_init_fail:
    xor eax, eax
    
@arena_init_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
InitializeMemoryArenas ENDP

;-------------------------------------------------------------------------------
; ArenaAllocate - Bump allocator from arena
; Input:  RCX = MEMORY_ARENA*
;         RDX = Size
;         R8  = Alignment (power of 2)
; Output: RAX = Pointer or NULL
;-------------------------------------------------------------------------------
ArenaAllocate PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    mov r12, rdx                      ; R12 = size
    mov r13, r8                       ; R13 = alignment
    
    ; Get current offset
    mov r14, [rbx].MEMORY_ARENA.current_offset
    
    ; Align
    mov rax, r14
    add rax, r13
    dec rax
    neg r13
    and rax, r13
    mov r14, rax
    
    ; Check if we have space
    mov r15, r14
    add r15, r12                      ; End of allocation
    
    cmp r15, [rbx].MEMORY_ARENA.committed_size
    jbe @arena_no_commit
    
    ; Out of space
    xor eax, eax
    jmp @arena_alloc_exit
    
@arena_no_commit:
    ; Return pointer
    mov rax, [rbx].MEMORY_ARENA.base_address
    add rax, r14
    
    ; Update offset
    add r14, r12
    mov [rbx].MEMORY_ARENA.current_offset, r14
    
@arena_alloc_exit:
    RESTORE_REGS
    ret
ArenaAllocate ENDP

;================================================================================
; SYNCHRONIZATION PRIMITIVES
;================================================================================

;-------------------------------------------------------------------------------
; InitializeSynchronizationPrimitives - Pre-allocate sync objects
;-------------------------------------------------------------------------------
InitializeSynchronizationPrimitives PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    
    ; For now, just zero out the pools
    mov qword ptr [rbx].PHASE1_CONTEXT.cs_pool, 0
    mov qword ptr [rbx].PHASE1_CONTEXT.event_pool, 0
    mov qword ptr [rbx].PHASE1_CONTEXT.semaphore_pool, 0
    
    RESTORE_REGS
    ret
InitializeSynchronizationPrimitives ENDP

;================================================================================
; PERFORMANCE COUNTERS
;================================================================================

;-------------------------------------------------------------------------------
; CalibratePerformanceCounters - Setup high-res timing
;-------------------------------------------------------------------------------
CalibratePerformanceCounters PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 40h
    
    mov rbx, rcx
    
    ; Get QPF
    lea rcx, [rbp-8]
    call QueryPerformanceFrequency
    mov rax, [rbp-8]
    mov [rbx].PHASE1_CONTEXT.perf_frequency, rax
    
    ; Get start counter
    lea rcx, [rbp-10h]
    call QueryPerformanceCounter
    mov rax, [rbp-10h]
    mov [rbx].PHASE1_CONTEXT.perf_counter_start, rax
    
    mov rsp, rbp
    RESTORE_REGS
    ret
CalibratePerformanceCounters ENDP

;-------------------------------------------------------------------------------
; ReadTsc - RDTSC with serialization
;-------------------------------------------------------------------------------
ReadTsc PROC FRAME
    SAVE_REGS
    
    ; Serialize instruction stream
    xor eax, eax
    cpuid
    rdtsc
    shl rdx, 32
    or rax, rdx
    
    RESTORE_REGS
    ret
ReadTsc ENDP

;-------------------------------------------------------------------------------
; GetElapsedMicroseconds - High-res elapsed time
;-------------------------------------------------------------------------------
GetElapsedMicroseconds PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 40h
    
    mov rbx, rcx                      ; PHASE1_CONTEXT*
    
    ; Get current QPC
    lea rcx, [rbp-8]
    call QueryPerformanceCounter
    
    ; Calculate delta
    mov rax, [rbp-8]
    sub rax, [rbx].PHASE1_CONTEXT.perf_counter_start
    
    ; Avoid division by zero
    test qword ptr [rbx].PHASE1_CONTEXT.perf_frequency, 0
    jz @elapsed_zero
    
    ; Convert to microseconds: delta * 1000000 / frequency
    mov rcx, 1000000
    mul rcx
    
    mov rcx, [rbx].PHASE1_CONTEXT.perf_frequency
    div rcx
    
    jmp @elapsed_exit
    
@elapsed_zero:
    xor eax, eax
    
@elapsed_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
GetElapsedMicroseconds ENDP

;================================================================================
; THREAD POOL INFRASTRUCTURE
;================================================================================

;-------------------------------------------------------------------------------
; InitializeThreadPoolInfrastructure - Setup worker thread management
;-------------------------------------------------------------------------------
InitializeThreadPoolInfrastructure PROC FRAME
    SAVE_REGS
    
    mov rbx, rcx
    
    ; Determine worker count (physical cores)
    mov eax, [rbx].PHASE1_CONTEXT.topology.cpu.physical_cores
    cmp eax, 0
    jne @tp_have_cores
    mov eax, 4                        ; Fallback: 4 threads
    
@tp_have_cores:
    mov [rbx].PHASE1_CONTEXT.worker_thread_count, eax
    
    RESTORE_REGS
    ret
InitializeThreadPoolInfrastructure ENDP

;================================================================================
; LOGGING
;================================================================================

;-------------------------------------------------------------------------------
; Phase1LogMessage - Write to console
;-------------------------------------------------------------------------------
Phase1LogMessage PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 40h
    
    mov rbx, rcx                      ; PHASE1_CONTEXT*
    mov r12, rdx                      ; Message
    
    ; Get stdout
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    test rax, rax
    jz @log_no_console
    
    ; Calculate string length
    mov rcx, r12
    call PhaseStrLen
    mov r13, rax                      ; R13 = length
    
    ; Write message
    mov rcx, rax
    mov rdx, r12
    mov r8, r13
    xor r9d, r9d
    lea r10, [rbp-8]
    push r10
    sub rsp, 20h
    call WriteConsoleA
    add rsp, 28h
    
@log_no_console:
    mov rsp, rbp
    RESTORE_REGS
    ret
Phase1LogMessage ENDP

;===============================================================================
; UTILITY FUNCTIONS
;===============================================================================

;-------------------------------------------------------------------------------
; PhaseStrLen - Get string length
;-------------------------------------------------------------------------------
PhaseStrLen PROC FRAME
    SAVE_REGS
    
    mov rax, rcx
    xor ecx, ecx
    dec rcx
    
@strlen_loop:
    inc rcx
    cmp byte ptr [rax+rcx], 0
    jne @strlen_loop
    mov rax, rcx
    
    RESTORE_REGS
    ret
PhaseStrLen ENDP

;================================================================================
; MACRO DEFINITIONS
;================================================================================
SAVE_REGS MACRO
    push rbx
    push r12
    push r13
    push r14
    push r15
ENDM

RESTORE_REGS MACRO
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
ENDM

;================================================================================
; EXPORTS
;================================================================================
PUBLIC Phase1Initialize
PUBLIC DetectCpuCapabilities
PUBLIC DetectMemoryTopology
PUBLIC InitializeMemoryArenas
PUBLIC ArenaAllocate
PUBLIC ReadTsc
PUBLIC GetElapsedMicroseconds
PUBLIC InitializeThreadPoolInfrastructure
PUBLIC Phase1LogMessage
PUBLIC g_Phase1Context
PUBLIC g_Phase1Initialized

END
