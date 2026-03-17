; pifabric_gguf_jailbreak_editor.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc

includelib kernel32.lib
includelib user32.lib

PUBLIC GgufJailbreakEditor_Init
PUBLIC GgufJailbreakEditor_AddRule
PUBLIC GgufJailbreakEditor_Clear
PUBLIC GgufJailbreakEditor_Shutdown

.data
g_hEditorList DWORD 0
szJailbreakEditorClass db "PiFabricJailbreakEditor",0

.code

GgufJailbreakEditor_Init PROC hParent:DWORD
    invoke CreateWindowEx,WS_EX_CLIENTEDGE,ADDR szJailbreakEditorClass,0,\
           WS_CHILD or WS_VISIBLE or LBS_NOTIFY,\
           0,0,300,300,hParent,0,g_hEditorList,0
    mov g_hEditorList,eax
    mov eax,1
    ret
GgufJailbreakEditor_Init ENDP

GgufJailbreakEditor_AddRule PROC lpText:DWORD
    mov eax,g_hEditorList
    test eax,eax
    jz @done
    invoke SendMessage,eax,LB_ADDSTRING,0,lpText
@done:
    ret
GgufJailbreakEditor_AddRule ENDP

GgufJailbreakEditor_Clear PROC
    mov eax,g_hEditorList
    test eax,eax
    jz @done
    invoke SendMessage,eax,LB_RESETCONTENT,0,0
@done:
    ret
GgufJailbreakEditor_Clear ENDP

GgufJailbreakEditor_Shutdown PROC
    mov eax,g_hEditorList
    test eax,eax
    jz @done
    invoke DestroyWindow,eax
    mov g_hEditorList,0
@done:
    ret
GgufJailbreakEditor_Shutdown ENDP

END
