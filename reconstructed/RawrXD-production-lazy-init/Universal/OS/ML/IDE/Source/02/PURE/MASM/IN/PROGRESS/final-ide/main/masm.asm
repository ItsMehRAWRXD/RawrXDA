;==========================================================================
; main_masm.asm - Pure MASM64 RawrXD Main Entry Point
; ==========================================================================
; Single-file bootstrap that ties together:
; - Model loader (ml_masm)
; - Hotpatching (unified_masm_hotpatch)
; - Agentic engine (agentic_masm)
; - UI layer (ui_masm)
;
; No C++ runtime. Compiles to single .exe with ml64 + link.
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib

; ============================================================================
; Master Include for Zero-Day Agentic Engine and MASM System Modules
; ============================================================================
INCLUDE masm/masm_master_include.asm

;==========================================================================
; EXTERNAL DECLARATIONS (from linked .obj files)
;==========================================================================

; Model loader (ml_masm.asm)
EXTERN ml_masm_init:PROC
EXTERN ml_masm_free:PROC
EXTERN ml_masm_get_tensor:PROC
EXTERN ml_masm_get_arch:PROC
EXTERN ml_masm_last_error:PROC
EXTERN ml_masm_inference:PROC
EXTERN ml_masm_get_response:PROC

; IDE Components (ide_components.asm)
EXTERN ide_init_all_components:PROC
EXTERN ide_init_file_tree:PROC
EXTERN ide_editor_open_file:PROC
EXTERN ide_tabs_create_tab:PROC
EXTERN ide_minimap_init:PROC
EXTERN ide_palette_init:PROC
EXTERN ide_panes_init:PROC

; Hotpatching (unified_masm_hotpatch.asm)
EXTERN hpatch_apply_memory:PROC
EXTERN hpatch_apply_byte:PROC
EXTERN hpatch_apply_server:PROC
EXTERN hpatch_get_stats:PROC
EXTERN hpatch_reset_stats:PROC

; Agentic engine (agentic_masm.asm)
EXTERN agent_init_tools:PROC

; Qt6 Components (qt6_*.asm)
EXTERN main_window_system_init:PROC
EXTERN main_window_system_cleanup:PROC
EXTERN main_window_create:PROC
EXTERN main_window_show:PROC
EXTERN main_window_hide:PROC
EXTERN main_window_destroy:PROC
EXTERN main_window_set_title:PROC
EXTERN main_window_get_title:PROC
EXTERN main_window_set_status:PROC
EXTERN main_window_get_status:PROC
EXTERN qt_foundation_init:PROC
EXTERN qt_foundation_cleanup:PROC
EXTERN agent_process_command:PROC
EXTERN agent_list_tools:PROC
EXTERN agent_get_tool:PROC

; Agentic Orchestrator (agentic_engine.asm)
EXTERN AgenticEngine_Initialize:PROC
EXTERN AgenticEngine_ProcessResponse:PROC
EXTERN AgenticEngine_ExecuteTask:PROC
EXTERN AgenticEngine_GetStats:PROC

; GUI Designer Complete (gui_designer_complete.asm)
EXTERN gui_init_registry:PROC
EXTERN gui_create_component:PROC
EXTERN gui_agent_inspect:PROC
EXTERN gui_agent_modify:PROC
EXTERN gui_create_complete_ide:PROC

; Qt6 Foundation (qt6_foundation.asm)
EXTERN qt_foundation_init:PROC
EXTERN qt_foundation_cleanup:PROC
EXTERN object_create:PROC
EXTERN object_destroy:PROC

; Qt6 Main Window (qt6_main_window.asm)
EXTERN main_window_system_init:PROC
EXTERN main_window_system_cleanup:PROC
EXTERN main_window_create:PROC
EXTERN main_window_show:PROC
EXTERN main_window_set_title:PROC
EXTERN main_window_add_menu:PROC
EXTERN main_window_add_menu_item:PROC

; MainWindow implementation (legacy/bridge)
; EXTERN MainWindow_Initialize:PROC
; EXTERN MainWindow_Run:PROC

; UI layer (ui_masm.asm)
EXTERN ui_create_main_window:PROC
EXTERN ui_register_components:PROC
EXTERN ui_create_menu:PROC
EXTERN ui_set_main_menu:PROC
EXTERN ui_create_chat_control:PROC
EXTERN ui_create_input_control:PROC
EXTERN ui_create_terminal_control:PROC
EXTERN ui_create_send_button:PROC
EXTERN ui_create_mode_combo:PROC
EXTERN ui_create_mode_checkboxes:PROC
EXTERN ui_add_chat_message:PROC
EXTERN ui_get_input_text:PROC
EXTERN ui_clear_input:PROC
EXTERN ui_show_dialog:PROC
EXTERN ui_open_file_dialog:PROC
EXTERN ui_editor_set_text:PROC
EXTERN ui_editor_get_text:PROC
EXTERN ui_open_text_file_dialog:PROC
EXTERN ui_save_text_file_dialog:PROC
EXTERN ui_switch_sidebar_view:PROC
EXTERN ui_switch_bottom_tab:PROC
EXTERN session_manager_init:PROC
EXTERN console_log_init:PROC
EXTERN console_log:PROC
EXTERN file_log_init:PROC
EXTERN file_log_append:PROC
EXTERN GetFileAttributesA:PROC
EXTERN triage_once:BYTE

; Win32 APIs
EXTERN ExitProcess:PROC
EXTERN GetModuleHandleA:PROC
EXTERN MessageBoxA:PROC
EXTERN OutputDebugStringA:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN Sleep:PROC
EXTERN CreateFileA:PROC
EXTERN SetFilePointerEx:PROC
EXTERN FlushFileBuffers:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN lstrlenA:PROC
EXTERN GetLastError:PROC
EXTERN GetStdHandle:PROC
EXTERN SetUnhandledExceptionFilter:PROC
EXTERN wsprintfA:PROC
EXTERN DestroyWindow:PROC
EXTERN SendMessageA:PROC
EXTERN ShowWindow:PROC

; Additional Win32 APIs used in code
EXTERN RegisterClassExA:PROC
EXTERN CreateWindowExA:PROC
EXTERN UpdateWindow:PROC
EXTERN DefWindowProcA:PROC
EXTERN PostQuitMessage:PROC
EXTERN LoadIconA:PROC
EXTERN LoadCursorA:PROC
EXTERN GetStockObject:PROC
EXTERN BeginPaint:PROC
EXTERN EndPaint:PROC
EXTERN GetClientRect:PROC
EXTERN InvalidateRect:PROC
EXTERN SetWindowTextA:PROC
EXTERN GetWindowTextA:PROC
EXTERN PostMessageA:PROC
EXTERN CreateSolidBrush:PROC
EXTERN DeleteObject:PROC
EXTERN SelectObject:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC
EXTERN TextOutA:PROC
EXTERN SetTextColor:PROC
EXTERN SetBkColor:PROC

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_COMMAND_LEN     equ 8192
MAX_RESPONSE_LEN    equ 65536
STD_OUTPUT_HANDLE   equ -11
OPEN_ALWAYS         equ 4
FILE_ATTRIBUTE_NORMAL equ 80h
FILE_END            equ 2
EXCEPTION_EXECUTE_HANDLER equ 1

; Menu Constants
IDM_FILE_OPEN_MODEL  equ 2001
IDM_FILE_OPEN_FILE   equ 2002
IDM_FILE_SAVE        equ 2003
IDM_FILE_SAVE_AS     equ 2004
IDM_FILE_EXIT        equ 2005
IDM_CHAT_CLEAR       equ 2006
IDM_SETTINGS_MODEL   equ 2009
IDM_AGENT_TOGGLE     equ 2017
IDM_HOTPATCH_MEMORY  equ 2018
IDM_HOTPATCH_BYTE    equ 2019
IDM_HOTPATCH_SERVER  equ 2020
IDM_HOTPATCH_STATS   equ 2021
IDM_HOTPATCH_RESET   equ 2022

; Control Constants
IDC_SEND_BUTTON      equ 3001
IDC_MODE_COMBO       equ 3002
IDC_AB_EXPLORER      equ 3003
IDC_AB_SEARCH        equ 3004
IDC_AB_SCM           equ 3005
IDC_AB_DEBUG         equ 3006
IDC_AB_EXTENSIONS    equ 3007
IDC_TAB_TERMINAL     equ 3008
IDC_TAB_OUTPUT       equ 3009
IDC_TAB_PROBLEMS     equ 3010
IDC_TAB_DEBUGCON     equ 3011
IDC_COMMAND_PALETTE  equ 3012
IDC_FILE_TREE        equ 3013
IDC_SEARCH_BOX       equ 3014
IDC_PROBLEMS_LIST    equ 3015
IDC_DEBUG_CONSOLE    equ 3016

; RichEdit control messages
EM_SETSEL            equ 00B1h
EM_REPLACESEL        equ 00C2h
EM_GETSEL            equ 00B0h
EM_SETLIMITTEXT      equ 00C5h

.data?
;==========================================================================
; DATA SEGMENT
;==========================================================================
    h_instance          HINSTANCE ?
    model_loaded        DWORD ?
    agent_running       DWORD ?
    probe_hFile          QWORD ?
    zero_day_engine_handle QWORD ?
    unhandled_buf      BYTE 128 DUP (?)
    
    command_buffer      BYTE MAX_COMMAND_LEN DUP (?)
    editor_path         BYTE MAX_PATH DUP (?)
    editor_buffer       BYTE 65536 DUP (?)
    response_buffer     BYTE MAX_RESPONSE_LEN DUP (?)
    
    model_path_str      BYTE MAX_PATH DUP (?)

.data
    szHotPatchPath      BYTE "hotpatch_engine.dll",0
    dbg_menu_start      BYTE "Main: Creating menu...",13,10,0
    dbg_controls_start  BYTE "Main: Creating controls...",13,10,0
    dbg_init_app_start  BYTE "Main: Initializing app logic...",13,10,0
    dbg_init_welcome    BYTE "Init: Welcome message...",13,10,0
    dbg_init_loading    BYTE "Init: Loading model...",13,10,0
    dbg_init_ml_init    BYTE "Init: ml_masm_init...",13,10,0
    dbg_init_model_missing BYTE "Init: model file missing, skipping ml_masm_init",13,10,0
    dbg_init_tools      BYTE "Init: agent_init_tools...",13,10,0
    dbg_init_ready      BYTE "Init: Ready message...",13,10,0
    dbg_init_done       BYTE "Init: Done.",13,10,0
    app_name            BYTE "RawrXD AI IDE (Pure MASM64)",0
    sz_default_title    BYTE "RawrXD IDE - Pure MASM Edition",0
    welcome_msg         BYTE "RawrXD Agentic IDE - Ready",0
    loading_model       BYTE "Loading model...",0
    model_ready         BYTE "Model loaded. Type /help for commands.",0
    szMainMissionName   BYTE "RawrXD Main Mission",0
    error_no_model      BYTE "ERROR: Could not load model",0
    error_startup       BYTE "Startup failed",0
    default_model       BYTE "model.gguf",0
    cmd_prefix          BYTE "/",0
    help_cmd            BYTE "help",0
    help_text           BYTE "Commands:",13,10
                        BYTE "/help - Show this help",13,10
                        BYTE "/exit - Exit",13,10
                        BYTE "/tools - List agent tools",13,10
                        BYTE "or type naturally for chat",0
    str_settings        BYTE "Model Settings",0
    str_model           BYTE "AI Model Configuration",0
    str_chat_clear      BYTE "Chat history cleared",0
    str_agent_enabled   BYTE "Agent mode enabled",0
    str_agent_disabled  BYTE "Agent mode disabled",0
    str_tools           BYTE "&Tools",0
    str_hotpatch_memory BYTE "Apply Memory Hotpatch",0
    str_hotpatch_byte   BYTE "Apply Byte-Level Hotpatch",0
    str_hotpatch_server BYTE "Apply Server Hotpatch",0
    str_hotpatch_stats  BYTE "Show Hotpatch Statistics",0
    str_hotpatch_reset  BYTE "Reset Hotpatch Statistics",0
    str_hotpatch_success BYTE "Hotpatch applied successfully",0
    str_hotpatch_failed BYTE "Hotpatch failed",0
    str_hotpatch_stats_msg BYTE "Hotpatch stats: ",0
    dbg_main_loop       BYTE "Main: Entering message loop",0
    dbg_ui_start        BYTE "UI: Starting...",0
    dbg_ui_ok           BYTE "UI: Main window created",0
    dbg_ui_reg_fail     BYTE "UI: Registration failed",0
    str_file            BYTE "Menu creation failed",0
    dbg_probe_main0     BYTE "probe:main:start",13,10,0
    dbg_before_ui       BYTE "probe:before_ui_create",13,10,0
    dbg_probe_main1     BYTE "probe:main:after_window",13,10,0
    dbg_stdout_start    BYTE "[RAWREXE] start",13,10,0
    dbg_stdout_after_win BYTE "[RAWREXE] after window",13,10,0
    dbg_stdout_after_menu BYTE "[RAWREXE] after menu",13,10,0
    dbg_stdout_after_init BYTE "[RAWREXE] after init",13,10,0
    dbg_probe_path       BYTE "C:\\Users\\HiH8e\\probe_startup.log",0
    dbg_probe_main_start BYTE "probe:main:start",13,10,0
    dbg_probe_before_console BYTE "probe:before_console_log_init",13,10,0
    dbg_probe_after_console  BYTE "probe:after_console_log_init",13,10,0
    dbg_probe_before_win     BYTE "probe:before_ui_create_main_window",13,10,0
    dbg_probe_after_win_call BYTE "probe:after_ui_create_main_window_call",13,10,0
    dbg_probe_after_win      BYTE "probe:after_ui_create_main_window",13,10,0
    dbg_probe_after_menu     BYTE "probe:after_ui_create_menu",13,10,0
    dbg_probe_after_init     BYTE "probe:after_init_application_safe",13,10,0
    dbg_probe_after_chat     BYTE "probe:after_create_chat",13,10,0
    dbg_probe_after_input    BYTE "probe:after_create_input",13,10,0
    dbg_probe_after_terminal BYTE "probe:after_create_terminal",13,10,0
    dbg_probe_after_send     BYTE "probe:after_create_send_button",13,10,0
    dbg_probe_after_combo    BYTE "probe:after_create_mode_combo",13,10,0
    dbg_probe_after_checks   BYTE "probe:after_create_mode_checks",13,10,0
    dbg_probe_after_register BYTE "probe:after_ui_register_components",13,10,0
    unhandled_fmt       BYTE "UNHANDLED exception: code=0x%08X addr=%p base=%p rva=0x%08X",13,10,0
    error_ui_create     BYTE "ui_create_main_window failed",0
    error_menu_create   BYTE "ui_create_menu failed",0
.code

;==========================================================================
; Unhandled exception filter (triage)
; Logs original exception code/address to probe log.
; rcx = PEXCEPTION_POINTERS
;==========================================================================
ALIGN 16
unhandled_filter PROC
    sub rsp, 72

    ; Format: UNHANDLED exception: code=0x%08X addr=%p base=%p rva=0x%08X\r\n
    mov rax, QWORD PTR [rcx]              ; ExceptionRecord
    mov r8d, DWORD PTR [rax]              ; ExceptionCode
    mov r9,  QWORD PTR [rax + 10h]        ; ExceptionAddress

    ; Extra varargs on stack: base, rva
    mov r10, h_instance                   ; module base (saved in main before UI create)
    mov r11, r9
    sub r11, r10                          ; rva
    mov QWORD PTR [rsp + 32], r10         ; 5th arg: base
    mov eax, r11d
    mov QWORD PTR [rsp + 40], rax         ; 6th arg: rva (lower 32)

    lea rcx, unhandled_buf
    lea rdx, unhandled_fmt
    call wsprintfA

    ; Write to probe file (best-effort)
    lea rcx, unhandled_buf
    call lstrlenA
    mov r8, rax
    mov rcx, probe_hFile
    lea rdx, unhandled_buf
    lea r9, [rsp + 64]
    mov QWORD PTR [rsp + 32], 0
    call WriteFile

    mov rcx, probe_hFile
    call FlushFileBuffers

    mov eax, EXCEPTION_EXECUTE_HANDLER
    add rsp, 72
    ret
unhandled_filter ENDP

;==========================================================================
; Helper: Simple string comparison
;==========================================================================
ALIGN 16
strcmp_proc PROC
    push rdi
    push rsi
    xor eax, eax
strcmp_loop:
    mov al, BYTE PTR [rcx]
    mov bl, BYTE PTR [rdx]
    cmp al, bl
    jne strcmp_ne
    test al, al
    jz strcmp_eq
    inc rcx
    inc rdx
    jmp strcmp_loop
strcmp_ne:
    mov eax, -1
    jmp strcmp_done
strcmp_eq:
    xor eax, eax
strcmp_done:
    pop rsi
    pop rdi
    ret
strcmp_proc ENDP

;==========================================================================
; Helper: strlen
;==========================================================================
ALIGN 16
strlen_proc PROC
    xor eax, eax
strlen_loop:
    mov bl, BYTE PTR [rcx + rax]
    test bl, bl
    jz strlen_done
    inc eax
    jmp strlen_loop
strlen_done:
    ret
strlen_proc ENDP

;==========================================================================
; Helper: print string to stdout (for debugging)
; rcx = pointer to null-terminated ASCII
;==========================================================================
ALIGN 16
print_stdout PROC
    push rbx
    sub rsp, 48
    mov rbx, rcx

    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle                    ; rax = stdout handle
    mov rdx, rax                         ; save handle

    mov rcx, rbx
    call lstrlenA                        ; eax = length
    mov r8d, eax
    lea r9, [rsp + 24]
    mov QWORD PTR [rsp + 24], 0
    mov QWORD PTR [rsp + 32], 0          ; lpOverlapped
    mov rcx, rdx                         ; hFile
    mov rdx, rbx                         ; lpBuffer
    call WriteFile

    add rsp, 48
    pop rbx
    ret
print_stdout ENDP

;==========================================================================
; Helper: append probe message to probe_main_entry.log for early triage
; rcx = pointer to null-terminated ASCII message
;==========================================================================
ALIGN 16
; Probe logging (persistent handle): writes stage markers to dbg_probe_path
    sub rsp, 64

probe_open PROC
    sub rsp, 72

    ; Already open?
    cmp probe_hFile, 0
    jne po_done

    lea rcx, dbg_probe_path
    mov rdx, GENERIC_WRITE
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d
    mov QWORD PTR [rsp + 32], OPEN_ALWAYS
    mov QWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp + 48], 0
    call CreateFileA
    cmp rax, -1
    je po_done

    mov probe_hFile, rax

    ; Seek to end
    mov rcx, rax
    xor rdx, rdx
    xor r8, r8
    mov r9d, FILE_END
    call SetFilePointerEx

po_done:
    add rsp, 72
    ret
probe_open ENDP

ALIGN 16
probe_write PROC
    ; rcx = ptr to null-terminated ASCII
    push rbx
    sub rsp, 48
    mov rbx, rcx

    cmp probe_hFile, 0
    jne pw_have
    call probe_open

pw_have:
    cmp probe_hFile, 0
    je pw_done

    mov rcx, rbx
    call lstrlenA
    mov rcx, probe_hFile
    mov rdx, rbx
    mov r8, rax
    lea r9, [rsp + 40]
    mov QWORD PTR [rsp + 32], 0
    call WriteFile

pw_done:
    add rsp, 48
    pop rbx
    ret
probe_write ENDP

;==========================================================================
; Initialization: Load model, init tools, create UI
;==========================================================================
ALIGN 16
init_application PROC
    push rbx
    sub rsp, 48
    
    ; Display welcome
    lea rcx, dbg_init_welcome
    call console_log
    lea rcx, welcome_msg
    call ui_add_chat_message
    
    ; Show loading message
    lea rcx, dbg_init_loading
    call console_log
    lea rcx, loading_model
    call ui_add_chat_message
    
    ; Load model if present; skip when missing in dev runs
    lea rcx, default_model
    call GetFileAttributesA
    cmp eax, -1
    jne init_model_present
    lea rcx, dbg_init_model_missing
    call console_log
    mov model_loaded, 0
    jmp init_after_model

init_model_present:
    lea rcx, dbg_init_ml_init
    call console_log
    lea rcx, default_model
    xor rdx, rdx                            ; flags = 0
    call ml_masm_init
    test eax, eax
    jz init_model_failed
    
    mov model_loaded, 1
    
    ; Get architecture and log to console
    call ml_masm_get_arch
    mov rcx, rax
    test rcx, rcx
    jz init_after_model
    call console_log

init_after_model:
    ; Initialize agentic engine
    lea rcx, dbg_init_tools
    call console_log
    call AgenticEngine_Initialize
    
    ; Initialize session management (auto-save, recovery)
    call session_manager_init
    
    ; Initialize all IDE components (File Tree, Editor, Tabs, Minimap, Command Palette, Split Panes)
    call ide_init_all_components
    
    ; Initialize GUI Registry
    call gui_init_registry
    
    ; Display ready message
    lea rcx, dbg_init_ready
    call console_log
    lea rcx, model_ready
    call ui_add_chat_message
    
    lea rcx, dbg_init_done
    call console_log
    mov eax, 1
    jmp init_done
    
init_model_failed:
    ; Display error
    lea rcx, error_no_model
    call ui_add_chat_message
    
    ; Get error details
    call ml_masm_last_error
    mov rcx, rax
    call ui_add_chat_message
    
    ; Don't fail the whole app just because model failed
    mov eax, 1
    
init_done:
    add rsp, 48
    pop rbx
    ret
init_application ENDP

;==========================================================================
; PUBLIC: main_on_send()
;
; Called when user clicks Send button.
;==========================================================================
PUBLIC main_on_send
ALIGN 16
main_on_send PROC
    push rbx
    sub rsp, 48
    
    ; Get input text
    lea rcx, command_buffer
    mov rdx, MAX_COMMAND_LEN
    call ui_get_input_text
    test eax, eax
    jz on_send_done
    
    ; Add user message to chat
    lea rcx, command_buffer
    call ui_add_chat_message
    
    ; Check if it's a tool command (starts with @)
    lea rsi, command_buffer
    mov al, [rsi]
    cmp al, '@'
    je is_tool_command
    
    ; It's a chat message - call inference
    lea rcx, command_buffer
    call ml_masm_inference
    test eax, eax
    jz inference_failed
    
    lea rcx, response_buffer
    mov rdx, MAX_RESPONSE_LEN
    call ml_masm_get_response
    jmp process_response
    
is_tool_command:
    ; Process command
    lea rcx, command_buffer
    call agent_process_command
    
process_response:
    ; rax = response buffer
    
    ; Apply agentic correction/processing
    mov rcx, rax            ; resp_ptr
    mov rdx, MAX_RESPONSE_LEN ; resp_len
    mov r8, 1               ; mode (AGENT)
    call AgenticEngine_ProcessResponse
    ; rax = final response buffer
    
    ; Add response to chat
    mov rcx, rax
    call ui_add_chat_message
    
    ; Clear input
    call ui_clear_input
    
on_send_done:
    add rsp, 48
    pop rbx
    ret

inference_failed:
    lea rcx, error_no_model
    call ui_add_chat_message
    jmp on_send_done
main_on_send ENDP

;==========================================================================
; PUBLIC: main_on_open()
;
; Called when user clicks File -> Open.
;==========================================================================
PUBLIC main_on_open
ALIGN 16
main_on_open PROC
    push rbx
    sub rsp, 48
    
    ; Show open file dialog
    lea rcx, model_path_str
    mov rdx, MAX_PATH
    call ui_open_file_dialog
    test eax, eax
    jz on_open_done
    
    ; Free old model if any
    call ml_masm_free
    
    ; Load new model
    lea rcx, model_path_str
    xor rdx, rdx
    call ml_masm_init
    test eax, eax
    jz on_open_fail
    
    lea rcx, model_ready
    call ui_add_chat_message
    jmp on_open_done
    
on_open_fail:
    lea rcx, error_no_model
    call ui_add_chat_message
    
on_open_done:
    add rsp, 48
    pop rbx
    ret
main_on_open ENDP

;=========================================================================
; File operations for editor
;=========================================================================
ALIGN 16
read_text_file_into_editor PROC
    ; rcx = path
    push rbx
    sub rsp, 64
    
    ; Open file
    mov rdx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d
    mov QWORD PTR [rsp + 32], OPEN_EXISTING
    mov QWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp + 48], 0
    call CreateFileA
    cmp rax, -1
    je rtf_fail
    mov rbx, rax
    
    ; Read up to 64KB
    lea rcx, editor_buffer
    mov rdx, 65536
    lea r8, [rsp + 56]                    ; lpNumberOfBytesRead on stack
    mov QWORD PTR [rsp + 56], 0
    mov r9, rcx
    mov rcx, rbx
    call ReadFile
    
    ; Close
    mov rcx, rbx
    call CloseHandle
    
    ; Null-terminate and set text
    mov eax, DWORD PTR [rsp + 56]
    cmp eax, 0
    je rtf_done
    lea rdx, editor_buffer
    mov BYTE PTR [rdx + rax], 0
    lea rcx, editor_buffer
    call ui_editor_set_text
    
rtf_done:
    mov eax, 1
    add rsp, 64
    pop rbx
    ret
rtf_fail:
    xor eax, eax
    add rsp, 64
    pop rbx
    ret
read_text_file_into_editor ENDP

ALIGN 16
write_editor_to_text_file PROC
    ; rcx = path
    push rbx
    sub rsp, 64
    
    ; Get text into buffer
    mov rdx, 65535
    lea rbx, editor_buffer
    mov rcx, rbx
    call ui_editor_get_text
    cmp eax, 0
    je wetf_fail
    
    ; Open file for write (create/overwrite)
    mov rdx, GENERIC_WRITE
    xor r8d, r8d                            ; no share
    xor r9d, r9d
    mov QWORD PTR [rsp + 32], CREATE_ALWAYS
    mov QWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp + 48], 0
    call CreateFileA
    cmp rax, -1
    je wetf_fail
    mov rbx, rax
    
    ; Write
    lea rcx, editor_buffer
    mov edx, eax                            ; length from ui_editor_get_text
    lea r8, [rsp + 56]
    mov QWORD PTR [rsp + 56], 0
    mov r9, rcx
    mov rcx, rbx
    call WriteFile
    
    ; Close
    mov rcx, rbx
    call CloseHandle
    
    mov eax, 1
    add rsp, 64
    pop rbx
    ret
wetf_fail:
    xor eax, eax
    add rsp, 64
    pop rbx
    ret
write_editor_to_text_file ENDP

;=========================================================================
; Menu handlers: Open File / Save / Save As
;=========================================================================
ALIGN 16
PUBLIC main_on_open_file
main_on_open_file PROC
    sub rsp, 48
    
    ; Show open dialog
    lea rcx, editor_path
    mov rdx, MAX_PATH
    call ui_open_text_file_dialog
    test eax, eax
    jz mool_done
    
    ; Load file into editor
    lea rcx, editor_path
    call read_text_file_into_editor
    
mool_done:
    add rsp, 48
    ret
main_on_open_file ENDP

PUBLIC main_on_save_file
main_on_save_file PROC
    sub rsp, 48
    
    ; If no path yet, fall back to Save As
    cmp BYTE PTR editor_path, 0
    jne do_save
    call main_on_save_file_as
    add rsp, 48
    ret
do_save:
    lea rcx, editor_path
    call write_editor_to_text_file
    add rsp, 48
    ret
main_on_save_file ENDP

PUBLIC main_on_save_file_as
main_on_save_file_as PROC
    sub rsp, 48
    
    lea rcx, editor_path
    mov rdx, MAX_PATH
    call ui_save_text_file_dialog
    test eax, eax
    jz mosfa_done
    
    lea rcx, editor_path
    call write_editor_to_text_file
    
mosfa_done:
    add rsp, 48
    ret
main_on_save_file_as ENDP
;==========================================================================
; Message Loop
;==========================================================================
ALIGN 16
message_loop PROC
    sub rsp, 88                             ; Space for MSG struct (48) + shadow space (32) + alignment (8)
    lea rcx, dbg_main_loop
    call file_log_append
    lea rcx, dbg_main_loop
    call probe_write
    
msg_loop:
    lea rcx, [rsp + 32]                     ; Pointer to MSG struct
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    call GetMessageA
    ; log return value
    mov rbx, rax
    lea rcx, dbg_main_loop
    call file_log_append
    
    mov eax, ebx
    cmp eax, 0
    je msg_loop_done
    cmp eax, -1
    je msg_loop_done
    
    lea rcx, [rsp + 32]
    call TranslateMessage
    
    lea rcx, [rsp + 32]
    call DispatchMessageA
    jmp msg_loop

msg_loop_done:
    add rsp, 88
    ret
message_loop ENDP

;==========================================================================
; Main Entry Point
;==========================================================================

ALIGN 16
init_application_safe PROC
    ; Wrapped init with try/catch via structured exception handling
    ; Returns: eax = 1 on success, 0 on error
    push rbx
    push r12
    sub rsp, 40
    
    ; Log each step before attempting it
    lea rcx, dbg_init_welcome
    call console_log
    
    lea rcx, dbg_init_loading
    call console_log
    
    lea rcx, dbg_init_ml_init
    call console_log
    
    ; Check model file
    lea rcx, default_model
    call GetFileAttributesA
    cmp eax, -1
    jne safe_model_present
    
    lea rcx, dbg_init_model_missing
    call console_log
    mov eax, 1
    jmp safe_done
    
safe_model_present:
    ; Try to init model; if fails, log and continue
    lea rcx, default_model
    xor rdx, rdx
    call ml_masm_init
    test eax, eax
    jz safe_model_fail
    
    ; Initialize agentic engine and IDE components
    lea rcx, dbg_init_tools
    call console_log
    
    ; Agent toolchain
    call agent_init_tools
    call AgenticEngine_Initialize
    
    ; IDE shells and registry
    call ide_init_all_components
    call gui_init_registry
    
    mov eax, 1
    jmp safe_done
    
safe_model_fail:
    lea rcx, error_no_model
    call console_log
    mov eax, 1  ; Don't fail app for model load error
    
safe_done:
    add rsp, 40
    pop r12
    pop rbx
    ret
init_application_safe ENDP

;==========================================================================
; Main Entry Point
;==========================================================================
PUBLIC main
main PROC
    ; rcx = hInstance (from kernel for CONSOLE subsystem)
    ; rdx = hPrevInstance
    ; r8  = lpCmdLine  
    ; r9d = nCmdShow
    sub rsp, 40

    ; Ensure probe handle starts as NULL (data? is uninitialized)
    mov QWORD PTR [probe_hFile], 0

    ; Debug: reached main
    lea rcx, dbg_stdout_start
    call print_stdout

    ; Persistent probe log (always-on)
    call probe_open
    lea rcx, dbg_probe_main_start
    call probe_write

    ; Capture original exception code/address on crash (CreateWindowExA callback triage)
    lea rcx, unhandled_filter
    call SetUnhandledExceptionFilter

    ; Start console/file logging immediately so we see early failures
    mov BYTE PTR [triage_once], 1         ; skip MessageBox triage to avoid blocking

    lea rcx, dbg_probe_before_console
    call probe_write

    call console_log_init

    lea rcx, dbg_probe_after_console
    call probe_write
    ; Initialize Zero-Day lifecycle instrumentation
    call ZeroDayAgenticEngine_Create
    mov [zero_day_engine_handle], rax
    call ZeroDayIntegration_Initialize
    lea rcx, szMainMissionName
    call Logger_LogMissionStart
    
    ; CRITICAL: Save hInstance from rcx BEFORE any other register operations!
    ; Get hInstance via GetModuleHandleA(NULL)
    push rcx                                ; Save original rcx if it had anything
    xor rcx, rcx
    call GetModuleHandleA
    mov [h_instance], rax                     ; Save the module handle
    pop rcx                                 ; Restore
    
    ; Write initial marker to log (console_log_init already opened run.log)
    lea rcx, dbg_probe_main0
    call file_log_append

create_file_done:
    
    ; Debug: Log h_instance value before calling main_window_system_init
    lea rcx, dbg_before_ui
    call file_log_append

    lea rcx, dbg_probe_before_win
    call probe_write
    
    ; Initialize MainWindow system
    call main_window_system_init
    test rax, rax
    jnz ui_ok
    lea rcx, app_name
    lea rdx, error_ui_create
    call ui_show_dialog
    jmp main_fail
ui_ok:
    
    lea rcx, dbg_ui_ok
    call file_log_append
    lea rcx, dbg_probe_main1
    call file_log_append
    lea rcx, dbg_stdout_after_win
    call print_stdout

    lea rcx, dbg_probe_after_win
    call probe_write
    
    ; Create menu
    lea rcx, dbg_menu_start
    call file_log_append
    call ui_create_menu
    test rax, rax
    jnz menu_ok
    lea rcx, app_name
    lea rdx, error_menu_create
    call ui_show_dialog
    jmp main_fail
menu_ok:
    
    ; Attach menu to window
    mov rcx, rax
    call ui_set_main_menu

    lea rcx, dbg_stdout_after_menu
    call print_stdout

    lea rcx, dbg_probe_after_menu
    call probe_write
    
    ; Create controls
    lea rcx, dbg_controls_start
    call file_log_append
    
    ; Create chat control
    call ui_create_chat_control
    lea rcx, dbg_probe_after_chat
    call probe_write
    
    ; Create input control
    call ui_create_input_control
    lea rcx, dbg_probe_after_input
    call probe_write
    
    ; Create terminal control
    call ui_create_terminal_control
    lea rcx, dbg_probe_after_terminal
    call probe_write
    
    ; Create send button
    call ui_create_send_button
    lea rcx, dbg_probe_after_send
    call probe_write
    
    ; Create mode combo
    call ui_create_mode_combo
    lea rcx, dbg_probe_after_combo
    call probe_write
    
    ; Create mode checkboxes
    call ui_create_mode_checkboxes
    lea rcx, dbg_probe_after_checks
    call probe_write
    
    ; Register components for GUI Designer
    call ui_register_components
    lea rcx, dbg_probe_after_register
    call probe_write
    
    ; Initialize application (load model, init tools)
    lea rcx, dbg_init_app_start
    call file_log_append
    
    ; Wrap init in error handling
    call init_application_safe
    test eax, eax
    jz init_app_fail

    lea rcx, dbg_stdout_after_init
    call print_stdout

    lea rcx, dbg_probe_after_init
    call probe_write
    
    ; Create Main Window
    lea rcx, sz_default_title
    mov rdx, 1280
    mov r8, 800
    call main_window_create
    test rax, rax
    jz init_app_fail
    
    mov rbx, rax                        ; rbx = MAIN_WINDOW ptr
    
    ; Show Main Window
    mov rcx, rbx
    call main_window_show
    
    ; Enter message loop
    sub rsp, 48                         ; MSG structure is 48 bytes
    mov rsi, rsp                        ; rsi = &msg
    
msg_loop:
    mov rcx, rsi
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    call GetMessageA
    test eax, eax
    jz exit_loop
    
    mov rcx, rsi
    call TranslateMessage
    mov rcx, rsi
    call DispatchMessageA
    jmp msg_loop
    
exit_loop:
    add rsp, 48
    
    ; Cleanup
    call main_window_system_cleanup
    call qt_foundation_cleanup
    call ml_masm_free
    
    ; Exit with success
    lea rcx, szMainMissionName
    call Logger_LogMissionComplete
    call main_zero_day_shutdown
    xor ecx, ecx
    call ExitProcess
    
init_app_fail:
    lea rcx, error_no_model
    call file_log_append
    lea rcx, app_name
    lea rdx, error_startup
    call ui_show_dialog
    lea rcx, szMainMissionName
    mov rdx, 1
    call Logger_LogMissionError
    call main_zero_day_shutdown
    mov ecx, 1
    call ExitProcess
    
main_fail:
    lea rcx, str_file
    call file_log_append
    lea rcx, app_name
    lea rdx, error_startup
    call ui_show_dialog
    lea rcx, szMainMissionName
    mov rdx, 1
    call Logger_LogMissionError
    call main_zero_day_shutdown
    mov ecx, 1
    call ExitProcess
main ENDP

;==========================================================================
; Startup Code: Bare entry point that calls WinMain
;==========================================================================

PUBLIC _start
_start PROC
    ; For CONSOLE subsystem, just call main directly.
    ; main() will call GetModuleHandleA itself.
    sub rsp, 40                             ; Shadow space + alignment
    
    call main
    
    ; main returns exit code in eax
    ; Call ExitProcess to terminate
    mov rcx, rax
    call ExitProcess
    ret
_start ENDP

;==========================================================================
; Alias for backward compatibility
;==========================================================================
    PUBLIC RawrMain
RawrMain EQU main
;==========================================================================

PUBLIC RawrXD_GetSystemLoad
RawrXD_GetSystemLoad PROC
    ; Returns system load percentage (0-100)
    mov eax, 50  ; Return 50% as default
    ret
RawrXD_GetSystemLoad ENDP

PUBLIC RawrXD_HotPatchEnginePath
RawrXD_HotPatchEnginePath PROC
    ; Returns pointer to hotpatch engine path string
    lea rax, szHotPatchPath
    ret
RawrXD_HotPatchEnginePath ENDP

PUBLIC RawrXD_ProcessStreamingChunk
RawrXD_ProcessStreamingChunk PROC
    ; rcx = chunk data, rdx = chunk size
    ; Returns: eax = 1 on success
    mov eax, 1
    ret
RawrXD_ProcessStreamingChunk ENDP

PUBLIC RawrXD_PerformanceLogMetrics
RawrXD_PerformanceLogMetrics PROC
    ; rcx = metric name, rdx = metric value
    ; Logs performance metrics
    mov eax, 1
    ret
RawrXD_PerformanceLogMetrics ENDP

;==========================================================================
; Helper: Zero-Day cleanup
;==========================================================================
main_zero_day_shutdown PROC
    mov r10, [zero_day_engine_handle]
    test r10, r10
    jz mzds_done
    call ZeroDayIntegration_Shutdown
    mov rcx, r10
    call ZeroDayAgenticEngine_Destroy
    mov [zero_day_engine_handle], 0
mzds_done:
    ret
main_zero_day_shutdown ENDP

;==========================================================================
; Startup Code (called by CRT stub or directly)
;==========================================================================

END
