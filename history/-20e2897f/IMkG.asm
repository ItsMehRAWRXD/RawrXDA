;==========================================================================
; rawr1024_ide_menu_integration.asm - IDE Menu & Model Management UI
; ==========================================================================
; Exposes quad dual engine controls through IDE menu bar with:
; - Load GGUF Model submenu
; - Model Memory Dashboard
; - Engine Status Display
; - Automatic streaming and cache management
;==========================================================================

option casemap:none

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN rawr1024_stream_gguf_model:PROC
EXTERN rawr1024_check_memory_pressure:PROC
EXTERN rawr1024_evict_idle_models:PROC
EXTERN rawr1024_model_memory_init:PROC
EXTERN rawr1024_model_access_update:PROC
EXTERN rawr1024_model_release:PROC

EXTERN engine_states:DWORD
EXTERN model_mgr:QWORD
EXTERN loaded_models:QWORD

;==========================================================================
; CONSTANTS from rawr1024_model_streaming.asm
;==========================================================================
CACHE_EVICT_THRESHOLD   EQU 90          ; 90% memory - trigger eviction
MAX_LOADED_MODELS       EQU 8

;==========================================================================
; STRUCTURES FROM rawr1024_dual_engine_custom.asm
;==========================================================================
ENGINE_STATE STRUCT
    id                  DWORD ?
    status              DWORD ?
    progress            DWORD ?
    error_code          DWORD ?
    reserved            DWORD ?
ENGINE_STATE ENDS

;==========================================================================
; MENU COMMAND IDs
;==========================================================================
IDM_FILE_LOAD_MODEL         EQU 40001
IDM_FILE_LOAD_MULTI_MODEL   EQU 40002
IDM_FILE_UNLOAD_MODEL       EQU 40003
IDM_ENGINE_STATUS           EQU 40010
IDM_ENGINE_DASHBOARD        EQU 40011
IDM_MODEL_STREAM_SETTINGS   EQU 40012
IDM_MEMORY_CLEAR_CACHE      EQU 40013
IDM_MEMORY_EVICT_IDLE       EQU 40014
IDM_DISPATCH_TEST           EQU 40015

;==========================================================================
; MODEL LOAD DIALOG STRUCTURE
;==========================================================================
MODEL_LOAD_DIALOG STRUCT
    file_path           BYTE 260 DUP (?)
    model_name          BYTE 128 DUP (?)
    engine_id           DWORD ?
    auto_stream         DWORD ?         ; 0=off, 1=on
    memory_limit_mb     DWORD ?
    quantization_pref   DWORD ?         ; 0=any, 1=Q4_0, etc.
MODEL_LOAD_DIALOG ENDS

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    PUBLIC model_load_dialog
    model_load_dialog   MODEL_LOAD_DIALOG <>
    
    ; Menu strings
    menu_file           BYTE "&File", 0
    menu_model          BYTE "&Model", 0
    menu_load_model     BYTE "&Load GGUF Model", 0
    menu_load_multi     BYTE "Load &Multiple Models", 0
    menu_unload         BYTE "&Unload Model", 0
    menu_sep1           BYTE "-", 0
    
    menu_engine         BYTE "&Engine", 0
    menu_status         BYTE "Engine &Status", 0
    menu_dashboard      BYTE "&Dashboard", 0
    menu_dispatch_test  BYTE "&Test Dispatch", 0
    menu_sep2           BYTE "-", 0
    
    menu_memory         BYTE "&Memory", 0
    menu_streaming      BYTE "&Streaming Settings", 0
    menu_clear_cache    BYTE "&Clear Cache", 0
    menu_evict_idle     BYTE "&Evict Idle Models", 0
    
    ; Dialog messages
    dlg_load_title      BYTE "Load GGUF Model", 0
    dlg_select_file     BYTE "Select GGUF model file", 0
    dlg_select_engine   BYTE "Select target engine", 0
    dlg_loading         BYTE "Loading model, please wait...", 0
    dlg_loaded          BYTE "Model loaded successfully!", 0
    dlg_error           BYTE "Failed to load model!", 0

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;--------------------------------------------------------------------------
; Create Model Management Menu
;--------------------------------------------------------------------------
PUBLIC rawr1024_create_model_menu
rawr1024_create_model_menu PROC
    ; Input: RCX = parent_menu_handle
    ; Output: RAX = menu_handle
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    
    ; Create menu structure:
    ; File -> Load GGUF Model
    ;      -> Load Multiple Models
    ;      -> Unload Model
    ; Engine -> Status
    ;       -> Dashboard
    ;       -> Test Dispatch
    ; Memory -> Streaming Settings
    ;       -> Clear Cache
    ;       -> Evict Idle Models
    
    ; This is a placeholder structure
    ; In real implementation, would call Win32 CreateMenu, AppendMenu, etc.
    
    mov     rax, rcx            ; Return parent handle for now
    
    pop     rsi
    pop     rbx
    pop     rbp
    ret
rawr1024_create_model_menu ENDP

;--------------------------------------------------------------------------
; Handle Load Model Menu Command
;--------------------------------------------------------------------------
PUBLIC rawr1024_menu_load_model
rawr1024_menu_load_model PROC
    ; Input: None (uses global model_load_dialog)
    ; Output: RAX = model_id (or 0 on failure)
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    r12
    
    ; Show file open dialog (simplified - placeholder)
    ; In real impl: CreateFileDialog, GetOpenFileName, etc.
    
    lea     rax, model_load_dialog
    
    ; Preset for demo: assume file path is set
    lea     rcx, [rax].MODEL_LOAD_DIALOG.file_path
    
    ; Load model via streaming
    mov     edx, DWORD PTR [rax].MODEL_LOAD_DIALOG.engine_id
    call    rawr1024_stream_gguf_model
    
    ; If successful (RAX != 0), display status
    test    rax, rax
    jz      menu_load_fail
    
    mov     r12, rax            ; save model_id
    
    ; Check memory pressure
    call    rawr1024_check_memory_pressure
    cmp     eax, CACHE_EVICT_THRESHOLD
    jl      menu_load_success
    
    ; Trigger eviction if needed
    call    rawr1024_evict_idle_models
    
menu_load_success:
    mov     rax, r12            ; return model_id
    jmp     menu_load_exit
    
menu_load_fail:
    xor     rax, rax
    
menu_load_exit:
    pop     r12
    pop     rsi
    pop     rbx
    pop     rbp
    ret
rawr1024_menu_load_model ENDP

;--------------------------------------------------------------------------
; Handle Unload Model Menu Command
;--------------------------------------------------------------------------
PUBLIC rawr1024_menu_unload_model
rawr1024_menu_unload_model PROC
    ; Input: RCX = model_id
    ; Output: RAX = 1 on success, 0 on failure
    
    push    rbp
    mov     rbp, rsp
    
    ; Call release function (decrements ref_count)
    call    rawr1024_model_release
    
    ; If ref_count reaches 0, it becomes candidate for eviction
    ; Actual unload happens through eviction mechanism
    
    cmp     eax, 0
    jl      menu_unload_fail
    
    mov     rax, 1
    jmp     menu_unload_exit
    
menu_unload_fail:
    xor     rax, rax
    
menu_unload_exit:
    pop     rbp
    ret
rawr1024_menu_unload_model ENDP

;--------------------------------------------------------------------------
; Display Engine Status Dashboard
;--------------------------------------------------------------------------
PUBLIC rawr1024_menu_engine_status
rawr1024_menu_engine_status PROC
    ; Input: None
    ; Output: RAX = status_code
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    
    ; Collect status from all 8 engines
    xor     ecx, ecx            ; engine counter
status_loop:
    cmp     ecx, 8
    jge     status_complete
    
    ; Get engine state
    imul    rax, rcx, SIZEOF ENGINE_STATE
    lea     rsi, engine_states
    add     rsi, rax
    
    ; Extract: ID, status, progress, error_code
    mov     r8d, DWORD PTR [rsi].ENGINE_STATE.id
    mov     r9d, DWORD PTR [rsi].ENGINE_STATE.status
    mov     r10d, DWORD PTR [rsi].ENGINE_STATE.progress
    mov     r11d, DWORD PTR [rsi].ENGINE_STATE.error_code
    
    ; Format and display (placeholder)
    ; Real impl: sprintf, OutputDebugString, UI update, etc.
    
    inc     ecx
    jmp     status_loop
    
status_complete:
    ; Also collect memory status
    call    rawr1024_check_memory_pressure
    ; Display memory percentage
    
    mov     rax, 1
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
rawr1024_menu_engine_status ENDP

;--------------------------------------------------------------------------
; Test Dispatch Functionality
;--------------------------------------------------------------------------
PUBLIC rawr1024_menu_test_dispatch
rawr1024_menu_test_dispatch PROC
    ; Input: None
    ; Output: RAX = test_result (1=pass, 0=fail)
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    
    ; Test dispatch of all 4 task types
    
    ; Type 0: Primary AI
    mov     rcx, 0
    xor     rdx, rdx
    xor     r8, r8
    call    rawr1024_dispatch_agent_task
    mov     ebx, eax            ; save result
    
    cmp     ebx, 0
    jl      dispatch_test_fail
    cmp     ebx, 1
    jg      dispatch_test_fail
    
    ; Type 1: Secondary AI
    mov     rcx, 1
    xor     rdx, rdx
    xor     r8, r8
    call    rawr1024_dispatch_agent_task
    cmp     eax, 2
    jl      dispatch_test_fail
    cmp     eax, 3
    jg      dispatch_test_fail
    
    ; Type 3: Orchestration
    mov     rcx, 3
    xor     rdx, rdx
    xor     r8, r8
    call    rawr1024_dispatch_agent_task
    cmp     eax, 6
    jne     dispatch_test_fail
    
    mov     rax, 1
    jmp     dispatch_test_exit
    
dispatch_test_fail:
    xor     rax, rax
    
dispatch_test_exit:
    pop     rbx
    pop     rbp
    ret
rawr1024_menu_test_dispatch ENDP

;--------------------------------------------------------------------------
; Clear Model Cache
;--------------------------------------------------------------------------
PUBLIC rawr1024_menu_clear_cache
rawr1024_menu_clear_cache PROC
    ; Input: None
    ; Output: RAX = models_evicted
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    
    xor     ebx, ebx            ; evict counter
    
clear_cache_loop:
    call    rawr1024_evict_idle_models
    test    eax, eax
    jz      clear_cache_done
    
    inc     ebx
    jmp     clear_cache_loop
    
clear_cache_done:
    mov     rax, rbx
    pop     rbx
    pop     rbp
    ret
rawr1024_menu_clear_cache ENDP

;--------------------------------------------------------------------------
; Configure Streaming Settings
;--------------------------------------------------------------------------
PUBLIC rawr1024_menu_streaming_settings
rawr1024_menu_streaming_settings PROC
    ; Input: None (uses global model_load_dialog)
    ; Output: RAX = 1 on success
    
    push    rbp
    mov     rbp, rsp
    
    ; Show settings dialog (placeholder)
    ; Options:
    ; - Streaming threshold (MB)
    ; - Chunk size (MB)
    ; - Auto-evict idle (yes/no)
    ; - Idle timeout (seconds)
    
    mov     rax, 1
    pop     rbp
    ret
rawr1024_menu_streaming_settings ENDP

;--------------------------------------------------------------------------
; Declare external ENGINE_STATE (from rawr1024_dual_engine_custom.asm)
;--------------------------------------------------------------------------
EXTERN engine_states:BYTE
EXTERN rawr1024_stream_gguf_model:PROC
EXTERN rawr1024_check_memory_pressure:PROC
EXTERN rawr1024_evict_idle_models:PROC
EXTERN rawr1024_model_access_update:PROC
EXTERN rawr1024_model_release:PROC
EXTERN rawr1024_dispatch_agent_task:PROC

END
