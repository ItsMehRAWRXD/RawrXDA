; ============================================================================
; File 25: text_search.asm - Boyer-Moore-Horspool pattern matching
; ============================================================================
; Purpose: Fast find/replace with O(n/m) average algorithm
; Uses: BMH precomputed table, match caching (10K max), highlight generation
; Functions: Init, BuildBMHTable, FindNext, FindAll, ReplaceAll, HighlightMatches
; ============================================================================

.code

; CONSTANTS
; ============================================================================

SEARCH_MAX_MATCHES      equ 10000     ; cache 10K matches max
SEARCH_MAX_PATTERN      equ 256       ; 256 byte pattern max

; INITIALIZATION
; ============================================================================

Search_Init PROC USES rbx rcx rdx rsi rdi r8
    ; Returns: SearchContext* in rax
    ; { pattern, patternLen, bmhTable, matches[], matchCount, caseSensitive, mutex }
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax  ; process heap
    
    ; Allocate main struct (96 bytes)
    mov rdx, 0
    mov r8, 96
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov rbx, rax  ; rbx = SearchContext*
    
    ; Allocate pattern buffer (256 bytes)
    mov r8, 256
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rbx + 0], rax  ; pattern
    
    ; Allocate BMH table (256 entries × 4 bytes = 1KB)
    mov r8, 1024
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rbx + 8], rax  ; bmhTable
    
    ; Allocate matches array (10000 × 16 bytes = 160KB)
    mov r8, 160000
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rbx + 16], rax  ; matches
    
    ; Initialize fields
    mov [rbx + 24], 0   ; patternLen
    mov [rbx + 32], 0   ; matchCount
    mov [rbx + 40], 1   ; caseSensitive
    
    ; Initialize CRITICAL_SECTION
    lea rdx, [rbx + 48]
    sub rsp, 40
    mov rcx, rdx
    call InitializeCriticalSection
    add rsp, 40
    
    mov rax, rbx
    ret
Search_Init ENDP

; SET PATTERN
; ============================================================================

Search_SetPattern PROC USES rbx rcx rdx rsi rdi context:PTR DWORD, pattern:PTR BYTE, length:QWORD, caseSensitive:DWORD
    ; context = SearchContext*
    ; pattern = pattern string
    ; length = pattern length
    ; caseSensitive = 0/1
    
    mov rcx, context
    lea rdx, [rcx + 48]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, context
    mov rsi, pattern
    mov rax, length
    
    ; Copy pattern
    mov rdi, [rcx + 0]  ; pattern buffer
    mov rcx, rax
    rep movsb
    
    mov rcx, context
    mov [rcx + 24], rax  ; patternLen
    mov [rcx + 40], r8d  ; caseSensitive
    
    ; Build BMH table
    call Search_BuildBMHTable
    
    mov rcx, context
    lea rdx, [rcx + 48]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 1
    ret
Search_SetPattern ENDP

; BUILD BOYER-MOORE-HORSPOOL TABLE
; ============================================================================

Search_BuildBMHTable PROC USES rbx rcx rdx rsi rdi r8 r9 context:PTR DWORD
    ; Precompute jump offsets for each byte (0-255)
    ; bmhTable[byte] = distance from right of pattern
    
    mov rcx, context
    mov rsi, [rcx + 0]  ; pattern
    mov rax, [rcx + 24]  ; patternLen
    mov rdi, [rcx + 8]   ; bmhTable
    
    ; Initialize all entries to pattern length
    xor r8, r8
@init_loop:
    cmp r8, 256
    jge @init_done
    
    mov [rdi + r8*4], eax  ; bmhTable[i] = patternLen
    inc r8
    jmp @init_loop
    
@init_done:
    ; Scan pattern from left to right, update table
    mov r8, 0
@scan_loop:
    cmp r8, rax
    jge @scan_done
    
    mov r9b, byte ptr [rsi + r8]  ; pattern[i]
    movzx r9d, r9b
    
    ; Distance = patternLen - i - 1
    mov r10, rax
    sub r10, r8
    dec r10
    
    mov [rdi + r9*4], r10d  ; bmhTable[byte] = distance
    
    inc r8
    jmp @scan_loop
    
@scan_done:
    ret
Search_BuildBMHTable ENDP

; FIND NEXT MATCH
; ============================================================================

Search_FindNext PROC USES rbx rcx rdx rsi rdi r8 r9 context:PTR DWORD, buffer:PTR BYTE, bufferLen:QWORD, startPos:QWORD
    ; context = SearchContext*
    ; buffer = text buffer
    ; bufferLen = buffer length
    ; startPos = where to start searching
    ; Returns: match position in rax (-1 if not found)
    
    mov rcx, context
    mov rsi, buffer
    mov rax, bufferLen
    mov rdx, startPos
    
    mov r8, [rcx + 0]   ; pattern
    mov r9, [rcx + 24]  ; patternLen
    
    ; Boyer-Moore-Horspool matching
    ; Compare from right to left
    
    mov r10, r9
    dec r10             ; patternLen - 1 (last index)
    
    mov r11, rdx        ; current position
    add r11, r10        ; position + patternLen - 1
    
@search_loop:
    cmp r11, rax
    jge @search_not_found
    
    ; Get character at current position
    mov r12b, byte ptr [rsi + r11]
    
    ; Load it from pattern for comparison (right-to-left)
    mov r13b, byte ptr [r8 + r10]
    
    ; Convert to uppercase if not case-sensitive
    mov ebx, [rcx + 40]
    cmp ebx, 0
    je @case_sensitive
    
    ; Case-insensitive: convert to uppercase
    cmp r12b, 'a'
    jl @cs_ok
    cmp r12b, 'z'
    jg @cs_ok
    sub r12b, 32
    
@cs_ok:
    cmp r13b, 'a'
    jl @case_sensitive
    cmp r13b, 'z'
    jg @case_sensitive
    sub r13b, 32
    
@case_sensitive:
    cmp r12b, r13b
    jne @mismatch
    
    ; Match: go to previous character
    cmp r10, 0
    je @full_match
    
    dec r10
    dec r11
    jmp @search_loop
    
@mismatch:
    ; Use BMH table to jump
    movzx r12d, r12b
    mov rdi, [rcx + 8]  ; bmhTable
    mov r12d, [rdi + r12*4]  ; jump distance
    
    add r11, r12        ; r11 += jump distance
    mov r10, r9
    dec r10             ; reset pattern index
    
    jmp @search_loop
    
@full_match:
    ; Found match at position r11 - (patternLen - 1)
    mov rax, r11
    mov r9, [rcx + 24]
    sub rax, r9
    inc rax
    ret
    
@search_not_found:
    mov rax, -1
    ret
Search_FindNext ENDP

; FIND ALL MATCHES
; ============================================================================

Search_FindAll PROC USES rbx rcx rdx rsi rdi r8 r9 context:PTR DWORD, buffer:PTR BYTE, bufferLen:QWORD
    ; context = SearchContext*
    ; buffer = text buffer
    ; bufferLen = buffer length
    ; Returns: match count in rax
    
    mov rcx, context
    lea rdx, [rcx + 48]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, context
    mov rsi, buffer
    mov rax, bufferLen
    
    mov rbx, 0          ; match index
    mov r8, 0           ; search position
    
@find_loop:
    cmp rbx, SEARCH_MAX_MATCHES
    jge @find_done
    
    ; Find next match starting from r8
    mov rdx, rcx
    mov rdi, rsi
    mov r9, rax
    mov r10, r8
    call Search_FindNext
    
    cmp rax, -1
    je @find_done
    
    ; Store match position
    mov rdi, [rcx + 16]  ; matches array
    mov [rdi + rbx*16], rax  ; position
    
    ; Store match length
    mov r9, [rcx + 24]
    mov [rdi + rbx*16 + 8], r9  ; length
    
    inc rbx
    add rax, 1
    mov r8, rax  ; continue from next position
    
    jmp @find_loop
    
@find_done:
    mov rcx, context
    mov [rcx + 32], rbx  ; matchCount
    mov rax, rbx
    
    mov rcx, context
    lea rdx, [rcx + 48]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    ret
Search_FindAll ENDP

; REPLACE ALL MATCHES
; ============================================================================

Search_ReplaceAll PROC USES rbx rcx rdx rsi rdi r8 r9 context:PTR DWORD, buffer:PTR BYTE, bufferLen:QWORD, replacement:PTR BYTE, replaceLen:QWORD
    ; context = SearchContext*
    ; buffer = source text
    ; bufferLen = source length
    ; replacement = replacement text
    ; replaceLen = replacement length
    ; Returns: new buffer* in rax
    
    ; First, find all matches
    mov rdx, rcx
    mov rsi, buffer
    mov rdi, bufferLen
    call Search_FindAll
    
    mov rcx, context
    
    ; Calculate new size:
    ; newSize = bufferLen - (matchCount * patternLen) + (matchCount * replaceLen)
    
    mov rax, bufferLen
    mov rbx, [rcx + 32]  ; matchCount
    mov r8, [rcx + 24]   ; patternLen
    mov r9, replaceLen
    
    mov r10, rbx
    imul r10, r8         ; matchCount * patternLen
    sub rax, r10
    
    mov r10, rbx
    imul r10, r9         ; matchCount * replaceLen
    add rax, r10
    
    ; Allocate new buffer
    sub rsp, 40
    call GetProcessHeap
    mov rcx, rax
    mov rdx, 0
    mov r8, rax  ; size
    call HeapAlloc
    add rsp, 40
    mov rdi, rax  ; rdi = new buffer
    
    ; Copy with replacements
    mov rsi, buffer
    mov rbx, [rcx + 16]  ; matches array
    mov r8, 0            ; match index
    mov r9, 0            ; source offset
    
@replace_loop:
    cmp r8, [rcx + 32]  ; matchCount
    jge @replace_final
    
    ; Copy from r9 to match position
    mov rax, [rbx + r8*16]  ; match position
    mov r10, rax
    sub r10, r9             ; length to copy
    
    mov r11, rsi
    add r11, r9
    mov r12, rdi
    
    mov rcx, r10
    rep movsb
    
    ; Insert replacement
    mov rsi, replacement
    mov rcx, replaceLen
    rep movsb
    
    ; Update source offset
    mov rax, [rbx + r8*16]  ; match position
    add rax, [rcx + 24]     ; + patternLen
    mov r9, rax
    
    inc r8
    jmp @replace_loop
    
@replace_final:
    ; Copy remaining
    mov rax, bufferLen
    sub rax, r9
    mov rsi, buffer
    add rsi, r9
    mov rcx, rax
    rep movsb
    
    ret
Search_ReplaceAll ENDP

; GENERATE HIGHLIGHTS
; ============================================================================

Search_HighlightMatches PROC USES rbx rcx rdx rsi rdi context:PTR DWORD
    ; Generate highlight entries for all cached matches
    ; Returns: highlight count in rax
    
    mov rcx, context
    mov rax, [rcx + 32]  ; matchCount
    
    ; In real implementation, would populate highlight array
    ; For now just return match count
    
    ret
Search_HighlightMatches ENDP

; CLEAR SEARCH STATE
; ============================================================================

Search_Clear PROC USES rbx rcx rdx rsi rdi context:PTR DWORD
    ; Clear pattern and match cache
    
    mov rcx, context
    lea rdx, [rcx + 48]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, context
    mov [rcx + 24], 0   ; patternLen = 0
    mov [rcx + 32], 0   ; matchCount = 0
    
    mov rcx, context
    lea rdx, [rcx + 48]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    ret
Search_Clear ENDP

end
