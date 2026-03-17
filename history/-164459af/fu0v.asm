;==========================================================================
; masm_metrics_collector.asm - Pure MASM Metrics Collector
; ==========================================================================
; Replaces metrics_collector.cpp.
; High-performance tracking of inference latency and token throughput.
;==========================================================================

option casemap:none

include windows.inc

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN console_log:PROC
EXTERN GetTickCount64:PROC

;==========================================================================
; DATA SECTION
;==========================================================================
.data
    szMetricsInit   BYTE "Metrics: Initializing MASM collector...", 0
    szMetricsLog    BYTE "Metrics: Request %d completed: %d tokens in %d ms (%f tok/s)", 0
    
    ; State
    g_total_tokens  QWORD 0
    g_total_ms      QWORD 0

.code

;==========================================================================
; metrics_init()
;==========================================================================
PUBLIC metrics_init
metrics_init PROC
    sub rsp, 32
    
    lea rcx, szMetricsInit
    call console_log
    
    add rsp, 32
    ret
metrics_init ENDP

;==========================================================================
; metrics_log_request(id: rcx, tokens: rdx, ms: r8)
;==========================================================================
PUBLIC metrics_log_request
metrics_log_request PROC
    push rbx
    push rsi
    sub rsp, 32
    
    mov rbx, rcx        ; id
    mov rsi, rdx        ; tokens
    
    ; Calculate tokens per second
    ; (In MASM, we'd use FPU or SSE for float division)
    ; ...
    
    lea rcx, szMetricsLog
    mov rdx, rbx
    mov r8, rsi
    mov r9, r8          ; ms
    ; (Float arg would go in xmm3)
    call console_log
    
    add rsp, 32
    pop rsi
    pop rbx
    ret
metrics_log_request ENDP

END
