; pifabric_enterprise_metrics.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC EnterpriseMetrics_Init
PUBLIC EnterpriseMetrics_Record
PUBLIC EnterpriseMetrics_Report
PUBLIC EnterpriseMetrics_Clear

.data
MAX_METRICS EQU 64

Metric STRUCT
    id DWORD ?
    value DWORD ?
Metric ENDS

Metrics STRUCT
    count DWORD ?
    items Metric MAX_METRICS DUP(<0,0>)
Metrics ENDS

g_Metrics Metrics <0>

.code

EnterpriseMetrics_Init PROC
    mov g_Metrics.count,0
    mov eax,1
    ret
EnterpriseMetrics_Init ENDP

EnterpriseMetrics_Record PROC id:DWORD,value:DWORD
    mov eax,g_Metrics.count
    cmp eax,MAX_METRICS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Metric
    mov edx,OFFSET g_Metrics.items
    add edx,ecx
    mov [edx].Metric.id,id
    mov [edx].Metric.value,value
    inc g_Metrics.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
EnterpriseMetrics_Record ENDP

EnterpriseMetrics_Report PROC
    mov eax,g_Metrics.count
    ret
EnterpriseMetrics_Report ENDP

EnterpriseMetrics_Clear PROC
    mov g_Metrics.count,0
    mov eax,1
    ret
EnterpriseMetrics_Clear ENDP

END