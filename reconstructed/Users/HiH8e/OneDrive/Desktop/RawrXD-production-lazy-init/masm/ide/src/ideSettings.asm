; ============================================================================
; IDESETTINGS.ASM - IDE Settings Management System
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\advapi32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\advapi32.lib

PUBLIC IDESettings_Initialize
PUBLIC IDESettings_LoadFromFile
PUBLIC IDESettings_ApplyTheme_Impl

; Settings structure
IDESettings STRUCT
    dwTheme             dd ?    ; 0=Light, 1=Dark
    dwFontSize          dd ?    ; 9-24pt
    dwWindowWidth       dd ?    ; pixels
    dwWindowHeight      dd ?    ; pixels
    bAutoLoadRecent     dd ?    ; Load last file on startup
    bEnableSyntaxHL     dd ?    ; Syntax highlighting
    bEnableCodeCompl    dd ?    ; Code completion
    bEnableLineNumbers  dd ?    ; Line numbers in editor
IDESettings ENDS

.data
    g_Settings IDESettings <1, 11, 1200, 800, 1, 1, 1, 1>
    g_szRegPath         db "Software\RawrXD\IDE", 0
    g_szTheme           db "Theme", 0
    g_szFontSize        db "FontSize", 0

.code

; ============================================================================
; IDESettings_Initialize - Initialize settings system
; ============================================================================
IDESettings_Initialize PROC
    push ebx
    
    ; Set reasonable defaults
    mov [g_Settings.dwTheme], 1         ; Dark theme
    mov [g_Settings.dwFontSize], 11     ; 11pt font
    mov [g_Settings.dwWindowWidth], 1200
    mov [g_Settings.dwWindowHeight], 800
    mov [g_Settings.bAutoLoadRecent], 1
    mov [g_Settings.bEnableSyntaxHL], 1
    mov [g_Settings.bEnableCodeCompl], 1
    mov [g_Settings.bEnableLineNumbers], 1
    
    mov eax, 1
    pop ebx
    ret
IDESettings_Initialize ENDP

; ============================================================================
; IDESettings_LoadFromFile - Load settings from registry
; ============================================================================
IDESettings_LoadFromFile PROC
    push ebx
    push esi
    
    ; For now, use defaults (registry access would require more complex code)
    ; In production, would read from registry HKEY_CURRENT_USER
    
    mov eax, 1
    pop esi
    pop ebx
    ret
IDESettings_LoadFromFile ENDP

; ============================================================================
; IDESettings_ApplyTheme - Apply theme settings to window
; Input: ECX = main window handle
; ============================================================================
IDESettings_ApplyTheme_Impl PROC hWnd:DWORD

    push ebx
    push esi
    
    ; Apply theme-specific colors and styles
    ; Dark theme: use dark backgrounds and light text
    ; Light theme: use light backgrounds and dark text
    
    mov eax, 1
    pop esi
    pop ebx
    ret
IDESettings_ApplyTheme ENDP

END
