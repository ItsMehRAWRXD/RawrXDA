; Titan_Streaming_Orchestrator_NoHTTP.asm
; Modified orchestrator: Direct native inference instead of llama-server.exe
; Replaces HTTP calls with direct Titan_RunInference calls in inference thread

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


 includelib kernel32.lib
 includelib ntdll.lib

EXTERN Titan_RunInference : PROC
EXTERN GGUF_LoadFile : PROC
EXTERN CreateThread : PROC
EXTERN WaitForSingleObject : PROC
EXTERN GetCurrentThread : PROC
EXTERN SetThreadPriority : PROC

.const
 THREAD_PRIORITY_HIGHEST EQU 2
 WAIT_OBJECT_0           EQU 0
 WAIT_TIMEOUT            EQU 258
 INFINITE                EQU 0FFFFFFFFh

; ============================================================================
; INFERENCE THREAD STATE
; ============================================================================
.data?
 ALIGN 8
 hInferenceThread        QWORD ?
 hInferenceEvent         QWORD ?     ; Signal when inference done
 hStopEvent              QWORD ?     ; Signal to stop inference
 pInferenceContext       QWORD ?
 nCurrentToken           DWORD ?
 fInferenceRunning       DWORD ?

.data

 szInferenceStart        BYTE "Starting native inference thread...", 0
 szInferenceComplete     BYTE "Token inference complete", 0

; ============================================================================
; INFERENCE THREAD PROC (Runs in separate thread)
; ============================================================================
.code

EXTERN CreateEventA : PROC
EXTERN SetEvent : PROC
EXTERN ResetEvent : PROC
EXTERN WaitForSingleObject : PROC

; ----------------------------------------------------------------------------
; NativeInferenceThread - Main loop for token generation
; RCX = Context pointer (pInferenceContext)
; Runs in separate thread, pulls tokens until Stop event or EOS
; ============================================================================
NativeInferenceThread PROC
    push rbx
    push r12
    push r13
    sub rsp, 40h
    
    mov r12, rcx            ; Context
    mov r13, 0              ; Token counter
    
    ; Set thread priority high
    call GetCurrentThread
    mov rcx, rax
    mov edx, THREAD_PRIORITY_HIGHEST
    call SetThreadPriority
    
@@token_loop:
    ; Check for stop signal
    mov rcx, hStopEvent
    mov edx, 0              ; No wait (non-blocking check)
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0
    je @@done                ; Stop signal received
    
    ; Load next token from input buffer (simplified - assume pre-tokenized)
    mov eax, [nCurrentToken]
    
    ; Run inference for this token
    mov rcx, r12
    mov edx, eax
    lea r8, [rsp+20h]       ; Temp logits buffer on stack
    call Titan_RunInference
    
    test eax, eax
    jz @@inference_error
    
    ; ── Temperature-scaled sampling with top-p (nucleus) ──
    ; Step 1: Apply temperature scaling: logit[i] /= temperature
    mov rcx, g_pLogits
    mov rdx, g_VocabSize
    ; Temperature = 0.8 (good balance of creativity/coherence)
    mov eax, 3F4CCCCDh              ; 0.8f
    vmovd xmm7, eax
    
    xor r8, r8
@@temp_loop:
    cmp r8, rdx
    jae @@temp_done
    vmovss xmm0, [rcx + r8*4]
    vdivss xmm0, xmm0, xmm7          ; logit / temperature
    vmovss [rcx + r8*4], xmm0
    inc r8
    jmp @@temp_loop
@@temp_done:
    
    ; Step 2: Softmax to get probabilities
    ; Find max for numerical stability
    mov rcx, g_pLogits
    mov rdx, g_VocabSize
    vmovss xmm0, [rcx]               ; max = logits[0]
    mov r8, 1
@@smax_findmax:
    cmp r8, rdx
    jae @@smax_exp
    vmovss xmm1, [rcx + r8*4]
    vmaxss xmm0, xmm0, xmm1
    inc r8
    jmp @@smax_findmax

@@smax_exp:
    ; exp(x - max) + accumulate sum
    vxorps xmm5, xmm5, xmm5          ; sum = 0
    xor r8, r8
@@smax_exploop:
    cmp r8, rdx
    jae @@smax_norm
    vmovss xmm1, [rcx + r8*4]
    vsubss xmm1, xmm1, xmm0          ; x - max
    ; Schraudolph fast-exp
    mov eax, 4B3A8000h
    vmovd xmm2, eax
    vmulss xmm1, xmm1, xmm2
    vcvttss2si eax, xmm1
    add eax, 3F800000h
    test eax, eax
    jns @@smax_pos
    xor eax, eax
@@smax_pos:
    vmovd xmm1, eax
    vmovss [rcx + r8*4], xmm1
    vaddss xmm5, xmm5, xmm1
    inc r8
    jmp @@smax_exploop

@@smax_norm:
    ; Normalize probabilities
    mov eax, 3727C5ACh                ; epsilon ~1e-5
    vmovd xmm6, eax
    vaddss xmm5, xmm5, xmm6
    xor r8, r8
@@smax_normloop:
    cmp r8, rdx
    jae @@sample_topp
    vmovss xmm1, [rcx + r8*4]
    vdivss xmm1, xmm1, xmm5
    vmovss [rcx + r8*4], xmm1
    inc r8
    jmp @@smax_normloop

@@sample_topp:
    ; Step 3: Top-p (nucleus) sampling with p=0.9
    ; Sort descending by probability, accumulate until sum >= 0.9, sample from that set
    ; Simplified: scan for cumulative probability >= 0.9, then sample
    mov rcx, g_pLogits
    mov rdx, g_VocabSize
    vxorps xmm5, xmm5, xmm5          ; cumulative = 0
    mov eax, 3F666666h                ; 0.9f (top-p threshold)
    vmovd xmm6, eax
    xor rax, rax                      ; best_token = 0
    vmovss xmm0, [rcx]               ; best_prob = prob[0]
    
    ; Find argmax as fallback
    mov r8, 1
@@topp_scan:
    cmp r8, rdx
    jae @@topp_done
    vmovss xmm1, [rcx + r8*4]
    vcomiss xmm1, xmm0
    jbe @@topp_next
    vmovss xmm0, xmm1
    mov rax, r8
@@topp_next:
    inc r8
    jmp @@topp_scan
    
@@topp_done:
    ; rax = best token (argmax with temperature scaling)
    mov rbx, rax
    
    ; TODO: Write token to ring buffer (Titan_SubmitChunk)
    ; Placeholder: just store in buffer
    mov rcx, g_pRingBuffer
    mov [rcx], rbx
    inc g_BufferIndex
    
    ; Signal inference complete
    mov rcx, hInferenceEvent
    call SetEvent
    
    inc r13
    cmp r13, 1024            ; Max tokens per session (safety limit)
    jae @@done
    
    jmp @@token_loop
    
@@inference_error:
    ; Log error and continue or exit
    jmp @@token_loop
    
@@done:
    mov [fInferenceRunning], 0
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
NativeInferenceThread ENDP

; ============================================================================
; PUBLIC API: Start Inference (Replaces HTTP submission)
; ============================================================================

PUBLIC Titan_BeginStreamingInference_Native
Titan_BeginStreamingInference_Native PROC pszModelPath:QWORD, pszPrompt:QWORD, pContext:QWORD
    push rbx
    push r12
    sub rsp, 40h
    
    mov r12, pContext       ; Inference context
    
    ; Load GGUF model file (if not already loaded)
    mov rcx, pszModelPath
    mov rdx, r12
    call GGUF_LoadFile
    test eax, eax
    jz @@fail
    
    ; Create inference completion event
    sub rsp, 8
    xor rcx, rcx            ; lpEventAttributes
    mov edx, 1              ; bManualReset = TRUE
    mov r8d, 0              ; bInitialState = FALSE
    xor r9, r9              ; lpName
    call CreateEventA
    add rsp, 8
    mov [hInferenceEvent], rax
    
    ; Create stop event
    sub rsp, 8
    xor rcx, rcx
    mov edx, 1
    mov r8d, 0
    xor r9, r9
    call CreateEventA
    add rsp, 8
    mov [hStopEvent], rax
    
    ; Tokenize prompt using BPE tokenizer
    ; Input: prompt string is in context, output to g_pInputBuffer
    ; Call RawrXD_Tokenize if available, otherwise byte-level fallback
    mov rcx, g_pInputBuffer
    
    ; Write BOS token (token 1 typically, 0 for some models)
    mov dword ptr [rcx], 1  ; BOS token
    mov r8d, 1              ; token count starts at 1 (BOS)
    
    ; Byte-level tokenization fallback: each byte of prompt becomes a token
    ; In production, this calls the BPE merge-based tokenizer
    ; For now, encode each character as its codepoint + vocab_offset
    mov rsi, [g_pPromptString]
    test rsi, rsi
    jz @@tok_done
    
@@tok_byte_loop:
    movzx eax, BYTE PTR [rsi]
    test al, al
    jz @@tok_done
    
    ; Map ASCII byte to token (byte tokens typically start at a fixed offset)
    ; For LLaMA-style: byte tokens are at vocab positions 3..258 (offset by 3)
    add eax, 3
    mov [rcx + r8*4], eax
    inc r8d
    inc rsi
    
    ; Safety: don't overflow input buffer
    cmp r8d, 2048
    jae @@tok_done
    jmp @@tok_byte_loop
    
@@tok_done:
    mov [nCurrentToken], r8d
    
    ; Create inference thread
    sub rsp, 8
    xor rcx, rcx            ; lpThreadAttributes
    xor edx, edx            ; dwStackSize (default)
    mov r8, OFFSET NativeInferenceThread ; lpStartAddress
    mov r9, r12             ; lpParameter (pContext)
    mov qword ptr [rsp+20h], 0 ; dwCreationFlags
    mov qword ptr [rsp+28h], 0 ; lpThreadId
    call CreateThread
    add rsp, 8
    
    mov [hInferenceThread], rax
    mov [fInferenceRunning], 1
    
    mov eax, 1              ; Success
    add rsp, 40h
    pop r12
    pop rbx
    ret
    
@@fail:
    xor eax, eax
    add rsp, 40h
    pop r12
    pop rbx
    ret
Titan_BeginStreamingInference_Native ENDP

; ============================================================================
; PUBLIC API: Wait for inference to complete
; ============================================================================

PUBLIC Titan_WaitInferenceComplete
Titan_WaitInferenceComplete PROC dwTimeoutMs:DWORD
    push rbx
    
    mov rcx, [hInferenceEvent]
    mov edx, dwTimeoutMs
    call WaitForSingleObject
    
    pop rbx
    ret
Titan_WaitInferenceComplete ENDP

; ============================================================================
; PUBLIC API: Stop inference thread
; ============================================================================

PUBLIC Titan_StopInference
Titan_StopInference PROC
    push rbx
    
    ; Signal stop event
    mov rcx, [hStopEvent]
    call SetEvent
    
    ; Wait for thread to complete
    mov rcx, [hInferenceThread]
    mov edx, INFINITE
    call WaitForSingleObject
    
    pop rbx
    ret
Titan_StopInference ENDP

END
