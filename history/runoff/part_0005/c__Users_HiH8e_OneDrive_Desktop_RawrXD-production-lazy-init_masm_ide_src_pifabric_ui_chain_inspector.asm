; pifabric_ui_chain_inspector.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc
include kernel32.inc

includelib user32.lib
includelib kernel32.lib

PUBLIC UiChainInspector_Init
PUBLIC UiChainInspector_AddEntry
PUBLIC UiChainInspector_Clear
PUBLIC UiChainInspector_Shutdown

.data
g_hChainList DWORD 0
szChainInspectorClass db "PiFabricChainInspector",0

.code

UiChainInspector_Init PROC hParent:DWORD
    invoke CreateWindowEx,WS_EX_CLIENTEDGE,ADDR szChainInspectorClass,0,\
           WS_CHILD or WS_VISIBLE or LBS_NOTIFY,\
           0,0,250,300,hParent,0,g_hChainList,0
    mov g_hChainList,eax
    mov eax,1
    ret
UiChainInspector_Init ENDP

UiChainInspector_AddEntry PROC lpText:DWORD
    mov eax,g_hChainList
    test eax,eax
    jz @done
    invoke SendMessage,eax,LB_ADDSTRING,0,lpText
@done:
    ret
UiChainInspector_AddEntry ENDP

UiChainInspector_Clear PROC
    mov eax,g_hChainList
    test eax,eax
    jz @done
    invoke SendMessage,eax,LB_RESETCONTENT,0,0
@done:
    ret
UiChainInspector_Clear ENDP

UiChainInspector_Shutdown PROC
    mov eax,g_hChainList
    test eax,eax
    jz @done
    invoke DestroyWindow,eax
    mov g_hChainList,0
@done:
    ret
UiChainInspector_Shutdown ENDP

END