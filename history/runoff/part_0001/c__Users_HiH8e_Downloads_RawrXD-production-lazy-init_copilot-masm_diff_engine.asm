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
    
            mov rcx, WS_EX_TOOLWINDOW
            lea rdx, DIFF_VIEW_CLASS
            lea r8, DIFF_VIEW_CLASS
            mov r9d, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE
            push 0
            push 0
            push ghInstance
            push 0
            push DIFF_VIEW_HEIGHT
            push DIFF_VIEW_WIDTH
            push 100
            push 200
            call CreateWindowExA
            add rsp, 8*8
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, \
        0, 0, DIFF_VIEW_WIDTH/2, DIFF_VIEW_HEIGHT-60, \
        hDiffWnd, IDC_DIFF_OLD, ghInstance, 0
    
    mov hOldEdit, rax
    mov rcx, rax
    mov rdx, pOriginal
    call SetWindowTextA
    
    ; New version (right)
            mov rcx, WS_EX_CLIENTEDGE
            lea rdx, EDIT_CLASS
            lea r8, EMPTY_STR
            mov r9d, WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL
            push 0
            push ghInstance
            push IDC_DIFF_OLD
            push hDiffWnd
            push DIFF_VIEW_HEIGHT-60
            push DIFF_VIEW_WIDTH/2
            push 0
            push 0
            call CreateWindowExA
            add rsp, 8*8
    
    mov hNewEdit, rax
    mov rcx, rax
    mov rdx, pModified
    call SetWindowTextA
    
    ; Highlight differences
            mov rcx, WS_EX_CLIENTEDGE
            lea rdx, EDIT_CLASS
            lea r8, EMPTY_STR
            mov r9d, WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL
            push 0
            push ghInstance
            push IDC_DIFF_NEW
            push hDiffWnd
            push DIFF_VIEW_HEIGHT-60
            push DIFF_VIEW_WIDTH/2
            push 0
            push DIFF_VIEW_WIDTH/2
            call CreateWindowExA
            add rsp, 8*8
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, \
        DIFF_VIEW_WIDTH/2 - 150, DIFF_VIEW_HEIGHT-55, 140, 30, \
        hDiffWnd, IDC_DIFF_ACCEPT, ghInstance, 0
    
    mov hAcceptBtn, rax
    
    ; Reject button
    invoke CreateWindowExA, \
            mov rcx, 0
            lea rdx, BUTTON_CLASS
            lea r8, ACCEPT_BTN_TEXT
            mov r9d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON
            push 0
            push ghInstance
            push IDC_DIFF_ACCEPT
            push hDiffWnd
            push 30
            push 140
            push DIFF_VIEW_HEIGHT-55
            push DIFF_VIEW_WIDTH/2 - 150
            call CreateWindowExA
            add rsp, 8*8
    mov hRejectBtn, rax
    
    ; Position window center screen
    invoke GetDesktopWindow
            mov rcx, 0
            lea rdx, BUTTON_CLASS
            lea r8, REJECT_BTN_TEXT
            mov r9d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON
            push 0
            push ghInstance
            push IDC_DIFF_REJECT
            push hDiffWnd
            push 30
            push 140
            push DIFF_VIEW_HEIGHT-55
            push DIFF_VIEW_WIDTH/2 + 10
            call CreateWindowExA
            add rsp, 8*8
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
    mov rcx, hOld
    call GetWindowTextLengthA
    mov ecx, eax
    inc eax
    invoke GetProcessHeap
    mov rdx, rax
    mov r8, HEAP_ZERO_MEMORY
    call HeapAlloc
    mov pOldText, rax
    
    mov rcx, hOld
    mov rdx, pOldText
    mov r8d, ecx
    call GetWindowTextA
    
    ; Do line-by-line diff
    mov rcx, pOldText
    mov rdx, pNewText
    call Diff_ComputeLines
    
    ; Highlight added lines in green, removed in red
    mov ecx, numDifferences
    .repeat
           mov rcx, hOld
           mov rdx, EM_SETSEL
           mov r8d, startPos
           mov r9d, endPos
           call SendMessageA
           mov rcx, hOld
           mov rdx, EM_SETCHARFORMAT
           mov r8d, SCF_SELECTION
           lea r9, redFormat
           call SendMessageA
        
           mov rcx, hNew
           mov rdx, EM_SETSEL
           mov r8d, startPos
           mov r9d, endPos
           call SendMessageA
           mov rcx, hNew
           mov rdx, EM_SETCHARFORMAT
           mov r8d, SCF_SELECTION
           lea r9, greenFormat
           call SendMessageA
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