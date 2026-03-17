;==========================================================================
; pane_globals_stubs.asm - Provide missing pane globals and minimal helpers
;==========================================================================
option casemap:none
include windows.inc

PUBLIC g_hInstance, g_mainWindow, g_paneList, g_paneCount, g_panes
PUBLIC szGhostClass
PUBLIC GetPaneRect, PaneSystem_RefreshLayout

.data
; Basic globals used by pane manager
g_hInstance     QWORD   0
g_mainWindow    QWORD   0
g_paneList      QWORD   0
g_paneCount     DWORD   0
szGhostClass    db      "RAWR_GHOST", 0

; Opaque pane array storage (layout defined in other modules)
g_panes         BYTE    4096 dup(0)

; Static rectangle buffer
PaneRectBuf STRUCT
    X       DWORD   10
    Y       DWORD   10
    W       DWORD   200
    H       DWORD   120
PaneRectBuf ENDS

rect_data      PaneRectBuf <>

.code

; Return address of static pane rect in rax
GetPaneRect PROC
    lea rax, rect_data
    ret
GetPaneRect ENDP

; No-op refresh layout
PaneSystem_RefreshLayout PROC
    ret
PaneSystem_RefreshLayout ENDP

END
