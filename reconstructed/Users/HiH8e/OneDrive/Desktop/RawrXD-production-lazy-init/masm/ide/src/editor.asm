; ============================================================================
; RawrXD Agentic IDE - Code Editor Implementation (Pure MASM)
; RichEdit control with syntax highlighting
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comctl32.inc
include \masm32\include\richedit.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comctl32.lib
includelib \masm32\lib\riched20.lib

.data
include constants.inc
include structures.inc
include macros.inc

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    szRichEditClass    db "RichEdit20W", 0
    szDefaultText      db "Welcome to RawrXD Agentic IDE (Pure MASM)", 13, 10, 13, 10
                       db "Use the magic wand button to execute agentic wishes!", 13, 10
                       db "Or run autonomous loops for complex tasks.", 13, 10, 0
    
    ; Syntax highlighting colors
    clrKeyword         dd 00007ACCh    ; Blue
    clrComment         dd 00008000h    ; Green
    clrString          dd 00A31515h    ; Red
    clrNumber          dd 000000FFh    ; Purple
    clrNormal          dd 00E0E0E0h    ; Light gray
    
    ; Keywords for syntax highlighting
    szKeywords         db "mov invoke call push pop add sub mul div cmp test jmp je jne jg jl jge jle loop ret", 0
    szComments         db ";", 0
    
    ; Editor state
    g_hEditor          dd 0
    g_bModified        dd 0
    g_szCurrentFile    db MAX_PATH dup(0)

.data?
    g_hRichEditLib     dd ?

; ============================================================================
; PROCEDURES
; ============================================================================

; ============================================================================
; CreateEditor - Create RichEdit control
; Returns: Editor handle in eax
; ============================================================================
CreateEditor proc
    LOCAL dwStyle:DWORD
    LOCAL hEdit:DWORD
    
    ; Load RichEdit library
    invoke LoadLibrary, addr szRichEditClass
    mov g_hRichEditLib, eax
    
    ; Create RichEdit control
    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_HSCROLL or ES_MULTILINE or ES_AUTOVSCROLL or ES_AUTOHSCROLL
    
    CreateWnd szRichEditClass, NULL, dwStyle, 0, 0, 600, 400, hTabControl, IDC_EDITOR
    mov hEdit, eax
    mov g_hEditor, eax
    
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Set font
    invoke SendMessage, hEdit, WM_SETFONT, hEditorFont, TRUE
    
    ; Set background color
    invoke SendMessage, hEdit, EM_SETBKGNDCOLOR, 0, clrBackground
    
    ; Set text color
    invoke SendMessage, hEdit, EM_SETCHARFORMAT, SCF_ALL, addr cfDefault
    
    ; Set default text
    invoke SendMessage, hEdit, WM_SETTEXT, 0, addr szDefaultText
    
    ; Enable syntax highlighting
    call EnableSyntaxHighlighting
    
    mov eax, hEdit
    ret
CreateEditor endp

; ============================================================================
; EnableSyntaxHighlighting - Enable syntax highlighting
; ============================================================================
EnableSyntaxHighlighting proc
    LOCAL cf:CHARFORMAT
    
    ; Set up default format
    mov cf.cbSize, sizeof CHARFORMAT
    mov cf.dwMask, CFM_COLOR
    mov cf.dwEffects, 0
    mov cf.crTextColor, clrNormal
    
    invoke SendMessage, g_hEditor, EM_SETCHARFORMAT, SCF_ALL, addr cf
    
    ret
EnableSyntaxHighlighting endp

; ============================================================================
; Editor_LoadFile - Load file into editor
; Input: pszFilePath
; ============================================================================
Editor_LoadFile proc pszFilePath:DWORD
    LOCAL hFile:DWORD
    LOCAL dwFileSize:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL pBuffer:DWORD
    
    ; Open file
    invoke CreateFile, pszFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    
    .if eax == INVALID_HANDLE_VALUE
        xor eax, eax
        ret
    .endif
    
    ; Get file size
    invoke GetFileSize, hFile, NULL
    mov dwFileSize, eax
    
    ; Allocate buffer
    MemAlloc dwFileSize
    mov pBuffer, eax
    
    ; Read file
    invoke ReadFile, hFile, pBuffer, dwFileSize, addr dwBytesRead, NULL
    
    ; Set text in editor
    invoke SendMessage, g_hEditor, WM_SETTEXT, 0, pBuffer
    
    ; Free buffer
    MemFree pBuffer
    
    ; Close file
    invoke CloseHandle, hFile
    
    ; Update current file
    szCopy addr g_szCurrentFile, pszFilePath
    mov g_bModified, 0
    
    mov eax, 1
    ret
Editor_LoadFile endp

; ============================================================================
; Editor_SaveFile - Save editor content to file
; Input: pszFilePath (optional, uses current if NULL)
; ============================================================================
Editor_SaveFile proc pszFilePath:DWORD
    LOCAL hFile:DWORD
    LOCAL dwTextLength:DWORD
    LOCAL dwBytesWritten:DWORD
    LOCAL pBuffer:DWORD
    LOCAL pszTarget:DWORD
    
    ; Determine target file
    .if pszFilePath == 0
        mov eax, offset g_szCurrentFile
    .else
        mov eax, pszFilePath
    .endif
    mov pszTarget, eax
    
    ; Get text length
    invoke SendMessage, g_hEditor, WM_GETTEXTLENGTH, 0, 0
    mov dwTextLength, eax
    
    ; Allocate buffer
    MemAlloc dwTextLength
    mov pBuffer, eax
    
    ; Get text
    invoke SendMessage, g_hEditor, WM_GETTEXT, dwTextLength, pBuffer
    
    ; Create file
    invoke CreateFile, pszTarget, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    
    .if eax == INVALID_HANDLE_VALUE
        MemFree pBuffer
        xor eax, eax
        ret
    .endif
    
    ; Write file
    invoke WriteFile, hFile, pBuffer, dwTextLength, addr dwBytesWritten, NULL
    
    ; Cleanup
    MemFree pBuffer
    invoke CloseHandle, hFile
    
    ; Update state
    mov g_bModified, 0
    
    mov eax, 1
    ret
Editor_SaveFile endp

; ============================================================================
; Editor_GetText - Get editor text
; Returns: Pointer to text buffer in eax (caller must free)
; ============================================================================
Editor_GetText proc
    LOCAL dwLength:DWORD
    LOCAL pBuffer:DWORD
    
    invoke SendMessage, g_hEditor, WM_GETTEXTLENGTH, 0, 0
    mov dwLength, eax
    
    MemAlloc dwLength
    mov pBuffer, eax
    
    invoke SendMessage, g_hEditor, WM_GETTEXT, dwLength, pBuffer
    
    mov eax, pBuffer
    ret
Editor_GetText endp

; ============================================================================
; Editor_SetText - Set editor text
; Input: pszText
; ============================================================================
Editor_SetText proc pszText:DWORD
    invoke SendMessage, g_hEditor, WM_SETTEXT, 0, pszText
    mov g_bModified, 1
    ret
Editor_SetText endp

; ============================================================================
; Editor_InsertText - Insert text at cursor
; Input: pszText
; ============================================================================
Editor_InsertText proc pszText:DWORD
    invoke SendMessage, g_hEditor, EM_REPLACESEL, TRUE, pszText
    mov g_bModified, 1
    ret
Editor_InsertText endp

; ============================================================================
; Editor_GetSelection - Get selected text
; Returns: Pointer to selection in eax (caller must free)
; ============================================================================
Editor_GetSelection proc
    LOCAL chr:CHARRANGE
    LOCAL dwLength:DWORD
    LOCAL pBuffer:DWORD
    
    ; Get selection range
    invoke SendMessage, g_hEditor, EM_EXGETSEL, 0, addr chr
    
    ; Calculate length
    mov eax, chr.cpMax
    sub eax, chr.cpMin
    mov dwLength, eax
    
    .if dwLength == 0
        xor eax, eax
        ret
    .endif
    
    MemAlloc dwLength
    mov pBuffer, eax
    
    ; Get selection
    invoke SendMessage, g_hEditor, EM_GETSELTEXT, 0, pBuffer
    
    mov eax, pBuffer
    ret
Editor_GetSelection endp

; ============================================================================
; Editor_Undo - Undo last action
; ============================================================================
Editor_Undo proc
    invoke SendMessage, g_hEditor, EM_UNDO, 0, 0
    ret
Editor_Undo endp

; ============================================================================
; Editor_Redo - Redo last undone action
; ============================================================================
Editor_Redo proc
    invoke SendMessage, g_hEditor, EM_REDO, 0, 0
    ret
Editor_Redo endp

; ============================================================================
; Editor_Cut - Cut selection to clipboard
; ============================================================================
Editor_Cut proc
    invoke SendMessage, g_hEditor, WM_CUT, 0, 0
    ret
Editor_Cut endp

; ============================================================================
; Editor_Copy - Copy selection to clipboard
; ============================================================================
Editor_Copy proc
    invoke SendMessage, g_hEditor, WM_COPY, 0, 0
    ret
Editor_Copy endp

; ============================================================================
; Editor_Paste - Paste from clipboard
; ============================================================================
Editor_Paste proc
    invoke SendMessage, g_hEditor, WM_PASTE, 0, 0
    ret
Editor_Paste endp

; ============================================================================
; Editor_Find - Find text in editor
; Input: pszFindText, bMatchCase, bWholeWord
; ============================================================================
Editor_Find proc pszFindText:DWORD, bMatchCase:DWORD, bWholeWord:DWORD
    LOCAL fr:FINDREPLACE
    LOCAL flags:DWORD
    
    mov flags, 0
    .if bMatchCase
        or flags, FR_MATCHCASE
    .endif
    .if bWholeWord
        or flags, FR_WHOLEWORD
    .endif
    
    mov fr.lStructSize, sizeof FINDREPLACE
    mov fr.hwndOwner, g_hEditor
    mov fr.lpstrFindWhat, pszFindText
    mov fr.wFindWhatLen, MAX_PATH
    mov fr.Flags, flags
    
    invoke SendMessage, g_hEditor, EM_FINDTEXT, flags, addr fr
    ret
Editor_Find endp

; ============================================================================
; Editor_Replace - Replace text in editor
; Input: pszFindText, pszReplaceText, bMatchCase, bWholeWord
; ============================================================================
Editor_Replace proc pszFindText:DWORD, pszReplaceText:DWORD, bMatchCase:DWORD, bWholeWord:DWORD
    LOCAL fr:FINDREPLACE
    LOCAL flags:DWORD
    
    mov flags, FR_REPLACE
    .if bMatchCase
        or flags, FR_MATCHCASE
    .endif
    .if bWholeWord
        or flags, FR_WHOLEWORD
    .endif
    
    mov fr.lStructSize, sizeof FINDREPLACE
    mov fr.hwndOwner, g_hEditor
    mov fr.lpstrFindWhat, pszFindText
    mov fr.lpstrReplaceWith, pszReplaceText
    mov fr.wFindWhatLen, MAX_PATH
    mov fr.wReplaceWithLen, MAX_PATH
    mov fr.Flags, flags
    
    invoke SendMessage, g_hEditor, EM_REPLACE, flags, addr fr
    ret
Editor_Replace endp

; ============================================================================
; Editor_GotoLine - Go to specific line
; Input: dwLineNumber
; ============================================================================
Editor_GotoLine proc dwLineNumber:DWORD
    LOCAL dwIndex:DWORD
    
    invoke SendMessage, g_hEditor, EM_LINEINDEX, dwLineNumber, 0
    mov dwIndex, eax
    
    invoke SendMessage, g_hEditor, EM_SETSEL, dwIndex, dwIndex
    invoke SendMessage, g_hEditor, EM_SCROLLCARET, 0, 0
    
    ret
Editor_GotoLine endp

; ============================================================================
; Editor_GetCurrentLine - Get current line number
; Returns: Line number in eax
; ============================================================================
Editor_GetCurrentLine proc
    LOCAL dwIndex:DWORD
    
    invoke SendMessage, g_hEditor, EM_LINEFROMCHAR, -1, 0
    ret
Editor_GetCurrentLine endp

; ============================================================================
; Editor_Cleanup - Cleanup editor resources
; ============================================================================
Editor_Cleanup proc
    .if g_hRichEditLib != 0
        invoke FreeLibrary, g_hRichEditLib
        mov g_hRichEditLib, 0
    .endif
    ret
Editor_Cleanup endp

; ============================================================================
; Data for RichEdit
; ============================================================================

.data
    cfDefault CHARFORMAT <sizeof CHARFORMAT, CFM_COLOR, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>

end
