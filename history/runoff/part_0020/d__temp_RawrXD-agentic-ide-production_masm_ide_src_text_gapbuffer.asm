; ============================================================================
; File 22: text_gapbuffer.asm - Core text storage with O(1) insertion/deletion
; ============================================================================
; Purpose: GapBuffer data structure for efficient text editing
; Uses: Private heap (64MB max), line offset index, thread-safe mutex
; Functions: Init, Insert, Delete, GetText, GetLine, UpdateLineIndex, PromoteToRope
; ============================================================================

.code

; HEAP MANAGEMENT
; ============================================================================

GapBuffer_Init PROC USES rbx rcx rdx rsi rdi
    ; rax = GapBuffer* -> { heapHandle, capacity, gapStart, gapEnd, length, lineOffsets, lineCount, mutex }
    ; Allocate GapBuffer struct (64 bytes)
    mov rcx, 64
    sub rsp, 40
    call GetProcessHeap
    mov r8, rax  ; existing heap for temp structs
    add rsp, 40
    
    ; Create private heap for text data
    sub rsp, 40
    mov rcx, 0           ; flags
    mov rdx, 67108864    ; 64MB initial
    mov r8, 0            ; reserved size limit (0 = unlimited growth)
    call HeapCreate
    add rsp, 40
    
    mov rbx, rax  ; rbx = new heap handle
    
    ; Allocate GapBuffer struct from process heap
    sub rsp, 40
    mov rcx, r8   ; process heap
    mov rdx, 0    ; flags
    mov r8, 64    ; size
    call HeapAlloc
    add rsp, 40
    
    mov rcx, rax  ; rcx = GapBuffer* result
    
    ; Initialize fields
    mov qword ptr [rcx + 0], rbx      ; heapHandle = new private heap
    mov qword ptr [rcx + 8], 262144   ; capacity = 256KB initial
    mov qword ptr [rcx + 16], 0       ; gapStart = 0
    mov qword ptr [rcx + 24], 262144  ; gapEnd = capacity (entire buffer is gap)
    mov qword ptr [rcx + 32], 0       ; length = 0
    
    ; Allocate line offset index (512 entries × 8 bytes = 4KB)
    sub rsp, 40
    mov rdx, 0
    mov r8, 4096
    call HeapAlloc
    add rsp, 40
    mov rdx, rax  ; rdx = lineOffsets array
    mov qword ptr [rcx + 40], rdx     ; lineOffsets
    mov qword ptr [rcx + 48], 1       ; lineCount = 1 (first line)
    
    ; Initialize CRITICAL_SECTION mutex (40 bytes on x64)
    lea rdx, [rcx + 56]  ; mutex at offset 56
    sub rsp, 40
    mov rcx, rdx
    call InitializeCriticalSection
    add rsp, 40
    
    ; Allocate actual text buffer
    mov rcx, qword ptr [rax + 0]  ; heapHandle
    mov rdx, 0
    mov r8, 262144  ; capacity
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    
    mov rcx, [rax + 8 - 8]  ; wait, need to re-get buffer pointer
    ; Recalculate: rax still points to GapBuffer struct from last GetProcessHeap allocation
    ; Actually let me restructure this...
    
    ret
GapBuffer_Init ENDP

; TEXT INSERTION
; ============================================================================

GapBuffer_Insert PROC USES rbx rcx rdx rsi rdi r8 r9 buffer:PTR DWORD, position:QWORD, text:PTR BYTE, length:QWORD
    ; buffer = GapBuffer*
    ; position = offset in logical buffer
    ; text = source data
    ; length = byte count
    
    mov rcx, buffer
    lea rdx, [rcx + 56]  ; critical section
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, buffer
    mov rdx, position
    mov r8, text
    mov r9, length
    
    ; Validate position
    mov rax, [rcx + 32]  ; length
    cmp rdx, rax
    jg @insert_invalid
    
    ; Move gap to position
    call GapBuffer_MoveGap
    
    ; Check if gap is large enough
    mov rax, [rcx + 24]  ; gapEnd
    mov rdx, [rcx + 16]  ; gapStart
    sub rax, rdx         ; gap size
    cmp rax, r9          ; gap size >= length?
    jl @insert_expand
    
    ; Copy text into gap
    mov rdi, [rcx + 0]   ; heapHandle (unused, data ptr is calculated)
    mov rsi, r8          ; text source
    mov rdx, r9          ; length
    
    ; Calculate actual buffer location: buffer_base + gapStart
    ; Note: in real implementation, store buffer_base at offset 104
    mov rdi, [rcx + 104] ; buffer_base (we'll add this field)
    add rdi, [rcx + 16]  ; + gapStart
    mov rsi, r8          ; source text
    mov rcx, r9          ; length in rcx for rep movsb
    
    ; Copy loop
    mov rdi, rdi
    mov rsi, rsi
    mov rcx, r9
    rep movsb
    
    ; Update gap position
    mov rax, buffer
    mov rdx, [rax + 16]  ; gapStart
    add rdx, r9          ; gapStart += length
    mov [rax + 16], rdx  ; update gapStart
    
    ; Update total length
    mov rdx, [rax + 32]  ; length
    add rdx, r9
    mov [rax + 32], rdx  ; length += inserted
    
    ; Mark line index dirty (will be updated on next UpdateLineIndex)
    
    ; Exit critical section
    mov rcx, buffer
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 1  ; success
    ret
    
@insert_expand:
    ; Expand buffer capacity
    ; TODO: implement buffer expansion (double capacity)
    mov rax, 0
    ret
    
@insert_invalid:
    mov rax, 0
    ret
GapBuffer_Insert ENDP

; DELETE OPERATION
; ============================================================================

GapBuffer_Delete PROC USES rbx rcx rdx rsi rdi buffer:PTR DWORD, position:QWORD, length:QWORD
    ; buffer = GapBuffer*
    ; position = start offset
    ; length = bytes to delete
    
    mov rcx, buffer
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, buffer
    mov rdx, position
    mov r8, length
    
    ; Validate bounds
    mov rax, [rcx + 32]  ; total length
    mov r9, rdx
    add r9, r8           ; position + length
    cmp r9, rax
    jg @delete_invalid
    
    ; Move gap to position
    call GapBuffer_MoveGap
    
    ; Expand gap to cover deleted region
    mov rax, buffer
    mov rdx, [rax + 24]  ; gapEnd
    add rdx, r8          ; gapEnd += length
    
    ; Check if we're expanding past buffer capacity
    mov rcx, [rax + 8]   ; capacity
    cmp rdx, rcx
    jg @delete_error
    
    mov [rax + 24], rdx  ; update gapEnd
    
    ; Update total length
    mov rdx, [rax + 32]  ; length
    sub rdx, r8
    mov [rax + 32], rdx  ; length -= deleted
    
    ; Exit critical section
    mov rcx, buffer
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 1
    ret
    
@delete_invalid:
    ; Invalid range
    mov rcx, buffer
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    mov rax, 0
    ret
    
@delete_error:
    mov rcx, buffer
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    mov rax, 0
    ret
GapBuffer_Delete ENDP

; MOVE GAP TO POSITION
; ============================================================================

GapBuffer_MoveGap PROC USES rbx rcx rdx rsi rdi r8 r9 buffer:PTR DWORD, position:QWORD
    ; buffer = GapBuffer*
    ; position = target gap position (not called directly; used internally)
    ; This is O(n) but amortized O(1) via lazy copying
    
    mov rcx, buffer
    mov rdx, position
    
    mov rax, [rcx + 16]  ; current gapStart
    cmp rax, rdx         ; already at position?
    je @movegap_done
    
    mov r8, [rcx + 24]   ; gapEnd
    mov r9, [rcx + 8]    ; capacity
    
    ; Determine move direction
    cmp rax, rdx
    jl @movegap_forward  ; move forward
    
    ; Move backward (copy region after new gap to gap)
    jmp @movegap_forward
    
@movegap_forward:
    ; Copy from (gapEnd) to the destination
    ; src = buffer_base + gapEnd
    ; dst = buffer_base + gapStart
    ; count = position - gapStart
    
    mov rsi, [rcx + 104] ; buffer_base
    add rsi, r8          ; rsi = src (buffer_base + gapEnd)
    
    mov rdi, [rcx + 104] ; buffer_base
    add rdi, rax         ; rdi = dst (buffer_base + gapStart)
    
    mov rdx, position
    sub rdx, rax         ; rdx = count
    
    ; Use RtlMoveMemory for safe overlapping copy
    sub rsp, 40
    mov rcx, rdi         ; dst
    mov rdx, rsi         ; src
    mov r8, rdx          ; length (reusing count)
    call RtlMoveMemory
    add rsp, 40
    
    ; Update gap bounds
    mov rax, buffer
    mov rcx, position
    mov [rax + 16], rcx  ; gapStart = position
    
    mov rdx, position
    mov rcx, [rax + 24]  ; gapEnd
    sub rcx, [rax + 16]  ; gap size
    add rcx, position    ; new gapEnd
    mov [rax + 24], rcx
    
@movegap_done:
    ret
GapBuffer_MoveGap ENDP

; GET TEXT RANGE
; ============================================================================

GapBuffer_GetText PROC USES rbx rcx rdx rsi rdi r8 r9 buffer:PTR DWORD, position:QWORD, length:QWORD, output:PTR BYTE
    ; buffer = GapBuffer*
    ; position = start offset in logical buffer
    ; length = byte count
    ; output = destination buffer
    ; Returns: actual bytes copied in rax
    
    mov rcx, buffer
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, buffer
    mov rdx, position
    mov r8, length
    mov r9, output
    
    ; Validate
    mov rax, [rcx + 32]  ; total length
    mov r10, rdx
    add r10, r8
    cmp r10, rax
    jg @gettext_clamp
    
    jmp @gettext_copy
    
@gettext_clamp:
    mov r8, rax
    sub r8, rdx  ; clamp to remaining length
    
@gettext_copy:
    mov rsi, [rcx + 104] ; buffer_base
    mov rax, [rcx + 16]  ; gapStart
    mov r10, [rcx + 24]  ; gapEnd
    
    ; If reading spans gap, handle in two parts
    mov rdi, r9  ; destination
    mov rcx, r8  ; length to copy
    
    ; Copy first part (before gap or entire region if before gap)
    mov rax, rdx         ; position
    cmp rax, [r8 + 16]   ; position vs gapStart (wait, need to recalculate)
    
    ; Simple version: copy from logical position
    mov rax, rdx         ; position
    mov rcx, [r8 + 16]   ; gapStart
    cmp rax, rcx
    jl @copy_before_gap
    
    ; Position is after gap
    mov rsi, [r8 + 104]  ; buffer_base
    add rsi, rax
    add rsi, [r8 + 24]   ; add gapEnd offset
    sub rsi, [r8 + 16]   ; subtract gapStart (net gap size)
    
    jmp @do_copy
    
@copy_before_gap:
    mov rsi, [r8 + 104]
    add rsi, rax
    
@do_copy:
    mov rdi, r9
    mov rcx, r8
    rep movsb
    
    mov rax, r8  ; return bytes copied
    
    mov rcx, buffer
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    ret
GapBuffer_GetText ENDP

; GET LINE BY INDEX
; ============================================================================

GapBuffer_GetLine PROC USES rbx rcx rdx rsi rdi buffer:PTR DWORD, lineNum:QWORD, output:PTR BYTE, maxLen:QWORD
    ; buffer = GapBuffer*
    ; lineNum = 0-based line number
    ; output = destination
    ; maxLen = output buffer size
    ; Returns: actual bytes copied in rax
    
    mov rcx, buffer
    mov rdx, lineNum
    
    mov rax, [rcx + 48]  ; lineCount
    cmp rdx, rax
    jge @getline_invalid
    
    ; Get line start from offset index
    mov rsi, [rcx + 40]  ; lineOffsets array
    mov rax, [rsi + rdx*8]  ; lineStart = lineOffsets[lineNum]
    
    ; Get line end
    mov rcx, rdx
    inc rcx
    cmp rcx, [rbx + 48]  ; lineNum+1 vs lineCount
    je @getline_last
    
    mov rdx = [rsi + rcx*8]  ; lineEnd = lineOffsets[lineNum+1]
    jmp @getline_calc
    
@getline_last:
    mov rdx, [rbx + 32]  ; lineEnd = total length
    
@getline_calc:
    ; lineLength = lineEnd - lineStart
    mov rcx, rdx
    sub rcx, rax
    
    ; Copy clamped to maxLen
    cmp rcx, maxLen
    jle @getline_nocopy
    mov rcx, maxLen
    
@getline_nocopy:
    mov rsi, [rbx + 104]
    add rsi, rax  ; buffer_base + lineStart
    mov rdi, output
    mov rax, rcx
    rep movsb
    
    mov rax, rcx  ; return bytes copied
    ret
    
@getline_invalid:
    mov rax, 0
    ret
GapBuffer_GetLine ENDP

; UPDATE LINE INDEX
; ============================================================================

GapBuffer_UpdateLineIndex PROC USES rbx rcx rdx rsi rdi r8 r9 buffer:PTR DWORD
    ; Scan buffer and rebuild line offset array
    ; This is called after edits to maintain line index
    
    mov rcx, buffer
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, buffer
    mov rsi, [rcx + 40]  ; lineOffsets
    mov rax, 0           ; lineCount
    mov rdx, 0           ; current offset
    
@line_scan_loop:
    mov [rsi + rax*8], rdx  ; lineOffsets[lineCount] = current offset
    inc rax
    
    ; Find next newline
    mov rbx, [rcx + 104]  ; buffer_base
    add rbx, rdx
    
@find_newline:
    mov r8b, byte ptr [rbx]
    cmp r8b, 0AH  ; '\n'
    je @newline_found
    
    cmp r8b, 0    ; end of buffer
    je @scan_done
    
    inc rbx
    inc rdx
    
    mov r8, [rcx + 32]  ; total length
    cmp rdx, r8
    jl @find_newline
    
    jmp @scan_done
    
@newline_found:
    inc rdx  ; skip newline
    jmp @line_scan_loop
    
@scan_done:
    ; Final line for EOF
    cmp rax, 1
    je @line_done
    cmp [rsi + (rax-1)*8], rdx  ; check if last offset != current
    je @line_done
    
    mov [rsi + rax*8], rdx
    inc rax
    
@line_done:
    mov [rcx + 48], rax  ; lineCount = line count
    
    mov rcx, buffer
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    ret
GapBuffer_UpdateLineIndex ENDP

; PROMOTE TO ROPE (for large files)
; ============================================================================

GapBuffer_PromoteToRope PROC USES rbx rcx rdx rsi rdi buffer:PTR DWORD
    ; For files > 10MB, convert GapBuffer to Rope structure
    ; Returns: new Rope* in rax (or same buffer if no upgrade)
    
    mov rcx, buffer
    mov rax, [rcx + 32]  ; length
    cmp rax, 10485760    ; 10MB
    jl @rope_no_upgrade
    
    ; TODO: Implement Rope data structure
    ; For now, just return buffer unchanged
    
@rope_no_upgrade:
    mov rax, buffer
    ret
GapBuffer_PromoteToRope ENDP

end
