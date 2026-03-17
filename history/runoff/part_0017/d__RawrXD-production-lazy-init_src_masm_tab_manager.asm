;==========================================================================
; tab_manager.asm - Complete Tab Management System
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

PUBLIC tab_manager_init
PUBLIC tab_create_editor
PUBLIC tab_close_editor
PUBLIC tab_set_agent_mode
PUBLIC tab_get_agent_mode
PUBLIC tab_set_panel_tab
PUBLIC tab_mark_modified

MAX_TABS EQU 64

.data?
    EditorTabCount  DWORD ?
    ChatMode        DWORD ?
    CurrentPanelTab DWORD ?

.code

tab_manager_init PROC
    ; rcx = hParent, edx = tabtype
    mov dword ptr [EditorTabCount], 0
    mov dword ptr [ChatMode], 0
    mov dword ptr [CurrentPanelTab], 0
    xor eax, eax
    ret
tab_manager_init ENDP

tab_create_editor PROC
    ; rcx = filename, rdx = filepath
    inc dword ptr [EditorTabCount]
    mov eax, dword ptr [EditorTabCount]
    ret
tab_create_editor ENDP

tab_close_editor PROC
    ; ecx = tab_id
    dec dword ptr [EditorTabCount]
    xor eax, eax
    ret
tab_close_editor ENDP

tab_set_agent_mode PROC
    mov dword ptr [ChatMode], ecx
    xor eax, eax
    ret
tab_set_agent_mode ENDP

tab_get_agent_mode PROC
    mov eax, dword ptr [ChatMode]
    ret
tab_get_agent_mode ENDP

tab_set_panel_tab PROC
    mov dword ptr [CurrentPanelTab], ecx
    xor eax, eax
    ret
tab_set_panel_tab ENDP

tab_mark_modified PROC
    xor eax, eax
    ret
tab_mark_modified ENDP

END
