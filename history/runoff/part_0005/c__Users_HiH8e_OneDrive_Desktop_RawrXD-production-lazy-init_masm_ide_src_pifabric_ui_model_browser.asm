; pifabric_ui_model_browser.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc
include kernel32.inc

includelib user32.lib
includelib kernel32.lib

PUBLIC UiModelBrowser_Init
PUBLIC UiModelBrowser_AddItem
PUBLIC UiModelBrowser_Clear
PUBLIC UiModelBrowser_GetSelection
PUBLIC UiModelBrowser_Shutdown

.data
g_hModelList DWORD 0
szModelBrowserClass db "PiFabricModelBrowser",0

.code

UiModelBrowser_Init PROC hParent:DWORD
    invoke CreateWindowEx,WS_EX_CLIENTEDGE,ADDR szModelBrowserClass,0,\
           WS_CHILD or WS_VISIBLE or LBS_NOTIFY,\
           0,0,200,400,hParent,0,g_hModelList,0
    mov g_hModelList,eax
    mov eax,1
    ret
UiModelBrowser_Init ENDP

UiModelBrowser_AddItem PROC lpText:DWORD
    mov eax,g_hModelList
    test eax,eax
    jz @done
    invoke SendMessage,eax,LB_ADDSTRING,0,lpText
@done:
    ret
UiModelBrowser_AddItem ENDP

UiModelBrowser_Clear PROC
    mov eax,g_hModelList
    test eax,eax
    jz @done
    invoke SendMessage,eax,LB_RESETCONTENT,0,0
@done:
    ret
UiModelBrowser_Clear ENDP

UiModelBrowser_GetSelection PROC
    mov eax,g_hModelList
    test eax,eax
    jz @fail
    invoke SendMessage,eax,LB_GETCURSEL,0,0
    ret
@fail:
    mov eax,-1
    ret
UiModelBrowser_GetSelection ENDP

UiModelBrowser_Shutdown PROC
    mov eax,g_hModelList
    test eax,eax
    jz @done
    invoke DestroyWindow,eax
    mov g_hModelList,0
@done:
    ret
UiModelBrowser_Shutdown ENDP

END