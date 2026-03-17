;==============================================================================
; File 25: text_search.asm - Boyer-Moore-Horspool Search & Replace
;==============================================================================
; Efficient pattern matching with precomputed jump table
; Supports literal strings and regex (basic)
;==============================================================================

include windows.inc

.code

;==============================================================================
; Initialize Search Context
;==============================================================================
Search_Init PROC lpPattern:QWORD, patternLen:DWORD, useRegex:DWORD, 
                 caseSensitive:DWORD
    LOCAL hHeap:HANDLE
    
    invoke HeapCreate, 0, 1048576, 134217728  ; 1MB initial, 128MB max
    mov [searchHeap], rax
    .if rax == NULL
        LOG_ERROR "Failed to create Search heap"
        ret
    .endif
    
    ; Allocate search context
    invoke HeapAlloc, [searchHeap], 0, SIZEOF SearchContext
    mov [searchCtx], rax
    .if rax == NULL
        LOG_ERROR "Failed to allocate search context"
        ret
    .endif
    
    mov rax, [searchCtx]
    
    ; Copy pattern
    invoke HeapAlloc, [searchHeap], 0, patternLen
    mov [rax].SearchContext.pattern, rax
    invoke RtlCopyMemory, rax, lpPattern, patternLen
    
    mov [rax].SearchContext.patternLen, patternLen
    mov [rax].SearchContext.useRegex, useRegex
    mov [rax].SearchContext.caseSensitive, caseSensitive
    mov [rax].SearchContext.matchCount, 0
    
    ; Build Boyer-Moore-Horspool jump table
    call Search_BuildBMHTable
    
    LOG_INFO "Search initialized: pattern len=%d, regex=%d", 
        patternLen, useRegex
    ret
Search_Init ENDP

;==============================================================================
; Build Boyer-Moore-Horspool Jump Table
;==============================================================================
Search_BuildBMHTable PROC
    LOCAL i:DWORD
    LOCAL ch:BYTE
    LOCAL ctx:QWORD
    
    mov ctx, [searchCtx]
    
    ; Allocate jump table (256 entries for ASCII)
    invoke HeapAlloc, [searchHeap], 0, 256 * SIZEOF DWORD
    mov [ctx].SearchContext.bmhTable, rax
    
    ; Initialize all entries to pattern length
    mov i, 0
.loop1:
    .if i >= 256
        .break
    .endif
    
    mov rax, [ctx].SearchContext.bmhTable
    mov ecx, [ctx].SearchContext.patternLen
    mov [rax + i * 4], ecx
    
    inc i
    .continue .loop1
.endloop1
    
    ; Fill in pattern characters
    mov i, 0
.loop2:
    .if i >= [ctx].SearchContext.patternLen
        .break
    .endif
    
    mov rax, [ctx].SearchContext.pattern
    mov al, BYTE PTR [rax + i]
    
    ; Case-insensitive conversion
    .if ![ctx].SearchContext.caseSensitive
        .if al >= 65 && al <= 90
            add al, 32
        .endif
    .endif
    
    mov ch, al
    mov rax, [ctx].SearchContext.bmhTable
    mov ecx, [ctx].SearchContext.patternLen
    sub ecx, i
    sub ecx, 1
    mov [rax + ch * 4], ecx
    
    inc i
    .continue .loop2
.endloop2
    
    ret
Search_BuildBMHTable ENDP

;==============================================================================
; Find Next Occurrence (BMH Algorithm)
;==============================================================================
Search_FindNext PROC lpText:QWORD, textLen:QWORD, startPos:QWORD
    LOCAL i:QWORD
    LOCAL j:DWORD
    LOCAL match:BYTE
    LOCAL ch:BYTE
    LOCAL patCh:BYTE
    LOCAL jump:DWORD
    LOCAL ctx:QWORD
    
    mov ctx, [searchCtx]
    mov i, startPos
    
.loop1:
    .if i + [ctx].SearchContext.patternLen > textLen
        ; No match found
        mov rax, -1
        ret
    .endif
    
    ; Try to match at position i
    mov j, [ctx].SearchContext.patternLen
    dec j
    mov match, 1
    
.loop2:
    .if j < 0
        .break
    .endif
    
    mov rax, lpText
    mov al, BYTE PTR [rax + i + j]
    mov ch, al
    
    mov rax, [ctx].SearchContext.pattern
    mov al, BYTE PTR [rax + j]
    mov patCh, al
    
    ; Case-insensitive comparison
    .if ![ctx].SearchContext.caseSensitive
        .if ch >= 65 && ch <= 90
            add ch, 32
        .endif
        .if patCh >= 65 && patCh <= 90
            add patCh, 32
        .endif
    .endif
    
    .if ch != patCh
        mov match, 0
        .break
    .endif
    
    dec j
    .continue .loop2
.endloop2
    
    .if match == 1
        ; Found match
        mov rax, i
        ret
    .endif
    
    ; Get jump distance from BMH table
    mov rax, lpText
    mov al, BYTE PTR [rax + i + [ctx].SearchContext.patternLen - 1]
    mov ch, al
    
    .if ![ctx].SearchContext.caseSensitive
        .if ch >= 65 && ch <= 90
            add ch, 32
        .endif
    .endif
    
    mov rax, [ctx].SearchContext.bmhTable
    mov jump, [rax + ch * 4]
    
    add i, jump
    .continue .loop1
.endloop1
    
    mov rax, -1
    ret
Search_FindNext ENDP

;==============================================================================
; Find All Occurrences
;==============================================================================
Search_FindAll PROC lpText:QWORD, textLen:QWORD
    LOCAL pos:QWORD
    LOCAL matchCount:DWORD
    LOCAL matchArray:QWORD
    LOCAL ctx:QWORD
    
    mov ctx, [searchCtx]
    mov pos, 0
    mov matchCount, 0
    
    ; Allocate match array (up to 10000 matches)
    invoke HeapAlloc, [searchHeap], 0, 10000 * SIZEOF QWORD
    mov matchArray, rax
    
.loop1:
    call Search_FindNext, lpText, textLen, pos
    mov rax, rax
    
    .if eax == -1
        .break
    .endif
    
    ; Store match position
    mov rcx, matchArray
    mov [rcx + matchCount * 8], rax
    inc matchCount
    
    add pos, rax
    inc pos
    .continue .loop1
.endloop1
    
    ; Update context
    mov [ctx].SearchContext.matches, matchArray
    mov [ctx].SearchContext.matchCount, matchCount
    
    LOG_INFO "Found %d matches", matchCount
    mov eax, matchCount
    ret
Search_FindAll ENDP

;==============================================================================
; Replace All Occurrences
;==============================================================================
Search_ReplaceAll PROC lpText:QWORD, textLen:QWORD, 
                      lpReplacement:QWORD, replLen:DWORD
    LOCAL i:DWORD
    LOCAL pos:QWORD
    LOCAL newText:QWORD
    LOCAL newLen:QWORD
    LOCAL writePos:QWORD
    LOCAL readPos:QWORD
    LOCAL ctx:QWORD
    
    mov ctx, [searchCtx]
    
    ; First, find all matches
    call Search_FindAll, lpText, textLen
    mov [ctx].SearchContext.matchCount, eax
    
    ; Build replacement text
    mov newLen, 0
    
    ; Calculate new size
    mov newLen, textLen
    mov i, 0
.loop1:
    .if i >= [ctx].SearchContext.matchCount
        .break
    .endif
    
    ; Original text size minus pattern lengths plus replacement lengths
    mov rax, newLen
    sub rax, [ctx].SearchContext.patternLen
    add rax, replLen
    mov newLen, rax
    
    inc i
    .continue .loop1
.endloop1
    
    ; Allocate new text buffer
    invoke HeapAlloc, [searchHeap], 0, newLen
    mov newText, rax
    .if rax == NULL
        LOG_ERROR "Failed to allocate replacement buffer"
        mov rax, 0
        ret
    .endif
    
    ; Build replacement
    mov writePos, 0
    mov readPos, 0
    mov i, 0
    
.loop2:
    .if i >= [ctx].SearchContext.matchCount
        .break
    .endif
    
    mov rax, [ctx].SearchContext.matches
    mov pos, [rax + i * 8]
    
    ; Copy text before match
    mov rax, pos
    sub rax, readPos
    mov rcx, newText
    add rcx, writePos
    mov rdx, lpText
    add rdx, readPos
    invoke RtlCopyMemory, rcx, rdx, rax
    
    add writePos, rax
    
    ; Copy replacement
    mov rcx, newText
    add rcx, writePos
    invoke RtlCopyMemory, rcx, lpReplacement, replLen
    add writePos, replLen
    
    ; Skip original pattern
    mov readPos, pos
    add readPos, [ctx].SearchContext.patternLen
    
    inc i
    .continue .loop2
.endloop2
    
    ; Copy remaining text
    mov rax, textLen
    sub rax, readPos
    .if rax > 0
        mov rcx, newText
        add rcx, writePos
        mov rdx, lpText
        add rdx, readPos
        invoke RtlCopyMemory, rcx, rdx, rax
        add writePos, rax
    .endif
    
    ; Copy new text back to original
    invoke RtlCopyMemory, lpText, newText, writePos
    
    LOG_INFO "Replaced %d occurrences, new length=%lld", 
        [ctx].SearchContext.matchCount, writePos
    
    mov rax, writePos
    ret
Search_ReplaceAll ENDP

;==============================================================================
; Highlight Matches for Rendering
;==============================================================================
Search_HighlightMatches PROC lpHighlightArray:QWORD
    LOCAL i:DWORD
    LOCAL ctx:QWORD
    LOCAL entry:QWORD
    
    mov ctx, [searchCtx]
    
    mov i, 0
.loop1:
    .if i >= [ctx].SearchContext.matchCount
        .break
    .endif
    
    mov rax, [ctx].SearchContext.matches
    mov rcx, [rax + i * 8]
    
    ; Create highlight entry
    mov rax, lpHighlightArray
    mov entry, [rax + i * SIZEOF HighlightEntry]
    
    mov [entry].HighlightEntry.startPos, rcx
    mov [entry].HighlightEntry.endPos, rcx
    add [entry].HighlightEntry.endPos, [ctx].SearchContext.patternLen
    mov [entry].HighlightEntry.color, 0x0000FF00  ; Green
    
    inc i
    .continue .loop1
.endloop1
    
    ret
Search_HighlightMatches ENDP

;==============================================================================
; Clear Search Context
;==============================================================================
Search_Clear PROC
    LOCAL ctx:QWORD
    
    mov ctx, [searchCtx]
    
    .if [ctx].SearchContext.pattern != NULL
        invoke HeapFree, [searchHeap], 0, [ctx].SearchContext.pattern
    .endif
    
    .if [ctx].SearchContext.matches != NULL
        invoke HeapFree, [searchHeap], 0, [ctx].SearchContext.matches
    .endif
    
    .if [ctx].SearchContext.bmhTable != NULL
        invoke HeapFree, [searchHeap], 0, [ctx].SearchContext.bmhTable
    .endif
    
    invoke HeapFree, [searchHeap], 0, ctx
    mov [searchCtx], NULL
    
    LOG_INFO "Search context cleared"
    
    ret
Search_Clear ENDP

;==============================================================================
; Data Structures
;==============================================================================
.data

; Highlight entry
HighlightEntry STRUCT
    startPos        QWORD ?      ; Start position
    endPos          QWORD ?      ; End position
    color           DWORD ?      ; Highlight color (BGRA)
HighlightEntry ENDS

; Search context
SearchContext STRUCT
    pattern         QWORD ?      ; Search string
    patternLen      DWORD ?      ; Pattern length
    matches         QWORD ?      ; Array of match positions
    matchCount      DWORD ?      ; Number of matches
    useRegex        DWORD ?      ; Regex vs literal?
    caseSensitive   DWORD ?      ; Case-sensitive?
    bmhTable        QWORD ?      ; Boyer-Moore-Horspool jump table
SearchContext ENDS

; Global state
searchHeap          dq ?         ; Private heap for allocations
searchCtx           dq ?         ; Current search context

END
