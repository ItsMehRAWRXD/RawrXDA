;==============================================================================
; File 5: ide_menu.asm - Complete Menu System
;==============================================================================
include windows.inc

.code
;==============================================================================
; Create Complete Menu Bar
;==============================================================================
IDEMenu_Create PROC hParentWnd:QWORD
    LOCAL hMenu:QWORD
    LOCAL hSubMenu:QWORD
    
    ; Main menu bar
    invoke CreateMenu
    mov [hMainMenu], rax
    mov hMenu, rax
    
    ;===== FILE MENU =====
    invoke CreatePopupMenu
    mov hSubMenu, rax
    
    invoke AppendMenu, hSubMenu, 0, 100, OFFSET szMenuNewTab
    invoke AppendMenu, hSubMenu, 0, 101, OFFSET szMenuOpenFolder
    invoke AppendMenu, hSubMenu, 0, 102, OFFSET szMenuOpenFile
    invoke AppendMenu, hSubMenu, 0x00000800, 0, 0  ; Separator
    invoke AppendMenu, hSubMenu, 0, 103, OFFSET szMenuSave
    invoke AppendMenu, hSubMenu, 0, 104, OFFSET szMenuSaveAll
    invoke AppendMenu, hSubMenu, 0x00000800, 0, 0
    invoke AppendMenu, hSubMenu, 0, 105, OFFSET szMenuExit
    
    invoke AppendMenu, hMenu, 0x00000010, hSubMenu, OFFSET szMenuFile
    
    ;===== EDIT MENU =====
    invoke CreatePopupMenu
    mov hSubMenu, rax
    
    invoke AppendMenu, hSubMenu, 0, 200, OFFSET szMenuUndo
    invoke AppendMenu, hSubMenu, 0, 201, OFFSET szMenuRedo
    invoke AppendMenu, hSubMenu, 0x00000800, 0, 0
    invoke AppendMenu, hSubMenu, 0, 202, OFFSET szMenuCut
    invoke AppendMenu, hSubMenu, 0, 203, OFFSET szMenuCopy
    invoke AppendMenu, hSubMenu, 0, 204, OFFSET szMenuPaste
    invoke AppendMenu, hSubMenu, 0x00000800, 0, 0
    invoke AppendMenu, hSubMenu, 0, 205, OFFSET szMenuFind
    invoke AppendMenu, hSubMenu, 0, 206, OFFSET szMenuReplace
    
    invoke AppendMenu, hMenu, 0x00000010, hSubMenu, OFFSET szMenuEdit
    
    ;===== VIEW MENU =====
    invoke CreatePopupMenu
    mov hSubMenu, rax
    
    invoke AppendMenu, hSubMenu, 0, 300, OFFSET szMenuZoomIn
    invoke AppendMenu, hSubMenu, 0, 301, OFFSET szMenuZoomOut
    invoke AppendMenu, hSubMenu, 0, 302, OFFSET szMenuResetZoom
    invoke AppendMenu, hSubMenu, 0x00000800, 0, 0
    invoke AppendMenu, hSubMenu, 0, 303, OFFSET szMenuSplitEditor
    invoke AppendMenu, hSubMenu, 0, 304, OFFSET szMenuToggleTerminal
    invoke AppendMenu, hSubMenu, 0, 305, OFFSET szMenuToggleAgent
    
    invoke AppendMenu, hMenu, 0x00000010, hSubMenu, OFFSET szMenuView
    
    ;===== AGENT MENU =====
    invoke CreatePopupMenu
    mov hSubMenu, rax
    
    invoke AppendMenu, hSubMenu, 0, 400, OFFSET szMenuNewChat
    invoke AppendMenu, hSubMenu, 0x00000800, 0, 0
    invoke AppendMenu, hSubMenu, 0, 401, OFFSET szMenuFixBugs
    invoke AppendMenu, hSubMenu, 0, 402, OFFSET szMenuWriteTests
    invoke AppendMenu, hSubMenu, 0, 403, OFFSET szMenuDocument
    invoke AppendMenu, hSubMenu, 0x00000800, 0, 0
    invoke AppendMenu, hSubMenu, 0, 404, OFFSET szMenuReviewPR
    invoke AppendMenu, hSubMenu, 0x00000800, 0, 0
    invoke AppendMenu, hSubMenu, 0x00000008, 405, OFFSET szMenuPrivacyMode
    
    invoke AppendMenu, hMenu, 0x00000010, hSubMenu, OFFSET szMenuAgent
    
    ;===== TOOLS MENU =====
    invoke CreatePopupMenu
    mov hSubMenu, rax
    
    invoke AppendMenu, hSubMenu, 0, 500, OFFSET szMenuMCPServers
    invoke AppendMenu, hSubMenu, 0, 501, OFFSET szMenuModelManager
    invoke AppendMenu, hSubMenu, 0x00000800, 0, 0
    invoke AppendMenu, hSubMenu, 0, 502, OFFSET szMenuSettings
    
    invoke AppendMenu, hMenu, 0x00000010, hSubMenu, OFFSET szMenuTools
    
    ;===== HELP MENU =====
    invoke CreatePopupMenu
    mov hSubMenu, rax
    
    invoke AppendMenu, hSubMenu, 0, 600, OFFSET szMenuDocumentation
    invoke AppendMenu, hSubMenu, 0x00000800, 0, 0
    invoke AppendMenu, hSubMenu, 0, 601, OFFSET szMenuAbout
    
    invoke AppendMenu, hMenu, 0x00000010, hSubMenu, OFFSET szMenuHelp
    
    ; Set menu
    invoke SetMenu, hParentWnd, [hMainMenu]
    invoke DrawMenuBar, hParentWnd
    
    LOG_INFO "Menu bar created with 7 menus"
    
    ret
IDEMenu_Create ENDP

;==============================================================================
; Create Accelerators (Keyboard Shortcuts)
;==============================================================================
IDEMenu_CreateAccelerators PROC
    LOCAL accel[20]:BYTE
    LOCAL accelSize:DWORD
    
    ; Ctrl+N = New Tab
    mov [accelSize], 0
    
    ; Create table (simplified - would need proper structure)
    invoke CreateAcceleratorTable, ADDR accel, 0
    mov [hAccelTable], rax
    
    ret
IDEMenu_CreateAccelerators ENDP

;==============================================================================
; Data
;==============================================================================
.data
hMainMenu         dq ?
hAccelTable       dq ?

; Menu strings
szMenuFile        db '&File',0
szMenuEdit        db '&Edit',0
szMenuView        db '&View',0
szMenuAgent       db '&Agent',0
szMenuTools       db '&Tools',0
szMenuHelp        db '&Help',0

; File menu
szMenuNewTab      db '&New Tab',0
szMenuOpenFolder  db '&Open Folder...',0
szMenuOpenFile    db '&Open File...',0
szMenuSave        db '&Save',0
szMenuSaveAll     db 'Save &All',0
szMenuExit        db 'E&xit',0

; Edit menu
szMenuUndo        db '&Undo',0
szMenuRedo        db '&Redo',0
szMenuCut         db 'Cu&t',0
szMenuCopy        db '&Copy',0
szMenuPaste       db '&Paste',0
szMenuFind        db '&Find',0
szMenuReplace     db '&Replace',0

; View menu
szMenuZoomIn      db 'Zoom &In',0
szMenuZoomOut     db 'Zoom &Out',0
szMenuResetZoom   db '&Reset Zoom',0
szMenuSplitEditor db '&Split Editor',0
szMenuToggleTerminal db 'Toggle &Terminal',0
szMenuToggleAgent db 'Toggle &Agent',0

; Agent menu
szMenuNewChat     db '&New Chat',0
szMenuFixBugs     db '&Fix Bugs',0
szMenuWriteTests  db '&Write Tests',0
szMenuDocument    db '&Document Code',0
szMenuReviewPR    db '&Review PR',0
szMenuPrivacyMode db 'P&rivacy Mode',0

; Tools menu
szMenuMCPServers  db 'MCP &Servers...',0
szMenuModelManager db '&Models...',0
szMenuSettings    db '&Settings...',0

; Help menu
szMenuDocumentation db '&Docs',0
szMenuAbout       db '&About',0

END
