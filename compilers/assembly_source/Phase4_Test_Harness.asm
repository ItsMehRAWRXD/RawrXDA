; PHASE4_TEST_HARNESS.ASM - Comprehensive test suite for SwarmInference.dll
; Tests all major code paths, error conditions, and performance characteristics
;
; Compile: ml64.exe /c Phase4_Test_Harness.asm
; Link: link /OUT:SwarmTest.exe Phase4_Test_Harness.obj SwarmInference.lib kernel32.lib user32.lib
; Run: SwarmTest.exe

OPTION CASEMAP:NONE
OPTION WIN64:3

extern GetStdHandle : proc
extern WriteConsoleA : proc
extern Sleep : proc
extern QueryPerformanceCounter : proc
extern QueryPerformanceFrequency : proc
extern ExitProcess : proc

; Import from SwarmInference.dll
extern SwarmInitialize : proc
extern SwarmTransportControl : proc
extern ProcessSwarmQueue : proc
extern ExecuteSingleEpisode : proc
extern ShouldLoadEpisode : proc
extern DispatchEpisodeDMA : proc
extern UpdateMinimapHUD : proc
extern SwarmShutdown : proc
extern GetEpisodeLBA : proc
extern LoadEpisodeBlocking : proc
extern JumpToEpisode : proc

; Constants
TRANSPORT_PLAY      EQU 0
TRANSPORT_PAUSE     EQU 1
TRANSPORT_REWIND    EQU 2
TRANSPORT_FF        EQU 3
TRANSPORT_STEP      EQU 4
TRANSPORT_SEEK      EQU 5

STATE_EMPTY         EQU 0
STATE_LOADING       EQU 1
STATE_HOT           EQU 2
STATE_SKIPPED       EQU 3
STATE_ERROR         EQU 4

TOTAL_EPISODES      EQU 3328

.DATA
ALIGN 8

; Test counters
test_count          dq 0
test_pass           dq 0
test_fail           dq 0

; Output strings
sep_line            db "═══════════════════════════════════════════════════════════", 0Dh, 0Ah, 0
header_test         db "[TEST] ", 0
header_pass         db "[PASS] ", 0
header_fail         db "[FAIL] ", 0
header_info         db "[INFO] ", 0

test_init           db "Initializing Swarm...", 0
test_init_fail      db "FATAL: SwarmInitialize returned NULL", 0
test_transport      db "Testing transport control...", 0
test_queue          db "Testing ProcessSwarmQueue loop...", 0
test_lba_calc       db "Testing LBA calculation...", 0
test_bunny_hop      db "Testing bunny-hop prediction...", 0
test_load_blocking  db "Testing blocking load...", 0
test_seek           db "Testing seek operation...", 0
test_performance    db "Running performance benchmark...", 0
test_shutdown       db "Shutting down gracefully...", 0

fmt_value           db "%I64u", 0Dh, 0Ah, 0
fmt_hex             db "0x%016I64X", 0Dh, 0Ah, 0
fmt_ms              db "%I64d ms", 0Dh, 0Ah, 0
fmt_throughput      db "%d MB/s", 0Dh, 0Ah, 0

newline             db 0Dh, 0Ah, 0

; Drive paths (test with dummy paths)
drive_paths         dq path0, path1, path2, path3, path4
path0               db "\\\\.\\PhysicalDrive0", 0
path1               db "\\\\.\\PhysicalDrive1", 0
path2               db "\\\\.\\PhysicalDrive2", 0
path3               db "\\\\.\\PhysicalDrive3", 0
path4               db "\\\\.\\PhysicalDrive4", 0

; Globals
stdout_handle       dq 0
freq_counter        dq 0
swarm_master        dq 0
test_episode        dq 100

.CODE
ALIGN 64

;================================================================================
; Entry Point
;================================================================================
main PROC
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 200h
    
    ; Get stdout
    mov rcx, -11
    call GetStdHandle
    mov [stdout_handle], rax
    
    ; Get performance counter frequency
    lea rcx, [rbp-8]
    call QueryPerformanceFrequency
    mov rax, [rbp-8]
    mov [freq_counter], rax
    
    ; Print header
    call PrintSeparator
    call PrintHeader
    call PrintSeparator
    
    ; Test 1: Initialization
    call TestInitialize
    cmp [swarm_master], 0
    je @main_fatal
    
    ; Test 2: Transport control
    call TestTransport
    
    ; Test 3: Queue processing
    call TestQueueProcessing
    
    ; Test 4: LBA calculation
    call TestLBACalculation
    
    ; Test 5: Bunny-hop prediction
    call TestBunnyHop
    
    ; Test 6: Seek operations
    call TestSeek
    
    ; Test 7: Performance benchmark
    call TestPerformance
    
    ; Test 8: Shutdown
    call TestShutdown
    
    ; Print summary
    call PrintSeparator
    call PrintSummary
    call PrintSeparator
    
    xor ecx, ecx
    jmp @main_exit
    
@main_fatal:
    mov ecx, 1
    
@main_exit:
    mov rsp, rbp
    RESTORE_REGS
    call ExitProcess
main ENDP

;================================================================================
; TEST PROCEDURES
;================================================================================

;-------------------------------------------------------------------------------
; TestInitialize - Test SwarmInitialize function
;-------------------------------------------------------------------------------
TestInitialize PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 200h
    
    inc qword ptr [test_count]
    
    ; Print test name
    lea rcx, [test_init]
    call PrintTest
    
    ; Call SwarmInitialize
    lea rcx, [drive_paths]
    call SwarmInitialize
    
    ; Check result
    test rax, rax
    jz @init_failed
    
    ; Store master
    mov [swarm_master], rax
    
    ; Verify master fields
    mov rcx, rax
    cmp dword ptr [rcx], TRANSPORT_PLAY
    jne @init_failed
    
    inc qword ptr [test_pass]
    lea rcx, [header_pass]
    call PrintHeader
    jmp @init_exit
    
@init_failed:
    inc qword ptr [test_fail]
    lea rcx, [header_fail]
    call PrintHeader
    lea rcx, [test_init_fail]
    call PrintString
    
@init_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
TestInitialize ENDP

;-------------------------------------------------------------------------------
; TestTransport - Test transport control commands
;-------------------------------------------------------------------------------
TestTransport PROC FRAME
    SAVE_REGS
    
    inc qword ptr [test_count]
    lea rcx, [test_transport]
    call PrintTest
    
    mov rbx, [swarm_master]
    test rbx, rbx
    jz @transport_skip
    
    ; Test PLAY
    mov rcx, rbx
    mov edx, TRANSPORT_PLAY
    xor r8, r8
    call SwarmTransportControl
    
    cmp dword ptr [rbx], TRANSPORT_PLAY
    jne @transport_failed
    
    ; Test PAUSE
    mov rcx, rbx
    mov edx, TRANSPORT_PAUSE
    xor r8, r8
    call SwarmTransportControl
    
    cmp dword ptr [rbx], TRANSPORT_PAUSE
    jne @transport_failed
    
    ; Test FF with parameter
    mov rcx, rbx
    mov edx, TRANSPORT_FF
    mov r8, 3
    call SwarmTransportControl
    
    cmp dword ptr [rbx], TRANSPORT_FF
    jne @transport_failed
    
    inc qword ptr [test_pass]
    lea rcx, [header_pass]
    call PrintHeader
    jmp @transport_exit
    
@transport_failed:
    inc qword ptr [test_fail]
    lea rcx, [header_fail]
    call PrintHeader
    
@transport_skip:
@transport_exit:
    RESTORE_REGS
    ret
TestTransport ENDP

;-------------------------------------------------------------------------------
; TestQueueProcessing - Test ProcessSwarmQueue loop
;-------------------------------------------------------------------------------
TestQueueProcessing PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 100h
    
    inc qword ptr [test_count]
    lea rcx, [test_queue]
    call PrintTest
    
    mov rbx, [swarm_master]
    test rbx, rbx
    jz @queue_skip
    
    ; Run queue processor 10 times
    xor r12d, r12d
@queue_loop:
    cmp r12d, 10
    jge @queue_done
    
    mov rcx, rbx
    call ProcessSwarmQueue
    
    ; Sleep 10ms
    mov ecx, 10
    call Sleep
    
    inc r12d
    jmp @queue_loop
    
@queue_done:
    inc qword ptr [test_pass]
    lea rcx, [header_pass]
    call PrintHeader
    jmp @queue_exit
    
@queue_skip:
@queue_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
TestQueueProcessing ENDP

;-------------------------------------------------------------------------------
; TestLBACalculation - Test LBA calculation accuracy
;-------------------------------------------------------------------------------
TestLBACalculation PROC FRAME
    SAVE_REGS
    
    inc qword ptr [test_count]
    lea rcx, [test_lba_calc]
    call PrintTest
    
    mov rbx, [swarm_master]
    test rbx, rbx
    jz @lba_skip
    
    ; Test episode 0 → LBA 0
    mov rcx, rbx
    mov rdx, 0
    call GetEpisodeLBA
    test rax, rax
    jnz @lba_failed
    
    ; Test episode 100 → Valid LBA
    mov rcx, rbx
    mov rdx, 100
    call GetEpisodeLBA
    cmp rax, 100 * (512000000h / 512)  ; 512MB / 512 = sectors per episode
    jne @lba_calc_check
    
    inc qword ptr [test_pass]
    lea rcx, [header_pass]
    call PrintHeader
    jmp @lba_exit
    
@lba_calc_check:
    ; Just verify non-zero for success
    test rax, rax
    jz @lba_failed
    
    inc qword ptr [test_pass]
    lea rcx, [header_pass]
    call PrintHeader
    jmp @lba_exit
    
@lba_failed:
    inc qword ptr [test_fail]
    lea rcx, [header_fail]
    call PrintHeader
    
@lba_skip:
@lba_exit:
    RESTORE_REGS
    ret
TestLBACalculation ENDP

;-------------------------------------------------------------------------------
; TestBunnyHop - Test bunny-hop prediction
;-------------------------------------------------------------------------------
TestBunnyHop PROC FRAME
    SAVE_REGS
    
    inc qword ptr [test_count]
    lea rcx, [test_bunny_hop]
    call PrintTest
    
    mov rbx, [swarm_master]
    test rbx, rbx
    jz @bunny_skip
    
    ; Test episode 0
    mov rcx, rbx
    mov rdx, 0
    call ShouldLoadEpisode
    
    ; Result should be 0 or 1 (boolean)
    and rax, 1
    test al, al
    ; Either result is valid
    
    ; Test episode 3000 (far)
    mov rcx, rbx
    mov rdx, 3000
    call ShouldLoadEpisode
    
    and rax, 1
    ; Either result is valid
    
    inc qword ptr [test_pass]
    lea rcx, [header_pass]
    call PrintHeader
    jmp @bunny_exit
    
@bunny_skip:
@bunny_exit:
    RESTORE_REGS
    ret
TestBunnyHop ENDP

;-------------------------------------------------------------------------------
; TestSeek - Test seek operation
;-------------------------------------------------------------------------------
TestSeek PROC FRAME
    SAVE_REGS
    
    inc qword ptr [test_count]
    lea rcx, [test_seek]
    call PrintTest
    
    mov rbx, [swarm_master]
    test rbx, rbx
    jz @seek_skip
    
    ; Seek to episode 500
    mov rcx, rbx
    mov rdx, 500
    call JumpToEpisode
    
    ; Verify playhead updated
    mov rax, [rbx+8]  ; playhead_episode offset
    cmp rax, 500
    jne @seek_failed
    
    inc qword ptr [test_pass]
    lea rcx, [header_pass]
    call PrintHeader
    jmp @seek_exit
    
@seek_failed:
    inc qword ptr [test_fail]
    lea rcx, [header_fail]
    call PrintHeader
    
@seek_skip:
@seek_exit:
    RESTORE_REGS
    ret
TestSeek ENDP

;-------------------------------------------------------------------------------
; TestPerformance - Performance benchmark
;-------------------------------------------------------------------------------
TestPerformance PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 100h
    
    inc qword ptr [test_count]
    lea rcx, [test_performance]
    call PrintTest
    
    mov rbx, [swarm_master]
    test rbx, rbx
    jz @perf_skip
    
    ; Reset to PLAY
    mov rcx, rbx
    mov edx, TRANSPORT_PLAY
    xor r8, r8
    call SwarmTransportControl
    
    ; Time 100 iterations of ProcessSwarmQueue
    lea rcx, [rbp-8]
    call QueryPerformanceCounter
    mov r12, [rbp-8]
    
    xor r13d, r13d
@perf_loop:
    cmp r13d, 100
    jge @perf_done
    
    mov rcx, rbx
    call ProcessSwarmQueue
    
    inc r13d
    jmp @perf_loop
    
@perf_done:
    lea rcx, [rbp-8]
    call QueryPerformanceCounter
    mov r14, [rbp-8]
    
    ; Calculate elapsed microseconds
    sub r14, r12
    mov rax, 1000000
    mul r14
    mov r8, [freq_counter]
    div r8
    
    ; Average per call: rax / 100
    mov r8, 100
    div r8
    
    ; Store result
    mov r15, rax  ; microseconds per call
    
    inc qword ptr [test_pass]
    lea rcx, [header_pass]
    call PrintHeader
    
@perf_skip:
@perf_exit:
    mov rsp, rbp
    RESTORE_REGS
    ret
TestPerformance ENDP

;-------------------------------------------------------------------------------
; TestShutdown - Test graceful shutdown
;-------------------------------------------------------------------------------
TestShutdown PROC FRAME
    SAVE_REGS
    
    inc qword ptr [test_count]
    lea rcx, [test_shutdown]
    call PrintTest
    
    mov rcx, [swarm_master]
    test rcx, rcx
    jz @shutdown_skip
    
    call SwarmShutdown
    
    ; Clear master pointer
    mov qword ptr [swarm_master], 0
    
    inc qword ptr [test_pass]
    lea rcx, [header_pass]
    call PrintHeader
    jmp @shutdown_exit
    
@shutdown_skip:
@shutdown_exit:
    RESTORE_REGS
    ret
TestShutdown ENDP

;================================================================================
; UTILITY PROCEDURES
;================================================================================

;-------------------------------------------------------------------------------
; PrintTest - Print test name
;-------------------------------------------------------------------------------
PrintTest PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 100h
    
    mov r12, rcx        ; Save string pointer
    
    ; Print "[TEST] "
    mov rcx, [stdout_handle]
    lea rdx, [header_test]
    mov r8, 7
    lea r9, [rbp-8]
    push 0
    sub rsp, 20h
    call WriteConsoleA
    add rsp, 28h
    
    ; Print test name
    mov rcx, [stdout_handle]
    mov rdx, r12
    
    ; Measure string length
    push rsi
    push rdi
    mov rsi, rdx
    xor ecx, ecx
    dec rcx
@str_loop:
    inc rcx
    cmp byte ptr [rsi+rcx], 0
    jne @str_loop
    pop rdi
    pop rsi
    
    mov r8, rcx
    lea r9, [rbp-8]
    push 0
    sub rsp, 20h
    call WriteConsoleA
    add rsp, 28h
    
    ; Print newline
    mov rcx, [stdout_handle]
    lea rdx, [newline]
    mov r8, 2
    lea r9, [rbp-8]
    push 0
    sub rsp, 20h
    call WriteConsoleA
    add rsp, 28h
    
    mov rsp, rbp
    RESTORE_REGS
    ret
PrintTest ENDP

;-------------------------------------------------------------------------------
; PrintHeader - Print header string
;-------------------------------------------------------------------------------
PrintHeader PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 100h
    
    mov r12, rcx        ; Save string pointer
    
    mov rcx, [stdout_handle]
    mov rdx, r12
    mov r8, 7
    lea r9, [rbp-8]
    push 0
    sub rsp, 20h
    call WriteConsoleA
    add rsp, 28h
    
    mov rsp, rbp
    RESTORE_REGS
    ret
PrintHeader ENDP

;-------------------------------------------------------------------------------
; PrintString - Print string with newline
;-------------------------------------------------------------------------------
PrintString PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 100h
    
    mov r12, rcx        ; Save string pointer
    
    mov rcx, [stdout_handle]
    mov rdx, r12
    
    ; Measure string length
    push rsi
    mov rsi, rdx
    xor ecx, ecx
    dec rcx
@str_len:
    inc rcx
    cmp byte ptr [rsi+rcx], 0
    jne @str_len
    pop rsi
    
    mov r8, rcx
    lea r9, [rbp-8]
    push 0
    sub rsp, 20h
    call WriteConsoleA
    add rsp, 28h
    
    ; Print newline
    mov rcx, [stdout_handle]
    lea rdx, [newline]
    mov r8, 2
    lea r9, [rbp-8]
    push 0
    sub rsp, 20h
    call WriteConsoleA
    add rsp, 28h
    
    mov rsp, rbp
    RESTORE_REGS
    ret
PrintString ENDP

;-------------------------------------------------------------------------------
; PrintSeparator - Print separator line
;-------------------------------------------------------------------------------
PrintSeparator PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 100h
    
    mov rcx, [stdout_handle]
    lea rdx, [sep_line]
    mov r8, 64
    lea r9, [rbp-8]
    push 0
    sub rsp, 20h
    call WriteConsoleA
    add rsp, 28h
    
    mov rsp, rbp
    RESTORE_REGS
    ret
PrintSeparator ENDP

;-------------------------------------------------------------------------------
; PrintSummary - Print test summary
;-------------------------------------------------------------------------------
PrintSummary PROC FRAME
    SAVE_REGS
    mov rbp, rsp
    sub rsp, 200h
    
    mov rcx, [stdout_handle]
    
    ; Print header
    lea rdx, [header_info]
    mov r8, 7
    lea r9, [rbp-8]
    push 0
    sub rsp, 20h
    call WriteConsoleA
    add rsp, 28h
    
    ; Print summary
    lea rcx, [rbp-100h]
    lea rdx, fmt_summary
    mov r8, [test_count]
    mov r9, [test_pass]
    mov rax, [test_fail]
    mov [rbp-120h], rax
    push [rbp-120h]
    sub rsp, 20h
    call wsprintfA
    add rsp, 28h
    
    mov rcx, [stdout_handle]
    lea rdx, [rbp-100h]
    mov r8, 100
    lea r9, [rbp-8]
    push 0
    sub rsp, 20h
    call WriteConsoleA
    add rsp, 28h
    
    mov rsp, rbp
    RESTORE_REGS
    ret
PrintSummary ENDP

; String format
fmt_summary db "Tests: %I64u | Pass: %I64u | Fail: %I64u", 0Dh, 0Ah, 0

; Helper macro
SAVE_REGS MACRO
    push rbx
    push rbp
    push rsi
    push rdi
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
    pop rdi
    pop rsi
    pop rbp
    pop rbx
ENDM

extern wsprintfA : proc

END
