; ===============================================================================
; RawrXD_TextEditor_EditOps.asm
; Character editing: insert, delete, backspace with TextBuffer integration
; ===============================================================================

OPTION CASEMAP:NONE

EXTERN OutputDebugStringA:PROC
EXTERN CharUpperA:PROC
EXTERN CharLowerA:PROC

; External TextBuffer procedures
EXTERN TextBuffer_GetLength:PROC
EXTERN TextBuffer_GetChar:PROC
EXTERN TextBuffer_InsertChar:PROC
EXTERN TextBuffer_DeleteChar:PROC
EXTERN TextBuffer_GetCursorPos:PROC
EXTERN TextBuffer_SetCursorPos:PROC

; External FileIO procedures  
EXTERN FileIO_SetModified:PROC

.data
    ALIGN 16
    szEditCharInsert    db "[EDIT] Insert char: %c at pos %d", 0
    szEditCharDelete    db "[EDIT] Delete char at pos %d", 0
    szEditIndent        db "[EDIT] Indent (TAB) at pos %d", 0
    szEditNewline       db "[EDIT] Newline at pos %d", 0
    szEditBackspace     db "[EDIT] Backspace at pos %d", 0

    g_CurrentChar       db 0        ; Last character pressed
    g_EditingMode       dd 0        ; 0 = normal, 1 = selection, 2 = overwrite
    g_SelectionStart    dq 0        ; Start of selection
    g_SelectionEnd      dq 0        ; End of selection

.code

; ===============================================================================
; EditOps_InsertChar - Insert single character at cursor
; rcx = character to insert (ASCII)
; rdx = cursor position (qword)
; Returns: rax = new cursor position
; ===============================================================================
EditOps_InsertChar PROC FRAME USES rbx r12
    .endprolog
    
    mov r12, rcx                    ; r12 = char
    mov rbx, rdx                    ; rbx = cursor pos
    
    ; Validate character
    cmp r12, 0
    je InsertCharFail
    
    cmp r12, 127                    ; Printable ASCII
    jg InsertCharFail
    
    ; Call TextBuffer_InsertChar
    mov rcx, rbx                    ; Position
    mov rdx, r12                    ; Character
    call TextBuffer_InsertChar
    
    ; Mark buffer as modified
    call FileIO_SetModified
    
    ; Advance cursor
    mov rax, rbx
    inc rax
    ret
    
InsertCharFail:
    mov rax, rbx
    ret
EditOps_InsertChar ENDP

; ===============================================================================
; EditOps_DeleteChar - Delete character at cursor (forward delete)
; rcx = cursor position
; Returns: rax = new position
; ===============================================================================
EditOps_DeleteChar PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rbx, rcx                    ; rbx = cursor pos
    
    ; Call TextBuffer_DeleteChar at current position
    mov rcx, rbx
    call TextBuffer_DeleteChar
    
    ; Mark as modified
    call FileIO_SetModified
    
    ; Return same position (next char now at cursor)
    mov rax, rbx
    
    add rsp, 32
    pop rbx
    pop rbp
    ret
EditOps_DeleteChar ENDP

; ===============================================================================
; EditOps_Backspace - Delete character before cursor
; rcx = cursor position
; Returns: rax = new position (one back)
; ===============================================================================
EditOps_Backspace PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rbx, rcx                    ; rbx = cursor pos
    
    ; Can't backspace at start
    cmp rbx, 0
    je BackspaceFail
    
    ; Delete character before cursor
    mov rcx, rbx
    dec rcx                         ; Position of character to delete
    call TextBuffer_DeleteChar
    
    ; Mark as modified
    call FileIO_SetModified
    
    ; Return position moved back one
    mov rax, rbx
    dec rax
    
    add rsp, 32
    pop rbx
    pop rbp
    ret
    
BackspaceFail:
    mov rax, rbx
    add rsp, 32
    pop rbx
    pop rbp
    ret
EditOps_Backspace ENDP

; ===============================================================================
; EditOps_HandleTab - Insert indentation
; rcx = cursor position
; rdx = indentation width (usually 4)
; Returns: rax = new cursor position
; ===============================================================================
EditOps_HandleTab PROC FRAME USES rbx r12 r13
    .endprolog
    
    mov rbx, rcx                    ; rbx = cursor pos
    mov r13d, edx                   ; r13d = indent width
    
    ; Find start of line (scan backward for newline)
    mov r12, rbx
    call TextBuffer_GetCursorPos
    
    ; Get character at position
    mov rcx, rbx
    call TextBuffer_GetChar         ; al = char
    
    ; If already at indent level, insert spaces to next multiple
    mov r8d, 0                      ; Space counter
    
InsertSpaces:
    cmp r8d, r13d
    jge TabComplete
    
    ; Insert space
    mov rcx, rbx
    add rcx, r8                     ; Position offset
    mov rdx, 32                     ; Space character
    call TextBuffer_InsertChar
    
    inc r8d
    jmp InsertSpaces
    
TabComplete:
    ; Mark modified
    call FileIO_SetModified
    
    ; Return new position
    mov rax, rbx
    add rax, r13
    ret
EditOps_HandleTab ENDP

; ===============================================================================
; EditOps_HandleNewline - Insert line break
; rcx = cursor position
; Returns: rax = new cursor position (start of next line)
; ===============================================================================
EditOps_HandleNewline PROC FRAME USES rbx
    .endprolog
    
    mov rbx, rcx                    ; rbx = cursor pos
    
    ; Insert newline character (0x0A)
    mov rcx, rbx
    mov rdx, 0x0A                   ; LF
    call TextBuffer_InsertChar
    
    ; Mark as modified
    call FileIO_SetModified
    
    ; Return cursor at start of new line
    mov rax, rbx
    inc rax
    ret
EditOps_HandleNewline ENDP

; ===============================================================================
; EditOps_SelectRange - Select text range
; rcx = start position
; rdx = end position
; ===============================================================================
EditOps_SelectRange PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov [g_SelectionStart], rcx
    mov [g_SelectionEnd], rdx
    
    add rsp, 32
    pop rbp
    ret
EditOps_SelectRange ENDP

; ===============================================================================
; EditOps_GetSelectionRange - Get current selection
; Returns: rax = start, rdx = end
; ===============================================================================
EditOps_GetSelectionRange PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rax, [g_SelectionStart]
    mov rdx, [g_SelectionEnd]
    
    add rsp, 32
    pop rbp
    ret
EditOps_GetSelectionRange ENDP

; ===============================================================================
; EditOps_DeleteSelection - Delete selected text
; Returns: rax = new cursor position
; ===============================================================================
EditOps_DeleteSelection PROC FRAME USES rbx r12
    .endprolog
    
    mov rbx, [g_SelectionStart]
    mov r12, [g_SelectionEnd]
    
    cmp rbx, r12
    jge DelSelFail                  ; No selection
    
    ; Delete range
    mov rcx, rbx                    ; Start position
    mov rdx, r12
    sub rdx, rbx                    ; Length
    
DeleteSelLoop:
    test rdx, rdx
    jz DelSelDone
    
    mov rcx, rbx
    call TextBuffer_DeleteChar
    
    dec rdx
    jmp DeleteSelLoop
    
DelSelDone:
    ; Mark modified
    call FileIO_SetModified
    
    ; Return cursor at where selection started
    mov rax, rbx
    mov qword ptr [g_SelectionStart], 0
    mov qword ptr [g_SelectionEnd], 0
    ret
    
DelSelFail:
    xor eax, eax
    ret
EditOps_DeleteSelection ENDP

; ===============================================================================
; EditOps_SetEditMode - Set editing mode (normal/selection/overwrite)
; rcx = mode (0=normal, 1=selection, 2=overwrite)
; ===============================================================================
EditOps_SetEditMode PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov [g_EditingMode], ecx
    
    add rsp, 32
    pop rbp
    ret
EditOps_SetEditMode ENDP

; ===============================================================================
; EditOps_GetEditMode - Get current editing mode
; Returns: rax = mode
; ===============================================================================
EditOps_GetEditMode PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov eax, [g_EditingMode]
    
    add rsp, 32
    pop rbp
    ret
EditOps_GetEditMode ENDP

PUBLIC EditOps_InsertChar
PUBLIC EditOps_DeleteChar
PUBLIC EditOps_Backspace
PUBLIC EditOps_HandleTab
PUBLIC EditOps_HandleNewline
PUBLIC EditOps_SelectRange
PUBLIC EditOps_GetSelectionRange
PUBLIC EditOps_DeleteSelection
PUBLIC EditOps_SetEditMode
PUBLIC EditOps_GetEditMode
PUBLIC g_EditingMode
PUBLIC g_SelectionStart
PUBLIC g_SelectionEnd

END
