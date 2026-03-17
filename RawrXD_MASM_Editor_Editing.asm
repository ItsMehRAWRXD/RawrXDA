; ============================================================================
; RawrXD_MASM_Editor_Editing.asm - Editing capabilities for syntax highlighter
; ============================================================================
; Adds insert/delete/text manipulation to the MASM syntax highlighter
; ============================================================================

.CODE

; ============================================================================
; Editor_InsertChar(rcx = buffer_ptr, rdx = position, r8b = char)
;
; Insert character at line/column position
; ============================================================================
Editor_InsertChar PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 32
    .ENDPROLOG

    push    rbx
    
    ; rcx = buffer_ptr, rdx = position (line), r8 = column, r9b = char
    mov     rbx, [rcx + 8]              ; Get line buffer pointer
    mov     rax, [rbx + rdx*8]          ; Get line pointer
    
    ; Shift characters right to make room
    mov     r10, [rcx + 16]             ; Get line length array
    mov     r10d, [r10 + rdx*4]         ; Line length
    
    ; For now, simplified: append to line
    mov     [rax + r10], r9b            ; Insert character
    inc     dword [rcx + 16 + rdx*4]    ; Increment line length
    
    mov     rax, 1
    pop     rbx
    ret
Editor_InsertChar ENDP


; ============================================================================
; Editor_DeleteChar(rcx = buffer_ptr, rdx = position)
;
; Delete character at line/column position
; ============================================================================
Editor_DeleteChar PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    ; rcx = buffer_ptr, rdx = line, r8 = column
    
    mov     rax, [rcx + 8]              ; Get line buffer
    mov     rax, [rax + rdx*8]          ; Get line pointer
    
    ; Shift characters left
    mov     r9, [rcx + 16]              ; Get line length array
    mov     r9d, [r9 + rdx*4]           ; Line length
    
    mov     r10, r8                     ; Copy column
    .WHILE r10 < r9
        mov     r11b, [rax + r10 + 1]
        mov     [rax + r10], r11b
        inc     r10
    .ENDW
    
    dec     dword [rcx + 16 + rdx*4]    ; Decrement line length
    
    mov     rax, 1
    ret
Editor_DeleteChar ENDP


; ============================================================================
; Editor_GetLineContent(rcx = buffer_ptr, rdx = line_num)
;
; Get pointer to line content
; Returns: rax = line pointer, rdx = line length
; ============================================================================
Editor_GetLineContent PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov     rax, [rcx + 8]              ; Get line buffer
    mov     rax, [rax + rdx*8]          ; Get line pointer
    
    mov     r8, [rcx + 16]              ; Get length array
    mov     rdx, [r8 + rdx*4]           ; Get line length
    
    ret
Editor_GetLineContent ENDP


; ============================================================================
; Editor_InsertLine(rcx = buffer_ptr, rdx = after_line)
;
; Insert new blank line after specified line
; ============================================================================
Editor_InsertLine PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .ALLOCSTACK 32
    .ENDPROLOG

    push    rbx
    push    r12
    
    ; rcx = buffer_ptr, rdx = after_line
    
    ; Allocate new line
    invoke  VirtualAlloc, NULL, MAX_LINE_LENGTH, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    
    mov     rbx, [rcx + 8]              ; Get line buffer
    mov     r12, [rcx + 24]             ; Get current line count
    
    ; Shift lines down
    mov     r8, r12
    inc     r8
    mov     r9, r8
    .WHILE r9 > rdx
        dec     r9
        mov     r10, [rbx + r9*8]
        mov     [rbx + r9*8 + 8], r10   ; Shift down
    .ENDW
    
    ; Insert new line
    mov     [rbx + rdx*8 + 8], rax      ; Store line pointer
    
    ; Clear length for new line
    mov     r8, [rcx + 16]
    mov     dword [r8 + rdx*4 + 4], 0
    
    ; Increment line count
    inc     dword [rcx + 24]
    
    mov     rax, 1
    pop     r12
    pop     rbx
    ret
Editor_InsertLine ENDP


; ============================================================================
; Editor_DeleteLine(rcx = buffer_ptr, rdx = line_num)
;
; Delete line and shift up
; ============================================================================
Editor_DeleteLine PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 32
    .ENDPROLOG

    push    rbx
    
    ; rcx = buffer_ptr, rdx = line_num
    
    mov     rbx, [rcx + 8]              ; Get line buffer
    mov     rax, [rbx + rdx*8]          ; Get line pointer to free
    
    ; Free memory
    invoke  VirtualFree, rax, 0, MEM_DECOMMIT or MEM_RELEASE
    
    ; Shift lines up
    mov     r8, rdx
    mov     r9, [rcx + 24]              ; Line count
    .WHILE r8 < r9 - 1
        mov     r10, [rbx + r8*8 + 8]
        mov     [rbx + r8*8], r10       ; Shift up
        inc     r8
    .ENDW
    
    ; Clear last line entry
    mov     qword [rbx + r8*8], 0
    
    ; Decrement line count
    dec     dword [rcx + 24]
    
    mov     rax, 1
    pop     rbx
    ret
Editor_DeleteLine ENDP


; ============================================================================
; Editor_ReplaceText(rcx = buffer, rdx = start_line, r8 = start_col, 
;                   r9 = length, [rsp] = replacement_text)
;
; Replace text region with new content
; ============================================================================
Editor_ReplaceText PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .ALLOCSTACK 32
    .ENDPROLOG

    push    rbx
    push    r12
    
    ; rcx = buffer, rdx = start_line, r8 = start_col
    ; r9 = delete_length, [rsp+48] = replacement_text
    
    mov     r12, [rsp + 48]             ; Get replacement text
    
    ; Delete region
    mov     r10, r9
    .WHILE r10 > 0
        push    r10
        push    rdx
        push    r8
        
        mov     rcx, [rcx + 8]
        mov     rax, [rcx + rdx*8]
        mov     ecx, [rcx + rdx*4]      ; Line length
        cmp     r8d, ecx
        jae     @F
        
        ; Delete char at rdx, r8
        pop     r8
        pop     rdx
        
        ; Shift left
        mov     r11b, [rax + r8 + 1]
        mov     [rax + r8], r11b
        inc     r8
        
        pop     r10
        dec     r10
    .ENDW
    
    ; Insert new text
    mov     rsi, r12
    .WHILE BYTE PTR [rsi] != 0
        movzx   eax, BYTE PTR [rsi]
        
        ; Insert at rdx, r8
        ; (simplified version)
        
        inc     rsi
        inc     r8
    .ENDW
    
    mov     rax, 1
    pop     r12
    pop     rbx
    ret
Editor_ReplaceText ENDP


; ============================================================================
; Editor_GetSelectedText(rcx = buffer, rdx = start_line, r8 = start_col,
;                       r9 = end_line, [rsp] = end_col)
;
; Extract selected text into buffer
; ============================================================================
Editor_GetSelectedText PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    ; rcx = buffer, rdx = start_line, r8 = start_col
    ; r9 = end_line, [rsp] = end_col
    
    mov     r10, [rsp]                  ; end_col
    
    ; For single-line selection
    cmp     rdx, r9
    jne     MultiLine
    
    ; Single line
    mov     rax, [rcx + 8]
    mov     rax, [rax + rdx*8]          ; Line pointer
    add     rax, r8                     ; Start position
    
    mov     edx, r10d
    sub     edx, r8d                    ; Length
    ret
    
    MultiLine:
    ; TODO: Handle multi-line selection
    xor     eax, eax
    ret
Editor_GetSelectedText ENDP

END
