; ============================================================================
; ui_command_palette.asm
; Command Palette with fuzzy search (Ctrl+P style)
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\gdi32.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\gdi32.lib

; Constants
MAX_COMMANDS            equ 200
MAX_COMMAND_NAME        equ 128
MAX_RESULTS             equ 20

; Command types
CMD_TYPE_FILE           equ 0
CMD_TYPE_ACTION         equ 1
CMD_TYPE_SETTING        equ 2

; Structures
COMMAND_ENTRY struct
    szName          db MAX_COMMAND_NAME dup(?)
    dwType          dd ?
    pHandler        dd ?
    dwScore         dd ?
    bVisible        dd ?
COMMAND_ENTRY ends

PALETTE_STATE struct
    hWnd            dd ?
    hParent         dd ?
    hEditSearch     dd ?
    hListResults    dd ?
    hFont           dd ?
    dwResultCount   dd ?
    szSearch        db 256 dup(?)
PALETTE_STATE ends

.data
    g_Commands          COMMAND_ENTRY MAX_COMMANDS dup(<>)
    g_dwCommandCount    dd 0
    g_Palette           PALETTE_STATE <>
    
    szPaletteClass      db "RawrXDCommandPalette",0
    szTitle             db "Command Palette",0
    szPlaceholder       db "Type to search...",0
    
    ; Built-in commands
    szCmdOpenFile       db "Open File...",0
    szCmdSaveFile       db "Save File",0
    szCmdSettings       db "Open Settings",0
    szCmdBuild          db "Build Project",0
    szCmdRun            db "Run Project",0
    szCmdDebug          db "Start Debugging",0
    szCmdTogglePerfHUD  db "Toggle Performance HUD",0
    szCmdExitIDE        db "Exit IDE",0

.data?
    g_hInstance         dd ?

.code

; ---------------------------------------------------------------------------
; CommandPalette_Init - initialize command palette system
; ---------------------------------------------------------------------------
CommandPalette_Init proc hInst:DWORD
    mov g_hInstance, hInst
    xor eax, eax
    mov g_dwCommandCount, eax
    
    ; Register built-in commands
    call CommandPalette_RegisterCommand, addr szCmdOpenFile, CMD_TYPE_ACTION, NULL
    call CommandPalette_RegisterCommand, addr szCmdSaveFile, CMD_TYPE_ACTION, NULL
    call CommandPalette_RegisterCommand, addr szCmdSettings, CMD_TYPE_ACTION, NULL
    call CommandPalette_RegisterCommand, addr szCmdBuild, CMD_TYPE_ACTION, NULL
    call CommandPalette_RegisterCommand, addr szCmdRun, CMD_TYPE_ACTION, NULL
    call CommandPalette_RegisterCommand, addr szCmdDebug, CMD_TYPE_ACTION, NULL
    call CommandPalette_RegisterCommand, addr szCmdTogglePerfHUD, CMD_TYPE_SETTING, NULL
    call CommandPalette_RegisterCommand, addr szCmdExitIDE, CMD_TYPE_ACTION, NULL
    
    mov eax, TRUE
    ret
CommandPalette_Init endp

; ---------------------------------------------------------------------------
; CommandPalette_RegisterCommand - register a command
; ---------------------------------------------------------------------------
CommandPalette_RegisterCommand proc pszName:DWORD, dwType:DWORD, pHandler:DWORD
    LOCAL dwIdx:DWORD
    
    mov eax, g_dwCommandCount
    cmp eax, MAX_COMMANDS
    jge @Full
    
    mov dwIdx, eax
    mov eax, dwIdx
    mov ecx, sizeof COMMAND_ENTRY
    mul ecx
    lea edx, g_Commands
    add eax, edx
    
    invoke lstrcpyn, addr [eax].COMMAND_ENTRY.szName, pszName, MAX_COMMAND_NAME
    mov ecx, dwType
    mov [eax].COMMAND_ENTRY.dwType, ecx
    mov ecx, pHandler
    mov [eax].COMMAND_ENTRY.pHandler, ecx
    mov [eax].COMMAND_ENTRY.dwScore, 0
    mov [eax].COMMAND_ENTRY.bVisible, TRUE
    
    inc g_dwCommandCount
    mov eax, TRUE
    ret
@Full:
    xor eax, eax
    ret
CommandPalette_RegisterCommand endp

; ---------------------------------------------------------------------------
; CommandPalette_Show - show command palette
; ---------------------------------------------------------------------------
CommandPalette_Show proc hParent:DWORD
    LOCAL wc:WNDCLASSEX
    LOCAL rect:RECT
    LOCAL x:DWORD
    LOCAL y:DWORD
    LOCAL cx:DWORD
    LOCAL cy:DWORD
    
    .IF g_Palette.hWnd != 0
        invoke SetForegroundWindow, g_Palette.hWnd
        ret
    .ENDIF
    
    ; Register window class
    mov wc.cbSize, sizeof WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    mov wc.lpfnWndProc, offset CommandPalette_WndProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov wc.hInstance, g_hInstance
    invoke LoadCursor, NULL, IDC_ARROW
    mov wc.hCursor, eax
    invoke GetStockObject, WHITE_BRUSH
    mov wc.hbrBackground, eax
    mov wc.lpszMenuName, NULL
    mov wc.lpszClassName, offset szPaletteClass
    mov wc.hIcon, NULL
    mov wc.hIconSm, NULL
    invoke RegisterClassEx, addr wc
    
    ; Center on parent
    invoke GetWindowRect, hParent, addr rect
    mov eax, rect.right
    sub eax, rect.left
    shr eax, 1
    add eax, rect.left
    sub eax, 250
    mov x, eax
    
    mov eax, rect.bottom
    sub eax, rect.top
    shr eax, 2
    add eax, rect.top
    mov y, eax
    
    mov cx, 500
    mov cy, 400
    
    invoke CreateWindowEx, WS_EX_TOPMOST or WS_EX_TOOLWINDOW, \
        offset szPaletteClass, offset szTitle, \
        WS_POPUP or WS_BORDER or WS_VISIBLE, \
        x, y, cx, cy, hParent, NULL, g_hInstance, NULL
    
    mov g_Palette.hWnd, eax
    mov g_Palette.hParent, hParent
    
    ; Create search box
    invoke CreateWindowEx, WS_EX_CLIENTEDGE, CSTR("EDIT"), NULL, \
        WS_CHILD or WS_VISIBLE or ES_LEFT or ES_AUTOHSCROLL, \
        8, 8, 484, 24, g_Palette.hWnd, 1001, g_hInstance, NULL
    mov g_Palette.hEditSearch, eax
    
    ; Create results listbox
    invoke CreateWindowEx, 0, CSTR("LISTBOX"), NULL, \
        WS_CHILD or WS_VISIBLE or WS_VSCROLL or LBS_NOTIFY or LBS_NOINTEGRALHEIGHT, \
        8, 40, 484, 352, g_Palette.hWnd, 1002, g_hInstance, NULL
    mov g_Palette.hListResults, eax
    
    ; Create font
    invoke CreateFont, -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, \
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, \
        DEFAULT_PITCH or FF_DONTCARE, CSTR("Segoe UI")
    mov g_Palette.hFont, eax
    invoke SendMessage, g_Palette.hEditSearch, WM_SETFONT, eax, TRUE
    invoke SendMessage, g_Palette.hListResults, WM_SETFONT, eax, TRUE
    
    ; Populate initial results
    call CommandPalette_UpdateResults
    
    ; Focus search box
    invoke SetFocus, g_Palette.hEditSearch
    
    ret
CommandPalette_Show endp

; ---------------------------------------------------------------------------
; CommandPalette_UpdateResults - update results based on search
; ---------------------------------------------------------------------------
CommandPalette_UpdateResults proc uses ebx esi edi
    LOCAL i:DWORD
    LOCAL pCmd:DWORD
    
    ; Clear listbox
    invoke SendMessage, g_Palette.hListResults, LB_RESETCONTENT, 0, 0
    
    ; Get search text
    invoke GetWindowText, g_Palette.hEditSearch, addr g_Palette.szSearch, 255
    invoke lstrlen, addr g_Palette.szSearch
    
    ; If empty, show all
    .IF eax == 0
        xor ecx, ecx
        mov i, ecx
@@ShowAll:
        cmp i, g_dwCommandCount
        jge @@Done
        
        mov eax, i
        mov ecx, sizeof COMMAND_ENTRY
        mul ecx
        lea edx, g_Commands
        add eax, edx
        mov pCmd, eax
        
        invoke SendMessage, g_Palette.hListResults, LB_ADDSTRING, 0, addr [eax].COMMAND_ENTRY.szName
        
        inc i
        jmp @@ShowAll
    .ELSE
        ; Fuzzy match
        xor ecx, ecx
        mov i, ecx
@@Match:
        cmp i, g_dwCommandCount
        jge @@Done
        
        mov eax, i
        mov ecx, sizeof COMMAND_ENTRY
        mul ecx
        lea edx, g_Commands
        add eax, edx
        mov pCmd, eax
        
        ; Simple substring match (fuzzy scoring would go here)
        invoke lstrcmpi, addr g_Palette.szSearch, addr [eax].COMMAND_ENTRY.szName
        .IF eax == 0
            invoke SendMessage, g_Palette.hListResults, LB_ADDSTRING, 0, pCmd
            invoke SendMessage, g_Palette.hListResults, LB_ADDSTRING, 0, addr [pCmd].COMMAND_ENTRY.szName
        .ENDIF
        
        inc i
        jmp @@Match
    .ENDIF
    
@@Done:
    ; Select first result
    invoke SendMessage, g_Palette.hListResults, LB_SETCURSEL, 0, 0
    ret
CommandPalette_UpdateResults endp

; ---------------------------------------------------------------------------
; CommandPalette_ExecuteSelected - execute selected command
; ---------------------------------------------------------------------------
CommandPalette_ExecuteSelected proc
    LOCAL idx:DWORD
    LOCAL pCmd:DWORD
    
    invoke SendMessage, g_Palette.hListResults, LB_GETCURSEL, 0, 0
    .IF eax == LB_ERR
        ret
    .ENDIF
    mov idx, eax
    
    ; Get command
    cmp idx, g_dwCommandCount
    jge @Fail
    
    mov eax, idx
    mov ecx, sizeof COMMAND_ENTRY
    mul ecx
    lea edx, g_Commands
    add eax, edx
    mov pCmd, eax
    
    ; Execute handler if set
    mov eax, [eax].COMMAND_ENTRY.pHandler
    test eax, eax
    jz @NoHandler
    call eax
    
@NoHandler:
    ; Close palette
    invoke DestroyWindow, g_Palette.hWnd
    mov g_Palette.hWnd, 0
    
    mov eax, TRUE
    ret
@Fail:
    xor eax, eax
    ret
CommandPalette_ExecuteSelected endp

; ---------------------------------------------------------------------------
; CommandPalette_WndProc - window procedure
; ---------------------------------------------------------------------------
CommandPalette_WndProc proc hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    
    .IF uMsg == WM_COMMAND
        mov eax, wParam
        shr eax, 16
        .IF ax == EN_CHANGE
            call CommandPalette_UpdateResults
            xor eax, eax
            ret
        .ELSEIF ax == LBN_DBLCLK
            call CommandPalette_ExecuteSelected
            xor eax, eax
            ret
        .ENDIF
        
    .ELSEIF uMsg == WM_KEYDOWN
        .IF wParam == VK_ESCAPE
            invoke DestroyWindow, hWnd
            mov g_Palette.hWnd, 0
            xor eax, eax
            ret
        .ELSEIF wParam == VK_RETURN
            call CommandPalette_ExecuteSelected
            xor eax, eax
            ret
        .ENDIF
        
    .ELSEIF uMsg == WM_ACTIVATE
        .IF wParam == WA_INACTIVE
            invoke DestroyWindow, hWnd
            mov g_Palette.hWnd, 0
        .ENDIF
        xor eax, eax
        ret
        
    .ELSEIF uMsg == WM_DESTROY
        .IF g_Palette.hFont != 0
            invoke DeleteObject, g_Palette.hFont
            mov g_Palette.hFont, 0
        .ENDIF
        mov g_Palette.hWnd, 0
        xor eax, eax
        ret
    .ENDIF
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
CommandPalette_WndProc endp

; ---------------------------------------------------------------------------
; Exports
; ---------------------------------------------------------------------------
public CommandPalette_Init
public CommandPalette_RegisterCommand
public CommandPalette_Show

end
