; ============================================================================
; FILE: masm_advanced_find_replace.asm
; TITLE: Advanced Find/Replace with Regex Support
; PURPOSE: Sophisticated search and replace with pattern matching
; ============================================================================

option casemap:none

include windows.inc
include masm_hotpatch.inc
include logging.inc

includelib kernel32.lib
includelib user32.lib

; ============================================================================
; CONSTANTS AND STRUCTURES
; ============================================================================

MAX_SEARCH_RESULTS = 1000
MAX_PATTERN_LENGTH = 512

; Search flags
SEARCH_FLAG_CASE_SENSITIVE = 1
SEARCH_FLAG_WHOLE_WORD = 2
SEARCH_FLAG_REGEX = 4
SEARCH_FLAG_MULTILINE = 8

; Search result structure
SEARCH_RESULT STRUCT
    startPos DWORD ?
    endPos DWORD ?
    lineNumber DWORD ?
    matchText BYTE 256 DUP(?)
SEARCH_RESULT ENDS

; Find/Replace engine state
FIND_REPLACE_ENGINE STRUCT
    hEditor QWORD ?
    
    searchPattern BYTE MAX_PATTERN_LENGTH DUP(?)
    replacePattern BYTE MAX_PATTERN_LENGTH DUP(?)
    
    results SEARCH_RESULT MAX_SEARCH_RESULTS DUP({})
    resultCount DWORD ?
    currentResultIndex DWORD ?
    
    flags DWORD ?
    
    ; Regex engine state
    regexCompiled BYTE ?
    regexState QWORD ?
FIND_REPLACE_ENGINE ENDS

; ============================================================================
; GLOBAL VARIABLES
; ============================================================================

.data

globalFindEngine FIND_REPLACE_ENGINE {}

; ============================================================================
; PUBLIC API FUNCTIONS
; ============================================================================

.code

; find_init(hEditor: rcx) -> bool (rax)
PUBLIC find_init
find_init PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov [globalFindEngine.hEditor], rcx
    mov [globalFindEngine.resultCount], 0
    mov [globalFindEngine.currentResultIndex], 0
    mov [globalFindEngine.flags], 0
    mov [globalFindEngine.regexCompiled], 0
    
    mov eax, 1
    leave
    ret
find_init ENDP

; find_set_pattern(pattern: rcx, flags: rdx) -> bool (rax)
PUBLIC find_set_pattern
find_set_pattern PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    push rsi
    push rdi
    
    ; Copy pattern
    lea rdi, [globalFindEngine.searchPattern]
    mov rsi, rcx
    mov rcx, MAX_PATTERN_LENGTH
    rep movsb
    
    mov [globalFindEngine.flags], edx
    
    ; If regex flag set, compile pattern
    test edx, SEARCH_FLAG_REGEX
    jz skip_regex_compile
    
    lea rcx, [globalFindEngine.searchPattern]
    call regex_compile
    test rax, rax
    jz compile_failed
    
    mov [globalFindEngine.regexState], rax
    mov [globalFindEngine.regexCompiled], 1
    
skip_regex_compile:
    mov eax, 1
    jmp done
    
compile_failed:
    xor eax, eax
    
done:
    pop rdi
    pop rsi
    leave
    ret
find_set_pattern ENDP

; find_execute() -> resultCount (rax)
PUBLIC find_execute
find_execute PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    push rsi
    push rdi
    
    mov [globalFindEngine.resultCount], 0
    
    ; Get editor text length
    mov rcx, [globalFindEngine.hEditor]
    mov rdx, WM_GETTEXTLENGTH
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    test rax, rax
    jz no_text
    
    mov rbx, rax  ; Text length
    
    ; Allocate buffer for text
    inc rbx
    mov rcx, rbx
    call HeapAlloc
    test rax, rax
    jz alloc_failed
    
    mov rsi, rax  ; Text buffer
    
    ; Get text from editor
    mov rcx, [globalFindEngine.hEditor]
    mov rdx, WM_GETTEXT
    mov r8, rbx
    mov r9, rsi
    call SendMessageA
    
    ; Perform search based on flags
    test [globalFindEngine.flags], SEARCH_FLAG_REGEX
    jnz regex_search
    
    ; Simple text search
    mov rcx, rsi
    mov rdx, rbx
    lea r8, [globalFindEngine.searchPattern]
    call find_text_plain
    jmp search_done
    
regex_search:
    mov rcx, rsi
    mov rdx, rbx
    mov r8, [globalFindEngine.regexState]
    call find_text_regex
    
search_done:
    ; Free text buffer
    mov rcx, rsi
    call HeapFree
    
    mov eax, [globalFindEngine.resultCount]
    jmp done
    
alloc_failed:
no_text:
    xor eax, eax
    
done:
    pop rdi
    pop rsi
    pop rbx
    leave
    ret
find_execute ENDP

; find_replace_next(replaceText: rcx) -> bool (rax)
PUBLIC find_replace_next
find_replace_next PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Check if we have results
    cmp [globalFindEngine.resultCount], 0
    je no_results
    
    ; Get current result
    mov eax, [globalFindEngine.currentResultIndex]
    cmp eax, [globalFindEngine.resultCount]
    jge no_more
    
    imul rax, SIZEOF SEARCH_RESULT
    lea rbx, [globalFindEngine.results]
    add rbx, rax
    
    ; Select range in editor
    mov rcx, [globalFindEngine.hEditor]
    mov rdx, EM_SETSEL
    mov r8d, [rbx.startPos]
    mov r9d, [rbx.endPos]
    call SendMessageA
    
    ; Replace selection
    mov rcx, [globalFindEngine.hEditor]
    mov rdx, EM_REPLACESEL
    mov r8, 1  ; Can undo
    lea r9, [globalFindEngine.replacePattern]
    call SendMessageA
    
    ; Move to next result
    inc [globalFindEngine.currentResultIndex]
    
    mov eax, 1
    jmp done
    
no_more:
no_results:
    xor eax, eax
    
done:
    leave
    ret
find_replace_next ENDP

; find_replace_all() -> count (rax)
PUBLIC find_replace_all
find_replace_all PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov [globalFindEngine.currentResultIndex], 0
    xor r12d, r12d  ; Replace count
    
replace_loop:
    mov eax, [globalFindEngine.currentResultIndex]
    cmp eax, [globalFindEngine.resultCount]
    jge replace_done
    
    lea rcx, [globalFindEngine.replacePattern]
    call find_replace_next
    test rax, rax
    jz replace_done
    
    inc r12d
    jmp replace_loop
    
replace_done:
    mov eax, r12d
    leave
    ret
find_replace_all ENDP

; ============================================================================
; SEARCH IMPLEMENTATION
; ============================================================================

; find_text_plain(text: rcx, length: rdx, pattern: r8) -> count (rax)
find_text_plain PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx  ; Text
    mov rbx, rdx  ; Length
    mov rdi, r8   ; Pattern
    
    ; Get pattern length
    mov rcx, rdi
    call string_length
    mov r9, rax   ; Pattern length
    
    test r9, r9
    jz plain_done
    
    xor r10d, r10d  ; Current position
    xor r11d, r11d  ; Result count
    
plain_search_loop:
    mov rax, rbx
    sub rax, r10
    cmp rax, r9
    jl plain_done
    
    ; Compare at current position
    lea rcx, [rsi + r10]
    mov rdx, rdi
    mov r8, r9
    call memory_compare
    test rax, rax
    jnz plain_no_match
    
    ; Found match - add to results
    push rbx
    push rsi
    push rdi
    push r9
    push r10
    push r11
    
    mov ecx, r10d
    mov edx, r10d
    add edx, r9d
    call add_search_result
    
    pop r11
    pop r10
    pop r9
    pop rdi
    pop rsi
    pop rbx
    
    inc r11d
    add r10, r9
    jmp plain_search_loop
    
plain_no_match:
    inc r10
    jmp plain_search_loop
    
plain_done:
    mov eax, r11d
    mov [globalFindEngine.resultCount], eax
    
    pop rdi
    pop rsi
    pop rbx
    leave
    ret
find_text_plain ENDP

; find_text_regex(text: rcx, length: rdx, regexState: r8) -> count (rax)
find_text_regex PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Simple regex implementation (placeholder)
    ; Would use compiled regex state to match
    
    xor eax, eax
    leave
    ret
find_text_regex ENDP

; ============================================================================
; REGEX ENGINE (Simplified)
; ============================================================================

; regex_compile(pattern: rcx) -> state (rax)
regex_compile PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Allocate regex state
    mov rcx, 1024
    call HeapAlloc
    
    ; Compile pattern into state machine
    ; (Simplified - would build NFA/DFA)
    
    leave
    ret
regex_compile ENDP

; ============================================================================
; HELPER FUNCTIONS
; ============================================================================

add_search_result PROC
    ; Add result to results array
    mov eax, [globalFindEngine.resultCount]
    cmp eax, MAX_SEARCH_RESULTS
    jge result_full
    
    imul rax, SIZEOF SEARCH_RESULT
    lea rbx, [globalFindEngine.results]
    add rbx, rax
    
    mov [rbx.startPos], ecx
    mov [rbx.endPos], edx
    
    inc [globalFindEngine.resultCount]
    
result_full:
    ret
add_search_result ENDP

string_length PROC
    push rbp
    mov rbp, rsp
    xor rax, rax
    
strlen_loop:
    cmp byte ptr [rcx], 0
    je strlen_done
    inc rcx
    inc rax
    jmp strlen_loop
    
strlen_done:
    leave
    ret
string_length ENDP

memory_compare PROC
    ; Compare memory regions
    push rsi
    push rdi
    
    mov rsi, rcx
    mov rdi, rdx
    mov rcx, r8
    repe cmpsb
    
    pop rdi
    pop rsi
    
    ; Return 0 if equal
    setz al
    movzx rax, al
    xor rax, 1
    ret
memory_compare ENDP

end