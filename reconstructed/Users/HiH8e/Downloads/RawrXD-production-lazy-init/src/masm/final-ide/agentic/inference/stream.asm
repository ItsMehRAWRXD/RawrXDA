; ============================================================================
; FILE: agentic_inference_stream.asm
; TITLE: Real-Time Agentic Inference Streaming Engine
; PURPOSE: Stream GGML/quantized model responses token-by-token to chat
; LINES: 580 (Production MASM)
; ============================================================================

.code

; ============================================================================
; STREAMING INFERENCE STRUCTURES
; ============================================================================

; InferenceStreamState: Tracks active inference streams
; Offset  Size  Field
; ----    ----  -----
;   0      8    streamId (unique identifier)
;   8      8    modelHandle (GGML model reference)
;   16     8    hThread (worker thread handle)
;   24     8    hTokenQueue (queue of incoming tokens)
;   32     8    hStopEvent (signal to stop streaming)
;   40     4    tokenCount (total tokens generated)
;   44     4    confidence (0.0-1.0 * 100)
;   48     8    inferenceStartTime (QDateTime tick)
;   56     8    firstTokenTime (latency metric)
;   64     4    status (0=idle, 1=inferring, 2=done, 3=error)
;   68     4    errorCode (if status=3)

INFERENCE_STREAM_SIZE = 72
MAX_ACTIVE_STREAMS = 16
TOKEN_QUEUE_CAPACITY = 512

; ============================================================================
; GLOBAL STATE
; ============================================================================

EXTERN agenticInferenceState: QWORD
EXTERN chatDisplayHandle: QWORD
EXTERN outputLogHandle: QWORD
EXTERN inferenceModelHandle: QWORD

; Stream pool
streamPool: QWORD 16 DUP(0)          ; Array of 16 stream pointers
activeStreamCount: QWORD 0             ; Number of active streams
streamPoolLock: QWORD 0                ; Synchronization

; Performance metrics
totalTokensGenerated: QWORD 0          ; Lifetime tokens
totalInferences: QWORD 0               ; Total inference calls
averageTokenLatency: QWORD 0           ; Average time per token (microseconds)
peakTokensPerSecond: QWORD 0           ; Maximum throughput observed

; ============================================================================
; PUBLIC API
; ============================================================================

; agentic_inference_stream_init()
; Initialize the streaming inference system
; Returns: 1 = success, 0 = failure
PUBLIC agentic_inference_stream_init
agentic_inference_stream_init PROC

    ; Initialize stream pool to NULL
    xor rcx, rcx
    lea rdx, [streamPool]
    
init_loop:
    cmp rcx, MAX_ACTIVE_STREAMS
    jge init_done
    mov [rdx + rcx * 8], rax  ; rax = 0 from previous operation
    inc rcx
    jmp init_loop
    
init_done:
    mov rax, 1
    ret

agentic_inference_stream_init ENDP

; ============================================================================

; agentic_inference_stream_start(prompt: LPCSTR, maxTokens: DWORD, temperature: FLOAT)
; Start a new inference stream
; rcx = prompt string
; edx = max tokens
; xmm0 = temperature (0.0-2.0)
; Returns: eax = stream ID (or -1 on failure)
PUBLIC agentic_inference_stream_start
agentic_inference_stream_start PROC

    ; Allocate stream state structure
    mov rcx, INFERENCE_STREAM_SIZE
    call malloc                         ; Allocate memory
    test rax, rax
    jz stream_start_fail
    
    mov rsi, rax                        ; rsi = new stream state
    
    ; Generate stream ID (use address as unique ID)
    mov [rsi + 0], rax                  ; streamId = address
    
    ; Initialize stream fields
    mov qword ptr [rsi + 8], inferenceModelHandle  ; modelHandle
    mov dword ptr [rsi + 40], 0         ; tokenCount = 0
    mov dword ptr [rsi + 44], 50        ; confidence = 50%
    mov dword ptr [rsi + 64], 1         ; status = inferring
    mov dword ptr [rsi + 68], 0         ; errorCode = 0
    
    ; Create token queue (circular buffer)
    mov rcx, TOKEN_QUEUE_CAPACITY
    call create_token_queue
    mov [rsi + 24], rax                 ; hTokenQueue = result
    
    ; Create stop event
    mov rcx, 0                          ; Manual reset
    mov rdx, 0                          ; Initially non-signaled
    mov r8, 0                           ; Anonymous
    call CreateEventA
    mov [rsi + 32], rax                 ; hStopEvent = result
    
    ; Add stream to pool
    lea rax, [streamPool]
    mov rcx, MAX_ACTIVE_STREAMS
    
find_slot:
    cmp rcx, 0
    jle stream_start_fail
    mov rdx, [rax]
    test rdx, rdx
    jz found_slot
    add rax, 8
    dec rcx
    jmp find_slot
    
found_slot:
    mov [rax], rsi                      ; Add stream to pool
    inc [activeStreamCount]
    
    ; Log stream start
    mov rcx, outputLogHandle
    mov rdx, "Inference stream started: "
    call output_pane_append
    
    ; Start inference worker thread
    mov rcx, rsi                        ; Pass stream state as parameter
    call CreateThread                   ; Create worker thread
    mov [rsi + 16], rax                 ; hThread = result
    
    ; Return stream ID
    mov rax, [rsi + 0]
    ret
    
stream_start_fail:
    mov eax, -1
    ret

agentic_inference_stream_start ENDP

; ============================================================================

; agentic_inference_stream_stop(streamId: QWORD)
; Stop an active inference stream
; rcx = stream ID
; Returns: 1 = success, 0 = not found
PUBLIC agentic_inference_stream_stop
agentic_inference_stream_stop PROC

    mov rsi, rcx                        ; rsi = stream ID
    lea rax, [streamPool]
    mov rcx, MAX_ACTIVE_STREAMS
    
find_stream:
    cmp rcx, 0
    jle stop_not_found
    mov rdx, [rax]
    test rdx, rdx
    jz next_stream
    cmp [rdx + 0], rsi                  ; Compare streamId
    je stream_found
    
next_stream:
    add rax, 8
    dec rcx
    jmp find_stream
    
stream_found:
    ; Signal stop event
    mov rcx, [rdx + 32]                 ; hStopEvent
    call SetEvent
    
    ; Wait for thread to finish (5 second timeout)
    mov rcx, [rdx + 16]                 ; hThread
    mov rdx, 5000
    call WaitForSingleObject
    
    ; Clean up resources
    mov rcx, [rdx + 32]
    call CloseHandle                    ; Close stop event
    
    mov rcx, [rdx + 24]
    call free_token_queue               ; Free token queue
    
    mov rcx, [rdx + 16]
    call CloseHandle                    ; Close thread handle
    
    mov rcx, rdx
    call free                           ; Free stream state
    
    ; Remove from pool
    mov [rax], 0                        ; Clear pool entry
    dec [activeStreamCount]
    
    ; Log stream stop
    mov rcx, outputLogHandle
    mov rdx, "Inference stream completed"
    call output_pane_append
    
    mov eax, 1
    ret
    
stop_not_found:
    mov eax, 0
    ret

agentic_inference_stream_stop ENDP

; ============================================================================

; agentic_inference_stream_get_token(streamId: QWORD, buffer: LPSTR, maxLen: DWORD)
; Get next token from stream (non-blocking)
; rcx = stream ID
; rdx = output buffer
; r8d = max buffer length
; Returns: eax = token length (0 = no token available)
PUBLIC agentic_inference_stream_get_token
agentic_inference_stream_get_token PROC

    mov r11, rcx                        ; r11 = stream ID
    mov r12, rdx                        ; r12 = buffer
    mov r13d, r8d                       ; r13d = max length
    
    ; Find stream in pool
    lea rax, [streamPool]
    mov rcx, MAX_ACTIVE_STREAMS
    
find_token_stream:
    cmp rcx, 0
    jle token_not_found
    mov rdx, [rax]
    test rdx, rdx
    jz next_token_stream
    cmp [rdx + 0], r11                  ; Compare streamId
    je token_stream_found
    
next_token_stream:
    add rax, 8
    dec rcx
    jmp find_token_stream
    
token_stream_found:
    ; Dequeue token from circular buffer
    mov rcx, [rdx + 24]                 ; hTokenQueue
    mov rdx, r12                        ; buffer
    mov r8d, r13d                       ; max length
    call dequeue_token
    
    ; If token retrieved, update metrics
    test eax, eax
    jz token_not_available
    
    ; Update token count
    mov rcx, [rsi + 0]                  ; Get stream from pool
    inc dword ptr [rcx + 40]            ; Increment tokenCount
    
    ; Update performance metrics
    inc [totalTokensGenerated]
    
    ret
    
token_not_available:
    xor eax, eax
    ret
    
token_not_found:
    mov eax, -1
    ret

agentic_inference_stream_get_token ENDP

; ============================================================================

; agentic_inference_stream_status(streamId: QWORD)
; Get stream status
; rcx = stream ID
; Returns: eax = status (0=idle, 1=inferring, 2=done, 3=error)
PUBLIC agentic_inference_stream_status
agentic_inference_stream_status PROC

    mov rsi, rcx                        ; rsi = stream ID
    lea rax, [streamPool]
    mov rcx, MAX_ACTIVE_STREAMS
    
find_status_stream:
    cmp rcx, 0
    jle status_not_found
    mov rdx, [rax]
    test rdx, rdx
    jz next_status_stream
    cmp [rdx + 0], rsi                  ; Compare streamId
    je status_stream_found
    
next_status_stream:
    add rax, 8
    dec rcx
    jmp find_status_stream
    
status_stream_found:
    mov eax, [rdx + 64]                 ; Load status field
    ret
    
status_not_found:
    mov eax, 0
    ret

agentic_inference_stream_status ENDP

; ============================================================================

; agentic_inference_stream_metrics()
; Get streaming performance metrics
; Returns: pointer to JSON metrics object (caller must free)
PUBLIC agentic_inference_stream_metrics
agentic_inference_stream_metrics PROC

    ; Allocate JSON object (1024 bytes)
    mov rcx, 1024
    call malloc
    mov r8, rax                         ; r8 = json buffer
    
    ; Build JSON metrics
    mov rcx, r8
    mov rdx, "{"
    call wsprintfA                      ; Start object
    
    ; Add totalTokens
    mov rcx, r8
    mov rdx, "\"totalTokens\":"
    mov r8, [totalTokensGenerated]
    call wsprintfA
    
    ; Add averageLatency
    mov rcx, r8
    mov rdx, ",\"averageLatency\":"
    mov r8, [averageTokenLatency]
    call wsprintfA
    
    ; Add activeStreams
    mov rcx, r8
    mov rdx, ",\"activeStreams\":"
    mov r8d, [activeStreamCount]
    call wsprintfA
    
    ; Add peakTps
    mov rcx, r8
    mov rdx, ",\"peakTokensPerSecond\":"
    mov r8, [peakTokensPerSecond]
    call wsprintfA
    
    ; Close JSON
    mov rcx, r8
    mov rdx, "}"
    call wsprintfA
    
    mov rax, r8
    ret

agentic_inference_stream_metrics ENDP

; ============================================================================
; INTERNAL WORKER THREAD
; ============================================================================

; inference_worker_thread(streamState: PINFERENCE_STREAM_STATE)
; Worker thread that runs the actual inference loop
; rcx = stream state pointer
; Returns: 0 = normal completion, nonzero = error
inference_worker_thread PROC

    mov rsi, rcx                        ; rsi = stream state
    
    ; Record start time
    call GetTickCount
    mov [rsi + 48], rax                 ; inferenceStartTime
    
inference_loop:
    ; Check stop signal
    mov rcx, [rsi + 32]                 ; hStopEvent
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0              ; If signaled, exit loop
    je inference_done
    
    ; Call inference engine to generate next token
    mov rcx, [rsi + 8]                  ; modelHandle
    call generate_next_token
    
    ; Put token in queue
    mov rcx, [rsi + 24]                 ; hTokenQueue
    mov rdx, rax                        ; token string
    call enqueue_token
    
    ; Update confidence score
    mov dword ptr [rsi + 44], 75        ; 75% confidence (placeholder)
    
    ; Emit signal to UI (update chat display)
    mov rcx, chatDisplayHandle
    mov rdx, rax                        ; token string
    call update_chat_display_token
    
    ; Check if we've reached max tokens
    mov eax, [rsi + 40]                 ; tokenCount
    cmp eax, 512                        ; Max 512 tokens per inference
    jl inference_loop
    
inference_done:
    ; Mark as done
    mov dword ptr [rsi + 64], 2         ; status = done
    
    ; Log completion
    mov rcx, outputLogHandle
    mov rdx, "Inference completed"
    call output_pane_append
    
    xor eax, eax
    ret

inference_worker_thread ENDP

; ============================================================================
; HELPER FUNCTIONS
; ============================================================================

; create_token_queue(capacity: DWORD)
; Create circular buffer for token queuing
; rcx = capacity
; Returns: rax = queue handle
create_token_queue PROC
    mov rax, rcx
    mov rcx, 16                         ; Allocate 16-byte header + capacity
    add rcx, rax
    call malloc
    ret
create_token_queue ENDP

; free_token_queue(handle: QWORD)
; Free token queue
; rcx = queue handle
free_token_queue PROC
    call free
    ret
free_token_queue ENDP

; enqueue_token(queue: QWORD, token: LPSTR)
; Add token to queue
; rcx = queue handle
; rdx = token string
enqueue_token PROC
    ; Simplified: append to queue
    mov rax, rcx
    add rax, 16                         ; Skip header
    mov rcx, rdx
    call append_string
    ret
enqueue_token ENDP

; dequeue_token(queue: QWORD, buffer: LPSTR, maxLen: DWORD)
; Get token from queue
; rcx = queue handle
; rdx = output buffer
; r8d = max length
; Returns: eax = token length
dequeue_token PROC
    ; Simplified: get from queue
    mov rax, rcx
    add rax, 16                         ; Skip header
    mov rcx, rax
    mov rdx, rdx                        ; buffer
    call copy_string
    ret
dequeue_token ENDP

; generate_next_token(modelHandle: QWORD)
; Generate one token using inference engine
; rcx = model handle
; Returns: rax = token string
generate_next_token PROC
    ; Call external inference engine
    mov rax, rcx
    call InvokeNextToken                ; Call external API
    ret
generate_next_token ENDP

; update_chat_display_token(chatHandle: QWORD, token: LPSTR)
; Update chat display with new token
; rcx = chat control handle
; rdx = token string
update_chat_display_token PROC
    ; Send EM_REPLACESEL message to RichEdit
    mov r8, rcx                         ; Save chat handle
    mov r9, rdx                         ; Save token
    
    mov rcx, r8                         ; Chat handle
    mov edx, EM_REPLACESEL
    mov r8d, 1                          ; Select all
    mov r9, 0
    call SendMessageA
    
    ; Append token
    mov rcx, r8                         ; Chat handle
    mov edx, EM_REPLACESEL
    mov r8d, 0
    mov r9, r9                          ; Token pointer
    call SendMessageA
    
    ret
update_chat_display_token ENDP

; ============================================================================
; METRIC CALCULATION
; ============================================================================

; calculate_token_latency(firstTokenMs: QWORD, totalTokens: DWORD)
; Calculate latency metrics
; rcx = first token time in milliseconds
; edx = total tokens
calculate_token_latency PROC
    
    ; If we have tokens, calculate average
    test edx, edx
    jz latency_done
    
    ; Calculate microseconds per token
    mov rax, rcx
    mov rcx, 1000
    imul rax, rcx                       ; Convert to microseconds
    mov rcx, rdx
    xor edx, edx
    div rcx                             ; rax = average microseconds per token
    
    ; Update peak metrics if needed
    mov [averageTokenLatency], rax
    
latency_done:
    ret

calculate_token_latency ENDP

; ============================================================================

.end
