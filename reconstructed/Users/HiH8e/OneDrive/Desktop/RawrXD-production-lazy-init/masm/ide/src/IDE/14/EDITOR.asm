; ============================================================================
; IDE_14_EDITOR.asm - Editor core subsystem wrapper
; ============================================================================
include IDE_INC.ASM

; External from editor_impl.asm
EXTERN Editor_Init:PROC
EXTERN Editor_WndProc:PROC

PUBLIC IDEEditor_Initialize
PUBLIC IDEEditor_GetWndProc

.code

IDEEditor_Initialize PROC
    call Editor_Init
    LOG CSTR("IDEEditor_Initialize"), eax
    ret
IDEEditor_Initialize ENDP

IDEEditor_GetWndProc PROC
    mov eax, offset Editor_WndProc
    ret
IDEEditor_GetWndProc ENDP

END
