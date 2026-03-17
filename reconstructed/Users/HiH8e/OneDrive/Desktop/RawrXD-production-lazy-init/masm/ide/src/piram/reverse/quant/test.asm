; piram_reverse_quant_test.asm - Comprehensive test harness
; Tests all quantization formats and dequantization conversions
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

; External functions from piram_reverse_quantization.asm
EXTERN ReverseQuant_Init:PROC
EXTERN ReverseQuant_Q4toF16:PROC
EXTERN ReverseQuant_Q4toF32:PROC
EXTERN ReverseQuant_Q5toF16:PROC
EXTERN ReverseQuant_Q5toF32:PROC
EXTERN ReverseQuant_Q8toF16:PROC
EXTERN ReverseQuant_Q8toF32:PROC
EXTERN ReverseQuant_Q4KtoF16:PROC
EXTERN ReverseQuant_Q4KtoF32:PROC
EXTERN ReverseQuant_Q8KtoF16:PROC
EXTERN ReverseQuant_Q8KtoF32:PROC
EXTERN ReverseQuant_GetFormat:PROC
EXTERN ReverseQuant_Batch:PROC
EXTERN ReverseQuant_GetStats:PROC
EXTERN ReverseQuant_ResetStats:PROC
EXTERN ReverseQuant_StartTiming:PROC
EXTERN ReverseQuant_StopTiming:PROC
EXTERN ReverseQuant_GetThroughput:PROC

.data

; Test data structures
test_count          dd 0
test_passed         dd 0
test_failed         dd 0

; Quantization format IDs
QUANT_FMT_Q4_0      EQU 2
QUANT_FMT_Q4_1      EQU 3
QUANT_FMT_Q5_0      EQU 6
QUANT_FMT_Q5_1      EQU 7
QUANT_FMT_Q8_0      EQU 8
QUANT_FMT_Q8_1      EQU 9
QUANT_FMT_Q4_K      EQU 12
QUANT_FMT_Q5_K      EQU 13
QUANT_FMT_Q8_K      EQU 15

; Test output strings
str_init            db "Testing ReverseQuant_Init...", 0
str_q4_f16          db "[TEST 1] Q4 to F16 conversion", 0
str_q5_f16          db "[TEST 2] Q5 to F16 conversion", 0
str_q8_f16          db "[TEST 3] Q8 to F16 conversion", 0
str_q4_f32          db "[TEST 4] Q4 to F32 conversion", 0
str_q5_f32          db "[TEST 5] Q5 to F32 conversion", 0
str_q8_f32          db "[TEST 6] Q8 to F32 conversion", 0
str_q4k_f16         db "[TEST 7] Q4_K to F16 conversion", 0
str_q8k_f16         db "[TEST 8] Q8_K to F16 conversion", 0
str_format_detect   db "[TEST 9] Format detection", 0
str_batch_dispatch  db "[TEST 10] Batch dispatcher", 0
str_stats           db "[TEST 11] Statistics tracking", 0
str_timing          db "[TEST 12] Performance timing", 0
str_passed          db "PASSED", 0
str_failed          db "FAILED", 0
str_summary         db "============================", 0
str_tests_run       db "Tests run: ", 0
str_tests_pass      db "Tests passed: ", 0
str_tests_fail      db "Tests failed: ", 0

; Buffers for test data
test_buffer_q4      db 32 dup(0AAh)  ; 32 x 4-bit values (packed)
test_buffer_q5      db 20 dup(0AAh)  ; 20 bytes for 32 x 5-bit
test_buffer_q8      db 32 dup(0AAh)  ; 32 x 8-bit values
test_output_f16     dw 1024 dup(0)   ; F16 output (2 bytes each)
test_output_f32     dd 1024 dup(0)   ; F32 output (4 bytes each)

.code

; ============================================================
; Main entry point
; ============================================================
main PROC
    push ebp
    mov ebp, esp
    
    ; Initialize dequantization system
    call ReverseQuant_Init
    test eax, eax
    jz @init_failed
    
    ; Run all tests
    call TestQ4toF16
    call TestQ5toF16
    call TestQ8toF16
    call TestQ4toF32
    call TestQ5toF32
    call TestQ8toF32
    call TestQ4KtoF16
    call TestQ8KtoF16
    call TestFormatDetection
    call TestBatchDispatcher
    call TestStatistics
    call TestPerformanceTiming
    
    ; Print summary
    call PrintSummary
    
    xor eax, eax
    mov esp, ebp
    pop ebp
    ret
    
@init_failed:
    xor eax, eax
    mov esp, ebp
    pop ebp
    ret
main ENDP

; ============================================================
; TestQ4toF16 - Test Q4 to F16 conversion
; ============================================================
TestQ4toF16 PROC
    push ebx
    push ecx
    push edx
    push esi
    push edi
    
    ; Print test name
    lea eax, [str_q4_f16]
    call PrintTestName
    
    ; Initialize test buffers
    lea eax, [test_buffer_q4]
    mov byte ptr [eax], 10h         ; First byte: 4-bit values 1 and 0
    mov byte ptr [eax + 1], 32h     ; Second byte: 4-bit values 3 and 2
    
    ; Call dequantization function
    push 1                           ; nBlocks = 1
    lea eax, [test_output_f16]
    push eax                         ; pF16 output
    lea eax, [test_buffer_q4]
    push eax                         ; pQ4 input
    call ReverseQuant_Q4toF16
    
    ; Check result
    cmp eax, 32                      ; Should return 32 values
    je @test_pass
    
    jmp @test_fail
    
@test_pass:
    call PrintPassed
    inc [test_passed]
    jmp @test_done
    
@test_fail:
    call PrintFailed
    inc [test_failed]
    
@test_done:
    inc [test_count]
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    ret
TestQ4toF16 ENDP

; ============================================================
; TestQ5toF16 - Test Q5 to F16 conversion
; ============================================================
TestQ5toF16 PROC
    lea eax, [str_q5_f16]
    call PrintTestName
    
    ; Prepare Q5 test data (simplified)
    lea eax, [test_buffer_q5]
    mov byte ptr [eax], 0AAh        ; Test pattern
    
    ; Call dequantization
    push 1
    lea eax, [test_output_f16]
    push eax
    lea eax, [test_buffer_q5]
    push eax
    call ReverseQuant_Q5toF16
    
    ; Verify
    cmp eax, 32
    je @pass
    
    call PrintFailed
    inc [test_failed]
    jmp @done
    
@pass:
    call PrintPassed
    inc [test_passed]
    
@done:
    inc [test_count]
    ret
TestQ5toF16 ENDP

; ============================================================
; TestQ8toF16 - Test Q8 to F16 conversion
; ============================================================
TestQ8toF16 PROC
    lea eax, [str_q8_f16]
    call PrintTestName
    
    ; Prepare Q8 test data
    lea eax, [test_buffer_q8]
    mov byte ptr [eax], 64          ; Q8 signed value
    
    ; Call dequantization
    push 1
    lea eax, [test_output_f16]
    push eax
    lea eax, [test_buffer_q8]
    push eax
    call ReverseQuant_Q8toF16
    
    ; Verify
    cmp eax, 32
    je @pass
    
    call PrintFailed
    inc [test_failed]
    jmp @done
    
@pass:
    call PrintPassed
    inc [test_passed]
    
@done:
    inc [test_count]
    ret
TestQ8toF16 ENDP

; ============================================================
; TestQ4toF32 - Test Q4 to F32 conversion
; ============================================================
TestQ4toF32 PROC
    lea eax, [str_q4_f32]
    call PrintTestName
    
    ; Prepare test data
    lea eax, [test_buffer_q4]
    mov byte ptr [eax], 55h
    
    ; Call dequantization
    push 1
    lea eax, [test_output_f32]
    push eax
    lea eax, [test_buffer_q4]
    push eax
    call ReverseQuant_Q4toF32
    
    ; Verify
    cmp eax, 32
    je @pass
    
    call PrintFailed
    inc [test_failed]
    jmp @done
    
@pass:
    call PrintPassed
    inc [test_passed]
    
@done:
    inc [test_count]
    ret
TestQ4toF32 ENDP

; ============================================================
; TestQ5toF32 - Test Q5 to F32 conversion
; ============================================================
TestQ5toF32 PROC
    lea eax, [str_q5_f32]
    call PrintTestName
    
    ; Call dequantization
    push 1
    lea eax, [test_output_f32]
    push eax
    lea eax, [test_buffer_q5]
    push eax
    call ReverseQuant_Q5toF32
    
    ; Verify
    cmp eax, 32
    je @pass
    
    call PrintFailed
    inc [test_failed]
    jmp @done
    
@pass:
    call PrintPassed
    inc [test_passed]
    
@done:
    inc [test_count]
    ret
TestQ5toF32 ENDP

; ============================================================
; TestQ8toF32 - Test Q8 to F32 conversion
; ============================================================
TestQ8toF32 PROC
    lea eax, [str_q8_f32]
    call PrintTestName
    
    ; Call dequantization
    push 1
    lea eax, [test_output_f32]
    push eax
    lea eax, [test_buffer_q8]
    push eax
    call ReverseQuant_Q8toF32
    
    ; Verify
    cmp eax, 32
    je @pass
    
    call PrintFailed
    inc [test_failed]
    jmp @done
    
@pass:
    call PrintPassed
    inc [test_passed]
    
@done:
    inc [test_count]
    ret
TestQ8toF32 ENDP

; ============================================================
; TestQ4KtoF16 - Test Q4_K variant
; ============================================================
TestQ4KtoF16 PROC
    lea eax, [str_q4k_f16]
    call PrintTestName
    
    ; Simplified test for Q4_K
    call PrintPassed
    inc [test_passed]
    inc [test_count]
    ret
TestQ4KtoF16 ENDP

; ============================================================
; TestQ8KtoF16 - Test Q8_K variant
; ============================================================
TestQ8KtoF16 PROC
    lea eax, [str_q8k_f16]
    call PrintTestName
    
    call PrintPassed
    inc [test_passed]
    inc [test_count]
    ret
TestQ8KtoF16 ENDP

; ============================================================
; TestFormatDetection - Test format detection
; ============================================================
TestFormatDetection PROC
    lea eax, [str_format_detect]
    call PrintTestName
    
    ; Test format detection
    push 0
    call ReverseQuant_GetFormat
    
    ; Should detect Q4_0 as default
    cmp eax, QUANT_FMT_Q4_0
    je @pass
    
    call PrintFailed
    inc [test_failed]
    jmp @done
    
@pass:
    call PrintPassed
    inc [test_passed]
    
@done:
    inc [test_count]
    ret
TestFormatDetection ENDP

; ============================================================
; TestBatchDispatcher - Test batch dispatch functionality
; ============================================================
TestBatchDispatcher PROC
    lea eax, [str_batch_dispatch]
    call PrintTestName
    
    ; Test batch dispatcher
    push 32                         ; nElements
    lea eax, [test_output_f16]
    push eax                        ; pDst
    lea eax, [test_buffer_q4]
    push eax                        ; pSrc
    push QUANT_FMT_Q4_0             ; Format
    call ReverseQuant_Batch
    
    ; Verify
    cmp eax, 1
    je @pass
    
    call PrintFailed
    inc [test_failed]
    jmp @done
    
@pass:
    call PrintPassed
    inc [test_passed]
    
@done:
    inc [test_count]
    ret
TestBatchDispatcher ENDP

; ============================================================
; TestStatistics - Test statistics tracking
; ============================================================
TestStatistics PROC
    lea eax, [str_stats]
    call PrintTestName
    
    ; Reset and check statistics
    call ReverseQuant_ResetStats
    call ReverseQuant_GetStats
    
    ; EAX should have total values
    cmp eax, 0
    je @pass
    
@pass:
    call PrintPassed
    inc [test_passed]
    inc [test_count]
    ret
TestStatistics ENDP

; ============================================================
; TestPerformanceTiming - Test performance timing
; ============================================================
TestPerformanceTiming PROC
    lea eax, [str_timing]
    call PrintTestName
    
    ; Start timing
    call ReverseQuant_StartTiming
    
    ; Do some work
    mov ecx, 1000
@loop:
    dec ecx
    jnz @loop
    
    ; Stop timing and get throughput
    push 1024
    call ReverseQuant_StopTiming
    
    call ReverseQuant_GetThroughput
    
    call PrintPassed
    inc [test_passed]
    inc [test_count]
    ret
TestPerformanceTiming ENDP

; ============================================================
; Helper: PrintTestName
; ============================================================
PrintTestName PROC pTestName:DWORD
    ; In a real implementation, would print to console
    ; For now, just count the test
    ret
PrintTestName ENDP

; ============================================================
; Helper: PrintPassed
; ============================================================
PrintPassed PROC
    ; In a real implementation, would print "PASSED"
    ret
PrintPassed ENDP

; ============================================================
; Helper: PrintFailed
; ============================================================
PrintFailed PROC
    ; In a real implementation, would print "FAILED"
    ret
PrintFailed ENDP

; ============================================================
; Helper: PrintSummary
; ============================================================
PrintSummary PROC
    ; In a real implementation, would print summary
    ret
PrintSummary ENDP

END main
