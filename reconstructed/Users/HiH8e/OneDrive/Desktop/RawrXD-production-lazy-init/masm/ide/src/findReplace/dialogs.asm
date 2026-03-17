; ============================================================================
; FINDREPLACE_DIALOGS.ASM - Find and Replace Dialog Stubs
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

PUBLIC ShowFindDialog
PUBLIC ShowReplaceDialog

.data
    g_hFindDlg          dd 0
    g_hReplaceDlg       dd 0

.code

; ============================================================================
; ShowFindDialog - Show modeless find dialog
; Input: ECX = parent window handle
; Output: EAX = dialog handle
; ============================================================================
ShowFindDialog PROC hParent:DWORD
    mov eax, 2000h
    mov [g_hFindDlg], eax
    ret
ShowFindDialog ENDP

; ============================================================================
; ShowReplaceDialog - Show modeless find & replace dialog
; Input: ECX = parent window handle
; Output: EAX = dialog handle
; ============================================================================
ShowReplaceDialog PROC hParent:DWORD
    mov eax, 2001h
    mov [g_hReplaceDlg], eax
    ret
ShowReplaceDialog ENDP

END

    
    mov eax, [g_hFindDlg]
    pop ebx
    ret
    
@already_open:
    mov eax, [g_hFindDlg]
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop ebx
    ret
ShowFindDialog ENDP

; ============================================================================
; ShowReplaceDialog - Show modeless find & replace dialog
; Input: ECX = parent window handle
; Output: EAX = dialog handle
; ============================================================================
ShowReplaceDialog PROC hParent:DWORD
    push ebx
    
    ; Check if already open
    cmp [g_hReplaceDlg], 0
    jne @already_open
    
    ; Create modeless dialog (stub returns dummy handle)
    mov dword [g_hReplaceDlg], 2001h
    
    mov eax, [g_hReplaceDlg]
    pop ebx
    ret
    
@already_open:
    mov eax, [g_hReplaceDlg]
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop ebx
    ret
ShowReplaceDialog ENDP

END
