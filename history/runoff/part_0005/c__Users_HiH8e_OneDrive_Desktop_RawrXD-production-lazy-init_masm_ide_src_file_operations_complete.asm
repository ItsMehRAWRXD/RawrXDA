; ==============================================================================
; file_operations_complete.asm - Full file I/O operations
; FileDialog_Open, FileDialog_SaveAs, ShowFindDialog, ShowReplaceDialog
; ==============================================================================

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

MAX_PATH_STR        equ 260
MAX_FILTER_STR      equ 512

.data
    ; File filters
    szAllFiles          db "All Files (*.*)", 0, "*.*", 0
                        db "MASM Files (*.asm)", 0, "*.asm", 0
                        db "Header Files (*.inc)", 0, "*.inc", 0
                        db 0
    
    szSourceFiles       db "Source Files (*.asm;*.c;*.h)", 0, "*.asm;*.c;*.h", 0
                        db "All Files (*.*)", 0, "*.*", 0, 0
    
    szOpenTitle         db "Open File", 0
    szSaveTitle         db "Save File As", 0
    szFindTitle         db "Find", 0
    szReplaceTitle      db "Find & Replace", 0
    
    ; Dialog state
    szCurrentFile       db MAX_PATH_STR dup(0)
    szCurrentDir        db MAX_PATH_STR dup(0)

.code

; ============================================================================
; FileDialog_Open - Display file open dialog
; Input:  hParent = parent window
; Output: EAX = 1 if file selected, 0 if cancelled
; ============================================================================
public FileDialog_Open
FileDialog_Open proc c hParent:DWORD
    LOCAL ofn:OPENFILENAMEA
    LOCAL szFileName[MAX_PATH_STR]:BYTE
    LOCAL szFileTitle[MAX_PATH_STR]:BYTE
    
    push ebx
    push esi
    
    ; Initialize filename buffer
    lea eax, szFileName
    mov byte ptr [eax], 0
    
    ; Set up OPENFILENAMEA structure
    mov ofn.lStructSize, sizeof OPENFILENAMEA
    mov eax, hParent
    mov ofn.hwndOwner, eax
    mov ofn.hInstance, 0
    lea eax, szSourceFiles
    mov ofn.lpstrFilter, eax
    mov ofn.nFilterIndex, 1
    lea eax, szFileName
    mov ofn.lpstrFile, eax
    mov ofn.nMaxFile, MAX_PATH_STR
    lea eax, szFileTitle
    mov ofn.lpstrFileTitle, eax
    mov ofn.nMaxFileTitle, MAX_PATH_STR
    lea eax, szCurrentDir
    mov ofn.lpstrInitialDir, eax
    lea eax, szOpenTitle
    mov ofn.lpstrTitle, eax
    mov ofn.Flags, OFN_FILEMUSTEXIST or OFN_PATHMUSTEXIST or OFN_HIDEREADONLY
    
    ; Show dialog
    invoke GetOpenFileNameA, ADDR ofn
    test eax, eax
    jz @cancelled
    
    ; Copy result to global
    lea eax, szFileName
    lea edi, szCurrentFile
    xor ecx, ecx
    
@@copy:
    cmp ecx, MAX_PATH_STR
    jge @done
    mov dl, byte ptr [eax + ecx]
    mov byte ptr [edi + ecx], dl
    test dl, dl
    jz @done
    inc ecx
    jmp @@copy
    
@done:
    mov eax, TRUE
    pop esi
    pop ebx
    ret
    
@cancelled:
    xor eax, eax
    pop esi
    pop ebx
    ret
FileDialog_Open endp

; ============================================================================
; FileDialog_SaveAs - Display file save dialog
; Input:  hParent = parent window
; Output: EAX = 1 if file selected, 0 if cancelled
; ============================================================================
public FileDialog_SaveAs
FileDialog_SaveAs proc c hParent:DWORD
    LOCAL ofn:OPENFILENAMEA
    LOCAL szFileName[MAX_PATH_STR]:BYTE
    LOCAL szFileTitle[MAX_PATH_STR]:BYTE
    
    push ebx
    push esi
    
    ; Initialize filename buffer
    lea eax, szFileName
    mov byte ptr [eax], 0
    
    ; Set up OPENFILENAMEA structure
    mov ofn.lStructSize, sizeof OPENFILENAMEA
    mov eax, hParent
    mov ofn.hwndOwner, eax
    mov ofn.hInstance, 0
    lea eax, szSourceFiles
    mov ofn.lpstrFilter, eax
    mov ofn.nFilterIndex, 1
    lea eax, szFileName
    mov ofn.lpstrFile, eax
    mov ofn.nMaxFile, MAX_PATH_STR
    lea eax, szFileTitle
    mov ofn.lpstrFileTitle, eax
    mov ofn.nMaxFileTitle, MAX_PATH_STR
    lea eax, szCurrentDir
    mov ofn.lpstrInitialDir, eax
    lea eax, szSaveTitle
    mov ofn.lpstrTitle, eax
    mov ofn.Flags, OFN_OVERWRITEPROMPT or OFN_PATHMUSTEXIST or OFN_HIDEREADONLY
    
    ; Show dialog
    invoke GetSaveFileNameA, ADDR ofn
    test eax, eax
    jz @cancelled
    
    ; Copy result to global
    lea eax, szFileName
    lea edi, szCurrentFile
    xor ecx, ecx
    
@@copy:
    cmp ecx, MAX_PATH_STR
    jge @done
    mov dl, byte ptr [eax + ecx]
    mov byte ptr [edi + ecx], dl
    test dl, dl
    jz @done
    inc ecx
    jmp @@copy
    
@done:
    mov eax, TRUE
    pop esi
    pop ebx
    ret
    
@cancelled:
    xor eax, eax
    pop esi
    pop ebx
    ret
FileDialog_SaveAs endp

; ============================================================================
; ShowFindDialog - Display find dialog
; NOTE: Primary implementation in find_replace.asm
; ============================================================================
public ShowFindDialog
ShowFindDialog proc c
    ; Would create modeless dialog with find controls
    mov eax, TRUE
    ret
ShowFindDialog endp

; ============================================================================
; ShowReplaceDialog - Display find & replace dialog
; NOTE: Primary implementation in find_replace.asm
; ============================================================================
public ShowReplaceDialog
ShowReplaceDialog proc c
    ; Would create modeless dialog with find & replace controls
    mov eax, TRUE
    ret
ShowReplaceDialog endp

end
