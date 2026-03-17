; ============================================================================
; QT PANE SYSTEM INTEGRATION FOR RAWRXD IDE
; Integrates the complete pane system into the IDE and provides helper functions
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; External declarations
extern PaneManager_Initialize:proc
extern PaneManager_CreatePane:proc
extern PaneManager_DeletePane:proc
extern PaneManager_SetPanePosition:proc
extern PaneManager_SetZOrder:proc
extern PaneManager_GetPaneAtCoords:proc
extern PaneManager_RenderAllPanes:proc
extern PaneManager_HandleMouseDown:proc
extern PaneManager_HandleMouseMove:proc
extern PaneManager_HandleMouseUp:proc
extern PaneManager_SetThemeColor:proc
extern LayoutManager_CreateSplitter:proc
extern LayoutManager_ResizeSplitter:proc
extern LayoutManager_SaveLayout:proc
extern LayoutManager_LoadLayout:proc
extern LayoutManager_TiledLayout:proc
extern ThemeManager_ApplyDarkTheme:proc
extern ThemeManager_ApplyLightTheme:proc

.data

; IDE pane types
IDE_PANE_EDITOR         equ 1
IDE_PANE_TERMINAL       equ 2
IDE_PANE_FILETREE       equ 3
IDE_PANE_CHAT           equ 4
IDE_PANE_ORCHESTRA      equ 5
IDE_PANE_PROGRESS       equ 6
IDE_PANE_PROPERTIES     equ 7
IDE_PANE_OUTPUT         equ 8

; Default pane IDs (for quick reference)
.data
    g_idEditorPane          dd 0
    g_idTerminalPane        dd 0
    g_idFileTreePane        dd 0
    g_idChatPane            dd 0
    g_idOrchestraPane       dd 0
    g_idProgressPane        dd 0
    g_idPropertiesPane      dd 0
    g_idOutputPane          dd 0
    
    ; Layout state
    g_DefaultLayoutLoaded   dd 0
    g_CurrentPaneCount      dd 0

.code

; ============================================================================
; IDE INTEGRATION FUNCTIONS
; ============================================================================

; IDEPaneSystem_Initialize - Initialize the Qt pane system for the IDE
; Sets up default layout with essential panes
public IDEPaneSystem_Initialize
IDEPaneSystem_Initialize proc
    invoke PaneManager_Initialize
    test eax, eax
    jz @InitFailed
    
    ; Apply dark theme by default
    invoke ThemeManager_ApplyDarkTheme
    
    ; Create default panes
    invoke IDEPaneSystem_CreateDefaultLayout
    test eax, eax
    jz @InitFailed
    
    mov eax, TRUE
    ret
    
@InitFailed:
    xor eax, eax
    ret
IDEPaneSystem_Initialize endp

; IDEPaneSystem_CreateDefaultLayout - Create standard IDE pane layout
; Creates: Editor (center), Terminal (bottom), File Tree (left), Chat (right)
public IDEPaneSystem_CreateDefaultLayout
IDEPaneSystem_CreateDefaultLayout proc
    LOCAL paneID:DWORD
    
    ; Create File Tree pane (left, dockable)
    invoke PaneManager_CreatePane, 3, 0x3F, 1, 105    ; Type=3, Flags, Dock=LEFT, Widget=TREEVIEW
    mov g_idFileTreePane, eax
    mov paneID, eax
    invoke PaneManager_SetPanePosition, paneID, 0, 0, 200, 600
    
    ; Create Editor pane (center, resizable)
    invoke PaneManager_CreatePane, 1, 0x3F, 5, 103    ; Type=1, Flags, Dock=CENTER, Widget=TEXTEDIT
    mov g_idEditorPane, eax
    mov paneID, eax
    invoke PaneManager_SetPanePosition, paneID, 200, 0, 800, 600
    
    ; Create Chat pane (right, dockable)
    invoke PaneManager_CreatePane, 4, 0x3F, 2, 103    ; Type=4, Flags, Dock=RIGHT, Widget=TEXTEDIT
    mov g_idChatPane, eax
    mov paneID, eax
    invoke PaneManager_SetPanePosition, paneID, 1000, 0, 280, 600
    
    ; Create Terminal pane (bottom)
    invoke PaneManager_CreatePane, 2, 0x3F, 4, 103    ; Type=2, Flags, Dock=BOTTOM, Widget=TEXTEDIT
    mov g_idTerminalPane, eax
    mov paneID, eax
    invoke PaneManager_SetPanePosition, paneID, 200, 600, 800, 200
    
    ; Create Orchestra pane (bottom right)
    invoke PaneManager_CreatePane, 5, 0x3F, 4, 103    ; Type=5, Flags, Dock=BOTTOM, Widget=TEXTEDIT
    mov g_idOrchestraPane, eax
    mov paneID, eax
    invoke PaneManager_SetPanePosition, paneID, 1000, 600, 280, 200
    
    ; Create Progress pane (hidden by default)
    invoke PaneManager_CreatePane, 6, 0x2F, 4, 103    ; Type=6, Flags (not visible), Dock=BOTTOM
    mov g_idProgressPane, eax
    
    ; Create Properties pane (hidden by default)
    invoke PaneManager_CreatePane, 7, 0x2F, 2, 103    ; Type=7, Flags (not visible), Dock=RIGHT
    mov g_idPropertiesPane, eax
    
    ; Create Output pane (hidden by default)
    invoke PaneManager_CreatePane, 8, 0x2F, 4, 103    ; Type=8, Flags (not visible), Dock=BOTTOM
    mov g_idOutputPane, eax
    
    mov g_DefaultLayoutLoaded, 1
    mov eax, TRUE
    ret
IDEPaneSystem_CreateDefaultLayout endp

; ============================================================================
; PANE MANAGEMENT HELPERS
; ============================================================================

; IDEPaneSystem_AddPane - Add a new custom pane to the IDE
; Parameters:
;   esp+8:  dwPaneType (IDE_PANE_*)
;   esp+12: dwPosX
;   esp+16: dwPosY
;   esp+20: dwWidth
;   esp+24: dwHeight
; Returns: Pane ID
public IDEPaneSystem_AddPane
IDEPaneSystem_AddPane proc dwType:DWORD, x:DWORD, y:DWORD, w:DWORD, h:DWORD
    LOCAL paneID:DWORD
    
    ; Map IDE pane type to generic pane type
    mov eax, dwType
    
    ; Create pane (all as custom floating panes)
    invoke PaneManager_CreatePane, dwType, 0x3F, 0, 103    ; Flags=0x3F, Dock=FLOAT, Widget=TEXTEDIT
    mov paneID, eax
    
    ; Set position
    invoke PaneManager_SetPanePosition, paneID, x, y, w, h
    
    mov eax, paneID
    ret
IDEPaneSystem_AddPane endp

; IDEPaneSystem_RemovePane - Remove a pane from the IDE
; Parameters:
;   esp+8: paneID
; Returns: TRUE if successful
public IDEPaneSystem_RemovePane
IDEPaneSystem_RemovePane proc paneID:DWORD
    invoke PaneManager_DeletePane, paneID
    ret
IDEPaneSystem_RemovePane endp

; IDEPaneSystem_ShowPane - Make a pane visible
; Parameters:
;   esp+8: paneID
public IDEPaneSystem_ShowPane
IDEPaneSystem_ShowPane proc paneID:DWORD
    invoke PaneManager_SetPaneProperty, paneID, 0, 0x01    ; Set PANE_VISIBLE flag
    ret
IDEPaneSystem_ShowPane endp

; IDEPaneSystem_HidePane - Hide a pane
; Parameters:
;   esp+8: paneID
public IDEPaneSystem_HidePane
IDEPaneSystem_HidePane proc paneID:DWORD
    invoke PaneManager_SetPaneProperty, paneID, 0, 0x00    ; Clear PANE_VISIBLE flag
    ret
IDEPaneSystem_HidePane endp

; IDEPaneSystem_TogglePaneVisibility - Toggle pane visibility
; Parameters:
;   esp+8: paneID
; Returns: New visibility state (1=visible, 0=hidden)
public IDEPaneSystem_TogglePaneVisibility
IDEPaneSystem_TogglePaneVisibility proc paneID:DWORD
    LOCAL currentFlags:DWORD
    
    invoke PaneManager_GetPaneProperty, paneID, 0    ; Get flags
    mov currentFlags, eax
    
    ; Check if PANE_VISIBLE (bit 0) is set
    and eax, 1
    test eax, eax
    jz @MakeVisible
    
    ; Currently visible - hide it
    invoke PaneManager_SetPaneProperty, paneID, 0, 0x00
    xor eax, eax
    ret
    
@MakeVisible:
    ; Currently hidden - show it
    mov eax, currentFlags
    or eax, 1
    invoke PaneManager_SetPaneProperty, paneID, 0, eax
    mov eax, 1
    ret
IDEPaneSystem_TogglePaneVisibility endp

; ============================================================================
; LAYOUT MANAGEMENT
; ============================================================================

; IDEPaneSystem_ResetToDefaultLayout - Reset IDE to default pane layout
public IDEPaneSystem_ResetToDefaultLayout
IDEPaneSystem_ResetToDefaultLayout proc
    ; In a real implementation, would delete all custom panes and recreate defaults
    invoke IDEPaneSystem_CreateDefaultLayout
    ret
IDEPaneSystem_ResetToDefaultLayout endp

; IDEPaneSystem_SetLayoutMode - Switch between layout modes
; Parameters:
;   esp+8: dwLayoutMode (LAYOUT_SINGLE_WINDOW, LAYOUT_LEFT_RIGHT_SPLIT, etc.)
public IDEPaneSystem_SetLayoutMode
IDEPaneSystem_SetLayoutMode proc dwMode:DWORD
    ; TODO: Implement layout switching
    ; For now, just validate the mode
    
    cmp dwMode, 1
    jl @ModeInvalid
    cmp dwMode, 5
    jg @ModeInvalid
    
    mov eax, TRUE
    ret
    
@ModeInvalid:
    xor eax, eax
    ret
IDEPaneSystem_SetLayoutMode endp

; IDEPaneSystem_SwitchTheme - Switch between themes
; Parameters:
;   esp+8: dwThemeID (0=dark, 1=light)
public IDEPaneSystem_SwitchTheme
IDEPaneSystem_SwitchTheme proc dwThemeID:DWORD
    cmp dwThemeID, 0
    jne @LightTheme
    
    invoke ThemeManager_ApplyDarkTheme
    ret
    
@LightTheme:
    invoke ThemeManager_ApplyLightTheme
    ret
IDEPaneSystem_SwitchTheme endp

; ============================================================================
; RENDERING AND EVENT HANDLING
; ============================================================================

; IDEPaneSystem_Render - Render all panes for display
; Parameters:
;   esp+8: hdc (device context)
public IDEPaneSystem_Render
IDEPaneSystem_Render proc hdc:DWORD
    invoke PaneManager_RenderAllPanes, hdc
    ret
IDEPaneSystem_Render endp

; IDEPaneSystem_HandleMouseClick - Handle mouse click event
; Parameters:
;   esp+8: x
;   esp+12: y
; Returns: Pane ID that was clicked (or 0 if none)
public IDEPaneSystem_HandleMouseClick
IDEPaneSystem_HandleMouseClick proc x:DWORD, y:DWORD
    invoke PaneManager_HandleMouseDown, x, y
    ret
IDEPaneSystem_HandleMouseClick endp

; IDEPaneSystem_HandleMouseMove - Handle mouse move event
; Parameters:
;   esp+8: x
;   esp+12: y
public IDEPaneSystem_HandleMouseMove
IDEPaneSystem_HandleMouseMove proc x:DWORD, y:DWORD
    invoke PaneManager_HandleMouseMove, x, y
    ret
IDEPaneSystem_HandleMouseMove endp

; IDEPaneSystem_HandleMouseRelease - Handle mouse release event
public IDEPaneSystem_HandleMouseRelease
IDEPaneSystem_HandleMouseRelease proc
    invoke PaneManager_HandleMouseUp
    ret
IDEPaneSystem_HandleMouseRelease endp

; ============================================================================
; PANE STATISTICS
; ============================================================================

; IDEPaneSystem_GetPaneCount - Get total number of open panes
; Returns: Count (0-100)
public IDEPaneSystem_GetPaneCount
IDEPaneSystem_GetPaneCount proc
    invoke PaneManager_GetPaneCount
    ret
IDEPaneSystem_GetPaneCount endp

; IDEPaneSystem_GetPaneInfo - Get information about a pane
; Parameters:
;   esp+8: paneID
; Returns: Pointer to PANE structure
public IDEPaneSystem_GetPaneInfo
IDEPaneSystem_GetPaneInfo proc paneID:DWORD
    invoke PaneManager_GetPaneInfo, paneID
    ret
IDEPaneSystem_GetPaneInfo endp

; IDEPaneSystem_FindPaneAtCoords - Find which pane is at given coordinates
; Parameters:
;   esp+8: x
;   esp+12: y
; Returns: Pane ID
public IDEPaneSystem_FindPaneAtCoords
IDEPaneSystem_FindPaneAtCoords proc x:DWORD, y:DWORD
    invoke PaneManager_GetPaneAtCoords, x, y
    ret
IDEPaneSystem_FindPaneAtCoords endp

; ============================================================================
; PANE POSITIONING
; ============================================================================

; IDEPaneSystem_MovePaneToFront - Bring pane to front
; Parameters:
;   esp+8: paneID
public IDEPaneSystem_MovePaneToFront
IDEPaneSystem_MovePaneToFront proc paneID:DWORD
    invoke PaneManager_SetZOrder, paneID, 1
    ret
IDEPaneSystem_MovePaneToFront endp

; IDEPaneSystem_MovePaneToBack - Send pane to back
; Parameters:
;   esp+8: paneID
public IDEPaneSystem_MovePaneToBack
IDEPaneSystem_MovePaneToBack proc paneID:DWORD
    invoke PaneManager_SetZOrder, paneID, 0
    ret
IDEPaneSystem_MovePaneToBack endp

; IDEPaneSystem_ResizePane - Resize a pane
; Parameters:
;   esp+8: paneID
;   esp+12: x
;   esp+16: y
;   esp+20: width
;   esp+24: height
public IDEPaneSystem_ResizePane
IDEPaneSystem_ResizePane proc paneID:DWORD, x:DWORD, y:DWORD, w:DWORD, h:DWORD
    invoke PaneManager_SetPanePosition, paneID, x, y, w, h
    ret
IDEPaneSystem_ResizePane endp

; ============================================================================
; PERSISTENCE
; ============================================================================

; IDEPaneSystem_SaveCurrentLayout - Save current pane layout to memory
; Returns: Pointer to layout buffer
public IDEPaneSystem_SaveCurrentLayout
IDEPaneSystem_SaveCurrentLayout proc
    invoke LayoutManager_SaveLayout
    ret
IDEPaneSystem_SaveCurrentLayout endp

; IDEPaneSystem_LoadLayout - Load pane layout from memory
; Parameters:
;   esp+8: pBuffer (pointer to serialized layout)
public IDEPaneSystem_LoadLayout
IDEPaneSystem_LoadLayout proc pBuffer:DWORD
    invoke LayoutManager_LoadLayout, pBuffer
    ret
IDEPaneSystem_LoadLayout endp

; IDEPaneSystem_ExportLayoutJSON - Export layout as JSON
; Parameters:
;   esp+8: pOutputBuffer
;   esp+12: dwBufferSize
; Returns: Number of bytes written
public IDEPaneSystem_ExportLayoutJSON
IDEPaneSystem_ExportLayoutJSON proc pBuffer:DWORD, dwSize:DWORD
    invoke LayoutManager_ExportLayoutJSON, pBuffer, dwSize
    ret
IDEPaneSystem_ExportLayoutJSON endp

end
