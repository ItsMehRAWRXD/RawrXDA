; ============================================================================
; QT-LIKE PANE SYSTEM - ADVANCED LAYOUT AND PERSISTENCE
; Docking layouts, serialization, and dynamic layout management
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

.data

; Layout modes
LAYOUT_SINGLE_WINDOW    equ 1
LAYOUT_LEFT_RIGHT_SPLIT equ 2
LAYOUT_TOP_BOTTOM_SPLIT equ 3
LAYOUT_CUSTOM_GRID      equ 4
LAYOUT_TABBED           equ 5

; Splitter structure (for dynamic splitting)
SPLITTER struct
    dwPosition          dd ?    ; X or Y position depending on orientation
    bVertical           dd ?    ; 1 = vertical splitter, 0 = horizontal
    dwLeftPaneID        dd ?    ; Left/top pane ID
    dwRightPaneID       dd ?    ; Right/bottom pane ID
SPLITTER ends

; Layout state
.data
    g_LayoutMode            dd LAYOUT_CUSTOM_GRID
    g_Splitters             dd 50 dup(0)        ; Support 50 splitters
    g_SplitterCount         dd 0
    g_LayoutConfigBuffer    dd ?                ; Pointer to serialized layout
    g_LayoutConfigSize      dd 0

.code

; ============================================================================
; LAYOUT MANAGEMENT
; ============================================================================

; LayoutManager_CreateSplitter - Create a new splitter
; Parameters:
;   esp+8:  dwPosition (pixel position)
;   esp+12: bVertical (1=vertical, 0=horizontal)
;   esp+16: dwLeftPaneID (or top pane for horizontal)
;   esp+20: dwRightPaneID (or bottom pane for horizontal)
; Returns: Splitter handle (or 0 on failure)
public LayoutManager_CreateSplitter
LayoutManager_CreateSplitter proc dwPos:DWORD, bVert:DWORD, dwLeftID:DWORD, dwRightID:DWORD
    LOCAL splitterID:DWORD
    LOCAL pSplitter:DWORD
    
    cmp g_SplitterCount, 50
    jge @SplitFailed
    
    mov eax, g_SplitterCount
    mov splitterID, eax
    inc g_SplitterCount
    
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, sizeof SPLITTER
    test eax, eax
    jz @SplitFailed
    mov pSplitter, eax
    
    mov ecx, pSplitter
    
    mov eax, dwPos
    mov [ecx].SPLITTER.dwPosition, eax
    
    mov eax, bVert
    mov [ecx].SPLITTER.bVertical, eax
    
    mov eax, dwLeftID
    mov [ecx].SPLITTER.dwLeftPaneID, eax
    
    mov eax, dwRightID
    mov [ecx].SPLITTER.dwRightPaneID, eax
    
    mov eax, g_SplitterCount
    dec eax
    mov [g_Splitters + eax*4], pSplitter
    
    mov eax, splitterID
    ret
    
@SplitFailed:
    xor eax, eax
    ret
LayoutManager_CreateSplitter endp

; LayoutManager_ResizeSplitter - Resize a splitter
; Parameters:
;   esp+8:  splitterID
;   esp+12: dwNewPosition
public LayoutManager_ResizeSplitter
LayoutManager_ResizeSplitter proc splitterID:DWORD, dwNewPos:DWORD
    LOCAL pSplitter:DWORD
    
    mov eax, splitterID
    cmp eax, g_SplitterCount
    jge @ResizeFailed
    
    mov eax, [g_Splitters + eax*4]
    test eax, eax
    jz @ResizeFailed
    mov pSplitter, eax
    
    mov eax, dwNewPos
    mov ecx, pSplitter
    mov [ecx].SPLITTER.dwPosition, eax
    
    mov eax, TRUE
    ret
    
@ResizeFailed:
    xor eax, eax
    ret
LayoutManager_ResizeSplitter endp

; ============================================================================
; LAYOUT CALCULATION
; ============================================================================

; LayoutManager_CalculateSplitLayout - Calculate pane positions for split layout
; Parameters:
;   esp+8:  dwClientWidth
;   esp+12: dwClientHeight
;   esp+16: dwSplitRatio (0-100, percentage)
;   esp+20: bVertical (1=vertical split, 0=horizontal)
; Returns: Position structure in eax (use carefully - stack-based)
public LayoutManager_CalculateSplitLayout
LayoutManager_CalculateSplitLayout proc w:DWORD, h:DWORD, ratio:DWORD, bVert:DWORD
    LOCAL leftW:DWORD
    LOCAL leftH:DWORD
    LOCAL rightW:DWORD
    LOCAL rightH:DWORD
    
    cmp bVert, 0
    jne @VerticalSplit
    
    ; Horizontal split (left-right)
    mov eax, w
    imul eax, ratio
    xor edx, edx
    mov ecx, 100
    div ecx
    mov leftW, eax
    
    mov eax, w
    sub eax, leftW
    mov rightW, eax
    
    mov leftH, h
    mov rightH, h
    
    jmp @SplitCalcDone
    
@VerticalSplit:
    ; Vertical split (top-bottom)
    mov leftW, w
    mov rightW, w
    
    mov eax, h
    imul eax, ratio
    xor edx, edx
    mov ecx, 100
    div ecx
    mov leftH, eax
    
    mov eax, h
    sub eax, leftH
    mov rightH, eax
    
@SplitCalcDone:
    ; Return values in eax:edx:ecx:esi
    mov eax, leftW
    mov edx, leftH
    mov ecx, rightW
    mov esi, rightH
    ret
LayoutManager_CalculateSplitLayout endp

; ============================================================================
; LAYOUT SERIALIZATION
; ============================================================================

; LayoutManager_SaveLayout - Save current layout to memory
; Returns: Pointer to serialized layout buffer
public LayoutManager_SaveLayout
LayoutManager_SaveLayout proc
    LOCAL pBuffer:DWORD
    LOCAL bufferSize:DWORD
    LOCAL i:DWORD
    
    ; Calculate buffer size needed
    ; (4 bytes per pane * 100) + (4 bytes splitters count) + (16 bytes per splitter * 50)
    mov bufferSize, 400      ; 100 panes * 4 bytes
    add bufferSize, 4        ; splitter count
    add bufferSize, 800      ; 50 splitters * 16 bytes
    
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, bufferSize
    test eax, eax
    jz @SaveFailed
    mov pBuffer, eax
    
    ; For simplicity, just store pane count at start
    mov ecx, pBuffer
    invoke PaneManager_GetPaneCount
    mov [ecx], eax
    add ecx, 4
    
    ; Store each pane's info (just ID for now - can be expanded)
    mov i, 0
@@SaveLoop:
    mov eax, i
    invoke PaneManager_GetPaneCount
    cmp eax, eax      ; Check against pane count
    jge @@SaveDone
    
    ; Simplified - just store pane ID
    mov [ecx], eax
    add ecx, 4
    inc i
    jmp @@SaveLoop
    
@@SaveDone:
    mov g_LayoutConfigBuffer, pBuffer
    mov g_LayoutConfigSize, bufferSize
    mov eax, pBuffer
    ret
    
@SaveFailed:
    xor eax, eax
    ret
LayoutManager_SaveLayout endp

; LayoutManager_LoadLayout - Load layout from memory
; Parameters:
;   esp+8:  pBuffer (pointer to serialized layout)
public LayoutManager_LoadLayout
LayoutManager_LoadLayout proc pBuffer:DWORD
    LOCAL paneCount:DWORD
    LOCAL i:DWORD
    
    test pBuffer, pBuffer
    jz @LoadFailed
    
    ; Read pane count
    mov eax, [pBuffer]
    mov paneCount, eax
    
    ; For now, just verify it looks reasonable
    cmp paneCount, 0
    je @LoadFailed
    cmp paneCount, 100
    jge @LoadFailed
    
    mov eax, TRUE
    ret
    
@LoadFailed:
    xor eax, eax
    ret
LayoutManager_LoadLayout endp

; LayoutManager_ExportLayoutJSON - Export layout as JSON string
; Parameters:
;   esp+8:  pBuffer (output buffer)
;   esp+12: dwBufferSize
; Returns: Number of bytes written
public LayoutManager_ExportLayoutJSON
LayoutManager_ExportLayoutJSON proc pBuffer:DWORD, dwSize:DWORD
    LOCAL offset:DWORD
    LOCAL i:DWORD
    LOCAL pPaneInfo:DWORD
    
    mov offset, 0
    mov ecx, pBuffer
    
    ; Write JSON header
    cmp offset, dwSize
    jge @JSONDone
    
    ; "{"
    mov byte ptr [ecx], '{'
    inc ecx
    inc offset
    mov byte ptr [ecx], 13
    inc ecx
    inc offset
    mov byte ptr [ecx], 10
    inc ecx
    inc offset
    
    ; Write panes array
    mov byte ptr [ecx], '"'
    inc ecx
    inc offset
    mov byte ptr [ecx], 'p'
    inc ecx
    inc offset
    mov byte ptr [ecx], 'a'
    inc ecx
    inc offset
    mov byte ptr [ecx], 'n'
    inc ecx
    inc offset
    mov byte ptr [ecx], 'e'
    inc ecx
    inc offset
    mov byte ptr [ecx], 's'
    inc ecx
    inc offset
    mov byte ptr [ecx], '"'
    inc ecx
    inc offset
    mov byte ptr [ecx], ':'
    inc ecx
    inc offset
    mov byte ptr [ecx], '['
    inc ecx
    inc offset
    
    ; JSON footer
    mov byte ptr [ecx], ']'
    inc ecx
    inc offset
    mov byte ptr [ecx], 13
    inc ecx
    inc offset
    mov byte ptr [ecx], 10
    inc ecx
    inc offset
    mov byte ptr [ecx], '}'
    inc ecx
    inc offset
    mov byte ptr [ecx], 0
    
@JSONDone:
    mov eax, offset
    ret
LayoutManager_ExportLayoutJSON endp

; ============================================================================
; ADVANCED LAYOUT ALGORITHMS
; ============================================================================

; LayoutManager_TiledLayout - Auto-arrange panes in grid
; Parameters:
;   esp+8:  dwClientWidth
;   esp+12: dwClientHeight
;   esp+16: dwColumns (how many columns)
public LayoutManager_TiledLayout
LayoutManager_TiledLayout proc w:DWORD, h:DWORD, cols:DWORD
    LOCAL paneCount:DWORD
    LOCAL tileW:DWORD
    LOCAL tileH:DWORD
    LOCAL rows:DWORD
    LOCAL i:DWORD
    LOCAL col:DWORD
    LOCAL row:DWORD
    LOCAL x:DWORD
    LOCAL y:DWORD
    LOCAL paneID:DWORD
    
    invoke PaneManager_GetPaneCount
    mov paneCount, eax
    
    ; Calculate rows needed
    mov eax, paneCount
    add eax, cols
    dec eax
    xor edx, edx
    mov ecx, cols
    div ecx
    mov rows, eax
    
    ; Calculate tile size
    mov eax, w
    xor edx, edx
    mov ecx, cols
    div ecx
    mov tileW, eax
    
    mov eax, h
    xor edx, edx
    mov ecx, rows
    div ecx
    mov tileH, eax
    
    ; Arrange panes
    mov i, 0
    mov col, 0
    mov row, 0
    
@@ArrangeLoop:
    mov eax, i
    cmp eax, paneCount
    jge @ArrangeDone
    
    ; Calculate position
    mov eax, col
    imul eax, tileW
    mov x, eax
    
    mov eax, row
    imul eax, tileH
    mov y, eax
    
    ; Move to next column
    mov eax, col
    inc eax
    mov col, eax
    
    cmp col, cols
    jl @@SameRow
    
    ; Wrap to next row
    mov col, 0
    inc row
    
@@SameRow:
    ; TODO: Get pane at index i and reposition it
    ; invoke PaneManager_SetPanePosition, paneID, x, y, tileW, tileH
    
    inc i
    jmp @@ArrangeLoop
    
@ArrangeDone:
    mov eax, TRUE
    ret
LayoutManager_TiledLayout endp

; LayoutManager_FocusedLayout - Make one pane large in center, others small on sides
; Parameters:
;   esp+8:  dwClientWidth
;   esp+12: dwClientHeight
;   esp+16: dwFocusedPaneID
public LayoutManager_FocusedLayout
LayoutManager_FocusedLayout proc w:DWORD, h:DWORD, focusPaneID:DWORD
    ; Large central pane takes 70% of space
    ; Side panels (30%) divided among remaining panes
    
    ; This is a simplified version - can be expanded
    
    mov eax, TRUE
    ret
LayoutManager_FocusedLayout endp

; ============================================================================
; THEME APPLICATION
; ============================================================================

; ThemeManager_ApplyDarkTheme - Apply dark theme to all panes
public ThemeManager_ApplyDarkTheme
ThemeManager_ApplyDarkTheme proc
    invoke PaneManager_SetThemeColor, 0, 001E1E1Eh    ; Background
    invoke PaneManager_SetThemeColor, 1, 00505050h    ; Border
    invoke PaneManager_SetThemeColor, 2, 0007ACCh     ; Accent
    invoke PaneManager_SetThemeColor, 3, 00E0E0E0h    ; Text
    invoke PaneManager_SetThemeColor, 4, 00264F78h    ; Selection
    invoke PaneManager_SetThemeColor, 5, 003E3E42h    ; Hover
    
    mov eax, TRUE
    ret
ThemeManager_ApplyDarkTheme endp

; ThemeManager_ApplyLightTheme - Apply light theme to all panes
public ThemeManager_ApplyLightTheme
ThemeManager_ApplyLightTheme proc
    invoke PaneManager_SetThemeColor, 0, 00FFFFFFh    ; Background (white)
    invoke PaneManager_SetThemeColor, 1, 00D0D0D0h    ; Border (light gray)
    invoke PaneManager_SetThemeColor, 2, 00007ACCh    ; Accent (blue)
    invoke PaneManager_SetThemeColor, 3, 00000000h    ; Text (black)
    invoke PaneManager_SetThemeColor, 4, 00B0D7F8h    ; Selection (light blue)
    invoke PaneManager_SetThemeColor, 5, 00E0E0E0h    ; Hover (light gray)
    
    mov eax, TRUE
    ret
ThemeManager_ApplyLightTheme endp

end
