; pifabric_gguf_diff_view.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc

includelib kernel32.lib
includelib user32.lib

PUBLIC GgufDiffView_Init
PUBLIC GgufDiffView_AddDiff
PUBLIC GgufDiffView_Clear
PUBLIC GgufDiffView_Shutdown

.data
g_hDiffEdit DWORD 0
szDiffViewClass db "PiFabricDiffView",0
szNewLine db 13,10,0
szEmpty db 0

.code

GgufDiffView_Init PROC hParent:DWORD
    invoke CreateWindowEx,WS_EX_CLIENTEDGE,ADDR szDiffViewClass,0,\
           WS_CHILD or WS_VISIBLE or ES_MULTILINE or ES_AUTOVSCROLL or ES_READONLY,\
           0,0,400,200,hParent,0,g_hDiffEdit,0
    mov g_hDiffEdit,eax
    mov eax,1
    ret
GgufDiffView_Init ENDP

GgufDiffView_AddDiff PROC lpText:DWORD
    mov eax,g_hDiffEdit
    test eax,eax
    jz @done
    invoke SendMessage,eax,EM_SETSEL,-1,-1
    invoke SendMessage,eax,EM_REPLACESEL,FALSE,lpText
    invoke SendMessage,eax,EM_REPLACESEL,FALSE,ADDR szNewLine
@done:
    ret
GgufDiffView_AddDiff ENDP

GgufDiffView_Clear PROC
    mov eax,g_hDiffEdit
    test eax,eax
    jz @done
    invoke SetWindowText,eax,ADDR szEmpty
@done:
    ret
GgufDiffView_Clear ENDP

GgufDiffView_Shutdown PROC
    mov eax,g_hDiffEdit
    test eax,eax
    jz @done
    invoke DestroyWindow,eax
    mov g_hDiffEdit,0
@done:
    ret
GgufDiffView_Shutdown ENDP

END
