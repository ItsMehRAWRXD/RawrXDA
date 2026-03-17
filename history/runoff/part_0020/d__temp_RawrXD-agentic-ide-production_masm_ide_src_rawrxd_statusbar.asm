;======================================================================
; RawrXD IDE - Status Bar Component
; Status text display, progress indicator, mode display, diagnostics
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
g_hStatusBar            DQ ?
g_statusBarHeight       DQ 20

; Status bar parts
PART_FILE_INFO          EQU 0
PART_POSITION           EQU 1
PART_MODE               EQU 2
PART_PROGRESS           EQU 3

; Current status info
g_currentFile[260]      DB 260 DUP(0)
g_currentLine           DQ 1
g_currentCol            DQ 1
g_buildMode[32]         DB "READY", 0
g_progressValue         DQ 0
g_statusText[256]       DB 256 DUP(0)

.CODE

;----------------------------------------------------------------------
; RawrXD_StatusBar_Create - Create status bar with parts
;----------------------------------------------------------------------
RawrXD_StatusBar_Create PROC hParent:QWORD
    LOCAL hWnd:QWORD
    
    ; Create status bar control
    INVOKE CreateWindowEx,
        0,
        "msctls_statusbar32",
        NULL,
        WS_CHILD OR WS_VISIBLE OR SBARS_SIZEGRIP,
        0, 0, 0, g_statusBarHeight,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hStatusBar, rax
    mov hWnd, rax
    test rax, rax
    jz @@fail
    
    ; Initialize status bar parts
    INVOKE RawrXD_StatusBar_SetParts, hWnd
    
    xor eax, eax
    ret
    
@@fail:
    mov eax, -1
    ret
    
RawrXD_StatusBar_Create ENDP

;----------------------------------------------------------------------
; RawrXD_StatusBar_SetParts - Configure status bar parts
;----------------------------------------------------------------------
RawrXD_StatusBar_SetParts PROC hStatusBar:QWORD
    LOCAL partRights[4]:DWORD
    LOCAL totalWidth:QWORD
    
    ; Set part widths
    ; Widths: FileInfo (200px), Position (100px), Mode (50px), Progress (stretch)
    
    ; Get total window width (for now use fixed width)
    mov totalWidth, 800
    
    ; Calculate part boundaries
    mov eax, totalWidth
    sub eax, 200    ; Progress takes 200px
    mov partRights[0], eax  ; FileInfo ends at 600
    
    mov eax, totalWidth
    sub eax, 150    ; Mode takes 150px
    mov partRights[1], eax  ; Position ends at 650
    
    mov eax, totalWidth
    sub eax, 50     ; Mode takes 50px
    mov partRights[2], eax  ; Mode ends at 750
    
    mov eax, -1     ; Progress stretches to end
    mov partRights[3], eax
    
    ; Set parts
    INVOKE SendMessage, hStatusBar, SB_SETPARTS, 4, ADDR partRights
    
    ret
    
RawrXD_StatusBar_SetParts ENDP

;----------------------------------------------------------------------
; RawrXD_StatusBar_SetText - Update status bar text for a part
;----------------------------------------------------------------------
RawrXD_StatusBar_SetText PROC part:QWORD, pszText:QWORD, flags:QWORD
    ; flags: 0 = left, SBT_POPOUT = raised, SBT_NOBORDERS = no border
    
    ; Create message parameter
    mov rax, part
    shl rax, 32
    or rax, flags
    
    INVOKE SendMessage, g_hStatusBar, SB_SETTEXT, rax, pszText
    
    ret
    
RawrXD_StatusBar_SetText ENDP

;----------------------------------------------------------------------
; RawrXD_StatusBar_SetFileInfo - Update file information display
;----------------------------------------------------------------------
RawrXD_StatusBar_SetFileInfo PROC pszFilename:QWORD, pszEncoding:QWORD, lineEnding:QWORD
    LOCAL szText[128]:BYTE
    
    ; Copy filename
    INVOKE lstrcpyA, ADDR g_currentFile, pszFilename
    
    ; Build status text
    INVOKE lstrcpyA, ADDR szText, pszFilename
    INVOKE lstrcatA, ADDR szText, " - "
    INVOKE lstrcatA, ADDR szText, pszEncoding
    
    ; Add line ending info
    cmp lineEnding, 0
    je @@crlf
    cmp lineEnding, 1
    je @@lf
    
    ; Default CR
    INVOKE lstrcatA, ADDR szText, " (CR)"
    jmp @@set_text
    
@@crlf:
    INVOKE lstrcatA, ADDR szText, " (CRLF)"
    jmp @@set_text
    
@@lf:
    INVOKE lstrcatA, ADDR szText, " (LF)"
    
@@set_text:
    INVOKE RawrXD_StatusBar_SetText, PART_FILE_INFO, ADDR szText, 0
    
    ret
    
RawrXD_StatusBar_SetFileInfo ENDP

;----------------------------------------------------------------------
; RawrXD_StatusBar_SetPosition - Update cursor position display
;----------------------------------------------------------------------
RawrXD_StatusBar_SetPosition PROC line:QWORD, col:QWORD
    LOCAL szText[32]:BYTE
    LOCAL intStr[16]:BYTE
    
    ; Store current position
    mov g_currentLine, line
    mov g_currentCol, col
    
    ; Build text "Line 123, Col 456"
    INVOKE lstrcpyA, ADDR szText, "Ln "
    
    ; Convert line number to string
    INVOKE RawrXD_Util_IntToStr, line, ADDR intStr
    INVOKE lstrcatA, ADDR szText, ADDR intStr
    
    INVOKE lstrcatA, ADDR szText, ", Col "
    
    ; Convert column number
    INVOKE RawrXD_Util_IntToStr, col, ADDR intStr
    INVOKE lstrcatA, ADDR szText, ADDR intStr
    
    ; Set status text
    INVOKE RawrXD_StatusBar_SetText, PART_POSITION, ADDR szText, 0
    
    ret
    
RawrXD_StatusBar_SetPosition ENDP

;----------------------------------------------------------------------
; RawrXD_StatusBar_SetMode - Update mode/status display
;----------------------------------------------------------------------
RawrXD_StatusBar_SetMode PROC pszMode:QWORD
    ; Copy mode string
    INVOKE lstrcpyA, ADDR g_buildMode, pszMode
    
    ; Set status text
    INVOKE RawrXD_StatusBar_SetText, PART_MODE, ADDR g_buildMode, 0
    
    ret
    
RawrXD_StatusBar_SetMode ENDP

;----------------------------------------------------------------------
; RawrXD_StatusBar_SetReady - Set status to "READY"
;----------------------------------------------------------------------
RawrXD_StatusBar_SetReady PROC
    INVOKE RawrXD_StatusBar_SetMode, OFFSET szReady
    INVOKE RawrXD_StatusBar_ClearProgress
    ret
RawrXD_StatusBar_SetReady ENDP

;----------------------------------------------------------------------
; RawrXD_StatusBar_SetBuilding - Set status to "BUILDING..."
;----------------------------------------------------------------------
RawrXD_StatusBar_SetBuilding PROC
    INVOKE RawrXD_StatusBar_SetMode, OFFSET szBuilding
    ret
RawrXD_StatusBar_SetBuilding ENDP

;----------------------------------------------------------------------
; RawrXD_StatusBar_SetError - Set status to error
;----------------------------------------------------------------------
RawrXD_StatusBar_SetError PROC pszError:QWORD
    LOCAL szText[64]:BYTE
    
    INVOKE lstrcpyA, ADDR szText, "ERROR: "
    INVOKE lstrcatA, ADDR szText, pszError
    
    INVOKE RawrXD_StatusBar_SetMode, ADDR szText
    
    ret
    
RawrXD_StatusBar_SetError ENDP

;----------------------------------------------------------------------
; RawrXD_StatusBar_ShowProgress - Display progress indicator
;----------------------------------------------------------------------
RawrXD_StatusBar_ShowProgress PROC percent:QWORD
    LOCAL szText[32]:BYTE
    LOCAL progressBar[32]:BYTE
    LOCAL idx:QWORD
    LOCAL filled:QWORD
    
    ; Store progress value
    mov g_progressValue, percent
    
    ; Build progress bar visualization
    mov idx, 0
    mov filled, 0
    
    ; Calculate number of filled sections (0-20)
    mov rax, percent
    imul rax, 20
    mov rcx, 100
    xor rdx, rdx
    div rcx
    mov filled, rax
    
    ; Build bar string
    INVOKE lstrcpyA, ADDR progressBar, "["
    
    ; Add filled sections
    mov idx, 0
@@fill_loop:
    cmp idx, filled
    jge @@empty_loop
    INVOKE lstrcatA, ADDR progressBar, "="
    inc idx
    jmp @@fill_loop
    
@@empty_loop:
    ; Add empty sections
@@empty_continue:
    cmp idx, 20
    jge @@bar_complete
    INVOKE lstrcatA, ADDR progressBar, " "
    inc idx
    jmp @@empty_continue
    
@@bar_complete:
    INVOKE lstrcatA, ADDR progressBar, "] "
    
    ; Add percentage
    INVOKE RawrXD_Util_IntToStr, percent, ADDR szText
    INVOKE lstrcatA, ADDR progressBar, ADDR szText
    INVOKE lstrcatA, ADDR progressBar, "%"
    
    ; Set text
    INVOKE RawrXD_StatusBar_SetText, PART_PROGRESS, ADDR progressBar, 0
    
    ret
    
RawrXD_StatusBar_ShowProgress ENDP

;----------------------------------------------------------------------
; RawrXD_StatusBar_ClearProgress - Clear progress display
;----------------------------------------------------------------------
RawrXD_StatusBar_ClearProgress PROC
    mov g_progressValue, 0
    INVOKE RawrXD_StatusBar_SetText, PART_PROGRESS, OFFSET szEmpty, 0
    ret
RawrXD_StatusBar_ClearProgress ENDP

;----------------------------------------------------------------------
; RawrXD_StatusBar_GetHeight - Get status bar height
;----------------------------------------------------------------------
RawrXD_StatusBar_GetHeight PROC
    mov rax, g_statusBarHeight
    ret
RawrXD_StatusBar_GetHeight ENDP

;----------------------------------------------------------------------
; RawrXD_StatusBar_Resize - Resize status bar on parent resize
;----------------------------------------------------------------------
RawrXD_StatusBar_Resize PROC hParent:QWORD, cx:QWORD, cy:QWORD
    ; Status bar auto-sizes, but we update parts
    INVOKE RawrXD_StatusBar_SetParts, g_hStatusBar
    
    ret
    
RawrXD_StatusBar_Resize ENDP

; String literals
szReady                 DB "READY", 0
szBuilding              DB "BUILDING...", 0
szEmpty                 DB "", 0

END
