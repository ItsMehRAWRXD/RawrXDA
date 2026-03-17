; streaming_inference_masm.asm
; Pure MASM x64 - Streaming Inference (converted from C++ StreamingInference class)
; Token-by-token streaming output for real-time LLM responses

option casemap:none

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memset:PROC
EXTERN memcpy:PROC
EXTERN strlen:PROC
EXTERN strcpy:PROC
EXTERN sprintf:PROC
EXTERN console_log:PROC
EXTERN CreateThreadA:PROC
EXTERN WaitForSingleObject:PROC
EXTERN SetEvent:PROC
EXTERN ResetEvent:PROC
EXTERN CreateEventA:PROC
EXTERN CloseHandle:PROC

; Streaming constants
STREAMING_BUFFER_SIZE EQU 4096      ; 4 KB token buffer
MAX_PENDING_TOKENS EQU 1000         ; Queue up to 1000 tokens
STREAMING_TIMEOUT_MS EQU 5000       ; 5 second timeout

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; STREAM_TOKEN_ENTRY - Single token in stream
STREAM_TOKEN_ENTRY STRUCT
    tokenId DWORD ?
    text QWORD ?                    ; String pointer
    logits QWORD ?                  ; Float array for logits
    timestamp QWORD ?
STREAM_TOKEN_ENTRY ENDS

; STREAM_CONTEXT - Streaming context
STREAM_CONTEXT STRUCT
    streamId QWORD ?
    enginePtr QWORD ?               ; InferenceEngine pointer
    
    tokenQueue QWORD ?              ; Array of STREAM_TOKEN_ENTRY
    queueHead DWORD ?               ; Write position
    queueTail DWORD ?               ; Read position
    queueSize DWORD ?               ; Capacity
    
    tokenCount DWORD ?              ; Total tokens streamed
    totalCharacters QWORD ?         ; Total characters output
    
    outputBuffer QWORD ?            ; Accumulated output
    outputBufferSize QWORD ?        ; Current size
    maxOutputBufferSize QWORD ?     ; Capacity
    
    ; Threading
    streamThread QWORD ?            ; Thread handle
    threadId DWORD ?                ; Thread ID
    
    ; Synchronization
    dataReady QWORD ?               ; Event handle
    dataLock QWORD ?                ; Mutex (simplified as BYTE flag)
    
    ; Callbacks
    tokenCallback QWORD ?           ; Called for each token
    streamCallback QWORD ?          ; Called when stream chunk ready
    completeCallback QWORD ?        ; Called when stream done
    errorCallback QWORD ?           ; Called on error
    
    ; State
    streaming BYTE ?
    done BYTE ?
    cancelled BYTE ?
    error BYTE ?
STREAM_CONTEXT ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szStreamCreated DB "[STREAM] Created stream %lld, buffer=%ld KB", 0
    szStreamStarted DB "[STREAM] Stream %lld started (engine=%llx)", 0
    szTokenStreamed DB "[STREAM] Token streamed: id=%d, text='%s'", 0
    szStreamChunk DB "[STREAM] Chunk ready: %d tokens, %.1f KB", 0
    szStreamComplete DB "[STREAM] Stream %lld complete: %d tokens, %.2f seconds", 0
    szStreamError DB "[STREAM] Stream error: %s", 0
    szStreamCancelled DB "[STREAM] Stream %lld cancelled", 0

.code

; ============================================================================
; PUBLIC API
; ============================================================================

; streaming_create(RCX = enginePtr, RDX = maxTokens)
; Create streaming inference context
; Returns: RAX = pointer to STREAM_CONTEXT
PUBLIC streaming_create
streaming_create PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = enginePtr
    mov r8, rdx                     ; r8 = maxTokens
    
    ; Allocate context
    mov rcx, SIZEOF STREAM_CONTEXT
    call malloc
    mov r9, rax
    
    ; Allocate token queue
    mov rcx, r8
    imul rcx, SIZEOF STREAM_TOKEN_ENTRY
    call malloc
    mov [r9 + STREAM_CONTEXT.tokenQueue], rax
    
    ; Allocate output buffer (8 MB)
    mov rcx, 8388608
    call malloc
    mov [r9 + STREAM_CONTEXT.outputBuffer], rax
    mov [r9 + STREAM_CONTEXT.maxOutputBufferSize], rcx
    
    ; Initialize
    mov [r9 + STREAM_CONTEXT.enginePtr], rbx
    mov [r9 + STREAM_CONTEXT.queueSize], r8d
    mov [r9 + STREAM_CONTEXT.queueHead], 0
    mov [r9 + STREAM_CONTEXT.queueTail], 0
    mov [r9 + STREAM_CONTEXT.tokenCount], 0
    mov [r9 + STREAM_CONTEXT.totalCharacters], 0
    mov [r9 + STREAM_CONTEXT.outputBufferSize], 0
    mov byte [r9 + STREAM_CONTEXT.streaming], 0
    mov byte [r9 + STREAM_CONTEXT.done], 0
    mov byte [r9 + STREAM_CONTEXT.cancelled], 0
    mov byte [r9 + STREAM_CONTEXT.error], 0
    
    ; Get stream ID
    static_stream_id QWORD ?
    mov rax, [offset static_stream_id]
    mov [r9 + STREAM_CONTEXT.streamId], rax
    inc qword [offset static_stream_id]
    
    ; Create data ready event
    mov rcx, 0                      ; lpEventAttributes
    mov rdx, 0                      ; bManualReset
    mov r8, 0                       ; bInitialState
    mov r9d, 0                      ; lpName
    call CreateEventA
    mov [r9 + STREAM_CONTEXT.dataReady], rax
    
    ; Log
    lea rcx, [szStreamCreated]
    mov rdx, [r9 + STREAM_CONTEXT.streamId]
    movsx r8, dword [r9 + STREAM_CONTEXT.queueSize]
    imul r8, 4
    shr r8, 10                      ; Convert to KB
    call console_log
    
    mov rax, r9
    pop rbx
    ret
streaming_create ENDP

; ============================================================================

; streaming_start(RCX = stream)
; Start streaming inference thread
; Returns: RAX = thread ID
PUBLIC streaming_start
streaming_start PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = stream
    
    ; Log
    lea rcx, [szStreamStarted]
    mov rdx, [rbx + STREAM_CONTEXT.streamId]
    mov r8, [rbx + STREAM_CONTEXT.enginePtr]
    call console_log
    
    ; Mark as streaming
    mov byte [rbx + STREAM_CONTEXT.streaming], 1
    
    ; Create streaming thread
    mov rcx, 0                      ; lpThreadAttributes
    mov rdx, 0                      ; dwStackSize (default)
    lea r8, [streaming_thread_proc]
    mov r9, rbx                     ; lpParameter = context
    call CreateThreadA
    mov [rbx + STREAM_CONTEXT.streamThread], rax
    
    ; Get thread ID
    mov rax, [rbx + STREAM_CONTEXT.streamThread]
    mov [rbx + STREAM_CONTEXT.threadId], eax
    
    pop rbx
    ret
streaming_start ENDP

; ============================================================================

; streaming_push_token(RCX = stream, RDX = tokenId, R8 = tokenText)
; Add token to stream queue
; Returns: RAX = 1 if successful, 0 if queue full
PUBLIC streaming_push_token
streaming_push_token PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = stream
    mov r9d, edx                    ; r9d = tokenId
    mov rsi, r8                     ; rsi = tokenText
    
    ; Lock (simplified)
    mov byte [rbx + STREAM_CONTEXT.dataLock], 1
    
    ; Check queue capacity
    mov r10d, [rbx + STREAM_CONTEXT.queueHead]
    mov r11d, [rbx + STREAM_CONTEXT.queueTail]
    mov r12d, [rbx + STREAM_CONTEXT.queueSize]
    
    sub r10d, r11d
    js .wrap_check
    cmp r10d, r12d
    jge .queue_full
    jmp .queue_ok
    
.wrap_check:
    add r10d, r12d
    cmp r10d, r12d
    jge .queue_full
    
.queue_ok:
    ; Get queue entry
    mov r13, [rbx + STREAM_CONTEXT.tokenQueue]
    mov r14d, [rbx + STREAM_CONTEXT.queueHead]
    mov r15, r14
    imul r15, SIZEOF STREAM_TOKEN_ENTRY
    add r13, r15
    
    ; Store token
    mov [r13 + STREAM_TOKEN_ENTRY.tokenId], r9d
    mov [r13 + STREAM_TOKEN_ENTRY.text], rsi
    
    ; Get timestamp
    call GetTickCount64
    mov [r13 + STREAM_TOKEN_ENTRY.timestamp], rax
    
    ; Advance head
    inc r14d
    cmp r14d, r12d
    jl .no_wrap
    xor r14d, r14d
    
.no_wrap:
    mov [rbx + STREAM_CONTEXT.queueHead], r14d
    inc dword [rbx + STREAM_CONTEXT.tokenCount]
    
    ; Unlock
    mov byte [rbx + STREAM_CONTEXT.dataLock], 0
    
    ; Log
    lea rcx, [szTokenStreamed]
    mov edx, r9d
    mov r8, rsi
    call console_log
    
    ; Signal data ready
    mov rcx, [rbx + STREAM_CONTEXT.dataReady]
    call SetEvent
    
    mov rax, 1
    pop rsi
    pop rbx
    ret
    
.queue_full:
    ; Unlock
    mov byte [rbx + STREAM_CONTEXT.dataLock], 0
    xor rax, rax
    pop rsi
    pop rbx
    ret
streaming_push_token ENDP

; ============================================================================

; streaming_get_chunk(RCX = stream, RDX = outputBuffer, R8 = maxLength)
; Get accumulated tokens as text chunk
; Returns: RAX = chunk size
PUBLIC streaming_get_chunk
streaming_get_chunk PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = stream
    mov rsi, rdx                    ; rsi = outputBuffer
    mov r12, r8                     ; r12 = maxLength
    
    ; Lock
    mov byte [rbx + STREAM_CONTEXT.dataLock], 1
    
    ; Copy output buffer to result
    mov r13, [rbx + STREAM_CONTEXT.outputBuffer]
    mov r14, [rbx + STREAM_CONTEXT.outputBufferSize]
    
    ; Check size
    cmp r14, r12
    jle .size_ok
    mov r14, r12
    
.size_ok:
    ; Copy data
    mov rcx, r13
    mov rdx, rsi
    mov r8, r14
    call memcpy
    
    ; Null terminate
    mov byte [rsi + r14], 0
    
    ; Unlock
    mov byte [rbx + STREAM_CONTEXT.dataLock], 0
    
    ; Log chunk
    lea rcx, [szStreamChunk]
    mov rdx, [rbx + STREAM_CONTEXT.tokenCount]
    cvtsi2sd xmm0, r14
    divsd xmm0, [f1K]
    movsd xmm1, xmm0
    call console_log
    
    mov rax, r14
    pop rsi
    pop rbx
    ret
streaming_get_chunk ENDP

; ============================================================================

; streaming_finish(RCX = stream)
; Finish streaming and wait for completion
; Returns: RAX = 1 if successful
PUBLIC streaming_finish
streaming_finish PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = stream
    
    ; Mark as done
    mov byte [rbx + STREAM_CONTEXT.done], 1
    
    ; Wait for thread to complete
    mov rcx, [rbx + STREAM_CONTEXT.streamThread]
    mov rdx, STREAMING_TIMEOUT_MS
    call WaitForSingleObject
    
    ; Log completion
    lea rcx, [szStreamComplete]
    mov rdx, [rbx + STREAM_CONTEXT.streamId]
    mov r8, [rbx + STREAM_CONTEXT.tokenCount]
    mov r9, 5                       ; Simplified: 5 seconds
    call console_log
    
    mov rax, 1
    pop rbx
    ret
streaming_finish ENDP

; ============================================================================

; streaming_cancel(RCX = stream)
; Cancel streaming inference
PUBLIC streaming_cancel
streaming_cancel PROC
    mov byte [rcx + STREAM_CONTEXT.cancelled], 1
    
    ; Log
    lea rcx, [szStreamCancelled]
    mov rdx, [rcx + STREAM_CONTEXT.streamId]
    call console_log
    
    ret
streaming_cancel ENDP

; ============================================================================

; streaming_get_token_count(RCX = stream)
; Get current token count
; Returns: RAX = number of tokens streamed
PUBLIC streaming_get_token_count
streaming_get_token_count PROC
    mov eax, [rcx + STREAM_CONTEXT.tokenCount]
    ret
streaming_get_token_count ENDP

; ============================================================================

; streaming_get_output_size(RCX = stream)
; Get current output size
; Returns: RAX = size in bytes
PUBLIC streaming_get_output_size
streaming_get_output_size PROC
    mov rax, [rcx + STREAM_CONTEXT.outputBufferSize]
    ret
streaming_get_output_size ENDP

; ============================================================================

; streaming_set_token_callback(RCX = stream, RDX = callback)
; Set callback for each token
PUBLIC streaming_set_token_callback
streaming_set_token_callback PROC
    mov [rcx + STREAM_CONTEXT.tokenCallback], rdx
    ret
streaming_set_token_callback ENDP

; ============================================================================

; streaming_set_stream_callback(RCX = stream, RDX = callback)
; Set callback for stream chunk ready
PUBLIC streaming_set_stream_callback
streaming_set_stream_callback PROC
    mov [rcx + STREAM_CONTEXT.streamCallback], rdx
    ret
streaming_set_stream_callback ENDP

; ============================================================================

; streaming_set_complete_callback(RCX = stream, RDX = callback)
; Set callback for stream complete
PUBLIC streaming_set_complete_callback
streaming_set_complete_callback PROC
    mov [rcx + STREAM_CONTEXT.completeCallback], rdx
    ret
streaming_set_complete_callback ENDP

; ============================================================================

; streaming_is_complete(RCX = stream)
; Check if streaming is complete
; Returns: RAX = 1 if done, 0 if still streaming
PUBLIC streaming_is_complete
streaming_is_complete PROC
    movzx eax, byte [rcx + STREAM_CONTEXT.done]
    ret
streaming_is_complete ENDP

; ============================================================================

; streaming_destroy(RCX = stream)
; Free streaming context
PUBLIC streaming_destroy
streaming_destroy PROC
    push rbx
    
    mov rbx, rcx
    
    ; Close thread handle
    mov rcx, [rbx + STREAM_CONTEXT.streamThread]
    cmp rcx, 0
    je .skip_thread
    call CloseHandle
    
.skip_thread:
    ; Close data ready event
    mov rcx, [rbx + STREAM_CONTEXT.dataReady]
    cmp rcx, 0
    je .skip_event
    call CloseHandle
    
.skip_event:
    ; Free token queue
    mov rcx, [rbx + STREAM_CONTEXT.tokenQueue]
    cmp rcx, 0
    je .skip_queue
    call free
    
.skip_queue:
    ; Free output buffer
    mov rcx, [rbx + STREAM_CONTEXT.outputBuffer]
    cmp rcx, 0
    je .skip_output
    call free
    
.skip_output:
    ; Free context
    mov rcx, rbx
    call free
    
    pop rbx
    ret
streaming_destroy ENDP

; ============================================================================
; INTERNAL THREAD PROCEDURE
; ============================================================================

; streaming_thread_proc(RCX = context)
; Internal thread function for streaming
streaming_thread_proc PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = context
    
    ; Main streaming loop
.stream_loop:
    cmp byte [rbx + STREAM_CONTEXT.done], 1
    je .stream_exit
    
    cmp byte [rbx + STREAM_CONTEXT.cancelled], 1
    je .stream_exit
    
    ; Wait for data
    mov rcx, [rbx + STREAM_CONTEXT.dataReady]
    mov rdx, 1000                   ; 1 second timeout
    call WaitForSingleObject
    
    ; Process queued tokens
    mov r8d, [rbx + STREAM_CONTEXT.queueTail]
    mov r9d, [rbx + STREAM_CONTEXT.queueHead]
    
.process_loop:
    cmp r8d, r9d
    je .stream_loop
    
    ; Get token from queue
    mov r10, [rbx + STREAM_CONTEXT.tokenQueue]
    mov r11, r8
    imul r11, SIZEOF STREAM_TOKEN_ENTRY
    add r10, r11
    
    ; Get token text
    mov r12, [r10 + STREAM_TOKEN_ENTRY.text]
    
    ; Append to output buffer
    mov r13, [rbx + STREAM_CONTEXT.outputBuffer]
    mov r14, [rbx + STREAM_CONTEXT.outputBufferSize]
    add r13, r14
    
    ; Copy token text
    mov rcx, r12
    call strlen
    mov r15, rax                    ; r15 = text length
    
    mov rcx, r12
    mov rdx, r13
    mov r8, r15
    call memcpy
    
    ; Update output size
    add [rbx + STREAM_CONTEXT.outputBufferSize], r15
    
    ; Advance tail
    inc r8d
    mov r10d, [rbx + STREAM_CONTEXT.queueSize]
    cmp r8d, r10d
    jl .no_wrap2
    xor r8d, r8d
    
.no_wrap2:
    mov [rbx + STREAM_CONTEXT.queueTail], r8d
    
    jmp .process_loop
    
.stream_exit:
    pop rbx
    ret
streaming_thread_proc ENDP

; ============================================================================

.data
    f1K REAL8 1000.0
    static_stream_id QWORD 1

END
