;==========================================================================
; output_pane_search.asm - Search and Navigate in Output Pane History
; ==========================================================================
; Features:
; - Search text in pane history
; - Find next/previous occurrence
; - Case-sensitive/insensitive search
; - Highlight results (background color)
; - Search statistics (matches found)
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_SEARCH_HISTORY  EQU 10            ; Remember last 10 searches
MAX_SEARCH_TERM     EQU 128
MAX_OUTPUT_ENTRIES  EQU 1024
IDC_OUTPUT_RICH     EQU 4001          ; RichEdit control

; Search flags
SEARCH_CASE_SENSITIVE   EQU 0001h
SEARCH_WHOLE_WORD       EQU 0002h
SEARCH_REGEX            EQU 0004h

; RichEdit messages
EM_FINDTEXT             EQU 000Ch
EM_SETSEL               EQU 00B1h
EM_EXSETSEL             EQU 0437h
EM_GETTEXTRANGE         EQU 044Bh

;==========================================================================
; STRUCTURES
;==========================================================================
SEARCH_RESULT STRUCT
    offset              QWORD ?         ; Offset in text
    line_num            DWORD ?         ; Line number where found
    start_pos           DWORD ?         ; Start position
    end_pos             DWORD ?         ; End position
SEARCH_RESULT ENDS

FINDTEXT STRUCT
    chrg_cpMin          DWORD ?
    chrg_cpMax          DWORD ?
    lpstrText           QWORD ?
    chrgText_cpMin      DWORD ?
    chrgText_cpMax      DWORD ?
FINDTEXT ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Search parameters
    SearchTerm          BYTE MAX_SEARCH_TERM DUP (0)
    SearchFlags         DWORD 0
    SearchCase          DWORD 0         ; 1=case sensitive
    
    ; Search results
    SearchMatches       SEARCH_RESULT MAX_OUTPUT_ENTRIES DUP (<>)
    MatchCount          DWORD 0
    CurrentMatchIdx     DWORD 0
    
    ; Search history
    SearchHistory       QWORD MAX_SEARCH_HISTORY DUP (0)
    SearchHistoryCount  DWORD 0
    
    ; Statistics
    LastSearchTime      QWORD 0
    SearchDuration      QWORD 0
    
    ; Strings
    szNoMatches         BYTE "No matches found",0
    szSearchComplete    BYTE "Search complete: %d matches",0

.data?
    ; Output pane handle (set by caller)
    hOutputPane         QWORD ?
    
    ; Temporary buffer for search text
    TempSearchBuffer    BYTE 512 DUP (?)

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: output_search_init(hPane: rcx) -> void
; Initialize search subsystem with output pane handle
;==========================================================================
PUBLIC output_search_init
output_search_init PROC
    mov hOutputPane, rcx
    mov SearchTerm, 0
    mov MatchCount, 0
    mov CurrentMatchIdx, 0
    xor eax, eax
    ret
output_search_init ENDP

;==========================================================================
; PUBLIC: output_search_find(term: rcx, flags: edx) -> eax (matches found)
; Search output pane for term with specified flags
; flags: SEARCH_CASE_SENSITIVE | SEARCH_WHOLE_WORD | SEARCH_REGEX
;==========================================================================
PUBLIC output_search_find
output_search_find PROC
    push rbx
    push rdi
    push rsi
    sub rsp, 48
    
    ; Save search parameters
    mov rsi, rcx                        ; rsi = term
    mov edi, edx                        ; edi = flags
    
    ; Copy search term to global
    lea rcx, SearchTerm
    mov rdx, rsi
    mov r8d, MAX_SEARCH_TERM
    call copy_string_safe_search
    
    ; Get current time for duration measurement
    call GetTickCount64
    mov LastSearchTime, rax
    
    ; Initialize match results
    mov MatchCount, 0
    mov CurrentMatchIdx, 0
    
    ; Get all text from output pane
    mov rcx, hOutputPane
    call get_pane_text_length
    mov r8d, eax                        ; r8d = text length
    
    ; Allocate temporary buffer for pane text
    lea rcx, TempSearchBuffer
    mov rdx, r8d
    
    ; Get all text from RichEdit
    mov rcx, hOutputPane
    lea rdx, TempSearchBuffer
    mov r8, 512                         ; Max buffer size
    call WM_GETTEXT_equivalent
    
    ; Search through text
    lea rdi, SearchTerm                 ; rdi = search term
    lea rsi, TempSearchBuffer           ; rsi = pane text
    
.search_loop:
    cmp BYTE PTR [rsi], 0               ; End of text?
    je .search_done
    
    ; Try to match at current position
    mov rcx, rsi
    mov rdx, rdi
    mov r8d, edi                        ; flags
    call string_match_at_position
    
    test eax, eax                       ; Match found?
    jz .no_match_here
    
    ; Record match
    mov ebx, MatchCount
    cmp ebx, MAX_OUTPUT_ENTRIES
    jge .search_done                    ; Buffer full
    
    ; Calculate offset
    mov r8d, ebx
    imul r8d, SIZE SEARCH_RESULT
    lea r8, SearchMatches[r8]
    
    mov QWORD PTR [r8], rsi             ; offset = current position
    mov DWORD PTR [r8 + 8], 0           ; line_num (TODO: calculate)
    mov DWORD PTR [r8 + 12], 0          ; start_pos
    
    inc MatchCount
    
.no_match_here:
    inc rsi
    jmp .search_loop
    
.search_done:
    ; Calculate search duration
    call GetTickCount64
    sub rax, LastSearchTime
    mov SearchDuration, rax
    
    mov eax, MatchCount                 ; Return number of matches
    add rsp, 48
    pop rsi
    pop rdi
    pop rbx
    ret
output_search_find ENDP

;==========================================================================
; PRIVATE: copy_string_safe_search(dst: rcx, src: rdx, max: r8d) -> void
;==========================================================================
PRIVATE copy_string_safe_search
copy_string_safe_search PROC
    xor eax, eax
    
.copy_loop:
    cmp eax, r8d
    jge .done
    
    movzx ebx, BYTE PTR [rdx + rax]
    mov BYTE PTR [rcx + rax], bl
    
    test bl, bl
    je .done
    
    inc eax
    jmp .copy_loop
    
.done:
    mov BYTE PTR [rcx + rax], 0
    ret
copy_string_safe_search ENDP

;==========================================================================
; PRIVATE: string_match_at_position(text: rcx, pattern: rdx, flags: r8d) -> eax
; Check if pattern matches at current text position
; Returns: 1 if match, 0 if no match
;==========================================================================
PRIVATE string_match_at_position
string_match_at_position PROC
    push rbx
    push rsi
    xor eax, eax                        ; Default: no match
    
    ; Simple case-insensitive/sensitive matching
    cmp r8d, 0                          ; flags
    je .case_insensitive
    
    ; Case sensitive - direct byte comparison
    mov rsi, rcx                        ; rsi = text
    mov rbx, rdx                        ; rbx = pattern
    
.sensitive_loop:
    movzx eax, BYTE PTR [rbx]
    test al, al
    jz .match_found
    
    movzx ecx, BYTE PTR [rsi]
    cmp al, cl
    jne .no_match
    
    inc rsi
    inc rbx
    jmp .sensitive_loop
    
.case_insensitive:
    ; TODO: case-insensitive matching
    mov eax, 0
    jmp .match_exit
    
.match_found:
    mov eax, 1
    
.match_exit:
.no_match:
    pop rsi
    pop rbx
    ret
string_match_at_position ENDP

;==========================================================================
; PRIVATE: get_pane_text_length() -> eax
; Get length of text in output pane
;==========================================================================
PRIVATE get_pane_text_length
get_pane_text_length PROC
    push rcx
    
    ; WM_GETTEXTLENGTH
    mov rcx, hOutputPane
    mov edx, WM_GETTEXTLENGTH
    xor r8, r8
    xor r9, r9
    call SendMessage
    
    pop rcx
    ret
get_pane_text_length ENDP

;==========================================================================
; PRIVATE: WM_GETTEXT_equivalent(hwnd: rcx, buffer: rdx, max_len: r8) -> eax
; Get text from window into buffer
;==========================================================================
PRIVATE WM_GETTEXT_equivalent
WM_GETTEXT_equivalent PROC
    ; WM_GETTEXT = 0x0D
    mov edx, 0Dh
    call SendMessage
    ret
WM_GETTEXT_equivalent ENDP

;==========================================================================
; PUBLIC: output_search_find_next() -> eax (1=found, 0=not found)
; Find next occurrence of current search term
;==========================================================================
PUBLIC output_search_find_next
output_search_find_next PROC
    mov eax, CurrentMatchIdx
    mov edx, MatchCount
    cmp eax, edx
    jge .not_found
    
    ; Move to next match
    inc CurrentMatchIdx
    cmp CurrentMatchIdx, MatchCount
    jl .found
    
.not_found:
    xor eax, eax
    ret
    
.found:
    mov eax, 1
    ret
output_search_find_next ENDP

;==========================================================================
; PUBLIC: output_search_find_prev() -> eax (1=found, 0=not found)
; Find previous occurrence of current search term
;==========================================================================
PUBLIC output_search_find_prev
output_search_find_prev PROC
    cmp CurrentMatchIdx, 0
    je .not_found
    
    dec CurrentMatchIdx
    mov eax, 1
    ret
    
.not_found:
    xor eax, eax
    ret
output_search_find_prev ENDP

;==========================================================================
; PUBLIC: output_search_get_current() -> rcx
; Get pointer to current match
;==========================================================================
PUBLIC output_search_get_current
output_search_get_current PROC
    mov eax, CurrentMatchIdx
    cmp eax, MatchCount
    jge .invalid
    
    imul eax, SIZE SEARCH_RESULT
    lea rax, SearchMatches[rax]
    ret
    
.invalid:
    xor eax, eax
    ret
output_search_get_current ENDP

;==========================================================================
; PUBLIC: output_search_highlight_current() -> eax
; Highlight current match in output pane (via RichEdit formatting)
;==========================================================================
PUBLIC output_search_highlight_current
output_search_highlight_current PROC
    push rbx
    sub rsp, 32
    
    ; Get current match
    call output_search_get_current
    test rax, rax
    jz .highlight_fail
    
    ; Extract start/end positions
    mov ebx, DWORD PTR [rax + 12]       ; start_pos
    mov edx, DWORD PTR [rax + 16]       ; end_pos
    
    ; Use EM_SETSEL to select text in RichEdit
    mov rcx, hOutputPane
    mov r8d, ebx                        ; start
    mov r9d, edx                        ; end
    
    ; SendMessage(hWnd, EM_SETSEL, start, end)
    call SendMessage_EM_SETSEL
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rbx
    ret
    
.highlight_fail:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
output_search_highlight_current ENDP

;==========================================================================
; PRIVATE: SendMessage_EM_SETSEL() -> void
; Send EM_SETSEL to highlight text
;==========================================================================
PRIVATE SendMessage_EM_SETSEL
SendMessage_EM_SETSEL PROC
    ; rcx = hwnd, r8d = start, r9d = end
    mov edx, EM_SETSEL                  ; 00B1h
    mov r8d, ebx                        ; start (from caller)
    mov r9d, edx                        ; end (from caller)
    call SendMessage
    ret
SendMessage_EM_SETSEL ENDP

END
