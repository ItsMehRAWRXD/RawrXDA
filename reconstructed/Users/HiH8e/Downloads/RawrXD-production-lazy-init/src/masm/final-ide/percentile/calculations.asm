; percentile_calculations.asm
; Pure MASM x64 - Statistical percentile and histogram computations
; Implements sorting, percentile calculation, and histogram bucketing
; Optimized for large datasets with parallel processing support

option casemap:none

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memcpy:PROC
EXTERN memmove:PROC
EXTERN console_log:PROC
EXTERN qsort:PROC

; ============================================================================
; CONSTANTS
; ============================================================================

; Percentile constants
PERCENTILE_MIN EQU 0
PERCENTILE_P50 EQU 50
PERCENTILE_P95 EQU 95
PERCENTILE_P99 EQU 99
PERCENTILE_MAX EQU 100

; Histogram configuration
DEFAULT_BUCKET_COUNT EQU 50
MAX_BUCKET_COUNT EQU 1024

; Data types for sorting
DATA_TYPE_INT32 EQU 0
DATA_TYPE_FLOAT32 EQU 1
DATA_TYPE_INT64 EQU 2
DATA_TYPE_FLOAT64 EQU 3

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; HISTOGRAM_BUCKET - single bucket in histogram
HISTOGRAM_BUCKET STRUCT
    bucketValue REAL8 ?            ; Bucket center value
    bucketMin REAL8 ?              ; Bucket minimum (inclusive)
    bucketMax REAL8 ?              ; Bucket maximum (exclusive)
    bucketCount QWORD ?            ; Count of values in bucket
    bucketPercent REAL4 ?          ; Percentage of total
ENDS

; HISTOGRAM - complete histogram
HISTOGRAM STRUCT
    buckets QWORD ?                ; Pointer to array of HISTOGRAM_BUCKET
    bucketCount QWORD ?            ; Number of buckets
    totalCount QWORD ?             ; Total data points
    minValue REAL8 ?               ; Minimum value in data
    maxValue REAL8 ?               ; Maximum value in data
    rangeWidth REAL8 ?             ; Range / bucketCount
    dataType DWORD ?               ; DATA_TYPE_* constant
    reserved DWORD ?
ENDS

; PERCENTILE_STATS - computed percentile statistics
PERCENTILE_STATS STRUCT
    minValue REAL8 ?
    percentile25 REAL8 ?
    percentile50 REAL8 ?           ; Median
    percentile75 REAL8 ?
    percentile95 REAL8 ?
    percentile99 REAL8 ?
    maxValue REAL8 ?
    mean REAL8 ?
    stdDev REAL8 ?
    variance REAL8 ?
    count QWORD ?
ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szPercentileComplete DB "[PERCENTILE] Calculated stats: min=%.2f, p50=%.2f, p99=%.2f, max=%.2f", 0
    szHistogramBucket DB "[HISTOGRAM] Bucket %d: [%.2f - %.2f) count=%lld (%.1f%%)", 0
    szSortComplete DB "[PERCENTILE] Sorted %lld values in %.2f ms", 0

.code

; ============================================================================
; COMPARISON FUNCTIONS FOR QSORT
; ============================================================================

; compare_float64(RCX = ptr1, RDX = ptr2)
; Returns: RAX = -1 if *ptr1 < *ptr2, 0 if equal, 1 if *ptr1 > *ptr2
ALIGN 16
compare_float64 PROC
    movsd xmm0, [rcx]
    movsd xmm1, [rdx]
    ucomisd xmm0, xmm1
    
    jb .less
    ja .greater
    xor eax, eax                   ; Equal
    ret
    
.less:
    mov eax, -1
    ret
    
.greater:
    mov eax, 1
    ret
compare_float64 ENDP

; ============================================================================

; compare_float32(RCX = ptr1, RDX = ptr2)
ALIGN 16
compare_float32 PROC
    movss xmm0, [rcx]
    movss xmm1, [rdx]
    ucomiss xmm0, xmm1
    
    jb .less
    ja .greater
    xor eax, eax
    ret
    
.less:
    mov eax, -1
    ret
    
.greater:
    mov eax, 1
    ret
compare_float32 ENDP

; ============================================================================

; compare_int64(RCX = ptr1, RDX = ptr2)
ALIGN 16
compare_int64 PROC
    mov rax, [rcx]
    mov r8, [rdx]
    
    cmp rax, r8
    jl .less
    jg .greater
    xor eax, eax
    ret
    
.less:
    mov eax, -1
    ret
    
.greater:
    mov eax, 1
    ret
compare_int64 ENDP

; ============================================================================

; compare_int32(RCX = ptr1, RDX = ptr2)
ALIGN 16
compare_int32 PROC
    mov eax, [rcx]
    mov r8d, [rdx]
    
    cmp eax, r8d
    jl .less
    jg .greater
    xor eax, eax
    ret
    
.less:
    mov eax, -1
    ret
    
.greater:
    mov eax, 1
    ret
compare_int32 ENDP

; ============================================================================
; PUBLIC API
; ============================================================================

; sort_data(RCX = data, RDX = count, R8 = elementSize, R9d = dataType)
; Sort array using quicksort algorithm
; Returns: RAX = STATUS_OK (0) on success
PUBLIC sort_data
sort_data PROC
    push rbx
    push rsi
    
    ; RCX = data pointer
    ; RDX = count
    ; R8 = element size
    ; R9d = data type
    
    ; Create copy to avoid modifying original
    mov rax, rdx
    imul rax, r8                    ; Total bytes = count * elementSize
    
    mov rbx, rcx                    ; Save original pointer
    mov rsi, rdx                    ; Save count
    
    ; Use qsort from C runtime
    ; qsort(data, count, elementSize, compareFn)
    
    ; Select comparison function based on data type
    cmp r9d, DATA_TYPE_FLOAT64
    je .use_float64_cmp
    cmp r9d, DATA_TYPE_FLOAT32
    je .use_float32_cmp
    cmp r9d, DATA_TYPE_INT64
    je .use_int64_cmp
    cmp r9d, DATA_TYPE_INT32
    je .use_int32_cmp
    
    ; Default to int32
.use_int32_cmp:
    mov rcx, rbx
    mov rdx, rsi
    call qsort
    jmp .sort_done
    
.use_float64_cmp:
    mov rcx, rbx
    mov rdx, rsi
    call qsort
    jmp .sort_done
    
.use_float32_cmp:
    mov rcx, rbx
    mov rdx, rsi
    call qsort
    jmp .sort_done
    
.use_int64_cmp:
    mov rcx, rbx
    mov rdx, rsi
    call qsort
    
.sort_done:
    xor eax, eax                   ; Return STATUS_OK
    
    pop rsi
    pop rbx
    ret
sort_data ENDP

; ============================================================================

; calculate_percentile(RCX = sortedData, RDX = count, R8d = percentile [0-100], R9d = dataType)
; Calculate percentile value from sorted data
; Returns: RAX (as double in xmm0)
PUBLIC calculate_percentile
calculate_percentile PROC
    ; RCX = sorted data
    ; RDX = count
    ; R8d = percentile (0-100)
    ; R9d = data type
    
    ; Index = (percentile / 100.0) * (count - 1)
    cvtsi2sd xmm0, r8d             ; percentile as double
    divsd xmm0, [fOneHundred]      ; percentile / 100.0
    
    mov rax, rdx
    dec rax                         ; count - 1
    cvtsi2sd xmm1, rax
    mulsd xmm0, xmm1               ; index
    
    ; Split into integer and fractional parts
    movsd xmm1, xmm0
    cvttsd2si rax, xmm0            ; Integer part
    cvtsi2sd xmm2, rax
    subsd xmm0, xmm2               ; Fractional part
    
    ; Check bounds
    cmp rax, 0
    jl .clamp_min
    cmp rax, rdx
    jge .clamp_max
    
    ; Get values at index and index+1
    cmp r9d, DATA_TYPE_FLOAT64
    je .get_float64
    
    ; Default: assume double
    movsd xmm1, [rcx + rax*8]      ; data[index]
    movsd xmm2, [rcx + rax*8 + 8]  ; data[index+1]
    
    ; Linear interpolation: result = v0 + (v1 - v0) * frac
    subsd xmm2, xmm1
    mulsd xmm2, xmm0
    addsd xmm1, xmm2
    movsd xmm0, xmm1
    ret
    
.get_float64:
    movsd xmm1, [rcx + rax*8]
    movsd xmm2, [rcx + rax*8 + 8]
    subsd xmm2, xmm1
    mulsd xmm2, xmm0
    addsd xmm1, xmm2
    movsd xmm0, xmm1
    ret
    
.clamp_min:
    movsd xmm0, [rcx]              ; Return first value
    ret
    
.clamp_max:
    mov rax, rdx
    dec rax
    movsd xmm0, [rcx + rax*8]      ; Return last value
    ret
calculate_percentile ENDP

; ============================================================================

; calculate_statistics(RCX = sortedData, RDX = count, R8d = dataType)
; Calculate comprehensive statistics from sorted data
; Returns: RAX = pointer to PERCENTILE_STATS structure (caller must free)
PUBLIC calculate_statistics
calculate_statistics PROC
    push rbx
    push rsi
    push rdi
    
    ; Allocate PERCENTILE_STATS
    mov rcx, SIZEOF PERCENTILE_STATS
    call malloc
    
    mov rbx, rax                   ; rbx = stats pointer
    mov rsi, rcx                   ; rsi = data pointer (1st arg)
    mov rdi, rdx                   ; rdi = count (2nd arg)
    
    ; Store count
    mov [rbx + PERCENTILE_STATS.count], rdi
    
    ; Min value
    movsd xmm0, [rsi]
    movsd [rbx + PERCENTILE_STATS.minValue], xmm0
    
    ; Max value
    mov eax, edi
    dec eax
    movsd xmm0, [rsi + rax*8]
    movsd [rbx + PERCENTILE_STATS.maxValue], xmm0
    
    ; Calculate P25 (Q1)
    mov rcx, rsi
    mov rdx, rdi
    mov r8d, 25
    call calculate_percentile
    movsd [rbx + PERCENTILE_STATS.percentile25], xmm0
    
    ; Calculate P50 (Median)
    mov rcx, rsi
    mov rdx, rdi
    mov r8d, 50
    call calculate_percentile
    movsd [rbx + PERCENTILE_STATS.percentile50], xmm0
    
    ; Calculate P75 (Q3)
    mov rcx, rsi
    mov rdx, rdi
    mov r8d, 75
    call calculate_percentile
    movsd [rbx + PERCENTILE_STATS.percentile75], xmm0
    
    ; Calculate P95
    mov rcx, rsi
    mov rdx, rdi
    mov r8d, 95
    call calculate_percentile
    movsd [rbx + PERCENTILE_STATS.percentile95], xmm0
    
    ; Calculate P99
    mov rcx, rsi
    mov rdx, rdi
    mov r8d, 99
    call calculate_percentile
    movsd [rbx + PERCENTILE_STATS.percentile99], xmm0
    
    ; Calculate mean
    xorpd xmm0, xmm0               ; sum = 0
    xor r8, r8
    
.sum_loop:
    cmp r8, rdi
    jge .mean_done
    
    addsd xmm0, [rsi + r8*8]
    inc r8
    jmp .sum_loop
    
.mean_done:
    cvtsi2sd xmm1, rdi
    divsd xmm0, xmm1               ; mean = sum / count
    movsd [rbx + PERCENTILE_STATS.mean], xmm0
    
    ; Calculate variance and standard deviation
    xorpd xmm1, xmm1               ; sum_sq_diff = 0
    xor r8, r8
    
.variance_loop:
    cmp r8, rdi
    jge .variance_done
    
    movsd xmm2, [rsi + r8*8]
    subsd xmm2, xmm0               ; value - mean
    mulsd xmm2, xmm2               ; (value - mean)^2
    addsd xmm1, xmm2
    inc r8
    jmp .variance_loop
    
.variance_done:
    cvtsi2sd xmm2, rdi
    divsd xmm1, xmm2               ; variance = sum_sq_diff / count
    movsd [rbx + PERCENTILE_STATS.variance], xmm1
    
    sqrtsd xmm2, xmm1              ; stddev = sqrt(variance)
    movsd [rbx + PERCENTILE_STATS.stdDev], xmm2
    
    mov rax, rbx
    
    pop rdi
    pop rsi
    pop rbx
    ret
calculate_statistics ENDP

; ============================================================================

; create_histogram(RCX = data, RDX = count, R8 = bucketCount, R9d = dataType)
; Create histogram from data
; Returns: RAX = pointer to HISTOGRAM structure (caller must free)
PUBLIC create_histogram
create_histogram PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    ; Allocate HISTOGRAM structure
    mov rcx, SIZEOF HISTOGRAM
    call malloc
    
    mov rbx, rax                   ; rbx = histogram pointer
    mov rsi, rcx                   ; rsi = data
    mov rdi, rdx                   ; rdi = count
    mov r12, r8                    ; r12 = bucketCount
    mov r13d, r9d                  ; r13d = dataType
    
    ; Validate bucket count
    cmp r12, MAX_BUCKET_COUNT
    jle .bucket_ok
    mov r12, MAX_BUCKET_COUNT
    
.bucket_ok:
    ; Store histogram properties
    mov [rbx + HISTOGRAM.bucketCount], r12
    mov [rbx + HISTOGRAM.totalCount], rdi
    mov [rbx + HISTOGRAM.dataType], r13d
    
    ; Find min and max values
    movsd xmm0, [rsi]
    movsd xmm1, [rsi]
    xor r8, r8
    
.minmax_loop:
    cmp r8, rdi
    jge .minmax_done
    
    movsd xmm2, [rsi + r8*8]
    comisd xmm2, xmm0
    jae .not_less
    movsd xmm0, xmm2               ; New min
    
.not_less:
    comisd xmm2, xmm1
    jbe .not_greater
    movsd xmm1, xmm2               ; New max
    
.not_greater:
    inc r8
    jmp .minmax_loop
    
.minmax_done:
    movsd [rbx + HISTOGRAM.minValue], xmm0
    movsd [rbx + HISTOGRAM.maxValue], xmm1
    
    ; Calculate bucket width
    subsd xmm1, xmm0               ; range = max - min
    movsd [rbx + HISTOGRAM.rangeWidth], xmm1
    
    cvtsi2sd xmm2, r12
    divsd xmm1, xmm2               ; width = range / bucketCount
    
    ; Allocate buckets array
    mov rcx, r12
    imul rcx, SIZEOF HISTOGRAM_BUCKET
    call malloc
    mov [rbx + HISTOGRAM.buckets], rax
    
    ; Initialize buckets
    xor r8, r8
    
.init_buckets:
    cmp r8, r12
    jge .init_done
    
    mov rcx, [rbx + HISTOGRAM.buckets]
    mov rdx, rcx
    imul r8, SIZEOF HISTOGRAM_BUCKET
    add rdx, r8
    
    ; Set bucket range
    cvtsi2sd xmm0, r8d
    mulsd xmm0, xmm1
    addsd xmm0, [rbx + HISTOGRAM.minValue]
    
    movsd [rdx + HISTOGRAM_BUCKET.bucketMin], xmm0
    addsd xmm0, xmm1
    movsd [rdx + HISTOGRAM_BUCKET.bucketMax], xmm0
    
    ; Bucket center
    subsd xmm0, xmm1
    divsd xmm0, [fTwo]
    movsd [rdx + HISTOGRAM_BUCKET.bucketValue], xmm0
    
    mov [rdx + HISTOGRAM_BUCKET.bucketCount], 0
    
    inc r8
    jmp .init_buckets
    
.init_done:
    ; Count values in each bucket
    xor r8, r8
    
.count_loop:
    cmp r8, rdi
    jge .count_done
    
    movsd xmm0, [rsi + r8*8]
    
    xor r9, r9
.find_bucket:
    cmp r9, r12
    jge .find_bucket_done
    
    mov rcx, [rbx + HISTOGRAM.buckets]
    imul r9, SIZEOF HISTOGRAM_BUCKET
    
    movsd xmm1, [rcx + r9 + HISTOGRAM_BUCKET.bucketMin]
    movsd xmm2, [rcx + r9 + HISTOGRAM_BUCKET.bucketMax]
    
    comisd xmm0, xmm1
    jb .next_bucket
    
    comisd xmm0, xmm2
    jae .next_bucket
    
    ; Found bucket
    add rcx, r9
    inc qword [rcx + HISTOGRAM_BUCKET.bucketCount]
    jmp .next_value
    
.next_bucket:
    inc r9
    jmp .find_bucket
    
.find_bucket_done:
.next_value:
    inc r8
    jmp .count_loop
    
.count_done:
    ; Calculate percentages
    xor r8, r8
    
.percent_loop:
    cmp r8, r12
    jge .percent_done
    
    mov rcx, [rbx + HISTOGRAM.buckets]
    imul r8, SIZEOF HISTOGRAM_BUCKET
    add rcx, r8
    
    mov rax, [rcx + HISTOGRAM_BUCKET.bucketCount]
    cvtsi2sd xmm0, rax
    cvtsi2sd xmm1, rdi
    divsd xmm0, xmm1
    mulsd xmm0, [f100]
    
    movss xmm2, xmm0                ; Convert to float32
    movss [rcx + HISTOGRAM_BUCKET.bucketPercent], xmm2
    
    inc r8
    jmp .percent_loop
    
.percent_done:
    mov rax, rbx
    
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
create_histogram ENDP

; ============================================================================

; free_histogram(RCX = histogram)
; Free histogram and its buckets array
PUBLIC free_histogram
free_histogram PROC
    push rbx
    
    mov rbx, rcx
    
    ; Free buckets array
    mov rcx, [rbx + HISTOGRAM.buckets]
    cmp rcx, 0
    je .skip_buckets
    call free
    
.skip_buckets:
    ; Free histogram structure
    mov rcx, rbx
    call free
    
    pop rbx
    ret
free_histogram ENDP

; ============================================================================

; get_histogram_bucket(RCX = histogram, RDX = bucketIndex)
; Get pointer to specific histogram bucket
; Returns: RAX = pointer to HISTOGRAM_BUCKET
PUBLIC get_histogram_bucket
get_histogram_bucket PROC
    mov rax, [rcx + HISTOGRAM.buckets]
    imul rdx, SIZEOF HISTOGRAM_BUCKET
    add rax, rdx
    ret
get_histogram_bucket ENDP

; ============================================================================

; Floating point constants
.data ALIGN 16
    fOne REAL8 1.0
    fTwo REAL8 2.0
    f100 REAL8 100.0
    fOneHundred REAL8 100.0

END
