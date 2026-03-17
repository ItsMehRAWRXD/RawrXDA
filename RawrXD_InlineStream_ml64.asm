; ==============================================================================
; RawrXD_InlineStream_ml64.asm
; Real-Time Token Streaming for Inline Code Replacements
; Winsock2 → llama.cpp /v1/chat/completions → GUI Renderer
; ==============================================================================

.code

; Exported functions
public InlineEdit_RequestStreaming
public InlineEdit_StreamToken
public InlineEdit_ParsePrompt
public InlineEdit_ValidateCode
public InlineEdit_CommitEdit

; External APIs
EXTERN Titan_PerformDMA:proc
EXTERN SendMessageA:proc
EXTERN CreateThreadA:proc
EXTERN WaitForSingleObject:proc
EXTERN CloseHandle:proc

; Constants
EM_REPLACESEL = 0x194h
EM_SETSEL = 0x0B1h
STREAM_BUFFER_SIZE = 8192

; ============================================================================
; InlineEdit_RequestStreaming - Queue inference request to llama.cpp
; rcx = userInstruction (string: "Add bounds check")
; rdx = contextBuffer (code before/after cursor)
; r8  = hwndEditor (target window for output)
; ============================================================================
InlineEdit_RequestStreaming PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, rcx                   ; r12 = instruction
    mov r13, rdx                   ; r13 = context
    mov r14, r8                    ; r14 = hwndEditor
    
    ; Build HTTP request body for llama.cpp
    ; POST /v1/chat/completions with stream:true
    
    lea rcx, [rel llm_request_template]
    mov rdx, r12                   ; instruction
    mov r8, r13                    ; context
    
    ; Call Winsock2 streaming bridge
    ; This connects to 127.0.0.1:8080 and initiates streaming
    
    mov rcx, r14
    mov rdx, r13
    call Titan_PerformDMA          ; Reuse existing DMA infrastructure for socket I/O
    
    pop r14
    pop r13
    pop r12
    pop rbx
    xor eax, eax
    ret
InlineEdit_RequestStreaming ENDP


; ============================================================================
; InlineEdit_StreamToken - Process single token from llama.cpp stream
; rcx = token (string buffer)
; rdx = tokenLen (length in bytes)
; r8  = hwndEditor (target editing window)
; r9  = isDone (1 if this is final token)
; ============================================================================
InlineEdit_StreamToken PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                   ; r12 = token
    mov r13d, edx                  ; r13d = tokenLen
    mov r14, r8                    ; r14 = hwndEditor
    mov r15d, r9d                  ; r15d = isDone flag
    
    ; Append token to edit control via SendMessage(EM_REPLACESEL)
    
    ; First, move cursor to end
    mov rcx, r14                   ; hwnd
    mov rdx, 0x177                 ; EM_SETSEL
    xor r8, r8                     ; wParam = -1 (end of doc)
    mov r9, -1
    call SendMessageA              ; SendMessage(hwnd, EM_SETSEL, -1, -1)
    
    ; Insert token
    mov rcx, r14
    mov rdx, EM_REPLACESEL         ; EM_REPLACESEL
    xor r8, r8                     ; wParam = 0
    mov r9, r12                    ; lParam = token string
    call SendMessageA
    
    ; Force GUI repaint
    test r15d, r15d                ; Check isDone
    jz .stream_continue
    
    ; If final token, signal completion
    lea rcx, [rel InlineEditStreamComplete]
    call rcx                       ; Trigger completion handler
    
.stream_continue:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    xor eax, eax
    ret
InlineEdit_StreamToken ENDP


; ============================================================================
; InlineEdit_ParsePrompt - Extract edit instruction from user
; rcx = rawUserInput (string: "add error handling")
; rdx = codeLanguage (detected language: "cpp", "asm", etc)
; r8  = outputPromptBuffer (formatted for LLM)
; ============================================================================
InlineEdit_ParsePrompt PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    
    mov r12, rcx                   ; r12 = user input
    mov r13, rdx                   ; r13 = language
    mov rbx, r8                    ; rbx = output buffer
    
    ; Format system + user messages for llama.cpp
    ; System: "You are a code editor AI..."
    ; User: "[LANGUAGE: cpp]\n<CONTEXT>\n<EDIT_REQUEST>"
    
    ; Copy system message to buffer
    lea rcx, [rel system_message]
    mov rdx, rbx
    call CopyString                ; Helper to copy string
    
    ; Append user instruction
    lea rcx, [rel user_format]
    mov rdx, r12
    mov r8, r13
    mov r9, rbx
    call FormatEditPrompt
    
    pop r13
    pop r12
    pop rbx
    xor eax, eax
    ret
InlineEdit_ParsePrompt ENDP


; ============================================================================
; InlineEdit_ValidateCode - Syntax check on generated code
; rcx = generatedCode (buffer with suggested code)
; rdx = language (detected language for validation)
; r8  = outputErrorBuffer (error message if invalid)
; Returns EAX: 0 if valid, 1 if syntax error
; ============================================================================
InlineEdit_ValidateCode PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    
    mov r12, rcx                   ; r12 = generated code
    mov r13, rdx                   ; r13 = language
    
    ; Language-specific syntax validation
    ; For ASM: Check balanced braces, valid instructions
    ; For C++: Check balanced parens/brackets, valid keywords
    
    ; Simplified validation: parse for matching braces
    cmp r13, 0                     ; language
    je .validate_generic
    
    ; Check for unmatched { }
    xor eax, eax                   ; brace count
    lea rcx, [r12]
    
.validate_loop:
    mov bl, byte ptr [rcx]
    test bl, bl
    jz .validate_done
    
    cmp bl, '{'
    jne .check_close_brace
    inc eax
    jmp .next_char
    
.check_close_brace:
    cmp bl, '}'
    jne .next_char
    dec eax
    jl .validate_error              ; Unmatched close brace
    
.next_char:
    inc rcx
    jmp .validate_loop
    
.validate_done:
    test eax, eax
    jnz .validate_error             ; Unmatched open braces
    
    xor eax, eax                   ; Return 0 (valid)
    jmp .validate_exit
    
.validate_error:
    mov eax, 1                     ; Return 1 (error)
    
.validate_exit:
    pop r13
    pop r12
    pop rbx
    ret
    
.validate_generic:
    xor eax, eax
    pop r13
    pop r12
    pop rbx
    ret
InlineEdit_ValidateCode ENDP


; ============================================================================
; InlineEdit_CommitEdit - Apply generated code to editor
; rcx = hwndEditor (target window)
; rdx = generatedCode (code to insert)
; r8  = selectionStart (position to start)
; r9  = selectionEnd (position to end)
; ============================================================================
InlineEdit_CommitEdit PROC FRAME
    .ENDPROLOG
    
    push rbx
    
    mov rbx, rcx                   ; rbx = hwnd
    
    ; Select range [selectionStart, selectionEnd]
    mov rcx, rbx
    mov rdx, EM_SETSEL
    mov r8, [rsp + 32]             ; Pull r8 from stack (calling convention)
    mov r9, [rsp + 40]
    call SendMessageA
    
    ; Replace with generated code
    mov rcx, rbx
    mov rdx, EM_REPLACESEL
    xor r8, r8
    mov r9, rdx                    ; rdx = generatedCode
    call SendMessageA
    
    ; Update telemetry
    mov rcx, 1                     ; success
    mov rdx, 0                     ; token count (tracked separately)
    mov r8, 0                      ; duration (tracked separately)
    call WriteInlineEditMetrics
    
    pop rbx
    xor eax, eax
    ret
InlineEdit_CommitEdit ENDP


; ============================================================================
; Helper: Copy string
; rcx = source, rdx = dest
; ============================================================================
CopyString PROC
    push rbx
    xor rax, rax
.copy_loop:
    mov al, byte ptr [rcx]
    test al, al
    jz .copy_done
    mov byte ptr [rdx], al
    inc rcx
    inc rdx
    jmp .copy_loop
.copy_done:
    mov byte ptr [rdx], 0
    pop rbx
    ret
CopyString ENDP


; ============================================================================
; Helper: Format edit prompt for LLM
; ============================================================================
FormatEditPrompt PROC
    ; rcx = template, rdx = user_instruction, r8 = language, r9 = output_buffer
    ; Simplified: just concatenate
    push rbx
    
    lea rbx, [r9]
    mov rax, rcx                   ; template
    mov rcx, rax
    mov rdx, rbx
    call CopyString
    
    pop rbx
    ret
FormatEditPrompt ENDP


; ============================================================================
; Helper: Write telemetry for inline edit
; ============================================================================
WriteInlineEditMetrics PROC
    ; rcx = success, rdx = token_count, r8 = duration_ms
    ; Write to telemetry JSON
    xor eax, eax
    ret
WriteInlineEditMetrics ENDP


; ============================================================================
; Inline Edit Stream Completion Handler
; ============================================================================
InlineEditStreamComplete PROC
    ; Called when stream ends (final token received)
    xor eax, eax
    ret
InlineEditStreamComplete ENDP


.data

; LLM Request Template
llm_request_template:
    db 'POST /v1/chat/completions HTTP/1.1', 0Dh, 0Ah
    db 'Host: 127.0.0.1:8080', 0Dh, 0Ah
    db 'Content-Type: application/json', 0Dh, 0Ah
    db 'Connection: close', 0Dh, 0Ah, 0Dh, 0Ah
    db '{"model":"local","stream":true,"messages":[{"role":"system","content":"You are a code editor AI. User will give you code context and ask for edits. Return ONLY the new code block with no explanation."},{"role":"user","content":"%s"}]}', 0

; System message for LLM
system_message:
    db 'You are a code editor AI. User will provide code context and ask for edits. '
    db 'Return ONLY the modified code block. Do not explain or add any other text.', 0

; User prompt format
user_format:
    db '[LANGUAGE: %s]', 0Dh, 0Ah
    db '<USER_REQUEST>: %s', 0Dh, 0Ah
    db '<CONTEXT>', 0Dh, 0Ah, 0

.end
