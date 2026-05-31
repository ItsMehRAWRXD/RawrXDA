; RawrXD_ExtensionBridge.asm
; MASM bridge implementations for extension-host related bridge exports.

OPTION CASEMAP:NONE

EXTERN OutputDebugStringA:PROC

includelib kernel32.lib

.data
sz_ext_cleanup_lang   db "[MASM] Extension_CleanupLanguageClients",0
sz_ext_cleanup_web    db "[MASM] Extension_CleanupWebviews",0
sz_ext_get_current    db "[MASM] Extension_GetCurrent",0
sz_ext_validate_caps  db "[MASM] Extension_ValidateCapabilities",0
sz_ext_ctx_create     db "[MASM] ExtensionContext_Create",0
sz_ext_proc_msgs      db "[MASM] ExtensionHostBridge_ProcessMessages",0
sz_ext_reg_webview    db "[MASM] ExtensionHostBridge_RegisterWebview",0
sz_ext_send_msg       db "[MASM] ExtensionHostBridge_SendMessage",0
sz_ext_send_note      db "[MASM] ExtensionHostBridge_SendNotification",0
sz_ext_send_req       db "[MASM] ExtensionHostBridge_SendRequest",0
sz_ext_manifest_json  db "[MASM] ExtensionManifest_FromJson",0
sz_ext_module_load    db "[MASM] ExtensionModule_Load",0
sz_ext_storage_path   db "[MASM] ExtensionStorage_GetPath",0

.code
PUBLIC RawrXD_Extension_CleanupLanguageClients_MASM
PUBLIC RawrXD_Extension_CleanupWebviews_MASM
PUBLIC RawrXD_Extension_GetCurrent_MASM
PUBLIC RawrXD_Extension_ValidateCapabilities_MASM
PUBLIC RawrXD_ExtensionContext_Create_MASM
PUBLIC RawrXD_ExtensionHostBridge_ProcessMessages_MASM
PUBLIC RawrXD_ExtensionHostBridge_RegisterWebview_MASM
PUBLIC RawrXD_ExtensionHostBridge_SendMessage_MASM
PUBLIC RawrXD_ExtensionHostBridge_SendNotification_MASM
PUBLIC RawrXD_ExtensionHostBridge_SendRequest_MASM
PUBLIC RawrXD_ExtensionManifest_FromJson_MASM
PUBLIC RawrXD_ExtensionModule_Load_MASM
PUBLIC RawrXD_ExtensionStorage_GetPath_MASM

RawrXD_Extension_CleanupLanguageClients_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_cleanup_lang
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Extension_CleanupLanguageClients_MASM ENDP

RawrXD_Extension_CleanupWebviews_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_cleanup_web
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Extension_CleanupWebviews_MASM ENDP

RawrXD_Extension_GetCurrent_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_get_current
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Extension_GetCurrent_MASM ENDP

RawrXD_Extension_ValidateCapabilities_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_validate_caps
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Extension_ValidateCapabilities_MASM ENDP

RawrXD_ExtensionContext_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_ctx_create
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionContext_Create_MASM ENDP

RawrXD_ExtensionHostBridge_ProcessMessages_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_proc_msgs
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionHostBridge_ProcessMessages_MASM ENDP

RawrXD_ExtensionHostBridge_RegisterWebview_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_reg_webview
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionHostBridge_RegisterWebview_MASM ENDP

RawrXD_ExtensionHostBridge_SendMessage_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_send_msg
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionHostBridge_SendMessage_MASM ENDP

RawrXD_ExtensionHostBridge_SendNotification_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_send_note
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionHostBridge_SendNotification_MASM ENDP

RawrXD_ExtensionHostBridge_SendRequest_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_send_req
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionHostBridge_SendRequest_MASM ENDP

RawrXD_ExtensionManifest_FromJson_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_manifest_json
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionManifest_FromJson_MASM ENDP

RawrXD_ExtensionModule_Load_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_module_load
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionModule_Load_MASM ENDP

RawrXD_ExtensionStorage_GetPath_MASM PROC
    sub rsp, 28h
    lea rcx, sz_ext_storage_path
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionStorage_GetPath_MASM ENDP

END
