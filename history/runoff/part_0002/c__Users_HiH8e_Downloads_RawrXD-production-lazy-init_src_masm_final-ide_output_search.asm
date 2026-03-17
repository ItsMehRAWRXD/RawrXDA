;==========================================================================
; output_search.asm - Search/Find in Output Pane
; ==========================================================================
; Implements search functionality for output pane:
; - Find next occurrence
; - Find previous occurrence
; - Case-sensitive toggle
; - Highlight matches
; - Search position tracking
;
; Integration Points:
; - output_pane_logger.asm (RichEdit control)
; - keyboard_shortcuts.asm (Ctrl+Shift+F trigger)
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_SEARCH_LEN      EQU 256
EM_FINDTEXT         EQU 0418h
EM_SETSEL           EQU 00B1h
EM_GETTEXTEX        EQU 044Dh
EM_GETLINE          EQU 00C4h

SEARCH_CASE_SENSITIVE   EQU 1
SEARCH_WRAP_AROUND      EQU 2
SEARCH_HIGHLIGHT        EQU 4

;==========================================================================
; STRUCTURES
;==========================================================================
FINDTEXT STRUCT
    chrg_cpMin      DWORD ?     ; Start position
    chrg_cpMax      DWORD ?     ; End position
    lpstrText       QWORD ?     ; Search text pointer
FINDTEXT ENDS

SEARCH_STATE STRUCT
    search_text     BYTE 256 DUP (?)
    current_pos     DWORD ?
    match_count     DWORD ?
    flags           DWORD ?
    case_sensitive  DWORD ?
SEARCH_STATE ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Search state
    SearchState     SEARCH_STATE <>
    
    ; Strings
    szSearchPrompt   BYTE "Find in Output:",0
    szNoMatches     BYTE "No matches found",0
    szMatchFound    BYTE "Found %d matches",0
    szCaseSensitive BYTE "Case Sensitive",0

.data?
    ; Search buffer
    SearchBuffer    BYTE MAX_SEARCH_LEN DUP (?)
    ResultBuffer    BYTE 1024 DUP (?)

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: output_search_init() -> eax
; Initialize search system
;==========================================================================
PUBLIC output_search_init
output_search_init PROC
    mov SearchState.current_pos, 0
    mov SearchState.match_count, 0
    mov SearchState.flags, 0
    mov SearchState.case_sensitive, 0
    mov eax, 1
    ret
output_search_init ENDP

;==========================================================================
; PUBLIC: output_search_set_text(text: rcx) -> eax
; Set the search text
;==========================================================================
PUBLIC output_search_set_text
output_search_set_text PROC
    push rsi
    push rdi
    sub rsp, 32
    
    ; Copy search text
    lea rdi, [SearchState.search_text]
    mov rsi, rcx
    mov rcx, MAX_SEARCH_LEN - 1
    
copy_loop:
    mov al, BYTE PTR [rsi]
    test al, al
    jz copy_done
    
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jnz copy_loop
    
copy_done:
    mov BYTE PTR [rdi], 0
    
    ; Reset position
    mov SearchState.current_pos, 0
    mov SearchState.match_count, 0
    
    mov eax, 1
    add rsp, 32
    pop rdi
    pop rsi
    ret
output_search_set_text ENDP

;==========================================================================
; PUBLIC: output_search_find_next(outputHwnd: rcx) -> eax (found: 1/0)
; Find next occurrence starting from current position
;==========================================================================
PUBLIC output_search_find_next
output_search_find_next PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 96
    
    mov rbx, rcx        ; output hwnd
    
    ; Build FINDTEXT structure
    lea rdi, [rsp + 32]
    
    ; chrg_cpMin = current_pos
    mov eax, SearchState.current_pos
    mov DWORD PTR [rdi + FINDTEXT.chrg_cpMin], eax
    
    ; chrg_cpMax = -1 (to end of text)
    mov DWORD PTR [rdi + FINDTEXT.chrg_cpMax], -1
    
    ; lpstrText = search text
    lea rax, [SearchState.search_text]
    mov QWORD PTR [rdi + FINDTEXT.lpstrText], rax
    
    ; Call EM_FINDTEXT
    mov rcx, rbx
    mov edx, EM_FINDTEXT
    xor r8d, r8d
    mov r9, rdi
    call SendMessageA
    
    ; Check result (eax = position, or -1 if not found)
    cmp rax, -1
    je find_not_found
    
    ; Found at position rax
    mov ebx, eax
    mov SearchState.current_pos, ebx
    inc SearchState.match_count
    
    ; Highlight the match
    mov SearchState.flags, SEARCH_HIGHLIGHT
    call output_search_highlight_match
    
    mov eax, 1
    jmp find_done
    
find_not_found:
    ; Check if should wrap around
    mov eax, SearchState.flags
    and eax, SEARCH_WRAP_AROUND
    test eax, eax
    jz no_wrap
    
    ; Reset to beginning and try again
    mov SearchState.current_pos, 0
    ; Recursive call would go here - simplified for now
    
no_wrap:
    xor eax, eax
    
find_done:
    add rsp, 96
    pop rdi
    pop rsi
    pop rbx
    ret
output_search_find_next ENDP

;==========================================================================
; PUBLIC: output_search_find_prev(outputHwnd: rcx) -> eax (found: 1/0)
; Find previous occurrence
;==========================================================================
PUBLIC output_search_find_prev
output_search_find_prev PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 96
    
    mov rbx, rcx        ; output hwnd
    
    ; Build FINDTEXT structure (searching backwards)
    lea rdi, [rsp + 32]
    
    ; chrg_cpMin = 0
    mov DWORD PTR [rdi + FINDTEXT.chrg_cpMin], 0
    
    ; chrg_cpMax = current_pos - 1
    mov eax, SearchState.current_pos
    dec eax
    mov DWORD PTR [rdi + FINDTEXT.chrg_cpMax], eax
    
    ; lpstrText = search text
    lea rax, [SearchState.search_text]
    mov QWORD PTR [rdi + FINDTEXT.lpstrText], rax
    
    ; Call EM_FINDTEXT
    mov rcx, rbx
    mov edx, EM_FINDTEXT
    xor r8d, r8d
    mov r9, rdi
    call SendMessageA
    
    cmp rax, -1
    je find_prev_not_found
    
    ; Found
    mov ebx, eax
    mov SearchState.current_pos, ebx
    dec SearchState.match_count
    
    mov SearchState.flags, SEARCH_HIGHLIGHT
    call output_search_highlight_match
    
    mov eax, 1
    jmp find_prev_done
    
find_prev_not_found:
    xor eax, eax
    
find_prev_done:
    add rsp, 96
    pop rdi
    pop rsi
    pop rbx
    ret
output_search_find_prev ENDP

;==========================================================================
; PUBLIC: output_search_toggle_case() -> eax
; Toggle case-sensitive search
;==========================================================================
PUBLIC output_search_toggle_case
output_search_toggle_case PROC
    mov eax, SearchState.case_sensitive
    xor eax, 1
    mov SearchState.case_sensitive, eax
    ret
output_search_toggle_case ENDP

;==========================================================================
; PUBLIC: output_search_get_case() -> eax (0/1)
; Get case-sensitive flag
;==========================================================================
PUBLIC output_search_get_case
output_search_get_case PROC
    mov eax, SearchState.case_sensitive
    ret
output_search_get_case ENDP

;==========================================================================
; PUBLIC: output_search_set_wrap_around(enable: ecx) -> eax
; Enable/disable wrapping to beginning when reaching end
;==========================================================================
PUBLIC output_search_set_wrap_around
output_search_set_wrap_around PROC
    test ecx, ecx
    jz no_wrap_set
    
    ; Set wrap flag
    mov eax, SearchState.flags
    or eax, SEARCH_WRAP_AROUND
    mov SearchState.flags, eax
    mov eax, 1
    ret
    
no_wrap_set:
    ; Clear wrap flag
    mov eax, SearchState.flags
    and eax, NOT SEARCH_WRAP_AROUND
    mov SearchState.flags, eax
    xor eax, eax
    ret
output_search_set_wrap_around ENDP

;==========================================================================
; PUBLIC: output_search_get_match_count() -> eax
; Get number of matches found so far
;==========================================================================
PUBLIC output_search_get_match_count
output_search_get_match_count PROC
    mov eax, SearchState.match_count
    ret
output_search_get_match_count ENDP

;==========================================================================
; INTERNAL: output_search_highlight_match() -> eax
; Highlight the current match with selection
;==========================================================================
output_search_highlight_match PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    ; Get output window handle
    call ui_get_output_hwnd
    mov rbx, rax
    test rbx, rbx
    jz highlight_fail
    
    ; Select the matched text
    mov r8d, SearchState.current_pos
    mov r9d, r8d
    
    ; Add length of search text
    lea rcx, [SearchState.search_text]
    xor edx, edx
strlen_loop:
    mov al, BYTE PTR [rcx + rdx]
    test al, al
    jz strlen_done
    inc edx
    jmp strlen_loop
strlen_done:
    add r9d, edx
    
    ; Send EM_SETSEL
    mov rcx, rbx
    mov rdx, EM_SETSEL
    call SendMessageA
    
    ; Scroll into view
    mov rcx, rbx
    mov rdx, EM_SCROLLCARET
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    mov eax, 1
    jmp highlight_done
    
highlight_fail:
    xor eax, eax
    
highlight_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
output_search_highlight_match ENDP

;==========================================================================
; PUBLIC: output_search_find_next() -> eax
; Find next occurrence of search text
;==========================================================================
PUBLIC output_search_find_next
output_search_find_next PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    call ui_get_output_hwnd
    mov rbx, rax
    test rbx, rbx
    jz find_fail
    
    ; Setup FINDTEXT structure
    mov eax, SearchState.current_pos
    mov [rsp + 32], eax ; chrg.cpMin
    mov dword ptr [rsp + 36], -1 ; chrg.cpMax
    lea rax, [SearchState.search_text]
    mov [rsp + 40], rax ; lpstrText
    
    mov rcx, rbx
    mov rdx, EM_FINDTEXT
    mov r8, 1           ; FR_DOWN
    lea r9, [rsp + 32]
    call SendMessageA
    
    cmp rax, -1
    je find_not_found
    
    ; Update position and highlight
    mov SearchState.current_pos, eax
    call output_search_highlight_match
    
    mov eax, 1
    jmp find_done
    
find_not_found:
    ; Wrap around if flag set
    mov eax, SearchState.flags
    test eax, SEARCH_WRAP_AROUND
    jz find_fail
    
    mov SearchState.current_pos, 0
    ; Recurse once
    call output_search_find_next
    jmp find_done
    
find_fail:
    xor eax, eax
    
find_done:
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
output_search_find_next ENDP

;==========================================================================
; EXTERN DECLARATIONS
;==========================================================================
EXTERN ui_get_output_hwnd:PROC
EXTERN SendMessageA:PROC

EM_SCROLLCARET      EQU 0449h

END
