; Titan_Streaming_Orchestrator_NoHTTP.asm
; Modified orchestrator: Direct native inference instead of llama-server.exe
; Replaces HTTP calls with direct Titan_RunInference calls in inference thread

OPTION CASEMAP:NONE

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
    
    ; TODO: Sample from logits (temperature, top-p, etc)
    ; For now, argmax (greedy decoding)
    ; TODO: Write token to ring buffer (Titan_SubmitChunk)
    
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
    
    ; Tokenize prompt and load into input buffer (placeholder)
    ; For now, assume single token (token 0 = BOS)
    mov [nCurrentToken], 1
    
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
