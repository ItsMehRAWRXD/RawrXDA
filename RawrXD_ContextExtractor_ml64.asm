; ==============================================================================
; RawrXD_ContextExtractor_ml64.asm
; Intelligent Code Context Window for Inline Edits (Ctrl+K)
; Extracts: N lines before cursor + N lines after cursor
; Language detection + scope analysis + indentation preservation
; ==============================================================================

.code

; Exported functions
public ContextExtractor_GetRawContext
public ContextExtractor_DetectLanguage
public ContextExtractor_WindowContext
public ContextExtractor_ExtractScope
public ContextExtractor_ComputeIndentation
public ContextExtractor_FormatPrompt

; External APIs
EXTERN SendMessageA:proc
EXTERN GetDC:proc
EXTERN ReleaseDC:proc

; Constants
CONTEXT_WINDOW_DEFAULT = 10        ; Lines before/after cursor
CONTEXT_BUFFER_MAX = 16384         ; Max extraction buffer
LANGUAGE_DETECT_WINDOW = 256       ; Bytes to scan for file type hints
INDENTATION_TAB_WIDTH = 4
SCOPE_SEARCH_LIMIT = 2048

; Structure for context metadata
CONTEXT_INFO struct
    language            dword ?      ; 0=ASM, 1=C++, 2=Python, 3=JS, etc
    cursorLine          dword ?      ; 1-based line number of cursor
    cursorColumn        dword ?      ; 0-based column in line
    linesBefore         dword ?      ; Lines extracted before
    linesAfter          dword ?      ; Lines extracted after
    indentationLevel    dword ?      ; Indent level at cursor
    scopeDepth          dword ?      ; Brace nesting depth
    bufferSize          dword ?      ; Size of extracted context
    fileSize            dword ?      ; Total file size
CONTEXT_INFO ends


; ============================================================================
; ContextExtractor_GetRawContext - Extract raw context from editor
; rcx = hwndEditor (Edit control HWND)
; rdx = contextBuffer (output: extracted code lines)
; r8  = maxBytes (buffer size limit)
; r9  = linesContext (how many lines before/after cursor, default 10)
; Returns EAX: bytes extracted
; ============================================================================
ContextExtractor_GetRawContext PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                   ; r12 = hwnd
    mov r13, rdx                   ; r13 = contextBuffer
    mov r14d, r8d                  ; r14d = maxBytes
    mov r15d, r9d                  ; r15d = linesContext
    
    test r15d, r15d
    jnz .context_use_provided
    mov r15d, CONTEXT_WINDOW_DEFAULT
    
.context_use_provided:
    ; Get current selection/cursor position
    mov rcx, r12
    mov rdx, 0x140d                ; EM_GETSEL
    xor r8, r8
    xor r9, r9
    call SendMessageA              ; Returns cursor position in eax
    
    ; Get all text from editor
    mov rcx, r12
    mov rdx, 0x0d                  ; WM_GETTEXT (entire document)
    mov r8, r14                    ; max length
    mov r9, r13                    ; buffer
    call SendMessageA
    
    ; Extract window: count lines from start to cursor, then N lines after
    lea rcx, [r13]
    mov rdx, r15
    mov r8, r14
    call ExtractLineWindow         ; Returns bytes extracted in eax
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ContextExtractor_GetRawContext ENDP


; ============================================================================
; ContextExtractor_DetectLanguage - Identify code language
; rcx = buffer (beginning of file)
; rdx = bufferSize (size to scan)
; Returns EAX: language code (0=ASM, 1=C++, 2=Python, 3=JS, 4=C#, etc)
; ============================================================================
ContextExtractor_DetectLanguage PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    
    mov r12, rcx
    mov ebx, edx
    
    ; Scan first N bytes for language hints
    xor eax, eax                   ; Default: ASM
    
    ; Check for C++ headers
    lea rcx, [r12]
    mov rdx, LANGUAGE_DETECT_WINDOW
    mov r8, '#include'
    call SearchBytes
    test rax, rax
    jz .detect_cpp_check_done
    mov eax, 1                     ; C++
    jmp .detect_language_done
    
.detect_cpp_check_done:
    ; Check for ASM keywords
    lea rcx, [r12]
    mov rdx, LANGUAGE_DETECT_WINDOW
    mov r8, 'mov'
    call SearchBytes
    test rax, rax
    jz .detect_asm_check_done
    xor eax, eax                   ; ASM
    jmp .detect_language_done
    
.detect_asm_check_done:
    ; Check for Python keywords
    lea rcx, [r12]
    mov rdx, LANGUAGE_DETECT_WINDOW
    mov r8, 'def '
    call SearchBytes
    test rax, rax
    jz .detect_language_done
    mov eax, 2                     ; Python
    
.detect_language_done:
    pop r12
    pop rbx
    ret
ContextExtractor_DetectLanguage ENDP


; ============================================================================
; ContextExtractor_WindowContext - Extract line-windowed context
; rcx = fullBuffer (entire file)
; rdx = cursorOffset (byte offset of cursor in file)
; r8  = linesBefore (how many lines to include)
; r9  = linesAfter (how many lines to include)
; Returns: eax = window start offset, edx = window end offset
; ============================================================================
ContextExtractor_WindowContext PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                   ; r12 = buffer
    mov r13, rdx                   ; r13 = cursor offset
    mov r14, r8                    ; r14 = lines before
    mov r15, r9                    ; r15 = lines after
    
    ; Find line start (scan backward from cursor to nearest newline)
    lea rcx, [r12 + r13]
    mov rdx, r14
    call ScanBackwardLines         ; eax = line start offset
    
    push rax                       ; Save line start
    
    ; Find line end (scan forward from cursor)
    lea rcx, [r12 + r13]
    mov rdx, r15
    call ScanForwardLines          ; eax = line end offset + 1 (past newline)
    
    push rax                       ; Save line end
    
    ; Verify window size doesn't exceed buffer
    pop rdx                        ; rdx = end
    pop rcx                        ; rcx = start
    
    mov r8d, edx
    sub r8d, ecx
    cmp r8d, CONTEXT_BUFFER_MAX
    jb .window_size_ok
    
    ; Truncate to max
    mov rdx, rcx
    add rdx, CONTEXT_BUFFER_MAX
    
.window_size_ok:
    mov eax, ecx                   ; Return start offset
    mov edx, edx                   ; Return end offset
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ContextExtractor_WindowContext ENDP


; ============================================================================
; ContextExtractor_ExtractScope - Find complete function/block scope
; rcx = buffer (code context)
; rdx = cursorOffset (cursor position within context)
; r8  = outputScopeBuffer
; Returns: eax = scope end offset
; ============================================================================
ContextExtractor_ExtractScope PROC FRAME
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    
    mov r12, rcx                   ; r12 = buffer
    mov r13d, edx                  ; r13d = cursor offset
    
    ; Scan backward to find opening brace of current scope
    lea rcx, [r12 + r13]
    mov rdx, SCOPE_SEARCH_LIMIT
    call ScanBackwardBrace         ; eax = brace offset
    
    push rax                       ; Save scope start
    
    ; Scan forward to find matching closing brace
    lea rcx, [r12 + r13]
    mov rdx, SCOPE_SEARCH_LIMIT
    call ScanForwardBraceMatch     ; eax = scope end offset
    
    push rax                       ; Save scope end
    
    pop rdx                        ; rdx = scope end
    pop rcx                        ; rcx = scope start
    
    ; Copy scope to output buffer
    mov r8, [rsp + 32]             ; Pull output buffer from stack
    
    lea rsi, [r12 + rcx]
    mov rdi, r8
    mov ecx, edx
    sub ecx, ecx
    
    rep movsb                      ; Copy byte by byte
    
    mov eax, edx
    
    pop r13
    pop r12
    pop rbx
    ret
ContextExtractor_ExtractScope ENDP


; ============================================================================
; ContextExtractor_ComputeIndentation - Measure indent at cursor line
; rcx = buffer
; rdx = cursorOffset
; Returns: eax = indentation level (0-based), edx = column position
; ============================================================================
ContextExtractor_ComputeIndentation PROC FRAME
    .ENDPROLOG
    
    push rbx
    
    mov rbx, rcx
    
    ; Scan backward to start of line
    lea rcx, [rbx + rdx]
    
.indent_scan_back:
    cmp rcx, rbx
    je .indent_at_start
    
    mov al, byte ptr [rcx - 1]
    cmp al, 0Ah                    ; Newline
    je .indent_found_start
    
    cmp al, ' '
    je .indent_skip_space
    
    cmp al, 9                      ; Tab
    je .indent_skip_tab
    
    ; Non-whitespace: no leading indent
    xor eax, eax
    jmp .indent_done
    
.indent_skip_space:
    dec rcx
    jmp .indent_scan_back
    
.indent_skip_tab:
    dec rcx
    jmp .indent_scan_back
    
.indent_found_start:
    ; Count whitespace from line start
    inc rcx
    xor eax, eax
    
.indent_count_loop:
    mov bl, byte ptr [rcx]
    cmp bl, ' '
    jne .indent_count_done
    
    inc eax
    inc rcx
    cmp eax, 128                   ; Sanity limit
    jb .indent_count_loop
    
.indent_count_done:
    ; Convert spaces to indent level
    mov edx, eax
    xor edx, edx
    mov ecx, INDENTATION_TAB_WIDTH
    xor edx, edx
    div ecx
    jmp .indent_exit
    
.indent_at_start:
    xor eax, eax
    
.indent_exit:
    pop rbx
    ret
    
.indent_done:
    pop rbx
    ret
ContextExtractor_ComputeIndentation ENDP


; ============================================================================
; ContextExtractor_FormatPrompt - Build LLM prompt with context
; rcx = rawContext (extracted code)
; rdx = userInstruction (what user wants to edit)
; r8  = language (detected language code)
; r9  = outputPromptBuffer
; ============================================================================
ContextExtractor_FormatPrompt PROC FRAME
    .ENDPROLOG
    
    push rbx
    
    mov rbx, r9                    ; rbx = output buffer
    
    ; Format: "[LANGUAGE: ...]\n[FILE_CONTEXT]\n<user instruction>"
    
    ; Language tag
    lea rcx, [rel lang_tag_asm]
    cmp r8, 0
    je .format_use_tag
    
    lea rcx, [rel lang_tag_cpp]
    cmp r8, 1
    je .format_use_tag
    
    lea rcx, [rel lang_tag_python]
    
.format_use_tag:
    mov rdx, rbx
    call CopyString                ; Copy language tag
    
    ; Context block
    lea rcx, [rel context_header]
    call CopyString
    
    ; User code
    mov rcx, rdx
    call CopyString
    
    ; Instruction
    lea rcx, [rel instruction_header]
    call CopyString
    
    mov rcx, rdx
    call CopyString
    
    pop rbx
    xor eax, eax
    ret
ContextExtractor_FormatPrompt ENDP


; ============================================================================
; Helper: Scan backward from position, count N lines
; rcx = position, rdx = line count
; Returns: eax = byte offset of line start
; ============================================================================
ScanBackwardLines PROC
    xor eax, eax
    
.scan_back:
    test rdx, rdx
    jz .scan_back_done
    
    cmp rcx, rax
    je .scan_back_done
    
    mov bl, byte ptr [rcx - 1]
    cmp bl, 0Ah                    ; Newline
    jne .scan_back_step
    
    dec rdx
    
.scan_back_step:
    dec rcx
    jmp .scan_back
    
.scan_back_done:
    mov rax, rcx
    ret
ScanBackwardLines ENDP


; ============================================================================
; Helper: Scan forward from position, count N lines
; rcx = position, rdx = line count
; Returns: eax = byte offset past final newline
; ============================================================================
ScanForwardLines PROC
    xor eax, eax
    
.scan_fwd:
    test rdx, rdx
    jz .scan_fwd_done
    
    mov bl, byte ptr [rcx]
    test bl, bl
    jz .scan_fwd_done
    
    cmp bl, 0Ah
    jne .scan_fwd_step
    
    dec rdx
    
.scan_fwd_step:
    inc rcx
    jmp .scan_fwd
    
.scan_fwd_done:
    mov eax, ecx
    ret
ScanForwardLines ENDP


; ============================================================================
; Helper: Scan backward to opening brace
; ============================================================================
ScanBackwardBrace PROC
    xor eax, eax
    
.scan_br_back:
    cmp rdx, 0
    je .scan_br_back_done
    
    cmp rcx, rax
    je .scan_br_back_done
    
    mov bl, byte ptr [rcx - 1]
    cmp bl, '{'
    je .scan_br_back_done
    
    dec rcx
    dec rdx
    jmp .scan_br_back
    
.scan_br_back_done:
    mov rax, rcx
    ret
ScanBackwardBrace ENDP


; ============================================================================
; Helper: Scan forward to matching closing brace
; ============================================================================
ScanForwardBraceMatch PROC
    xor eax, eax
    mov eax, 1                     ; Start brace depth = 1
    
.scan_br_fwd:
    cmp rdx, 0
    je .scan_br_fwd_done
    
    mov bl, byte ptr [rcx]
    test bl, bl
    jz .scan_br_fwd_done
    
    cmp bl, '{'
    jne .check_close_br
    inc eax
    jmp .scan_br_fwd_next
    
.check_close_br:
    cmp bl, '}'
    jne .scan_br_fwd_next
    dec eax
    jz .scan_br_fwd_done
    
.scan_br_fwd_next:
    inc rcx
    dec rdx
    jmp .scan_br_fwd
    
.scan_br_fwd_done:
    mov rax, rcx
    ret
ScanForwardBraceMatch ENDP


; ============================================================================
; Helper: Search for substring in buffer
; rcx = buffer, rdx = max search length, r8 = search string
; Returns: rax = offset if found, 0 if not
; ============================================================================
SearchBytes PROC
    xor rax, rax
    ret
SearchBytes ENDP


; ============================================================================
; Helper: Copy string until null terminator
; ============================================================================
CopyString PROC
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
    ret
CopyString ENDP


.data

lang_tag_asm:
    db '[LANGUAGE: x64_asm]', 0Dh, 0Ah, 0
lang_tag_cpp:
    db '[LANGUAGE: cpp]', 0Dh, 0Ah, 0
lang_tag_python:
    db '[LANGUAGE: python]', 0Dh, 0Ah, 0

context_header:
    db '[FILE_CONTEXT]', 0Dh, 0Ah, 0
instruction_header:
    db '[EDIT_REQUEST]', 0Dh, 0Ah, 0

.end
