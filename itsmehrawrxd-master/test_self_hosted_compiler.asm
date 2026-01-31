; test_self_hosted_compiler.asm
; Test the self-hosted Eon compiler (Phase 2)
; Comprehensive test suite for self-hosting capabilities

section .data
    ; === Self-Hosted Test Suite Information ===
    test_self_hosted_info        db "Self-Hosted Eon Compiler Test Suite v1.0", 10, 0
    test_self_hosted_capabilities db "Features: Self-compilation tests, Meta-programming tests, Bootstrap tests", 10, 0
    
    ; === Test Results ===
    test_self_hosted_results     resq 1024  ; Test results buffer
    test_self_hosted_count       resq 1     ; Number of tests
    test_self_hosted_passed      resq 1     ; Tests passed
    test_self_hosted_failed      resq 1     ; Tests failed
    test_self_hosted_skipped     resq 1     ; Tests skipped
    test_self_hosted_total_time  resq 1     ; Total test time
    
    ; === Test Categories ===
    TEST_SELF_HOSTING_BOOTSTRAP  equ 0
    TEST_SELF_HOSTING_COMPILATION equ 1
    TEST_SELF_HOSTING_ANALYSIS   equ 2
    TEST_SELF_HOSTING_GENERATION equ 3
    TEST_SELF_HOSTING_VALIDATION equ 4
    TEST_SELF_HOSTING_OPTIMIZATION equ 5
    TEST_SELF_HOSTING_EXECUTION  equ 6
    TEST_SELF_HOSTING_META       equ 7
    TEST_SELF_HOSTING_REFLECTION equ 8
    TEST_SELF_HOSTING_INTROSPECTION equ 9
    TEST_SELF_HOSTING_IMPROVEMENT equ 10
    TEST_SELF_HOSTING_EVOLUTION  equ 11
    
    ; === Test Status ===
    TEST_SELF_HOSTED_STATUS_PASSED equ 0
    TEST_SELF_HOSTED_STATUS_FAILED equ 1
    TEST_SELF_HOSTED_STATUS_SKIPPED equ 2
    TEST_SELF_HOSTED_STATUS_RUNNING equ 3
    TEST_SELF_HOSTED_STATUS_PENDING equ 4
    
    ; === Self-Hosting Test Data ===
    test_self_hosting_iterations resq 256   ; Self-hosting iterations
    test_self_hosting_depths     resq 256   ; Self-hosting depths
    test_self_hosting_times      resq 256   ; Self-hosting times
    test_self_hosting_successes  resq 256   ; Self-hosting successes
    test_self_hosting_failures   resq 256   ; Self-hosting failures
    test_self_hosting_improvements resq 256 ; Self-hosting improvements
    
    ; === Test Statistics ===
    tests_self_hosted_run        resq 1     ; Tests run
    tests_self_hosted_passed_count resq 1   ; Tests passed count
    tests_self_hosted_failed_count resq 1   ; Tests failed count
    tests_self_hosted_skipped_count resq 1  ; Tests skipped count
    tests_self_hosted_total_count resq 1    ; Tests total count
    test_self_hosted_execution_time resq 1  ; Test execution time
    test_self_hosted_memory_usage resq 1    ; Test memory usage
    test_self_hosted_error_count resq 1     ; Test error count
    test_self_hosted_warning_count resq 1   ; Test warning count
    test_self_hosted_bootstrap_success_count resq 1 ; Bootstrap successes
    test_self_hosted_bootstrap_failure_count resq 1 ; Bootstrap failures
    test_self_hosted_self_compilation_success_count resq 1 ; Self-compilation successes
    test_self_hosted_self_compilation_failure_count resq 1 ; Self-compilation failures

section .text
    global test_self_hosted_compiler_init
    global test_self_hosted_compiler_run_all_tests
    global test_self_hosted_compiler_run_bootstrap_tests
    global test_self_hosted_compiler_run_compilation_tests
    global test_self_hosted_compiler_run_analysis_tests
    global test_self_hosted_compiler_run_generation_tests
    global test_self_hosted_compiler_run_validation_tests
    global test_self_hosted_compiler_run_optimization_tests
    global test_self_hosted_compiler_run_execution_tests
    global test_self_hosted_compiler_run_meta_tests
    global test_self_hosted_compiler_run_reflection_tests
    global test_self_hosted_compiler_run_introspection_tests
    global test_self_hosted_compiler_run_improvement_tests
    global test_self_hosted_compiler_run_evolution_tests
    global test_self_hosted_compiler_test_bootstrap_self
    global test_self_hosted_compiler_test_compile_self
    global test_self_hosted_compiler_test_analyze_self
    global test_self_hosted_compiler_test_generate_self
    global test_self_hosted_compiler_test_validate_self
    global test_self_hosted_compiler_test_optimize_self
    global test_self_hosted_compiler_test_execute_self
    global test_self_hosted_compiler_test_meta_compile
    global test_self_hosted_compiler_test_reflect_on_self
    global test_self_hosted_compiler_test_introspect_self
    global test_self_hosted_compiler_test_improve_self
    global test_self_hosted_compiler_test_evolve_self
    global test_self_hosted_compiler_test_self_hosting_depth
    global test_self_hosted_compiler_test_self_hosting_iterations
    global test_self_hosted_compiler_test_self_hosting_performance
    global test_self_hosted_compiler_test_self_hosting_memory
    global test_self_hosted_compiler_test_self_hosting_correctness
    global test_self_hosted_compiler_test_self_hosting_robustness
    global test_self_hosted_compiler_test_self_hosting_scalability
    global test_self_hosted_compiler_record_test_result
    global test_self_hosted_compiler_get_test_results
    global test_self_hosted_compiler_get_statistics
    global test_self_hosted_compiler_cleanup

; === Initialize Self-Hosted Test Suite ===
test_self_hosted_compiler_init:
    push rbp
    mov rbp, rsp
    
    ; Print test suite info
    mov rdi, test_self_hosted_info
    call print_string
    mov rdi, test_self_hosted_capabilities
    call print_string
    
    ; Initialize test state
    mov qword [test_self_hosted_count], 0
    mov qword [test_self_hosted_passed], 0
    mov qword [test_self_hosted_failed], 0
    mov qword [test_self_hosted_skipped], 0
    mov qword [test_self_hosted_total_time], 0
    
    ; Initialize test statistics
    mov qword [tests_self_hosted_run], 0
    mov qword [tests_self_hosted_passed_count], 0
    mov qword [tests_self_hosted_failed_count], 0
    mov qword [tests_self_hosted_skipped_count], 0
    mov qword [tests_self_hosted_total_count], 0
    mov qword [test_self_hosted_execution_time], 0
    mov qword [test_self_hosted_memory_usage], 0
    mov qword [test_self_hosted_error_count], 0
    mov qword [test_self_hosted_warning_count], 0
    mov qword [test_self_hosted_bootstrap_success_count], 0
    mov qword [test_self_hosted_bootstrap_failure_count], 0
    mov qword [test_self_hosted_self_compilation_success_count], 0
    mov qword [test_self_hosted_self_compilation_failure_count], 0
    
    ; Clear test results buffer
    mov rdi, test_self_hosted_results
    mov rsi, 1024 * 8
    call zero_memory
    
    ; Clear test data buffers
    mov rdi, test_self_hosting_iterations
    mov rsi, 256 * 8
    call zero_memory
    
    mov rdi, test_self_hosting_depths
    mov rsi, 256 * 8
    call zero_memory
    
    mov rdi, test_self_hosting_times
    mov rsi, 256 * 8
    call zero_memory
    
    mov rdi, test_self_hosting_successes
    mov rsi, 256 * 8
    call zero_memory
    
    mov rdi, test_self_hosting_failures
    mov rsi, 256 * 8
    call zero_memory
    
    mov rdi, test_self_hosting_improvements
    mov rsi, 256 * 8
    call zero_memory
    
    leave
    ret

; === Run All Tests ===
test_self_hosted_compiler_run_all_tests:
    push rbp
    mov rbp, rsp
    
    ; Run all self-hosting test categories
    call test_self_hosted_compiler_run_bootstrap_tests
    call test_self_hosted_compiler_run_compilation_tests
    call test_self_hosted_compiler_run_analysis_tests
    call test_self_hosted_compiler_run_generation_tests
    call test_self_hosted_compiler_run_validation_tests
    call test_self_hosted_compiler_run_optimization_tests
    call test_self_hosted_compiler_run_execution_tests
    call test_self_hosted_compiler_run_meta_tests
    call test_self_hosted_compiler_run_reflection_tests
    call test_self_hosted_compiler_run_introspection_tests
    call test_self_hosted_compiler_run_improvement_tests
    call test_self_hosted_compiler_run_evolution_tests
    
    ; Get final statistics
    call test_self_hosted_compiler_get_statistics
    
    leave
    ret

; === Run Bootstrap Tests ===
test_self_hosted_compiler_run_bootstrap_tests:
    push rbp
    mov rbp, rsp
    
    ; Run bootstrap tests
    call test_self_hosted_compiler_test_bootstrap_self
    
    leave
    ret

; === Run Compilation Tests ===
test_self_hosted_compiler_run_compilation_tests:
    push rbp
    mov rbp, rsp
    
    ; Run compilation tests
    call test_self_hosted_compiler_test_compile_self
    
    leave
    ret

; === Run Analysis Tests ===
test_self_hosted_compiler_run_analysis_tests:
    push rbp
    mov rbp, rsp
    
    ; Run analysis tests
    call test_self_hosted_compiler_test_analyze_self
    
    leave
    ret

; === Run Generation Tests ===
test_self_hosted_compiler_run_generation_tests:
    push rbp
    mov rbp, rsp
    
    ; Run generation tests
    call test_self_hosted_compiler_test_generate_self
    
    leave
    ret

; === Run Validation Tests ===
test_self_hosted_compiler_run_validation_tests:
    push rbp
    mov rbp, rsp
    
    ; Run validation tests
    call test_self_hosted_compiler_test_validate_self
    
    leave
    ret

; === Run Optimization Tests ===
test_self_hosted_compiler_run_optimization_tests:
    push rbp
    mov rbp, rsp
    
    ; Run optimization tests
    call test_self_hosted_compiler_test_optimize_self
    
    leave
    ret

; === Run Execution Tests ===
test_self_hosted_compiler_run_execution_tests:
    push rbp
    mov rbp, rsp
    
    ; Run execution tests
    call test_self_hosted_compiler_test_execute_self
    
    leave
    ret

; === Run Meta Tests ===
test_self_hosted_compiler_run_meta_tests:
    push rbp
    mov rbp, rsp
    
    ; Run meta tests
    call test_self_hosted_compiler_test_meta_compile
    
    leave
    ret

; === Run Reflection Tests ===
test_self_hosted_compiler_run_reflection_tests:
    push rbp
    mov rbp, rsp
    
    ; Run reflection tests
    call test_self_hosted_compiler_test_reflect_on_self
    
    leave
    ret

; === Run Introspection Tests ===
test_self_hosted_compiler_run_introspection_tests:
    push rbp
    mov rbp, rsp
    
    ; Run introspection tests
    call test_self_hosted_compiler_test_introspect_self
    
    leave
    ret

; === Run Improvement Tests ===
test_self_hosted_compiler_run_improvement_tests:
    push rbp
    mov rbp, rsp
    
    ; Run improvement tests
    call test_self_hosted_compiler_test_improve_self
    
    leave
    ret

; === Run Evolution Tests ===
test_self_hosted_compiler_run_evolution_tests:
    push rbp
    mov rbp, rsp
    
    ; Run evolution tests
    call test_self_hosted_compiler_test_evolve_self
    
    leave
    ret

; === Test Bootstrap Self ===
test_self_hosted_compiler_test_bootstrap_self:
    push rbp
    mov rbp, rsp
    
    ; Initialize self-hosted compiler
    call self_hosted_eon_compiler_init
    
    ; Test bootstrap self
    call self_hosted_eon_compiler_bootstrap_self
    cmp rax, 0
    je .test_failed
    
    ; Test passed
    call test_self_hosted_compiler_record_test_result
    mov rdi, TEST_SELF_HOSTED_STATUS_PASSED
    mov rsi, 0  ; test name
    call test_self_hosted_compiler_record_test_result
    
    ; Update statistics
    inc qword [test_self_hosted_bootstrap_success_count]
    
    jmp .done
    
.test_failed:
    ; Test failed
    call test_self_hosted_compiler_record_test_result
    mov rdi, TEST_SELF_HOSTED_STATUS_FAILED
    mov rsi, 0  ; test name
    call test_self_hosted_compiler_record_test_result
    
    ; Update statistics
    inc qword [test_self_hosted_bootstrap_failure_count]
    
.done:
    ; Cleanup
    call self_hosted_eon_compiler_cleanup
    
    leave
    ret

; === Test Compile Self ===
test_self_hosted_compiler_test_compile_self:
    push rbp
    mov rbp, rsp
    
    ; Initialize self-hosted compiler
    call self_hosted_eon_compiler_init
    
    ; Enable self-hosting
    mov rdi, 1
    call self_hosted_eon_compiler_enable_self_hosting
    
    ; Test compile self
    call self_hosted_eon_compiler_compile_self                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                uccess_count], 0
    mov qword [test_self_hosted_bootstrap_failure_count], 0
    mov qword [test_self_hosted_self_compilation_success_count], 0
    mov qword [test_self_hosted_self_compilation_failure_count], 0
    
    leave
    ret

; === Utility Functions ===
zero_memory:
    push rbp
    mov rbp, rsp
    push rbx
    push rcx
    
    ; Args: rdi = memory pointer, rsi = size
    mov rbx, rdi
    mov rcx, rsi
    
.zero_loop:
    cmp rcx, 0
    je .zero_done
    
    mov byte [rbx], 0
    inc rbx
    dec rcx
    jmp .zero_loop
    
.zero_done:
    pop rcx
    pop rbx
    leave
    ret

print_string:
    push rbp
    mov rbp, rsp
    push rbx
    
    mov rbx, rdi
    mov rdx, 0
    
    ; Calculate string length
.length_loop:
    cmp byte [rbx + rdx], 0
    je .print
    inc rdx
    jmp .length_loop
    
.print:
    mov rax, 1              ; sys_write
    mov rdi, 1              ; stdout
    mov rsi, rbx
    syscall
    
    pop rbx
    leave
    ret

; === Self-Hosted Test Suite Demo ===
test_self_hosted_compiler_demo:
    push rbp
    mov rbp, rsp
    
    ; Initialize test suite
    call test_self_hosted_compiler_init
    
    ; Run all tests
    call test_self_hosted_compiler_run_all_tests
    
    ; Get test results
    call test_self_hosted_compiler_get_test_results
    call test_self_hosted_compiler_get_statistics
    
    ; Cleanup
    call test_self_hosted_compiler_cleanup
    
    leave
    ret
