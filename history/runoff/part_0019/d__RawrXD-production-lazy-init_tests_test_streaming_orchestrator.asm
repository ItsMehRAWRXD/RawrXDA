; RawrXD Streaming Orchestrator Validation Tests
; Pure MASM64 - No Dependencies

include windows.inc
include masm64rt.inc
include ../src/masm/RawrXD_Streaming_Orchestrator.asm

.data
test_name db "[TEST] ",0
pass_msg db " PASSED",0ah,0
fail_msg db " FAILED",0ah,0

.code

main PROC
    call asm_log, "=== Streaming Orchestrator Validation ===", ASM_LOG_INFO
    call test_init_orchestrator
    call test_wraparound
    call test_backpressure
    call test_engine_switch
    call test_patch_conflict
    call asm_log, "=== All Tests Complete ===", ASM_LOG_INFO
    xor eax, eax
    ret
main ENDP

; Test 1: Initialize orchestrator
test_init_orchestrator PROC
    lea rcx, test_name
    call asm_sprintf, rcx, "[TEST] %s", "Init_Orchestrator"
    call RawrXD_Streaming_Orchestrator_Init
    test rax, rax
    jnz @fail
    mov rax, g_stream_state.buffer_base
    test rax, rax
    jz @fail
    cmp g_stream_state.is_empty, 1
    jne @fail
    lea rcx, pass_msg
    jmp @done
@fail:
    lea rcx, fail_msg
@done:
    call asm_log, rcx, ASM_LOG_INFO
    ret
test_init_orchestrator ENDP

; Test 2: Ring buffer wraparound
test_wraparound PROC
    LOCAL bytes_written:qword
    lea rcx, test_name
    call asm_sprintf, rcx, "[TEST] %s", "Wraparound"
    mov rax, STREAM_BUFFER_SIZE
    sub rax, 1024
    mov g_stream_state.write_ptr, rax
    mov rcx, g_stream_state.buffer_base
    add rcx, rax
    mov rdx, 2048
    mov r8, rdx
    call RawrXD_Stream_WriteChunk
    mov bytes_written, rax
    cmp rax, 2048
    jne @fail
    cmp g_stream_state.write_ptr, 1024
    jne @fail
    lea rcx, pass_msg
    jmp @done
@fail:
    lea rcx, fail_msg
@done:
    call asm_log, rcx, ASM_LOG_INFO
    ret
test_wraparound ENDP

; Test 3: Backpressure signaling
test_backpressure PROC
    LOCAL initial_events:qword
    lea rcx, test_name
    call asm_sprintf, rcx, "[TEST] %s", "Backpressure"
    mov rax, STREAM_HIGH_WATER
    mov g_stream_state.bytes_available, rax
    mov rcx, g_stream_state.buffer_base
    mov rdx, STREAM_CHUNK_SIZE
    call RawrXD_Stream_WriteChunk
    test rax, rax
    jnz @fail
    cmp g_stream_state.is_full, 1
    jne @fail
    cmp g_backpressure_events, 0
    jle @fail
    lea rcx, pass_msg
    jmp @done
@fail:
    lea rcx, fail_msg
@done:
    call asm_log, rcx, ASM_LOG_INFO
    ret
test_backpressure ENDP

; Test 4: Engine switch mid-stream
test_engine_switch PROC
    LOCAL initial_engine:byte
    lea rcx, test_name
    call asm_sprintf, rcx, "[TEST] %s", "Engine_Switch"
    mov g_stream_state.active_engine, 0
    mov dword ptr g_stream_state.chunks_in_flight, 2
    mov rcx, 1
    call RawrXD_Stream_SwitchEngine
    cmp rax, 2
    jne @fail
    cmp g_stream_state.active_engine, 0
    jne @fail
    mov dword ptr g_stream_state.chunks_in_flight, 0
    mov rcx, 1
    call RawrXD_Stream_SwitchEngine
    test rax, rax
    jnz @fail
    cmp g_stream_state.active_engine, 1
    jne @fail
    lea rcx, pass_msg
    jmp @done
@fail:
    lea rcx, fail_msg
@done:
    call asm_log, rcx, ASM_LOG_INFO
    ret
test_engine_switch ENDP

; Test 5: Patch conflict detection
test_patch_conflict PROC
    lea rcx, test_name
    call asm_sprintf, rcx, "[TEST] %s", "Patch_Conflict"
    lea rdi, g_dma_chunks
    mov byte ptr [rdi].DMA_CHUNK.status, 1  ; ACTIVE
    lea rax, tensor_name
    mov [rdi].DMA_CHUNK.tensor_name, rax
    lea rcx, tensor_name
    call RawrXD_Stream_CheckPatchConflict
    cmp rax, 1
    jne @fail
    lea rcx, pass_msg
    jmp @done
@fail:
    lea rcx, fail_msg
@done:
    call asm_log, rcx, ASM_LOG_INFO
    ret
test_patch_conflict ENDP

.data
tensor_name db "blk.0.attn.q",0

END
