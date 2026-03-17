; ============================================================================
; FILEDIALOGS_INIT.ASM - File Dialog System Initialization
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

PUBLIC FileDialogs_Initialize

.data
    ; Default file dialog settings
    g_szModelDir        db "C:\Models\", 0
    g_szSourceDir       db "C:\Projects\", 0
    g_szDefaultFilter   db "GGUF Models (*.gguf)|*.gguf|All Files (*.*)|*.*", 0
    g_bDialogsReady     dd 0

.code

; ============================================================================
; FileDialogs_Initialize - Initialize file dialog system
; ============================================================================
FileDialogs_Initialize PROC
    push ebx
    push esi
    push edi
    
    ; Check if already initialized
    cmp [g_bDialogsReady], 1
    je @already_init
    
    ; Create default directories if they don't exist
    invoke CreateDirectoryA, ADDR g_szModelDir, NULL
    invoke CreateDirectoryA, ADDR g_szSourceDir, NULL
    
    ; Set initialized flag
    mov [g_bDialogsReady], 1
    
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
    
@already_init:
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
FileDialogs_Initialize ENDP

END
