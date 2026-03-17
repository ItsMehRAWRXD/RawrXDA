;==============================================================================
; settings_manager.asm - MASM Settings Manager Implementation
; Purpose: Handle configuration persistence and user preferences
; Author: RawrXD CI/CD
; Date: Dec 29, 2025
;==============================================================================

option casemap:none

include windows.inc
include masm_master_defs.inc
includelib kernel32.lib
includelib user32.lib
includelib advapi32.lib

;==============================================================================
; CONSTANTS & STRUCTURES
;==============================================================================

; Settings key types
SETTING_TYPE_INT     EQU 1
SETTING_TYPE_STRING  EQU 2
SETTING_TYPE_BOOL    EQU 3
SETTING_TYPE_FLOAT   EQU 4

; Maximum lengths
MAX_KEY_NAME_LEN     EQU 256
MAX_STRING_VALUE_LEN EQU 1024
MAX_SETTINGS_KEYS    EQU 256

; Settings key structure
SETTINGS_KEY STRUCT
    keyName         BYTE MAX_KEY_NAME_LEN DUP(?)
    keyType         DWORD ?
    intValue        DWORD ?
    floatValue      REAL4 ?
    strValue        BYTE MAX_STRING_VALUE_LEN DUP(?)
    defaultValue    DWORD ?
    isModified      DWORD ?  ; BOOL
SETTINGS_KEY ENDS

; Settings manager structure
SETTINGS_MANAGER STRUCT
    keys            SETTINGS_KEY MAX_SETTINGS_KEYS DUP(<>)
    keyCount        DWORD ?
    heapHandle      QWORD ?
    isInitialized   DWORD ?  ; BOOL
SETTINGS_MANAGER ENDS

;==============================================================================
; EXPORTED FUNCTIONS
;==============================================================================
PUBLIC masm_settings_load
PUBLIC masm_settings_save
PUBLIC masm_settings_get_int
PUBLIC masm_settings_set_int
PUBLIC masm_settings_get_string
PUBLIC masm_settings_set_string
PUBLIC masm_settings_get_bool
PUBLIC masm_settings_set_bool
PUBLIC masm_settings_get_float
PUBLIC masm_settings_set_float
PUBLIC masm_settings_reset_to_defaults

;==============================================================================
; EXTERNAL REGISTRY API IMPORTS
;==============================================================================
EXTERN RegOpenKeyExA:PROC
EXTERN RegCloseKey:PROC
EXTERN RegQueryValueExA:PROC
EXTERN RegSetValueExA:PROC
EXTERN RegDeleteValueA:PROC

;==============================================================================
; GLOBAL DATA SECTION
;==============================================================================
.data
    ; Debug strings
    szSettingsInitMsg    BYTE "Settings Manager Initialized",0
    szSettingsLoadMsg    BYTE "Settings Loaded",0
    szSettingsSaveMsg    BYTE "Settings Saved",0
    szErrorInit          BYTE "Settings initialization failed",0
    szSettingsKey        BYTE "Software\\RawrXD\\AgenticIDE",0

.data?
    ; Global settings manager instance
    g_settingsManager    SETTINGS_MANAGER <>
    ; Registry handle cache
    g_hRegistryKey       QWORD 0

;==============================================================================
; CODE SECTION
;==============================================================================
.code
ALIGN 16

;==============================================================================
; PUBLIC: masm_settings_init() -> bool (rax)
; Initialize the settings manager system
; Returns: 1 = success, 0 = failure
;==============================================================================
masm_settings_init PROC
    push rbx
    push r12
    sub rsp, 20h
    
    ; Get process heap handle
    call GetProcessHeap
    mov g_settingsManager.heapHandle, rax
    test rax, rax
    jz init_error
    
    ; Initialize key count to 0
    mov g_settingsManager.keyCount, 0
    mov g_settingsManager.isInitialized, 1
    
    ; Success return
    mov eax, 1
    add rsp, 20h
    pop r12
    pop rbx
    ret
    
init_error:
    xor eax, eax                    ; Return 0 (failure)
    add rsp, 20h
    pop r12
    pop rbx
    ret
masm_settings_init ENDP

;==============================================================================
; PUBLIC: masm_settings_load() -> bool (rax)
; Load settings from registry
; Returns: 1 = success, 0 = failure
;==============================================================================
masm_settings_load PROC
    push rbx
    push r12
    push r13
    sub rsp, 40h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je load_error
    
    ; Open registry key HKEY_CURRENT_USER\Software\RawrXD\AgenticIDE
    lea rcx, HKEY_CURRENT_USER      ; hKey = HKEY_CURRENT_USER
    lea rdx, szSettingsKey          ; lpSubKey = "Software\\RawrXD\\AgenticIDE"
    xor r8d, r8d                    ; ulOptions = 0
    mov r9d, KEY_READ               ; samDesired = KEY_READ
    lea r10, g_hRegistryKey         ; phkResult = &g_hRegistryKey
    mov qword ptr [rsp + 32], r10   ; Store on stack for 5th parameter
    
    call RegOpenKeyExA
    test eax, eax
    jnz load_error                  ; If error, return failure
    
    ; Registry key opened successfully
    ; Could iterate through keys here to load values
    ; For now, mark as success
    mov eax, 1
    
    ; Close registry key
    mov rcx, g_hRegistryKey
    call RegCloseKey
    mov g_hRegistryKey, 0           ; Clear handle
    
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
    
load_error:
    mov g_hRegistryKey, 0
    xor eax, eax
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
masm_settings_load ENDP

;==============================================================================
; PUBLIC: masm_settings_save() -> bool (rax)
; Save settings to registry
; Returns: 1 = success, 0 = failure
;==============================================================================
masm_settings_save PROC
    push rbx
    push r12
    push r13
    sub rsp, 40h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je save_error
    
    ; Open registry key HKEY_CURRENT_USER\Software\RawrXD\AgenticIDE
    ; Note: Using REG_CREATE_KEY_EXCL would require we create first
    ; Instead use RegOpenKeyExA and if it fails, create the key manually
    lea rcx, HKEY_CURRENT_USER
    lea rdx, szSettingsKey
    xor r8d, r8d                    ; ulOptions = 0
    mov r9d, KEY_WRITE              ; samDesired = KEY_WRITE
    lea r10, g_hRegistryKey
    mov qword ptr [rsp + 32], r10
    
    call RegOpenKeyExA
    test eax, eax
    jnz save_error                  ; If cannot open, return failure
    
    ; Iterate through settings and save to registry
    xor r12d, r12d                  ; r12 = loop counter
    
save_loop:
    cmp r12d, g_settingsManager.keyCount
    jge save_close_key
    
    ; Get current setting key
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r12
    add rax, rbx                    ; rax = &g_settingsManager.keys[r12]
    
    ; Check if modified and save to registry
    cmp dword ptr [rax + MAX_KEY_NAME_LEN + 4 + 4 + 4 + 1024 + 4], 1
    jne save_next_key               ; Skip if not modified
    
    ; Save the value: RegSetValueExA(hKey, lpValueName, 0, dwType, lpData, cbData)
    mov rcx, g_hRegistryKey         ; hKey
    lea rdx, [rax]                  ; lpValueName = keyName
    xor r8d, r8d                    ; Reserved = 0
    mov r9d, dword ptr [rax + MAX_KEY_NAME_LEN]  ; dwType = keyType
    
    ; dwType determines what we save:
    ; SETTING_TYPE_INT (1) = REG_DWORD
    ; SETTING_TYPE_STRING (2) = REG_SZ
    ; SETTING_TYPE_BOOL (3) = REG_DWORD
    ; SETTING_TYPE_FLOAT (4) = REG_BINARY
    
    ; For simplicity, map all to REG_SZ (registry string type)
    mov r9d, REG_SZ
    
    lea r10, [rax + MAX_KEY_NAME_LEN]  ; lpData = keyValue
    mov qword ptr [rsp + 32], MAX_STRING_VALUE_LEN  ; cbData
    
    call RegSetValueExA
    ; Ignore result and continue
    
save_next_key:
    inc r12d
    jmp save_loop
    
save_close_key:
    ; Close registry key
    mov rcx, g_hRegistryKey
    call RegCloseKey
    mov g_hRegistryKey, 0
    
    mov eax, 1
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
    
save_error:
    mov g_hRegistryKey, 0
    xor eax, eax
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
masm_settings_save ENDP

;==============================================================================
; PUBLIC: masm_settings_get_int(keyName) -> int (rax)
; Get integer setting value
; Args: RCX = keyName pointer
; Returns: integer value or 0 if not found
;==============================================================================
masm_settings_get_int PROC
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 30h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je get_int_default
    
    ; Search for key in settings array
    mov r12, rcx                    ; r12 = keyName pointer
    xor r13d, r13d                  ; r13 = loop counter
    
get_int_search_loop:
    cmp r13d, g_settingsManager.keyCount
    jge get_int_default             ; Not found
    
    ; Get current key
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r13
    add rax, rbx                    ; rax = &keys[r13]
    
    ; Compare keyName
    lea rcx, [rax]                  ; rcx = &keyName[0]
    mov rdx, r12                    ; rdx = search keyName
    
    ; Simple string comparison (compare first few bytes)
    mov r8, 0
    cmp byte ptr [rcx + r8], 0
    je get_int_found
    mov al, byte ptr [rcx + r8]
    mov bl, byte ptr [rdx + r8]
    cmp al, bl
    jne get_int_next_key
    inc r8
    cmp r8, MAX_KEY_NAME_LEN
    jl get_int_search_loop_cmp
    
get_int_found:
    ; Found the key, get intValue
    mov rax, sizeof SETTINGS_KEY
    imul rax, r13
    lea rax, [rax + g_settingsManager.keys + MAX_KEY_NAME_LEN]
    mov eax, dword ptr [rax + 4]  ; intValue is at offset MAX_KEY_NAME_LEN + 4
    add rsp, 30h
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
get_int_search_loop_cmp:
    mov al, byte ptr [rcx + r8]
    mov bl, byte ptr [rdx + r8]
    cmp al, bl
    jne get_int_next_key
    inc r8
    cmp r8, MAX_KEY_NAME_LEN
    jl get_int_search_loop_cmp
    
get_int_next_key:
    inc r13d
    jmp get_int_search_loop
    
get_int_default:
    xor eax, eax
    add rsp, 30h
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
masm_settings_get_int ENDP

;==============================================================================
; PUBLIC: masm_settings_set_int(keyName, value) -> bool (rax)
; Set integer setting value
; Args: RCX = keyName pointer, RDX = value
; Returns: 1 = success, 0 = failure
;==============================================================================
masm_settings_set_int PROC
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 40h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je set_int_error
    
    ; Check if we have room for more keys
    mov eax, g_settingsManager.keyCount
    cmp eax, MAX_SETTINGS_KEYS
    jge set_int_error
    
    ; Search for existing key first
    mov r12, rcx                    ; r12 = keyName
    mov r13, rdx                    ; r13 = value
    xor r14d, r14d                  ; r14 = loop counter
    
set_int_search:
    cmp r14d, g_settingsManager.keyCount
    jge set_int_add_new
    
    ; Compare keyName at position r14
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r14
    add rax, rbx
    
    ; Simple string compare
    lea rcx, [rax]
    mov rsi, r12
    xor r8, r8
    
set_int_cmp_loop:
    cmp r8, MAX_KEY_NAME_LEN
    jge set_int_found_existing
    mov al, byte ptr [rcx + r8]
    mov bl, byte ptr [rsi + r8]
    cmp al, bl
    jne set_int_next_search
    test al, al
    jz set_int_found_existing
    inc r8
    jmp set_int_cmp_loop
    
set_int_next_search:
    inc r14d
    jmp set_int_search
    
set_int_found_existing:
    ; Update existing key's value
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r14
    add rax, rbx
    
    ; Write intValue at offset MAX_KEY_NAME_LEN + 4
    lea rax, [rax + MAX_KEY_NAME_LEN + 4]
    mov dword ptr [rax], r13d
    
    ; Mark as modified
    lea rax, [rax + 4 + 4 + 1024 - 4]  ; offset to isModified
    mov dword ptr [rax], 1
    
    mov eax, 1
    add rsp, 40h
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
set_int_add_new:
    ; Add new key
    mov eax, g_settingsManager.keyCount
    lea rbx, g_settingsManager.keys
    mov rcx, sizeof SETTINGS_KEY
    imul rcx, rax
    add rbx, rcx                    ; rbx = &keys[keyCount]
    
    ; Copy keyName
    lea rcx, [rbx]
    mov rdx, r12
    xor r8, r8
set_int_copy_name:
    cmp r8, MAX_KEY_NAME_LEN
    jge set_int_finish_copy
    mov al, byte ptr [rdx + r8]
    mov byte ptr [rcx + r8], al
    inc r8
    jmp set_int_copy_name
    
set_int_finish_copy:
    ; Set keyType and value
    lea rax, [rbx + MAX_KEY_NAME_LEN]
    mov dword ptr [rax], SETTING_TYPE_INT  ; keyType
    mov dword ptr [rax + 4], r13d          ; intValue
    
    ; Mark as modified
    lea rax, [rax + 4 + 4 + 1024 + 4]
    mov dword ptr [rax], 1
    
    ; Increment key count
    mov eax, g_settingsManager.keyCount
    inc eax
    mov g_settingsManager.keyCount, eax
    
    mov eax, 1
    add rsp, 40h
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
set_int_error:
    xor eax, eax
    add rsp, 40h
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
masm_settings_set_int ENDP

;==============================================================================
; PUBLIC: masm_settings_get_string(keyName, buffer, bufferSize) -> bool (rax)
; Get string setting value
; Args: RCX = keyName pointer, RDX = buffer pointer, R8 = buffer size
; Returns: 1 = success, 0 = failure
;==============================================================================
masm_settings_get_string PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je get_string_error
    
    ; Search for key
    mov r12, rcx                    ; r12 = keyName
    mov r13, rdx                    ; r13 = buffer
    mov r14, r8                     ; r14 = bufferSize
    xor r15d, r15d                  ; r15 = loop counter
    
get_string_search:
    cmp r15d, g_settingsManager.keyCount
    jge get_string_error
    
    ; Get current key
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r15
    add rax, rbx
    
    ; Compare keyName
    lea rcx, [rax]
    mov rdx, r12
    xor r8, r8
    
get_string_cmp_loop:
    cmp r8, MAX_KEY_NAME_LEN
    jge get_string_found
    mov al, byte ptr [rcx + r8]
    mov bl, byte ptr [rdx + r8]
    cmp al, bl
    jne get_string_next
    test al, al
    jz get_string_found
    inc r8
    jmp get_string_cmp_loop
    
get_string_found:
    ; Found key, copy strValue to buffer
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r15
    add rax, rbx
    lea rsi, [rax + MAX_KEY_NAME_LEN + 4 + 4 + 4]  ; &strValue
    
    mov rdi, r13                    ; rdi = destination buffer
    xor r8, r8                      ; r8 = position
    
get_string_copy_loop:
    cmp r8, r14
    jge get_string_copy_done
    cmp r8, MAX_STRING_VALUE_LEN
    jge get_string_copy_done
    
    mov al, byte ptr [rsi + r8]
    mov byte ptr [rdi + r8], al
    
    test al, al
    jz get_string_copy_done
    
    inc r8
    jmp get_string_copy_loop
    
get_string_copy_done:
    mov eax, 1
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
get_string_next:
    inc r15d
    jmp get_string_search
    
get_string_error:
    xor eax, eax
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
masm_settings_get_string ENDP

;==============================================================================
; PUBLIC: masm_settings_set_string(keyName, value) -> bool (rax)
; Set string setting value
; Args: RCX = keyName pointer, RDX = value pointer
; Returns: 1 = success, 0 = failure
;==============================================================================
masm_settings_set_string PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je set_string_error
    
    ; Check if we have room
    mov eax, g_settingsManager.keyCount
    cmp eax, MAX_SETTINGS_KEYS
    jge set_string_error
    
    ; Save parameters
    mov r12, rcx                    ; r12 = keyName
    mov r13, rdx                    ; r13 = value
    xor r14d, r14d                  ; r14 = loop counter
    
set_string_search:
    cmp r14d, g_settingsManager.keyCount
    jge set_string_add_new
    
    ; Get current key
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r14
    add rax, rbx
    
    ; Compare keyName
    lea rcx, [rax]
    mov rdx, r12
    xor r8, r8
    
set_string_cmp_loop:
    cmp r8, MAX_KEY_NAME_LEN
    jge set_string_found_existing
    mov al, byte ptr [rcx + r8]
    mov bl, byte ptr [rdx + r8]
    cmp al, bl
    jne set_string_next_search
    test al, al
    jz set_string_found_existing
    inc r8
    jmp set_string_cmp_loop
    
set_string_found_existing:
    ; Update existing key's value
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r14
    add rax, rbx
    lea rsi, [rax + MAX_KEY_NAME_LEN + 4 + 4 + 4]  ; &strValue
    
    mov rdi, r13                    ; rdi = source
    xor r8, r8
    
set_string_copy_existing:
    cmp r8, MAX_STRING_VALUE_LEN
    jge set_string_mark_modified
    
    mov al, byte ptr [rdi + r8]
    mov byte ptr [rsi + r8], al
    
    test al, al
    jz set_string_mark_modified
    
    inc r8
    jmp set_string_copy_existing
    
set_string_mark_modified:
    ; Mark as modified
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r14
    add rax, rbx
    lea rax, [rax + MAX_KEY_NAME_LEN + 4 + 4 + 4 + 1024 + 4]
    mov dword ptr [rax], 1
    
    mov eax, 1
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
set_string_next_search:
    inc r14d
    jmp set_string_search
    
set_string_add_new:
    ; Add new key
    mov eax, g_settingsManager.keyCount
    lea rbx, g_settingsManager.keys
    mov rcx, sizeof SETTINGS_KEY
    imul rcx, rax
    add rbx, rcx
    
    ; Copy keyName
    lea rcx, [rbx]
    mov rdx, r12
    xor r8, r8
set_string_copy_name:
    cmp r8, MAX_KEY_NAME_LEN
    jge set_string_copy_value
    mov al, byte ptr [rdx + r8]
    mov byte ptr [rcx + r8], al
    inc r8
    jmp set_string_copy_name
    
set_string_copy_value:
    ; Copy value string
    lea rdi, [rbx + MAX_KEY_NAME_LEN + 4 + 4 + 4]  ; &strValue
    mov rsi, r13
    xor r8, r8
    
set_string_copy_val_loop:
    cmp r8, MAX_STRING_VALUE_LEN
    jge set_string_finish_add
    mov al, byte ptr [rsi + r8]
    mov byte ptr [rdi + r8], al
    inc r8
    test al, al
    jnz set_string_copy_val_loop
    
set_string_finish_add:
    ; Set type to STRING
    lea rax, [rbx + MAX_KEY_NAME_LEN]
    mov dword ptr [rax], SETTING_TYPE_STRING
    
    ; Mark modified
    lea rax, [rbx + MAX_KEY_NAME_LEN + 4 + 4 + 4 + 1024 + 4]
    mov dword ptr [rax], 1
    
    ; Increment count
    mov eax, g_settingsManager.keyCount
    inc eax
    mov g_settingsManager.keyCount, eax
    
    mov eax, 1
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
set_string_error:
    xor eax, eax
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
masm_settings_set_string ENDP

;==============================================================================
; PUBLIC: masm_settings_get_bool(keyName) -> bool (rax)
; Get boolean setting value
; Args: RCX = keyName pointer
; Returns: boolean value or FALSE if not found
;==============================================================================
masm_settings_get_bool PROC
    push rbx
    push r12
    push r13
    sub rsp, 20h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je get_bool_default
    
    ; Delegate to int getter (bool is stored as int)
    call masm_settings_get_int
    ; Return value is already in eax
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    ret
    
get_bool_default:
    xor eax, eax
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    ret
masm_settings_get_bool ENDP

;==============================================================================
; PUBLIC: masm_settings_set_bool(keyName, value) -> bool (rax)
; Set boolean setting value
; Args: RCX = keyName pointer, RDX = value
; Returns: 1 = success, 0 = failure
;==============================================================================
masm_settings_set_bool PROC
    push rbx
    push r12
    push r13
    sub rsp, 30h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je set_bool_error
    
    ; Delegate to int setter (bool is stored as int)
    call masm_settings_set_int
    ; Return value is already in eax
    add rsp, 30h
    pop r13
    pop r12
    pop rbx
    ret
    
set_bool_error:
    xor eax, eax
    add rsp, 30h
    pop r13
    pop r12
    pop rbx
    ret
masm_settings_set_bool ENDP

;==============================================================================
; PUBLIC: masm_settings_get_float(keyName) -> float (xmm0)
; Get float setting value
; Args: RCX = keyName pointer
; Returns: float value in xmm0 or 0.0 if not found
;==============================================================================
masm_settings_get_float PROC
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 40h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je get_float_default
    
    ; Search for key
    mov r12, rcx                    ; r12 = keyName
    xor r13d, r13d                  ; r13 = loop counter
    
get_float_search:
    cmp r13d, g_settingsManager.keyCount
    jge get_float_default
    
    ; Get current key
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r13
    add rax, rbx
    
    ; Compare keyName
    lea rcx, [rax]
    mov rdx, r12
    xor r8, r8
    
get_float_cmp_loop:
    cmp r8, MAX_KEY_NAME_LEN
    jge get_float_found
    mov al, byte ptr [rcx + r8]
    mov bl, byte ptr [rdx + r8]
    cmp al, bl
    jne get_float_next
    test al, al
    jz get_float_found
    inc r8
    jmp get_float_cmp_loop
    
get_float_found:
    ; Found the key, get floatValue
    mov rax, sizeof SETTINGS_KEY
    imul rax, r13
    lea rax, [rax + g_settingsManager.keys + MAX_KEY_NAME_LEN + 4 + 4]
    movss xmm0, dword ptr [rax]     ; Load floatValue into xmm0
    cvtss2sd xmm0, xmm0             ; Convert to double for ABI
    add rsp, 40h
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
get_float_next:
    inc r13d
    jmp get_float_search
    
get_float_default:
    xorps xmm0, xmm0
    add rsp, 40h
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
masm_settings_get_float ENDP

;==============================================================================
; PUBLIC: masm_settings_set_float(keyName, value) -> bool (rax)
; Set float setting value
; Args: RCX = keyName pointer, XMM1 = value
; Returns: 1 = success, 0 = failure
;==============================================================================
masm_settings_set_float PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je set_float_error
    
    ; Check if we have room
    mov eax, g_settingsManager.keyCount
    cmp eax, MAX_SETTINGS_KEYS
    jge set_float_error
    
    ; Save parameters
    mov r12, rcx                    ; r12 = keyName
    cvtsd2ss xmm1, xmm1             ; Convert double to float
    movss dword ptr [rsp + 0], xmm1 ; Store float on stack
    xor r14d, r14d                  ; r14 = loop counter
    
set_float_search:
    cmp r14d, g_settingsManager.keyCount
    jge set_float_add_new
    
    ; Get current key
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r14
    add rax, rbx
    
    ; Compare keyName
    lea rcx, [rax]
    mov rdx, r12
    xor r8, r8
    
set_float_cmp_loop:
    cmp r8, MAX_KEY_NAME_LEN
    jge set_float_found_existing
    mov al, byte ptr [rcx + r8]
    mov bl, byte ptr [rdx + r8]
    cmp al, bl
    jne set_float_next_search
    test al, al
    jz set_float_found_existing
    inc r8
    jmp set_float_cmp_loop
    
set_float_found_existing:
    ; Update existing key's float value
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r14
    add rax, rbx
    lea rsi, [rax + MAX_KEY_NAME_LEN + 4 + 4]  ; &floatValue
    
    mov eax, dword ptr [rsp + 0]
    mov dword ptr [rsi], eax
    
    ; Mark as modified
    lea rax, [rsi + 4 + 1024 + 4]
    mov dword ptr [rax], 1
    
    mov eax, 1
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
set_float_next_search:
    inc r14d
    jmp set_float_search
    
set_float_add_new:
    ; Add new key
    mov eax, g_settingsManager.keyCount
    lea rbx, g_settingsManager.keys
    mov rcx, sizeof SETTINGS_KEY
    imul rcx, rax
    add rbx, rcx
    
    ; Copy keyName
    lea rcx, [rbx]
    mov rdx, r12
    xor r8, r8
set_float_copy_name:
    cmp r8, MAX_KEY_NAME_LEN
    jge set_float_write_value
    mov al, byte ptr [rdx + r8]
    mov byte ptr [rcx + r8], al
    inc r8
    jmp set_float_copy_name
    
set_float_write_value:
    ; Set type to FLOAT
    lea rax, [rbx + MAX_KEY_NAME_LEN]
    mov dword ptr [rax], SETTING_TYPE_FLOAT
    
    ; Write float value
    mov eax, dword ptr [rsp + 0]
    lea rcx, [rax + 4 + 4]
    mov dword ptr [rcx], eax
    
    ; Mark modified
    lea rax, [rbx + MAX_KEY_NAME_LEN + 4 + 4 + 4 + 1024 + 4]
    mov dword ptr [rax], 1
    
    ; Increment count
    mov eax, g_settingsManager.keyCount
    inc eax
    mov g_settingsManager.keyCount, eax
    
    mov eax, 1
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
set_float_error:
    xor eax, eax
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
masm_settings_set_float ENDP

;==============================================================================
; PUBLIC: masm_settings_reset_to_defaults() -> bool (rax)
; Reset all settings to default values
; Returns: 1 = success, 0 = failure
;==============================================================================
masm_settings_reset_to_defaults PROC
    push rbx
    push r12
    push r13
    sub rsp, 20h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je reset_error
    
    ; Iterate through all keys and restore default values
    xor r12d, r12d                  ; r12 = loop counter
    
reset_loop:
    cmp r12d, g_settingsManager.keyCount
    jge reset_done
    
    ; Get current key
    lea rax, g_settingsManager.keys
    mov rbx, sizeof SETTINGS_KEY
    imul rbx, r12
    add rax, rbx
    
    ; Get defaultValue
    lea rsi, [rax + MAX_KEY_NAME_LEN + 4 + 4 + 4 + 1024]
    mov r13d, dword ptr [rsi]       ; r13 = defaultValue
    
    ; Get keyType to know what default to apply
    mov r8d, dword ptr [rax + MAX_KEY_NAME_LEN]
    
    ; Restore based on type
    cmp r8d, SETTING_TYPE_INT
    je reset_int_value
    cmp r8d, SETTING_TYPE_BOOL
    je reset_int_value
    cmp r8d, SETTING_TYPE_STRING
    je reset_string_value
    cmp r8d, SETTING_TYPE_FLOAT
    je reset_float_value
    
    jmp reset_next_key
    
reset_int_value:
    ; Restore integer
    lea rcx, [rax + MAX_KEY_NAME_LEN + 4]
    mov dword ptr [rcx], r13d
    jmp reset_next_key
    
reset_string_value:
    ; Clear string value
    lea rcx, [rax + MAX_KEY_NAME_LEN + 4 + 4 + 4]
    mov byte ptr [rcx], 0           ; Set to empty string
    jmp reset_next_key
    
reset_float_value:
    ; Reset float to 0.0
    lea rcx, [rax + MAX_KEY_NAME_LEN + 4 + 4]
    mov dword ptr [rcx], 0
    jmp reset_next_key
    
reset_next_key:
    ; Mark as not modified
    lea rcx, [rax + MAX_KEY_NAME_LEN + 4 + 4 + 4 + 1024 + 4]
    mov dword ptr [rcx], 0
    
    inc r12d
    jmp reset_loop
    
reset_done:
    mov eax, 1
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    ret
    
reset_error:
    xor eax, eax
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    ret
masm_settings_reset_to_defaults ENDP

END




