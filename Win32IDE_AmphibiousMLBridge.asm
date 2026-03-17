; ==============================================================================
; Win32IDE_AmphibiousMLBridge.asm
; Pure x64 MASM Bridge between Win32IDE and RawrXD Amphibious ML System
; Real-time token streaming to editor surface + JSON telemetry output
; ==============================================================================

.code

; Exported functions
public Win32IDE_InitializeML
public Win32IDE_StartInference
public Win32IDE_StreamTokenToEditor
public Win32IDE_CommitTelemetry
public Win32IDE_CancelInference

; External APIs
EXTERN SendMessageA:proc
EXTERN GetWindowTextA:proc
EXTERN SetWindowTextA:proc
EXTERN CreateFileA:proc
EXTERN WriteFile:proc
EXTERN CloseHandle:proc

; Import from RawrXD Amphibious
EXTERN Titan_PerformDMA:proc
EXTERN TryRealLLMInferenceStreaming:proc

; Constants
EM_GETSEL = 0x0B0h
EM_SETSEL = 0x0B1h
EM_REPLACESEL = 0x194h
STREAM_BUFFER_SIZE = 8192
MAX_TOKENS = 512

; ============================================================================
; Win32IDE_InitializeML - Initialize ML inference engine for IDE
; rcx = hEditorWindow (HWND of editor control)
; rdx = hStatusBar (HWND of status bar for progress)
; r8 = modelPath (path to GGUF model or NULL for llama.cpp)
; ============================================================================
Win32IDE_InitializeML PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    
    mov r12, rcx                   ; r12 = editor hwnd
    mov r13, rdx                   ; r13 = status bar hwnd
    
    ; Initialize inference runtime
    xor ecx, ecx
    call Titan_PerformDMA          ; Warmup DMA path
    
    ; Status: "ML Runtime Initialized"
    mov rcx, r13
    lea rdx, [rel msg_ml_init]
    call SetWindowTextA
    
    pop r13
    pop r12
    pop rbx
    xor eax, eax
    ret
Win32IDE_InitializeML ENDP


; ============================================================================
; Win32IDE_StartInference - Queue inference request with context
; rcx = hEditor (editor window)
; rdx = selectedCode (code selection/context)
; r8 = userPrompt (what user wants to do)
; r9 = outputBuffer (accumulate tokens here)
; ============================================================================
Win32IDE_StartInference PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, rcx                   ; r12 = editor hwnd
    mov r13, rdx                   ; r13 = selected code
    mov r14, r8                    ; r14 = user prompt
    
    ; Build LLM request: "[LANGUAGE: cpp]\n[CONTEXT]\n<selectedCode>\n[REQUEST]\n<userPrompt>"
    lea rcx, [r9]                  ; Output buffer
    mov rdx, r13                   ; Context
    mov r8, r14                    ; Prompt
    call FormatInferencePrompt
    
    ; Send to llama.cpp or GPU DMA runtime
    mov rcx, r9
    call Titan_PerformDMA
    
    pop r14
    pop r13
    pop r12
    pop rbx
    xor eax, eax
    ret
Win32IDE_StartInference ENDP


; ============================================================================
; Win32IDE_StreamTokenToEditor - Process single token, append to editor
; rcx = hEditor (target editor window)
; rdx = token (UTF-8 string token)
; r8 = tokenLen (length in bytes)
; r9 = isDone (1 if final token, 0 if more coming)
; ============================================================================
Win32IDE_StreamTokenToEditor PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    
    mov r12, rcx                   ; r12 = editor hwnd
    
    ; Move cursor to end of document
    mov rcx, r12
    mov rdx, EM_SETSEL
    mov r8, -1
    mov r9, -1
    call SendMessageA
    
    ; Insert token via EM_REPLACESEL
    mov rcx, r12
    mov rdx, EM_REPLACESEL
    xor r8, r8                     ; wParam = 0
    mov r9, rdx                    ; rdx = token
    call SendMessageA
    
    ; If final token, trigger validation
    test r9d, r9d                  ; Check isDone
    jz .stream_continue
    
    ; Mark stream complete
    lea rcx, [rel msg_stream_done]
    call LogCompletion
    
.stream_continue:
    pop r12
    pop rbx
    xor eax, eax
    ret
Win32IDE_StreamTokenToEditor ENDP


; ============================================================================
; Win32IDE_CommitTelemetry - Write JSON telemetry artifact
; rcx = filePath (output JSON path)
; rdx = tokenCount (total tokens generated)
; r8 = durationMs (inference duration in milliseconds)
; r9 = success (1 if successful, 0 if failed)
; ============================================================================
Win32IDE_CommitTelemetry PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                   ; r12 = filepath
    mov r13d, edx                  ; r13d = token count
    mov r14, r8                    ; r14 = duration (qword)
    mov r15d, r9d                  ; r15d = success flag
    
    ; Create output file
    mov rcx, r12
    mov rdx, 2                     ; GENERIC_WRITE
    mov r8, 1                      ; FILE_SHARE_READ
    mov r9, 0
    push 2                         ; OPEN_ALWAYS
    push 0x80                      ; FILE_ATTRIBUTE_NORMAL
    sub rsp, 32
    call CreateFileA
    add rsp, 32 + 16
    
    test rax, rax
    jz .telemetry_failed
    
    mov r12, rax                   ; r12 = hFile
    
    ; Build JSON payload
    lea rcx, [rsp - 4096]          ; Allocate JSON buffer on stack
    call BuildTelemetryJSON        ; Build JSON in buffer
    
    ; Write to file
    mov rcx, r12
    mov rdx, rcx                   ; JSON buffer
    mov r8, rax                    ; JSON size (returned from build)
    lea r9, [rsp - 4096 - 8]       ; Bytes written placeholder
    call WriteFile
    
    ; Close file
    mov rcx, r12
    call CloseHandle
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    xor eax, eax
    ret
    
.telemetry_failed:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    mov eax, 1
    ret
Win32IDE_CommitTelemetry ENDP


; ============================================================================
; Win32IDE_CancelInference - Abort ongoing inference, revert changes
; rcx = hEditor (editor window)
; rdx = originalText (text snapshot to revert to)
; ============================================================================
Win32IDE_CancelInference PROC FRAME
    .ENDPROLOG
    
    push rbx
    
    mov rbx, rcx                   ; rbx = editor hwnd
    
    ; Set editor text back to original
    mov rcx, rbx
    mov rdx, rdx                   ; rdx = original text (already in reg)
    call SetWindowTextA
    
    ; Log cancellation
    lea rcx, [rel msg_inference_canceled]
    call LogCompletion
    
    pop rbx
    xor eax, eax
    ret
Win32IDE_CancelInference ENDP


; ============================================================================
; Helper: Format inference prompt for LLM
; rcx = outputBuffer, rdx = selectedCode, r8 = userPrompt
; ============================================================================
FormatInferencePrompt PROC
    push rbx
    
    ; Write language tag
    mov rbx, rcx
    lea rcx, [rel prompt_lang_tag]
    call CopyString
    mov rcx, rbx
    add rcx, rax
    
    ; Write context
    lea rcx, [rel prompt_context_tag]
    call CopyString
    mov rcx, rbx
    add rcx, rax
    
    ; Append selected code
    mov rcx, rdx
    mov rdx, rcx
    add rdx, rax
    call CopyString
    
    ; Append user prompt
    lea rcx, [rel prompt_request_tag]
    mov rdx, rcx
    add rdx, rax
    call CopyString
    
    mov rcx, r8
    mov rdx, rcx
    add rdx, rax
    call CopyString
    
    pop rbx
    ret
FormatInferencePrompt ENDP


; ============================================================================
; Helper: Build JSON telemetry
; rcx = outputBuffer
; Modifies: r13d (tokens), r14 (duration), r15d (success)
; Returns: rax = JSON size
; ============================================================================
BuildTelemetryJSON PROC
    push rbx
    
    mov rbx, rcx                   ; rbx = output buffer
    
    ; Write JSON header
    mov rcx, rbx
    lea rdx, [rel json_header]
    call CopyString
    
    ; Write telemetry
    mov rcx, rbx
    add rcx, rax
    
    ; success field
    lea rdx, [rel json_success]
    test r15d, r15d
    jz .json_failure
    
    ; ... write success JSON ...
    
.json_failure:
    ; ... write failure JSON ...
    
    ; Write JSON footer
    lea rdx, [rel json_footer]
    call CopyString
    
    mov rax, rcx
    sub rax, rbx
    
    pop rbx
    ret
BuildTelemetryJSON ENDP


; ============================================================================
; Helper: Copy string until null terminator
; rcx = source, destination implicitly follows
; ============================================================================
CopyString PROC
    xor rax, rax
.copy_loop:
    mov dl, byte ptr [rcx]
    test dl, dl
    jz .copy_done
    mov byte ptr [rcx + rax], dl
    inc rax
    jmp .copy_loop
.copy_done:
    ret
CopyString ENDP


; ============================================================================
; Helper: Log completion message
; rcx = message string
; ============================================================================
LogCompletion PROC
    ; Stub: forward to debug output or telemetry
    ret
LogCompletion ENDP


.data

; Status messages
msg_ml_init db "[ML] Runtime Initialized", 0
msg_stream_done db "[ML] Token stream complete", 0
msg_inference_canceled db "[ML] Inference canceled", 0

; Prompt templates
prompt_lang_tag db "[LANGUAGE: cpp]", 0Dh, 0Ah, 0
prompt_context_tag db "[EDITOR_CONTEXT]", 0Dh, 0Ah, 0
prompt_request_tag db "[USER_REQUEST]", 0Dh, 0Ah, 0

; JSON templates
json_header db "{", 0Dh, 0Ah, "  ""telemetry"": {", 0Dh, 0Ah, 0
json_success db "    ""success"": true,", 0Dh, 0Ah, 0
json_failure db "    ""success"": false,", 0Dh, 0Ah, 0
json_footer db "  }", 0Dh, 0Ah, "}", 0

.end
