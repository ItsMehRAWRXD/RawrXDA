; ============================================================================
; RawrXD Agentic IDE - Menu System Implementation (Pure MASM)
; Complete menu bar with all IDE functions
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
include constants.inc

; Global menu handle
g_hMainMenu     dd 0

public g_hMainMenu

; Menu resource structure (manually defined)
; We'll create the menu programmatically

; Menu text strings
szMenuFile      db "&File",0
szMenuAgentic   db "&Agentic",0
szMenuTools     db "&Tools",0
szMenuView      db "&View",0
szMenuHelp      db "&Help",0

; File menu items
szMenuNew       db "&New\tCtrl+N",0
szMenuOpen      db "&Open...\tCtrl+O",0
szMenuSave      db "&Save\tCtrl+S",0
szMenuSaveAs    db "Save &As...",0
szMenuExit      db "E&xit\tAlt+F4",0

; Agentic menu items
szMenuWish      db "Make a &Wish...",0
szMenuLoop      db "Start &Loop...",0

; Tools menu items
szMenuRegistry  db "Tool &Registry...",0
szMenuGGUF      db "&Load GGUF Model...",0
szMenuCompress  db "&Compress Info...",0

; View menu items  
szMenuFloating  db "&Floating Panel",0
szMenuRefresh   db "&Refresh File Tree\tF5",0

; Help menu items
szMenuAbout     db "&About RawrXD IDE...",0

.code

public CreateMainMenu

; Forward declarations
CreateMainMenu proto
CreateFileMenu proto
CreateAgenticMenu proto  
CreateToolsMenu proto
CreateViewMenu proto
CreateHelpMenu proto

; ============================================================================
; CreateMainMenu - Create complete menu bar
; Returns: Menu handle in eax
; ============================================================================
public CreateMainMenu
CreateMainMenu proc
    LOCAL hMenu:DWORD
    LOCAL hFileMenu:DWORD
    LOCAL hAgenticMenu:DWORD
    LOCAL hToolsMenu:DWORD
    LOCAL hViewMenu:DWORD
    LOCAL hHelpMenu:DWORD
    
    ; Create main menu bar
    invoke CreateMenu
    mov hMenu, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Create File menu
    call CreateFileMenu
    mov hFileMenu, eax
    invoke AppendMenu, hMenu, MF_POPUP, hFileMenu, addr szMenuFile
    
    ; Create Agentic menu
    call CreateAgenticMenu
    mov hAgenticMenu, eax
    invoke AppendMenu, hMenu, MF_POPUP, hAgenticMenu, addr szMenuAgentic
    
    ; Create Tools menu
    call CreateToolsMenu
    mov hToolsMenu, eax
    invoke AppendMenu, hMenu, MF_POPUP, hToolsMenu, addr szMenuTools
    
    ; Create View menu
    call CreateViewMenu
    mov hViewMenu, eax
    invoke AppendMenu, hMenu, MF_POPUP, hViewMenu, addr szMenuView
    
    ; Create Help menu
    call CreateHelpMenu
    mov hHelpMenu, eax
    invoke AppendMenu, hMenu, MF_POPUP, hHelpMenu, addr szMenuHelp
    
    ; Store menu handle globally
    mov eax, hMenu
    mov g_hMainMenu, eax
    
    ret
CreateMainMenu endp

; ============================================================================
; CreateFileMenu - Create File menu
; ============================================================================
CreateFileMenu proc
    LOCAL hMenu:DWORD
    
    invoke CreatePopupMenu
    mov hMenu, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Add File menu items
    invoke AppendMenu, hMenu, MF_STRING, IDM_FILE_NEW, addr szMenuNew
    invoke AppendMenu, hMenu, MF_STRING, IDM_FILE_OPEN, addr szMenuOpen
    invoke AppendMenu, hMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenu, hMenu, MF_STRING, IDM_FILE_SAVE, addr szMenuSave
    invoke AppendMenu, hMenu, MF_STRING, IDM_FILE_SAVEAS, addr szMenuSaveAs
    invoke AppendMenu, hMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenu, hMenu, MF_STRING, IDM_FILE_EXIT, addr szMenuExit
    
    mov eax, hMenu
    ret
CreateFileMenu endp

; ============================================================================
; CreateAgenticMenu - Create Agentic menu
; ============================================================================
CreateAgenticMenu proc
    LOCAL hMenu:DWORD
    
    invoke CreatePopupMenu
    mov hMenu, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Add Agentic menu items
    invoke AppendMenu, hMenu, MF_STRING, IDM_AGENTIC_WISH, addr szMenuWish
    invoke AppendMenu, hMenu, MF_STRING, IDM_AGENTIC_LOOP, addr szMenuLoop
    
    mov eax, hMenu
    ret
CreateAgenticMenu endp

; ============================================================================
; CreateToolsMenu - Create Tools menu
; ============================================================================
CreateToolsMenu proc
    LOCAL hMenu:DWORD
    
    invoke CreatePopupMenu
    mov hMenu, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Add Tools menu items
    invoke AppendMenu, hMenu, MF_STRING, IDM_TOOLS_REGISTRY, addr szMenuRegistry
    invoke AppendMenu, hMenu, MF_STRING, IDM_TOOLS_LOAD_GGUF, addr szMenuGGUF
    invoke AppendMenu, hMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenu, hMenu, MF_STRING, IDM_FILE_COMPRESS_INFO, addr szMenuCompress
    
    mov eax, hMenu
    ret
CreateToolsMenu endp

; ============================================================================
; CreateViewMenu - Create View menu
; ============================================================================
CreateViewMenu proc
    LOCAL hMenu:DWORD
    
    invoke CreatePopupMenu
    mov hMenu, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Add View menu items
    invoke AppendMenu, hMenu, MF_STRING, IDM_VIEW_FLOATING, addr szMenuFloating
    invoke AppendMenu, hMenu, MF_SEPARATOR, 0, NULL
        invoke AppendMenu, hMenu, MF_STRING, IDM_VIEW_REFRESH_TREE, addr szMenuRefresh
    
    mov eax, hMenu
    ret
CreateViewMenu endp

; ============================================================================
; CreateHelpMenu - Create Help menu
; ============================================================================
CreateHelpMenu proc
    LOCAL hMenu:DWORD
    
    invoke CreatePopupMenu
    mov hMenu, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Add Help menu items
        invoke AppendMenu, hMenu, MF_STRING, IDM_HELP_ABOUT, addr szMenuAbout
    
    mov eax, hMenu
    ret
CreateHelpMenu endp

; ============================================================================
; SetupMenuAccelerators - Create accelerator table
; ============================================================================
SetupMenuAccelerators proc
    ; Accelerator table would be set up here
    ; For now, we'll handle accelerators in the main message loop
    ret
SetupMenuAccelerators endp

; ============================================================================
; UpdateMenuStates - Update menu item states based on current context
; ============================================================================
UpdateMenuStates proc hMenu:DWORD
    ; Enable/disable menu items based on state
    ; This can be called when file state changes, etc.
    ret
UpdateMenuStates endp

; ============================================================================
; Public interface
; ============================================================================
public CreateMainMenu
public SetupMenuAccelerators
public UpdateMenuStates

end