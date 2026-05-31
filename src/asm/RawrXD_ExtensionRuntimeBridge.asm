; RawrXD_ExtensionRuntimeBridge.asm
; MASM bridge implementations for extension runtime API bridge exports.

OPTION CASEMAP:NONE

EXTERN OutputDebugStringA:PROC

includelib kernel32.lib

.data
sz_obs_active_text   db "[MASM] Observable_Create_ActiveTextEditor",0
sz_obs_visible_text  db "[MASM] Observable_Create_VisibleTextEditors",0
sz_obs_workspace     db "[MASM] Observable_Create_WorkspaceFolders",0
sz_out_create        db "[MASM] OutputChannel_Create",0
sz_out_create_api    db "[MASM] OutputChannel_CreateAPI",0
sz_out_append        db "[MASM] OutputChannel_Append",0
sz_out_append_line   db "[MASM] OutputChannel_AppendLine",0
sz_webview_create    db "[MASM] WebviewPanel_CreateAPI",0
sz_evt_activated     db "[MASM] EventFire_ExtensionActivated",0
sz_evt_deactivated   db "[MASM] EventFire_ExtensionDeactivated",0
sz_evt_dispose       db "[MASM] EventListener_DisposeInternal",0

.code
PUBLIC RawrXD_Observable_Create_ActiveTextEditor_MASM
PUBLIC RawrXD_Observable_Create_VisibleTextEditors_MASM
PUBLIC RawrXD_Observable_Create_WorkspaceFolders_MASM
PUBLIC RawrXD_OutputChannel_Create_MASM
PUBLIC RawrXD_OutputChannel_CreateAPI_MASM
PUBLIC RawrXD_OutputChannel_Append_MASM
PUBLIC RawrXD_OutputChannel_AppendLine_MASM
PUBLIC RawrXD_WebviewPanel_CreateAPI_MASM
PUBLIC RawrXD_EventFire_ExtensionActivated_MASM
PUBLIC RawrXD_EventFire_ExtensionDeactivated_MASM
PUBLIC RawrXD_EventListener_DisposeInternal_MASM

RawrXD_Observable_Create_ActiveTextEditor_MASM PROC
    sub rsp, 28h
    lea rcx, sz_obs_active_text
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Observable_Create_ActiveTextEditor_MASM ENDP

RawrXD_Observable_Create_VisibleTextEditors_MASM PROC
    sub rsp, 28h
    lea rcx, sz_obs_visible_text
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Observable_Create_VisibleTextEditors_MASM ENDP

RawrXD_Observable_Create_WorkspaceFolders_MASM PROC
    sub rsp, 28h
    lea rcx, sz_obs_workspace
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Observable_Create_WorkspaceFolders_MASM ENDP

RawrXD_OutputChannel_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_out_create
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_OutputChannel_Create_MASM ENDP

RawrXD_OutputChannel_CreateAPI_MASM PROC
    sub rsp, 28h
    lea rcx, sz_out_create_api
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_OutputChannel_CreateAPI_MASM ENDP

RawrXD_OutputChannel_Append_MASM PROC
    sub rsp, 28h
    lea rcx, sz_out_append
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_OutputChannel_Append_MASM ENDP

RawrXD_OutputChannel_AppendLine_MASM PROC
    sub rsp, 28h
    lea rcx, sz_out_append_line
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_OutputChannel_AppendLine_MASM ENDP

RawrXD_WebviewPanel_CreateAPI_MASM PROC
    sub rsp, 28h
    lea rcx, sz_webview_create
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_WebviewPanel_CreateAPI_MASM ENDP

RawrXD_EventFire_ExtensionActivated_MASM PROC
    sub rsp, 28h
    lea rcx, sz_evt_activated
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_EventFire_ExtensionActivated_MASM ENDP

RawrXD_EventFire_ExtensionDeactivated_MASM PROC
    sub rsp, 28h
    lea rcx, sz_evt_deactivated
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_EventFire_ExtensionDeactivated_MASM ENDP

RawrXD_EventListener_DisposeInternal_MASM PROC
    sub rsp, 28h
    lea rcx, sz_evt_dispose
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_EventListener_DisposeInternal_MASM ENDP

END
