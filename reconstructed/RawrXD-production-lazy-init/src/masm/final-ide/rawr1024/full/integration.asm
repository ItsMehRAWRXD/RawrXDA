option casemap:none

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN rawr1024_model_memory_init:PROC
EXTERN rawr1024_init_quad_dual_engines:PROC
EXTERN rawr1024_stream_gguf_model:PROC
EXTERN rawr1024_check_memory_pressure:PROC
EXTERN rawr1024_evict_idle_models:PROC
EXTERN rawr1024_model_access_update:PROC
EXTERN rawr1024_model_release:PROC
EXTERN rawr1024_dispatch_agent_task:PROC
EXTERN rawr1024_hotpatch_engine:PROC

EXTERN engine_states:DWORD
EXTERN model_mgr:QWORD
EXTERN loaded_models:QWORD

;==========================================================================
; INTEGRATION CONSTANTS
;==========================================================================
STATE_IDLE              EQU 0
STATE_LOADING           EQU 1
STATE_READY             EQU 2
STATE_INFERENCING       EQU 3
STATE_ERROR             EQU 4

CACHE_EVICT_THRESHOLD   EQU 90
TOTAL_MEMORY_BYTES      EQU 1073741824  ; 1GB memory budget

;==========================================================================
; INTEGRATION SESSION STRUCTURE
;==========================================================================
INTEGRATION_SESSION STRUCT
    session_id          DWORD ?
    active_models       DWORD 8 DUP (?)
    engine_states       DWORD 8 DUP (?)
    last_model_accessed QWORD ?
    inference_count     QWORD ?
    total_memory_used   QWORD ?
INTEGRATION_SESSION ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    PUBLIC integration_session
    integration_session INTEGRATION_SESSION <>
    
    ; Feature flags
    automatic_streaming_enabled DWORD 1
    memory_eviction_enabled     DWORD 1
    hotpatch_enabled            DWORD 1
    
    ; Configuration
    streaming_threshold_mb      DWORD 512
    idle_timeout_seconds        DWORD 300
    memory_pressure_threshold   DWORD 80

;==========================================================================
; CODE
;==========================================================================
.code

PUBLIC rawr1024_init_full_integration
rawr1024_init_full_integration PROC
    ; Input: RCX = total_memory_bytes
    ; Initialize all subsystems
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    
    ; Initialize model memory manager
    call    rawr1024_model_memory_init
    
    ; Initialize quad dual engines (8 engines)
    call    rawr1024_init_quad_dual_engines
    
    ; Initialize session
    lea     rax, integration_session
    mov     DWORD PTR [rax].INTEGRATION_SESSION.session_id, 1
    mov     QWORD PTR [rax].INTEGRATION_SESSION.inference_count, 0
    mov     QWORD PTR [rax].INTEGRATION_SESSION.total_memory_used, 0
    
    ; Zero out active models array
    xor     ecx, ecx
session_init_loop:
    cmp     ecx, 8
    jge     session_init_done
    
    mov     DWORD PTR [rax + INTEGRATION_SESSION.active_models + rcx*4], 0
    inc     ecx
    jmp     session_init_loop
    
session_init_done:
    pop     rbx
    pop     rbp
    ret
rawr1024_init_full_integration ENDP

PUBLIC rawr1024_ide_load_and_dispatch
rawr1024_ide_load_and_dispatch PROC
    ; Input: RCX = file_path, EDX = preferred_engine (-1 for auto)
    ; Output: EAX = model_id
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    r12
    
    ; Check memory pressure first
    call    rawr1024_check_memory_pressure
    cmp     eax, CACHE_EVICT_THRESHOLD
    jl      load_proceed
    
    ; Memory too full, evict something
    call    rawr1024_evict_idle_models
    
load_proceed:
    ; Load model via streaming
    mov     r8, rcx
    xor     edx, edx        ; Auto select engine
    mov     rcx, r8
    call    rawr1024_stream_gguf_model
    mov     r12d, eax       ; Save model ID
    
    ; Update session active models
    lea     rax, integration_session
    mov     QWORD PTR [rax].INTEGRATION_SESSION.last_model_accessed, rcx
    
    mov     eax, r12d
    pop     r12
    pop     rbx
    pop     rbp
    ret
rawr1024_ide_load_and_dispatch ENDP

PUBLIC rawr1024_ide_run_inference
rawr1024_ide_run_inference PROC
    ; Input: EAX = model_id, ECX = engine_id
    ; Update access time and run inference
    
    push    rbp
    mov     rbp, rsp
    push    r12
    
    mov     r12d, eax       ; Save model_id
    
    ; Update access time (keep model warm)
    mov     eax, r12d
    call    rawr1024_model_access_update
    
    ; Dispatch inference task (type 1 = secondary AI)
    mov     eax, 1          ; task type
    mov     ecx, r12d       ; model id
    call    rawr1024_dispatch_agent_task
    
    ; Increment inference count
    lea     rax, integration_session
    mov     rcx, [rax].INTEGRATION_SESSION.inference_count
    inc     rcx
    mov     [rax].INTEGRATION_SESSION.inference_count, rcx
    
    pop     r12
    pop     rbp
    ret
rawr1024_ide_run_inference ENDP

PUBLIC rawr1024_ide_hotpatch_model
rawr1024_ide_hotpatch_model PROC
    ; Input: EDX = source_engine, EAX = target_model_id
    ; Hotpatch model swap
    
    push    rbp
    mov     rbp, rsp
    
    ; Call hotpatch
    call    rawr1024_hotpatch_engine
    
    pop     rbp
    ret
rawr1024_ide_hotpatch_model ENDP

PUBLIC rawr1024_periodic_memory_maintenance
rawr1024_periodic_memory_maintenance PROC
    ; Background task for memory management
    
    push    rbp
    mov     rbp, rsp
    
    ; Check memory pressure periodically
    call    rawr1024_check_memory_pressure
    
    ; If over threshold, evict
    cmp     eax, 80
    jle     maint_exit
    
    call    rawr1024_evict_idle_models
    
maint_exit:
    pop     rbp
    ret
rawr1024_periodic_memory_maintenance ENDP

PUBLIC rawr1024_get_session_status
rawr1024_get_session_status PROC
    ; Return session state
    
    lea     rax, integration_session
    ret
rawr1024_get_session_status ENDP

END
