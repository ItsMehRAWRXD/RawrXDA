; ============================================================================
; Performance Dashboard Notification Stub
; Phase 7 - Provides PerformanceDashboard_NotifyConfigChange export
; Used by Quantization Controls (Batch 2) to notify dashboard of config changes
; ============================================================================

option casemap:none

include windows.inc
include kernel32.inc

; ============================================================================
; STUB IMPLEMENTATIONS
; ============================================================================

.CODE

PUBLIC PerformanceDashboard_NotifyConfigChange
PerformanceDashboard_NotifyConfigChange PROC FRAME USES rbx
    ; RCX = config change type (PERF_CHANGE_QUANTIZATION = 1, etc.)
    ; RDX = value (e.g., new quantization type)
    ; This stub accepts but ignores notifications
    ; In production, this would update dashboard metrics in real-time
    
    ; For now, just return success
    mov rax, TRUE
    ret
PerformanceDashboard_NotifyConfigChange ENDP

END
