; pifabric_ui_settings_panel.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc
include kernel32.inc

includelib user32.lib
includelib kernel32.lib

PUBLIC UiSettingsPanel_Init
PUBLIC UiSettingsPanel_AddOption
PUBLIC UiSettingsPanel_GetSelection
PUBLIC UiSettingsPanel_Shutdown

.data
g_hSettingsList DWORD 0
szSettingsPanelClass db "PiFabricSettingsPanel",0

.code

UiSettingsPanel_Init PROC hParent:DWORD
    invoke CreateWindowEx,WS_EX_CLIENTEDGE,ADDR szSettingsPanelClass,0,\
           WS_CHILD or WS_VISIBLE or LBS_NOTIFY,\
           0,0,200,300,hParent,0,g_hSettingsList,0
    mov g_hSettingsList,eax
    mov eax,1
    ret
UiSettingsPanel_Init ENDP

UiSettingsPanel_AddOption PROC lpText:DWORD
    mov eax,g_hSettingsList
    test eax,eax
    jz @done
    invoke SendMessage,eax,LB_ADDSTRING,0,lpText
@done:
    ret
UiSettingsPanel_AddOption ENDP

UiSettingsPanel_GetSelection PROC
    mov eax,g_hSettingsList
    test eax,eax
    jz @fail
    invoke SendMessage,eax,LB_GETCURSEL,0,0
    ret
@fail:
    mov eax,-1
    ret
UiSettingsPanel_GetSelection ENDP

UiSettingsPanel_Shutdown PROC
    mov eax,g_hSettingsList
    test eax,eax
    jz @done
    invoke DestroyWindow,eax
    mov g_hSettingsList,0
@done:
    ret
UiSettingsPanel_Shutdown ENDP

END