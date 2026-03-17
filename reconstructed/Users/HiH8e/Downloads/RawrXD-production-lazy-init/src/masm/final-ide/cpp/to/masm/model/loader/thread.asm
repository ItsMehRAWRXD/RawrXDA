; model_loader_thread_masm.asm
; Pure MASM x64 - Model Loader Thread (converted from C++ ModelLoaderThread class)
; Pure threading using Win32 CreateThread (replaces std::thread)

option casemap:none

EXTERN CreateThreadA:PROC
EXTERN TerminateThread:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC
EXTERN GetCurrentThreadId:PROC
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memcpy:PROC
EXTERN strlen:PROC
EXTERN console_log:PROC

; Thread constants
INFINITE EQU -1
INVALID_HANDLE_VALUE EQU -1

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; CALLBACK_FUNCTION - typedef for callbacks
; typedef void (*ProgressCallback)(const char*);
; typedef void (*CompleteCallback)(bool, const char*);

; MODEL_LOADER_THREAD - Thread context
MODEL_LOADER_THREAD STRUCT
    threadHandle QWORD ?            ; Thread handle
    threadId DWORD ?                ; Thread ID
    padding1 DWORD ?
    
    enginePtr QWORD ?               ; Pointer to InferenceEngine
    modelPath QWORD ?               ; Model path string (malloc'd)
    
    canceled BYTE ?                 ; Cancellation flag
    running BYTE ?                  ; Running flag
    padding2 WORD ?
    
    progressCallback QWORD ?        ; Function pointer
    completeCallback QWORD ?        ; Function pointer
    
    errorMessage BYTE 512 DUP(?)   ; Last error message
MODEL_LOADER_THREAD ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szThreadStarted DB "[LOADER] Model loader thread started (path=%s)", 0
    szThreadRunning DB "[LOADER] Thread running (ID=%d)", 0
    szThreadCanceled DB "[LOADER] Thread canceled by request", 0
    szThreadCompleted DB "[LOADER] Thread completed", 0
    szLoadProgress DB "[LOADER] Progress: %s", 0

.code

; ============================================================================
; THREAD FUNCTION (called from CreateThread)
; ============================================================================

; model_loader_thread_function(RCX = lpParameter)
; This is the actual thread procedure
ALIGN 16
model_loader_thread_function PROC FRAME
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                   ; rbx = MODEL_LOADER_THREAD*
    
    ; Mark as running
    mov byte [rbx + MODEL_LOADER_THREAD.running], 1
    
    ; Get thread ID
    call GetCurrentThreadId
    mov [rbx + MODEL_LOADER_THREAD.threadId], eax
    
    ; Log
    lea rcx, [szThreadRunning]
    mov edx, eax
    call console_log
    
    ; Load model
    ; This would call inference engine's loadModel() with modelPath
    
    ; Check for cancellation
.load_loop:
    cmp byte [rbx + MODEL_LOADER_THREAD.canceled], 1
    je .load_canceled
    
    ; Simulate progress
    mov rcx, [rbx + MODEL_LOADER_THREAD.progressCallback]
    cmp rcx, 0
    je .skip_progress
    
    lea rdx, [szLoadProgress]
    mov r8, [rbx + MODEL_LOADER_THREAD.modelPath]
    call rcx                       ; Call progress callback
    
.skip_progress:
    ; Sleep a bit and loop
    jmp .load_loop
    
.load_canceled:
    lea rcx, [szThreadCanceled]
    call console_log
    
    ; Call completion callback with error
    mov rcx, [rbx + MODEL_LOADER_THREAD.completeCallback]
    cmp rcx, 0
    je .skip_complete
    
    xor edx, edx                   ; success = false
    lea r8, [szThreadCanceled]
    call rcx                       ; Call complete callback
    
.skip_complete:
    ; Mark as not running
    mov byte [rbx + MODEL_LOADER_THREAD.running], 0
    
    lea rcx, [szThreadCompleted]
    call console_log
    
    pop rdi
    pop rsi
    pop rbx
    xor eax, eax                   ; Return 0
    ret
model_loader_thread_function ENDP

; ============================================================================
; PUBLIC API
; ============================================================================

; model_loader_thread_create(RCX = enginePtr, RDX = modelPath)
; Create model loader thread context
; Returns: RAX = pointer to MODEL_LOADER_THREAD
PUBLIC model_loader_thread_create
model_loader_thread_create PROC
    push rbx
    push rsi
    
    mov rbx, rcx                   ; rbx = enginePtr
    mov rsi, rdx                   ; rsi = modelPath
    
    ; Allocate thread context
    mov rcx, SIZEOF MODEL_LOADER_THREAD
    call malloc
    
    ; Store engine pointer
    mov [rax + MODEL_LOADER_THREAD.enginePtr], rbx
    
    ; Copy model path
    mov rcx, rsi
    call strlen
    
    mov rdx, rax
    inc rdx                        ; +1 for null terminator
    
    push rax
    mov rcx, rdx
    call malloc
    pop rdx
    
    mov [rax + MODEL_LOADER_THREAD.modelPath], rax
    
    mov rcx, rax
    mov rdx, rsi
    call memcpy
    
    ; Log
    lea rcx, [szThreadStarted]
    mov rdx, rsi
    call console_log
    
    pop rsi
    pop rbx
    ret
model_loader_thread_create ENDP

; ============================================================================

; model_loader_thread_start(RCX = loaderThread)
; Start the loading thread
PUBLIC model_loader_thread_start
model_loader_thread_start PROC
    push rbx
    
    mov rbx, rcx                   ; rbx = thread context
    
    ; Create Win32 thread
    mov rcx, 0                     ; lpThreadAttributes
    mov rdx, 0                     ; dwStackSize (default)
    mov r8, offset model_loader_thread_function  ; lpStartAddress
    mov r9, rbx                    ; lpParameter = context
    lea r10, [rbx + MODEL_LOADER_THREAD.threadId]
    
    call CreateThreadA
    
    ; Store handle
    mov [rbx + MODEL_LOADER_THREAD.threadHandle], rax
    
    pop rbx
    ret
model_loader_thread_start ENDP

; ============================================================================

; model_loader_thread_cancel(RCX = loaderThread)
; Request thread cancellation
PUBLIC model_loader_thread_cancel
model_loader_thread_cancel PROC
    mov byte [rcx + MODEL_LOADER_THREAD.canceled], 1
    ret
model_loader_thread_cancel ENDP

; ============================================================================

; model_loader_thread_is_canceled(RCX = loaderThread)
; Check if cancellation requested
; Returns: AL = 1 (canceled) or 0 (running)
PUBLIC model_loader_thread_is_canceled
model_loader_thread_is_canceled PROC
    movzx eax, byte [rcx + MODEL_LOADER_THREAD.canceled]
    ret
model_loader_thread_is_canceled ENDP

; ============================================================================

; model_loader_thread_is_running(RCX = loaderThread)
; Check if thread is running
; Returns: AL = 1 (running) or 0 (stopped)
PUBLIC model_loader_thread_is_running
model_loader_thread_is_running PROC
    movzx eax, byte [rcx + MODEL_LOADER_THREAD.running]
    ret
model_loader_thread_is_running ENDP

; ============================================================================

; model_loader_thread_wait(RCX = loaderThread, RDX = timeoutMs)
; Wait for thread to complete
; Returns: AL = 1 (completed) or 0 (timeout)
PUBLIC model_loader_thread_wait
model_loader_thread_wait PROC
    mov r8, [rcx + MODEL_LOADER_THREAD.threadHandle]
    
    call WaitForSingleObject
    
    cmp eax, 0                     ; WAIT_OBJECT_0 = success
    jne .timeout
    
    mov al, 1
    ret
    
.timeout:
    xor eax, eax
    ret
model_loader_thread_wait ENDP

; ============================================================================

; model_loader_thread_set_progress_callback(RCX = loaderThread, RDX = callback)
; Set progress callback function
PUBLIC model_loader_thread_set_progress_callback
model_loader_thread_set_progress_callback PROC
    mov [rcx + MODEL_LOADER_THREAD.progressCallback], rdx
    ret
model_loader_thread_set_progress_callback ENDP

; ============================================================================

; model_loader_thread_set_complete_callback(RCX = loaderThread, RDX = callback)
; Set completion callback function
PUBLIC model_loader_thread_set_complete_callback
model_loader_thread_set_complete_callback PROC
    mov [rcx + MODEL_LOADER_THREAD.completeCallback], rdx
    ret
model_loader_thread_set_complete_callback ENDP

; ============================================================================

; model_loader_thread_destroy(RCX = loaderThread)
; Free thread resources
PUBLIC model_loader_thread_destroy
model_loader_thread_destroy PROC
    push rbx
    
    mov rbx, rcx
    
    ; Request cancellation
    mov byte [rbx + MODEL_LOADER_THREAD.canceled], 1
    
    ; Wait for thread
    mov rcx, [rbx + MODEL_LOADER_THREAD.threadHandle]
    mov rdx, 5000                  ; 5 second timeout
    call WaitForSingleObject
    
    ; Close thread handle
    mov rcx, [rbx + MODEL_LOADER_THREAD.threadHandle]
    cmp rcx, INVALID_HANDLE_VALUE
    je .skip_close
    call CloseHandle
.skip_close:
    
    ; Free model path
    mov rcx, [rbx + MODEL_LOADER_THREAD.modelPath]
    cmp rcx, 0
    je .skip_path
    call free
.skip_path:
    
    ; Free context
    mov rcx, rbx
    call free
    
    pop rbx
    ret
model_loader_thread_destroy ENDP

; ============================================================================

END
