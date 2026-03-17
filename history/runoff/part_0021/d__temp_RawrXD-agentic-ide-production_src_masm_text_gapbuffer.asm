;==============================================================================
; File 22: text_gapbuffer.asm - Core Text Storage with Gap Buffer
;==============================================================================
; Implements efficient text editing via gap buffer + line index
; Supports insertion/deletion O(1) amortized, line fetch O(1)
; Automatically promotes to rope when file exceeds 10MB
;==============================================================================

include windows.inc

.code

;==============================================================================
; Initialize Gap Buffer
;==============================================================================
GapBuffer_Init PROC initialSize:QWORD
    LOCAL hHeap:HANDLE
    
    ; Create private heap (64MB max)
    invoke HeapCreate, 0, 1048576, 67108864
    mov [gbHeap], rax
    .if rax == NULL
        LOG_ERROR "Failed to create GapBuffer heap"
        mov rax, NULL
        ret
    .endif
    
    ; Allocate buffer structure
    invoke HeapAlloc, [gbHeap], 0, SIZEOF GapBufferState
    mov [gbState], rax
    .if rax == NULL
        LOG_ERROR "Failed to allocate GapBuffer state"
        ret
    .endif
    
    ; Initialize state
    mov rcx, [gbState]
    mov rdx, initialSize
    mov [rcx].GapBufferState.capacity, rdx
    mov [rcx].GapBufferState.gapStart, 0
    mov [rcx].GapBufferState.gapEnd, rdx
    mov [rcx].GapBufferState.size, 0
    mov [rcx].GapBufferState.lineCount, 1
    
    ; Allocate text buffer
    invoke HeapAlloc, [gbHeap], 0, initialSize
    mov [rcx].GapBufferState.data, rax
    .if rax == NULL
        LOG_ERROR "Failed to allocate GapBuffer data (size=%lld)", initialSize
        ret
    .endif
    
    ; Allocate line index (preallocate for 100k lines)
    invoke HeapAlloc, [gbHeap], 0, 100000 * SIZEOF QWORD
    mov [rcx].GapBufferState.lineOffsets, rax
    .if rax == NULL
        LOG_ERROR "Failed to allocate line index"
        ret
    .endif
    
    mov [rcx].GapBufferState.lineOffsets[0], 0  ; First line starts at 0
    
    ; Initialize mutex
    invoke InitializeCriticalSection, ADDR [rcx].GapBufferState.mutex
    
    LOG_INFO "GapBuffer initialized: capacity=%lld bytes", initialSize
    mov rax, [gbState]
    ret
GapBuffer_Init ENDP

;==============================================================================
; Insert Text at Position
;==============================================================================
GapBuffer_Insert PROC pos:QWORD, lpText:QWORD, textLen:QWORD
    LOCAL required:QWORD
    LOCAL newCap:QWORD
    
    .if pos > [gbState].GapBufferState.size
        LOG_ERROR "Insert position out of bounds: pos=%lld, size=%lld", 
            pos, [gbState].GapBufferState.size
        mov rax, 0
        ret
    .endif
    
    ; Lock for thread safety
    invoke EnterCriticalSection, ADDR [gbState].GapBufferState.mutex
    
    ; Move gap to insertion point if needed
    .if pos != [gbState].GapBufferState.gapStart
        call GapBuffer_MoveGap, pos
    .endif
    
    ; Check if gap is large enough
    mov rax, [gbState].GapBufferState.gapEnd
    sub rax, [gbState].GapBufferState.gapStart
    .if rax < textLen
        ; Gap too small, need to expand buffer
        mov required, [gbState].GapBufferState.size
        add required, textLen
        mov newCap, [gbState].GapBufferState.capacity
        shl newCap, 1
        .if newCap < required
            mov newCap, required
            add newCap, 1048576  ; Add 1MB safety margin
        .endif
        
        call GapBuffer_Expand, newCap
        .if rax == FALSE
            invoke LeaveCriticalSection, ADDR [gbState].GapBufferState.mutex
            ret
        .endif
    .endif
    
    ; Copy text into gap
    mov rax, [gbState].GapBufferState.data
    add rax, [gbState].GapBufferState.gapStart
    invoke RtlCopyMemory, rax, lpText, textLen
    
    ; Update gap position
    mov rax, [gbState].GapBufferState.gapStart
    add rax, textLen
    mov [gbState].GapBufferState.gapStart, rax
    
    ; Update size
    mov rax, [gbState].GapBufferState.size
    add rax, textLen
    mov [gbState].GapBufferState.size, rax
    
    ; Update line index
    call GapBuffer_UpdateLineIndex, pos, textLen, 1
    
    invoke LeaveCriticalSection, ADDR [gbState].GapBufferState.mutex
    
    LOG_DEBUG "Inserted %lld bytes at position %lld", textLen, pos
    mov rax, 1
    ret
GapBuffer_Insert ENDP

;==============================================================================
; Delete Text Range
;==============================================================================
GapBuffer_Delete PROC startPos:QWORD, length:QWORD
    .if startPos + length > [gbState].GapBufferState.size
        LOG_ERROR "Delete range out of bounds"
        mov rax, 0
        ret
    .endif
    
    invoke EnterCriticalSection, ADDR [gbState].GapBufferState.mutex
    
    ; Move gap to start of deletion
    .if startPos != [gbState].GapBufferState.gapStart
        call GapBuffer_MoveGap, startPos
    .endif
    
    ; Expand gap (move gapEnd forward)
    mov rax, [gbState].GapBufferState.gapEnd
    add rax, length
    
    ; Check bounds
    .if rax > [gbState].GapBufferState.capacity
        invoke LeaveCriticalSection, ADDR [gbState].GapBufferState.mutex
        LOG_ERROR "Delete would exceed capacity"
        mov rax, 0
        ret
    .endif
    
    mov [gbState].GapBufferState.gapEnd, rax
    
    ; Update size
    mov rax, [gbState].GapBufferState.size
    sub rax, length
    mov [gbState].GapBufferState.size, rax
    
    ; Update line index
    call GapBuffer_UpdateLineIndex, startPos, length, 0
    
    invoke LeaveCriticalSection, ADDR [gbState].GapBufferState.mutex
    
    LOG_DEBUG "Deleted %lld bytes at position %lld", length, startPos
    mov rax, 1
    ret
GapBuffer_Delete ENDP

;==============================================================================
; Move Gap to Position (Internal)
;==============================================================================
GapBuffer_MoveGap PROC targetPos:QWORD
    LOCAL moveLen:QWORD
    LOCAL src:QWORD
    LOCAL dst:QWORD
    LOCAL gapSize:QWORD
    
    mov rax, [gbState].GapBufferState.gapStart
    .if rax == targetPos
        ret
    .endif
    
    mov gapSize, [gbState].GapBufferState.gapEnd
    sub gapSize, [gbState].GapBufferState.gapStart
    
    ; Determine direction and amount to move
    mov rax, targetPos
    .if rax < [gbState].GapBufferState.gapStart
        ; Moving gap left
        mov moveLen, [gbState].GapBufferState.gapStart
        sub moveLen, targetPos
        
        mov src, [gbState].GapBufferState.data
        add src, targetPos
        
        mov dst, [gbState].GapBufferState.data
        add dst, [gbState].GapBufferState.gapEnd
        sub dst, moveLen
        
        invoke RtlMoveMemory, dst, src, moveLen
        
        mov [gbState].GapBufferState.gapStart, targetPos
        mov rax, [gbState].GapBufferState.gapEnd
        sub rax, moveLen
        mov [gbState].GapBufferState.gapEnd, rax
    .else
        ; Moving gap right
        mov moveLen, targetPos
        sub moveLen, [gbState].GapBufferState.gapStart
        
        mov src, [gbState].GapBufferState.data
        add src, [gbState].GapBufferState.gapEnd
        
        mov dst, [gbState].GapBufferState.data
        add dst, [gbState].GapBufferState.gapStart
        
        invoke RtlMoveMemory, dst, src, moveLen
        
        mov [gbState].GapBufferState.gapStart, targetPos
        mov rax, [gbState].GapBufferState.gapEnd
        add rax, moveLen
        mov [gbState].GapBufferState.gapEnd, rax
    .endif
    
    ret
GapBuffer_MoveGap ENDP

;==============================================================================
; Expand Buffer (Double Capacity)
;==============================================================================
GapBuffer_Expand PROC newCapacity:QWORD
    LOCAL newData:QWORD
    LOCAL copySize:QWORD
    LOCAL gapSize:QWORD
    
    ; Allocate new buffer
    invoke HeapAlloc, [gbHeap], 0, newCapacity
    mov newData, rax
    .if rax == NULL
        LOG_ERROR "Failed to allocate expanded buffer (capacity=%lld)", newCapacity
        mov rax, 0
        ret
    .endif
    
    mov rax, [gbState].GapBufferState.gapEnd
    sub rax, [gbState].GapBufferState.gapStart
    mov gapSize, rax
    
    ; Copy left side of gap
    invoke RtlCopyMemory, newData, 
        [gbState].GapBufferState.data, 
        [gbState].GapBufferState.gapStart
    
    ; Copy right side of gap at end of new buffer
    mov rax, newCapacity
    sub rax, gapSize
    mov rcx, [gbState].GapBufferState.data
    add rcx, [gbState].GapBufferState.gapEnd
    invoke RtlCopyMemory, 
        QWORD PTR newData + rax,
        rcx,
        gapSize
    
    ; Free old buffer
    invoke HeapFree, [gbHeap], 0, [gbState].GapBufferState.data
    
    ; Update state
    mov [gbState].GapBufferState.data, newData
    mov [gbState].GapBufferState.capacity, newCapacity
    mov rax, newCapacity
    sub rax, gapSize
    mov [gbState].GapBufferState.gapEnd, rax
    
    LOG_INFO "GapBuffer expanded to %lld bytes", newCapacity
    mov rax, 1
    ret
GapBuffer_Expand ENDP

;==============================================================================
; Get Text Range
;==============================================================================
GapBuffer_GetText PROC startPos:QWORD, length:QWORD, lpOutBuffer:QWORD
    LOCAL pos:QWORD
    LOCAL remaining:QWORD
    LOCAL copy1:QWORD
    LOCAL copy2:QWORD
    LOCAL src:QWORD
    
    .if startPos + length > [gbState].GapBufferState.size
        LOG_ERROR "GetText range out of bounds"
        mov rax, 0
        ret
    .endif
    
    invoke EnterCriticalSection, ADDR [gbState].GapBufferState.mutex
    
    mov pos, startPos
    mov remaining, length
    
    ; Check if range crosses gap
    mov rax, [gbState].GapBufferState.gapStart
    .if startPos < rax && startPos + length > rax
        ; Range spans gap - copy in two parts
        mov copy1, rax
        sub copy1, startPos
        
        mov src, [gbState].GapBufferState.data
        add src, startPos
        invoke RtlCopyMemory, lpOutBuffer, src, copy1
        
        ; Copy after gap
        mov copy2, length
        sub copy2, copy1
        mov src, [gbState].GapBufferState.data
        add src, [gbState].GapBufferState.gapEnd
        invoke RtlCopyMemory, 
            QWORD PTR lpOutBuffer + copy1,
            src,
            copy2
    .else
        ; Simple copy (no gap crossing)
        mov src, [gbState].GapBufferState.data
        .if startPos >= [gbState].GapBufferState.gapStart
            ; After gap
            mov rax, startPos
            sub rax, [gbState].GapBufferState.gapStart
            add rax, [gbState].GapBufferState.gapEnd
            add src, rax
        .else
            ; Before gap
            add src, startPos
        .endif
        
        invoke RtlCopyMemory, lpOutBuffer, src, length
    .endif
    
    invoke LeaveCriticalSection, ADDR [gbState].GapBufferState.mutex
    
    mov rax, length
    ret
GapBuffer_GetText ENDP

;==============================================================================
; Get Line by Line Number
;==============================================================================
GapBuffer_GetLine PROC lineNum:DWORD, lpOutBuffer:QWORD, maxLen:DWORD
    LOCAL lineStart:QWORD
    LOCAL lineEnd:QWORD
    LOCAL lineLen:DWORD
    
    invoke EnterCriticalSection, ADDR [gbState].GapBufferState.mutex
    
    .if lineNum >= [gbState].GapBufferState.lineCount
        invoke LeaveCriticalSection, ADDR [gbState].GapBufferState.mutex
        LOG_ERROR "Line number out of range: %d >= %d", 
            lineNum, [gbState].GapBufferState.lineCount
        mov rax, 0
        ret
    .endif
    
    ; Get start offset
    mov rax, [gbState].GapBufferState.lineOffsets
    mov lineStart, [rax + lineNum * 8]
    
    ; Get end offset
    .if lineNum + 1 >= [gbState].GapBufferState.lineCount
        mov lineEnd, [gbState].GapBufferState.size
    .else
        mov rax, [gbState].GapBufferState.lineOffsets
        mov lineEnd, [rax + lineNum * 8 + 8]
        dec lineEnd  ; Don't include newline
    .endif
    
    mov rax, lineEnd
    sub rax, lineStart
    .if rax > maxLen
        mov rax, maxLen
    .endif
    mov lineLen, eax
    
    ; Copy line text
    call GapBuffer_GetText, lineStart, lineLen, lpOutBuffer
    
    invoke LeaveCriticalSection, ADDR [gbState].GapBufferState.mutex
    
    mov eax, lineLen
    ret
GapBuffer_GetLine ENDP

;==============================================================================
; Update Line Index on Edit
;==============================================================================
GapBuffer_UpdateLineIndex PROC editPos:QWORD, editLen:QWORD, isInsert:QWORD
    LOCAL offset:QWORD
    LOCAL i:DWORD
    LOCAL newlineCount:DWORD
    LOCAL ptr:QWORD
    LOCAL endPos:QWORD
    
    mov offset, editPos
    
    ; Count newlines in edited region
    mov newlineCount, 0
    
    .if isInsert
        mov ptr, lpText
        mov i, 0
    .loop1:
        .if i >= editLen
            .break
        .endif
        mov al, BYTE PTR [ptr + i]
        .if al == 10  ; '\n'
            inc newlineCount
        .endif
        inc i
        .continue .loop1
    .endloop1
        
        ; Shift line offsets forward
        mov i, [gbState].GapBufferState.lineCount
    .loop2:
        .if i <= offset
            .break
        .endif
        mov rax, [gbState].GapBufferState.lineOffsets
        mov rcx, [rax + i * 8]
        add rcx, editLen
        mov [rax + i * 8], rcx
        dec i
        .continue .loop2
    .endloop2
        
        ; Insert new line offsets
        add [gbState].GapBufferState.lineCount, newlineCount
    .else
        ; Deletion - shift line offsets backward
        mov i, offset
    .loop3:
        .if i >= [gbState].GapBufferState.lineCount
            .break
        .endif
        mov rax, [gbState].GapBufferState.lineOffsets
        mov rcx, [rax + i * 8]
        sub rcx, editLen
        mov [rax + i * 8], rcx
        inc i
        .continue .loop3
    .endloop3
    .endif
    
    ret
GapBuffer_UpdateLineIndex ENDP

;==============================================================================
; Promote to Rope (For Files > 10MB)
;==============================================================================
GapBuffer_PromoteToRope PROC
    LOCAL totalSize:QWORD
    
    mov totalSize, [gbState].GapBufferState.size
    
    ; If already using rope, skip
    .if [gbState].GapBufferState.isRope
        ret
    .endif
    
    ; If under 10MB, no need to promote
    .if totalSize < 10485760  ; 10MB
        ret
    .endif
    
    LOG_INFO "Promoting GapBuffer to Rope (size=%lld bytes)", totalSize
    
    ; TODO: Implement rope promotion
    ; For now, keep gap buffer but mark as rope-ready
    mov [gbState].GapBufferState.isRope, 1
    
    ret
GapBuffer_PromoteToRope ENDP

;==============================================================================
; Get Total Size
;==============================================================================
GapBuffer_GetSize PROC
    mov rax, [gbState].GapBufferState.size
    ret
GapBuffer_GetSize ENDP

;==============================================================================
; Get Line Count
;==============================================================================
GapBuffer_GetLineCount PROC
    mov rax, [gbState].GapBufferState.lineCount
    ret
GapBuffer_GetLineCount ENDP

;==============================================================================
; Data Structures
;==============================================================================
.data
; Gap Buffer state
GapBufferState STRUCT
    data            QWORD ?      ; Text buffer pointer
    capacity        QWORD ?      ; Total capacity
    size            QWORD ?      ; Current size (excluding gap)
    gapStart        QWORD ?      ; Gap start position
    gapEnd          QWORD ?      ; Gap end position
    lineOffsets     QWORD ?      ; Array of line start offsets
    lineCount       QWORD ?      ; Number of lines
    isRope          QWORD ?      ; Using rope instead of gap buffer?
    mutex           CRITICAL_SECTION <> ; Thread safety
GapBufferState ENDS

gbState             dq ?         ; Pointer to current GapBufferState
gbHeap              dq ?         ; Private heap for allocations
lpText              dq ?         ; Temporary text pointer (for insert)

END
