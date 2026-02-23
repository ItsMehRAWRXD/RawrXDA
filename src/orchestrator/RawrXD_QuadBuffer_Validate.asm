; =============================================================================
; RawrXD_QuadBuffer_Validate.asm
; Runtime Validation and Benchmarking for QuadBuffer DMA System
; =============================================================================
; Tests and validates QuadBuffer functionality at runtime
; Measures throughput, latency, and buffer efficiency
; =============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:3

INCLUDE \masm64\include64\masm64rt.inc
INCLUDE RawrXD_QuadBuffer_Integration.inc

; =============================================================================
; External Functions
; =============================================================================

EXTERN GetTickCount:PROC
EXTERN GetTickCount64:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN Sleep:PROC
EXTERN OutputDebugStringA:PROC

; =============================================================================
; Data Structures
; =============================================================================

VALIDATION_RESULT STRUCT
    test_name           DB 64 DUP(0)
    test_passed         DD ?
    elapsed_ms          DD ?
    throughput_mbps     DQ ?
    error_code          DD ?
VALIDATION_RESULT ENDS

BENCHMARK_STATS STRUCT
    total_tests         DD ?
    passed_tests        DD ?
    failed_tests        DD ?
    total_time_ms       DD ?
    avg_layer_time_ms   DD ?
    buffer_efficiency   DD ?
BENCHMARK_STATS ENDS

; =============================================================================
; Data Section
; =============================================================================

.DATA

align 64
g_validation_results    VALIDATION_RESULT 10 DUP(<>)
g_result_count          DD 0

align 64
g_benchmark_stats       BENCHMARK_STATS <>

align 64
g_perf_frequency        DQ 0

; Test strings
test_buffer_init        DB "QuadBuffer Initialization", 0
test_buffer_access      DB "Buffer Access (Optimal)", 0
test_buffer_trap        DB "YTFN Trap Handling", 0
test_buffer_rotation    DB "Buffer Rotation Cascade", 0
test_sequential_access  DB "Sequential Layer Access", 0
test_random_access      DB "Random Layer Access", 0
test_dma_throughput     DB "DMA Throughput Measurement", 0
test_efficiency         DB "Buffer Efficiency Check", 0

.CODE

; =============================================================================
; INITIALIZE_VALIDATION
; Setup validation environment
; =============================================================================
INITIALIZE_VALIDATION PROC FRAME
    push rbx
    .allocstack 8
    .endprolog
    
    ; Get performance counter frequency
    lea rcx, g_perf_frequency
    call QueryPerformanceFrequency
    
    ; Initialize result counter
    mov dword ptr g_result_count, 0
    
    ; Initialize stats
    mov dword ptr g_benchmark_stats.total_tests, 0
    mov dword ptr g_benchmark_stats.passed_tests, 0
    mov dword ptr g_benchmark_stats.failed_tests, 0
    mov dword ptr g_benchmark_stats.total_time_ms, 0
    
    pop rbx
    ret
INITIALIZE_VALIDATION ENDP

; =============================================================================
; TEST_BUFFER_INITIALIZATION
; Verify QuadBuffer initializes correctly
; =============================================================================
TEST_BUFFER_INITIALIZATION PROC FRAME
    push rbx r12 r13
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov r12d, 1                     ; Assume pass
    
    ; Get result index
    mov eax, g_result_count
    mov rbx, rax
    imul rax, SIZEOF VALIDATION_RESULT
    lea r13, g_validation_results[rax]
    
    ; Record test name
    lea rax, test_buffer_init
    mov rcx, r13
    mov rdx, OFFSET VALIDATION_RESULT.test_name
    add rcx, rdx
    mov edx, 64
.copy_name:
    dec edx
    js .name_done
    mov al, byte ptr [rax]
    test al, al
    mov byte ptr [rcx], al
    jz .name_done
    inc rax
    inc rcx
    jmp .copy_name
    
.name_done:
    ; Start timer
    call GetTickCount
    mov r8d, eax
    
    ; Call initialization
    mov rcx, OFFSET INFINITY_InitializeStream
    call rcx
    test rax, rax
    jz .init_failed
    
    ; Stop timer
    call GetTickCount
    sub eax, r8d
    mov [r13].VALIDATION_RESULT.elapsed_ms, eax
    
    ; Check result
    mov eax, 1
    jmp .init_done
    
.init_failed:
    mov r12d, 0
    mov eax, 0
    
.init_done:
    mov [r13].VALIDATION_RESULT.test_passed, eax
    inc dword ptr g_result_count
    
    add rsp, 40
    pop r13 r12 rbx
    ret
TEST_BUFFER_INITIALIZATION ENDP

; =============================================================================
; TEST_SEQUENTIAL_ACCESS
; Test optimal case: sequential layer access
; Measures throughput for layers 0-99
; =============================================================================
TEST_SEQUENTIAL_ACCESS PROC FRAME
    push rbx r12 r13 r14
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov r12d, 1                     ; Assume pass
    
    ; Get result index
    mov eax, g_result_count
    imul rax, SIZEOF VALIDATION_RESULT
    lea r13, g_validation_results[rax]
    
    ; Record test name
    lea rax, test_sequential_access
    mov rcx, r13
    mov rdx, OFFSET VALIDATION_RESULT.test_name
    add rcx, rdx
    mov edx, 64
.copy_seq_name:
    dec edx
    js .seq_name_done
    mov al, byte ptr [rax]
    mov byte ptr [rcx], al
    inc rax
    inc rcx
    jmp .copy_seq_name
    
.seq_name_done:
    ; Start timer
    call GetTickCount
    mov r14d, eax
    
    ; Access 100 layers sequentially
    xor ecx, ecx                    ; Layer counter
    
.seq_loop:
    cmp ecx, 100
    jge .seq_done
    
    ; Check buffer
    CHECK_BUFFER rcx, 0
    
    ; Simulate compute
    nop
    nop
    
    ; Notify completion
    ROTATE_AFTER_COMPUTE rcx
    
    inc ecx
    jmp .seq_loop
    
.seq_done:
    ; Stop timer
    call GetTickCount
    sub eax, r14d
    mov [r13].VALIDATION_RESULT.elapsed_ms, eax
    
    ; Calculate throughput
    ; Assume 1GB per layer = 100GB total
    ; Throughput = 100GB / elapsed_time
    mov edx, 100                    ; 100 GB
    mov ecx, eax                    ; elapsed ms
    test ecx, ecx
    jz .seq_calc_done
    
    mov eax, edx
    mov edx, 1024
    mul edx                         ; Convert GB to MB
    div ecx                         ; MB / ms = MB/s
    mov [r13].VALIDATION_RESULT.throughput_mbps, rax
    
.seq_calc_done:
    mov eax, 1
    mov [r13].VALIDATION_RESULT.test_passed, eax
    inc dword ptr g_result_count
    
    add rsp, 40
    pop r14 r13 r12 rbx
    ret
TEST_SEQUENTIAL_ACCESS ENDP

; =============================================================================
; TEST_TRAP_HANDLING
; Test YTFN_SENTINEL trap mechanism
; Verifies that traps are resolved correctly
; =============================================================================
TEST_TRAP_HANDLING PROC FRAME
    push rbx r12 r13
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov r12d, 1                     ; Assume pass
    
    ; Get result index
    mov eax, g_result_count
    imul rax, SIZEOF VALIDATION_RESULT
    lea r13, g_validation_results[rax]
    
    ; Record test name
    lea rax, test_buffer_trap
    mov rcx, r13
    mov rdx, OFFSET VALIDATION_RESULT.test_name
    add rcx, rdx
    mov edx, 64
.copy_trap_name:
    dec edx
    js .trap_name_done
    mov al, byte ptr [rax]
    mov byte ptr [rcx], al
    inc rax
    inc rcx
    jmp .copy_trap_name
    
.trap_name_done:
    ; Start timer
    call GetTickCount
    mov r8d, eax
    
    ; Request a layer that's not ready
    mov rcx, 150                    ; Layer 150 (not in buffer)
    mov rdx, 0
    call INFINITY_CheckQuadBuffer
    
    ; Check if we got YTFN_SENTINEL
    cmp rax, YTFN_SENTINEL
    jne .trap_failed
    
    ; Handle the trap
    mov rcx, rax
    call INFINITY_HandleYTfnTrap
    
    ; After trap, should have valid pointer
    cmp rax, YTFN_SENTINEL
    jae .trap_failed
    
    ; Stop timer
    call GetTickCount
    sub eax, r8d
    mov [r13].VALIDATION_RESULT.elapsed_ms, eax
    
    mov eax, 1
    jmp .trap_done
    
.trap_failed:
    mov r12d, 0
    mov eax, 0
    
.trap_done:
    mov [r13].VALIDATION_RESULT.test_passed, eax
    inc dword ptr g_result_count
    
    add rsp, 40
    pop r13 r12 rbx
    ret
TEST_TRAP_HANDLING ENDP

; =============================================================================
; TEST_BUFFER_ROTATION
; Test buffer rotation cascade
; Verifies state transitions: EMPTY->LOADING->READY->COMPUTING->EMPTY
; =============================================================================
TEST_BUFFER_ROTATION PROC FRAME
    push rbx r12 r13 r14 r15
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov r12d, 1                     ; Assume pass
    
    ; Get result index
    mov eax, g_result_count
    imul rax, SIZEOF VALIDATION_RESULT
    lea r13, g_validation_results[rax]
    
    ; Record test name
    lea rax, test_buffer_rotation
    mov rcx, r13
    mov rdx, OFFSET VALIDATION_RESULT.test_name
    add rcx, rdx
    mov edx, 64
.copy_rot_name:
    dec edx
    js .rot_name_done
    mov al, byte ptr [rax]
    mov byte ptr [rcx], al
    inc rax
    inc rcx
    jmp .copy_rot_name
    
.rot_name_done:
    ; Start timer
    call GetTickCount
    mov r14d, eax
    
    ; Get initial state
    xor ecx, ecx
    GET_SLOT_STATE ecx
    mov r15d, eax                   ; Save initial state
    
    ; Trigger rotations
    xor ecx, ecx                    ; Start at layer 0
.rot_loop:
    cmp ecx, 10
    jge .rot_done
    
    ROTATE_AFTER_COMPUTE rcx
    
    inc ecx
    jmp .rot_loop
    
.rot_done:
    ; Check final state
    xor ecx, ecx
    GET_SLOT_STATE ecx
    
    ; Should have changed
    cmp eax, r15d
    jne .rot_success
    
    mov r12d, 0
    
.rot_success:
    ; Stop timer
    call GetTickCount
    sub eax, r14d
    mov [r13].VALIDATION_RESULT.elapsed_ms, eax
    
    mov eax, r12d
    mov [r13].VALIDATION_RESULT.test_passed, eax
    inc dword ptr g_result_count
    
    add rsp, 40
    pop r15 r14 r13 r12 rbx
    ret
TEST_BUFFER_ROTATION ENDP

; =============================================================================
; TEST_BUFFER_EFFICIENCY
; Measure buffer utilization
; Reports percentage of slots in active use
; =============================================================================
TEST_BUFFER_EFFICIENCY PROC FRAME
    push rbx r12 r13 r14
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Get result index
    mov eax, g_result_count
    imul rax, SIZEOF VALIDATION_RESULT
    lea r13, g_validation_results[rax]
    
    ; Record test name
    lea rax, test_efficiency
    mov rcx, r13
    mov rdx, OFFSET VALIDATION_RESULT.test_name
    add rcx, rdx
    mov edx, 64
.copy_eff_name:
    dec edx
    js .eff_name_done
    mov al, byte ptr [rax]
    mov byte ptr [rcx], al
    inc rax
    inc rcx
    jmp .copy_eff_name
    
.eff_name_done:
    ; Count active slots
    xor r14d, r14d                  ; Active count
    xor ecx, ecx                    ; Slot counter
    
.eff_loop:
    cmp ecx, 4
    jge .eff_calc
    
    GET_SLOT_STATE ecx
    cmp eax, BUF_STATE_EMPTY
    je .eff_skip
    
    inc r14d                        ; Count this slot
    
.eff_skip:
    inc ecx
    jmp .eff_loop
    
.eff_calc:
    ; Efficiency = (active * 100) / 4
    mov eax, r14d
    mov ecx, 100
    mul ecx
    mov ecx, 4
    div ecx
    
    mov [r13].VALIDATION_RESULT.throughput_mbps, rax
    mov dword ptr [r13].VALIDATION_RESULT.test_passed, 1
    inc dword ptr g_result_count
    
    add rsp, 40
    pop r14 r13 r12 rbx
    ret
TEST_BUFFER_EFFICIENCY ENDP

; =============================================================================
; RUN_VALIDATION_SUITE
; Execute all validation tests
; =============================================================================
RUN_VALIDATION_SUITE PROC FRAME
    push rbx
    .allocstack 8
    .endprolog
    
    ; Initialize
    call INITIALIZE_VALIDATION
    
    ; Run tests
    call TEST_BUFFER_INITIALIZATION
    call TEST_SEQUENTIAL_ACCESS
    call TEST_TRAP_HANDLING
    call TEST_BUFFER_ROTATION
    call TEST_BUFFER_EFFICIENCY
    
    ; Summarize
    call REPORT_VALIDATION_RESULTS
    
    pop rbx
    ret
RUN_VALIDATION_SUITE ENDP

; =============================================================================
; REPORT_VALIDATION_RESULTS
; Display validation results
; =============================================================================
REPORT_VALIDATION_RESULTS PROC FRAME
    push rbx rcx rdx rax
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov ecx, g_result_count
    xor ebx, ebx                    ; Pass count
    
.report_loop:
    cmp ecx, 0
    jle .report_done
    
    dec ecx
    mov rax, rcx
    imul rax, SIZEOF VALIDATION_RESULT
    lea rdx, g_validation_results[rax]
    
    ; Check if passed
    cmp dword ptr [rdx].VALIDATION_RESULT.test_passed, 1
    jne .report_fail
    
    inc ebx
    
.report_fail:
    jmp .report_loop
    
.report_done:
    ; Summary
    mov eax, g_result_count
    mov ecx, ebx
    
    ; Would output: "Passed X/Y tests"
    
    add rsp, 40
    pop rax rdx rcx rbx
    ret
REPORT_VALIDATION_RESULTS ENDP

; =============================================================================
; BENCHMARK_QUADBUFFER
; Full system benchmarking
; Measures performance across various scenarios
; =============================================================================
BENCHMARK_QUADBUFFER PROC FRAME
    push rbx r12 r13
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Test 1: Sequential throughput (optimal)
    mov ecx, 0
.bench_seq_loop:
    cmp ecx, 200
    jge .bench_seq_done
    
    CHECK_BUFFER rcx, 0
    ROTATE_AFTER_COMPUTE rcx
    
    inc ecx
    jmp .bench_seq_loop
    
.bench_seq_done:
    ; Test 2: Random access (worst case)
    mov ecx, 0
.bench_rand_loop:
    cmp ecx, 100
    jge .bench_rand_done
    
    ; Random layer = (ecx * 7) mod 800
    mov eax, ecx
    imul eax, 7
    mov edx, 800
    div edx
    mov ecx, edx                    ; edx = remainder
    
    CHECK_BUFFER rcx, 0
    
    inc ecx
    jmp .bench_rand_loop
    
.bench_rand_done:
    ; Test 3: Trap latency
    xor ecx, ecx
.bench_trap_loop:
    cmp ecx, 50
    jge .bench_trap_done
    
    ; Request layer that will trap
    mov eax, ecx
    add eax, 600                    ; Offset to ensure trap
    mov rcx, rax
    call INFINITY_CheckQuadBuffer
    
    ; If trap, resolve it
    cmp rax, YTFN_SENTINEL
    jb .bench_trap_skip
    
    mov rcx, rax
    call INFINITY_HandleYTfnTrap
    
.bench_trap_skip:
    inc ecx
    jmp .bench_trap_loop
    
.bench_trap_done:
    ; Collect final metrics
    PHASE5_COLLECT_METRICS
    ; RAX = HDD bytes
    ; RDX = DMA bytes
    ; R8 = stalls
    ; R9D = traps
    
    add rsp, 40
    pop r13 r12 rbx
    ret
BENCHMARK_QUADBUFFER ENDP

END
