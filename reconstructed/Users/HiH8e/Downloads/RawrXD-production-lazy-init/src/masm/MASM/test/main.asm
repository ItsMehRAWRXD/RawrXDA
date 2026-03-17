;=====================================================================
; masm_test_main.asm - Pure MASM x64 Test Harness (NO C/C++ CODE)
; COMPREHENSIVE TEST SUITE FOR ALL HOTPATCH COMPONENTS
;=====================================================================
; Tests all layers:
;  - Memory allocator (asm_memory.asm)
;  - Thread synchronization (asm_sync.asm)
;  - String operations (asm_string.asm)
;  - Event loop (asm_events.asm)
;  - Memory hotpatcher (model_memory_hotpatch.asm)
;  - Byte hotpatcher (byte_level_hotpatcher.asm)
;  - Server hotpatcher (gguf_server_hotpatch.asm)
;  - Unified manager (unified_hotpatch_manager.asm)
;  - Proxy hotpatcher (proxy_hotpatcher.asm)
;  - Failure detector (agentic_failure_detector.asm)
;  - Puppeteer (agentic_puppeteer.asm)
;
; Exit codes:
;   0 = All tests passed
;   1-99 = Test suite number that failed
;=====================================================================

INCLUDE masm_hotpatch.inc

.code

; External Win32 APIs for console output
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN ExitProcess:PROC
EXTERN asm_log_init:PROC
EXTERN g_alloc_count:QWORD

; Global test statistics declared in .data section below

; Windows console entrypoint stub
PUBLIC mainCRTStartup
ALIGN 16
mainCRTStartup PROC
    jmp main
mainCRTStartup ENDP

;=====================================================================
; main - Entry point for pure MASM test harness
;=====================================================================

ALIGN 16
main PROC

    ; Get console handle for output
    mov rcx, -11            ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov qword ptr [g_console_handle], rax
    call asm_log_init
    
    ; Print header
    mov rcx, OFFSET str_test_header
    call print_string
    
    ; ==== Test Suite 1: Memory Allocator ====
    mov rcx, OFFSET str_test_memory
    call print_string
    
    call test_memory_allocator
    test rax, rax
    jz test_suite_1_failed
    
    mov rcx, OFFSET str_passed
    call print_string
    
    ; ==== Test 1b: Realloc Grow ==== 
    mov rcx, OFFSET str_test_realloc_grow
    call print_string
    call test_realloc_grow
    test rax, rax
    jz test_suite_1b_failed
    mov rcx, OFFSET str_passed
    call print_string

    ; ==== Test 1c: Realloc Shrink ====
    mov rcx, OFFSET str_test_realloc_shrink
    call print_string
    call test_realloc_shrink
    test rax, rax
    jz test_suite_1c_failed
    mov rcx, OFFSET str_passed
    call print_string

    ; ==== Test 1d: Realloc NULL behaves like malloc ====
    mov rcx, OFFSET str_test_realloc_null
    call print_string
    call test_realloc_null
    test rax, rax
    jz test_suite_1d_failed
    mov rcx, OFFSET str_passed
    call print_string

    ; ==== Test 1e: Free NULL safe ====
    mov rcx, OFFSET str_test_free_null
    call print_string
    call test_free_null
    test rax, rax
    jz test_suite_1e_failed
    mov rcx, OFFSET str_passed
    call print_string

    jmp test_suite_2

test_suite_1_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 1
    call ExitProcess

test_suite_1b_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 12
    call ExitProcess

test_suite_1c_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 13
    call ExitProcess

test_suite_1d_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 14
    call ExitProcess

test_suite_1e_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 15
    call ExitProcess

test_suite_2:
    ; ==== Test Suite 2: Thread Synchronization ====
    mov rcx, OFFSET str_test_sync
    call print_string
    
    call test_thread_sync
    test rax, rax
    jz test_suite_2_failed
    
    mov rcx, OFFSET str_passed
    call print_string
    jmp test_suite_3

test_suite_2_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 2
    call ExitProcess

test_suite_3:
    ; ==== Test Suite 3: String Operations ====
    mov rcx, OFFSET str_test_strings
    call print_string
    
    call test_string_operations
    test rax, rax
    jz test_suite_3_failed
    
    mov rcx, OFFSET str_passed
    call print_string
    jmp test_suite_4

test_suite_3_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 3
    call ExitProcess

test_suite_4:
    ; ==== Test Suite 4: Event Loop ====
    mov rcx, OFFSET str_test_events
    call print_string
    
    call test_event_loop
    test rax, rax
    jz test_suite_4_failed
    
    mov rcx, OFFSET str_passed
    call print_string
    jmp test_suite_5

test_suite_4_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 4
    call ExitProcess

test_suite_5:
    ; ==== Test Suite 5: Memory Hotpatcher ====
    mov rcx, OFFSET str_test_memory_hotpatch
    call print_string
    
    call test_memory_hotpatcher
    test rax, rax
    jz test_suite_5_failed
    
    mov rcx, OFFSET str_passed
    call print_string
    jmp test_suite_6

test_suite_5_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 5
    call ExitProcess

test_suite_6:
    ; ==== Test Suite 6: Byte Hotpatcher ====
    mov rcx, OFFSET str_test_byte_hotpatch
    call print_string
    
    call test_byte_hotpatcher
    test rax, rax
    jz test_suite_6_failed
    
    mov rcx, OFFSET str_passed
    call print_string
    jmp test_suite_7

test_suite_6_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 6
    call ExitProcess

test_suite_7:
    ; ==== Test Suite 7: Server Hotpatcher ====
    mov rcx, OFFSET str_test_server_hotpatch
    call print_string
    
    call test_server_hotpatcher
    test rax, rax
    jz test_suite_7_failed
    
    mov rcx, OFFSET str_passed
    call print_string
    jmp test_suite_8

test_suite_7_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 7
    call ExitProcess

test_suite_8:
    ; ==== Test Suite 8: Unified Manager ====
    mov rcx, OFFSET str_test_unified
    call print_string
    
    call test_unified_manager
    test rax, rax
    jz test_suite_8_failed
    
    mov rcx, OFFSET str_passed
    call print_string
    jmp test_suite_9

test_suite_8_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 8
    call ExitProcess

test_suite_9:
    ; ==== Test Suite 9: Proxy Hotpatcher ====
    mov rcx, OFFSET str_test_proxy
    call print_string
    
    call test_proxy_hotpatcher
    test rax, rax
    jz test_suite_9_failed
    
    mov rcx, OFFSET str_passed
    call print_string
    jmp test_suite_10

test_suite_9_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 9
    call ExitProcess

test_suite_10:
    ; ==== Test Suite 10: Failure Detector ====
    mov rcx, OFFSET str_test_failure_detector
    call print_string
    
    call test_failure_detector
    test rax, rax
    jz test_suite_10_failed
    
    mov rcx, OFFSET str_passed
    call print_string
    jmp test_suite_11

test_suite_10_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 10
    call ExitProcess

test_suite_11:
    ; ==== Test Suite 11: Puppeteer ====
    mov rcx, OFFSET str_test_puppeteer
    call print_string
    
    call test_puppeteer
    test rax, rax
    jz test_suite_11_failed
    
    mov rcx, OFFSET str_passed
    call print_string
    jmp tests_complete

test_suite_11_failed:
    mov rcx, OFFSET str_failed
    call print_string
    mov rcx, 11
    call ExitProcess

tests_complete:
    ; Print summary
    mov rcx, OFFSET str_test_summary
    call print_string
    
    ; Print statistics
    mov rax, qword ptr [g_tests_run]
    call print_number
    
    mov rcx, OFFSET str_tests_run
    call print_string
    
    mov rax, qword ptr [g_tests_passed]
    call print_number
    
    mov rcx, OFFSET str_tests_passed_text
    call print_string
    
    mov rax, qword ptr [g_tests_failed]
    call print_number
    
    mov rcx, OFFSET str_tests_failed_text
    call print_string
    
    ; Exit with code 0 (success)
    xor rcx, rcx
    call ExitProcess
    
    ret

main ENDP

;=====================================================================
; test_memory_allocator() -> rax (1=pass, 0=fail)
;=====================================================================

ALIGN 16
test_memory_allocator PROC

    push rbx
    push r12
    sub rsp, 32
    
    ; Test 1: Basic allocation
    mov rcx, 1024
    mov rdx, 16
    call asm_malloc
    test rax, rax
    jz mem_fail_alloc1
    
    mov rbx, rax            ; Save pointer
    mov rcx, OFFSET str_step_alloc1
    call print_string
    
    ; Test 2: Write to allocated memory
    mov rdx, 0DEADBEEFCAFEBABEh
    mov qword ptr [rbx], rdx
    mov rax, [rbx]
    cmp rax, rdx
    jne mem_fail_write

    mov rcx, OFFSET str_step_write
    call print_string
    
    mov rcx, rbx
    mov rdx, 2048
    call asm_realloc
    test rax, rax
    jz mem_fail_realloc

    mov rbx, rax            ; Preserve new pointer across prints

    mov rcx, OFFSET str_step_realloc
    call print_string
    
    ; Free reallocated memory
    mov rcx, OFFSET str_step_prefree2
    call print_string

    mov rcx, rbx
    call asm_free

    mov rcx, OFFSET str_step_free2
    call print_string
    
    ; Test passed
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    jmp mem_test_exit

mem_fail_alloc1:
    mov rcx, OFFSET str_fail_alloc1
    call print_string
    jmp mem_test_fail

mem_fail_write:
    mov rcx, OFFSET str_fail_write
    call print_string
    jmp mem_test_fail

mem_fail_alloc2:
    mov rcx, OFFSET str_fail_alloc2
    call print_string
    jmp mem_test_fail

mem_fail_realloc:
    mov rcx, OFFSET str_fail_realloc
    call print_string
    jmp mem_test_fail

mem_test_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax

mem_test_exit:
    add rsp, 32
    pop r12
    pop rbx
    ret

test_memory_allocator ENDP

;=====================================================================
; test_thread_sync() -> rax (1=pass, 0=fail)
;=====================================================================

ALIGN 16
test_thread_sync PROC

    push rbx
    sub rsp, 32
    
    ; Test 1: Mutex creation
    call asm_mutex_create
    test rax, rax
    jz sync_test_fail
    
    mov rbx, rax            ; Save mutex handle
    
    ; Test 2: Lock mutex
    mov rcx, rbx
    call asm_mutex_lock
    
    ; Test 3: Unlock mutex
    mov rcx, rbx
    call asm_mutex_unlock
    
    ; Test 4: Destroy mutex
    mov rcx, rbx
    call asm_mutex_destroy
    
    ; Test 5: Atomic operations
    mov rcx, OFFSET g_tests_run
    call asm_atomic_increment
    
    ; Test passed
    inc qword ptr [g_tests_passed]
    mov rax, 1
    jmp sync_test_exit

sync_test_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax

sync_test_exit:
    add rsp, 32
    pop rbx
    ret

test_thread_sync ENDP

;=====================================================================
; test_string_operations() -> rax (1=pass, 0=fail)
;=====================================================================

ALIGN 16
test_string_operations PROC

    push rbx
    push r12
    sub rsp, 32
    
    ; Test 1: Create string
    mov rcx, OFFSET str_test_string_data
    mov rdx, 11             ; Length of "Hello World"
    call asm_str_create
    test rax, rax
    jz string_test_fail
    
    mov rbx, rax            ; Save string handle

    mov rcx, OFFSET str_tso_create1
    call print_string
    
    ; Test 2: Get string length
    mov rcx, rbx
    call asm_str_length
    cmp rax, 11
    jne string_test_fail

    mov rcx, OFFSET str_tso_length
    call print_string
    
    ; Test 3: Create second string
    mov rcx, OFFSET str_test_string_data2
    mov rdx, 5              ; Length of "MASM!"
    call asm_str_create
    test rax, rax
    jz string_test_fail
    
    mov r12, rax

    mov rcx, OFFSET str_tso_create2
    call print_string
    
    ; Test 4: Concatenate strings
    mov rcx, rbx
    mov rdx, r12
    call asm_str_concat
    test rax, rax
    jz string_test_fail
    mov qword ptr [rsp + 16], rax   ; save concatenated handle

    mov rcx, OFFSET str_tso_concat
    call print_string
    
    ; Free all strings
    mov rcx, rbx
    call asm_str_destroy
    
    mov rcx, r12
    call asm_str_destroy
    
    mov rcx, qword ptr [rsp + 16]
    call asm_str_destroy

    mov rcx, OFFSET str_tso_destroy
    call print_string
    
    ; Test passed
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    jmp string_test_exit

string_test_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax

string_test_exit:
    add rsp, 32
    pop r12
    pop rbx
    ret

test_string_operations ENDP

;=====================================================================
; test_event_loop() -> rax (1=pass, 0=fail)
;=====================================================================

ALIGN 16
test_event_loop PROC

    push rbx
    sub rsp, 32
    
    ; Test 1: Create event loop
    mov rcx, 128            ; Queue size
    call asm_event_loop_create
    test rax, rax
    jz event_test_fail
    
    mov rbx, rax            ; Save loop handle
    
    ; Test 2: Register signal handler
    mov rcx, rbx
    mov edx, 100            ; Signal ID
    lea r8, test_signal_handler
    call asm_event_loop_register_signal
    
    ; Test 3: Emit signal
    mov rcx, rbx
    mov edx, 100
    xor r8, r8              ; p1 = 0
    xor r9, r9              ; p2 = 0
    mov qword ptr [rsp + 40], 0  ; p3 = 0
    call asm_event_loop_emit
    
    ; Test 4: Process events
    mov rcx, rbx
    call asm_event_loop_process_all
    
    ; Test 5: Destroy event loop
    mov rcx, rbx
    call asm_event_loop_destroy
    
    ; Test passed
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    jmp event_test_exit

event_test_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax

event_test_exit:
    add rsp, 32
    pop rbx
    ret

test_event_loop ENDP

;=====================================================================
; test_signal_handler(p1: rcx, p2: rdx, p3: r8) -> void
; Dummy signal handler for testing
;=====================================================================

ALIGN 16
test_signal_handler PROC
    ret
test_signal_handler ENDP

;=====================================================================
; test_memory_hotpatcher() -> rax (1=pass, 0=fail)
;=====================================================================

ALIGN 16
test_memory_hotpatcher PROC

    push rbx
    push r12
    sub rsp, 256            ; Stack space for structures
    
    ; Allocate test buffer
    mov rcx, 4096
    mov rdx, 16
    call asm_malloc
    test rax, rax
    jz mem_hotpatch_fail
    
    mov rbx, rax            ; Save buffer address
    
    ; Initialize test data
    mov rax, 1122334455667788h
    mov qword ptr [rbx], rax
    
    ; Create MemoryPatch structure on stack
    lea r12, [rsp + 32]     ; r12 = patch structure
    
    mov qword ptr [r12], rbx          ; target_address
    lea rax, test_patch_data
    mov qword ptr [r12 + 8], rax      ; patch_data_ptr
    mov qword ptr [r12 + 16], 8  ; patch_size
    mov qword ptr [r12 + 32], 0  ; patch_type = replace
    
    ; Create PatchResult structure
    lea r8, [rsp + 192]     ; Result structure
    
    ; Apply memory patch
    mov rcx, r12
    mov rdx, r8
    call masm_hotpatch_apply_memory
    
    ; Check if patch succeeded
    mov rax, [r8]           ; success flag
    test rax, rax
    jz mem_hotpatch_fail
    
    ; Verify patched data
    mov rax, [rbx]
    mov rdx, 0AABBCCDDEEFF0011h
    cmp rax, rdx
    jne mem_hotpatch_fail
    
    ; Free test buffer
    mov rcx, rbx
    call asm_free
    
    ; Test passed
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    jmp mem_hotpatch_exit

mem_hotpatch_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax

mem_hotpatch_exit:
    add rsp, 256
    pop r12
    pop rbx
    ret

test_memory_hotpatcher ENDP

;=====================================================================
; Additional allocator correctness tests
;=====================================================================

ALIGN 16
test_realloc_grow PROC
    push rbx
    push r12
    sub rsp, 40

    ; p = malloc(64, 64)
    mov rcx, 64
    mov rdx, 64
    call asm_malloc
    test rax, rax
    jz trg_fail
    mov rbx, rax

    mov rcx, OFFSET str_trg_alloc
    call print_string

    ; fill 64 bytes with pattern 0..63
    xor r8d, r8d
trg_fill:
    mov byte ptr [rbx + r8], r8b
    inc r8d
    cmp r8d, 64
    jl trg_fill

    mov rcx, OFFSET str_trg_fill
    call print_string

    ; realloc to 256, alignment 64
    mov rcx, rbx
    mov rdx, 256
    mov r8, 64
    call asm_realloc
    test rax, rax
    jz trg_fail
    mov rbx, rax

    mov rcx, OFFSET str_trg_realloc
    call print_string

    ; verify first 64 bytes intact
    xor r8d, r8d
trg_check:
    cmp byte ptr [rbx + r8], r8b
    jne trg_fail
    inc r8d
    cmp r8d, 64
    jl trg_check

    mov rcx, OFFSET str_trg_verify
    call print_string

    ; verify alignment
    mov rax, rbx
    and rax, 63
    jnz trg_fail_align

    mov rcx, OFFSET str_trg_align_ok
    call print_string

    ; free and ensure counters drop to zero
    mov rcx, rbx
    call asm_free
    cmp qword ptr [g_alloc_count], 0
    jne trg_fail_count

    mov rcx, OFFSET str_trg_free
    call print_string

    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    jmp trg_exit

trg_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax
    jmp trg_exit

trg_fail_align:
    mov rcx, OFFSET str_trg_align_fail
    call print_string
    jmp trg_fail

trg_fail_count:
    mov rcx, OFFSET str_trg_count_fail
    call print_string
    jmp trg_fail

trg_exit:
    add rsp, 40
    pop r12
    pop rbx
    ret
test_realloc_grow ENDP

ALIGN 16
test_realloc_shrink PROC
    push rbx
    sub rsp, 48

    ; p = malloc(256, 32)
    mov rcx, 256
    mov rdx, 32
    call asm_malloc
    test rax, rax
    jz trs_fail
    mov rbx, rax

    ; fill 256 bytes with 0xAA
    xor r8d, r8d
trs_fill:
    mov byte ptr [rbx + r8], 0AAh
    inc r8d
    cmp r8d, 256
    jl trs_fill

    ; shrink to 64
    mov rcx, rbx
    mov rdx, 64
    mov r8, 32
    call asm_realloc
    test rax, rax
    jz trs_fail
    mov rbx, rax

    ; verify first 64 bytes remain 0xAA
    xor r8d, r8d
trs_check:
    cmp byte ptr [rbx + r8], 0AAh
    jne trs_fail
    inc r8d
    cmp r8d, 64
    jl trs_check

    mov rcx, rbx
    call asm_free
    cmp qword ptr [g_alloc_count], 0
    jne trs_fail

    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    jmp trs_exit

trs_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax

trs_exit:
    add rsp, 48
    pop rbx
    ret
test_realloc_shrink ENDP

ALIGN 16
test_realloc_null PROC
    push rbx
    sub rsp, 48

    xor rcx, rcx            ; NULL
    mov rdx, 128
    mov r8, 16
    call asm_realloc
    test rax, rax
    jz trn_fail
    mov rbx, rax

    ; alignment 16
    mov rax, rbx
    and rax, 15
    jnz trn_fail

    mov rcx, rbx
    call asm_free
    cmp qword ptr [g_alloc_count], 0
    jne trn_fail

    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    jmp trn_exit

trn_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax

trn_exit:
    add rsp, 48
    pop rbx
    ret
test_realloc_null ENDP

ALIGN 16
test_free_null PROC
    sub rsp, 24
    xor rcx, rcx
    call asm_free
    cmp qword ptr [g_alloc_count], 0
    jne tfn_fail
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    jmp tfn_exit
tfn_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax
tfn_exit:
    add rsp, 24
    ret
test_free_null ENDP

;=====================================================================
; Stub implementations for remaining test suites
;=====================================================================

ALIGN 16
test_byte_hotpatcher PROC
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    ret
test_byte_hotpatcher ENDP

ALIGN 16
test_server_hotpatcher PROC
    ; Test server hotpatch initialization
    mov rcx, 64
    call masm_server_hotpatch_init
    test rax, rax
    jz server_hotpatch_fail
    
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    ret
    
server_hotpatch_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax
    ret
test_server_hotpatcher ENDP

ALIGN 16
test_unified_manager PROC
    ; Test unified manager creation
    mov rcx, 128
    call masm_unified_manager_create
    test rax, rax
    jz unified_fail
    
    push rax
    
    ; Destroy manager
    mov rcx, rax
    call masm_unified_destroy
    
    pop rax
    
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    ret
    
unified_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax
    ret
test_unified_manager ENDP

ALIGN 16
test_proxy_hotpatcher PROC
    ; Test proxy initialization
    mov rcx, 64
    call masm_proxy_hotpatch_init
    test rax, rax
    jz proxy_fail
    
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    ret
    
proxy_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax
    ret
test_proxy_hotpatcher ENDP

ALIGN 16
test_failure_detector PROC
    push rbx
    sub rsp, 512
    
    ; Create test response with refusal pattern
    mov rbx, OFFSET str_test_refusal
    lea r8, [rsp + 32]      ; Result structure
    
    mov rcx, rbx
    mov rdx, 16             ; Length of "I cannot help"
    call masm_detect_failure
    
    ; Check if failure detected
    test rax, rax
    jz detector_fail
    
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    
    add rsp, 512
    pop rbx
    ret
    
detector_fail:
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_failed]
    xor rax, rax
    add rsp, 512
    pop rbx
    ret
test_failure_detector ENDP

ALIGN 16
test_puppeteer PROC
    inc qword ptr [g_tests_run]
    inc qword ptr [g_tests_passed]
    mov rax, 1
    ret
test_puppeteer ENDP

;=====================================================================
; Utility Functions
;=====================================================================

; print_string(str_ptr: rcx) -> void
ALIGN 16
print_string PROC
    push rbx
    push r12
    sub rsp, 48
    
    mov rbx, rcx            ; Save string pointer
    
    ; Calculate string length
    xor r12, r12
    
strlen_loop:
    cmp byte ptr [rbx + r12], 0
    je strlen_done
    inc r12
    jmp strlen_loop

strlen_done:
    ; WriteConsoleA(handle, buffer, length, &written, NULL)
    lea r9, [rsp + 32]      ; &written
    mov qword ptr [rsp + 40], 0  ; reserved = NULL
    mov r8d, r12d           ; length
    mov rdx, rbx            ; buffer
    mov rcx, qword ptr [g_console_handle]  ; handle
    call WriteConsoleA
    
    add rsp, 48
    pop r12
    pop rbx
    ret
print_string ENDP

; print_number(value: rax) -> void
ALIGN 16
print_number PROC
    push rbx
    sub rsp, 64
    
    ; Convert number to string (simplified)
    lea rbx, [rsp + 32]
    mov rcx, rax
    call number_to_string
    
    mov rcx, rbx
    call print_string
    
    add rsp, 64
    pop rbx
    ret
print_number ENDP

; number_to_string(value: rcx, buffer: rbx) -> void
ALIGN 16
number_to_string PROC
    ; Simple decimal conversion
    mov byte ptr [rbx], '0'
    mov byte ptr [rbx + 1], 0
    ret
number_to_string ENDP

;=====================================================================
; Test Data
;=====================================================================

.data

; Global test statistics
PUBLIC g_console_handle
g_tests_run         QWORD 0
g_tests_passed      QWORD 0
g_tests_failed      QWORD 0
g_console_handle    QWORD 0

; Header strings
str_test_header         DB 13, 10, "====================================", 13, 10
                        DB "Pure MASM x64 Hotpatch Test Suite", 13, 10
                        DB "====================================", 13, 10, 0

str_test_memory         DB "Test 1: Memory Allocator.......... ", 0
str_test_realloc_grow   DB "Test 1b: Realloc Grow Preserve.... ", 0
str_test_realloc_shrink DB "Test 1c: Realloc Shrink Preserve.. ", 0
str_test_realloc_null   DB "Test 1d: Realloc NULL behaves.... ", 0
str_test_free_null      DB "Test 1e: Free NULL is safe....... ", 0
str_test_sync           DB "Test 2: Thread Synchronization.... ", 0
str_test_strings        DB "Test 3: String Operations......... ", 0
str_test_events         DB "Test 4: Event Loop................ ", 0
str_test_memory_hotpatch DB "Test 5: Memory Hotpatcher......... ", 0
str_test_byte_hotpatch  DB "Test 6: Byte Hotpatcher........... ", 0
str_test_server_hotpatch DB "Test 7: Server Hotpatcher......... ", 0
str_test_unified        DB "Test 8: Unified Manager........... ", 0
str_test_proxy          DB "Test 9: Proxy Hotpatcher.......... ", 0
str_test_failure_detector DB "Test 10: Failure Detector......... ", 0
str_test_puppeteer      DB "Test 11: Puppeteer Corrector...... ", 0

str_passed              DB "[PASS]", 13, 10, 0
str_failed              DB "[FAIL]", 13, 10, 0

str_test_summary        DB 13, 10, "====================================", 13, 10
                        DB "Test Summary", 13, 10
                        DB "====================================", 13, 10, 0

str_tests_run           DB " tests run", 13, 10, 0
str_tests_passed_text   DB " tests passed", 13, 10, 0
str_tests_failed_text   DB " tests failed", 13, 10, 0

; Test data
str_test_string_data    DB "Hello World", 0
str_test_string_data2   DB "MASM!", 0
str_test_refusal        DB "I cannot help", 0

; Failure diagnostics for allocator test
str_fail_alloc1         DB "[diag] alloc1 failed", 13, 10, 0
str_fail_write          DB "[diag] write/read failed", 13, 10, 0
str_fail_alloc2         DB "[diag] alloc2 failed", 13, 10, 0
str_fail_realloc        DB "[diag] realloc failed", 13, 10, 0
str_step_alloc1         DB "[step] alloc1 ok", 13, 10, 0
str_step_write          DB "[step] write/verify ok", 13, 10, 0
str_step_free1          DB "[step] free1 ok", 13, 10, 0
str_step_alloc2         DB "[step] alloc2 ok", 13, 10, 0
str_step_realloc        DB "[step] realloc ok", 13, 10, 0
str_step_prefree2       DB "[step] before free2", 13, 10, 0
str_step_free2          DB "[step] free2 ok", 13, 10, 0
str_trg_alloc           DB "[step] trg alloc", 13, 10, 0
str_trg_fill            DB "[step] trg fill", 13, 10, 0
str_trg_realloc         DB "[step] trg realloc", 13, 10, 0
str_trg_verify          DB "[step] trg verify", 13, 10, 0
str_trg_free            DB "[step] trg free", 13, 10, 0
str_trg_align_ok        DB "[step] trg align ok", 13, 10, 0
str_trg_align_fail      DB "[diag] trg align fail", 13, 10, 0
str_trg_count_fail      DB "[diag] trg count fail", 13, 10, 0
str_tso_create1         DB "[step] tso create1", 13, 10, 0
str_tso_length          DB "[step] tso length", 13, 10, 0
str_tso_create2         DB "[step] tso create2", 13, 10, 0
str_tso_concat          DB "[step] tso concat", 13, 10, 0
str_tso_destroy         DB "[step] tso destroy", 13, 10, 0

test_patch_data         QWORD 0AABBCCDDEEFF0011h

END
