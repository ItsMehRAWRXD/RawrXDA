; ============================================================================
; ALL_STUBS.ASM - Complete stub library for all missing symbols
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; All public exports (only ones NOT already provided by other modules)
PUBLIC ErrorLogging_LogEvent
PUBLIC InferenceBackend_Init
PUBLIC InferenceBackend_SelectBackend
PUBLIC InferenceBackend_CreateInferenceContext
PUBLIC Editor_Init
PUBLIC Editor_WndProc
PUBLIC FileDialog_Open
PUBLIC FileDialog_SaveAs
PUBLIC IDEPaneSystem_Initialize
PUBLIC IDEPaneSystem_CreateDefaultLayout
PUBLIC UIGguf_CreateMenuBar
PUBLIC UIGguf_CreateToolbar
PUBLIC UIGguf_CreateStatusPane
PUBLIC IDESettings_Initialize
PUBLIC IDESettings_LoadFromFile
PUBLIC IDESettings_ApplyTheme
PUBLIC FileDialogs_Initialize

.code

; Logging stub
ErrorLogging_LogEvent PROC dwLevel:DWORD, pMessage:DWORD
    ret
ErrorLogging_LogEvent ENDP

; Inference backend stubs
InferenceBackend_Init PROC
    mov eax, 1
    ret
InferenceBackend_Init ENDP

InferenceBackend_SelectBackend PROC dwType:DWORD
    mov eax, 1
    ret
InferenceBackend_SelectBackend ENDP

InferenceBackend_CreateInferenceContext PROC hBackend:DWORD
    xor eax, eax
    ret
InferenceBackend_CreateInferenceContext ENDP

; Editor stubs
Editor_Init PROC
    mov eax, 1
    ret
Editor_Init ENDP

Editor_WndProc PROC hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret
Editor_WndProc ENDP

; Dialog stubs
FileDialog_Open PROC hParent:DWORD
    mov eax, 1
    ret
FileDialog_Open ENDP

FileDialog_SaveAs PROC hParent:DWORD, dwType:DWORD
    mov eax, 1
    ret
FileDialog_SaveAs ENDP

ShowFindDialog PROC hParent:DWORD
    mov eax, 1
    ret
ShowFindDialog ENDP

ShowReplaceDialog PROC hParent:DWORD
    mov eax, 1
    ret
ShowReplaceDialog ENDP

; Pane system stubs
IDEPaneSystem_Initialize PROC
    mov eax, 1
    ret
IDEPaneSystem_Initialize ENDP

IDEPaneSystem_CreateDefaultLayout PROC hParent:DWORD
    mov eax, 1
    ret
IDEPaneSystem_CreateDefaultLayout ENDP

; UI stubs
UIGguf_CreateMenuBar PROC hWnd:DWORD
    xor eax, eax
    ret
UIGguf_CreateMenuBar ENDP

UIGguf_CreateToolbar PROC hWnd:DWORD
    xor eax, eax
    ret
UIGguf_CreateToolbar ENDP

UIGguf_CreateStatusPane PROC hWnd:DWORD
    xor eax, eax
    ret
UIGguf_CreateStatusPane ENDP

; Settings stubs
IDESettings_Initialize PROC
    mov eax, 1
    ret
IDESettings_Initialize ENDP

IDESettings_LoadFromFile PROC
    ret
IDESettings_LoadFromFile ENDP

IDESettings_ApplyTheme PROC
    ret
IDESettings_ApplyTheme ENDP

FileDialogs_Initialize PROC
    ret
FileDialogs_Initialize ENDP

END
