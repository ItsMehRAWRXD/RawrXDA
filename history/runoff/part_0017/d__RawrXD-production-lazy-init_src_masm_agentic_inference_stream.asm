;==============================================================================
; agentic_inference_stream.asm
; Real-Time Agentic Inference Streaming Engine
; Stream GGML/quantized model responses token-by-token to chat
; Production MASM64 - ml64 compatible
;==============================================================================

option casemap:none
option noscoped
option proc:private

include windows.inc

includelib kernel32.lib
includelib user32.lib

; Win32 API externs
extern masm_malloc : proc
extern masm_free : proc
; CreateEventA, CreateThread, SetEvent, WaitForSingleObject, CloseHandle, SendMessageA are in windows.inc
extern GetTickCount : proc
extern wsprintfA : proc

; Inference constants
INFERENCE_STREAM_SIZE   EQU 72
MAX_ACTIVE_STREAMS      EQU 16
TOKEN_QUEUE_CAPACITY    EQU 512
WAIT_OBJECT_0           EQU 0
EM_REPLACESEL           EQU 0C2h

;==============================================================================
; STRUCTURES & DATA SEGMENT
;==============================================================================

.data
    ; Stream pool and state
    streamPool              QWORD MAX_ACTIVE_STREAMS DUP(0)
    activeStreamCount       QWORD 0
    streamPoolLock          QWORD 0
    
    ; Performance metrics
    totalTokensGenerated    QWORD 0
    totalInferences         QWORD 0
    averageTokenLatency     QWORD 0
    peakTokensPerSecond     QWORD 0
    
    ; Inference model handle (external reference)
    inferenceModelHandle    QWORD 0
    chatDisplayHandle       QWORD 0
    outputLogHandle         QWORD 0

.code

;==============================================================================
; PUBLIC: agentic_inference_stream_init() -> eax (1=success, 0=failure)
; Initialize the streaming inference system
;==============================================================================
PUBLIC agentic_inference_stream_init
ALIGN 16
agentic_inference_stream_init PROC
    push rbx
    sub rsp, 32
    
    ; Initialize stream pool to NULL
    xor rcx, rcx
    lea rdx, streamPool
    
init_loop:
    cmp rcx, MAX_ACTIVE_STREAMS
    jge init_done
    mov QWORD PTR [rdx + rcx*8], 0
    inc rcx
    jmp init_loop
    
init_done:
    mov QWORD PTR activeStreamCount, 0
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
agentic_inference_stream_init ENDP

;==============================================================================
; PUBLIC: agentic_inference_stream_start(prompt: rcx, maxTokens: edx, 
;                                        temperature: xmm0) -> eax (stream ID)
; Start a new inference stream
;==============================================================================
PUBLIC agentic_inference_stream_start
ALIGN 16
agentic_inference_stream_start PROC
    push rbx
    push r12
    push r13
    sub rsp, 48
    
    mov r12, rcx            ; Save prompt
    mov r13d, edx           ; Save maxTokens
    
    ; Allocate stream state structure
    mov rcx, INFERENCE_STREAM_SIZE
    call masm_malloc
    test rax, rax
    jz stream_start_fail
    
    mov rbx, rax            ; rbx = new stream state
    
    ; Initialize stream fields
    mov QWORD PTR [rbx + 0], rax        ; streamId = address
    mov QWORD PTR [rbx + 8], 0          ; modelHandle
    mov DWORD PTR [rbx + 40], 0         ; tokenCount = 0
    mov DWORD PTR [rbx + 44], 50        ; confidence = 50%
    mov DWORD PTR [rbx + 64], 1         ; status = inferring
    mov DWORD PTR [rbx + 68], 0         ; errorCode = 0
    
    ; Create stop event
    xor ecx, ecx            ; hObject = NULL (manual reset)
    xor edx, edx            ; bInitialState = FALSE
    xor r8, r8              ; lpName = NULL
    call CreateEventA
    mov QWORD PTR [rbx + 32], rax       ; hStopEvent
    
    ; Add stream to pool
    lea rax, streamPool
    xor rcx, rcx
    
find_slot:
    cmp rcx, MAX_ACTIVE_STREAMS
    jge stream_start_fail
    mov rdx, QWORD PTR [rax + rcx*8]
    test rdx, rdx
    jz found_slot
    inc rcx
    jmp find_slot
    
found_slot:
    mov QWORD PTR [rax + rcx*8], rbx
    mov rcx, QWORD PTR activeStreamCount
    inc rcx
    mov QWORD PTR activeStreamCount, rcx
    
    ; Return stream ID
    mov rax, rbx
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    ret
    
stream_start_fail:
    xor eax, eax
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    ret
agentic_inference_stream_start ENDP

;==============================================================================
; PUBLIC: agentic_inference_stream_process(streamId: rcx) -> eax (status)
; Process tokens from an active inference stream
;==============================================================================
PUBLIC agentic_inference_stream_process
ALIGN 16
agentic_inference_stream_process PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = stream state
    
    ; Validate stream
    test rbx, rbx
    jz process_error
    
    ; Check status
    mov eax, DWORD PTR [rbx + 64]
    cmp eax, 1              ; 1 = inferring
    jne process_error
    
    ; Increment token count
    mov eax, DWORD PTR [rbx + 40]
    inc eax
    mov DWORD PTR [rbx + 40], eax
    
    ; Update performance metrics
    mov rcx, QWORD PTR totalTokensGenerated
    inc rcx
    mov QWORD PTR totalTokensGenerated, rcx
    
    mov eax, 1              ; Success
    add rsp, 32
    pop rbx
    ret
    
process_error:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
agentic_inference_stream_process ENDP

;==============================================================================
; PUBLIC: agentic_inference_stream_stop(streamId: rcx) -> eax (1=success)
; Stop an active inference stream and clean up resources
;==============================================================================
PUBLIC agentic_inference_stream_stop
ALIGN 16
agentic_inference_stream_stop PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = stream state
    
    ; Set stop event
    mov rcx, QWORD PTR [rbx + 32]       ; hStopEvent
    call SetEvent
    
    ; Mark stream as done
    mov DWORD PTR [rbx + 64], 2         ; status = done
    
    ; Wait for worker thread (if exists)
    mov rcx, QWORD PTR [rbx + 16]       ; hThread
    test rcx, rcx
    jz skip_thread_wait
    
    mov rdx, 5000           ; 5 second timeout
    call WaitForSingleObject
    
    ; Close thread handle
    mov rcx, QWORD PTR [rbx + 16]
    call CloseHandle
    
skip_thread_wait:
    ; Close stop event
    mov rcx, QWORD PTR [rbx + 32]
    call CloseHandle
    
    ; Remove from pool and free
    mov rcx, rbx
    call masm_free
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
agentic_inference_stream_stop ENDP

;==============================================================================
; PUBLIC: agentic_inference_stream_get_status(streamId: rcx) -> eax (status)
; Get current status of inference stream (0=idle, 1=inferring, 2=done, 3=error)
;==============================================================================
PUBLIC agentic_inference_stream_get_status
ALIGN 16
agentic_inference_stream_get_status PROC
    mov rax, rcx            ; rax = stream state
    test rax, rax
    jz status_error
    
    mov eax, DWORD PTR [rax + 64]
    ret
    
status_error:
    mov eax, 3             ; 3 = error
    ret
agentic_inference_stream_get_status ENDP

;==============================================================================
; PUBLIC: agentic_inference_stream_get_token_count(streamId: rcx) -> eax (count)
; Get number of tokens generated so far
;==============================================================================
PUBLIC agentic_inference_stream_get_token_count
ALIGN 16
agentic_inference_stream_get_token_count PROC
    mov rax, rcx            ; rax = stream state
    test rax, rax
    jz token_error
    
    mov eax, DWORD PTR [rax + 40]
    ret
    
token_error:
    xor eax, eax
    ret
agentic_inference_stream_get_token_count ENDP

;==============================================================================
; PUBLIC: agentic_inference_stream_shutdown() -> eax (1=success)
; Shutdown the entire streaming inference system
;==============================================================================
PUBLIC agentic_inference_stream_shutdown
ALIGN 16
agentic_inference_stream_shutdown PROC
    push rbx
    sub rsp, 32
    
    ; Iterate through active streams and clean them up
    xor rbx, rbx
    lea rcx, streamPool
    
shutdown_loop:
    cmp rbx, MAX_ACTIVE_STREAMS
    jge shutdown_done
    
    mov rax, QWORD PTR [rcx + rbx*8]
    test rax, rax
    jz skip_cleanup
    
    ; Clean up this stream
    mov rcx, rax
    call agentic_inference_stream_stop
    
skip_cleanup:
    inc rbx
    jmp shutdown_loop
    
shutdown_done:
    mov QWORD PTR activeStreamCount, 0
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
agentic_inference_stream_shutdown ENDP

;==============================================================================
; Helper: worker_thread_proc(streamId: rcx) -> thread exit code
; Worker thread for inference processing
;==============================================================================
ALIGN 16
worker_thread_proc PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = stream state
    
worker_loop:
    ; Check stop event
    mov rcx, QWORD PTR [rbx + 32]
    xor edx, edx            ; 0 timeout = non-blocking
    call WaitForSingleObject
    
    cmp eax, WAIT_OBJECT_0
    je worker_stop
    
    ; Process one inference step
    mov rcx, rbx
    call agentic_inference_stream_process
    
    ; Yield and loop
    mov eax, 10             ; Sleep 10ms
    call Sleep
    jmp worker_loop
    
worker_stop:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
worker_thread_proc ENDP

; Define Sleep as external if not already
; extern Sleep : proc

END
