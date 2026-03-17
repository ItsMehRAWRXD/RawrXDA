; ============================================================================
; RawrXD_MASM_Editor_MLCompletion.asm - ML-based code completion integration
; ============================================================================
; Integrates with RawrXD autonomy stack for:
; - Code suggestion on Ctrl+Space
; - Inline completions
; - Error detection + fixes
; ============================================================================

.CODE

; ============================================================================
; MLCompletion_RequestSuggestions(rcx = editor_handle, rdx = line_num, r8 = col)
;
; Query ML for code suggestions at cursor position
; Returns: rax = suggestion_count
; ============================================================================
MLCompletion_RequestSuggestions PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 32 + 1024  ; Buffer for HTTP request
    .ENDPROLOG

    push    rbx
    sub     rsp, 1024
    
    ; rcx = editor_handle, rdx = line_num, r8 = column
    
    ; Build HTTP request to localhost:11434
    ; Extract current context (function, surrounding code)
    
    ; Get current line content
    mov     rax, [rcx + 8]              ; Line buffer
    mov     rax, [rax + rdx*8]          ; Current line
    
    ; Build JSON request
    lea     rbx, [rsp]                  ; rbx = request buffer
    
    ; Request format:
    ; {
    ;   "model": "codellama:7b",
    ;   "prompt": "current_code_context",
    ;   "stream": false,
    ;   "num_predict": 50,
    ;   "temperature": 0.2
    ; }
    
    ; For now: return placeholder suggestion count
    mov     rax, 3                      ; 3 suggestions
    
    add     rsp, 1024
    pop     rbx
    ret
MLCompletion_RequestSuggestions ENDP


; ============================================================================
; MLCompletion_ShowPopup(rcx = hwnd, rdx = x_pos, r8 = y_pos)
;
; Display completion popup at cursor position
; ============================================================================
MLCompletion_ShowPopup PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    ; rcx = hwnd, rdx = x_pos, r8 = y_pos
    
    ; Create owner-drawn popup showing suggestions
    ; Items:
    ; 1. mov rax, [rbx]    (current context)
    ; 2. add rax, rbx      (arithmetic)
    ; 3. call Function     (function call)
    
    ; Display logic:
    ; - Draw semi-transparent box
    ; - Draw suggestion list (highlighted item)
    ; - Allow Up/Down arrow to navigate
    ; - Enter to select
    ; - Esc to dismiss
    
    mov     rax, 1                      ; Show popup
    ret
MLCompletion_ShowPopup ENDP


; ============================================================================
; MLCompletion_GetSuggestion(rcx = suggestions_ptr, rdx = index)
;
; Get specific suggestion text
; Returns: rax = suggestion_text, rdx = length
; ============================================================================
MLCompletion_GetSuggestion PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    ; rcx = suggestions array, rdx = index
    
    ; Fetch suggestion at index
    mov     rax, [rcx + rdx*8]          ; suggestion pointer
    mov     rdx, [rcx + rdx*8 + 8]      ; length
    
    ret
MLCompletion_GetSuggestion ENDP


; ============================================================================
; MLCompletion_InsertSuggestion(rcx = editor, rdx = suggestion_text)
;
; Insert selected suggestion into editor
; ============================================================================
MLCompletion_InsertSuggestion PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    ; rcx = editor_handle, rdx = suggestion text
    
    ; Get current line and column
    mov     r8, [rcx + 2080]            ; cursor pointer
    mov     r9d, [r8]                   ; byte offset
    
    ; Call Editor_InsertText or similar
    ; (implementation depends on exact insertion point)
    
    mov     rax, 1
    ret
MLCompletion_InsertSuggestion ENDP


; ============================================================================
; MLCompletion_AnalyzeError(rcx = editor, rdx = line_num)
;
; Send line to ML for error checking
; Returns: rax = error_code (0=ok, 1=syntax_error, 2=logical_error)
; ============================================================================
MLCompletion_AnalyzeError PROC FRAME
    .ALLOCSTACK 32 + 512
    .ENDPROLOG

    sub     rsp, 512
    
    ; rcx = editor_handle, rdx = line_num
    
    ; Get line content
    mov     rax, [rcx + 8]
    mov     rax, [rax + rdx*8]          ; Line pointer
    
    ; Send to ML for analysis
    ; Check for:
    ; - Typos in instructions
    ; - Invalid register usage
    ; - Syntax errors
    
    ; Return error status
    xor     eax, eax                    ; 0 = OK for now
    
    add     rsp, 512
    ret
MLCompletion_AnalyzeError ENDP


; ============================================================================
; MLCompletion_GetErrorFix(rcx = error_line, rdx = error_type)
;
; Request fix suggestion from ML
; Returns: rax = fixed_text, rdx = length
; ============================================================================
MLCompletion_GetErrorFix PROC FRAME
    .ALLOCSTACK 32 + 1024
    .ENDPROLOG

    sub     rsp, 1024
    
    ; rcx = original line, rdx = error type
    
    ; Query ML with:
    ; "Fix this x64 MASM line: <original_line>"
    
    ; Example responses:
    ; "mov rax, [rbx]"        (for typo fix)
    ; "add rcx, 1"            (for instruction fix)
    ; "push rbp; mov rbp, rsp" (for missing setup)
    
    xor     eax, eax                    ; Placeholder
    
    add     rsp, 1024
    ret
MLCompletion_GetErrorFix ENDP


; ============================================================================
; MLCompletion_OnKeyDown(rcx = hwnd, rdx = vk_code)
;
; Handle keyboard input for completion
; ============================================================================
MLCompletion_OnKeyDown PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    ; rcx = hwnd, rdx = vk_code
    
    .IF rdx == 27                       ; ESC - dismiss popup
        ; Hide completion popup
        mov     rax, 1
        
    .ELSEIF rdx == 38                   ; UP - previous suggestion
        ; Select previous item
        mov     rax, 2
        
    .ELSEIF rdx == 40                   ; DOWN - next suggestion
        ; Select next item
        mov     rax, 3
        
    .ELSEIF rdx == 13                   ; Enter - accept suggestion
        ; Insert selected suggestion
        mov     rax, 4
        
    .ELSEIF rdx == 32 && g_bCtrlPressed ; Ctrl+Space - request completions
        ; Show completion popup
        call    MLCompletion_RequestSuggestions
        mov     rax, 5
        
    .ELSE
        ; Dismiss popup on other keys
        mov     rax, 0
        
    .ENDIF
    
    ret
MLCompletion_OnKeyDown ENDP


; ============================================================================
; MLCompletion_ValidateSyntax(rcx = line_text)
;
; Quick syntax validation (doesn't require ML)
; ============================================================================
MLCompletion_ValidateSyntax PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 32
    .ENDPROLOG

    push    rbx
    
    ; rcx = line_text
    
    mov     rsi, rcx
    xor     ebx, ebx                    ; Error count
    
    ; Check for common MASM syntax errors:
    ; - Mismatched brackets []
    ; - Mismatched quotes ""
    ; - Invalid instruction format
    
    xor     eax, eax                    ; Count brackets
    xor     edx, edx                    ; Count quotes
    
    .WHILE BYTE PTR [rsi] != 0
        movzx   ecx, BYTE PTR [rsi]
        
        .IF cl == '['
            inc     eax
        .ELSEIF cl == ']'
            dec     eax
        .ELSEIF cl == '"'
            inc     edx
        .ELSE
            ; Check for other issues
        .ENDIF
        
        inc     rsi
    .ENDW
    
    ; Return error state
    cmp     eax, 0
    jne     BracketError
    test    edx, 1
    jnz     QuoteError
    
    xor     eax, eax                    ; No error
    
    pop     rbx
    ret
    
    BracketError:
    mov     eax, 1                      ; Bracket mismatch
    pop     rbx
    ret
    
    QuoteError:
    mov     eax, 2                      ; Quote mismatch
    pop     rbx
    ret
MLCompletion_ValidateSyntax ENDP

; Global state
g_bCtrlPressed      DB      0

END
