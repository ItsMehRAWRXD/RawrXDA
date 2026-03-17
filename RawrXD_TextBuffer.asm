; ============================================================================
; RawrXD_TextBuffer.asm - Text Buffer Management Module
; ============================================================================
; High-performance text buffer with:
; - Dynamic allocation and reallocation
; - Line/column indexing
; - Cursor position tracking
; - Efficient insert/delete/replace operations
; ============================================================================

.CODE

; Buffer structure (stored at known address, e.g., rax = buffer ptr)
; Offset 0:  qword - buffer data pointer
; Offset 8:  qword - buffer capacity (bytes)
; Offset 16: qword - buffer length (bytes used)
; Offset 24: qword - line count
; Offset 32: 256*qword - line start offsets (line offset table, idx 0-255)

BUFFER_HEADER_SIZE = 32 + (256 * 8)  ; 2080 bytes for header
INITIAL_BUFFER_SIZE = 65536          ; 64 KB initial text capacity
LINE_OFFSET_TABLE_SIZE = 256          ; Support up to 256 lines


; ============================================================================
; TextBuffer_Initialize(rcx = buffer_ptr)
; 
; Initialize text buffer structure:
; - Allocate initial data buffer
; - Set capacity and length to 0
; - Initialize line offset table
; ============================================================================
TextBuffer_Initialize PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 32
    .ENDPROLOG

    push rbx

    mov rbx, rcx                       ; rbx = buffer_ptr
    
    ; Allocate initial text buffer via VirtualAlloc
    mov rcx, 0                         ; lpAddress = NULL (let OS choose)
    mov rdx, INITIAL_BUFFER_SIZE       ; dwSize = 64 KB
    mov r8, 4096                       ; MEM_COMMIT = 0x1000
    mov r9, 4                          ; PAGE_READWRITE = 0x4
    sub rsp, 32
    call VirtualAlloc
    add rsp, 32
    
    test rax, rax
    jz .InitFail
    
    ; Store data pointer at offset 0
    mov [rbx], rax                     ; buffer->data = allocated_ptr
    
    ; Store capacity at offset 8
    mov qword [rbx + 8], INITIAL_BUFFER_SIZE
    
    ; Store length at offset 16 (starts empty)
    mov qword [rbx + 16], 0
    
    ; Store line count at offset 24 (1 line, even if empty)
    mov qword [rbx + 24], 1
    
    ; Initialize line offset table (offset 32+)
    xor rcx, rcx
    mov qword [rbx + 32], 0             ; Line 0 starts at offset 0
    
    mov rcx, 1
.InitLineTable:
    cmp rcx, LINE_OFFSET_TABLE_SIZE
    jge .InitDone
    mov qword [rbx + 32 + rcx*8], 0
    inc rcx
    jmp .InitLineTable

.InitDone:
    mov rax, 1                         ; Return success
    pop rbx
    ret

.InitFail:
    xor rax, rax                       ; Return failure
    pop rbx
    ret
TextBuffer_Initialize ENDP


; ============================================================================
; TextBuffer_InsertChar(rcx = buffer_ptr, rdx = position, r8b = character)
;
; Insert single character at absolute byte position
; - Grows buffer if needed
; - Updates line offset table for lines >= insertion_line
; ============================================================================
TextBuffer_InsertChar PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .PUSHREG r13
    .ALLOCSTACK 32
    .ENDPROLOG

    push rbx
    push r12
    push r13

    mov rbx, rcx                       ; rbx = buffer_ptr
    mov r12, rdx                       ; r12 = insertion position (byte offset)
    mov r13b, r8b                      ; r13b = character to insert

    ; Check if buffer needs growth
    mov rcx, [rbx + 16]                ; rcx = current length
    mov rdx, [rbx + 8]                 ; rdx = capacity
    
    cmp rcx, rdx
    jl .NoGrowth
    
    ; Need to grow buffer - double capacity
    call TextBuffer_Grow
    test rax, rax
    jz .InsertFail

.NoGrowth:
    mov rax, [rbx]                     ; rax = data pointer
    mov rcx, [rbx + 16]                ; rcx = current length
    
    ; Shift bytes to the right to make room
    mov rdx, rcx                       ; rdx = length
.ShiftLoop:
    cmp rdx, r12
    jle .ShiftDone
    mov r8, [rax + rdx - 1]            ; load byte from source
    mov [rax + rdx], r8                ; store at destination (one byte right)
    dec rdx
    jmp .ShiftLoop

.ShiftDone:
    ; Insert the character
    mov [rax + r12], r13b              ; Insert character at position
    
    ; Increment length
    add qword [rbx + 16], 1
    
    ; Update line table if character is newline
    cmp r13b, 10                       ; 10 = LF ('\n')
    jne .UpdateDone
    
    ; Find which line this newline is on and update all subsequent lines
    call TextBuffer_UpdateLineTable
    
.UpdateDone:
    mov rax, 1                         ; Return success
    pop r13
    pop r12
    pop rbx
    ret

.InsertFail:
    xor rax, rax                       ; Return failure
    pop r13
    pop r12
    pop rbx
    ret
TextBuffer_InsertChar ENDP


; ============================================================================
; TextBuffer_DeleteChar(rcx = buffer_ptr, rdx = position)
;
; Delete character at absolute byte position
; - Shifts remaining text left
; - Updates line offset table if newline deleted
; ============================================================================
TextBuffer_DeleteChar PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 32
    .ENDPROLOG

    push rbx

    mov rbx, rcx                       ; rbx = buffer_ptr
    mov rcx, [rbx + 16]                ; rcx = current length
    
    cmp rdx, rcx
    jge .DeleteFail                    ; Position out of bounds
    
    mov rax, [rbx]                     ; rax = data pointer
    
    ; Check if we're deleting a newline
    mov r8b, [rax + rdx]               ; r8b = character at position
    
    ; Shift bytes left
    mov rcx, rdx
.ShiftLoop:
    mov r8, [rax + rcx + 1]            ; Load next byte
    mov [rax + rcx], r8                ; Store current position
    inc rcx
    cmp rcx, [rbx + 16]
    jl .ShiftLoop
    
    ; Decrement length
    sub qword [rbx + 16], 1
    
    ; Update line table if newline deleted
    cmp [rax + rdx], 10                ; Was it a newline?
    jne .DeleteDone
    
    call TextBuffer_UpdateLineTable

.DeleteDone:
    mov rax, 1                         ; Return success
    pop rbx
    ret

.DeleteFail:
    xor rax, rax
    pop rbx
    ret
TextBuffer_DeleteChar ENDP


; ============================================================================
; TextBuffer_InsertString(rcx = buffer_ptr, rdx = position, r8 = string_ptr, r9 = length)
;
; Insert string of bytes at position
; ============================================================================
TextBuffer_InsertString PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .PUSHREG r13
    .PUSHREG r14
    .ALLOCSTACK 32
    .ENDPROLOG

    push rbx
    push r12
    push r13
    push r14

    mov rbx, rcx                       ; rbx = buffer_ptr
    mov r12, rdx                       ; r12 = position
    mov r13, r8                        ; r13 = string pointer
    mov r14, r9                        ; r14 = length

    ; Check buffer space
    mov rcx, [rbx + 16]                ; current length
    mov rdx, [rbx + 8]                 ; capacity
    add rcx, r14                       ; needed length
    
    cmp rcx, rdx
    jle .NoGrowthStr
    
    ; Grow buffer
    call TextBuffer_Grow
    test rax, rax
    jz .InsertStrFail

.NoGrowthStr:
    mov rax, [rbx]                     ; rax = data pointer
    mov rcx, [rbx + 16]                ; rcx = current length
    
    ; Shift existing data right by r14 bytes
    mov rdx, rcx
.ShiftStrLoop:
    cmp rdx, r12
    jle .ShiftStrDone
    mov r8, [rax + rdx - 1]
    mov [rax + rdx + r14 - 1], r8
    dec rdx
    jmp .ShiftStrLoop

.ShiftStrDone:
    ; Copy string into buffer
    xor rcx, rcx
.CopyLoop:
    cmp rcx, r14
    jge .CopyDone
    mov r8b, [r13 + rcx]               ; Load byte from source
    mov [rax + r12 + rcx], r8b         ; Store in buffer
    inc rcx
    jmp .CopyLoop

.CopyDone:
    ; Update length
    add [rbx + 16], r14
    
    ; Update line table
    call TextBuffer_UpdateLineTable
    
    mov rax, 1
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

.InsertStrFail:
    xor rax, rax
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
TextBuffer_InsertString ENDP


; ============================================================================
; TextBuffer_GetLineColumn(rcx = buffer_ptr, rdx = byte_offset)
;
; Convert byte offset to (line, column)
; Returns: rax = line number, rdx = column within line
; ============================================================================
TextBuffer_GetLineColumn PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    ; rcx = buffer_ptr, rdx = byte_offset
    mov rax, [rcx]                     ; rax = data pointer
    xor r8, r8                         ; r8 = current line
    xor r9, r9                         ; r9 = current offset in current line
    xor r10, r10                       ; r10 = position counter
    
.ScanLoop:
    cmp r10, rdx
    jge .ScanDone
    
    mov r11b, [rax + r10]              ; r11b = current character
    
    cmp r11b, 10                       ; Is it newline?
    jne .NotNewline
    
    inc r8                             ; Increment line
    xor r9, r9                         ; Reset column to 0
    jmp .NextChar

.NotNewline:
    inc r9                             ; Increment column

.NextChar:
    inc r10
    jmp .ScanLoop

.ScanDone:
    ; rax = line, rdx = column
    mov rax, r8
    mov rdx, r9
    ret
TextBuffer_GetLineColumn ENDP


; ============================================================================
; TextBuffer_GetOffsetFromLineColumn(rcx = buffer_ptr, rdx = line, r8 = column)
;
; Convert (line, column) to byte offset
; Returns: rax = byte offset (or -1 if out of bounds)
; ============================================================================
TextBuffer_GetOffsetFromLineColumn PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    ; rcx = buffer_ptr, rdx = line, r8 = column
    mov rax, [rcx]                     ; rax = data pointer
    mov r10, [rcx + 16]                ; r10 = buffer length
    xor r9, r9                         ; r9 = current line
    xor r11, r11                       ; r11 = offset counter
    xor r12, r12                       ; r12 = column counter

.ScanLC:
    cmp r11, r10
    jge .EndReached
    
    cmp r9, rdx
    jne .AdvanceLC
    
    ; We're on the target line
    cmp r12, r8
    je .FoundLC
    
.AdvanceLC:
    mov r13b, [rax + r11]
    cmp r13b, 10
    jne .NotNLLC
    inc r9
    xor r12, r12
    jmp .NextCharLC

.NotNLLC:
    inc r12

.NextCharLC:
    inc r11
    jmp .ScanLC

.FoundLC:
    mov rax, r11                       ; Return offset
    ret

.EndReached:
    mov rax, -1                        ; Return error
    ret
TextBuffer_GetOffsetFromLineColumn ENDP


; ============================================================================
; TextBuffer_GetLine(rcx = buffer_ptr, rdx = line_number, r8 = dest_ptr, r9 = dest_size)
;
; Extract entire line content (without newline)
; Returns: rax = bytes copied
; ============================================================================
TextBuffer_GetLine PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, [rcx]                     ; rax = data pointer
    mov r10, [rcx + 16]                ; r10 = buffer length
    xor r11, r11                       ; r11 = current line
    xor r12, r12                       ; r12 = offset counter
    xor r13, r13                       ; r13 = bytes copied
    xor r14, r14                       ; r14 = in_target_line

.ScanGetLine:
    cmp r12, r10
    jge .EndGetLine
    
    mov r15b, [rax + r12]
    
    cmp r11, rdx
    je .InTargetLine
    
    cmp r15b, 10
    jne .SkipGetLine
    inc r11
    jmp .SkipGetLine

.InTargetLine:
    cmp r15b, 10
    je .EndGetLine
    
    cmp r13, r9
    jge .EndGetLine
    
    mov [r8 + r13], r15b
    inc r13

.SkipGetLine:
    inc r12
    jmp .ScanGetLine

.EndGetLine:
    mov rax, r13
    ret
TextBuffer_GetLine ENDP


; ============================================================================
; TextBuffer_Grow (internal)
;
; Double buffer capacity via VirtualAlloc and memcpy
; Returns: rax = success (1) or failure (0)
; ============================================================================
TextBuffer_Grow PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 32
    .ENDPROLOG

    push rbx

    ; rbx still contains buffer_ptr from caller
    
    mov rcx, [rbx + 8]                 ; rcx = current capacity
    shl rcx, 1                         ; Double it
    
    ; Allocate new buffer
    mov rdx, 0                         ; lpAddress
    mov r8, rcx                        ; dwSize
    mov r9, 4096                       ; MEM_COMMIT
    sub rsp, 32
    push rcx                           ; Save new capacity
    mov [rsp + 32], r9                 ; PAGE_READWRITE on stack for call
    call VirtualAlloc
    add rsp, 40
    pop rcx
    
    test rax, rax
    jz .GrowFail
    
    ; Copy old data to new buffer
    mov r8, [rbx]                      ; r8 = old data pointer
    mov r9, [rbx + 16]                 ; r9 = length to copy
    
    xor r10, r10
.CopyGrowLoop:
    cmp r10, r9
    jge .CopyGrowDone
    mov r11b, [r8 + r10]
    mov [rax + r10], r11b
    inc r10
    jmp .CopyGrowLoop

.CopyGrowDone:
    ; Free old buffer
    mov rdx, [rbx]                     ; rdx = old pointer
    mov r8, 16384                      ; MEM_DECOMMIT
    sub rsp, 32
    mov rcx, rdx
    call VirtualFree
    add rsp, 32
    
    ; Update buffer header
    mov [rbx], rax                     ; Store new pointer
    mov [rbx + 8], rcx                 ; Store new capacity
    
    mov rax, 1
    pop rbx
    ret

.GrowFail:
    xor rax, rax
    pop rbx
    ret
TextBuffer_Grow ENDP


; ============================================================================
; TextBuffer_UpdateLineTable (internal)
;
; Recalculate line offset table
; Called after insertions/deletions affecting line structure
; ============================================================================
TextBuffer_UpdateLineTable PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    ; rbx = buffer_ptr
    mov rax, [rbx]                     ; rax = data pointer
    mov rcx, [rbx + 16]                ; rcx = buffer length
    xor rdx, rdx                       ; rdx = line index
    xor r8, r8                         ; r8 = byte offset
    
    ; Line 0 always starts at 0
    mov qword [rbx + 32], 0
    inc rdx

.TableScanLoop:
    cmp r8, rcx
    jge .TableScanDone
    
    cmp rdx, LINE_OFFSET_TABLE_SIZE
    jge .TableScanDone
    
    mov r9b, [rax + r8]
    cmp r9b, 10
    jne .NotNewlineTable
    
    ; Found newline, next line starts at r8+1
    mov qword [rbx + 32 + rdx*8], r8 + 1
    inc rdx

.NotNewlineTable:
    inc r8
    jmp .TableScanLoop

.TableScanDone:
    ; Update line count
    mov [rbx + 24], rdx
    ret
TextBuffer_UpdateLineTable ENDP

END
