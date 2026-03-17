;  ============================================================================
;  IDE_20_TELEMETRY.asm  –  Metrics, tracing, and ETW spans
;  ============================================================================
;  Implements:
;    - AgentTelemetry_Step (ETW span per autonomy tick)
;    - Telemetry_SendEvent / SendMetric
;  ============================================================================

include ..\agent_core\IDE_INC.ASM

; ============================================================================
;  EXPORTS
; ============================================================================
PUBLIC AgentTelemetry_Step
PUBLIC Telemetry_SendEvent
PUBLIC Telemetry_SendMetric

; ============================================================================
;  CONSTANTS
; ============================================================================
MAX_SPANS         equ 1024
MAX_METRICS       equ 256

; ============================================================================
;  SPAN STRUCTURE (for distributed tracing)
; ============================================================================
SPAN_RECORD STRUCT
    span_id     QWORD ?
    parent_id   QWORD ?
    start_time  QWORD ?
    end_time    QWORD ?
    operation   BYTE 128 dup(?)
    status      DWORD ?
SPAN_RECORD ENDS

; ============================================================================
;  METRIC STRUCTURE
; ============================================================================
METRIC_RECORD STRUCT
    name        BYTE 64 dup(?)
    value       REAL8 ?
    timestamp   QWORD ?
    metric_type DWORD ?            ; 0=counter, 1=gauge, 2=histogram
METRIC_RECORD ENDS

; ============================================================================
;  DATA
; ============================================================================
.data
g_spans         SPAN_RECORD MAX_SPANS dup(<>)
g_span_count    DWORD 0
g_next_span_id  QWORD 1

g_metrics       METRIC_RECORD MAX_METRICS dup(<>)
g_metric_count  DWORD 0

szTelStep       db "AgentTelemetry_Step", 0
szSendEvent     db "Telemetry_SendEvent", 0
szSendMetric    db "Telemetry_SendMetric", 0

; ============================================================================
;  CODE
; ============================================================================
.code

; ============================================================================
;  AgentTelemetry_Step – Record one autonomy tick
;  Input:  tick = tick number
;          duration_us = tick duration
;  Output: eax = span ID
; ============================================================================
AgentTelemetry_Step PROC uses ebx esi, tick:DWORD, duration_us:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    LOCAL pSpan:PTR SPAN_RECORD
    LOCAL spanId:QWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    ; Get next span ID
    mov eax, dword ptr g_next_span_id
    mov edx, dword ptr g_next_span_id+4
    mov dword ptr spanId, eax
    mov dword ptr spanId+4, edx
    
    add eax, 1
    adc edx, 0
    mov dword ptr g_next_span_id, eax
    mov dword ptr g_next_span_id+4, edx
    
    ; Find free span slot
    xor ecx, ecx
    mov esi, offset g_spans
    
find_span:
    .if ecx >= MAX_SPANS
        ; Table full, overwrite oldest
        mov esi, offset g_spans
        jmp record_span
    .endif
    
    .if (SPAN_RECORD PTR [esi]).span_id == 0
        mov pSpan, esi
        jmp record_span
    .endif
    
    add esi, sizeof SPAN_RECORD
    inc ecx
    jmp find_span
    
record_span:
    ; Fill span record
    mov eax, dword ptr spanId
    mov dword ptr (SPAN_RECORD PTR [esi]).span_id, eax
    mov eax, dword ptr spanId+4
    mov dword ptr (SPAN_RECORD PTR [esi]).span_id+4, eax
    
    mov (SPAN_RECORD PTR [esi]).parent_id, 0        ; Root span
    
    ; Set start and end time
    lea eax, (SPAN_RECORD PTR [esi]).start_time
    push eax
    call _perf_now
    add esp, 4
    
    ; End time = start + duration
    mov eax, dword ptr (SPAN_RECORD PTR [esi]).start_time
    add eax, duration_us
    mov dword ptr (SPAN_RECORD PTR [esi]).end_time, eax
    
    ; Set operation name
    invoke wsprintf, addr (SPAN_RECORD PTR [esi]).operation, CSTR("autonomy_tick_%d"), tick
    
    mov (SPAN_RECORD PTR [esi]).status, 0          ; 0=OK
    
    inc g_span_count
    mov hr, 0
    
    call _leave_crit
    PERF_DELTA offset start, offset dur, offset dur
    
    LOG offset szTelStep, hr, dword ptr [dur]
    
    mov eax, dword ptr spanId
    mov edx, dword ptr spanId+4
    ret
AgentTelemetry_Step ENDP

; ============================================================================
;  Telemetry_SendEvent – Send a telemetry event
;  Input:  pEventName = event name
;          pData = event data (JSON)
;  Output: eax = HRESULT
; ============================================================================
Telemetry_SendEvent PROC uses ebx, pEventName:DWORD, pData:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    .if pEventName == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    ; TODO: Send to telemetry backend (ETW, OpenTelemetry, etc.)
    ; For now, just log
    
    mov hr, 0
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur, offset dur
    
    LOG offset szSendEvent, hr, dword ptr [dur]
    
    mov eax, hr
    ret
Telemetry_SendEvent ENDP

; ============================================================================
;  Telemetry_SendMetric – Record a metric value
;  Input:  pMetricName = metric name
;          value = metric value (float)
;          metricType = 0=counter, 1=gauge, 2=histogram
;  Output: eax = HRESULT
; ============================================================================
Telemetry_SendMetric PROC uses ebx esi, pMetricName:DWORD, value:REAL8, metricType:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    LOCAL pMetric:PTR METRIC_RECORD
    
    PERF_INIT offset start
    call _enter_crit
    
    .if pMetricName == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    ; Find free metric slot
    xor ecx, ecx
    mov esi, offset g_metrics
    
find_metric:
    .if ecx >= MAX_METRICS
        ; Table full, overwrite first
        mov esi, offset g_metrics
        jmp record_metric
    .endif
    
    .if (METRIC_RECORD PTR [esi]).timestamp == 0
        mov pMetric, esi
        jmp record_metric
    .endif
    
    add esi, sizeof METRIC_RECORD
    inc ecx
    jmp find_metric
    
record_metric:
    ; Fill metric record
    invoke lstrcpynA, addr (METRIC_RECORD PTR [esi]).name, pMetricName, 64
    
    ; Copy value
    fld value
    fstp (METRIC_RECORD PTR [esi]).value
    
    ; Set timestamp
    lea eax, (METRIC_RECORD PTR [esi]).timestamp
    push eax
    call _perf_now
    add esp, 4
    
    mov eax, metricType
    mov (METRIC_RECORD PTR [esi]).metric_type, eax
    
    inc g_metric_count
    mov hr, 0
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur, offset dur
    
    LOG offset szSendMetric, hr, dword ptr [dur]
    
    mov eax, hr
    ret
Telemetry_SendMetric ENDP

END
