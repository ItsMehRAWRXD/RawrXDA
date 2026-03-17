; ==============================================================================
; dialogs.asm - Windows API Dialog System
; ==============================================================================
; 
; Wrapper functions for standard Windows dialogs:
;   - File Open Dialog
;   - File Save Dialog
;   - Message Boxes
;

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comdlg32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comdlg32.lib

; Dialog constants
DIALOG_MAX_PATH         equ 260

.data
    ; File type filters
    szAllFiles          db "All Files (*.*)", 0, "*.*", 0, 0
    szSourceFiles       db "Source Files (*.asm;*.c;*.h)", 0, "*.asm;*.c;*.h", 0
                        db "All Files (*.*)", 0, "*.*", 0, 0
    szTextFiles         db "Text Files (*.txt)", 0, "*.txt", 0
                        db "All Files (*.*)", 0, "*.*", 0, 0
    
    szOpenTitle         db "Open File", 0
    szSaveTitle         db "Save File As", 0
    szInitialDir        db ".\", 0

.code

public Dialog_FileOpen
public Dialog_FileSave
public Dialog_MessageBox

; ==============================================================================
; Open File Dialog using GetOpenFileName
; Parameters:
;   hwnd - parent window handle
;   pszFilePath - pointer to buffer for result path (260 bytes minimum)
;   pszFilter - file filter string (optional)
; Returns: TRUE if file selected, FALSE if cancelled
; ==============================================================================
Dialog_FileOpen proc hwnd:DWORD, pszFilePath:DWORD, pszFilter:DWORD
    LOCAL ofn:OPENFILENAME
    
    ; Fill OPENFILENAME structure
    mov ofn.lStructSize, sizeof OPENFILENAME
    mov eax, hwnd
    mov ofn.hwndOwner, eax
    xor eax, eax
    mov ofn.hInstance, eax
    
    ; Use provided filter or default
    mov eax, pszFilter
    test eax, eax
    jnz @HaveFilter
    mov eax, offset szAllFiles
@HaveFilter:
    mov ofn.lpstrFilter, eax
    
    mov ofn.lpstrCustomFilter, 0
    mov ofn.nMaxCustFilter, 0
    mov ofn.nFilterIndex, 1
    
    mov eax, pszFilePath
    mov ofn.lpstrFile, eax
    mov ofn.nMaxFile, DIALOG_MAX_PATH
    mov ofn.lpstrFileTitle, 0
    mov ofn.nMaxFileTitle, 0
    mov ofn.lpstrInitialDir, offset szInitialDir
    mov ofn.lpstrTitle, offset szOpenTitle
    
    mov ofn.Flags, OFN_FILEMUSTEXIST or OFN_PATHMUSTEXIST or OFN_HIDEREADONLY
    mov ofn.nFileOffset, 0
    mov ofn.nFileExtension, 0
    mov ofn.lpstrDefExt, 0
    mov ofn.lCustData, 0
    mov ofn.lpfnHook, 0
    mov ofn.lpTemplateName, 0
    
    ; Call GetOpenFileName
    invoke GetOpenFileNameA, addr ofn
    ret
Dialog_FileOpen endp

; ==============================================================================
; Save File Dialog
; Parameters:
;   hwnd - parent window handle
;   pszFilePath - pointer to buffer for result path
;   pszFilter - file filter string (optional)
; Returns: TRUE if file selected, FALSE if cancelled
; ==============================================================================
Dialog_FileSave proc hwnd:DWORD, pszFilePath:DWORD, pszFilter:DWORD
    LOCAL ofn:OPENFILENAME
    
    ; Fill OPENFILENAME structure
    mov ofn.lStructSize, sizeof OPENFILENAME
    mov eax, hwnd
    mov ofn.hwndOwner, eax
    xor eax, eax
    mov ofn.hInstance, eax
    
    ; Use provided filter or default
    mov eax, pszFilter
    test eax, eax
    jnz @HaveFilter
    mov eax, offset szTextFiles
@HaveFilter:
    mov ofn.lpstrFilter, eax
    
    mov ofn.lpstrCustomFilter, 0
    mov ofn.nMaxCustFilter, 0
    mov ofn.nFilterIndex, 1
    
    mov eax, pszFilePath
    mov ofn.lpstrFile, eax
    mov ofn.nMaxFile, DIALOG_MAX_PATH
    mov ofn.lpstrFileTitle, 0
    mov ofn.nMaxFileTitle, 0
    mov ofn.lpstrInitialDir, offset szInitialDir
    mov ofn.lpstrTitle, offset szSaveTitle
    
    mov ofn.Flags, OFN_OVERWRITEPROMPT or OFN_PATHMUSTEXIST or OFN_HIDEREADONLY
    mov ofn.nFileOffset, 0
    mov ofn.nFileExtension, 0
    mov ofn.lpstrDefExt, 0
    mov ofn.lCustData, 0
    mov ofn.lpfnHook, 0
    mov ofn.lpTemplateName, 0
    
    ; Call GetSaveFileName
    invoke GetSaveFileNameA, addr ofn
    ret
Dialog_FileSave endp

; ==============================================================================
; Message Box with title
; Parameters:
;   hwnd - parent window handle
;   pszTitle - title string
;   pszMessage - message text
;   uType - MessageBox type (MB_OK, MB_YESNO, etc.)
; Returns: MessageBox result (IDOK, IDCANCEL, IDYES, IDNO, etc.)
; ==============================================================================
Dialog_MessageBox proc hwnd:DWORD, pszTitle:DWORD, pszMessage:DWORD, uType:DWORD
    invoke MessageBoxA, hwnd, pszMessage, pszTitle, uType
    ret
Dialog_MessageBox endp

end
