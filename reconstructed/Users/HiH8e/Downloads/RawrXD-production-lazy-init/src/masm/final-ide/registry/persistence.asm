; ============================================================================
; Registry Persistence Layer for RawrXD Pure MASM IDE
; ============================================================================
; Windows Registry API wrapper for settings persistence
; ============================================================================

include windows.inc
include kernel32.inc
include advapi32.inc
include "settings_dialog.inc"

.DATA

; ============================================================================
; Registry Constants
; ============================================================================
REGISTRY_BASE_PATH     DB "Software\\RawrXD-QtShell\\Settings",0
REG_GENERAL_PATH       DB "General",0
REG_MODEL_PATH         DB "Model",0
REG_CHAT_PATH          DB "Chat",0
REG_SECURITY_PATH      DB "Security",0
REG_TRAINING_PATH      DB "Training",0
REG_CICD_PATH          DB "CI_CD",0
REG_ENTERPRISE_PATH    DB "Enterprise",0

; Registry value names
REG_AUTO_SAVE          DB "AutoSave",0
REG_STARTUP_FULLSCREEN DB "StartupFullscreen",0
REG_FONT_SIZE          DB "FontSize",0
REG_MODEL_PATH_VAL     DB "ModelPath",0
REG_DEFAULT_MODEL      DB "DefaultModel",0
REG_CHAT_MODEL         DB "ChatModel",0
REG_TEMPERATURE        DB "Temperature",0
REG_MAX_TOKENS         DB "MaxTokens",0
REG_SYSTEM_PROMPT      DB "SystemPrompt",0
REG_API_KEY            DB "ApiKey",0
REG_ENCRYPTION         DB "Encryption",0
REG_SECURE_STORAGE     DB "SecureStorage",0

.CODE

; ============================================================================
; RegistryOpenKey: Open or create registry key
; Parameters:
;   rcx = hKey (HKEY_CURRENT_USER, etc.)
;   rdx = subkey path
;   r8 = access rights
; Returns:
;   rax = hKey (0 if failed)
; ============================================================================
RegistryOpenKey PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 40h
    
    mov rbx, rcx  ; hKey
    mov rsi, rdx  ; subkey
    mov rdi, r8   ; access
    
    ; Call RegCreateKeyExW
    lea r9, [rsp+20h]  ; phkResult
    push 0              ; lpdwDisposition
    push 0              ; lpSecurityAttributes
    push 0              ; dwOptions
    push 0              ; lpClass
    push rdi            ; samDesired
    push 0              ; Reserved
    push rsi            ; lpSubKey
    push rbx            ; hKey
    call RegCreateKeyExW
    
    test eax, eax
    jnz open_failed
    
    mov rax, [rsp+20h]  ; return hKey
    jmp open_complete
    
open_failed:
    xor rax, rax
    
open_complete:
    add rsp, 40h
    pop rdi
    pop rsi
    pop rbx
    ret
RegistryOpenKey ENDP

; ============================================================================
; RegistryCloseKey: Close registry key
; Parameters:
;   rcx = hKey
; Returns:
;   rax = 1 if success
; ============================================================================
RegistryCloseKey PROC
    push rcx
    call RegCloseKey
    mov rax, 1
    ret
RegistryCloseKey ENDP

; ============================================================================
; RegistrySetDWORD: Set DWORD value in registry
; Parameters:
;   rcx = hKey
;   rdx = value name
;   r8 = DWORD value
; Returns:
;   rax = 1 if success
; ============================================================================
RegistrySetDWORD PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rbx, rcx  ; hKey
    mov rsi, rdx  ; value name
    mov edi, r8d  ; value
    
    ; Call RegSetValueExW
    push sizeof(DWORD)  ; cbData
    lea r9, rdi        ; lpData
    push REG_DWORD     ; dwType
    push 0             ; Reserved
    push rsi           ; lpValueName
    push rbx           ; hKey
    call RegSetValueExW
    
    test eax, eax
    jnz set_failed
    
    mov rax, 1
    jmp set_complete
    
set_failed:
    xor rax, rax
    
set_complete:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
RegistrySetDWORD ENDP

; ============================================================================
; RegistryGetDWORD: Get DWORD value from registry
; Parameters:
;   rcx = hKey
;   rdx = value name
;   r8 = default value
; Returns:
;   rax = DWORD value (default if not found)
; ============================================================================
RegistryGetDWORD PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 30h
    
    mov rbx, rcx  ; hKey
    mov rsi, rdx  ; value name
    mov edi, r8d  ; default value
    
    ; Prepare for RegQueryValueExW
    mov dword ptr [rsp+20h], sizeof(DWORD)  ; lpcbData
    lea r9, [rsp+24h]                       ; lpData
    push REG_DWORD                          ; lpType
    push 0                                  ; Reserved
    push rsi                                ; lpValueName
    push rbx                                ; hKey
    call RegQueryValueExW
    
    test eax, eax
    jnz get_failed
    
    mov eax, [rsp+24h]  ; return value
    jmp get_complete
    
get_failed:
    mov eax, edi        ; return default
    
get_complete:
    add rsp, 30h
    pop rdi
    pop rsi
    pop rbx
    ret
RegistryGetDWORD ENDP

; ============================================================================
; RegistrySetString: Set string value in registry
; Parameters:
;   rcx = hKey
;   rdx = value name
;   r8 = string pointer
; Returns:
;   rax = 1 if success
; ============================================================================
RegistrySetString PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rbx, rcx  ; hKey
    mov rsi, rdx  ; value name
    mov rdi, r8   ; string
    
    ; Get string length
    mov rcx, rdi
    call lstrlenW
    mov r9d, eax
    add r9d, r9d  ; multiply by 2 for wide chars
    add r9d, 2    ; include null terminator
    
    ; Call RegSetValueExW
    push r9d              ; cbData
    push rdi              ; lpData
    push REG_SZ           ; dwType
    push 0                ; Reserved
    push rsi              ; lpValueName
    push rbx              ; hKey
    call RegSetValueExW
    
    test eax, eax
    jnz set_failed
    
    mov rax, 1
    jmp set_complete
    
set_failed:
    xor rax, rax
    
set_complete:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
RegistrySetString ENDP

; ============================================================================
; RegistryGetString: Get string value from registry
; Parameters:
;   rcx = hKey
;   rdx = value name
;   r8 = buffer pointer
;   r9 = buffer size (in characters)
; Returns:
;   rax = string length (0 if not found)
; ============================================================================
RegistryGetString PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 30h
    
    mov rbx, rcx  ; hKey
    mov rsi, rdx  ; value name
    mov rdi, r8   ; buffer
    mov r10, r9   ; buffer size
    
    ; Prepare for RegQueryValueExW
    mov [rsp+20h], r10  ; lpcbData (in bytes)
    mov r9, rdi         ; lpData
    push REG_SZ         ; lpType
    push 0              ; Reserved
    push rsi            ; lpValueName
    push rbx            ; hKey
    call RegQueryValueExW
    
    test eax, eax
    jnz get_failed
    
    ; Calculate string length (in characters)
    mov rax, [rsp+20h]  ; bytes read
    shr rax, 1          ; convert to characters
    dec rax             ; exclude null terminator
    jmp get_complete
    
get_failed:
    xor rax, rax
    
get_complete:
    add rsp, 30h
    pop rdi
    pop rsi
    pop rbx
    ret
RegistryGetString ENDP

; ============================================================================
; LoadSettingsFromRegistry: Load all settings from registry
; Parameters:
;   rcx = SETTINGS_DATA pointer
; Returns:
;   rax = 1 if success
; ============================================================================
LoadSettingsFromRegistry PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rbx, rcx ; SETTINGS_DATA pointer
    
    ; Open Base Key
    mov rcx, HKEY_CURRENT_USER
    lea rdx, REGISTRY_BASE_PATH
    mov r8, KEY_READ
    call RegistryOpenKey
    test rax, rax
    jz load_failed
    
    mov rsi, rax ; rsi = hKey
    
    ; Load General settings
    mov rcx, rsi
    lea rdx, REG_AUTO_SAVE
    mov r8d, 1 ; default
    call RegistryGetDWORD
    mov [rbx+SETTINGS_DATA.auto_save_enabled], al
    
    mov rcx, rsi
    lea rdx, REG_STARTUP_FULLSCREEN
    mov r8d, 0 ; default
    call RegistryGetDWORD
    mov [rbx+SETTINGS_DATA.startup_fullscreen], al
    
    mov rcx, rsi
    lea rdx, REG_FONT_SIZE
    mov r8d, 10 ; default
    call RegistryGetDWORD
    mov [rbx+SETTINGS_DATA.font_size], eax
    
    ; Load Model settings
    mov rcx, rsi
    lea rdx, REG_MODEL_PATH_VAL
    xor r8, r8  ; no default for string
    mov r9, [rbx+SETTINGS_DATA.model_path]
    call RegistryGetString
    
    mov rcx, rsi
    lea rdx, REG_DEFAULT_MODEL
    xor r8, r8
    mov r9, [rbx+SETTINGS_DATA.default_model]
    call RegistryGetString
    
    ; Load Chat settings
    mov rcx, rsi
    lea rdx, REG_CHAT_MODEL
    xor r8, r8
    mov r9, [rbx+SETTINGS_DATA.chat_model]
    call RegistryGetString
    
    mov rcx, rsi
    lea rdx, REG_TEMPERATURE
    mov r8d, 100  ; default 1.0
    call RegistryGetDWORD
    mov [rbx+SETTINGS_DATA.temperature], eax
    
    mov rcx, rsi
    lea rdx, REG_MAX_TOKENS
    mov r8d, 2048  ; default
    call RegistryGetDWORD
    mov [rbx+SETTINGS_DATA.max_tokens], eax
    
    mov rcx, rsi
    lea rdx, REG_SYSTEM_PROMPT
    xor r8, r8
    mov r9, [rbx+SETTINGS_DATA.system_prompt]
    call RegistryGetString
    
    ; Load Security settings
    mov rcx, rsi
    lea rdx, REG_API_KEY
    xor r8, r8
    mov r9, [rbx+SETTINGS_DATA.api_key]
    call RegistryGetString
    
    mov rcx, rsi
    lea rdx, REG_ENCRYPTION
    mov r8d, 0  ; default
    call RegistryGetDWORD
    mov [rbx+SETTINGS_DATA.encryption_enabled], al
    
    mov rcx, rsi
    lea rdx, REG_SECURE_STORAGE
    mov r8d, 0  ; default
    call RegistryGetDWORD
    mov [rbx+SETTINGS_DATA.secure_storage], al
    
    mov rcx, rsi
    call RegistryCloseKey
    
    mov rax, 1
    jmp load_done
    
load_failed:
    xor rax, rax
    
load_done:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
LoadSettingsFromRegistry ENDP

; ============================================================================
; SaveSettingsToRegistry: Save all settings to registry
; Parameters:
;   rcx = SETTINGS_DATA pointer
; Returns:
;   rax = 1 if success
; ============================================================================
SaveSettingsToRegistry PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rbx, rcx ; SETTINGS_DATA pointer
    
    ; Open/Create Base Key
    mov rcx, HKEY_CURRENT_USER
    lea rdx, REGISTRY_BASE_PATH
    mov r8, KEY_ALL_ACCESS
    call RegistryOpenKey
    test rax, rax
    jz save_failed
    
    mov rsi, rax ; rsi = hKey
    
    ; Save General settings
    mov rcx, rsi
    lea rdx, REG_AUTO_SAVE
    movzx r8d, [rbx+SETTINGS_DATA.auto_save_enabled]
    call RegistrySetDWORD
    
    mov rcx, rsi
    lea rdx, REG_STARTUP_FULLSCREEN
    movzx r8d, [rbx+SETTINGS_DATA.startup_fullscreen]
    call RegistrySetDWORD
    
    mov rcx, rsi
    lea rdx, REG_FONT_SIZE
    mov r8d, [rbx+SETTINGS_DATA.font_size]
    call RegistrySetDWORD
    
    ; Save Model settings
    mov rcx, rsi
    lea rdx, REG_MODEL_PATH_VAL
    mov r8, [rbx+SETTINGS_DATA.model_path]
    test r8, r8
    jz skip_save_model_path
    call RegistrySetString
skip_save_model_path:
    
    mov rcx, rsi
    lea rdx, REG_DEFAULT_MODEL
    mov r8, [rbx+SETTINGS_DATA.default_model]
    test r8, r8
    jz skip_save_default_model
    call RegistrySetString
skip_save_default_model:
    
    ; Save Chat settings
    mov rcx, rsi
    lea rdx, REG_CHAT_MODEL
    mov r8, [rbx+SETTINGS_DATA.chat_model]
    test r8, r8
    jz skip_save_chat_model
    call RegistrySetString
skip_save_chat_model:
    
    mov rcx, rsi
    lea rdx, REG_TEMPERATURE
    mov r8d, [rbx+SETTINGS_DATA.temperature]
    call RegistrySetDWORD
    
    mov rcx, rsi
    lea rdx, REG_MAX_TOKENS
    mov r8d, [rbx+SETTINGS_DATA.max_tokens]
    call RegistrySetDWORD
    
    mov rcx, rsi
    lea rdx, REG_SYSTEM_PROMPT
    mov r8, [rbx+SETTINGS_DATA.system_prompt]
    test r8, r8
    jz skip_save_system_prompt
    call RegistrySetString
skip_save_system_prompt:
    
    ; Save Security settings
    mov rcx, rsi
    lea rdx, REG_API_KEY
    mov r8, [rbx+SETTINGS_DATA.api_key]
    test r8, r8
    jz skip_save_api_key
    call RegistrySetString
skip_save_api_key:
    
    mov rcx, rsi
    lea rdx, REG_ENCRYPTION
    movzx r8d, [rbx+SETTINGS_DATA.encryption_enabled]
    call RegistrySetDWORD
    
    mov rcx, rsi
    lea rdx, REG_SECURE_STORAGE
    movzx r8d, [rbx+SETTINGS_DATA.secure_storage]
    call RegistrySetDWORD
    
    mov rcx, rsi
    call RegistryCloseKey
    
    mov rax, 1
    jmp save_done
    
save_failed:
    xor rax, rax
    
save_done:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
SaveSettingsToRegistry ENDP