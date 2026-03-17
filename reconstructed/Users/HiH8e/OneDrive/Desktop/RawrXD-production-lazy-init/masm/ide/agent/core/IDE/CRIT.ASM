;  ============================================================================
;  IDE_CRIT.ASM  –  Critical sections, QPC timing, and core utilities
;  ============================================================================
;  Provides thread-safe primitives used by all IDE modules.
;  Links once, used by all 20 DLLs.
;  ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; ============================================================================
;  GLOBAL STATE
; ============================================================================
.data
g_cs         CRITICAL_SECTION <>
g_freq       QWORD 0
g_init_done  DWORD 0

PUBLIC g_cs
PUBLIC g_freq

; ============================================================================
;  DllMain – Initialize critical section and QPC frequency
; ============================================================================
.code
DllMain PROC hInstDLL:DWORD, fdwReason:DWORD, lpvReserved:DWORD
    .if fdwReason == DLL_PROCESS_ATTACH
        invoke InitializeCriticalSection, addr g_cs
        invoke QueryPerformanceFrequency, addr g_freq
        mov g_init_done, 1
    .elseif fdwReason == DLL_PROCESS_DETACH
        .if g_init_done == 1
            invoke DeleteCriticalSection, addr g_cs
        .endif
    .endif
    mov eax, TRUE
    ret
DllMain ENDP

; ============================================================================
;  _perf_now – Get current time (simplified to use GetTickCount)
;  Output: eax = current timestamp in milliseconds
; ============================================================================
PUBLIC _perf_now
_perf_now PROC
    invoke GetTickCount
    ret
_perf_now ENDP

; ============================================================================
;  _enter_crit – Enter critical section
; ============================================================================
PUBLIC _enter_crit
_enter_crit PROC
    invoke EnterCriticalSection, addr g_cs
    ret
_enter_crit ENDP

; ============================================================================
;  _leave_crit – Leave critical section
; ============================================================================
PUBLIC _leave_crit
_leave_crit PROC
    invoke LeaveCriticalSection, addr g_cs
    ret
_leave_crit ENDP

; ============================================================================
;  _get_hresult – Convert Win32 error to HRESULT
;  Input:  none (uses GetLastError)
;  Output: eax = HRESULT
; ============================================================================
PUBLIC _get_hresult
_get_hresult PROC
    invoke GetLastError
    .if eax == 0
        xor eax, eax    ; S_OK
    .else
        or eax, 80070000h    ; HRESULT_FROM_WIN32
    .endif
    ret
_get_hresult ENDP

; ============================================================================
;  _init_log_dir – Ensure log directory exists
; ============================================================================
.data
log_dir db "C:\ProgramData\RawrXD\logs", 0

.code
PUBLIC _init_log_dir
_init_log_dir PROC
    invoke CreateDirectoryA, addr log_dir, NULL
    ; Ignore errors - directory might already exist
    ret
_init_log_dir ENDP

END DllMain
