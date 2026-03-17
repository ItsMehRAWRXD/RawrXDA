;==============================================================================
; runtime_support.asm - Minimal runtime support for pure MASM builds
;==============================================================================
; Provides small but real implementations required by Phase 4 resource guards
; and structured logging used by the production error handler.
;
; Exports:
;   - asm_malloc(size, alignment)
;   - asm_free(ptr)
;   - asm_mutex_unlock(handle)
;   - Logger_LogStructured(message, level)
;==============================================================================

option casemap:none

LMEM_ZEROINIT EQU 40h

EXTERN LocalAlloc:PROC
EXTERN LocalFree:PROC
EXTERN ReleaseMutex:PROC
EXTERN OutputDebugStringA:PROC

PUBLIC asm_malloc
PUBLIC asm_free
PUBLIC asm_mutex_unlock
PUBLIC Logger_LogStructured
PUBLIC Metrics_RecordHistogramMission
PUBLIC Metrics_IncrementMissionCounter
PUBLIC Metrics_RecordLatency

.data
g_metrics_mission_histogram_count  qword 0
g_metrics_mission_counter          qword 0
g_metrics_latency_count            qword 0

.code

;------------------------------------------------------------------------------
; asm_malloc
; rcx = size (bytes)
; rdx = alignment (ignored; LocalAlloc provides sufficient alignment for our use)
; returns rax = pointer (NULL on failure)
;------------------------------------------------------------------------------
asm_malloc PROC
    sub rsp, 28h

    mov r8, rdx           ; preserve alignment (unused)
    mov rdx, rcx          ; uBytes
    mov ecx, LMEM_ZEROINIT
    call LocalAlloc

    add rsp, 28h
    ret
asm_malloc ENDP

;------------------------------------------------------------------------------
; asm_free
; rcx = pointer
; returns rax = 0 on success (LocalFree returns NULL on success)
;------------------------------------------------------------------------------
asm_free PROC
    sub rsp, 28h

    call LocalFree

    add rsp, 28h
    ret
asm_free ENDP

;------------------------------------------------------------------------------
; asm_mutex_unlock
; rcx = mutex handle
; returns eax = nonzero on success
;------------------------------------------------------------------------------
asm_mutex_unlock PROC
    sub rsp, 28h

    call ReleaseMutex

    add rsp, 28h
    ret
asm_mutex_unlock ENDP

;------------------------------------------------------------------------------
; Logger_LogStructured
; rcx = message (LPCSTR)
; rdx = log level (DWORD) (accepted but not required for OutputDebugStringA)
; returns eax = 1
;------------------------------------------------------------------------------
Logger_LogStructured PROC
    sub rsp, 28h

    ; message already in rcx
    call OutputDebugStringA

    mov eax, 1
    add rsp, 28h
    ret
Logger_LogStructured ENDP

;------------------------------------------------------------------------------
; Metrics_RecordHistogramMission
; Minimal real implementation: increments a counter.
; Signature intentionally permissive (unused parameters accepted).
; Returns eax = 1
;------------------------------------------------------------------------------
Metrics_RecordHistogramMission PROC
    lock inc qword ptr [g_metrics_mission_histogram_count]
    mov eax, 1
    ret
Metrics_RecordHistogramMission ENDP

;------------------------------------------------------------------------------
; Metrics_IncrementMissionCounter
; Minimal real implementation: increments a counter.
; Returns eax = 1
;------------------------------------------------------------------------------
Metrics_IncrementMissionCounter PROC
    lock inc qword ptr [g_metrics_mission_counter]
    mov eax, 1
    ret
Metrics_IncrementMissionCounter ENDP

;------------------------------------------------------------------------------
; Metrics_RecordLatency
; Minimal real implementation: increments a counter.
; Returns eax = 1
;------------------------------------------------------------------------------
Metrics_RecordLatency PROC
    lock inc qword ptr [g_metrics_latency_count]
    mov eax, 1
    ret
Metrics_RecordLatency ENDP

end
