; ============================================================================
; RawrXD Agentic IDE - Config Manager (Enterprise In-Place Parser)
; Zero-allocation, linear scan, robust error handling.
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\shlwapi.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\shlwapi.lib

CONFIG_ENTRY STRUCT
    pKey   DWORD ?
    pValue DWORD ?
CONFIG_ENTRY ENDS

.data
    config_file  db "config.ini", 0
    MAX_ENTRIES  EQU 32
    entry_count  dd 0
    szDefaultEndpoint db "http://localhost:8080", 0

.data?
    file_buffer  db 4096 dup(?)
    config_table CONFIG_ENTRY MAX_ENTRIES dup(<0,0>)

.code

; --- Internal Helper: Skip Spaces/Tabs ---
SkipWhitespace PROC
    @@:
    mov al, [esi]
    cmp al, ' '
    je @f
    cmp al, 09h
    je @f
    ret
    @@: inc esi
    jmp @b
SkipWhitespace ENDP

; --- Main Parser: Reads config.ini and builds symbol table ---
LoadConfig PROC public
    LOCAL hFile:HANDLE
    LOCAL bytesRead:DWORD

    mov entry_count, 0
    invoke CreateFile, addr config_file, GENERIC_READ, FILE_SHARE_READ, \
           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @Exit

    mov hFile, eax
    invoke ReadFile, hFile, addr file_buffer, 4096, addr bytesRead, NULL
    invoke CloseHandle, hFile
    
    mov eax, bytesRead
    cmp eax, 4096
    jge @Full
    mov byte ptr [file_buffer + eax], 0
    jmp @StartParse
@Full:
    mov byte ptr [file_buffer + 4095], 0

@StartParse:
    lea esi, file_buffer
    lea edi, config_table
    xor ebx, ebx

@NextLine:
    cmp ebx, MAX_ENTRIES
    jge @Done
    call SkipWhitespace
    mov al, [esi]
    test al, al
    jz @Done
    cmp al, ';'
    je @SkipLine
    cmp al, '#'
    je @SkipLine
    cmp al, 0Dh
    je @SkipLine
    cmp al, 0Ah
    je @SkipLine

    mov [edi].pKey, esi
@FindEqu:
    mov al, [esi]
    test al, al
    jz @Done
    cmp al, '='
    je @FoundEqu
    cmp al, 0Dh
    je @SkipLine
    inc esi
    jmp @FindEqu

@FoundEqu:
    mov byte ptr [esi], 0
    inc esi
    call SkipWhitespace
    mov dword ptr [edi], esi  ; pValue field (offset 4 in struct)

@FindEOL:
    mov al, [esi]
    cmp al, 0Dh
    je @Term
    cmp al, 0Ah
    je @Term
    test al, al
    jz @Term
    inc esi
    jmp @FindEOL

@Term:
    mov byte ptr [esi], 0
    inc esi
    add edi, TYPE CONFIG_ENTRY
    inc ebx
    jmp @NextLine

@SkipLine:
    mov al, [esi]
    test al, al
    jz @Done
    inc esi
    cmp al, 0Ah
    jne @SkipLine
    jmp @NextLine

@Done:
    mov entry_count, ebx
@Exit:
    mov eax, TRUE
    ret
LoadConfig ENDP

; --- Get Integer Value ---
GetConfigInt PROC public pKeyName:DWORD, defaultVal:DWORD
    xor ecx, ecx
@Loop:
    cmp ecx, entry_count
    jge @NotFound
    push ecx
    mov eax, ecx
    shl eax, 3 
    invoke lstrcmpi, [config_table + eax].pKey, pKeyName
    pop ecx
    test eax, eax
    jz @Found
    inc ecx
    jmp @Loop

@Found:
    mov eax, ecx
    shl eax, 3
    invoke StrToInt, [config_table + eax].pValue
    ret

@NotFound:
    mov eax, defaultVal
    ret
GetConfigInt ENDP

; --- Get String Value ---
GetConfigString PROC public pKeyName:DWORD, pDefault:DWORD
    xor ecx, ecx
@Loop:
    cmp ecx, entry_count
    jge @NotFound
    push ecx
    mov eax, ecx
    shl eax, 3 
    invoke lstrcmpi, [config_table + eax].pKey, pKeyName
    pop ecx
    test eax, eax
    jz @Found
    inc ecx
    jmp @Loop

@Found:
    mov eax, ecx
    shl eax, 3
    mov eax, [config_table + eax].pValue
    ret

@NotFound:
    mov eax, pDefault
    ret
GetConfigString ENDP

; --- Compatibility Stubs ---
ConfigManager_Load PROC pszPath:DWORD
    invoke LoadConfig
    ret
ConfigManager_Load ENDP

ConfigManager_GetString PROC pszKey:DWORD, pszDefault:DWORD
    invoke GetConfigString, pszKey, pszDefault
    ret
ConfigManager_GetString ENDP

ConfigManager_InitializeDefaults PROC
    mov eax, 1
    ret
ConfigManager_InitializeDefaults ENDP

ConfigManager_SetString PROC pszKey:DWORD, pszValue:DWORD
    mov eax, 1
    ret
ConfigManager_SetString ENDP

END