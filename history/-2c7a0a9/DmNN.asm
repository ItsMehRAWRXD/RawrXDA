;======================================================================
; diff_engine.asm - Inline Diff Viewer (Accept/Reject)
;======================================================================
INCLUDE windows.inc

.CONST
DIFF_VIEW_WIDTH  EQU 800
DIFF_VIEW_HEIGHT EQU 600
DIFF_SPLIT_RATIO EQU 0.5

.DATA?
hDiffWnd           QWORD ?
hOldEdit           QWORD ?
hNewEdit           QWORD ?
hAcceptBtn         QWORD ?
hRejectBtn         QWORD ?
pOriginalFile      QWORD ?
pModifiedFile      QWORD ?
pDiffData          QWORD ?
    .DATA
DIFF_VIEW_CLASS DB "DiffViewer",0
ACCEPT_BTN_TEXT DB "Accept (Ctrl+Y)",0
REJECT_BTN_TEXT DB "Reject (Ctrl+N)",0
EDIT_CLASS DB "EDIT",0
BUTTON_CLASS DB "BUTTON",0

.CODE

DiffEngine_Show PROC pFilePath:QWORD, pOriginal:QWORD, pModified:QWORD
    LOCAL rect:RECT
    
    mov pOriginalFile, pOriginal
    mov pModifiedFile, pModified
    
    ; Create diff viewer window
    invoke CreateWindowExA, \
        WS_EX_TOOLWINDOW, \
        `DiffViewer`, \
        `Suggested Changes`, \
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, \
        200, 100, DIFF_VIEW_WIDTH, DIFF_VIEW_HEIGHT, \
        0, 0, ghInstance, 0
    
    mov hDiffWnd, rax
    
            invoke CreateWindowExA, \
                WS_EX_TOOLWINDOW, \
                OFFSET DIFF_VIEW_CLASS, \
                OFFSET DIFF_VIEW_CLASS, \
                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, \
                200, 100, DIFF_VIEW_WIDTH, DIFF_VIEW_HEIGHT, \
                0, 0, ghInstance, 0
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, \
        0, 0, DIFF_VIEW_WIDTH/2, DIFF_VIEW_HEIGHT-60, \
        hDiffWnd, IDC_DIFF_OLD, ghInstance, 0
    
    mov hOldEdit, rax
    invoke SetWindowTextA, rax, pOriginal
    
    ; New version (right)
            invoke CreateWindowExA, \
                WS_EX_CLIENTEDGE, OFFSET EDIT_CLASS, OFFSET EMPTY_STR, \
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, \
                0, 0, DIFF_VIEW_WIDTH/2, DIFF_VIEW_HEIGHT-60, \
                hDiffWnd, IDC_DIFF_OLD, ghInstance, 0
    
    mov hNewEdit, rax
    invoke SetWindowTextA, rax, pModified
    
    ; Highlight differences
            invoke CreateWindowExA, \
                WS_EX_CLIENTEDGE, OFFSET EDIT_CLASS, OFFSET EMPTY_STR, \
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, \
                DIFF_VIEW_WIDTH/2, 0, DIFF_VIEW_WIDTH/2, DIFF_VIEW_HEIGHT-60, \
                hDiffWnd, IDC_DIFF_NEW, ghInstance, 0
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, \
        DIFF_VIEW_WIDTH/2 - 150, DIFF_VIEW_HEIGHT-55, 140, 30, \
        hDiffWnd, IDC_DIFF_ACCEPT, ghInstance, 0
    
    mov hAcceptBtn, rax
    
    ; Reject button
    invoke CreateWindowExA, \
            invoke CreateWindowExA, \
                0, OFFSET BUTTON_CLASS, OFFSET ACCEPT_BTN_TEXT, \
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, \
                DIFF_VIEW_WIDTH/2 - 150, DIFF_VIEW_HEIGHT-55, 140, 30, \
                hDiffWnd, IDC_DIFF_ACCEPT, ghInstance, 0
    mov hRejectBtn, rax
    
    ; Position window center screen
    invoke GetDesktopWindow
            invoke CreateWindowExA, \
                0, OFFSET BUTTON_CLASS, OFFSET REJECT_BTN_TEXT, \
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, \
                DIFF_VIEW_WIDTH/2 + 10, DIFF_VIEW_HEIGHT-55, 140, 30, \
                hDiffWnd, IDC_DIFF_REJECT, ghInstance, 0
    sub eax, DIFF_VIEW_WIDTH
    shr eax, 1
    mov ebx, rect.bottom
    sub ebx, DIFF_VIEW_HEIGHT
    shr ebx, 1
    
    invoke SetWindowPos, hDiffWnd, 0, eax, ebx, 0, 0, SWP_NOSIZE
    
    ret
DiffEngine_Show ENDP

DiffEngine_HighlightChanges PROC hOld:QWORD, hNew:QWORD
    LOCAL pOldText:QWORD
    LOCAL pNewText:QWORD
    LOCAL hOldDC:QWORD
    LOCAL hNewDC:QWORD
    
    ; Get text from both editors
    invoke GetWindowTextLengthA, hOld
    mov ecx, eax
    inc eax
    invoke GetProcessHeap
    mov rdx, rax
    mov r8, HEAP_ZERO_MEMORY
    call HeapAlloc
    mov pOldText, rax
    
    invoke GetWindowTextA, hOld, pOldText, ecx
    
    ; Do line-by-line diff
    invoke Diff_ComputeLines, pOldText, pNewText
    
    ; Highlight added lines in green, removed in red
    mov ecx, numDifferences
    .repeat
        invoke SendMessageA, hOld, EM_SETSEL, startPos, endPos
        invoke SendMessageA, hOld, EM_SETCHARFORMAT, SCF_SELECTION, ADDR redFormat
        
        invoke SendMessageA, hNew, EM_SETSEL, startPos, endPos
        invoke SendMessageA, hNew, EM_SETCHARFORMAT, SCF_SELECTION, ADDR greenFormat
    .untilcxz
    
    ret
DiffEngine_HighlightChanges ENDP

DiffEngine_OnAccept PROC
    ; Apply changes to file
    invoke GetWindowTextA, hNewEdit, ADDR buffer, SIZEOF buffer
    
    ; Write to file
    invoke CreateFileA, pOriginalFile, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0
    mov hFile, rax
    
    invoke WriteFile, hFile, ADDR buffer, lstrlenA(ADDR buffer), ADDR dwWritten, 0
    invoke CloseHandle, hFile
    
    ; Close diff viewer
    invoke DestroyWindow, hDiffWnd
    
    ret
DiffEngine_OnAccept ENDP

DiffEngine_OnReject PROC
    ; Discard changes
    invoke DestroyWindow, hDiffWnd
    ret
DiffEngine_OnReject ENDP

END