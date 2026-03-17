; ============================================================================
; PANE_SYSTEM_CORE.ASM - Qt-like Pane/Widget System
; Complete pane management with up to 100 customizable panes
; Supports docking, resizing, z-ordering, and dynamic layout
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comctl32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comctl32.lib

; ============================================================================
; PANE STRUCTURES
; ============================================================================

PANE_TYPE_EDITOR equ 1
PANE_TYPE_TERMINAL equ 2
PANE_TYPE_CHAT equ 3
PANE_TYPE_FILE_TREE equ 4
PANE_TYPE_TOOLS equ 5
PANE_TYPE_OUTPUT equ 6
PANE_TYPE_DEBUG equ 7
PANE_TYPE_SETTINGS equ 8
PANE_TYPE_CUSTOM equ 9

PANE_STATE_VISIBLE equ 1
PANE_STATE_HIDDEN equ 2
PANE_STATE_MINIMIZED equ 3
PANE_STATE_MAXIMIZED equ 4
PANE_STATE_DOCKED equ 5
PANE_STATE_FLOATING equ 6

DOCK_LEFT equ 1
DOCK_RIGHT equ 2
DOCK_TOP equ 3
DOCK_BOTTOM equ 4
DOCK_CENTER equ 5

; Pane structure (80 bytes)
PANE_INFO struct
    hWnd            dd ?    ; Window handle
    dwPaneID        dd ?    ; Unique pane ID (1-100)
    dwPaneType      dd ?    ; PANE_TYPE_*
    dwPaneState     dd ?    ; PANE_STATE_*
    dwDockPosition  dd ?    ; DOCK_*
    x               dd ?    ; X position
    y               dd ?    ; Y position
    width           dd ?    ; Pane width
    height          dd ?    ; Pane height
    zOrder          dd ?    ; Z-order (0-99)
    hParent         dd ?    ; Parent pane ID or 0 if top-level
    dwFlags         dd ?    ; Flags (resizable, closable, etc)
    pszTitle        dd ?    ; Pane title string pointer
    dwCustomColor   dd ?    ; Custom background color
    dwMinWidth      dd ?    ; Minimum width constraint
    dwMinHeight     dd ?    ; Minimum height constraint
    dwMaxWidth      dd ?    ; Maximum width constraint
    dwMaxHeight     dd ?    ; Maximum height constraint
    reserved        dd 2 dup (?) ; Reserved for future use
PANE_INFO ends

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    ; Pane storage (100 panes max)
    g_PaneArray PANE_INFO 100 dup (<>)
    g_dwPaneCount dd 0          ; Current number of active panes
    g_dwNextPaneID dd 1         ; Next pane ID to assign
    g_dwFocusedPane dd 0        ; Currently focused pane ID
    
    ; Pane type strings
    szPaneTypeEditor db "Editor", 0
    szPaneTypeTerminal db "Terminal", 0
    szPaneTypeChat db "Chat", 0
    szPaneTypeFileTree db "File Tree", 0
    szPaneTypeTools db "Tools", 0
    szPaneTypeOutput db "Output", 0
    szPaneTypeDebug db "Debug", 0
    szPaneTypeSettings db "Settings", 0
    
    ; Layout presets
    szLayoutVSCode db "VS Code Classic", 0
    szLayoutWebtorm db "WebStorm Style", 0
    szLayoutVisualStudio db "Visual Studio", 0
    szLayoutCustom db "Custom", 0

.code

; ============================================================================
; PANE_SYSTEM_INIT - Initialize pane system
; ============================================================================
public PANE_SYSTEM_INIT
PANE_SYSTEM_INIT proc
    mov g_dwPaneCount, 0
    mov g_dwNextPaneID, 1
    mov g_dwFocusedPane, 0
    
    ; Clear all pane entries
    xor ecx, ecx
    lea edi, g_PaneArray
    mov eax, 0
    mov ecx, 100 * sizeof(PANE_INFO) / 4
    rep stosd
    
    mov eax, TRUE
    ret
PANE_SYSTEM_INIT endp

; ============================================================================
; PANE_CREATE - Create a new pane
; Parameters: pszTitle, dwType, dwWidth, dwHeight, dwDockPosition
; Returns: Pane ID (eax), or 0 on failure
; ============================================================================
public PANE_CREATE
PANE_CREATE proc pszTitle:DWORD, dwType:DWORD, dwWidth:DWORD, dwHeight:DWORD, dwDockPos:DWORD
    LOCAL paneID:DWORD
    
    ; Check if we have room for another pane
    cmp g_dwPaneCount, 100
    jge @CreateFailed
    
    ; Get next available pane ID
    mov eax, g_dwNextPaneID
    mov paneID, eax
    inc g_dwNextPaneID
    
    ; Calculate array index (paneID - 1)
    mov eax, paneID
    dec eax
    imul eax, sizeof(PANE_INFO)
    lea edi, g_PaneArray
    add edi, eax
    
    ; Initialize pane structure
    mov eax, 0
    mov [edi].PANE_INFO.hWnd, eax
    mov eax, paneID
    mov [edi].PANE_INFO.dwPaneID, eax
    mov eax, dwType
    mov [edi].PANE_INFO.dwPaneType, eax
    mov eax, PANE_STATE_VISIBLE
    mov [edi].PANE_INFO.dwPaneState, eax
    mov eax, dwDockPos
    mov [edi].PANE_INFO.dwDockPosition, eax
    
    ; Set dimensions
    mov eax, 0
    mov [edi].PANE_INFO.x, eax
    mov [edi].PANE_INFO.y, eax
    mov eax, dwWidth
    mov [edi].PANE_INFO.width, eax
    mov eax, dwHeight
    mov [edi].PANE_INFO.height, eax
    
    ; Set Z-order to current count
    mov eax, g_dwPaneCount
    mov [edi].PANE_INFO.zOrder, eax
    
    ; Set defaults
    mov [edi].PANE_INFO.hParent, 0
    mov eax, 1  ; Resizable | Closable
    mov [edi].PANE_INFO.dwFlags, eax
    mov eax, pszTitle
    mov [edi].PANE_INFO.pszTitle, eax
    mov eax, 001E1E1Eh  ; Dark background
    mov [edi].PANE_INFO.dwCustomColor, eax
    
    ; Set constraints
    mov eax, 100
    mov [edi].PANE_INFO.dwMinWidth, eax
    mov [edi].PANE_INFO.dwMinHeight, eax
    mov eax, 4000
    mov [edi].PANE_INFO.dwMaxWidth, eax
    mov [edi].PANE_INFO.dwMaxHeight, eax
    
    ; Increment pane count
    inc g_dwPaneCount
    
    mov eax, paneID
    ret
    
@CreateFailed:
    xor eax, eax
    ret
PANE_CREATE endp

; ============================================================================
; PANE_DESTROY - Destroy a pane
; Parameters: paneID
; Returns: TRUE if successful, FALSE otherwise
; ============================================================================
public PANE_DESTROY
PANE_DESTROY proc dwPaneID:DWORD
    ; Validate pane ID
    cmp dwPaneID, 1
    jl @DestroyFailed
    cmp dwPaneID, 100
    jg @DestroyFailed
    
    ; Calculate array index
    mov eax, dwPaneID
    dec eax
    imul eax, sizeof(PANE_INFO)
    lea edi, g_PaneArray
    add edi, eax
    
    ; Check if pane exists
    mov eax, [edi].PANE_INFO.dwPaneID
    test eax, eax
    jz @DestroyFailed
    
    ; Destroy window if it exists
    mov eax, [edi].PANE_INFO.hWnd
    test eax, eax
    jz @SkipWindowDestroy
    invoke DestroyWindow, eax
@SkipWindowDestroy:
    
    ; Clear pane structure
    xor eax, eax
    mov [edi].PANE_INFO.dwPaneID, eax
    mov [edi].PANE_INFO.hWnd, eax
    
    ; Decrement pane count
    dec g_dwPaneCount
    
    mov eax, TRUE
    ret
    
@DestroyFailed:
    xor eax, eax
    ret
PANE_DESTROY endp

; ============================================================================
; PANE_SETPOSITION - Set pane position and size
; Parameters: paneID, x, y, width, height
; Returns: TRUE if successful
; ============================================================================
public PANE_SETPOSITION
PANE_SETPOSITION proc dwPaneID:DWORD, x:DWORD, y:DWORD, width:DWORD, height:DWORD
    ; Validate and get pane
    cmp dwPaneID, 1
    jl @SetPosFailed
    cmp dwPaneID, 100
    jg @SetPosFailed
    
    mov eax, dwPaneID
    dec eax
    imul eax, sizeof(PANE_INFO)
    lea edi, g_PaneArray
    add edi, eax
    
    ; Check if pane exists
    mov eax, [edi].PANE_INFO.dwPaneID
    test eax, eax
    jz @SetPosFailed
    
    ; Apply constraints and set position
    mov eax, x
    mov [edi].PANE_INFO.x, eax
    mov eax, y
    mov [edi].PANE_INFO.y, eax
    
    ; Check width constraints
    mov eax, width
    cmp eax, [edi].PANE_INFO.dwMinWidth
    jge @CheckMaxWidth
    mov eax, [edi].PANE_INFO.dwMinWidth
@CheckMaxWidth:
    cmp eax, [edi].PANE_INFO.dwMaxWidth
    jle @SetWidth
    mov eax, [edi].PANE_INFO.dwMaxWidth
@SetWidth:
    mov [edi].PANE_INFO.width, eax
    
    ; Check height constraints
    mov eax, height
    cmp eax, [edi].PANE_INFO.dwMinHeight
    jge @CheckMaxHeight
    mov eax, [edi].PANE_INFO.dwMinHeight
@CheckMaxHeight:
    cmp eax, [edi].PANE_INFO.dwMaxHeight
    jle @SetHeight
    mov eax, [edi].PANE_INFO.dwMaxHeight
@SetHeight:
    mov [edi].PANE_INFO.height, eax
    
    ; Move window if it exists
    mov eax, [edi].PANE_INFO.hWnd
    test eax, eax
    jz @SetPosDone
    
    invoke MoveWindow, eax, x, y, [edi].PANE_INFO.width, [edi].PANE_INFO.height, TRUE
    
@SetPosDone:
    mov eax, TRUE
    ret
    
@SetPosFailed:
    xor eax, eax
    ret
PANE_SETPOSITION endp

; ============================================================================
; PANE_SETSTATE - Set pane visibility/state
; Parameters: paneID, dwState
; Returns: TRUE if successful
; ============================================================================
public PANE_SETSTATE
PANE_SETSTATE proc dwPaneID:DWORD, dwState:DWORD
    ; Validate pane ID
    cmp dwPaneID, 1
    jl @SetStateFailed
    cmp dwPaneID, 100
    jg @SetStateFailed
    
    mov eax, dwPaneID
    dec eax
    imul eax, sizeof(PANE_INFO)
    lea edi, g_PaneArray
    add edi, eax
    
    ; Check if pane exists
    mov eax, [edi].PANE_INFO.dwPaneID
    test eax, eax
    jz @SetStateFailed
    
    ; Set state
    mov eax, dwState
    mov [edi].PANE_INFO.dwPaneState, eax
    
    ; Update window visibility
    mov eax, [edi].PANE_INFO.hWnd
    test eax, eax
    jz @SetStateDone
    
    cmp dwState, PANE_STATE_VISIBLE
    je @ShowWindow
    cmp dwState, PANE_STATE_HIDDEN
    je @HideWindow
    cmp dwState, PANE_STATE_MINIMIZED
    je @MinimizeWindow
    cmp dwState, PANE_STATE_MAXIMIZED
    je @MaximizeWindow
    jmp @SetStateDone
    
@ShowWindow:
    invoke ShowWindow, eax, SW_SHOW
    jmp @SetStateDone
@HideWindow:
    invoke ShowWindow, eax, SW_HIDE
    jmp @SetStateDone
@MinimizeWindow:
    invoke ShowWindow, eax, SW_MINIMIZE
    jmp @SetStateDone
@MaximizeWindow:
    invoke ShowWindow, eax, SW_MAXIMIZE
    jmp @SetStateDone
    
@SetStateDone:
    mov eax, TRUE
    ret
    
@SetStateFailed:
    xor eax, eax
    ret
PANE_SETSTATE endp

; ============================================================================
; PANE_SETZORDER - Set pane Z-order (depth)
; Parameters: paneID, zOrder (0-99)
; Returns: TRUE if successful
; ============================================================================
public PANE_SETZORDER
PANE_SETZORDER proc dwPaneID:DWORD, zOrder:DWORD
    ; Validate pane ID
    cmp dwPaneID, 1
    jl @SetZFailed
    cmp dwPaneID, 100
    jg @SetZFailed
    
    cmp zOrder, 99
    jg @SetZFailed
    
    mov eax, dwPaneID
    dec eax
    imul eax, sizeof(PANE_INFO)
    lea edi, g_PaneArray
    add edi, eax
    
    ; Check if pane exists
    mov eax, [edi].PANE_INFO.dwPaneID
    test eax, eax
    jz @SetZFailed
    
    ; Set Z-order
    mov eax, zOrder
    mov [edi].PANE_INFO.zOrder, eax
    
    mov eax, TRUE
    ret
    
@SetZFailed:
    xor eax, eax
    ret
PANE_SETZORDER endp

; ============================================================================
; PANE_GETINFO - Get pane information
; Parameters: paneID, pPaneInfo (pointer to PANE_INFO structure)
; Returns: TRUE if successful
; ============================================================================
public PANE_GETINFO
PANE_GETINFO proc dwPaneID:DWORD, pPaneInfo:DWORD
    ; Validate pane ID
    cmp dwPaneID, 1
    jl @GetInfoFailed
    cmp dwPaneID, 100
    jg @GetInfoFailed
    
    mov eax, dwPaneID
    dec eax
    imul eax, sizeof(PANE_INFO)
    lea esi, g_PaneArray
    add esi, eax
    
    ; Check if pane exists
    mov eax, [esi].PANE_INFO.dwPaneID
    test eax, eax
    jz @GetInfoFailed
    
    ; Copy pane info to destination
    mov edi, pPaneInfo
    mov ecx, sizeof(PANE_INFO) / 4
    rep movsd
    
    mov eax, TRUE
    ret
    
@GetInfoFailed:
    xor eax, eax
    ret
PANE_GETINFO endp

; ============================================================================
; PANE_SETCONSTRAINTS - Set pane size constraints
; Parameters: paneID, dwMinWidth, dwMinHeight, dwMaxWidth, dwMaxHeight
; Returns: TRUE if successful
; ============================================================================
public PANE_SETCONSTRAINTS
PANE_SETCONSTRAINTS proc dwPaneID:DWORD, dwMinW:DWORD, dwMinH:DWORD, dwMaxW:DWORD, dwMaxH:DWORD
    ; Validate pane ID
    cmp dwPaneID, 1
    jl @SetConstraintsFailed
    cmp dwPaneID, 100
    jg @SetConstraintsFailed
    
    mov eax, dwPaneID
    dec eax
    imul eax, sizeof(PANE_INFO)
    lea edi, g_PaneArray
    add edi, eax
    
    ; Check if pane exists
    mov eax, [edi].PANE_INFO.dwPaneID
    test eax, eax
    jz @SetConstraintsFailed
    
    ; Set constraints
    mov eax, dwMinW
    mov [edi].PANE_INFO.dwMinWidth, eax
    mov eax, dwMinH
    mov [edi].PANE_INFO.dwMinHeight, eax
    mov eax, dwMaxW
    mov [edi].PANE_INFO.dwMaxWidth, eax
    mov eax, dwMaxH
    mov [edi].PANE_INFO.dwMaxHeight, eax
    
    mov eax, TRUE
    ret
    
@SetConstraintsFailed:
    xor eax, eax
    ret
PANE_SETCONSTRAINTS endp

; ============================================================================
; PANE_SETCOLOR - Set pane background color
; Parameters: paneID, dwColor
; Returns: TRUE if successful
; ============================================================================
public PANE_SETCOLOR
PANE_SETCOLOR proc dwPaneID:DWORD, dwColor:DWORD
    ; Validate pane ID
    cmp dwPaneID, 1
    jl @SetColorFailed
    cmp dwPaneID, 100
    jg @SetColorFailed
    
    mov eax, dwPaneID
    dec eax
    imul eax, sizeof(PANE_INFO)
    lea edi, g_PaneArray
    add edi, eax
    
    ; Check if pane exists
    mov eax, [edi].PANE_INFO.dwPaneID
    test eax, eax
    jz @SetColorFailed
    
    ; Set color
    mov eax, dwColor
    mov [edi].PANE_INFO.dwCustomColor, eax
    
    mov eax, TRUE
    ret
    
@SetColorFailed:
    xor eax, eax
    ret
PANE_SETCOLOR endp

; ============================================================================
; PANE_GETCOUNT - Get total number of active panes
; Returns: Count (eax)
; ============================================================================
public PANE_GETCOUNT
PANE_GETCOUNT proc
    mov eax, g_dwPaneCount
    ret
PANE_GETCOUNT endp

; ============================================================================
; PANE_ENUMALL - Enumerate all panes
; Parameters: pCallback (pointer to callback proc: dwPaneID -> return TRUE to continue)
; Returns: TRUE if enumeration completed
; ============================================================================
public PANE_ENUMALL
PANE_ENUMALL proc pCallback:DWORD
    LOCAL i:DWORD
    LOCAL paneID:DWORD
    
    xor i, 0
    
@@EnumLoop:
    cmp i, 100
    jge @@EnumDone
    
    mov eax, i
    imul eax, sizeof(PANE_INFO)
    lea edi, g_PaneArray
    add edi, eax
    
    mov eax, [edi].PANE_INFO.dwPaneID
    test eax, eax
    jz @@EnumNext
    
    mov paneID, eax
    
    ; Call callback with pane ID
    mov eax, pCallback
    push paneID
    call eax
    add esp, 4
    
    test eax, eax
    jz @@EnumDone
    
@@EnumNext:
    inc i
    jmp @@EnumLoop
    
@@EnumDone:
    mov eax, TRUE
    ret
PANE_ENUMALL endp

end
