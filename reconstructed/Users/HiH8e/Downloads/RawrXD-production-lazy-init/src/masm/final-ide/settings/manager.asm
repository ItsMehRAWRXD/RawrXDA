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
PUBLIC masm_settings_init
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
    
    ; TODO: Implement registry loading
    ; For now, just return success
    mov eax, 1
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
    
load_error:
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
    
    ; TODO: Implement registry saving
    ; For now, just return success
    mov eax, 1
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
    
save_error:
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
    sub rsp, 20h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je get_int_default
    
    ; TODO: Implement key lookup
    ; For now, return 0
    xor eax, eax
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    ret
    
get_int_default:
    xor eax, eax
    add rsp, 20h
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
    sub rsp, 30h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je set_int_error
    
    ; TODO: Implement key setting
    ; For now, just return success
    mov eax, 1
    add rsp, 30h
    pop r13
    pop r12
    pop rbx
    ret
    
set_int_error:
    xor eax, eax
    add rsp, 30h
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
    sub rsp, 40h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je get_string_error
    
    ; TODO: Implement string key lookup
    ; For now, just return failure
    xor eax, eax
    add rsp, 40h
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
get_string_error:
    xor eax, eax
    add rsp, 40h
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
    sub rsp, 40h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je set_string_error
    
    ; TODO: Implement string key setting
    ; For now, just return success
    mov eax, 1
    add rsp, 40h
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
set_string_error:
    xor eax, eax
    add rsp, 40h
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
    sub rsp, 20h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je get_float_default
    
    ; TODO: Implement float key lookup
    ; For now, return 0.0
    xorps xmm0, xmm0
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    ret
    
get_float_default:
    xorps xmm0, xmm0
    add rsp, 20h
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
    sub rsp, 30h
    
    ; Check if initialized
    cmp g_settingsManager.isInitialized, 0
    je set_float_error
    
    ; TODO: Implement float key setting
    ; For now, just return success
    mov eax, 1
    add rsp, 30h
    pop r13
    pop r12
    pop rbx
    ret
    
set_float_error:
    xor eax, eax
    add rsp, 30h
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
    
    ; TODO: Implement reset to defaults
    ; For now, just return success
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