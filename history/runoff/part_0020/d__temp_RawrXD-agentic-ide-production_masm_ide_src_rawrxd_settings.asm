;======================================================================
; RawrXD IDE - Settings Manager
; Configuration loading, preferences, theme management
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
; Settings paths
g_settingsPath[260]     DB 260 DUP(0)
g_configFile[260]       DB 260 DUP(0)
g_themeFile[260]        DB 260 DUP(0)

; Editor settings
EDITOR_TAB_SIZE         EQU 4
EDITOR_USE_SPACES       EQU 1
EDITOR_WORD_WRAP        EQU 0
EDITOR_LINE_NUMBERS     EQU 1

g_editorTabSize         DQ EDITOR_TAB_SIZE
g_editorUseSpaces       DQ EDITOR_USE_SPACES
g_editorWordWrap        DQ EDITOR_WORD_WRAP
g_editorLineNumbers     DQ EDITOR_LINE_NUMBERS
g_editorFontSize        DQ 10
g_editorFontName[32]    DB "Courier New", 0

; Appearance settings
THEME_DARK              EQU 0
THEME_LIGHT             EQU 1
THEME_CUSTOM            EQU 2

g_currentTheme          DQ THEME_DARK
g_backColor             DD 01E1E1Eh  ; Dark gray background
g_foreColor             DD 0E0E0E0h  ; Light text
g_highlightColor        DD 0264F78h  ; Blue highlight

; Build settings
g_buildOutputPath[260]  DB 260 DUP(0)
g_compilerOptimize      DQ 0
g_compilerDebugInfo     DQ 1

; File settings
g_autoSaveInterval      DQ 60000  ; 60 seconds
g_autoSaveEnabled       DQ 1
g_createBackups         DQ 1
g_recentFileCount       DQ 0

; Behavior settings
g_confirmExit           DQ 1
g_restoreSession        DQ 1
g_showStatusBar         DQ 1
g_showToolbar           DQ 1
g_showProjectTree       DQ 1

; Color table (BGR format)
g_colorTheme[16*3]:
    DD 0FF0000h             ; Keyword
    DD 000000h              ; Identifier
    DD 0FF6000h             ; Number
    DD 0008000h             ; String
    DD 0808080h             ; Comment
    DD 0FF0000h             ; Operator
    DD 0FF6000h             ; Register
    DD 0FF0000h             ; Preprocessor
    DD 0C0C0C0h             ; Background
    DD 0E0E0E0h             ; Foreground
    DD 0808080h             ; Gutter
    DD 0264F78h             ; Selection
    DD 0FFFFFFh             ; Caret
    DD 0F0F0F0h             ; Margin
    DD 0404040h             ; Folding
    DD 0606060h             ; Cursor line

.CODE

;----------------------------------------------------------------------
; RawrXD_Settings_Initialize - Load all settings from config
;----------------------------------------------------------------------
RawrXD_Settings_Initialize PROC pszAppPath:QWORD
    LOCAL szConfigPath[260]:BYTE
    
    ; Build settings path
    INVOKE lstrcpyA, ADDR g_settingsPath, pszAppPath
    INVOKE lstrcatA, ADDR g_settingsPath, "\config"
    
    ; Load main config
    INVOKE lstrcpyA, ADDR szConfigPath, ADDR g_settingsPath
    INVOKE lstrcatA, ADDR szConfigPath, "\settings.ini"
    INVOKE RawrXD_Settings_LoadConfig, ADDR szConfigPath
    
    ; Load theme
    INVOKE lstrcpyA, ADDR g_themeFile, ADDR g_settingsPath
    INVOKE lstrcatA, ADDR g_themeFile, "\theme.ini"
    INVOKE RawrXD_Settings_LoadTheme, ADDR g_themeFile
    
    xor eax, eax
    ret
    
RawrXD_Settings_Initialize ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_LoadConfig - Load settings from INI file
;----------------------------------------------------------------------
RawrXD_Settings_LoadConfig PROC pszConfigFile:QWORD
    LOCAL hFile:QWORD
    LOCAL buffer[4096]:BYTE
    LOCAL bytesRead:QWORD
    LOCAL szKey[64]:BYTE
    LOCAL szValue[256]:BYTE
    
    ; Open config file
    INVOKE CreateFileA,
        pszConfigFile,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    
    cmp rax, INVALID_HANDLE_VALUE
    je @@file_not_found
    
    mov hFile, rax
    
    ; Read entire file
    INVOKE ReadFile, hFile, ADDR buffer, 4096, ADDR bytesRead, NULL
    test eax, eax
    jz @@read_error
    
    ; Parse file
    INVOKE RawrXD_Settings_ParseConfig, ADDR buffer, bytesRead
    
    ; Close file
    INVOKE CloseHandle, hFile
    
    xor eax, eax
    ret
    
@@file_not_found:
    ; Create default config
    INVOKE RawrXD_Settings_CreateDefault
    xor eax, eax
    ret
    
@@read_error:
    INVOKE CloseHandle, hFile
    mov eax, -1
    ret
    
RawrXD_Settings_LoadConfig ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_ParseConfig - Parse INI configuration
;----------------------------------------------------------------------
RawrXD_Settings_ParseConfig PROC pBuffer:QWORD, bufSize:QWORD
    LOCAL pPos:QWORD
    LOCAL szLine[256]:BYTE
    LOCAL szKey[64]:BYTE
    LOCAL szValue[256]:BYTE
    LOCAL idx:QWORD
    
    mov pPos, pBuffer
    mov idx, 0
    
@@parse_loop:
    cmp idx, bufSize
    jge @@done
    
    ; Extract line
    INVOKE RawrXD_Settings_ExtractLine, pPos, ADDR szLine, ADDR idx
    
    ; Skip empty lines and comments
    mov al, [ADDR szLine]
    cmp al, 0
    je @@next_line
    cmp al, ';'
    je @@next_line
    cmp al, '#'
    je @@next_line
    
    ; Parse key=value
    INVOKE RawrXD_Settings_ParseKeyValue, ADDR szLine, ADDR szKey, ADDR szValue
    
    ; Apply setting
    INVOKE RawrXD_Settings_ApplySetting, ADDR szKey, ADDR szValue
    
@@next_line:
    jmp @@parse_loop
    
@@done:
    ret
    
RawrXD_Settings_ParseConfig ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_ParseKeyValue - Parse key=value pair
;----------------------------------------------------------------------
RawrXD_Settings_ParseKeyValue PROC pszLine:QWORD, pszKey:QWORD, pszValue:QWORD
    LOCAL pos:QWORD
    LOCAL eqPos:QWORD
    
    ; Find equals sign
    mov pos, 0
    mov eqPos, -1
    
@@find_eq:
    mov al, [pszLine + pos]
    test al, al
    jz @@not_found
    
    cmp al, '='
    jne @@next
    
    mov eqPos, pos
    jmp @@split
    
@@next:
    inc pos
    jmp @@find_eq
    
@@not_found:
    ; No equals found - invalid line
    mov byte [pszKey], 0
    mov byte [pszValue], 0
    ret
    
@@split:
    ; Copy key
    mov rcx, eqPos
    mov rsi, pszLine
    mov rdi, pszKey
    rep movsb
    mov byte [rdi], 0
    
    ; Copy value
    mov rsi, pszLine
    add rsi, eqPos
    inc rsi  ; Skip equals
    mov rdi, pszValue
    
@@copy_value:
    mov al, [rsi]
    cmp al, 0
    je @@value_done
    cmp al, 13  ; CR
    je @@value_done
    cmp al, 10  ; LF
    je @@value_done
    
    mov [rdi], al
    inc rsi
    inc rdi
    jmp @@copy_value
    
@@value_done:
    mov byte [rdi], 0
    ret
    
RawrXD_Settings_ParseKeyValue ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_ApplySetting - Apply individual setting
;----------------------------------------------------------------------
RawrXD_Settings_ApplySetting PROC pszKey:QWORD, pszValue:QWORD
    ; Convert value string to integer
    LOCAL intVal:QWORD
    INVOKE RawrXD_Util_StrToInt, pszValue
    mov intVal, rax
    
    ; Check key and apply
    INVOKE lstrcmpA, pszKey, "TabSize"
    test eax, eax
    jnz @@not_tab
    mov g_editorTabSize, intVal
    jmp @@done
    
@@not_tab:
    INVOKE lstrcmpA, pszKey, "UseSpaces"
    test eax, eax
    jnz @@not_spaces
    mov g_editorUseSpaces, intVal
    jmp @@done
    
@@not_spaces:
    INVOKE lstrcmpA, pszKey, "WordWrap"
    test eax, eax
    jnz @@not_wrap
    mov g_editorWordWrap, intVal
    jmp @@done
    
@@not_wrap:
    INVOKE lstrcmpA, pszKey, "LineNumbers"
    test eax, eax
    jnz @@not_lines
    mov g_editorLineNumbers, intVal
    jmp @@done
    
@@not_lines:
    INVOKE lstrcmpA, pszKey, "FontSize"
    test eax, eax
    jnz @@not_font
    mov g_editorFontSize, intVal
    jmp @@done
    
@@not_font:
    INVOKE lstrcmpA, pszKey, "Theme"
    test eax, eax
    jnz @@not_theme
    mov g_currentTheme, intVal
    jmp @@done
    
@@not_theme:
    INVOKE lstrcmpA, pszKey, "AutoSave"
    test eax, eax
    jnz @@not_auto
    mov g_autoSaveEnabled, intVal
    jmp @@done
    
@@not_auto:
    INVOKE lstrcmpA, pszKey, "AutoSaveInterval"
    test eax, eax
    jnz @@not_interval
    mov g_autoSaveInterval, intVal
    
@@not_interval:
@@done:
    ret
    
RawrXD_Settings_ApplySetting ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_ExtractLine - Extract next line from config
;----------------------------------------------------------------------
RawrXD_Settings_ExtractLine PROC pBuffer:QWORD, pszLine:QWORD, pIdx:QWORD
    LOCAL pos:QWORD
    LOCAL idx:QWORD
    
    mov rax, pIdx
    mov idx, [rax]
    mov pos, 0
    
@@copy_loop:
    mov al, [pBuffer + idx]
    
    ; Check for line ending
    cmp al, 13  ; CR
    je @@end
    cmp al, 10  ; LF
    je @@end
    cmp al, 0   ; Null
    je @@end
    
    ; Copy character
    mov [pszLine + pos], al
    inc pos
    inc idx
    
    cmp pos, 255
    jl @@copy_loop
    
@@end:
    ; Null terminate
    mov byte [pszLine + pos], 0
    
    ; Skip line ending
    mov al, [pBuffer + idx]
    cmp al, 13
    jne @@check_lf
    inc idx
@@check_lf:
    cmp byte [pBuffer + idx], 10
    jne @@update_idx
    inc idx
    
@@update_idx:
    mov rax, pIdx
    mov [rax], idx
    ret
    
RawrXD_Settings_ExtractLine ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_LoadTheme - Load theme configuration
;----------------------------------------------------------------------
RawrXD_Settings_LoadTheme PROC pszThemeFile:QWORD
    ; Load theme colors and apply
    ; For now, use default theme
    ret
RawrXD_Settings_LoadTheme ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_CreateDefault - Create default config
;----------------------------------------------------------------------
RawrXD_Settings_CreateDefault PROC
    ; Set all defaults (already done in data section)
    ret
RawrXD_Settings_CreateDefault ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_SaveConfig - Save settings to file
;----------------------------------------------------------------------
RawrXD_Settings_SaveConfig PROC pszConfigFile:QWORD
    LOCAL hFile:QWORD
    LOCAL buffer[4096]:BYTE
    LOCAL pos:QWORD
    LOCAL szLine[256]:BYTE
    LOCAL bytesWritten:QWORD
    
    ; Create config file
    INVOKE CreateFileA,
        pszConfigFile,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    
    cmp rax, INVALID_HANDLE_VALUE
    je @@fail
    
    mov hFile, rax
    mov pos, 0
    
    ; Write editor settings
    INVOKE RawrXD_Settings_WriteEditorSettings, hFile
    INVOKE RawrXD_Settings_WriteAppearanceSettings, hFile
    INVOKE RawrXD_Settings_WriteBuildSettings, hFile
    INVOKE RawrXD_Settings_WriteFileSettings, hFile
    
    ; Close file
    INVOKE CloseHandle, hFile
    
    xor eax, eax
    ret
    
@@fail:
    mov eax, -1
    ret
    
RawrXD_Settings_SaveConfig ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_WriteEditorSettings - Write editor config
;----------------------------------------------------------------------
RawrXD_Settings_WriteEditorSettings PROC hFile:QWORD
    LOCAL szBuffer[512]:BYTE
    LOCAL bytesWritten:QWORD
    
    ; Write editor section header
    INVOKE lstrcpyA, ADDR szBuffer, "[Editor]", 13, 10
    INVOKE WriteFile, hFile, ADDR szBuffer, lstrlenA(ADDR szBuffer), ADDR bytesWritten, NULL
    
    ; Write settings
    mov eax, g_editorTabSize
    INVOKE wsprintf, ADDR szBuffer, "TabSize=%d", eax
    INVOKE WriteFile, hFile, ADDR szBuffer, lstrlenA(ADDR szBuffer), ADDR bytesWritten, NULL
    
    ret
    
RawrXD_Settings_WriteEditorSettings ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_WriteAppearanceSettings - Write appearance config
;----------------------------------------------------------------------
RawrXD_Settings_WriteAppearanceSettings PROC hFile:QWORD
    ret
RawrXD_Settings_WriteAppearanceSettings ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_WriteBuildSettings - Write build config
;----------------------------------------------------------------------
RawrXD_Settings_WriteBuildSettings PROC hFile:QWORD
    ret
RawrXD_Settings_WriteBuildSettings ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_WriteFileSettings - Write file config
;----------------------------------------------------------------------
RawrXD_Settings_WriteFileSettings PROC hFile:QWORD
    ret
RawrXD_Settings_WriteFileSettings ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_GetThemeColor - Get color for theme element
;----------------------------------------------------------------------
RawrXD_Settings_GetThemeColor PROC elementIdx:QWORD
    ; Bounds check
    cmp elementIdx, 16
    jae @@default
    
    ; Index into color table
    mov rax, elementIdx
    imul rax, 3
    mov rcx, OFFSET g_colorTheme
    add rcx, rax
    mov eax, [rcx]
    
    ret
    
@@default:
    mov eax, 0
    ret
    
RawrXD_Settings_GetThemeColor ENDP

;----------------------------------------------------------------------
; RawrXD_Settings_SetThemeColor - Set color for theme element
;----------------------------------------------------------------------
RawrXD_Settings_SetThemeColor PROC elementIdx:QWORD, color:QWORD
    cmp elementIdx, 16
    jae @@fail
    
    mov rax, elementIdx
    imul rax, 3
    mov rcx, OFFSET g_colorTheme
    add rcx, rax
    mov eax, color
    mov [rcx], eax
    
    xor eax, eax
    ret
    
@@fail:
    mov eax, -1
    ret
    
RawrXD_Settings_SetThemeColor ENDP

END
