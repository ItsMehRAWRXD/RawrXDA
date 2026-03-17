;======================================================================
; RawrXD IDE - Output Panel Component
; Build output, logs, diagnostics with syntax coloring
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
g_hOutputPanel          DQ ?
g_hOutputList           DQ ?
g_outputHeight          DQ 150

; Output item type
OUTPUT_INFO             EQU 0
OUTPUT_WARNING          EQU 1
OUTPUT_ERROR            EQU 2
OUTPUT_BUILD            EQU 3

; Log buffer
g_outputBuffer[4096]    DB 4096 DUP(0)
g_outputPos             DQ 0
g_outputLineCount       DQ 0
g_maxOutputLines        DQ 1000

.CODE

;----------------------------------------------------------------------
; RawrXD_Output_Create - Create output panel with listbox
;----------------------------------------------------------------------
RawrXD_Output_Create PROC hParent:QWORD, x:QWORD, y:QWORD, cx:QWORD, cy:QWORD
    LOCAL hWnd:QWORD
    
    ; Create listbox for output display
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "LISTBOX",
        NULL,
        WS_CHILD OR WS_VISIBLE OR LBS_NOSEL OR LBS_HASSTRINGS OR 
        LBS_OWNERDRAWFIXED OR WS_VSCROLL OR WS_HSCROLL,
        x, y, cx, cy,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hOutputPanel, rax
    mov g_hOutputList, rax
    test rax, rax
    jz @@fail
    
    ; Set listbox properties
    INVOKE SendMessage, g_hOutputList, LB_SETITEMHEIGHT, 0, 16
    
    ; Clear output buffer
    mov g_outputPos, 0
    mov g_outputLineCount, 0
    
    xor eax, eax
    ret
    
@@fail:
    mov eax, -1
    ret
    
RawrXD_Output_Create ENDP

;----------------------------------------------------------------------
; RawrXD_Output_AddLine - Add line to output panel
;----------------------------------------------------------------------
RawrXD_Output_AddLine PROC pszText:QWORD, type:QWORD
    LOCAL szLine[512]:BYTE
    
    ; Check line count limit
    cmp g_outputLineCount, g_maxOutputLines
    jl @@add
    
    ; Remove first line (circular buffer)
    INVOKE SendMessage, g_hOutputList, LB_DELETESTRING, 0, 0
    jmp @@add
    
@@add:
    ; Build line with timestamp/type prefix
    INVOKE RawrXD_Output_FormatLine, ADDR szLine, pszText, type
    
    ; Add to listbox
    INVOKE SendMessage, g_hOutputList, LB_ADDSTRING, 0, ADDR szLine
    
    ; Increment line count
    mov rax, g_outputLineCount
    inc rax
    mov g_outputLineCount, rax
    
    ; Scroll to bottom
    mov eax, g_outputLineCount
    dec eax
    INVOKE SendMessage, g_hOutputList, LB_SETCURSEL, rax, 0
    
    ret
    
RawrXD_Output_AddLine ENDP

;----------------------------------------------------------------------
; RawrXD_Output_FormatLine - Format output line with type indicator
;----------------------------------------------------------------------
RawrXD_Output_FormatLine PROC pszDest:QWORD, pszSource:QWORD, type:QWORD
    LOCAL szPrefix[32]:BYTE
    
    ; Build prefix based on type
    cmp type, OUTPUT_ERROR
    je @@error_type
    
    cmp type, OUTPUT_WARNING
    je @@warning_type
    
    cmp type, OUTPUT_BUILD
    je @@build_type
    
    ; Default INFO
    INVOKE lstrcpyA, ADDR szPrefix, "[INFO] "
    jmp @@build_line
    
@@error_type:
    INVOKE lstrcpyA, ADDR szPrefix, "[ERR] "
    jmp @@build_line
    
@@warning_type:
    INVOKE lstrcpyA, ADDR szPrefix, "[WARN] "
    jmp @@build_line
    
@@build_type:
    INVOKE lstrcpyA, ADDR szPrefix, "[BUILD] "
    
@@build_line:
    ; Copy prefix
    INVOKE lstrcpyA, pszDest, ADDR szPrefix
    
    ; Append source text
    INVOKE lstrcatA, pszDest, pszSource
    
    ret
    
RawrXD_Output_FormatLine ENDP

;----------------------------------------------------------------------
; RawrXD_Output_Info - Log info message
;----------------------------------------------------------------------
RawrXD_Output_Info PROC pszText:QWORD
    INVOKE RawrXD_Output_AddLine, pszText, OUTPUT_INFO
    ret
RawrXD_Output_Info ENDP

;----------------------------------------------------------------------
; RawrXD_Output_Warning - Log warning message
;----------------------------------------------------------------------
RawrXD_Output_Warning PROC pszText:QWORD
    INVOKE RawrXD_Output_AddLine, pszText, OUTPUT_WARNING
    ret
RawrXD_Output_Warning ENDP

;----------------------------------------------------------------------
; RawrXD_Output_Error - Log error message
;----------------------------------------------------------------------
RawrXD_Output_Error PROC pszText:QWORD
    INVOKE RawrXD_Output_AddLine, pszText, OUTPUT_ERROR
    ret
RawrXD_Output_Error ENDP

;----------------------------------------------------------------------
; RawrXD_Output_Build - Log build message
;----------------------------------------------------------------------
RawrXD_Output_Build PROC pszText:QWORD
    INVOKE RawrXD_Output_AddLine, pszText, OUTPUT_BUILD
    ret
RawrXD_Output_Build ENDP

;----------------------------------------------------------------------
; RawrXD_Output_Clear - Clear all output
;----------------------------------------------------------------------
RawrXD_Output_Clear PROC
    ; Clear listbox
    INVOKE SendMessage, g_hOutputList, LB_RESETCONTENT, 0, 0
    
    ; Reset counters
    mov g_outputPos, 0
    mov g_outputLineCount, 0
    
    ; Clear buffer
    INVOKE RtlZeroMemory, ADDR g_outputBuffer, 4096
    
    ret
    
RawrXD_Output_Clear ENDP

;----------------------------------------------------------------------
; RawrXD_Output_CopySelected - Copy selected text to clipboard
;----------------------------------------------------------------------
RawrXD_Output_CopySelected PROC
    LOCAL idx:QWORD
    LOCAL szText[512]:BYTE
    
    ; Get selected item
    INVOKE SendMessage, g_hOutputList, LB_GETCURSEL, 0, 0
    cmp rax, LB_ERR
    je @@ret
    
    mov idx, rax
    
    ; Get text of selected item
    INVOKE SendMessage, g_hOutputList, LB_GETTEXT, idx, ADDR szText
    
    ; Copy to clipboard
    INVOKE OpenClipboard, NULL
    test eax, eax
    jz @@ret
    
    INVOKE EmptyClipboard
    
    ; Allocate global memory for clipboard
    INVOKE lstrlenA, ADDR szText
    mov rcx, rax
    inc rcx  ; Include null terminator
    INVOKE GlobalAlloc, GMEM_MOVEABLE, rcx
    test rax, rax
    jz @@close_clip
    
    ; Copy text to clipboard memory
    INVOKE GlobalLock, rax
    mov rcx, rax
    INVOKE lstrcpyA, rcx, ADDR szText
    INVOKE GlobalUnlock, rax
    
    ; Set clipboard data
    INVOKE SetClipboardData, CF_TEXT, rax
    
@@close_clip:
    INVOKE CloseClipboard
    
@@ret:
    ret
    
RawrXD_Output_CopySelected ENDP

;----------------------------------------------------------------------
; RawrXD_Output_ExportToFile - Save output to file
;----------------------------------------------------------------------
RawrXD_Output_ExportToFile PROC pszFilename:QWORD
    LOCAL hFile:QWORD
    LOCAL idx:QWORD
    LOCAL count:QWORD
    LOCAL szLine[512]:BYTE
    LOCAL bytesWritten:QWORD
    
    ; Create file
    INVOKE CreateFileA,
        pszFilename,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    
    cmp rax, INVALID_HANDLE_VALUE
    je @@fail
    
    mov hFile, rax
    
    ; Get line count
    INVOKE SendMessage, g_hOutputList, LB_GETCOUNT, 0, 0
    mov count, rax
    
    ; Write each line
    xor idx, idx
    
@@loop:
    cmp idx, count
    jge @@close
    
    ; Get line text
    INVOKE SendMessage, g_hOutputList, LB_GETTEXT, idx, ADDR szLine
    
    ; Write to file
    INVOKE lstrlenA, ADDR szLine
    mov rcx, rax
    INVOKE WriteFile, hFile, ADDR szLine, rcx, ADDR bytesWritten, NULL
    
    ; Write newline
    INVOKE WriteFile, hFile, OFFSET szNewline, 2, ADDR bytesWritten, NULL
    
    inc idx
    jmp @@loop
    
@@close:
    INVOKE CloseHandle, hFile
    xor eax, eax
    ret
    
@@fail:
    mov eax, -1
    ret
    
RawrXD_Output_ExportToFile ENDP

;----------------------------------------------------------------------
; RawrXD_Output_GetHeight - Get output panel height
;----------------------------------------------------------------------
RawrXD_Output_GetHeight PROC
    mov rax, g_outputHeight
    ret
RawrXD_Output_GetHeight ENDP

;----------------------------------------------------------------------
; RawrXD_Output_Resize - Resize output panel
;----------------------------------------------------------------------
RawrXD_Output_Resize PROC x:QWORD, y:QWORD, cx:QWORD, cy:QWORD
    INVOKE MoveWindow, g_hOutputList, x, y, cx, cy, TRUE
    
    ; Update height variable
    mov g_outputHeight, cy
    
    ret
    
RawrXD_Output_Resize ENDP

; String literals
szNewline               DB 13, 10, 0

END
