; ============================================================================
; RawrXD Agentic IDE - Simple Text Editor Implementation (Pure MASM)
; Basic Edit control with file operations - Phase 1 Complete
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comdlg32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comdlg32.lib

.data
include constants.inc

extrn g_hInstance:DWORD
extrn g_hMainWindow:DWORD
extrn g_hMainFont:DWORD
extrn hTabControl:DWORD

; Editor constants
IDC_SIMPLE_EDITOR   equ 2100

; File dialog constants
MAX_FILE_SIZE       equ 1048576  ; 1MB max file size

; Data section
    szSimpleEditClass db "EDIT",0
    szDefaultText   db "Welcome to RawrXD Agentic IDE", 13, 10
                    db "Create, edit, and manage your code files.", 13, 10, 13, 10
                    db "Features:", 13, 10
                    db "- Multi-tab editing", 13, 10
                    db "- File operations (New, Open, Save)", 13, 10
                    db "- Project file tree navigation", 13, 10
                    db "- Agentic assistance integration", 13, 10, 0

    szFileFilter    db "Text Files", 0, "*.txt", 0
                    db "Assembly Files", 0, "*.asm", 0
                    db "C/C++ Files", 0, "*.c;*.cpp;*.h", 0
                    db "All Files", 0, "*.*", 0, 0

    g_hEditor       dd 0
    g_bModified     dd 0
    
.data?
    szCurrentFile   db MAX_PATH dup(?)
    szFileBuffer    db MAX_FILE_SIZE dup(?)
    ofn             OPENFILENAME <>

.code

; Forward declarations
CreateSimpleEditor proto
OpenFile proto
SaveFile proto
SaveFileAs proto
NewFile proto
IsModified proto
SetModified proto :DWORD
GetEditorText proto :DWORD, :DWORD
SetEditorText proto :DWORD

; ============================================================================
; CreateSimpleEditor - Create basic text editor control
; Returns: Editor handle in eax
; ============================================================================
CreateSimpleEditor proc
    LOCAL dwStyle:DWORD
    
    ; Create multiline edit control with scrollbars
    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_HSCROLL or ES_MULTILINE or ES_AUTOVSCROLL or ES_AUTOHSCROLL or ES_WANTRETURN
    
    invoke CreateWindowEx, WS_EX_CLIENTEDGE, addr szSimpleEditClass, NULL, dwStyle, 
           0, 0, 600, 400, g_hMainWindow, IDC_SIMPLE_EDITOR, g_hInstance, NULL
    mov g_hEditor, eax
    
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Set font if available
    .if g_hMainFont != 0
        invoke SendMessage, g_hEditor, WM_SETFONT, g_hMainFont, TRUE
    .endif
    
    ; Set default text
    invoke SendMessage, g_hEditor, WM_SETTEXT, 0, addr szDefaultText
    
    ; Clear modified flag
    mov g_bModified, 0
    
    mov eax, g_hEditor
    ret
CreateSimpleEditor endp

; ============================================================================
; NewFile - Create new file
; ============================================================================
NewFile proc
    ; Clear editor
    invoke SendMessage, g_hEditor, WM_SETTEXT, 0, addr szDefaultText
    
    ; Clear current file name
    mov BYTE PTR szCurrentFile, 0
    
    ; Clear modified flag
    mov g_bModified, 0
    
    ; Update window title could be added here
    
    mov eax, 1  ; Success
    ret
NewFile endp

; ============================================================================
; OpenFile - Open file dialog and load file
; ============================================================================
OpenFile proc
    LOCAL hFile:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL dwFileSize:DWORD
    
    ; Initialize OPENFILENAME structure
    mov ofn.lStructSize, sizeof OPENFILENAME
    mov ofn.hwndOwner, g_hMainWindow
    mov ofn.hInstance, g_hInstance
    mov ofn.lpstrFilter, offset szFileFilter
    mov ofn.lpstrCustomFilter, NULL
    mov ofn.nMaxCustFilter, 0
    mov ofn.nFilterIndex, 1
    mov ofn.lpstrFile, offset szCurrentFile
    mov ofn.nMaxFile, MAX_PATH
    mov ofn.lpstrFileTitle, NULL
    mov ofn.nMaxFileTitle, 0
    mov ofn.lpstrInitialDir, NULL
    mov ofn.lpstrTitle, NULL
    mov ofn.Flags, OFN_PATHMUSTEXIST or OFN_FILEMUSTEXIST or OFN_HIDEREADONLY
    mov ofn.nFileOffset, 0
    mov ofn.nFileExtension, 0
    mov ofn.lpstrDefExt, NULL
    mov ofn.lCustData, 0
    mov ofn.lpfnHook, NULL
    mov ofn.lpTemplateName, NULL
    
    ; Show open file dialog
    invoke GetOpenFileName, addr ofn
    .if eax == 0
        xor eax, eax  ; User cancelled
        ret
    .endif
    
    ; Open file
    invoke CreateFile, addr szCurrentFile, GENERIC_READ, FILE_SHARE_READ, 
           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    .if eax == INVALID_HANDLE_VALUE
        ; Could show error message here
        xor eax, eax
        ret
    .endif
    
    ; Get file size
    invoke GetFileSize, hFile, NULL
    mov dwFileSize, eax
    .if eax > MAX_FILE_SIZE
        invoke CloseHandle, hFile
        ; Could show "file too large" message
        xor eax, eax
        ret
    .endif
    
    ; Read file
    invoke ReadFile, hFile, addr szFileBuffer, dwFileSize, addr dwBytesRead, NULL
    invoke CloseHandle, hFile
    
    .if eax == 0
        ; Read error
        xor eax, eax
        ret
    .endif
    
    ; Null terminate the buffer
    mov eax, dwBytesRead
    mov BYTE PTR [szFileBuffer + eax], 0
    
    ; Set text in editor
    invoke SendMessage, g_hEditor, WM_SETTEXT, 0, addr szFileBuffer
    
    ; Clear modified flag
    mov g_bModified, 0
    
    mov eax, 1  ; Success
    ret
OpenFile endp

; ============================================================================
; SaveFile - Save current file
; ============================================================================
SaveFile proc
    ; Check if we have a filename
    .if BYTE PTR szCurrentFile == 0
        ; No filename, show Save As dialog
        call SaveFileAs
        ret
    .endif
    
    call DoSaveFile
    ret
SaveFile endp

; ============================================================================
; SaveFileAs - Save file with new name
; ============================================================================
SaveFileAs proc
    ; Initialize OPENFILENAME structure for save
    mov ofn.lStructSize, sizeof OPENFILENAME
    mov ofn.hwndOwner, g_hMainWindow
    mov ofn.hInstance, g_hInstance
    mov ofn.lpstrFilter, offset szFileFilter
    mov ofn.lpstrCustomFilter, NULL
    mov ofn.nMaxCustFilter, 0
    mov ofn.nFilterIndex, 1
    mov ofn.lpstrFile, offset szCurrentFile
    mov ofn.nMaxFile, MAX_PATH
    mov ofn.lpstrFileTitle, NULL
    mov ofn.nMaxFileTitle, 0
    mov ofn.lpstrInitialDir, NULL
    mov ofn.lpstrTitle, NULL
    mov ofn.Flags, OFN_PATHMUSTEXIST or OFN_OVERWRITEPROMPT or OFN_HIDEREADONLY
    mov ofn.nFileOffset, 0
    mov ofn.nFileExtension, 0
    mov ofn.lpstrDefExt, NULL
    mov ofn.lCustData, 0
    mov ofn.lpfnHook, NULL
    mov ofn.lpTemplateName, NULL
    
    ; Show save file dialog
    invoke GetSaveFileName, addr ofn
    .if eax == 0
        xor eax, eax  ; User cancelled
        ret
    .endif
    
    call DoSaveFile
    ret
SaveFileAs endp

; ============================================================================
; DoSaveFile - Internal save file implementation
; ============================================================================
DoSaveFile proc
    LOCAL hFile:DWORD
    LOCAL dwBytesWritten:DWORD
    LOCAL dwTextLength:DWORD
    
    ; Get text length
    invoke SendMessage, g_hEditor, WM_GETTEXTLENGTH, 0, 0
    mov dwTextLength, eax
    
    ; Get text from editor
    invoke SendMessage, g_hEditor, WM_GETTEXT, MAX_FILE_SIZE, addr szFileBuffer
    
    ; Create file
    invoke CreateFile, addr szCurrentFile, GENERIC_WRITE, 0, 
           NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    .if eax == INVALID_HANDLE_VALUE
        ; Could show error message
        xor eax, eax
        ret
    .endif
    
    ; Write file
    invoke WriteFile, hFile, addr szFileBuffer, dwTextLength, addr dwBytesWritten, NULL
    invoke CloseHandle, hFile
    
    .if eax == 0
        ; Write error
        xor eax, eax
        ret
    .endif
    
    ; Clear modified flag
    mov g_bModified, 0
    
    mov eax, 1  ; Success
    ret
DoSaveFile endp

; ============================================================================
; IsModified - Check if editor content is modified
; ============================================================================
IsModified proc
    mov eax, g_bModified
    ret
IsModified endp

; ============================================================================
; SetModified - Set modified flag
; ============================================================================
SetModified proc bModified:DWORD
    mov eax, bModified
    mov g_bModified, eax
    ret
SetModified endp

; ============================================================================
; GetEditorText - Get text from editor
; ============================================================================
GetEditorText proc pBuffer:DWORD, nMaxChars:DWORD
    invoke SendMessage, g_hEditor, WM_GETTEXT, nMaxChars, pBuffer
    ret
GetEditorText endp

; ============================================================================
; SetEditorText - Set text in editor
; ============================================================================
SetEditorText proc pText:DWORD
    invoke SendMessage, g_hEditor, WM_SETTEXT, 0, pText
    mov g_bModified, 0  ; Reset modified flag when setting text programmatically
    ret
SetEditorText endp

; ============================================================================
; Public interface
; ============================================================================
public CreateSimpleEditor
public NewFile
public OpenFile  
public SaveFile
public SaveFileAs
public IsModified
public SetModified
public GetEditorText
public SetEditorText

end