;  ============================================================================
;  IDE_INC.ASM  –  Enhanced production macros & common definitions
;  ============================================================================
;  Include this at the top of every IDE_*.asm file for consistent
;  logging, timing, thread-safety, and structured error handling.
;  ============================================================================

.386
.model flat, stdcall
option casemap:none

WINVER equ 0x0A00        ; Windows 10+

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\advapi32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\advapi32.lib

; ============================================================================
;  CONFIGURATION
; ============================================================================
LOG_ROOT_PATH     db "C:\ProgramData\RawrXD\logs",0
LOG_FILE_NAME     db "ide_runtime.jsonl",0
ENV_DEBUG_VAR     db "RAWRXD_DEBUG",0
LOG_BUFFER_SIZE   equ 512
MAX_PATH_SIZE     equ 260

; ============================================================================
;  HRESULT-STYLE ERROR CODES
; ============================================================================
S_OK              equ 0
E_FAIL            equ 80004005h
E_INVALIDARG      equ 80070057h
E_OUTOFMEMORY     equ 8007000Eh
E_NOTIMPL         equ 80004001h
E_TIMEOUT         equ 80070102h

; ============================================================================
;  PERFORMANCE TIMING MACROS
; ============================================================================

; Initialize performance counter (saves start time)
PERF_INIT MACRO pStart
    pushad
    push pStart
    call _perf_now
    popad
ENDM

; Calculate delta in microseconds
PERF_DELTA MACRO pStart, pEnd, pDur
    LOCAL do_calc
    pushad
    push pEnd
    call _perf_now
    mov ebx, pEnd
    mov eax, [ebx]
    mov ecx, pStart
    sub eax, [ecx]
    mov edx, pDur
    mov [edx], eax
    popad
ENDM

; ============================================================================
;  LOGGING MACRO
; ============================================================================

; Log function call with timing and result
; Usage: LOG "FunctionName", eax_result, duration_us
LOG MACRO pszFunc, dwResult, dwDurUs
    pushad
    push dwResult
    push dwDurUs
    push pszFunc
    call _log_json
    add esp, 12
    popad
ENDM

; ============================================================================
;  EXTERNAL HELPER FUNCTIONS (defined in IDE_CRIT.asm)
; ============================================================================
extern _perf_now:PROC
extern _log_json:PROC
extern _enter_crit:PROC
extern _leave_crit:PROC
extern _get_hresult:PROC
extern _init_log_dir:PROC

; ============================================================================
;  COMMON STRING HELPERS
; ============================================================================

; Convenience macro for inline string constants
CSTR MACRO text:VARARG
    LOCAL string
    .data
    string db text,0
    .code
    EXITM <offset string>
ENDM
