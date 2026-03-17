; ============================================================================
; PANE_SERIALIZATION.ASM - Pane configuration persistence
; Save and load pane layouts, sizes, and positions
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; ============================================================================
; SERIALIZATION CONSTANTS
; ============================================================================

PANE_CONFIG_FILE_VERSION equ 100h

PANE_CONFIG_HEADER struct
    dwVersion           dd ?    ; File version
    dwPaneCount         dd ?    ; Number of panes
    reserved            dd 6 dup (?)
PANE_CONFIG_HEADER ends

PANE_CONFIG_ENTRY struct
    dwPaneID            dd ?
    dwType              dd ?
    dwState             dd ?
    dwDockPosition      dd ?
    x                   dd ?
    y                   dd ?
    width               dd ?
    height              dd ?
    zOrder              dd ?
    pszTitle            db 256 dup (?)
    dwCustomColor       dd ?
PANE_CONFIG_ENTRY ends

; ============================================================================
; PANE_SERIALIZATION_SAVE - Save all panes to configuration file
; Parameters: pszFilename
; Returns: TRUE if successful
; ============================================================================
public PANE_SERIALIZATION_SAVE
PANE_SERIALIZATION_SAVE proc pszFilename:DWORD
    LOCAL hFile:HANDLE
    LOCAL dwBytesWritten:DWORD
    LOCAL header:PANE_CONFIG_HEADER
    LOCAL entry:PANE_CONFIG_ENTRY
    LOCAL i:DWORD
    
    ; Create file
    invoke CreateFile, pszFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @SaveFailed
    mov hFile, eax
    
    ; Prepare header
    mov header.dwVersion, PANE_CONFIG_FILE_VERSION
    invoke PANE_GETCOUNT
    mov header.dwPaneCount, eax
    
    ; Write header
    invoke WriteFile, hFile, addr header, sizeof(PANE_CONFIG_HEADER), addr dwBytesWritten, NULL
    
    ; Save each pane
    xor i, 0
@@SaveLoop:
    cmp i, 100
    jge @@SaveDone
    
    mov eax, i
    inc eax
    
    ; Get pane info
    invoke PANE_GETINFO, eax, addr entry
    test eax, eax
    jz @@SkipPane
    
    ; Write entry
    invoke WriteFile, hFile, addr entry, sizeof(PANE_CONFIG_ENTRY), addr dwBytesWritten, NULL
    
@@SkipPane:
    inc i
    jmp @@SaveLoop
    
@@SaveDone:
    invoke CloseHandle, hFile
    mov eax, TRUE
    ret
    
@SaveFailed:
    xor eax, eax
    ret
PANE_SERIALIZATION_SAVE endp

; ============================================================================
; PANE_SERIALIZATION_LOAD - Load panes from configuration file
; Parameters: pszFilename
; Returns: TRUE if successful
; ============================================================================
public PANE_SERIALIZATION_LOAD
PANE_SERIALIZATION_LOAD proc pszFilename:DWORD
    LOCAL hFile:HANDLE
    LOCAL dwBytesRead:DWORD
    LOCAL header:PANE_CONFIG_HEADER
    LOCAL entry:PANE_CONFIG_ENTRY
    LOCAL i:DWORD
    
    ; Open file
    invoke CreateFile, pszFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @LoadFailed
    mov hFile, eax
    
    ; Read header
    invoke ReadFile, hFile, addr header, sizeof(PANE_CONFIG_HEADER), addr dwBytesRead, NULL
    cmp dwBytesRead, sizeof(PANE_CONFIG_HEADER)
    jne @LoadFailed
    
    ; Check version
    cmp header.dwVersion, PANE_CONFIG_FILE_VERSION
    jne @LoadFailed
    
    ; Load each pane
    xor i, 0
@@LoadLoop:
    cmp i, header.dwPaneCount
    jge @@LoadDone
    
    ; Read entry
    invoke ReadFile, hFile, addr entry, sizeof(PANE_CONFIG_ENTRY), addr dwBytesRead, NULL
    cmp dwBytesRead, sizeof(PANE_CONFIG_ENTRY)
    jne @LoadFailed
    
    ; Create pane with loaded settings
    invoke PANE_CREATE, addr entry.pszTitle, entry.dwType, entry.width, entry.height, entry.dwDockPosition
    
    ; Set pane properties
    invoke PANE_SETPOSITION, eax, entry.x, entry.y, entry.width, entry.height
    invoke PANE_SETSTATE, eax, entry.dwState
    invoke PANE_SETZORDER, eax, entry.zOrder
    invoke PANE_SETCOLOR, eax, entry.dwCustomColor
    
    inc i
    jmp @@LoadLoop
    
@@LoadDone:
    invoke CloseHandle, hFile
    mov eax, TRUE
    ret
    
@LoadFailed:
    invoke CloseHandle, hFile
    xor eax, eax
    ret
PANE_SERIALIZATION_LOAD endp

; ============================================================================
; PANE_SERIALIZATION_RESET - Reset to default layout
; Returns: TRUE if successful
; ============================================================================
public PANE_SERIALIZATION_RESET
PANE_SERIALIZATION_RESET proc
    ; Close all panes
    xor ecx, ecx
@@CloseLoop:
    cmp ecx, 100
    jge @@CloseDone
    
    mov eax, ecx
    inc eax
    invoke PANE_DESTROY, eax
    
    inc ecx
    jmp @@CloseLoop
    
@@CloseDone:
    ; Reinitialize system
    invoke PANE_SYSTEM_INIT
    
    ; Apply default layout (VS Code)
    invoke LAYOUT_APPLY_VSCODE, hMainWindow, 1200, 800
    
    mov eax, TRUE
    ret
PANE_SERIALIZATION_RESET endp

end
