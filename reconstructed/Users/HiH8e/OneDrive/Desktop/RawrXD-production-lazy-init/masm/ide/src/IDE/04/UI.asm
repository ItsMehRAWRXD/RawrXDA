; ============================================================================
; IDE_04_UI.asm - UI subsystem for GGUF loader and pane manager
; ============================================================================
include IDE_INC.ASM

; External from ui_system_complete.asm
EXTERN UIGguf_UpdateLoadingProgress:PROC
EXTERN PaneManager_CreatePane:PROC
EXTERN PaneManager_RenderAllPanes:PROC

PUBLIC IDEUI_UpdateLoadingProgress
PUBLIC IDEUI_CreatePane
PUBLIC IDEUI_RenderAllPanes

.code

IDEUI_UpdateLoadingProgress PROC dwPct:DWORD
    push dwPct
    call UIGguf_UpdateLoadingProgress
    ret
IDEUI_UpdateLoadingProgress ENDP

IDEUI_CreatePane PROC pszTitle:DWORD, dwType:DWORD
    push dwType
    push pszTitle
    call PaneManager_CreatePane
    ret
IDEUI_CreatePane ENDP

IDEUI_RenderAllPanes PROC
    call PaneManager_RenderAllPanes
    ret
IDEUI_RenderAllPanes ENDP

END
