; dashboard.asm
; Real-Time Error Reporting Dashboard for RawrXD IDE
; Phase 5 Implementation

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; ==================== CONSTANTS ====================
ID_TIMER        equ 1
ID_LISTBOX      equ 100
ID_FILTER_COMBO equ 101
LOG_FILE_NAME   equ "rawrxd.log"
MAX_LINE_LENGTH equ 4096

; ==================== DATA SECTION ====================
.data
hMainWnd        HWND 0
hListBox        HWND 0
hFilterCombo    HWND 0
hLogFile        HANDLE 0
lastFilePos     QWORD 0
filterLevel     DWORD LOG_LEVEL_ERROR

; Log level strings
levelStrings    db "All",0
                db "INFO",0
                db "WARNING",0
                db "ERROR",0
                db "FATAL",0

; ==================== CODE SECTION ====================
.code

; ----------------------------------------------------
; Window procedure
; ----------------------------------------------------
WndProc proc hWnd:HWND, uMsg:UINT, wParam:WPARAM, lParam:LPARAM
    .IF uMsg == WM_CREATE
        invoke CreateListBox
        invoke CreateFilterCombo
        invoke SetTimer, hWnd, ID_TIMER, 1000, NULL
        invoke OpenLogFile
        ret
    .ELSEIF uMsg == WM_TIMER
        invoke UpdateDashboard
        ret
    .ELSEIF uMsg == WM_COMMAND
        .IF wParam == ID_FILTER_COMBO && lParam == CBN_SELCHANGE
            invoke SendMessage, hFilterCombo, CB_GETCURSEL, 0, 0
            .IF eax == 0
                mov filterLevel, 0
            .ELSE
                mov filterLevel, eax
            .ENDIF
            invoke ClearListBox
            invoke UpdateDashboard
        .ENDIF
        ret
    .ELSEIF uMsg == WM_CLOSE
        invoke KillTimer, hWnd, ID_TIMER
        invoke CloseLogFile
        invoke DestroyWindow, hWnd
        ret
    .ELSEIF uMsg == WM_DESTROY
        invoke PostQuitMessage, 0
        ret
    .ENDIF
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
WndProc endp

; ----------------------------------------------------
; Create list box for log display
; ----------------------------------------------------
CreateListBox proc
    invoke CreateWindowEx, WS_EX_CLIENTEDGE, $"LISTBOX", NULL,
        WS_CHILD or WS_VISIBLE or WS_VSCROLL or LBS_NOINTEGRALHEIGHT,
        10, 40, 580, 300, hMainWnd, ID_LISTBOX, NULL, NULL
    mov hListBox, eax
    ret
CreateListBox endp

; ----------------------------------------------------
; Create filter combo box
; ----------------------------------------------------
CreateFilterCombo proc
    invoke CreateWindow, $"COMBOBOX", NULL,
        WS_CHILD or WS_VISIBLE or CBS_DROPDOWNLIST,
        10, 10, 150, 200, hMainWnd, ID_FILTER_COMBO, NULL, NULL
    mov hFilterCombo, eax
    
    ; Add filter options
    invoke SendMessage, hFilterCombo, CB_ADDSTRING, 0, offset levelStrings
    invoke SendMessage, hFilterCombo, CB_ADDSTRING, 0, offset levelStrings+4
    invoke SendMessage, hFilterCombo, CB_ADDSTRING, 0, offset levelStrings+11
    invoke SendMessage, hFilterCombo, CB_ADDSTRING, 0, offset levelStrings+20
    invoke SendMessage, hFilterCombo, CB_ADDSTRING, 0, offset levelStrings+27
    invoke SendMessage, hFilterCombo, CB_SETCURSEL, 3, 0
    ret
CreateFilterCombo endp

; ----------------------------------------------------
; Open log file for monitoring
; ----------------------------------------------------
OpenLogFile proc
    invoke CreateFile, addr LOG_FILE_NAME, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    .IF eax != INVALID_HANDLE_VALUE
        mov hLogFile, eax
        invoke GetFileSizeEx, hLogFile, addr lastFilePos
    .ENDIF
    ret
OpenLogFile endp

; ----------------------------------------------------
; Close log file
; ----------------------------------------------------
CloseLogFile proc
    .IF hLogFile != 0
        invoke CloseHandle, hLogFile
        mov hLogFile, 0
    .ENDIF
    ret
CloseLogFile endp

; ----------------------------------------------------
; Update dashboard with new log entries
; ----------------------------------------------------
UpdateDashboard proc
    LOCAL bytesRead:DWORD
    LOCAL buffer[MAX_LINE_LENGTH]:BYTE
    LOCAL lineStart:DWORD
    LOCAL lineEnd:DWORD
    LOCAL level:DWORD
    
    .IF hLogFile == 0
        ret
    .ENDIF

    ; Seek to last position
    invoke SetFilePointerEx, hLogFile, lastFilePos, NULL, FILE_BEGIN
    
    ; Read new data
    invoke ReadFile, hLogFile, addr buffer, MAX_LINE_LENGTH-1, addr bytesRead, NULL
    .IF !eax || !bytesRead
        ret
    .ENDIF
    mov byte ptr [buffer+bytesRead], 0

    ; Update last position
    invoke SetFilePointerEx, hLogFile, lastFilePos, addr lastFilePos, FILE_CURRENT

    ; Parse and add new lines
    mov esi, offset buffer
    mov lineStart, 0
    
    .WHILE byte ptr [esi]
        .IF byte ptr [esi] == 10 || byte ptr [esi] == 13
            mov byte ptr [esi], 0
            .IF lineStart < esi
                ; Parse log level
                invoke ParseLogLevel, addr buffer[lineStart]
                mov level, eax
                
                ; Check filter
                .IF filterLevel == 0 || level == filterLevel
                    invoke SendMessage, hListBox, LB_ADDSTRING, 0, addr buffer[lineStart]
                .ENDIF
            .ENDIF
            mov lineStart, esi+1
        .ENDIF
        inc esi
    .ENDW
    ret
UpdateDashboard endp

; ----------------------------------------------------
; Parse log level from entry
; Input:  esi = pointer to log line
; Output: eax = log level (1-4)
; ----------------------------------------------------
ParseLogLevel proc line:DWORD
    LOCAL levelStr[16]:BYTE
    
    invoke lstrcpy, addr levelStr, line
    invoke lstrchr, addr levelStr, '['
    .IF eax
        inc eax
        invoke lstrchr, eax, ']'
        .IF eax
            mov byte ptr [eax], 0
            invoke lstrcmp, addr levelStr+1, $"INFO"
            .IF eax == 0
                mov eax, LOG_LEVEL_INFO
                ret
            .ENDIF
            invoke lstrcmp, addr levelStr+1, $"WARNING"
            .IF eax == 0
                mov eax, LOG_LEVEL_WARNING
                ret
            .ENDIF
            invoke lstrcmp, addr levelStr+1, $"ERROR"
            .IF eax == 0
                mov eax, LOG_LEVEL_ERROR
                ret
            .ENDIF
            invoke lstrcmp, addr levelStr+1, $"FATAL"
            .IF eax == 0
                mov eax, LOG_LEVEL_FATAL
                ret
            .ENDIF
        .ENDIF
    .ENDIF
    xor eax, eax
    ret
ParseLogLevel endp

; ----------------------------------------------------
; Clear list box contents
; ----------------------------------------------------
ClearListBox proc
    invoke SendMessage, hListBox, LB_RESETCONTENT, 0, 0
    ret
ClearListBox endp

; ----------------------------------------------------
; Main entry point
; ----------------------------------------------------
WinMain proc
    LOCAL wc:WNDCLASSEX
    LOCAL msg:MSG
    LOCAL hWnd:HWND

    ; Register window class
    mov wc.cbSize, SIZEOF WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    mov wc.lpfnWndProc, offset WndProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov wc.hInstance, NULL
    mov wc.hIcon, NULL
    mov wc.hCursor, NULL
    mov wc.hbrBackground, COLOR_WINDOW+1
    mov wc.lpszMenuName, NULL
    mov wc.lpszClassName, offset $"ErrorDashboard"
    mov wc.hIconSm, NULL
    invoke RegisterClassEx, addr wc

    ; Create window
    invoke CreateWindowEx, 0, offset $"ErrorDashboard", $"RawrXD Error Dashboard",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 600, 360, NULL, NULL, NULL, NULL
    mov hWnd, eax
    mov hMainWnd, eax
    invoke ShowWindow, hWnd, SW_SHOW
    invoke UpdateWindow, hWnd

    ; Message loop
    .WHILE TRUE
        invoke GetMessage, addr msg, NULL, 0, 0
        .BREAK .IF eax == 0
        invoke TranslateMessage, addr msg
        invoke DispatchMessage, addr msg
    .ENDW
    mov eax, msg.wParam
    ret
WinMain endp

end WinMain