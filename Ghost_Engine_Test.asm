; ==============================================================================
; Ghost_Engine_Test.asm
; Test harness for Predictive Overlay Interface
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

EXTERN ExitProcess : PROC
EXTERN INIT_GHOST_BUFFER : PROC
EXTERN PUSH_GHOST_PREDICTION : PROC
EXTERN RENDER_GHOST_PREDICTIVE : PROC
EXTERN GET_GHOST_LATENCY : PROC
EXTERN GET_GHOST_STATS : PROC
EXTERN SET_CONFIDENCE_THRESHOLD : PROC
EXTERN GET_CONFIDENCE_THRESHOLD : PROC
EXTERN FLUSH_GHOST_BUFFER : PROC
EXTERN GHOST_HEARTBEAT : PROC

.DATA
    ALIGN 16
    ; Test prediction text
    test_text       db "function calculateTotal(items) {", 0
    test_text_len   equ $ - test_text - 1
    
    ; Stats buffer
    ALIGN 16
    stats_buffer    dq 4 dup(0)
    
    ; Test confidence values
    CONFIDENCE_HIGH equ 03F800000h  ; 1.0f
    CONFIDENCE_MED  equ 03F19999Ah  ; 0.6f
    CONFIDENCE_LOW  equ 03E99999Ah  ; 0.3f

.CODE

PUBLIC main
main proc
    sub rsp, 40
    
    xor rbx, rbx                ; Test pass counter
    
    ; Test 1: Initialize ghost buffer
    call INIT_GHOST_BUFFER
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 2: Push high-confidence prediction
    lea rcx, test_text
    mov rdx, test_text_len
    mov r8d, CONFIDENCE_HIGH
    call PUSH_GHOST_PREDICTION
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 3: Get ghost stats (should show 1 slot)
    lea rcx, stats_buffer
    call GET_GHOST_STATS
    test eax, eax
    jz test_fail
    
    mov rax, stats_buffer[16]   ; Active slots
    cmp rax, 1
    jne test_fail
    inc rbx
    
    ; Test 4: Render prediction
    xor ecx, ecx                ; HWND = 0 (test mode)
    call RENDER_GHOST_PREDICTIVE
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 5: Get stats after render (should show 1 rendered)
    lea rcx, stats_buffer
    call GET_GHOST_STATS
    test eax, eax
    jz test_fail
    
    mov rax, stats_buffer[0]    ; Rendered count
    cmp rax, 1
    jne test_fail
    inc rbx
    
    ; Test 6: Push low-confidence prediction (should be dropped)
    lea rcx, test_text
    mov rdx, test_text_len
    mov r8d, CONFIDENCE_LOW
    call PUSH_GHOST_PREDICTION
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 7: Render low-confidence (should drop)
    xor ecx, ecx
    call RENDER_GHOST_PREDICTIVE
    test eax, eax
    jnz test_fail               ; Should return 0 (dropped)
    inc rbx
    
    ; Test 8: Set confidence threshold
    mov ecx, CONFIDENCE_MED
    call SET_CONFIDENCE_THRESHOLD
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 9: Get confidence threshold
    call GET_CONFIDENCE_THRESHOLD
    cmp eax, CONFIDENCE_MED
    jne test_fail
    inc rbx
    
    ; Test 10: Flush buffer
    call FLUSH_GHOST_BUFFER
    cmp rax, 0                  ; Low-confidence frame was consumed during render
    jne test_fail
    inc rbx
    
    ; Test 11: Verify flush (stats should be 0)
    lea rcx, stats_buffer
    call GET_GHOST_STATS
    test eax, eax
    jz test_fail
    
    mov rax, stats_buffer[16]   ; Active slots
    test rax, rax
    jnz test_fail
    inc rbx
    
    ; Test 12: Heartbeat with empty buffer
    xor ecx, ecx
    call GHOST_HEARTBEAT
    test eax, eax
    jnz test_fail               ; Should return 0 (no predictions)
    inc rbx
    
    ; All tests passed (12 tests)
    cmp rbx, 12
    jge test_success
    
test_fail:
    mov rcx, 1
    call ExitProcess
    
test_success:
    xor rcx, rcx
    call ExitProcess
    
main endp

end