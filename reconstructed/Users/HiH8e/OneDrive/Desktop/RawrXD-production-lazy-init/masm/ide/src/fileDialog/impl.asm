; ============================================================================
; FILEDIALOG_IMPL.ASM - File Open/Save Dialog Stubs
; ============================================================================

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

PUBLIC FileDialog_Open
PUBLIC FileDialog_SaveAs

Dialog_FileOpen PROTO :DWORD, :DWORD, :DWORD
Dialog_FileSave PROTO :DWORD, :DWORD, :DWORD

.data
    g_szSelectedFile    db 260 dup(0)
    szFilterAll         db "All Files (*.*)",0,"*.*",0,0
    szFilterSource      db "Source Files (*.asm;*.c;*.h)",0,"*.asm;*.c;*.h",0
                        db "All Files (*.*)",0,"*.*",0,0

.code

; ============================================================================
; FileDialog_Open - Show open file dialog
; Input: hParent = parent window handle
; Output: EAX = 1 if file selected, 0 if cancelled
; ============================================================================
FileDialog_Open PROC hParent:DWORD
    lea eax, g_szSelectedFile
    mov byte ptr [eax], 0
    push offset szFilterAll
    push eax
    push hParent
    call Dialog_FileOpen
    ret
FileDialog_Open ENDP

; ============================================================================
; FileDialog_SaveAs - Show save file dialog
; Input: hParent = parent window handle, dwType = reserved
; Output: EAX = 1 if file selected, 0 if cancelled
; ============================================================================
FileDialog_SaveAs PROC hParent:DWORD
    lea eax, g_szSelectedFile
    mov byte ptr [eax], 0
    push offset szFilterSource
    push eax
    push hParent
    call Dialog_FileSave
    ret
FileDialog_SaveAs ENDP

END

    mov [eax].OPENFILENAMEA.nMaxCustFilter, 0
    mov [eax].OPENFILENAMEA.nFilterIndex, 1
    lea ebx, g_szSelectedFile
    mov [eax].OPENFILENAMEA.lpstrFile, ebx
    mov [eax].OPENFILENAMEA.nMaxFile, 256
    mov [eax].OPENFILENAMEA.lpstrFileTitle, 0
    mov [eax].OPENFILENAMEA.nMaxFileTitle, 0
    lea ebx, szDefaultPath
    mov [eax].OPENFILENAMEA.lpstrInitialDir, ebx
    mov [eax].OPENFILENAMEA.lpstrTitle, 0
    mov [eax].OPENFILENAMEA.Flags, OFN_PATHMUSTEXIST or OFN_FILEMUSTEXIST
    
    ; Show dialog
    invoke GetOpenFileNameA, ADDR ofn
    test eax, eax
    jz @cancelled
    
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
    
@cancelled:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
FileDialog_Open ENDP

; ============================================================================
; FileDialog_SaveAs - Show save file dialog
; Input: ECX = parent window handle, EDX = dwType (unused)
; Output: EAX = 1 if file selected, 0 if cancelled
; ============================================================================
FileDialog_SaveAs PROC hParent:DWORD, dwType:DWORD
    LOCAL ofn:OPENFILENAMEA
    push ebx
    push esi
    push edi
    
    ; Initialize OPENFILENAME for save
    lea eax, ofn
    mov [eax].OPENFILENAMEA.lStructSize, sizeof OPENFILENAMEA
    mov [eax].OPENFILENAMEA.hwndOwner, hParent
    mov [eax].OPENFILENAMEA.hInstance, 0
    lea ebx, szFilterSource
    mov [eax].OPENFILENAMEA.lpstrFilter, ebx
    mov [eax].OPENFILENAMEA.lpstrCustomFilter, 0
    mov [eax].OPENFILENAMEA.nMaxCustFilter, 0
    mov [eax].OPENFILENAMEA.nFilterIndex, 1
    lea ebx, g_szSelectedFile
    mov [eax].OPENFILENAMEA.lpstrFile, ebx
    mov [eax].OPENFILENAMEA.nMaxFile, 256
    mov [eax].OPENFILENAMEA.lpstrFileTitle, 0
    mov [eax].OPENFILENAMEA.nMaxFileTitle, 0
    lea ebx, szDefaultPath
    mov [eax].OPENFILENAMEA.lpstrInitialDir, ebx
    mov [eax].OPENFILENAMEA.lpstrTitle, 0
    mov [eax].OPENFILENAMEA.Flags, OFN_OVERWRITEPROMPT
    
    ; Show save dialog
    invoke GetSaveFileNameA, ADDR ofn
    test eax, eax
    jz @cancelled
    
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
    
@cancelled:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
FileDialog_SaveAs ENDP

END
