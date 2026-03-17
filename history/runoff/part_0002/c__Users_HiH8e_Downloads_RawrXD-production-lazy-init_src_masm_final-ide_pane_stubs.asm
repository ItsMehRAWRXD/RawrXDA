;==========================================================================
; pane_stubs.asm - Minimal stubs for pane system symbols
;==========================================================================
option casemap:none

PUBLIC ui_create_editor, ui_create_terminal, ui_create_chat
PUBLIC DragPane_Init

.code

;==========================================================================
; Stub implementations return non-zero (success)
;==========================================================================

ui_create_editor PROC
    mov rax, 1
    ret
ui_create_editor ENDP

ui_create_terminal PROC
    mov rax, 1
    ret
ui_create_terminal ENDP

ui_create_chat PROC
    mov rax, 1
    ret
ui_create_chat ENDP

DragPane_Init PROC
    mov eax, 1
    ret
DragPane_Init ENDP

END
