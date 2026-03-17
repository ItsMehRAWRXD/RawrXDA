option casemap:none

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN rawr1024_stream_gguf_model:PROC
EXTERN rawr1024_check_memory_pressure:PROC
EXTERN rawr1024_evict_idle_models:PROC
EXTERN rawr1024_model_access_update:PROC
EXTERN rawr1024_model_release:PROC

EXTERN engine_states:DWORD
EXTERN model_mgr:QWORD
EXTERN loaded_models:QWORD

;==========================================================================
; CONSTANTS AND STRUCTURES FROM OTHER MODULES
;==========================================================================
CACHE_EVICT_THRESHOLD   EQU 90
MAX_LOADED_MODELS       EQU 8

ENGINE_STATE STRUCT
    id                  DWORD ?
    status              DWORD ?
    progress            DWORD ?
    error_code          DWORD ?
    memory_base         QWORD ?
    memory_size         QWORD ?
    reserved            QWORD ?
ENGINE_STATE ENDS

MODEL_STREAM STRUCT
    model_id            DWORD ?
    file_handle         QWORD ?
    model_size          QWORD ?
    loaded_size         QWORD ?
    is_streaming        DWORD ?
    last_access_time    QWORD ?
    memory_base         QWORD ?
    tensor_count        DWORD ?
    quantization_type   DWORD ?
    ref_count           DWORD ?
    state               DWORD ?
MODEL_STREAM ENDS

;==========================================================================
; MENU IDs
;==========================================================================
IDM_FILE_LOAD_MODEL         EQU 40001
IDM_ENGINE_STATUS           EQU 40010
IDM_DISPATCH_TEST           EQU 40015

;==========================================================================
; DATA
;==========================================================================
.data
    menu_file           BYTE "&File", 0
    menu_load_model     BYTE "&Load GGUF Model", 0
    menu_engine         BYTE "&Engine", 0
    menu_status         BYTE "Engine &Status", 0
    menu_dispatch_test  BYTE "&Test Dispatch", 0
    
    dlg_loaded          BYTE "Model loaded successfully!", 0Ah, 0
    dlg_error           BYTE "Failed to load model!", 0Ah, 0

;==========================================================================
; CODE
;==========================================================================
.code

PUBLIC rawr1024_create_model_menu
rawr1024_create_model_menu PROC
    ; Stub: Input RCX = parent, Output RAX = menu handle
    mov     rax, rcx
    ret
rawr1024_create_model_menu ENDP

PUBLIC rawr1024_menu_load_model
rawr1024_menu_load_model PROC
    ; Input: none
    ; Output: RAX = model_id
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    
    ; Call rawr1024_stream_gguf_model with preset path
    lea     rcx, dlg_loaded
    xor     edx, edx
    call    rawr1024_stream_gguf_model
    
    ; EAX now has model ID
    pop     rbx
    pop     rbp
    ret
rawr1024_menu_load_model ENDP

PUBLIC rawr1024_menu_unload_model
rawr1024_menu_unload_model PROC
    ; Input: ECX = model_id
    ; Output: EAX = result
    
    mov     eax, ecx
    call    rawr1024_model_release
    ret
rawr1024_menu_unload_model ENDP

PUBLIC rawr1024_menu_engine_status
rawr1024_menu_engine_status PROC
    ; Display engine status (stub)
    xor     eax, eax
    ret
rawr1024_menu_engine_status ENDP

PUBLIC rawr1024_menu_test_dispatch
rawr1024_menu_test_dispatch PROC
    ; Test dispatch routing (stub)
    xor     eax, eax
    ret
rawr1024_menu_test_dispatch ENDP

PUBLIC rawr1024_menu_clear_cache
rawr1024_menu_clear_cache PROC
    ; Clear all models (stub)
    xor     eax, eax
    ret
rawr1024_menu_clear_cache ENDP

PUBLIC rawr1024_menu_streaming_settings
rawr1024_menu_streaming_settings PROC
    ; Streaming settings dialog (stub)
    xor     eax, eax
    ret
rawr1024_menu_streaming_settings ENDP

END
