; pifabric_gguf_stream_view.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc

includelib kernel32.lib
includelib user32.lib

PUBLIC GgufStreamView_Init
PUBLIC GgufStreamView_AddLine
PUBLIC GgufStreamView_Clear
PUBLIC GgufStreamView_Shutdown

.data
g_hStreamEdit DWORD 0
szStreamViewClass db "PiFabricStreamView",0
szNewLine db 13,10,0
szEmpty db 0

.code

GgufStreamView_Init PROC hParent:DWORD
    invoke CreateWindowEx,WS_EX_CLIENTEDGE,ADDR szStreamViewClass,0,\
           WS_CHILD or WS_VISIBLE or ES_MULTILINE or ES_AUTOVSCROLL or ES_READONLY,\
           0,0,400,200,hParent,0,g_hStreamEdit,0
    mov g_hStreamEdit,eax
    mov eax,1
    ret
GgufStreamView_Init ENDP

GgufStreamView_AddLine PROC lpText:DWORD
    mov eax,g_hStreamEdit
    test eax,eax
    jz @done
    invoke SendMessage,eax,EM_SETSEL,-1,-1
    invoke SendMessage,eax,EM_REPLACESEL,FALSE,lpText
    invoke SendMessage,eax,EM_REPLACESEL,FALSE,ADDR szNewLine
@done:
    ret
GgufStreamView_AddLine ENDP

GgufStreamView_Clear PROC
    mov eax,g_hStreamEdit
    test eax,eax
    jz @done
    invoke SetWindowText,eax,ADDR szEmpty
@done:
    ret
GgufStreamView_Clear ENDP

GgufStreamView_Shutdown PROC
    mov eax,g_hStreamEdit
    test eax,eax
    jz @done
    invoke DestroyWindow,eax
    mov g_hStreamEdit,0
@done:
    ret
GgufStreamView_Shutdown ENDP

END
