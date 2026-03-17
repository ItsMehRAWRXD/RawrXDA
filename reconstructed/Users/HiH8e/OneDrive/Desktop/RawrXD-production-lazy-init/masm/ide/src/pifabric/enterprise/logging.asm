; pifabric_enterprise_logging.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC EnterpriseLog_Init
PUBLIC EnterpriseLog_Write
PUBLIC EnterpriseLog_Clear

.data
MAX_LOGS EQU 128

LogEntry STRUCT
    level DWORD ?
    msg   DWORD ?
LogEntry ENDS

Logs STRUCT
    count DWORD ?
    entries LogEntry MAX_LOGS DUP(<0,0>)
Logs ENDS

g_Logs Logs <0>

.code

EnterpriseLog_Init PROC
    mov g_Logs.count,0
    mov eax,1
    ret
EnterpriseLog_Init ENDP

EnterpriseLog_Write PROC level:DWORD,msg:DWORD
    mov eax,g_Logs.count
    cmp eax,MAX_LOGS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF LogEntry
    mov edx,OFFSET g_Logs.entries
    add edx,ecx
    mov [edx].LogEntry.level,level
    mov [edx].LogEntry.msg,msg
    inc g_Logs.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
EnterpriseLog_Write ENDP

EnterpriseLog_Clear PROC
    mov g_Logs.count,0
    mov eax,1
    ret
EnterpriseLog_Clear ENDP

END