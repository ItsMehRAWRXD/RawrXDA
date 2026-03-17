; ============================================================================
; RawrXD Agentic IDE - Status Bar Implementation (Pure MASM)
; Multi-part status bar with ready/cursor/file info
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

.data
include constants.inc

extrn g_hInstance:DWORD
extrn g_hMainWindow:DWORD

; Status bar constants
SB_PART_STATUS      equ 0
SB_PART_CURSOR      equ 1
SB_PART_FILE        equ 2
SB_PART_COUNT       equ 3

; Data section
    szSimpleStatusClass db "msctls_statusbar32",0
    szReady         db "Ready",0
    szModified      db "Modified",0
    szNoFile        db "No file",0
    szCursorFmt     db "Ln %d, Col %d",0
    
    g_hStatusBar    dd 0
    g_nParts        dd SB_PART_COUNT

.data?
    dwStatusParts   dd SB_PART_COUNT dup(?)
    szStatusText    db 256 dup(?)
    szCursorText    db 64 dup(?)

.code

; Forward declarations
CreateStatusBar proto
UpdateStatusBar proto :DWORD
UpdateCursorPosition proto :DWORD, :DWORD
UpdateFileName proto :DWORD
ResizeStatusBar proto

; ============================================================================
; CreateStatusBar - Create multi-part status bar
; Returns: Status bar handle in eax
; ============================================================================
CreateStatusBar proc
    LOCAL rect:RECT
    
    ; Create status bar
    invoke CreateWindowEx, 0, addr szSimpleStatusClass, NULL,
           WS_CHILD or WS_VISIBLE or SBARS_SIZEGRIP,
           0, 0, 0, 0, g_hMainWindow, IDC_STATUSBAR, g_hInstance, NULL
    mov g_hStatusBar, eax
    
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Get client rectangle
    invoke GetClientRect, g_hMainWindow, addr rect
    
    ; Calculate part widths (right to left)
    mov eax, rect.right
    sub eax, 150                ; File name part: 150 pixels
    mov dwStatusParts[2], eax
    
    sub eax, 120                ; Cursor position part: 120 pixels  
    mov dwStatusParts[1], eax
    
    mov dwStatusParts[0], -1    ; Status part: rest of space
    
    ; Set parts
    invoke SendMessage, g_hStatusBar, SB_SETPARTS, SB_PART_COUNT, addr dwStatusParts
    
    ; Set initial text
    invoke SendMessage, g_hStatusBar, SB_SETTEXT, SB_PART_STATUS, addr szReady
    invoke SendMessage, g_hStatusBar, SB_SETTEXT, SB_PART_CURSOR, addr szCursorText
    invoke SendMessage, g_hStatusBar, SB_SETTEXT, SB_PART_FILE, addr szNoFile
    
    mov eax, g_hStatusBar
    ret
CreateStatusBar endp

; ============================================================================
; UpdateStatusBar - Update main status text
; ============================================================================
UpdateStatusBar proc pszText:DWORD
    .if g_hStatusBar != 0
        invoke SendMessage, g_hStatusBar, SB_SETTEXT, SB_PART_STATUS, pszText
    .endif
    ret
UpdateStatusBar endp

; ============================================================================
; UpdateCursorPosition - Update cursor position display
; ============================================================================
UpdateCursorPosition proc nLine:DWORD, nColumn:DWORD
    .if g_hStatusBar != 0
        invoke wsprintf, addr szCursorText, addr szCursorFmt, nLine, nColumn
        invoke SendMessage, g_hStatusBar, SB_SETTEXT, SB_PART_CURSOR, addr szCursorText
    .endif
    ret
UpdateCursorPosition endp

; ============================================================================
; UpdateFileName - Update current file name display
; ============================================================================
UpdateFileName proc pszFileName:DWORD
    LOCAL pszName:DWORD
    
    .if g_hStatusBar != 0
        .if pszFileName != 0 && BYTE PTR [pszFileName] != 0
            ; Find last backslash to get just filename
            invoke lstrlen, pszFileName
            mov edx, eax
            mov eax, pszFileName
            add eax, edx
            dec eax
            mov pszName, pszFileName  ; Default to full path
            
            .while eax >= pszFileName
                .if BYTE PTR [eax] == '\'
                    inc eax
                    mov pszName, eax
                    .break
                .endif
                dec eax
            .endw
            
            invoke SendMessage, g_hStatusBar, SB_SETTEXT, SB_PART_FILE, pszName
        .else
            invoke SendMessage, g_hStatusBar, SB_SETTEXT, SB_PART_FILE, addr szNoFile
        .endif
    .endif
    ret
UpdateFileName endp

; ============================================================================
; ResizeStatusBar - Resize status bar when window resizes
; ============================================================================
ResizeStatusBar proc
    LOCAL rect:RECT
    
    .if g_hStatusBar != 0
        ; Resize status bar
        invoke SendMessage, g_hStatusBar, WM_SIZE, 0, 0
        
        ; Recalculate parts
        invoke GetClientRect, g_hMainWindow, addr rect
        
        mov eax, rect.right
        sub eax, 150
        mov dwStatusParts[2], eax
        
        sub eax, 120
        mov dwStatusParts[1], eax
        
        mov dwStatusParts[0], -1
        
        ; Update parts
        invoke SendMessage, g_hStatusBar, SB_SETPARTS, SB_PART_COUNT, addr dwStatusParts
    .endif
    ret
ResizeStatusBar endp

; ============================================================================
; SetStatusText - Convenient function to set status text
; ============================================================================
SetStatusText proc pszText:DWORD
    call UpdateStatusBar, pszText
    ret
SetStatusText endp

; ============================================================================
; SetCursorInfo - Convenient function to set cursor info
; ============================================================================
SetCursorInfo proc nLine:DWORD, nCol:DWORD
    call UpdateCursorPosition, nLine, nCol
    ret
SetCursorInfo endp

; ============================================================================
; SetFileInfo - Convenient function to set file info
; ============================================================================
SetFileInfo proc pszFile:DWORD
    call UpdateFileName, pszFile
    ret
SetFileInfo endp

; ============================================================================
; Public interface
; ============================================================================
public CreateStatusBar
public UpdateStatusBar
public UpdateCursorPosition
public UpdateFileName
public ResizeStatusBar
public SetStatusText
public SetCursorInfo
public SetFileInfo

end