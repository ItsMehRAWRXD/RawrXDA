; pifabric_enterprise_wiring.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

EXTERN EnterpriseSettings_Set:PROC
EXTERN EnterpriseConnections_Add:PROC
EXTERN EnterpriseAuth_SetToken:PROC
EXTERN EnterpriseLog_Write:PROC
EXTERN EnterpriseMetrics_Record:PROC

PUBLIC EnterpriseWiring_Init
PUBLIC EnterpriseWiring_ConnectAll

.code

EnterpriseWiring_Init PROC
    ; initialize subsystems
    invoke EnterpriseSettings_Set,1,100
    invoke EnterpriseConnections_Add,1,200
    invoke EnterpriseAuth_SetToken,12345
    invoke EnterpriseLog_Write,1,300
    invoke EnterpriseMetrics_Record,1,400
    mov eax,1
    ret
EnterpriseWiring_Init ENDP

EnterpriseWiring_ConnectAll PROC
    ; glue logic to tie modules together
    mov eax,1
    ret
EnterpriseWiring_ConnectAll ENDP

END