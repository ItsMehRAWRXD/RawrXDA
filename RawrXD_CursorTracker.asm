; ============================================================================
; RawrXD_CursorTracker.asm - Cursor Position and Selection Management
; ============================================================================
; Manages:
; - Current cursor position (line, column, byte offset)
; - Selection state (start, end, active)
; - Cursor bounds checking
; - Navigation commands (up, down, left, right, home, end, etc.)
; ============================================================================

.CODE

; Cursor structure (64 bytes):
; Offset 0:  qword - current byte offset
; Offset 8:  qword - current line
; Offset 16: qword - current column
; Offset 24: qword - selection start byte offset (-1 if no selection)
; Offset 32: qword - selection end byte offset (-1 if no selection)
; Offset 40: qword - last line (for bounds checking)
; Offset 48: dword - blink state (0=off, 1=on)
; Offset 52: dword - blink timer
; Offset 56: qword - buffer_ptr (reference)


; ============================================================================
; Cursor_Initialize(rcx = cursor_ptr, rdx = buffer_ptr)
;
; Initialize cursor at position (0, 0)
; ============================================================================
Cursor_Initialize PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    ; Initialize cursor to (0,0)
    mov qword [rcx], 0                 ; byte_offset = 0
    mov qword [rcx + 8], 0             ; line = 0
    mov qword [rcx + 16], 0            ; column = 0
    mov qword [rcx + 24], -1           ; selection_start = -1 (no selection)
    mov qword [rcx + 32], -1           ; selection_end = -1
    mov qword [rcx + 40], 0            ; last_line = 0
    mov dword [rcx + 48], 1            ; blink_state = on
    mov dword [rcx + 52], 0            ; blink_timer = 0
    mov [rcx + 56], rdx                ; buffer_ptr = rdx
    
    mov rax, 1
    ret
Cursor_Initialize ENDP


; ============================================================================
; Cursor_MoveLeft(rcx = cursor_ptr)
;
; Move cursor one position left with bounds checking
; Returns: rax = success (1) or at boundary (0)
; ============================================================================
Cursor_MoveLeft PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx]                     ; rax = current byte offset
    
    test rax, rax
    jz .AlreadyAtStart                 ; Already at position 0
    
    ; Move left
    dec rax
    mov [rcx], rax                     ; Update byte offset
    
    ; Update line/column from new byte offset
    mov rdx, [rcx + 56]                ; rdx = buffer_ptr
    call Cursor_UpdateLineColumn
    
    mov rax, 1
    ret

.AlreadyAtStart:
    xor rax, rax                       ; Return failure
    ret
Cursor_MoveLeft ENDP


; ============================================================================
; Cursor_MoveRight(rcx = cursor_ptr, rdx = buffer_length)
;
; Move cursor one position right with bounds checking
; Returns: rax = success (1) or at boundary (0)
; ============================================================================
Cursor_MoveRight PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx]                     ; rax = current byte offset
    
    cmp rax, rdx
    jge .AlreadyAtEnd                  ; Already at end
    
    ; Move right
    inc rax
    mov [rcx], rax                     ; Update byte offset
    
    ; Update line/column
    mov r8, [rcx + 56]                 ; r8 = buffer_ptr
    call Cursor_UpdateLineColumn
    
    mov rax, 1
    ret

.AlreadyAtEnd:
    xor rax, rax
    ret
Cursor_MoveRight ENDP


; ============================================================================
; Cursor_MoveUp(rcx = cursor_ptr, rdx = buffer_ptr)
;
; Move cursor up one line, maintaining column if possible
; Returns: rax = success (1) or at top line (0)
; ============================================================================
Cursor_MoveUp PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx + 8]                 ; rax = current line
    
    test rax, rax
    jz .AlreadyAtTop                   ; Already at line 0
    
    ; Move to previous line
    dec rax
    mov r8, [rcx + 16]                 ; r8 = current column to maintain
    
    ; Calculate new byte offset from line and column
    ; Use buffer line table to find line start
    mov r9, [rdx + 32 + rax*8]         ; r9 = line start offset
    add r9, r8                         ; Add column
    
    ; Verify we don't exceed the line length
    mov r10, [rdx]                     ; r10 = data pointer
    xor r11, r11                       ; r11 = column counter
    
    ; Find actual end of line
.CountLineLen:
    mov r12b, [r10 + r9 + r11]
    cmp r12b, 10                       ; Newline?
    je .LineEnd
    cmp r12b, 0                        ; End of buffer?
    je .LineEnd
    inc r11
    cmp r11, 256                       ; Max line length
    jl .CountLineLen

.LineEnd:
    ; r11 = line length
    cmp r8, r11
    jle .ColumnOK
    mov r8, r11                        ; Clamp to line length

.ColumnOK:
    mov r9, [rdx + 32 + rax*8]
    add r9, r8
    mov [rcx], r9                      ; Update byte offset
    mov [rcx + 8], rax                 ; Update line
    mov [rcx + 16], r8                 ; Keep column
    
    mov rax, 1
    ret

.AlreadyAtTop:
    xor rax, rax
    ret
Cursor_MoveUp ENDP


; ============================================================================
; Cursor_MoveDown(rcx = cursor_ptr, rdx = buffer_ptr)
;
; Move cursor down one line, maintaining column if possible
; Returns: rax = success (1) or at bottom line (0)
; ============================================================================
Cursor_MoveDown PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx + 8]                 ; rax = current line
    mov r8, [rdx + 24]                 ; r8 = total line count
    
    cmp rax, r8
    jge .AlreadyAtBottom               ; Already at last line
    
    ; Move to next line
    inc rax
    mov r9, [rcx + 16]                 ; r9 = current column to maintain
    
    ; Calculate new byte offset
    mov r10, [rdx + 32 + rax*8]        ; r10 = next line start
    add r10, r9                        ; Add column
    
    ; Verify column doesn't exceed next line length
    mov r11, [rdx]                     ; r11 = data pointer
    xor r12, r12                       ; r12 = column counter

.CountLineLen2:
    mov r13b, [r11 + r10 + r12]
    cmp r13b, 10
    je .LineEnd2
    cmp r13b, 0
    je .LineEnd2
    inc r12
    cmp r12, 256
    jl .CountLineLen2

.LineEnd2:
    cmp r9, r12
    jle .ColumnOK2
    mov r9, r12                        ; Clamp to line length

.ColumnOK2:
    mov r10, [rdx + 32 + rax*8]
    add r10, r9
    mov [rcx], r10                     ; Update byte offset
    mov [rcx + 8], rax                 ; Update line
    mov [rcx + 16], r9                 ; Keep column
    
    mov rax, 1
    ret

.AlreadyAtBottom:
    xor rax, rax
    ret
Cursor_MoveDown ENDP


; ============================================================================
; Cursor_MoveHome(rcx = cursor_ptr)
;
; Move cursor to beginning of current line
; ============================================================================
Cursor_MoveHome PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx + 8]                 ; rax = current line
    mov rdx, [rcx + 56]                ; rdx = buffer_ptr
    mov r8, [rdx + 32 + rax*8]         ; r8 = line start offset
    
    mov [rcx], r8                      ; Update byte offset
    mov [rcx + 16], 0                  ; Update column to 0
    
    mov rax, 1
    ret
Cursor_MoveHome ENDP


; ============================================================================
; Cursor_MoveEnd(rcx = cursor_ptr)
;
; Move cursor to end of current line
; ============================================================================
Cursor_MoveEnd PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx + 8]                 ; rax = current line
    mov rdx, [rcx + 56]                ; rdx = buffer_ptr
    mov r8, [rdx]                      ; r8 = data pointer
    
    ; Find end of current line
    mov r9, [rdx + 32 + rax*8]         ; r9 = line start
    xor r10, r10                       ; r10 = column counter

.FindLineEnd:
    mov r11b, [r8 + r9 + r10]
    cmp r11b, 10                       ; Newline?
    je .AtLineEnd
    cmp r11b, 0                        ; End of buffer?
    je .AtLineEnd
    inc r10
    cmp r10, 256
    jl .FindLineEnd

.AtLineEnd:
    add r9, r10
    mov [rcx], r9                      ; Update byte offset
    mov [rcx + 16], r10                ; Update column
    
    mov rax, 1
    ret
Cursor_MoveEnd ENDP


; ============================================================================
; Cursor_MoveStartOfDocument(rcx = cursor_ptr)
;
; Move cursor to start of document (0, 0)
; ============================================================================
Cursor_MoveStartOfDocument PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov qword [rcx], 0                 ; byte_offset = 0
    mov qword [rcx + 8], 0             ; line = 0
    mov qword [rcx + 16], 0            ; column = 0
    
    mov rax, 1
    ret
Cursor_MoveStartOfDocument ENDP


; ============================================================================
; Cursor_MoveEndOfDocument(rcx = cursor_ptr, rdx = buffer_ptr)
;
; Move cursor to end of document
; ============================================================================
Cursor_MoveEndOfDocument PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rdx + 16]                ; rax = buffer length
    mov [rcx], rax                     ; byte_offset = buffer_length
    
    ; Update line/column info
    mov r8, rdx
    call Cursor_UpdateLineColumn
    
    mov rax, 1
    ret
Cursor_MoveEndOfDocument ENDP


; ============================================================================
; Cursor_UpdateLineColumn(rcx = cursor_ptr, r8 = buffer_ptr)
;
; Recalculate line and column from current byte offset
; ============================================================================
Cursor_UpdateLineColumn PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx]                     ; rax = byte offset
    mov rdx, [r8]                      ; rdx = data pointer
    xor r9, r9                         ; r9 = line counter
    xor r10, r10                       ; r10 = column counter
    xor r11, r11                       ; r11 = offset counter

.ScanToPos:
    cmp r11, rax
    jge .ReachedPos
    
    mov r12b, [rdx + r11]
    cmp r12b, 10                       ; Newline?
    jne .NotNL
    
    inc r9
    xor r10, r10
    jmp .NextScan

.NotNL:
    inc r10

.NextScan:
    inc r11
    jmp .ScanToPos

.ReachedPos:
    mov [rcx + 8], r9                  ; Update line
    mov [rcx + 16], r10                ; Update column
    ret
Cursor_UpdateLineColumn ENDP


; ============================================================================
; Cursor_SelectTo(rcx = cursor_ptr, rdx = end_offset)
;
; Set selection from current position to end_offset
; ============================================================================
Cursor_SelectTo PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx]                     ; rax = current byte offset
    
    ; Set selection range
    mov [rcx + 24], rax                ; selection_start = current position
    mov [rcx + 32], rdx                ; selection_end = end_offset
    
    mov rax, 1
    ret
Cursor_SelectTo ENDP


; ============================================================================
; Cursor_ClearSelection(rcx = cursor_ptr)
;
; Clear any active selection
; ============================================================================
Cursor_ClearSelection PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov qword [rcx + 24], -1           ; selection_start = -1
    mov qword [rcx + 32], -1           ; selection_end = -1
    
    mov rax, 1
    ret
Cursor_ClearSelection ENDP


; ============================================================================
; Cursor_IsSelected(rcx = cursor_ptr)
;
; Check if there's an active selection
; Returns: rax = 1 if selected, 0 if not
; ============================================================================
Cursor_IsSelected PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx + 24]
    cmp rax, -1
    je .NoSelection
    
    mov rax, 1
    ret

.NoSelection:
    xor rax, rax
    ret
Cursor_IsSelected ENDP


; ============================================================================
; Cursor_GetSelection(rcx = cursor_ptr)
;
; Get selection start and end
; Returns: rax = start, rdx = end (or -1, -1 if no selection)
; ============================================================================
Cursor_GetSelection PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx + 24]
    mov rdx, [rcx + 32]
    ret
Cursor_GetSelection ENDP


; ============================================================================
; Cursor_SetBlink(rcx = cursor_ptr, rdx = state)
;
; Set cursor blink state (0=off, 1=on)
; ============================================================================
Cursor_SetBlink PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov [rcx + 48], edx                ; Set blink_state
    
    mov rax, 1
    ret
Cursor_SetBlink ENDP


; ============================================================================
; Cursor_UpdateBlink(rcx = cursor_ptr, rdx = delta_ms)
;
; Update blink timer (call every frame/timer tick)
; Cursor blinks (500ms on, 500ms off pattern)
; ============================================================================
Cursor_UpdateBlink PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov eax, [rcx + 52]                ; eax = blink_timer (current)
    add eax, edx                       ; Add delta_ms
    
    cmp eax, 500
    jl .UpdateTimer
    
    ; Cycle blink state
    mov edx, [rcx + 48]
    xor edx, 1                         ; Toggle blink state
    mov [rcx + 48], edx
    
    xor eax, eax                       ; Reset timer
    
.UpdateTimer:
    mov [rcx + 52], eax
    ret
Cursor_UpdateBlink ENDP


; ============================================================================
; Cursor_GetBlink(rcx = cursor_ptr)
;
; Get current blink state
; Returns: rax = blink_state (0=off, 1=on)
; ============================================================================
Cursor_GetBlink PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov eax, [rcx + 48]
    movzx rax, eax
    ret
Cursor_GetBlink ENDP


; ============================================================================
; Cursor_PageUp(rcx = cursor_ptr, rdx = buffer_ptr, r8 = lines_per_page)
;
; Move cursor up by lines_per_page lines
; ============================================================================
Cursor_PageUp PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx + 8]                 ; rax = current line
    sub rax, r8                        ; Subtract lines_per_page
    
    js .AtTop                          ; If negative, go to top
    
    mov [rcx + 8], rax                 ; Update line
    
    ; Calculate new byte offset and update
    mov r9, [rdx + 32 + rax*8]
    mov r10, [rcx + 16]
    add r9, r10
    mov [rcx], r9
    
    mov rax, 1
    ret

.AtTop:
    call Cursor_MoveStartOfDocument
    ret
Cursor_PageUp ENDP


; ============================================================================
; Cursor_PageDown(rcx = cursor_ptr, rdx = buffer_ptr, r8 = lines_per_page)
;
; Move cursor down by lines_per_page lines
; ============================================================================
Cursor_PageDown PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx + 8]                 ; rax = current line
    add rax, r8                        ; Add lines_per_page
    
    mov r9, [rdx + 24]                 ; r9 = total line count
    cmp rax, r9
    jge .AtBottom                      ; If >= line count, go to bottom
    
    mov [rcx + 8], rax                 ; Update line
    
    ; Calculate new byte offset
    mov r10, [rdx + 32 + rax*8]
    mov r11, [rcx + 16]
    add r10, r11
    mov [rcx], r10
    
    mov rax, 1
    ret

.AtBottom:
    call Cursor_MoveEndOfDocument
    ret
Cursor_PageDown ENDP

END
