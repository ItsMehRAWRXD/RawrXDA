;==============================================================================
; settings_manager.inc - MASM Settings Manager Header
; Purpose: Declare exported functions and constants for settings management
; Author: RawrXD CI/CD
; Date: Dec 29, 2025
;==============================================================================

IFNDEF SETTINGS_MANAGER_INC
SETTINGS_MANAGER_INC EQU 1

;==============================================================================
; CONSTANTS
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

;==============================================================================
; FUNCTION PROTOTYPES
;==============================================================================

; Initialize the settings manager system
; Returns: 1 = success, 0 = failure
EXTERN masm_settings_init:PROC

; Load settings from registry
; Returns: 1 = success, 0 = failure
EXTERN masm_settings_load:PROC

; Save settings to registry
; Returns: 1 = success, 0 = failure
EXTERN masm_settings_save:PROC

; Get integer setting value
; Args: RCX = keyName pointer
; Returns: integer value or 0 if not found
EXTERN masm_settings_get_int:PROC

; Set integer setting value
; Args: RCX = keyName pointer, RDX = value
; Returns: 1 = success, 0 = failure
EXTERN masm_settings_set_int:PROC

; Get string setting value
; Args: RCX = keyName pointer, RDX = buffer pointer, R8 = buffer size
; Returns: 1 = success, 0 = failure
EXTERN masm_settings_get_string:PROC

; Set string setting value
; Args: RCX = keyName pointer, RDX = value pointer
; Returns: 1 = success, 0 = failure
EXTERN masm_settings_set_string:PROC

; Get boolean setting value
; Args: RCX = keyName pointer
; Returns: boolean value or FALSE if not found
EXTERN masm_settings_get_bool:PROC

; Set boolean setting value
; Args: RCX = keyName pointer, RDX = value
; Returns: 1 = success, 0 = failure
EXTERN masm_settings_set_bool:PROC

; Get float setting value
; Args: RCX = keyName pointer
; Returns: float value in xmm0 or 0.0 if not found
EXTERN masm_settings_get_float:PROC

; Set float setting value
; Args: RCX = keyName pointer, XMM1 = value
; Returns: 1 = success, 0 = failure
EXTERN masm_settings_set_float:PROC

; Reset all settings to default values
; Returns: 1 = success, 0 = failure
EXTERN masm_settings_reset_to_defaults:PROC

ENDIF