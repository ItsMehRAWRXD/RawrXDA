; pifabric_ui_settings_panel.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc

includelib user32.lib

PUBLIC  UiSettingsPanel_Init
PUBLIC  UiSettingsPanel_Load
PUBLIC  UiSettingsPanel_Save
PUBLIC  UiSettingsPanel_Show
PUBLIC  UiSettingsPanel_Hide

settingsPanelHandle dq 0

.code

UiSettingsPanel_Init PROC USES esi edi hParent:DWORD
    ; Create a dialog for settings
    invoke CreateWindowEx, 0, "DIALOG", "Settings",
        WS_CHILD or WS_VISIBLE,
        0, 0, 400, 300,
        hParent, 0, NULL, NULL
    mov settingsPanelHandle, eax
    mov eax, 1
    ret
UiSettingsPanel_Init ENDP

UiSettingsPanel_Load PROC USES esi edi
    ; Load settings from file (stub)
    mov eax, 1
    ret
UiSettingsPanel_Load ENDP

UiSettingsPanel_Save PROC USES esi edi
    ; Save settings to file (stub)
    mov eax, 1
    ret
UiSettingsPanel_Save ENDP

UiSettingsPanel_Show PROC
    mov eax, settingsPanelHandle
    test eax, eax
    jz @done
    invoke ShowWindow, eax, SW_SHOW
@done:
    xor eax, eax
    ret
UiSettingsPanel_Show ENDP

UiSettingsPanel_Hide PROC
    mov eax, settingsPanelHandle
    test eax, eax
    jz @done
    invoke ShowWindow, eax, SW_HIDE
@done:
    xor eax, eax
    ret
UiSettingsPanel_Hide ENDP

END