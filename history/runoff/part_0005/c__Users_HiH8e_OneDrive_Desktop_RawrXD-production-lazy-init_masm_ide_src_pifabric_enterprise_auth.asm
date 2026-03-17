; pifabric_enterprise_auth.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC EnterpriseAuth_Init
PUBLIC EnterpriseAuth_SetToken
PUBLIC EnterpriseAuth_GetToken
PUBLIC EnterpriseAuth_Clear

.data
g_Token DWORD 0

.code

EnterpriseAuth_Init PROC
    mov g_Token,0
    mov eax,1
    ret
EnterpriseAuth_Init ENDP

EnterpriseAuth_SetToken PROC token:DWORD
    mov g_Token,token
    mov eax,1
    ret
EnterpriseAuth_SetToken ENDP

EnterpriseAuth_GetToken PROC
    mov eax,g_Token
    ret
EnterpriseAuth_GetToken ENDP

EnterpriseAuth_Clear PROC
    mov g_Token,0
    mov eax,1
    ret
EnterpriseAuth_Clear ENDP

END