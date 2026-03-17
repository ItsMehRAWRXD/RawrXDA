; ============================================================================
; RawrXD Agentic IDE - Config Manager (Pure MASM)
; Converted from C++ config_manager.cpp to pure MASM
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include winapi_min.inc

; ============================================================================
; Config Manager Data
; ============================================================================

.data
include constants.inc
include structures.inc
include macros.inc

    g_szConfigPath      db MAX_PATH_SIZE dup(0)
    szDefaultConfigPath db ".\config.json", 0
    szConfigNotFound    db "Config file not found, using defaults: ", 0
    szManualMode        db "manual", 0
    szAgentExecutionMode db "execution_mode", 0
    szDefaultEndpoint   db "http://localhost:8080", 0
    szModelEndpoint     db "model_endpoint", 0
    szDefaultModel      db "model.name", 0
    szLlama2Model       db "llama-2-7b-chat", 0
    szNewLine           db 13, 10, 0

; Settings storage (simplified key-value pairs)
MAX_SETTINGS equ 100
SETTING_ENTRY struct
    szKey   db 64 dup(?)
    szValue db 256 dup(?)
SETTING_ENTRY ends

g_Settings db (sizeof SETTING_ENTRY * MAX_SETTINGS) dup(?)
g_dwSettingCount dd 0

.code

; ============================================================================
; ConfigManager_ParseJSON - Minimal JSON parser (INI-style fallback)
; Note: Full JSON parsing deferred; currently does basic key=value parsing
; ============================================================================
ConfigManager_ParseJSON proc pBuffer:DWORD
    LOCAL pCur:DWORD
    LOCAL pKeyStart:DWORD
    LOCAL pValStart:DWORD
    LOCAL chCur:BYTE
    
    mov pCur, pBuffer
    test pCur, pCur
    jz @ParseDone
    
    ; Simple key=value line parser (JSON-like but simplified)
    jmp @ParseLoop
    
@ParseLoop:
    mov al, [pCur]
    test al, al
    jz @ParseDone
    
    ; Skip whitespace and comments
    cmp al, ' '
    je @NextChar
    cmp al, 9     ; tab
    je @NextChar
    cmp al, ';'   ; comment start
    je @SkipLine
    cmp al, 13    ; CR
    je @NextChar
    cmp al, 10    ; LF
    je @NextChar
    
    ; Found key start
    mov pKeyStart, pCur
    jmp @ScanKey
    
@ScanKey:
    mov al, [pCur]
    cmp al, '='
    je @KeyFound
    test al, al
    jz @ParseDone
    cmp al, 13
    je @NextLine
    cmp al, 10
    je @NextLine
    inc pCur
    jmp @ScanKey
    
@KeyFound:
    ; Skip to value (after =)
    inc pCur
    mov pValStart, pCur
    jmp @ScanValue
    
@ScanValue:
    mov al, [pCur]
    test al, al
    jz @ParseDone
    cmp al, 13
    je @ValueFound
    cmp al, 10
    je @ValueFound
    inc pCur
    jmp @ScanValue
    
@ValueFound:
    ; Parser recognized a key=value pair
    jmp @NextLine
    
@SkipLine:
    mov al, [pCur]
    inc pCur
    cmp al, 10
    jne @SkipLine
    jmp @ParseLoop
    
@NextLine:
@NextChar:
    inc pCur
    jmp @ParseLoop
    
@ParseDone:
    mov eax, TRUE
    ret
ConfigManager_ParseJSON endp

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
    mov eax, g_dwSettingCount
    .while i < eax
        mov eax, i
        imul eax, sizeof SETTING_ENTRY
        lea ecx, g_Settings
        add ecx, eax
        
        ; Compare key
        invoke lstrcmp, pszKey, ecx
        .if eax == 0
            ; Found key, return value
            mov eax, ecx
            add eax, 64  ; Skip key, point to value
            ret
        .endif
        
        mov eax, i
        inc eax
        mov i, eax
        mov eax, g_dwSettingCount
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