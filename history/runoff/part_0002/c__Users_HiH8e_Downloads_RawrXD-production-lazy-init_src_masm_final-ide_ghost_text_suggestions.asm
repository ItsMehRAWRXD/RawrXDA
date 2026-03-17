;==========================================================================
; ghost_text_suggestions.asm - Copilot-Style Ghost Text Suggestions
; ==========================================================================
; Features:
; - Inline code suggestions (ghost text with opacity)
; - Context-aware suggestions based on history
; - Tab to accept suggestion
; - Esc to dismiss suggestion
; - Multiple suggestion cycling
; - Visual differentiation (faded color)
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_SUGGESTIONS     EQU 10
MAX_SUGGESTION_LEN  EQU 256
GHOST_TEXT_TIMEOUT  EQU 3000            ; Show for 3 seconds max

; Ghost text states
GHOST_STATE_HIDDEN  EQU 0
GHOST_STATE_VISIBLE EQU 1
GHOST_STATE_ACCEPTED EQU 2

; Colors (COLORREF)
GHOST_TEXT_COLOR    EQU 0CCCCCC00h      ; Light gray for ghost text
NORMAL_TEXT_COLOR   EQU 00000000h       ; Black for normal text

; RichEdit messages (if used)
WM_CHAR             EQU 0102h
WM_KEYDOWN          EQU 0100h
EM_GETSEL           EQU 00B0h
EM_REPLACESEL       EQU 00C2h
EM_SETSEL           EQU 00B1h

;==========================================================================
; STRUCTURES
;==========================================================================
SUGGESTION STRUCT
    text            BYTE MAX_SUGGESTION_LEN DUP (?)
    confidence      DWORD ?             ; 0-100 (higher = more likely)
    source          DWORD ?             ; 0=history, 1=learning, 2=pattern
    display_time    QWORD ?
SUGGESTION ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Current suggestions
    CurrentSuggestions  SUGGESTION MAX_SUGGESTIONS DUP (<>)
    SuggestionCount     DWORD 0
    CurrentSuggestion   DWORD 0          ; Currently displayed suggestion index
    
    ; Ghost text state
    GhostTextState      DWORD GHOST_STATE_HIDDEN
    GhostTextBuffer     BYTE MAX_SUGGESTION_LEN DUP (0)
    CursorPosition      DWORD 0
    
    ; Editor handle
    hEditorWindow       QWORD ?
    
    ; Strings
    szGhostTextDismiss  BYTE "Ghost text dismissed",0
    szGhostTextAccepted BYTE "Ghost text accepted: %s",0
    
    ; Previous context for pattern matching
    PreviousContext     BYTE 256 DUP (0)

.data?
    ; Suggestion generation timer
    SuggestionTimer     QWORD ?

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: ghost_text_init(hEditor: rcx) -> rax (success)
; Initialize ghost text suggestion system
;==========================================================================
PUBLIC ghost_text_init
ghost_text_init PROC
    push rbx
    sub rsp, 32
    
    mov hEditorWindow, rcx
    
    ; Zero suggestions array
    lea rcx, CurrentSuggestions
    xor edx, edx
    mov r8d, MAX_SUGGESTIONS * (SIZE SUGGESTION) / 8
    
.zero_loop:
    cmp r8d, 0
    je .zero_done
    mov QWORD PTR [rcx + rdx], 0
    add rdx, 8
    dec r8d
    jmp .zero_loop
    
.zero_done:
    mov SuggestionCount, 0
    mov CurrentSuggestion, 0
    mov GhostTextState, GHOST_STATE_HIDDEN
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rbx
    ret
ghost_text_init ENDP

;==========================================================================
; PUBLIC: ghost_text_generate_suggestions(context: rcx) -> eax (count)
; Generate ghost text suggestions based on code context
; context: current line/code context
;==========================================================================
PUBLIC ghost_text_generate_suggestions
ghost_text_generate_suggestions PROC
    push rbx
    push rdi
    push rsi
    sub rsp, 32
    
    mov rsi, rcx                        ; rsi = context
    xor edi, edi                        ; edi = suggestion count
    
    ; Clear previous suggestions
    mov SuggestionCount, 0
    mov CurrentSuggestion, 0
    
    ; Analyze context for patterns
    ; Check if context contains common code patterns
    
    ; Example: if context ends with "for (", suggest loop completion
    mov rcx, rsi
    lea rdx, szForPattern
    call string_ends_with
    test eax, eax
    jz .check_if_pattern
    
    ; Generate for-loop suggestion
    lea rcx, CurrentSuggestions
    lea rdx, szSuggestionForLoop
    mov r8d, 100                        ; confidence
    call add_suggestion
    
    inc edi
    
.check_if_pattern:
    mov rcx, rsi
    lea rdx, szIfPattern
    call string_ends_with
    test eax, eax
    jz .check_while_pattern
    
    ; Generate if-statement suggestion
    lea rcx, CurrentSuggestions
    lea rdx, szSuggestionIfBlock
    mov r8d, 95
    call add_suggestion
    
    inc edi
    
.check_while_pattern:
    mov rcx, rsi
    lea rdx, szWhilePattern
    call string_ends_with
    test eax, eax
    jz .check_function_pattern
    
    ; Generate while-loop suggestion
    lea rcx, CurrentSuggestions
    lea rdx, szSuggestionWhileLoop
    mov r8d, 90
    call add_suggestion
    
    inc edi
    
.check_function_pattern:
    mov rcx, rsi
    lea rdx, szFunctionPattern
    call string_ends_with
    test eax, eax
    jz .suggestions_done
    
    ; Generate function body suggestion
    lea rcx, CurrentSuggestions
    lea rdx, szSuggestionFunctionBody
    mov r8d, 85
    call add_suggestion
    
    inc edi
    
.suggestions_done:
    ; Store context for next time
    mov rcx, rsi
    lea rdx, PreviousContext
    mov r8d, 256
    call copy_context_safe
    
    mov SuggestionCount, edi
    mov eax, edi                        ; Return suggestion count
    
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
ghost_text_generate_suggestions ENDP

;==========================================================================
; PRIVATE: add_suggestion(suggestions: rcx, text: rdx, confidence: r8d) -> void
;==========================================================================
PRIVATE add_suggestion
add_suggestion PROC
    push rbx
    
    mov rbx, SuggestionCount
    cmp rbx, MAX_SUGGESTIONS
    jge .add_done
    
    ; Get suggestion entry
    imul eax, ebx, SIZE SUGGESTION
    lea rax, [rcx + rax]                ; rcx is CurrentSuggestions base
    
    ; Copy text
    mov r9d, MAX_SUGGESTION_LEN
    call copy_suggestion_text
    
    ; Set confidence
    mov DWORD PTR [rax + MAX_SUGGESTION_LEN], r8d
    
    ; Set source
    mov DWORD PTR [rax + MAX_SUGGESTION_LEN + 4], 1  ; source = learning
    
    ; Timestamp
    call GetTickCount64
    mov QWORD PTR [rax + MAX_SUGGESTION_LEN + 8], rax
    
    inc SuggestionCount
    
.add_done:
    pop rbx
    ret
add_suggestion ENDP

;==========================================================================
; PRIVATE: copy_suggestion_text(dst: rax, src: rdx, max: r9d) -> void
;==========================================================================
PRIVATE copy_suggestion_text
copy_suggestion_text PROC
    xor ecx, ecx
    
.copy_loop:
    cmp ecx, r9d
    jge .copy_done
    
    movzx ebx, BYTE PTR [rdx + rcx]
    mov BYTE PTR [rax + rcx], bl
    
    test bl, bl
    je .copy_done
    
    inc ecx
    jmp .copy_loop
    
.copy_done:
    mov BYTE PTR [rax + rcx], 0
    ret
copy_suggestion_text ENDP

;==========================================================================
; PUBLIC: ghost_text_show(suggestion_index: ecx) -> eax (success)
; Display ghost text suggestion at current cursor position
;==========================================================================
PUBLIC ghost_text_show
ghost_text_show PROC
    push rbx
    push rdi
    push rsi
    sub rsp, 32
    
    ; Validate index
    cmp ecx, SuggestionCount
    jge .show_fail
    
    mov CurrentSuggestion, ecx
    
    ; Get suggestion text
    imul eax, ecx, SIZE SUGGESTION
    lea rax, CurrentSuggestions[rax]
    lea rsi, [rax]                      ; rsi = suggestion text
    
    ; Copy to ghost text buffer
    lea rcx, GhostTextBuffer
    mov rdx, rsi
    mov r8d, MAX_SUGGESTION_LEN
    call copy_ghost_text
    
    ; Set state to visible
    mov GhostTextState, GHOST_STATE_VISIBLE
    
    ; Display ghost text (would call into editor rendering)
    mov rcx, hEditorWindow
    mov rdx, GHOST_TEXT_COLOR
    lea r8, GhostTextBuffer
    call display_ghost_text_in_editor
    
    ; Set timer to auto-dismiss
    call GetTickCount64
    mov SuggestionTimer, rax
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
    
.show_fail:
    xor eax, eax
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
ghost_text_show ENDP

;==========================================================================
; PUBLIC: ghost_text_accept() -> eax (success)
; Accept current ghost text suggestion
;==========================================================================
PUBLIC ghost_text_accept
ghost_text_accept PROC
    push rbx
    sub rsp, 32
    
    cmp GhostTextState, GHOST_STATE_VISIBLE
    jne .accept_fail
    
    ; Get current suggestion text
    mov eax, CurrentSuggestion
    imul eax, SIZE SUGGESTION
    lea rax, CurrentSuggestions[rax]
    
    ; Insert text at cursor (would call editor API)
    mov rcx, hEditorWindow
    lea rdx, [rax]
    call insert_text_at_cursor
    
    ; Change state to accepted
    mov GhostTextState, GHOST_STATE_ACCEPTED
    
    ; Log acceptance
    lea rcx, szGhostTextAccepted
    lea rdx, [rax]
    call log_ghost_text_event
    
    ; Clear ghost text after short delay
    mov GhostTextState, GHOST_STATE_HIDDEN
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rbx
    ret
    
.accept_fail:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
ghost_text_accept ENDP

;==========================================================================
; PUBLIC: ghost_text_dismiss() -> eax
; Dismiss current ghost text
;==========================================================================
PUBLIC ghost_text_dismiss
ghost_text_dismiss PROC
    mov GhostTextState, GHOST_STATE_HIDDEN
    mov CurrentSuggestion, 0
    
    ; Clear buffer
    lea rcx, GhostTextBuffer
    mov BYTE PTR [rcx], 0
    
    xor eax, eax
    ret
ghost_text_dismiss ENDP

;==========================================================================
; PUBLIC: ghost_text_cycle_next() -> eax (success)
; Show next suggestion in list
;==========================================================================
PUBLIC ghost_text_cycle_next
ghost_text_cycle_next PROC
    mov eax, CurrentSuggestion
    inc eax
    cmp eax, SuggestionCount
    jl .show_next
    
    xor eax, eax                        ; Wrap to first
    
.show_next:
    mov ecx, eax
    call ghost_text_show
    ret
ghost_text_cycle_next ENDP

;==========================================================================
; PRIVATE: string_ends_with(text: rcx, pattern: rdx) -> eax (1=match, 0=no)
;==========================================================================
PRIVATE string_ends_with
string_ends_with PROC
    push rbx
    
    ; Get text length
    mov rsi, rcx
    xor eax, eax
    
.len_loop:
    cmp BYTE PTR [rsi + rax], 0
    je .len_found
    inc eax
    jmp .len_loop
    
.len_found:
    mov ebx, eax                        ; ebx = text length
    
    ; Get pattern length
    mov rsi, rdx
    xor eax, eax
    
.pattern_len_loop:
    cmp BYTE PTR [rsi + rax], 0
    je .pattern_len_found
    inc eax
    jmp .pattern_len_loop
    
.pattern_len_found:
    ; Check if pattern fits at end of text
    cmp eax, ebx
    jg .no_match
    
    ; Compare last N characters
    mov esi, ebx
    sub esi, eax                        ; esi = start position
    
    mov rdi, rcx
    add rdi, rsi
    
    mov rcx, 0
    
.compare_loop:
    mov al, BYTE PTR [rdi + rcx]
    mov bl, BYTE PTR [rdx + rcx]
    cmp al, bl
    jne .no_match
    
    test bl, bl
    je .match_found
    
    inc rcx
    jmp .compare_loop
    
.match_found:
    mov eax, 1
    pop rbx
    ret
    
.no_match:
    xor eax, eax
    pop rbx
    ret
string_ends_with ENDP

;==========================================================================
; PRIVATE: copy_ghost_text(dst: rcx, src: rdx, max: r8d) -> void
;==========================================================================
PRIVATE copy_ghost_text
copy_ghost_text PROC
    xor eax, eax
    
.copy_loop:
    cmp eax, r8d
    jge .copy_done
    
    movzx ebx, BYTE PTR [rdx + rax]
    mov BYTE PTR [rcx + rax], bl
    
    test bl, bl
    je .copy_done
    
    inc eax
    jmp .copy_loop
    
.copy_done:
    ret
copy_ghost_text ENDP

;==========================================================================
; PRIVATE: copy_context_safe(dst: rcx, src: rdx, max: r8d) -> void
;==========================================================================
PRIVATE copy_context_safe
copy_context_safe PROC
    xor eax, eax
    
.copy_loop:
    cmp eax, r8d
    jge .copy_done
    
    movzx ebx, BYTE PTR [rdx + rax]
    mov BYTE PTR [rcx + rax], bl
    
    test bl, bl
    je .copy_done
    
    inc eax
    jmp .copy_loop
    
.copy_done:
    ret
copy_context_safe ENDP

;==========================================================================
; PRIVATE: display_ghost_text_in_editor() -> void
; (Stub: actual implementation depends on editor type)
;==========================================================================
PRIVATE display_ghost_text_in_editor
display_ghost_text_in_editor PROC
    ; TODO: Actual rendering logic for ghost text
    ret
display_ghost_text_in_editor ENDP

;==========================================================================
; PRIVATE: insert_text_at_cursor() -> void
; (Stub: actual implementation depends on editor)
;==========================================================================
PRIVATE insert_text_at_cursor
insert_text_at_cursor PROC
    ; TODO: Actual text insertion logic
    ret
insert_text_at_cursor ENDP

;==========================================================================
; PRIVATE: log_ghost_text_event() -> void
;==========================================================================
PRIVATE log_ghost_text_event
log_ghost_text_event PROC
    ret
log_ghost_text_event ENDP

; Pattern matching strings (data section)
.data
    szForPattern        BYTE "for (",0
    szIfPattern         BYTE "if (",0
    szWhilePattern      BYTE "while (",0
    szFunctionPattern   BYTE "PROC",0
    
    szSuggestionForLoop BYTE " ; todo",0
    szSuggestionIfBlock BYTE " { }",0
    szSuggestionWhileLoop BYTE " { }",0
    szSuggestionFunctionBody BYTE "    ret",0

END
