; RawrXD_DataBridge.asm
; MASM bridge implementations for JSON/LSP/ModelBridge exports.

OPTION CASEMAP:NONE

EXTERN OutputDebugStringA:PROC

includelib kernel32.lib

.data
sz_json_parse_string      db "[MASM] Json_ParseString",0
sz_json_parse_object      db "[MASM] Json_ParseObject",0
sz_json_parse_file        db "[MASM] Json_ParseFile",0
sz_json_get_string        db "[MASM] Json_GetString",0
sz_json_get_int           db "[MASM] Json_GetInt",0
sz_json_get_array         db "[MASM] Json_GetArray",0
sz_json_get_obj_field     db "[MASM] Json_GetObjectField",0
sz_json_get_str_field     db "[MASM] Json_GetStringField",0
sz_json_get_arr_field     db "[MASM] Json_GetArrayField",0
sz_json_get_obj_keys      db "[MASM] Json_GetObjectKeys",0
sz_json_has_field         db "[MASM] Json_HasField",0
sz_json_obj_create        db "[MASM] JsonObject_Create",0

sz_lsp_handshake          db "[MASM] LSP_Handshake_Sequence",0
sz_lsp_build_note         db "[MASM] LSP_JsonRpc_BuildNotification",0
sz_lsp_transport_write    db "[MASM] LSP_Transport_Write",0
sz_lsp_forward_message    db "[MASM] LspClient_ForwardMessage",0

sz_model_init             db "[MASM] ModelBridge_Init",0
sz_model_load             db "[MASM] ModelBridge_LoadModel",0
sz_model_unload           db "[MASM] ModelBridge_UnloadModel",0
sz_model_validate         db "[MASM] ModelBridge_ValidateLoad",0
sz_model_profile          db "[MASM] ModelBridge_GetProfile",0

.code
PUBLIC RawrXD_Json_ParseString_MASM
PUBLIC RawrXD_Json_ParseObject_MASM
PUBLIC RawrXD_Json_ParseFile_MASM
PUBLIC RawrXD_Json_GetString_MASM
PUBLIC RawrXD_Json_GetInt_MASM
PUBLIC RawrXD_Json_GetArray_MASM
PUBLIC RawrXD_Json_GetObjectField_MASM
PUBLIC RawrXD_Json_GetStringField_MASM
PUBLIC RawrXD_Json_GetArrayField_MASM
PUBLIC RawrXD_Json_GetObjectKeys_MASM
PUBLIC RawrXD_Json_HasField_MASM
PUBLIC RawrXD_JsonObject_Create_MASM
PUBLIC RawrXD_LSP_Handshake_Sequence_MASM
PUBLIC RawrXD_LSP_JsonRpc_BuildNotification_MASM
PUBLIC RawrXD_LSP_Transport_Write_MASM
PUBLIC RawrXD_LspClient_ForwardMessage_MASM
PUBLIC RawrXD_ModelBridge_Init_MASM
PUBLIC RawrXD_ModelBridge_LoadModel_MASM
PUBLIC RawrXD_ModelBridge_UnloadModel_MASM
PUBLIC RawrXD_ModelBridge_ValidateLoad_MASM
PUBLIC RawrXD_ModelBridge_GetProfile_MASM

RawrXD_Json_ParseString_MASM PROC
    sub rsp, 28h
    lea rcx, sz_json_parse_string
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_ParseString_MASM ENDP

RawrXD_Json_ParseObject_MASM PROC
    sub rsp, 28h
    lea rcx, sz_json_parse_object
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_ParseObject_MASM ENDP

RawrXD_Json_ParseFile_MASM PROC
    sub rsp, 28h
    lea rcx, sz_json_parse_file
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_ParseFile_MASM ENDP

RawrXD_Json_GetString_MASM PROC
    sub rsp, 28h
    lea rcx, sz_json_get_string
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetString_MASM ENDP

RawrXD_Json_GetInt_MASM PROC
    sub rsp, 28h
    lea rcx, sz_json_get_int
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetInt_MASM ENDP

RawrXD_Json_GetArray_MASM PROC
    sub rsp, 28h
    lea rcx, sz_json_get_array
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetArray_MASM ENDP

RawrXD_Json_GetObjectField_MASM PROC
    sub rsp, 28h
    lea rcx, sz_json_get_obj_field
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetObjectField_MASM ENDP

RawrXD_Json_GetStringField_MASM PROC
    sub rsp, 28h
    lea rcx, sz_json_get_str_field
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetStringField_MASM ENDP

RawrXD_Json_GetArrayField_MASM PROC
    sub rsp, 28h
    lea rcx, sz_json_get_arr_field
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetArrayField_MASM ENDP

RawrXD_Json_GetObjectKeys_MASM PROC
    sub rsp, 28h
    lea rcx, sz_json_get_obj_keys
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetObjectKeys_MASM ENDP

RawrXD_Json_HasField_MASM PROC
    sub rsp, 28h
    lea rcx, sz_json_has_field
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_HasField_MASM ENDP

RawrXD_JsonObject_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_json_obj_create
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_JsonObject_Create_MASM ENDP

RawrXD_LSP_Handshake_Sequence_MASM PROC
    sub rsp, 28h
    lea rcx, sz_lsp_handshake
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_LSP_Handshake_Sequence_MASM ENDP

RawrXD_LSP_JsonRpc_BuildNotification_MASM PROC
    sub rsp, 28h
    lea rcx, sz_lsp_build_note
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_LSP_JsonRpc_BuildNotification_MASM ENDP

RawrXD_LSP_Transport_Write_MASM PROC
    sub rsp, 28h
    lea rcx, sz_lsp_transport_write
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_LSP_Transport_Write_MASM ENDP

RawrXD_LspClient_ForwardMessage_MASM PROC
    sub rsp, 28h
    lea rcx, sz_lsp_forward_message
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_LspClient_ForwardMessage_MASM ENDP

RawrXD_ModelBridge_Init_MASM PROC
    sub rsp, 28h
    lea rcx, sz_model_init
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelBridge_Init_MASM ENDP

RawrXD_ModelBridge_LoadModel_MASM PROC
    sub rsp, 28h
    lea rcx, sz_model_load
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelBridge_LoadModel_MASM ENDP

RawrXD_ModelBridge_UnloadModel_MASM PROC
    sub rsp, 28h
    lea rcx, sz_model_unload
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelBridge_UnloadModel_MASM ENDP

RawrXD_ModelBridge_ValidateLoad_MASM PROC
    sub rsp, 28h
    lea rcx, sz_model_validate
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelBridge_ValidateLoad_MASM ENDP

RawrXD_ModelBridge_GetProfile_MASM PROC
    sub rsp, 28h
    lea rcx, sz_model_profile
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelBridge_GetProfile_MASM ENDP

END
