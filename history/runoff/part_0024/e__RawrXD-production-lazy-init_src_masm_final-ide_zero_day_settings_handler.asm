;==============================================================================
; zero_day_settings_handler.asm - Zero-Day Settings UI Handler
; Purpose: Handle Zero-Day menu interactions and complexity threshold toggling
; Author: RawrXD CI/CD
; Date: Dec 29, 2025
;==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==============================================================================
; EXTERN DECLARATIONS
;==============================================================================
EXTERN masm_settings_get_bool:PROC
EXTERN masm_settings_set_bool:PROC
EXTERN masm_settings_get_int:PROC
EXTERN masm_settings_set_int:PROC
EXTERN MessageBoxA:PROC
EXTERN wsprintfA:PROC

;==============================================================================
; CONSTANTS
;==============================================================================
; Menu command IDs (from menu_system.asm)
IDM_TOOLS_ZERO_DAY      = 4005
IDM_TOOLS_ZERO_DAY_FORCE= 4006

; Settings key names
ZERO_DAY_FORCE_KEY     EQU <"zero_day_force_complex_goals">
COMPLEXITY_THRESHOLD_KEY EQU <"complexity_threshold">

; Default values
DEFAULT_FORCE_MODE     EQU 0  ; Disabled by default
DEFAULT_COMPLEXITY     EQU 50 ; 0-100 scale

;==============================================================================
; GLOBAL DATA
;==============================================================================
.data
    ; Zero-Day global flags
    gZeroDayForceMode    DWORD 0      ; Force complex goals through zero-day engine
    gComplexityThreshold DWORD 50     ; Complexity detection threshold (0-100)
    
    ; Menu strings for status messages
    szZeroDayTitle       BYTE "Zero-Day Engine Settings", 0
    szForceEnabled       BYTE "Force Complex Goals (Zero-Day) ENABLED", 0x0D, 0x0A, "The agentic pipeline will always use zero-day engine regardless of complexity detection.", 0
    szForceDisabled      BYTE "Force Complex Goals (Zero-Day) DISABLED", 0x0D, 0x0A, "Complexity will be detected automatically (simple->zero-day, complex->enterprise).", 0
    szThresholdMsg       BYTE "Complexity Threshold: %d%%", 0
    
    ; Debug output
    szZeroDayDebug       BYTE "[Zero-Day] Force mode toggled to %d", 0

.data?
    ; Temporary buffers
    gStatusBuffer        BYTE 512 DUP(?)
    gThresholdBuffer     BYTE 256 DUP(?)

;==============================================================================
; PUBLIC FUNCTIONS
;==============================================================================

; zero_day_settings_init()
; Initialize Zero-Day settings from persistent storage
PUBLIC zero_day_settings_init
zero_day_settings_init PROC
    push rbx
    push r12
    sub rsp, 32
    
    ; Load force mode setting from registry/config
    lea rcx, [ZERO_DAY_FORCE_KEY]
    mov edx, DEFAULT_FORCE_MODE
    call masm_settings_get_bool
    mov [gZeroDayForceMode], eax
    
    ; Load complexity threshold from registry/config
    lea rcx, [COMPLEXITY_THRESHOLD_KEY]
    mov edx, DEFAULT_COMPLEXITY
    call masm_settings_get_int
    mov [gComplexityThreshold], eax
    
    ; Clamp threshold to valid range (0-100)
    mov eax, [gComplexityThreshold]
    cmp eax, 100
    jle threshold_ok
    mov DWORD PTR [gComplexityThreshold], 100
threshold_ok:
    cmp DWORD PTR [gComplexityThreshold], 0
    jge init_done
    mov DWORD PTR [gComplexityThreshold], 0
    
init_done:
    add rsp, 32
    pop r12
    pop rbx
    ret
zero_day_settings_init ENDP

; zero_day_settings_toggle_force_mode()
; Toggle the force complex goals mode
PUBLIC zero_day_settings_toggle_force_mode
zero_day_settings_toggle_force_mode PROC
    push rbx
    sub rsp, 32
    
    ; Flip the current mode
    mov eax, [gZeroDayForceMode]
    xor eax, 1
    mov [gZeroDayForceMode], eax
    
    ; Persist to settings
    lea rcx, [ZERO_DAY_FORCE_KEY]
    mov edx, eax
    call masm_settings_set_bool
    
    ; Show status message
    lea rcx, [gStatusBuffer]
    mov edx, 512
    lea r8, [szForceEnabled]
    
    ; Check current mode and select message
    mov eax, [gZeroDayForceMode]
    test eax, eax
    jnz show_enabled
    
    lea r8, [szForceDisabled]
    
show_enabled:
    mov r9, r8  ; r9 = message string
    lea r8, [szZeroDayTitle]
    mov edx, 0  ; hwndOwner = NULL
    mov rcx, r9 ; lpText = message
    call MessageBoxA
    
    ; Return current state in eax
    mov eax, [gZeroDayForceMode]
    add rsp, 32
    pop rbx
    ret
zero_day_settings_toggle_force_mode ENDP

; zero_day_settings_set_threshold(ecx = new threshold 0-100)
; Set the complexity threshold
PUBLIC zero_day_settings_set_threshold
zero_day_settings_set_threshold PROC
    push rbx
    sub rsp, 32
    
    mov eax, ecx  ; eax = new threshold
    
    ; Clamp to valid range
    cmp eax, 100
    jle clamp_min
    mov eax, 100
clamp_min:
    cmp eax, 0
    jge threshold_valid
    xor eax, eax
    
threshold_valid:
    mov [gComplexityThreshold], eax
    
    ; Persist to settings
    lea rcx, [COMPLEXITY_THRESHOLD_KEY]
    mov edx, eax
    call masm_settings_set_int
    
    ; Return current value
    mov eax, [gComplexityThreshold]
    add rsp, 32
    pop rbx
    ret
zero_day_settings_set_threshold ENDP

; zero_day_settings_get_force_mode() -> eax (bool)
; Returns current force mode setting
PUBLIC zero_day_settings_get_force_mode
zero_day_settings_get_force_mode PROC
    mov eax, [gZeroDayForceMode]
    ret
zero_day_settings_get_force_mode ENDP

; zero_day_settings_get_threshold() -> eax (0-100)
; Returns current complexity threshold
PUBLIC zero_day_settings_get_threshold
zero_day_settings_get_threshold PROC
    mov eax, [gComplexityThreshold]
    ret
zero_day_settings_get_threshold ENDP

END
