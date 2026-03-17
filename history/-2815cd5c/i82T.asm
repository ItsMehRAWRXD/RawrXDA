; ============================================================================
; RawrXD Agentic IDE - Config Manager (Pure MASM)
; Converted from C++ config_manager.cpp to pure MASM
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

; ============================================================================
; Config Manager Data
; ============================================================================

.data
    g_szConfigPath      db MAX_PATH_SIZE dup(0)
    szDefaultConfigPath db ".\config.json", 0
    szConfigNotFound    db "Config file not found, using defaults: ", 0
    szNewLine           db 13, 10, 0

; Settings storage (simplified key-value pairs)
MAX_SETTINGS equ 100
SETTING_ENTRY struct
    szKey   db 64 dup(?)
    szValue db 256 dup(?)
SETTING_ENTRY ends

g_Settings db (sizeof SETTING_ENTRY * MAX_SETTINGS) dup(?)
g_dwSettingCount dd 0

; ============================================================================
; ConfigManager_Load - Load configuration (converted from C++)
; ============================================================================

ConfigManager_Load proc pszFilePath:DWORD
    LOCAL hFile:DWORD
    LOCAL dwFileSize:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL pBuffer:DWORD
    
    ; Store config path (equivalent to configPath_ = filePath)
    invoke lstrcpy, addr g_szConfigPath, pszFilePath
    
    ; Open file (equivalent to std::ifstream file(filePath))
    invoke CreateFile, pszFilePath, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    cmp eax, INVALID_HANDLE_VALUE
    je @FileNotFound
    
    ; Get file size
    invoke GetFileSize, hFile, NULL
    mov dwFileSize, eax
    cmp eax, -1
    je @CloseFile
    
    ; Allocate buffer
    MemAlloc dwFileSize + 1
    mov pBuffer, eax
    test eax, eax
    jz @CloseFile
    
    ; Read file (equivalent to buffer << file.rdbuf())
    invoke ReadFile, hFile, pBuffer, dwFileSize, addr dwBytesRead, NULL
    test eax, eax
    jz @FreeBuffer
    
    ; Null-terminate
    mov eax, pBuffer
    add eax, dwBytesRead
    mov byte ptr [eax], 0
    
    ; Close file
    invoke CloseHandle, hFile
    
    ; Parse JSON (equivalent to deserializeFromJSON())
    invoke ConfigManager_ParseJSON, pBuffer
    
    ; Free buffer
    MemFree pBuffer
    
    mov eax, 1  ; Success
    ret
    
@FreeBuffer:
    MemFree pBuffer
    
@CloseFile:
    invoke CloseHandle, hFile
    
@FileNotFound:
    ; Output message (equivalent to std::cout)
    invoke WriteConsole, GetStdHandle(STD_OUTPUT_HANDLE), addr szConfigNotFound, 
        sizeof szConfigNotFound - 1, NULL, NULL
    invoke WriteConsole, GetStdHandle(STD_OUTPUT_HANDLE), pszFilePath, 
        -1, NULL, NULL
    invoke WriteConsole, GetStdHandle(STD_OUTPUT_HANDLE), addr szNewLine, 
        2, NULL, NULL
    
    ; Initialize defaults
    call ConfigManager_InitializeDefaults
    
    xor eax, eax  ; Failure
    ret
ConfigManager_Load endp

; ============================================================================
; ConfigManager_GetString - Get string setting (converted from C++)
; ============================================================================

ConfigManager_GetString proc pszKey:DWORD, pszDefault:DWORD
    LOCAL i:DWORD
    
    mov i, 0
    .while i < g_dwSettingCount
        mov eax, i
        imul eax, sizeof SETTING_ENTRY
        lea ecx, g_Settings[eax]
        
        ; Compare key
        invoke lstrcmp, pszKey, ecx
        .if eax == 0
            ; Found key, return value
            mov eax, ecx
            add eax, 64  ; Skip key, point to value
            ret
        .endif
        
        inc i
    .endw
    
    ; Key not found, return default
    mov eax, pszDefault
    ret
ConfigManager_GetString endp

; ============================================================================
; ConfigManager_SetString - Set string setting
; ============================================================================

ConfigManager_SetString proc pszKey:DWORD, pszValue:DWORD
    LOCAL i:DWORD
    
    ; Check if key already exists
    mov i, 0
    .while i < g_dwSettingCount
        mov eax, i
        imul eax, sizeof SETTING_ENTRY
        lea ecx, g_Settings[eax]
        
        invoke lstrcmp, pszKey, ecx
        .if eax == 0
            ; Update existing value
            mov eax, ecx
            add eax, 64
            invoke lstrcpy, eax, pszValue
            mov eax, 1
            ret
        .endif
        
        inc i
    .endw
    
    ; Add new setting if space available
    mov eax, g_dwSettingCount
    cmp eax, MAX_SETTINGS
    jae @NoSpace
    
    imul eax, sizeof SETTING_ENTRY
    lea ecx, g_Settings[eax]
    
    ; Copy key and value
    invoke lstrcpy, ecx, pszKey
    add ecx, 64
    invoke lstrcpy, ecx, pszValue
    
    inc g_dwSettingCount
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

; ============================================================================
; ConfigManager_ParseJSON - Simple JSON parsing (placeholder)
; ============================================================================

ConfigManager_ParseJSON proc pszJSON:DWORD
    ; Simple JSON parsing - in full implementation, parse key-value pairs
    ; For now, just set some defaults
    call ConfigManager_InitializeDefaults
    mov eax, 1
    ret
ConfigManager_ParseJSON endp

; Stub implementation for JSON parsing
ConfigManager_ParseJSON proc pBuffer:DWORD
    mov eax, TRUE
    ret
ConfigManager_ParseJSON endp

; ============================================================================
; Default settings
; ============================================================================

.data
    szAgentExecutionMode db "agent.execution_mode", 0
    szManualMode         db "manual", 0
    szModelEndpoint      db "model.endpoint", 0
    szDefaultEndpoint    db "http://localhost:11434", 0
    szDefaultModel       db "model.name", 0
    szLlama2Model        db "llama2", 0

end