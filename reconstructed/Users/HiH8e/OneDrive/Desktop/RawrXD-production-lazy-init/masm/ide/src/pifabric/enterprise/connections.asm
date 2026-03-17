; pifabric_enterprise_connections.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC EnterpriseConnections_Init
PUBLIC EnterpriseConnections_Add
PUBLIC EnterpriseConnections_GetCount
PUBLIC EnterpriseConnections_Clear

.data
MAX_CONN EQU 32

Connection STRUCT
    id DWORD ?
    handle DWORD ?
Connection ENDS

Connections STRUCT
    count DWORD ?
    items Connection MAX_CONN DUP(<0,0>)
Connections ENDS

g_Connections Connections <0>

.code

EnterpriseConnections_Init PROC
    mov g_Connections.count,0
    mov eax,1
    ret
EnterpriseConnections_Init ENDP

EnterpriseConnections_Add PROC id:DWORD,handle:DWORD
    mov eax,g_Connections.count
    cmp eax,MAX_CONN
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Connection
    mov edx,OFFSET g_Connections.items
    add edx,ecx
    mov [edx].Connection.id,id
    mov [edx].Connection.handle,handle
    inc g_Connections.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
EnterpriseConnections_Add ENDP

EnterpriseConnections_GetCount PROC
    mov eax,g_Connections.count
    ret
EnterpriseConnections_GetCount ENDP

EnterpriseConnections_Clear PROC
    mov g_Connections.count,0
    mov eax,1
    ret
EnterpriseConnections_Clear ENDP

END