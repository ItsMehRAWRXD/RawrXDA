;======================================================================
; RawrXD IDE - Splitter Panel
; Resizable divider between project tree and editor
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
g_hSplitter             DQ ?
g_splitterX             DQ 200
g_isDraggingSplitter    DB 0

.CODE

;----------------------------------------------------------------------
; RawrXD_Splitter_Create - Create splitter
;----------------------------------------------------------------------
RawrXD_Splitter_Create PROC hParent:QWORD
    LOCAL rect:RECT
    
    ; Get parent dimensions
    INVOKE GetClientRect, hParent, ADDR rect
    
    ; Create splitter window
    INVOKE CreateWindowEx,
        0,
        "STATIC",
        NULL,
        WS_CHILD OR WS_VISIBLE,
        g_splitterX, 32, SPLITTER_WIDTH, rect.bottom - 52,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hSplitter, rax
    
    ret
    
RawrXD_Splitter_Create ENDP

;----------------------------------------------------------------------
; RawrXD_Splitter_Resize - Resize splitter and adjacent panels
;----------------------------------------------------------------------
RawrXD_Splitter_Resize PROC hParent:QWORD, x:QWORD, y:QWORD, cx:QWORD, cy:QWORD
    LOCAL toolbarHeight:QWORD
    LOCAL statusHeight:QWORD
    LOCAL remainingHeight:QWORD
    
    mov toolbarHeight, 32
    mov statusHeight, 20
    mov remainingHeight, cy
    sub remainingHeight, toolbarHeight
    sub remainingHeight, statusHeight
    
    ; Resize project tree (left)
    INVOKE MoveWindow, g_hProjectTree, 0, toolbarHeight, g_splitterX, remainingHeight, TRUE
    
    ; Resize splitter
    INVOKE MoveWindow, g_hSplitter, g_splitterX, toolbarHeight, SPLITTER_WIDTH, remainingHeight, TRUE
    
    ; Resize editor (center/right)
    mov rax, cx
    sub rax, g_splitterX
    sub rax, SPLITTER_WIDTH
    
    INVOKE MoveWindow, g_hEditorWnd, 
        g_splitterX + SPLITTER_WIDTH, toolbarHeight,
        rax, remainingHeight,
        TRUE
    
    ; Resize output panel (bottom)
    INVOKE MoveWindow, g_hOutputPanel,
        g_splitterX + SPLITTER_WIDTH, toolbarHeight + remainingHeight - 150,
        rax, 150,
        TRUE
    
    ret
    
RawrXD_Splitter_Resize ENDP

;----------------------------------------------------------------------
; RawrXD_Splitter_BeginDrag - Start drag operation
;----------------------------------------------------------------------
RawrXD_Splitter_BeginDrag PROC hParent:QWORD
    mov g_isDraggingSplitter, 1
    INVOKE SetCapture, hParent
    ret
RawrXD_Splitter_BeginDrag ENDP

;----------------------------------------------------------------------
; RawrXD_Splitter_EndDrag - End drag operation
;----------------------------------------------------------------------
RawrXD_Splitter_EndDrag PROC
    mov g_isDraggingSplitter, 0
    INVOKE ReleaseCapture
    ret
RawrXD_Splitter_EndDrag ENDP

;----------------------------------------------------------------------
; RawrXD_Splitter_UpdatePos - Update splitter position during drag
;----------------------------------------------------------------------
RawrXD_Splitter_UpdatePos PROC hParent:QWORD, newX:QWORD
    LOCAL minX:QWORD
    LOCAL maxX:QWORD
    LOCAL rect:RECT
    
    ; Clamp position (min 150, max screen - 300)
    mov minX, 150
    INVOKE GetClientRect, hParent, ADDR rect
    mov rax, rect.right
    sub rax, 300
    mov maxX, rax
    
    mov rax, newX
    cmp rax, minX
    jge @@check_max
    mov rax, minX
    
@@check_max:
    cmp rax, maxX
    jle @@set_pos
    mov rax, maxX
    
@@set_pos:
    mov g_splitterX, rax
    
    ; Update layout
    INVOKE InvalidateRect, hParent, NULL, FALSE
    
    ret
    
RawrXD_Splitter_UpdatePos ENDP

END
