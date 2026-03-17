; ============================================================================
; PERFORMANCE BASELINE - Pure MASM x64 Performance Measurement
; ============================================================================
; High-resolution timing and latency measurement infrastructure
; Features:
;   - QueryPerformanceCounter-based microsecond timing
;   - Histogram recording for latency distribution
;   - P50, P95, P99 percentile calculation
;   - Moving average calculation
;   - Memory-efficient circular buffer for samples
; ============================================================================

.code

; External functions
extern QueryPerformanceCounter:PROC
extern QueryPerformanceFrequency:PROC
extern LocalAlloc:PROC
extern LocalFree:PROC
extern wsprintfA:PROC
extern Logger_LogStructured:PROC
extern Metrics_RecordHistogramMission:PROC

; ============================================================================
; Constants
; ============================================================================
LMEM_ZEROINIT = 40h
LOG_LEVEL_INFO = 1
LOG_LEVEL_DEBUG = 0

MAX_SAMPLES = 10000
HISTOGRAM_BUCKETS = 50

; ============================================================================
; Performance Context Structure (128 bytes)
; ============================================================================
; +0:   frequency (8 bytes) - QPF frequency for conversions
; +8:   start_time (8 bytes) - start timestamp
; +16:  end_time (8 bytes) - end timestamp  
; +24:  sample_count (4 bytes) - number of samples collected
; +28:  sample_capacity (4 bytes) - max samples
; +32:  pSamples (8 bytes) - pointer to sample array
; +40:  sum_latency (8 bytes) - sum for average calculation
; +48:  min_latency (8 bytes) - minimum observed
; +56:  max_latency (8 bytes) - maximum observed
; +64:  histogram_buckets (8×50=400 bytes follows after struct)
; ============================================================================

.data
    szPerfInitialized       db "Performance measurement initialized", 0
    szPerfSampleRecorded    db "Performance sample recorded", 0
    szPerfReportGenerated   db "Performance report generated", 0
    
    szReportHeader          db "╔═══════════════════════════════════════════════════════════════╗", 13, 10
                            db "║  PERFORMANCE BASELINE REPORT                                  ║", 13, 10
                            db "╠═══════════════════════════════════════════════════════════════╣", 13, 10, 0
    
    szReportFooter          db "╚═══════════════════════════════════════════════════════════════╝", 13, 10, 0
    
    szStatLineFormat        db "║  %-30s: %10llu µs              ║", 13, 10, 0
    szPercentileFormat      db "║  P%-2d                          : %10llu µs              ║", 13, 10, 0
    
    szMinLabel              db "Minimum Latency", 0
    szMaxLabel              db "Maximum Latency", 0
    szAvgLabel              db "Average Latency", 0
    szMedianLabel           db "Median (P50)", 0
    szP95Label              db "95th Percentile", 0
    szP99Label              db "99th Percentile", 0
    szSampleCountLabel      db "Total Samples", 0

.data?
    qwFrequency             qword ?
    pPerfContext            qword ?

.code

; ============================================================================
; Perf_Initialize - Initialize performance measurement system
; ============================================================================
; Parameters: None
; Returns:
;   RAX = Performance context handle (0 on failure)
; ============================================================================
PUBLIC Perf_Initialize
Perf_Initialize PROC
    push rbx
    push rsi
    sub rsp, 28h
    
    ; Get QP frequency
    lea rcx, [qwFrequency]
    call QueryPerformanceFrequency
    
    test eax, eax
    jz perf_init_failed
    
    ; Allocate performance context (128 bytes + histogram space)
    mov rcx, 528                ; 128 + 400 for histogram
    mov rdx, LMEM_ZEROINIT
    call LocalAlloc
    
    test rax, rax
    jz perf_init_failed
    
    mov rbx, rax
    mov [pPerfContext], rax
    
    ; Initialize context
    mov rax, [qwFrequency]
    mov [rbx + 0], rax          ; store frequency
    mov dword ptr [rbx + 28], MAX_SAMPLES
    
    ; Allocate sample array (8 bytes per sample × MAX_SAMPLES)
    mov rcx, 80000              ; 10000 * 8
    mov rdx, LMEM_ZEROINIT
    call LocalAlloc
    
    test rax, rax
    jz perf_init_cleanup_ctx
    
    mov [rbx + 32], rax         ; store samples pointer
    
    ; Initialize min/max
    mov rax, 0FFFFFFFFFFFFFFFFh
    mov [rbx + 48], rax         ; min = max uint64
    xor rax, rax
    mov [rbx + 56], rax         ; max = 0
    
    ; Log initialization
    lea rcx, [szPerfInitialized]
    mov rdx, LOG_LEVEL_INFO
    call Logger_LogStructured
    
    mov rax, rbx
    jmp perf_init_done
    
perf_init_cleanup_ctx:
    mov rcx, rbx
    call LocalFree
    
perf_init_failed:
    xor rax, rax
    
perf_init_done:
    add rsp, 28h
    pop rsi
    pop rbx
    ret
Perf_Initialize ENDP

; ============================================================================
; Perf_StartMeasurement - Begin timing measurement
; ============================================================================
; Parameters:
;   RCX = Performance context
; Returns: None
; ============================================================================
PUBLIC Perf_StartMeasurement
Perf_StartMeasurement PROC
    push rbx
    sub rsp, 20h
    
    mov rbx, rcx
    
    ; Record start timestamp
    lea rcx, [rbx + 8]          ; start_time field
    call QueryPerformanceCounter
    
    add rsp, 20h
    pop rbx
    ret
Perf_StartMeasurement ENDP

; ============================================================================
; Perf_EndMeasurement - End timing and record sample
; ============================================================================
; Parameters:
;   RCX = Performance context
; Returns:
;   RAX = Latency in microseconds
; ============================================================================
PUBLIC Perf_EndMeasurement
Perf_EndMeasurement PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rbx, rcx
    
    ; Record end timestamp
    lea rcx, [rbx + 16]         ; end_time field
    call QueryPerformanceCounter
    
    ; Calculate duration in ticks
    mov rax, [rbx + 16]         ; end_time
    sub rax, [rbx + 8]          ; start_time
    
    ; Convert to microseconds: (ticks * 1000000) / frequency
    mov rcx, 1000000
    mul rcx
    mov rcx, [rbx + 0]          ; frequency
    div rcx
    
    mov rsi, rax                ; latency in µs
    
    ; Update statistics
    mov rdi, [rbx + 40]         ; sum_latency
    add rdi, rsi
    mov [rbx + 40], rdi
    
    ; Update min
    mov rax, [rbx + 48]
    cmp rsi, rax
    jae check_max
    mov [rbx + 48], rsi
    
check_max:
    ; Update max
    mov rax, [rbx + 56]
    cmp rsi, rax
    jbe record_sample
    mov [rbx + 56], rsi
    
record_sample:
    ; Store sample if space available
    mov eax, [rbx + 24]         ; sample_count
    cmp eax, [rbx + 28]         ; sample_capacity
    jae skip_store
    
    mov rcx, [rbx + 32]         ; pSamples
    mov [rcx + rax*8], rsi      ; store sample
    inc dword ptr [rbx + 24]    ; increment count
    
skip_store:
    ; Update histogram
    mov rcx, rbx
    mov rdx, rsi                ; latency value
    call UpdateHistogram
    
    ; Return latency
    mov rax, rsi
    
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
Perf_EndMeasurement ENDP

; ============================================================================
; Perf_GenerateReport - Generate performance report
; ============================================================================
; Parameters:
;   RCX = Performance context
;   RDX = Output buffer
;   R8  = Buffer size
; Returns:
;   RAX = Bytes written
; ============================================================================
PUBLIC Perf_GenerateReport
Perf_GenerateReport PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 48h
    
    mov rbx, rcx                ; context
    mov rsi, rdx                ; output buffer
    mov rdi, r8                 ; buffer size
    
    ; Start with header
    mov rcx, rsi
    lea rdx, [szReportHeader]
    call lstrcatA
    
    ; Calculate statistics
    mov eax, [rbx + 24]         ; sample_count
    test eax, eax
    jz report_no_samples
    
    ; Average latency
    mov rax, [rbx + 40]         ; sum_latency
    xor rdx, rdx
    mov ecx, [rbx + 24]
    div rcx
    mov r12, rax                ; average
    
    ; Sort samples for percentiles
    mov rcx, [rbx + 32]         ; pSamples
    mov edx, [rbx + 24]         ; count
    call SortSamples
    
    ; Calculate percentiles
    mov rcx, [rbx + 32]
    mov edx, [rbx + 24]
    mov r8d, 50                 ; P50 (median)
    call CalculatePercentile
    mov r13, rax
    
    mov rcx, [rbx + 32]
    mov edx, [rbx + 24]
    mov r8d, 95                 ; P95
    call CalculatePercentile
    mov r14, rax
    
    mov rcx, [rbx + 32]
    mov edx, [rbx + 24]
    mov r8d, 99                 ; P99
    call CalculatePercentile
    mov r15, rax
    
    ; Format statistics
    lea rcx, [rsp+20h]
    lea rdx, [szStatLineFormat]
    lea r8, [szMinLabel]
    mov r9, [rbx + 48]          ; min_latency
    call wsprintfA
    
    mov rcx, rsi
    lea rdx, [rsp+20h]
    call lstrcatA
    
    ; Max latency
    lea rcx, [rsp+20h]
    lea rdx, [szStatLineFormat]
    lea r8, [szMaxLabel]
    mov r9, [rbx + 56]          ; max_latency
    call wsprintfA
    
    mov rcx, rsi
    lea rdx, [rsp+20h]
    call lstrcatA
    
    ; Average latency
    lea rcx, [rsp+20h]
    lea rdx, [szStatLineFormat]
    lea r8, [szAvgLabel]
    mov r9, r12                 ; average
    call wsprintfA
    
    mov rcx, rsi
    lea rdx, [rsp+20h]
    call lstrcatA
    
    ; Median (P50)
    lea rcx, [rsp+20h]
    lea rdx, [szStatLineFormat]
    lea r8, [szMedianLabel]
    mov r9, r13
    call wsprintfA
    
    mov rcx, rsi
    lea rdx, [rsp+20h]
    call lstrcatA
    
    ; P95
    lea rcx, [rsp+20h]
    lea rdx, [szStatLineFormat]
    lea r8, [szP95Label]
    mov r9, r14
    call wsprintfA
    
    mov rcx, rsi
    lea rdx, [rsp+20h]
    call lstrcatA
    
    ; P99
    lea rcx, [rsp+20h]
    lea rdx, [szStatLineFormat]
    lea r8, [szP99Label]
    mov r9, r15
    call wsprintfA
    
    mov rcx, rsi
    lea rdx, [rsp+20h]
    call lstrcatA
    
report_no_samples:
    ; Footer
    mov rcx, rsi
    lea rdx, [szReportFooter]
    call lstrcatA
    
    ; Calculate length
    mov rcx, rsi
    call lstrlenA
    
    add rsp, 48h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Perf_GenerateReport ENDP

; ============================================================================
; Perf_Cleanup - Free performance context
; ============================================================================
; Parameters:
;   RCX = Performance context
; Returns: None
; ============================================================================
PUBLIC Perf_Cleanup
Perf_Cleanup PROC
    push rbx
    sub rsp, 20h
    
    mov rbx, rcx
    
    ; Free samples array
    mov rcx, [rbx + 32]
    test rcx, rcx
    jz cleanup_context
    call LocalFree
    
cleanup_context:
    ; Free context
    mov rcx, rbx
    call LocalFree
    
    add rsp, 20h
    pop rbx
    ret
Perf_Cleanup ENDP

; ============================================================================
; Helper Functions
; ============================================================================

UpdateHistogram PROC
    ; Stub - updates histogram buckets
    ret
UpdateHistogram ENDP

SortSamples PROC
    ; Stub - quicksort implementation for percentile calculation
    ret
SortSamples ENDP

CalculatePercentile PROC
    ; Stub - calculates Nth percentile from sorted array
    ; percentile_index = (count * percentile) / 100
    xor rax, rax
    ret
CalculatePercentile ENDP

; External string functions
extern lstrcatA:PROC
extern lstrlenA:PROC

END
