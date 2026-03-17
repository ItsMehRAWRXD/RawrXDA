;==============================================================================
; masm_gui_main.asm
; MASM Replacement for main_qt.cpp
; GUI Entry Point for Zero C++ Core Agentic IDE
;
; Replaces all Qt/C++ functionality with pure Windows API + MASM
; No QApplication, no C++ runtime, no Qt framework required
;==============================================================================

option casemap:none

include windows.inc
include winuser.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib advapi32.lib
includelib ws2_32.lib

; Import from unified bridge
EXTERN zero_cpp_bridge_initialize:PROC
EXTERN zero_cpp_bridge_process_wish:PROC
EXTERN zero_cpp_bridge_shutdown:PROC
EXTERN zero_cpp_bridge_register_callback:PROC

; UI Bridge (to remaining Qt)
EXTERN qt_window_create:PROC
EXTERN qt_panel_create:PROC
EXTERN qt_chat_display:PROC
EXTERN qt_status_update:PROC

; Constants
IDC_MAIN_WND            EQU 1000h
IDC_CHAT_PANEL          EQU 1001h
IDC_TERMINAL_PANEL      EQU 1002h
IDC_MODEL_SELECTOR      EQU 1003h

WM_CUSTOM_WISH          EQU WM_USER + 100
WM_CUSTOM_STATUS        EQU WM_USER + 101
WM_CUSTOM_EXECUTE       EQU WM_USER + 102

;==============================================================================
; DATA SECTION
;==============================================================================

.data

    ; Window Class & Title
    szWndClassName      DB "RawrXD-IDE-Window", 0
    szWndTitle          DB "RawrXD-QtShell Zero C++ Core - Agentic IDE", 0
    
    ; Status Messages
    szInitStatus        DB "GUI: Initializing zero C++ core interface...", 0
    szInitOk            DB "GUI: Interface initialization complete", 0
    szInitFailed        DB "GUI: Initialization failed. Error: 0x%X", 0
    szCreateWndFailed   DB "GUI: Failed to create main window", 0
    
    ; Application Info
    szAppName           DB "RawrXD-QtShell", 0
    szAppVersion        DB "1.0.0 - Zero C++ Core", 0
    
    ; Event Handling
    szWishReceived      DB "GUI: Wish received from user", 0
    szExecutionUpdate   DB "GUI: Execution progress: %d%%", 0
    szExecutionComplete DB "GUI: Execution complete", 0
    szExecutionError    DB "GUI: Execution error: 0x%X", 0
    
    ; Logging
    szLogPathPrefix     DB "C:\logs\rawrxd_", 0
    szLogPathSuffix     DB ".log", 0

.data?
    ; Window handles
    g_main_window       QWORD ?
    g_chat_panel        QWORD ?
    g_terminal_panel    QWORD ?
    g_model_selector    QWORD ?
    
    ; Instance handle
    g_instance          QWORD ?
    g_msg_queue         QWORD ?
    
    ; Logging
    g_log_file          QWORD ?
    g_log_initialized   DWORD 0
    
    ; Execution state
    g_current_wish      QWORD ?
    g_execution_active  DWORD 0
    g_gui_ready         DWORD 0

;==============================================================================
; PUBLIC EXPORTS
;==============================================================================

PUBLIC WinMain
PUBLIC MainWndProc

;==============================================================================
; MAIN ENTRY POINT - WinMain Signature
;==============================================================================

.code

ALIGN 16
WinMain PROC
    ; Standard Windows API: rcx = hInstance, rdx = hPrevInstance, 
    ; r8 = lpCmdLine, r9d = nCmdShow
    
    push rbx
    push r12
    sub rsp, 64
    
    mov g_instance, rcx
    
    ; =========================================================================
    ; PHASE 1: INITIALIZE LOGGING SYSTEM
    ; =========================================================================
    
    call initialize_logging
    
    ; =========================================================================
    ; PHASE 2: INITIALIZE ZERO C++ CORE BRIDGE
    ; =========================================================================
    
    lea rcx, szInitStatus
    call log_message
    
    mov ecx, 0          ; config_flags
    mov g_main_window, 0
    call zero_cpp_bridge_initialize
    
    test eax, eax
    jz .init_failed
    
    lea rcx, szInitOk
    call log_message
    
    mov g_gui_ready, 1
    
    ; =========================================================================
    ; PHASE 3: REGISTER WINDOW CLASS
    ; =========================================================================
    
    ; Create window class structure
    sub rsp, SIZEOF WNDCLASSA
    mov rbx, rsp
    
    mov DWORD PTR [rbx + WNDCLASSA.style], CS_VREDRAW or CS_HREDRAW
    lea rax, MainWndProc
    mov QWORD PTR [rbx + WNDCLASSA.lpfnWndProc], rax
    mov DWORD PTR [rbx + WNDCLASSA.cbClsExtra], 0
    mov DWORD PTR [rbx + WNDCLASSA.cbWndExtra], 0
    mov QWORD PTR [rbx + WNDCLASSA.hInstance], rcx
    
    ; Load icon and cursor
    mov ecx, 0
    call LoadCursorA
    mov QWORD PTR [rbx + WNDCLASSA.hCursor], rax
    
    mov ecx, COLOR_WINDOW + 1
    call GetSysColorBrush
    mov QWORD PTR [rbx + WNDCLASSA.hbrBackground], rax
    
    mov QWORD PTR [rbx + WNDCLASSA.lpszMenuName], 0
    lea rax, szWndClassName
    mov QWORD PTR [rbx + WNDCLASSA.lpszClassName], rax
    mov QWORD PTR [rbx + WNDCLASSA.hIconSm], 0
    
    ; Register class
    mov rcx, rbx
    call RegisterClassA
    
    test rax, rax
    jz .register_failed
    
    add rsp, SIZEOF WNDCLASSA
    
    ; =========================================================================
    ; PHASE 4: CREATE MAIN WINDOW
    ; =========================================================================
    
    lea rcx, szWndClassName
    lea rdx, szWndTitle
    mov r8, WS_OVERLAPPEDWINDOW
    mov r9d, CW_USEDEFAULT
    
    ; CreateWindowA parameters:
    ; rcx = lpClassName
    ; rdx = lpWindowName
    ; r8 = dwStyle
    ; r9d = x
    ; [rsp+40] = y
    ; [rsp+48] = nWidth
    ; [rsp+56] = nHeight
    ; [rsp+64] = hWndParent
    ; [rsp+72] = hMenu
    ; [rsp+80] = hInstance
    ; [rsp+88] = lpParam
    
    sub rsp, 48
    mov DWORD PTR [rsp + 40], CW_USEDEFAULT   ; y
    mov DWORD PTR [rsp + 48], 1200            ; nWidth
    mov DWORD PTR [rsp + 56], 800             ; nHeight
    mov QWORD PTR [rsp + 64], 0               ; hWndParent
    mov QWORD PTR [rsp + 72], 0               ; hMenu
    mov QWORD PTR [rsp + 80], g_instance
    mov QWORD PTR [rsp + 88], 0               ; lpParam
    
    call CreateWindowExA
    
    test rax, rax
    jz .wnd_create_failed
    
    mov g_main_window, rax
    add rsp, 48
    
    ; Register callback with bridge for notifications
    mov rcx, rax        ; window handle
    mov rdx, 0FFFFFFFFh ; all events
    call zero_cpp_bridge_register_callback
    
    ; =========================================================================
    ; PHASE 5: CREATE UI PANELS (via Qt bridge for now)
    ; =========================================================================
    
    ; Create chat panel
    mov rcx, g_main_window
    mov edx, IDC_CHAT_PANEL
    call qt_panel_create
    mov g_chat_panel, rax
    
    ; Create terminal panel
    mov rcx, g_main_window
    mov edx, IDC_TERMINAL_PANEL
    call qt_panel_create
    mov g_terminal_panel, rax
    
    ; Create model selector
    mov rcx, g_main_window
    mov edx, IDC_MODEL_SELECTOR
    call qt_panel_create
    mov g_model_selector, rax
    
    ; =========================================================================
    ; PHASE 6: SHOW WINDOW AND MESSAGE LOOP
    ; =========================================================================
    
    ; Show the main window
    mov rcx, g_main_window
    mov edx, r9d        ; nCmdShow parameter
    call ShowWindow
    
    ; Update window display
    mov rcx, g_main_window
    call UpdateWindow
    
    ; =========================================================================
    ; PHASE 7: MESSAGE LOOP
    ; =========================================================================
    
    sub rsp, SIZEOF MSG
    mov rbx, rsp
    
.msg_loop:
    ; GetMessageA(&msg, NULL, 0, 0)
    mov rcx, rbx        ; lpMsg
    mov rdx, 0          ; hWnd (NULL = all windows)
    mov r8d, 0          ; wMsgFilterMin
    mov r9d, 0          ; wMsgFilterMax
    call GetMessageA
    
    test eax, eax
    jle .msg_loop_exit  ; Exit on WM_QUIT or error
    
    ; TranslateMessage(&msg)
    mov rcx, rbx
    call TranslateMessage
    
    ; DispatchMessageA(&msg)
    mov rcx, rbx
    call DispatchMessageA
    
    jmp .msg_loop
    
.msg_loop_exit:
    add rsp, SIZEOF MSG
    
    ; =========================================================================
    ; PHASE 8: SHUTDOWN
    ; =========================================================================
    
    call zero_cpp_bridge_shutdown
    
    call cleanup_logging
    
    ; Return exit code (standard is 0 for success)
    xor eax, eax
    
    add rsp, 64
    pop r12
    pop rbx
    ret
    
.init_failed:
    lea rcx, szInitFailed
    call log_message
    mov eax, 1
    jmp .exit_cleanup
    
.register_failed:
    lea rcx, szCreateWndFailed
    call log_message
    add rsp, SIZEOF WNDCLASSA
    mov eax, 1
    jmp .exit_cleanup
    
.wnd_create_failed:
    lea rcx, szCreateWndFailed
    call log_message
    mov eax, 1
    jmp .exit_cleanup
    
.exit_cleanup:
    call cleanup_logging
    add rsp, 64
    pop r12
    pop rbx
    ret
ALIGN 16
WinMain ENDP

;==============================================================================
; WINDOW PROCEDURE (Message Handler)
;==============================================================================

ALIGN 16
MainWndProc PROC
    ; Standard Windows callback:
    ; rcx = hWnd, edx = uMsg, r8 = wParam, r9 = lParam
    
    push rbx
    push r12
    sub rsp, 32
    
    ; Dispatch based on message type
    mov eax, edx
    
    cmp eax, WM_CREATE
    je .handle_create
    
    cmp eax, WM_DESTROY
    je .handle_destroy
    
    cmp eax, WM_CLOSE
    je .handle_close
    
    cmp eax, WM_PAINT
    je .handle_paint
    
    cmp eax, WM_SIZE
    je .handle_size
    
    cmp eax, WM_CUSTOM_WISH
    je .handle_wish
    
    cmp eax, WM_CUSTOM_STATUS
    je .handle_status
    
    cmp eax, WM_CUSTOM_EXECUTE
    je .handle_execute
    
    ; Default message handling
    mov rcx, r9         ; lParam
    mov edx, r8         ; wParam
    mov rdx, edx
    call DefWindowProcA
    jmp .msg_done
    
.handle_create:
    xor eax, eax       ; Return 0 for success
    jmp .msg_done
    
.handle_destroy:
    xor ecx, ecx
    call PostQuitMessage  ; Send WM_QUIT
    xor eax, eax
    jmp .msg_done
    
.handle_close:
    ; Gracefully close the window
    mov rcx, g_main_window
    call DestroyWindow
    xor eax, eax
    jmp .msg_done
    
.handle_paint:
    ; Forward to Qt if needed
    sub rsp, SIZEOF PAINTSTRUCT
    mov rbx, rsp
    mov rcx, rcx        ; hWnd
    mov rdx, rbx        ; lpPaint
    call BeginPaint
    
    ; Paint the main content area
    ; For now, just clear the background
    mov rcx, rax
    mov edx, 0FFFFFFh   ; White
    call FillRect
    
    mov rcx, rcx        ; hWnd
    mov rdx, rbx        ; lpPaint
    call EndPaint
    
    add rsp, SIZEOF PAINTSTRUCT
    xor eax, eax
    jmp .msg_done
    
.handle_size:
    ; Resize panels to fit window
    ; r8 = new width, r9 = new height
    ; TODO: Resize panels
    xor eax, eax
    jmp .msg_done
    
.handle_wish:
    ; Custom message: user submitted a wish
    ; r8 = wish text pointer
    lea rcx, szWishReceived
    call log_message
    
    mov r12, r8         ; Save wish pointer
    
    ; Process wish through bridge
    sub rsp, SIZEOF WISH_CONTEXT
    mov rbx, rsp
    
    mov [rbx + WISH_CONTEXT.wish_text], r12
    mov DWORD PTR [rbx + WISH_CONTEXT.source], 2  ; UI source
    mov DWORD PTR [rbx + WISH_CONTEXT.priority], 1
    mov DWORD PTR [rbx + WISH_CONTEXT.timeout_ms], 30000
    mov [rbx + WISH_CONTEXT.callback_hwnd], rcx   ; Window for callbacks
    
    mov rcx, rbx
    call zero_cpp_bridge_process_wish
    
    add rsp, SIZEOF WISH_CONTEXT
    
    ; Update UI status
    mov ecx, 100
    lea rdx, szExecutionComplete
    call display_status_message
    
    xor eax, eax
    jmp .msg_done
    
.handle_status:
    ; Custom message: status update from bridge
    ; r8 = status code, r9 = progress percentage
    mov edx, r9d
    lea rcx, szExecutionUpdate
    call display_status_message
    xor eax, eax
    jmp .msg_done
    
.handle_execute:
    ; Custom message: execute a task
    ; r8 = task pointer
    xor eax, eax
    jmp .msg_done
    
.msg_done:
    add rsp, 32
    pop r12
    pop rbx
    ret
ALIGN 16
MainWndProc ENDP

;==============================================================================
; LOGGING SYSTEM
;==============================================================================

ALIGN 16
initialize_logging PROC
    
    ; Create logs directory if it doesn't exist
    ; Open/create log file for this session
    
    ; For now, just mark as initialized
    mov g_log_initialized, 1
    
    ret
ALIGN 16
initialize_logging ENDP

ALIGN 16
log_message PROC
    ; rcx = message string
    
    push rbx
    sub rsp, 32
    
    ; Get stdout handle
    mov edx, -11        ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rbx, rax
    
    ; Calculate string length
    mov rsi, rcx
    xor edx, edx
.len_loop:
    cmp BYTE PTR [rsi], 0
    je .len_done
    inc edx
    inc rsi
    jmp .len_loop
    
.len_done:
    ; Write to stdout
    mov rcx, rbx        ; hFile
    mov rdx, rcx        ; lpBuffer (original message ptr)
    mov r8d, edx        ; nNumberOfBytesToWrite
    lea r9, [rsp]       ; lpNumberOfBytesWritten
    call WriteFile
    
    ; Write newline
    mov BYTE PTR [rsp], 13   ; CR
    mov BYTE PTR [rsp+1], 10 ; LF
    mov rcx, rbx
    mov rdx, rsp
    mov r8d, 2
    lea r9, [rsp+4]
    call WriteFile
    
    add rsp, 32
    pop rbx
    ret
ALIGN 16
log_message ENDP

ALIGN 16
cleanup_logging PROC
    
    ; Close log file if open
    mov g_log_initialized, 0
    
    ret
ALIGN 16
cleanup_logging ENDP

;==============================================================================
; UI UPDATE UTILITIES
;==============================================================================

ALIGN 16
display_status_message PROC
    ; rcx = format string, edx = percentage
    
    ; Update status bar in UI
    mov rcx, g_main_window
    mov rdx, ecx        ; percentage
    call qt_status_update
    
    ret
ALIGN 16
display_status_message ENDP

;==============================================================================
; STRUCTURES
;==============================================================================

; Redefine WISH_CONTEXT for this module
WISH_CONTEXT STRUCT
    wish_text           QWORD ?
    wish_length         DWORD ?
    source              DWORD ?
    priority            DWORD ?
    timeout_ms          DWORD ?
    callback_hwnd       QWORD ?
    reserved            QWORD ?
WISH_CONTEXT ENDS

END
