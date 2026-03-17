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
;  PERFORMANCE TIMING MACROS (Ultra-simplified - no segment issues)
; ============================================================================

; These are now simple comment placeholders - timing done manually in each function
PERF_INIT MACRO pStart
    ; Timing: call GetTickCount and store in pStart
ENDM

PERF_DELTA MACRO pStart, pDur  
    ; Timing: get current time and subtract pStart, store in pDur
ENDM

; ============================================================================
;  LOGGING MACRO (Simplified)
; ============================================================================

; Log function call with timing and result
; Usage: LOG offset szFuncName, result, duration
LOG MACRO pszFunc, dwResult, dwDurUs
    invoke _log_json, pszFunc, dwDurUs, dwResult
ENDM

; ============================================================================
;  EXTERNAL HELPER FUNCTIONS (defined in IDE_CRIT.asm & IDE_JSONLOG.asm)
; ============================================================================
_perf_now PROTO
_log_json PROTO :DWORD, :DWORD, :SDWORD
_enter_crit PROTO
_leave_crit PROTO
_get_hresult PROTO
_init_log_dir PROTO

; ============================================================================
;  COMMON STRING HELPERS
; ============================================================================

; Convenience macro for inline string constants (defines in current segment)
CSTR MACRO text:VARARG
    LOCAL szStr, lblSkip
    jmp lblSkip
    szStr db text,0
lblSkip:
    EXITM <offset szStr>
ENDM
