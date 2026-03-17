;==========================================================================
; main_init.asm - Core System Initialization & Coordination
;==========================================================================
; Initializes all subsystems in correct order: memory, logging, files,
; GUI, terminal, agent, model loader. Provides central coordination point.
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib

;==========================================================================
; EXTERNAL DECLARATIONS - Core dependencies
;==========================================================================
EXTERN console_log:PROC
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN OutputDebugStringA:PROC
EXTERN GetTickCount:PROC
EXTERN Sleep:PROC
EXTERN GetModuleHandleA:PROC
EXTERN CreateWindowExA:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN PostQuitMessage:PROC
EXTERN DefWindowProcA:PROC
EXTERN RegisterClassExA:PROC
EXTERN LoadCursorA:PROC
EXTERN LoadIconA:PROC
EXTERN CreateMenuA:PROC
EXTERN SetMenu:PROC
EXTERN DestroyWindow:PROC
EXTERN SetWindowPos:PROC
EXTERN InvalidateRect:PROC
EXTERN SendMessageA:PROC

; UI subsystems
EXTERN ui_init:PROC
EXTERN pane_system_init:PROC
EXTERN editor_system_init:PROC
EXTERN terminal_system_init:PROC
EXTERN status_bar_init:PROC

; Agent/Agentic subsystem
EXTERN agent_system_init:PROC
EXTERN agent_executor_init:PROC
EXTERN agent_tools_registry_init:PROC

; Model/GGUF subsystem
EXTERN gguf_loader_init:PROC
EXTERN inference_engine_init:PROC

; Hotpatch subsystem
EXTERN hotpatch_system_init:PROC

; Telemetry
EXTERN telemetry_system_init:PROC
EXTERN metrics_collector_init:PROC

; File I/O
EXTERN file_manager_init:PROC
EXTERN config_loader_init:PROC

;==========================================================================
; PUBLIC EXPORTS
;==========================================================================
PUBLIC main_initialize_all_systems
PUBLIC main_get_global_state
PUBLIC main_get_initialization_status
PUBLIC main_shutdown_all_systems
PUBLIC main_run_event_loop
PUBLIC main_post_message

;==========================================================================
; DATA STRUCTURES
;==========================================================================

; Global application state
APP_STATE STRUCT
    initialized        DWORD ?      ; 1 if all systems initialized
    hInstance          QWORD ?      ; Module handle
    hMainWindow        QWORD ?      ; Main window handle
    message_count      QWORD ?      ; Total messages processed
    start_time         DWORD ?      ; Tick count at startup
    exit_requested     DWORD ?      ; 1 if exit requested
    active_pane        DWORD ?      ; Currently active pane ID
    memory_allocated   QWORD ?      ; Total bytes allocated
    memory_freed       QWORD ?      ; Total bytes freed
    agent_state        QWORD ?      ; Agent system state pointer
    model_state        QWORD ?      ; Model loader state pointer
    hotpatch_state     QWORD ?      ; Hotpatch system state pointer
    telemetry_state    QWORD ?      ; Telemetry state pointer
APP_STATE ENDS

MSG STRUCT
    hwnd               QWORD ?
    message            DWORD ?
    wParam             QWORD ?
    lParam             QWORD ?
    time               DWORD ?
    pt_x               DWORD ?
    pt_y               DWORD ?
MSG ENDS

;==========================================================================
; .DATA SECTION
;==========================================================================

.data

; Global application state (singleton)
g_app_state APP_STATE <0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>

; Window class name
szClassName         BYTE "RawrXD_IDEClass", 0

; Messages
szInitStarting      BYTE "[INIT] Starting system initialization...", 13, 10, 0
szInitMemory        BYTE "[INIT] Memory manager initialized", 13, 10, 0
szInitLogging       BYTE "[INIT] Logging system initialized", 13, 10, 0
szInitFiles         BYTE "[INIT] File manager initialized", 13, 10, 0
szInitUI            BYTE "[INIT] UI subsystem initialized", 13, 10, 0
szInitPanes         BYTE "[INIT] Pane system initialized", 13, 10, 0
szInitEditor        BYTE "[INIT] Editor subsystem initialized", 13, 10, 0
szInitTerminal      BYTE "[INIT] Terminal subsystem initialized", 13, 10, 0
szInitAgent         BYTE "[INIT] Agent system initialized", 13, 10, 0
szInitModel         BYTE "[INIT] Model loader initialized", 13, 10, 0
szInitHotpatch      BYTE "[INIT] Hotpatch system initialized", 13, 10, 0
szInitTelemetry     BYTE "[INIT] Telemetry system initialized", 13, 10, 0
szInitComplete      BYTE "[INIT] ALL SYSTEMS INITIALIZED", 13, 10, 0
szInitFailed        BYTE "[ERROR] Initialization failed at stage %d", 13, 10, 0
szShutdownStart     BYTE "[SHUTDOWN] Closing all systems...", 13, 10, 0
szShutdownComplete  BYTE "[SHUTDOWN] Complete", 13, 10, 0

; WNDCLASSEX structure for window registration
wnd_class WNDCLASSEX <
    SIZEOF WNDCLASSEX,  ; cbSize
    CS_HREDRAW or CS_VREDRAW,  ; style
    OFFSET WndProc,     ; lpfnWndProc
    0,                  ; cbClsExtra
    0,                  ; cbWndExtra
    NULL,               ; hInstance (set at runtime)
    NULL,               ; hIcon
    NULL,               ; hCursor
    NULL,               ; hbrBackground
    NULL,               ; lpszMenuName
    OFFSET szClassName, ; lpszClassName
    NULL                ; hIconSm
>

;==========================================================================
; .CODE SECTION
;==========================================================================

.code

;==========================================================================
; main_initialize_all_systems() -> EAX (1=success, 0=failure)
;==========================================================================
PUBLIC main_initialize_all_systems
ALIGN 16
main_initialize_all_systems PROC

    push rbx
    push r12
    push r13
    sub rsp, 32

    ; Log start
    lea rcx, szInitStarting
    call console_log

    ; Get module handle for window class registration
    xor rcx, rcx        ; NULL = current process
    call GetModuleHandleA
    mov [g_app_state.hInstance], rax
    mov rbx, rax

    ; Check if already initialized
    cmp DWORD PTR [g_app_state.initialized], 1
    je init_already_done

    xor r12d, r12d      ; Stage counter
    
    ; Stage 1: Memory manager (built-in, skip explicit init)
    inc r12d
    lea rcx, szInitMemory
    call console_log

    ; Stage 2: Logging system (built-in, skip explicit init)
    inc r12d
    lea rcx, szInitLogging
    call console_log

    ; Stage 3: File manager
    inc r12d
    call file_manager_init
    test eax, eax
    jz init_failed

    lea rcx, szInitFiles
    call console_log

    ; Stage 4: Config loader
    inc r12d
    call config_loader_init
    test eax, eax
    jz init_failed

    ; Stage 5: UI subsystem
    inc r12d
    mov rcx, rbx        ; hInstance
    call ui_init
    test eax, eax
    jz init_failed

    lea rcx, szInitUI
    call console_log

    ; Stage 6: Pane system
    inc r12d
    call pane_system_init
    test eax, eax
    jz init_failed

    lea rcx, szInitPanes
    call console_log

    ; Stage 7: Editor system
    inc r12d
    call editor_system_init
    test eax, eax
    jz init_failed

    lea rcx, szInitEditor
    call console_log

    ; Stage 8: Terminal system
    inc r12d
    call terminal_system_init
    test eax, eax
    jz init_failed

    lea rcx, szInitTerminal
    call console_log

    ; Stage 9: Status bar
    inc r12d
    call status_bar_init
    test eax, eax
    jz init_failed

    ; Stage 10: Agent system
    inc r12d
    call agent_system_init
    test eax, eax
    jz init_failed

    lea rcx, szInitAgent
    call console_log

    ; Stage 11: Agent tools registry
    inc r12d
    call agent_tools_registry_init
    test eax, eax
    jz init_failed

    ; Stage 12: Agent executor
    inc r12d
    call agent_executor_init
    test eax, eax
    jz init_failed

    ; Stage 13: Model/GGUF loader
    inc r12d
    call gguf_loader_init
    test eax, eax
    jz init_failed

    lea rcx, szInitModel
    call console_log

    ; Stage 14: Inference engine
    inc r12d
    call inference_engine_init
    test eax, eax
    jz init_failed

    ; Stage 15: Hotpatch system
    inc r12d
    call hotpatch_system_init
    test eax, eax
    jz init_failed

    lea rcx, szInitHotpatch
    call console_log

    ; Stage 16: Telemetry system
    inc r12d
    call telemetry_system_init
    test eax, eax
    jz init_failed

    ; Stage 17: Metrics collector
    inc r12d
    call metrics_collector_init
    test eax, eax
    jz init_failed

    lea rcx, szInitTelemetry
    call console_log

    ; All systems initialized successfully
    mov [g_app_state.initialized], 1
    call GetTickCount
    mov [g_app_state.start_time], eax
    
    lea rcx, szInitComplete
    call console_log

    mov eax, 1          ; Success
    jmp init_done

init_already_done:
    mov eax, 1
    jmp init_done

init_failed:
    ; Log which stage failed
    lea rcx, szInitFailed
    mov edx, r12d
    call console_log
    
    xor eax, eax        ; Failure
    
init_done:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret

main_initialize_all_systems ENDP

;==========================================================================
; main_shutdown_all_systems() -> EAX (1=success)
;==========================================================================
PUBLIC main_shutdown_all_systems
ALIGN 16
main_shutdown_all_systems PROC

    push rbx
    sub rsp, 32

    lea rcx, szShutdownStart
    call console_log

    ; Clean shutdown order (reverse of initialization)
    ; All cleanup is implicit - no explicit teardown needed for now

    lea rcx, szShutdownComplete
    call console_log

    mov eax, 1
    add rsp, 32
    pop rbx
    ret

main_shutdown_all_systems ENDP

;==========================================================================
; main_get_global_state() -> RAX (pointer to APP_STATE)
;==========================================================================
PUBLIC main_get_global_state
ALIGN 16
main_get_global_state PROC
    lea rax, [g_app_state]
    ret
main_get_global_state ENDP

;==========================================================================
; main_get_initialization_status() -> EAX (1 if initialized)
;==========================================================================
PUBLIC main_get_initialization_status
ALIGN 16
main_get_initialization_status PROC
    mov eax, [g_app_state.initialized]
    ret
main_get_initialization_status ENDP

;==========================================================================
; main_post_message(message: ECX, wparam: EDX, lparam: R8) -> EAX (1=success)
;==========================================================================
PUBLIC main_post_message
ALIGN 16
main_post_message PROC
    ; Simple message posting to main window
    ; rcx = message, edx = wParam, r8 = lParam
    
    push rbx
    sub rsp, 32

    mov rax, [g_app_state.hMainWindow]
    test rax, rax
    jz post_msg_fail

    mov rbx, rax        ; Save hwnd
    mov r9, r8          ; lParam
    mov r8d, edx        ; wParam
    mov edx, ecx        ; message
    mov rcx, rbx        ; hwnd

    call SendMessageA
    
    mov eax, 1
    jmp post_msg_done

post_msg_fail:
    xor eax, eax

post_msg_done:
    add rsp, 32
    pop rbx
    ret

main_post_message ENDP

;==========================================================================
; WndProc - Main window procedure (stub for now)
;==========================================================================
ALIGN 16
WndProc PROC hwnd:QWORD, uMsg:DWORD, wParam:QWORD, lParam:QWORD

    ; Minimal window procedure - delegates to specific handlers
    cmp edx, WM_DESTROY
    je wnd_destroy
    cmp edx, WM_PAINT
    je wnd_paint
    cmp edx, WM_SIZE
    je wnd_size

    ; Default behavior
    mov rcx, hwnd
    mov edx, uMsg
    mov r8, wParam
    mov r9, lParam
    call DefWindowProcA
    ret

wnd_destroy:
    call PostQuitMessage
    xor eax, eax
    ret

wnd_paint:
    xor eax, eax
    ret

wnd_size:
    xor eax, eax
    ret

WndProc ENDP

;==========================================================================
; main_run_event_loop() - Main message loop
;==========================================================================
PUBLIC main_run_event_loop
ALIGN 16
main_run_event_loop PROC

    push rbx
    sub rsp, 64         ; Room for MSG struct (56 bytes + alignment)

    lea rbx, [rsp]      ; Point to MSG on stack

loop_start:
    ; GetMessageA(&msg, NULL, 0, 0)
    mov rcx, rbx
    xor edx, edx        ; hwnd = NULL
    xor r8d, r8d        ; wMsgFilterMin = 0
    xor r9d, r9d        ; wMsgFilterMax = 0
    call GetMessageA
    
    test eax, eax
    jle loop_end         ; WM_QUIT or error

    ; Increment message counter
    inc QWORD PTR [g_app_state.message_count]

    ; TranslateMessage(&msg)
    mov rcx, rbx
    call TranslateMessage

    ; DispatchMessageA(&msg)
    mov rcx, rbx
    call DispatchMessageA

    jmp loop_start

loop_end:
    add rsp, 64
    pop rbx
    ret

main_run_event_loop ENDP

END
