; ============================================================================
; RawrXD Agentic IDE - Config Manager (Pure MASM)
; Converted from C++ config_manager.cpp to pure MASM
; ============================================================================
; ============================================================================
; Config Manager (stub) - minimal compile-safe implementation
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

.data
include constants.inc
include structures.inc
include macros.inc

    szManualMode        db "manual",0
    szDefaultEndpoint   db "http://localhost:8080",0
    szDefaultModel      db "model.name",0

.code

; Parse stub (no-op)
ConfigManager_ParseJSON proc pBuffer:DWORD
    mov eax, TRUE
    ret
ConfigManager_ParseJSON endp

; Load stub: always initialize defaults
ConfigManager_Load proc pszFilePath:DWORD
    call ConfigManager_InitializeDefaults
    mov eax, TRUE
    ret
ConfigManager_Load endp

; Return default string (pszDefault if provided, else literal)
ConfigManager_GetString proc pszKey:DWORD, pszDefault:DWORD
    mov eax, pszDefault
    test eax, eax
    jnz @ret
    mov eax, offset szDefaultEndpoint
@ret:
    ret
ConfigManager_GetString endp

; Set string stub (no-op)
ConfigManager_SetString proc pszKey:DWORD, pszValue:DWORD
    mov eax, TRUE
    ret
ConfigManager_SetString endp

ConfigManager_InitializeDefaults proc
    mov eax, TRUE
    ret
ConfigManager_InitializeDefaults endp

end

; ============================================================================
; ConfigManager_SetString - Set string setting
; ============================================================================

ConfigManager_SetString proc pszKey:DWORD, pszValue:DWORD
    LOCAL i:DWORD
    
    ; Check if key already exists
    mov i, 0
    mov eax, g_dwSettingCount
    .while i < eax
        mov eax, i
        imul eax, sizeof SETTING_ENTRY
        lea ecx, g_Settings
        add ecx, eax
        
        invoke lstrcmp, pszKey, ecx
        .if eax == 0
            ; Update existing value
            mov eax, ecx
            add eax, 64
            invoke lstrcpy, eax, pszValue
            mov eax, 1
            ret
        .endif
        
        mov eax, i
        inc eax
        mov i, eax
        mov eax, g_dwSettingCount
    .endw
    
    ; Add new setting if space available
    mov eax, g_dwSettingCount
    cmp eax, MAX_SETTINGS
    jae @NoSpace
    
    imul eax, sizeof SETTING_ENTRY
    lea ecx, g_Settings
    add ecx, eax
    
    ; Copy key and value
    invoke lstrcpy, ecx, pszKey
    add ecx, 64
    invoke lstrcpy, ecx, pszValue
    
    mov eax, g_dwSettingCount
    inc eax
    mov g_dwSettingCount, eax
    mov eax, 1
    ret
    
@NoSpace:
    xor eax, eax
    ret
ConfigManager_SetString endp

; ============================================================================
; ConfigManager_InitializeDefaults - Initialize default settings
; ============================================================================

ConfigManager_InitializeDefaults proc
    ; Set default settings
    invoke ConfigManager_SetString, addr szAgentExecutionMode, addr szManualMode
    invoke ConfigManager_SetString, addr szModelEndpoint, addr szDefaultEndpoint
    invoke ConfigManager_SetString, addr szDefaultModel, addr szLlama2Model
    
    ret
ConfigManager_InitializeDefaults endp

; ConfigManager_ParseJSON is defined at the beginning of .code section

end