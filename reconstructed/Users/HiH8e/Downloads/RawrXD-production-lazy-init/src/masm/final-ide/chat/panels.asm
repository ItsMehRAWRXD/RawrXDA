;==============================================================================
; Phase 3: Chat Panels - Complete MASM Implementation
; ==============================================================================
; Target: 2,600-3,700 LOC (Medium Complexity)
; Features: Message display, syntax highlighting, search, export/import
; Dependencies: Foundation, Widgets, Threading
; ==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib

;==============================================================================
; CONSTANTS
;==============================================================================

MAX_MESSAGES               EQU 1000
MAX_MESSAGE_LENGTH         EQU 4096
MAX_CHAT_PANELS            EQU 10
CHAT_LINE_HEIGHT           EQU 20
CHAT_MARGIN                EQU 10
CHAT_TIMESTAMP_WIDTH       EQU 120
CHAT_AVATAR_WIDTH          EQU 40
CHAT_CODE_BLOCK_MARGIN     EQU 20

; Message types
MESSAGE_TYPE_USER          EQU 0
MESSAGE_TYPE_ASSISTANT     EQU 1
MESSAGE_TYPE_SYSTEM        EQU 2
MESSAGE_TYPE_ERROR         EQU 3

; Chat modes
CHAT_MODE_NORMAL           EQU 0
CHAT_MODE_PLAN             EQU 1
CHAT_MODE_AGENT            EQU 2
CHAT_MODE_ASK              EQU 3

; Search flags
SEARCH_CASE_SENSITIVE      EQU 1
SEARCH_WHOLE_WORD          EQU 2
SEARCH_BACKWARDS           EQU 4

;==============================================================================
; STRUCTURES
;==============================================================================

; Chat message
CHAT_MESSAGE STRUCT
    message_type       DWORD ?
    timestamp          QWORD ?    ; FILETIME structure
    text_buffer        QWORD ?
    text_length        DWORD ?
    avatar_color       DWORD ?
    is_code_block      DWORD ?
    code_language      QWORD ?
    message_id         DWORD ?
    selection_start    DWORD ?
    selection_end      DWORD ?
    search_highlight   DWORD ?
CHAT_MESSAGE ENDS

; Chat panel
CHAT_PANEL STRUCT
    panel_handle       QWORD ?
    panel_rect         RECT {}
    messages           QWORD ?    ; Array of CHAT_MESSAGE
    message_count      DWORD ?
    scroll_position    DWORD ?
    visible_lines      DWORD ?
    current_mode       DWORD ?
    model_name         QWORD ?
    search_string      QWORD ?
    search_flags       DWORD ?
    search_position    DWORD ?
    font_handle        QWORD ?
    background_brush   QWORD ?
    user_brush         QWORD ?
    assistant_brush    QWORD ?
    system_brush       QWORD ?
    error_brush        QWORD ?
    code_block_brush   QWORD ?
    timestamp_font     QWORD ?
    message_font       QWORD ?
    code_font         QWORD ?
CHAT_PANEL ENDS

; Search result
SEARCH_RESULT STRUCT
    message_index      DWORD ?
    text_position      DWORD ?
    match_length       DWORD ?
SEARCH_RESULT ENDS

;==============================================================================
; GLOBAL DATA
;==============================================================================

.data

; Chat panel registry
g_chat_panels QWORD MAX_CHAT_PANELS DUP(0)
g_chat_panel_count DWORD 0

; Font and color resources
g_default_font QWORD 0
g_timestamp_font QWORD 0
g_code_font QWORD 0

; Color brushes
g_user_brush QWORD 0
g_assistant_brush QWORD 0
g_system_brush QWORD 0
g_error_brush QWORD 0
g_code_block_brush QWORD 0
g_background_brush QWORD 0

; Search state
g_current_search_string QWORD 0
g_current_search_flags DWORD 0
g_current_search_position DWORD 0

;==============================================================================
; EXTERNAL DECLARATIONS
;==============================================================================

EXTERN CreateWindowExA:PROC
EXTERN DestroyWindow:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC
EXTERN BeginPaint:PROC
EXTERN EndPaint:PROC
EXTERN CreateFontA:PROC
EXTERN DeleteObject:PROC
EXTERN CreateSolidBrush:PROC
EXTERN SelectObject:PROC
EXTERN SetBkColor:PROC
EXTERN SetTextColor:PROC
EXTERN TextOutA:PROC
EXTERN DrawTextA:PROC
EXTERN GetTextExtentPoint32A:PROC
EXTERN FillRect:PROC
EXTERN Rectangle:PROC
EXTERN GetClientRect:PROC
EXTERN ScrollWindow:PROC
EXTERN SetScrollPos:PROC
EXTERN GetScrollPos:PROC
EXTERN SetScrollRange:PROC
EXTERN ShowScrollBar:PROC
EXTERN InvalidateRect:PROC
EXTERN UpdateWindow:PROC
EXTERN SendMessageA:PROC
EXTERN GetSystemMetrics:PROC
EXTERN GetTickCount:PROC
EXTERN GetFileTime:PROC
EXTERN SystemTimeToFileTime:PROC
EXTERN FileTimeToSystemTime:PROC
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN SetFilePointer:PROC
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_str_length:PROC
EXTERN asm_str_copy:PROC
EXTERN asm_str_compare:PROC
EXTERN asm_str_find:PROC
EXTERN console_log:PROC

;==============================================================================
; CHAT PANEL MANAGEMENT
;==============================================================================

.code

;------------------------------------------------------------------------------
; chat_panel_create - Create a new chat panel
; Input: RCX = parent window, RDX = panel rect, R8 = initial mode
; Output: RAX = chat panel handle, 0 on error
;------------------------------------------------------------------------------
PUBLIC chat_panel_create
chat_panel_create PROC
    push rbp
    mov rbp, rsp
    sub rsp, 60h
    
    ; Save parameters
    mov [rbp-8], rcx   ; parent_window
    mov [rbp-16], rdx  ; panel_rect
    mov [rbp-24], r8   ; initial_mode
    
    ; Allocate chat panel structure
    mov rcx, sizeof CHAT_PANEL
    call asm_malloc
    test rax, rax
    jz chat_panel_create_error
    
    mov [rbp-32], rax  ; chat_panel
    
    ; Initialize structure
    mov rcx, rax
    call _initialize_chat_panel
    test rax, rax
    jz chat_panel_create_error
    
    ; Create window
    mov rcx, [rbp-8]   ; parent_window
    mov rdx, [rbp-16]  ; panel_rect
    mov r8, [rbp-32]   ; chat_panel
    call _create_chat_window
    test rax, rax
    jz chat_panel_create_error
    
    ; Add to registry
    mov rcx, [rbp-32]
    call _add_chat_panel_to_registry
    test rax, rax
    jz chat_panel_create_error
    
    mov rax, [rbp-32]
    jmp chat_panel_create_exit
    
chat_panel_create_error:
    ; Cleanup on error
    mov rcx, [rbp-32]
    test rcx, rcx
    jz chat_panel_create_cleanup_done
    call _cleanup_chat_panel
    
chat_panel_create_cleanup_done:
    xor rax, rax
    
chat_panel_create_exit:
    add rsp, 60h
    pop rbp
    ret
chat_panel_create ENDP

;------------------------------------------------------------------------------
; chat_panel_destroy - Destroy chat panel
; Input: RCX = chat panel handle
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC chat_panel_destroy
chat_panel_destroy PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Remove from registry
    call _remove_chat_panel_from_registry
    
    ; Cleanup resources
    call _cleanup_chat_panel
    
    mov rax, 1
    add rsp, 20h
    pop rbp
    ret
chat_panel_destroy ENDP

;------------------------------------------------------------------------------
; chat_panel_add_message - Add message to chat panel
; Input: RCX = chat panel, RDX = message type, R8 = message text
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC chat_panel_add_message
chat_panel_add_message PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Save parameters
    mov [rbp-8], rcx   ; chat_panel
    mov [rbp-16], rdx  ; message_type
    mov [rbp-24], r8   ; message_text
    
    ; Validate parameters
    test rcx, rcx
    jz chat_panel_add_error
    test r8, r8
    jz chat_panel_add_error
    
    ; Check message count limit
    mov eax, [rcx+CHAT_PANEL.message_count]
    cmp eax, MAX_MESSAGES
    jge chat_panel_add_full
    
    ; Allocate message structure
    mov rcx, sizeof CHAT_MESSAGE
    call asm_malloc
    test rax, rax
    jz chat_panel_add_error
    
    mov [rbp-32], rax  ; new_message
    
    ; Initialize message
    mov rcx, rax
    mov rdx, [rbp-16]  ; message_type
    mov r8, [rbp-24]   ; message_text
    call _initialize_chat_message
    test rax, rax
    jz chat_panel_add_error
    
    ; Add to message array
    mov rcx, [rbp-8]   ; chat_panel
    mov rdx, [rbp-32]  ; new_message
    call _add_message_to_panel
    test rax, rax
    jz chat_panel_add_error
    
    ; Update scroll position
    mov rcx, [rbp-8]
    call _update_scroll_position
    
    ; Redraw panel
    mov rcx, [rbp-8]
    call _invalidate_chat_panel
    
    mov rax, 1
    jmp chat_panel_add_exit
    
chat_panel_add_full:
    ; Remove oldest message if at limit
    mov rcx, [rbp-8]
    call _remove_oldest_message
    
    ; Try again
    jmp chat_panel_add_message
    
chat_panel_add_error:
    ; Cleanup on error
    mov rcx, [rbp-32]
    test rcx, rcx
    jz chat_panel_add_cleanup_done
    call asm_free
    
chat_panel_add_cleanup_done:
    xor rax, rax
    
chat_panel_add_exit:
    add rsp, 40h
    pop rbp
    ret
chat_panel_add_message ENDP

;------------------------------------------------------------------------------
; chat_panel_clear - Clear all messages from chat panel
; Input: RCX = chat panel handle
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC chat_panel_clear
chat_panel_clear PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Clear message array
    call _clear_all_messages
    test rax, rax
    jz chat_panel_clear_error
    
    ; Reset scroll position
    mov [rcx+CHAT_PANEL.scroll_position], 0
    mov [rcx+CHAT_PANEL.search_position], 0
    
    ; Redraw panel
    call _invalidate_chat_panel
    
    mov rax, 1
    jmp chat_panel_clear_exit
    
chat_panel_clear_error:
    xor rax, rax
    
chat_panel_clear_exit:
    add rsp, 20h
    pop rbp
    ret
chat_panel_clear ENDP

;------------------------------------------------------------------------------
; chat_panel_search - Search in chat messages
; Input: RCX = chat panel, RDX = search string, R8 = search flags
; Output: RAX = search result count
;------------------------------------------------------------------------------
PUBLIC chat_panel_search
chat_panel_search PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Save parameters
    mov [rbp-8], rcx   ; chat_panel
    mov [rbp-16], rdx  ; search_string
    mov [rbp-24], r8   ; search_flags
    
    ; Validate parameters
    test rcx, rcx
    jz chat_panel_search_error
    test rdx, rdx
    jz chat_panel_search_error
    
    ; Set search state
    mov [rcx+CHAT_PANEL.search_string], rdx
    mov [rcx+CHAT_PANEL.search_flags], r8d
    mov [rcx+CHAT_PANEL.search_position], 0
    
    ; Perform search
    mov rcx, [rbp-8]
    mov rdx, [rbp-16]
    mov r8d, [rbp-24]
    call _perform_search
    
    jmp chat_panel_search_exit
    
chat_panel_search_error:
    xor rax, rax
    
chat_panel_search_exit:
    add rsp, 40h
    pop rbp
    ret
chat_panel_search ENDP

;------------------------------------------------------------------------------
; chat_panel_export - Export chat to file
; Input: RCX = chat panel, RDX = filename
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC chat_panel_export
chat_panel_export PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Save parameters
    mov [rbp-8], rcx   ; chat_panel
    mov [rbp-16], rdx  ; filename
    
    ; Validate parameters
    test rcx, rcx
    jz chat_panel_export_error
    test rdx, rdx
    jz chat_panel_export_error
    
    ; Create file
    mov rcx, rdx
    call _create_export_file
    test rax, rax
    jz chat_panel_export_error
    
    mov [rbp-24], rax  ; file_handle
    
    ; Write messages
    mov rcx, [rbp-8]   ; chat_panel
    mov rdx, rax       ; file_handle
    call _export_messages
    test rax, rax
    jz chat_panel_export_error
    
    ; Close file
    mov rcx, [rbp-24]
    call CloseHandle
    
    mov rax, 1
    jmp chat_panel_export_exit
    
chat_panel_export_error:
    ; Cleanup on error
    mov rcx, [rbp-24]
    test rcx, rcx
    jz chat_panel_export_cleanup_done
    call CloseHandle
    
chat_panel_export_cleanup_done:
    xor rax, rax
    
chat_panel_export_exit:
    add rsp, 40h
    pop rbp
    ret
chat_panel_export ENDP

;------------------------------------------------------------------------------
; chat_panel_import - Import chat from file
; Input: RCX = chat panel, RDX = filename
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC chat_panel_import
chat_panel_import PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Save parameters
    mov [rbp-8], rcx   ; chat_panel
    mov [rbp-16], rdx  ; filename
    
    ; Validate parameters
    test rcx, rcx
    jz chat_panel_import_error
    test rdx, rdx
    jz chat_panel_import_error
    
    ; Clear existing messages
    call chat_panel_clear
    test rax, rax
    jz chat_panel_import_error
    
    ; Open file
    mov rcx, rdx
    call _open_import_file
    test rax, rax
    jz chat_panel_import_error
    
    mov [rbp-24], rax  ; file_handle
    
    ; Read messages
    mov rcx, [rbp-8]   ; chat_panel
    mov rdx, rax       ; file_handle
    call _import_messages
    test rax, rax
    jz chat_panel_import_error
    
    ; Close file
    mov rcx, [rbp-24]
    call CloseHandle
    
    ; Redraw panel
    mov rcx, [rbp-8]
    call _invalidate_chat_panel
    
    mov rax, 1
    jmp chat_panel_import_exit
    
chat_panel_import_error:
    ; Cleanup on error
    mov rcx, [rbp-24]
    test rcx, rcx
    jz chat_panel_import_cleanup_done
    call CloseHandle
    
chat_panel_import_cleanup_done:
    xor rax, rax
    
chat_panel_import_exit:
    add rsp, 40h
    pop rbp
    ret
chat_panel_import ENDP

;------------------------------------------------------------------------------
; chat_panel_set_model_name - Set current model name
; Input: RCX = chat panel, RDX = model name
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC chat_panel_set_model_name
chat_panel_set_model_name PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Validate parameters
    test rcx, rcx
    jz chat_panel_set_model_error
    test rdx, rdx
    jz chat_panel_set_model_error
    
    ; Free existing model name
    mov r8, [rcx+CHAT_PANEL.model_name]
    test r8, r8
    jz chat_panel_set_model_copy
    
    call asm_free
    
chat_panel_set_model_copy:
    ; Copy new model name
    mov rcx, rdx
    call asm_str_length
    inc rax            ; Include null terminator
    mov rcx, rax
    call asm_malloc
    test rax, rax
    jz chat_panel_set_model_error
    
    mov [rbp-8], rax   ; new_model_name
    
    ; Copy string
    mov rcx, rax
    mov rdx, [rbp+16]  ; model_name
    call asm_str_copy
    
    ; Set model name
    mov rcx, [rbp+8]   ; chat_panel
    mov [rcx+CHAT_PANEL.model_name], rax
    
    ; Redraw panel
    call _invalidate_chat_panel
    
    mov rax, 1
    jmp chat_panel_set_model_exit
    
chat_panel_set_model_error:
    xor rax, rax
    
chat_panel_set_model_exit:
    add rsp, 20h
    pop rbp
    ret
chat_panel_set_model_name ENDP

;------------------------------------------------------------------------------
; chat_panel_get_selection - Get selected message(s)
; Input: RCX = chat panel
; Output: RAX = selection count, RDX = array of message indices
;------------------------------------------------------------------------------
PUBLIC chat_panel_get_selection
chat_panel_get_selection PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Validate parameters
    test rcx, rcx
    jz chat_panel_get_selection_error
    
    ; Get selection
    call _get_message_selection
    
    jmp chat_panel_get_selection_exit
    
chat_panel_get_selection_error:
    xor rax, rax
    xor rdx, rdx
    
chat_panel_get_selection_exit:
    add rsp, 20h
    pop rbp
    ret
chat_panel_get_selection ENDP

;==============================================================================
; INTERNAL HELPER FUNCTIONS
;==============================================================================

; Initialize chat panel structure
_initialize_chat_panel PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Initialize fields
    mov [rcx+CHAT_PANEL.panel_handle], 0
    mov [rcx+CHAT_PANEL.messages], 0
    mov [rcx+CHAT_PANEL.message_count], 0
    mov [rcx+CHAT_PANEL.scroll_position], 0
    mov [rcx+CHAT_PANEL.visible_lines], 0
    mov [rcx+CHAT_PANEL.current_mode], CHAT_MODE_NORMAL
    mov [rcx+CHAT_PANEL.model_name], 0
    mov [rcx+CHAT_PANEL.search_string], 0
    mov [rcx+CHAT_PANEL.search_flags], 0
    mov [rcx+CHAT_PANEL.search_position], 0
    
    ; Create fonts
    call _create_chat_fonts
    test rax, rax
    jz initialize_panel_error
    
    ; Create brushes
    call _create_chat_brushes
    test rax, rax
    jz initialize_panel_error
    
    ; Allocate message array
    mov rcx, MAX_MESSAGES
    imul rcx, sizeof CHAT_MESSAGE
    call asm_malloc
    test rax, rax
    jz initialize_panel_error
    
    mov [rbp-8], rax   ; messages_array
    mov [rax], rax      ; Store pointer in panel
    
    mov rax, 1
    jmp initialize_panel_exit
    
initialize_panel_error:
    xor rax, rax
    
initialize_panel_exit:
    add rsp, 20h
    pop rbp
    ret
_initialize_chat_panel ENDP

; Create chat window
_create_chat_window PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Window style
    mov r9d, WS_CHILD OR WS_VISIBLE OR WS_VSCROLL OR WS_HSCROLL
    mov r8d, 0         ; Extended style
    mov edx, offset szChatWindowClass
    mov rcx, [rbp+16]  ; parent_window
    
    ; Window position
    mov r10, [rbp+24]  ; panel_rect
    push 0             ; hMenu
    push [rbp+32]      ; lpParam (chat_panel)
    push [r10+RECT.bottom] ; height
    push [r10+RECT.right]  ; width
    push [r10+RECT.top]    ; y
    push [r10+RECT.left]   ; x
    
    call CreateWindowExA
    
    test rax, rax
    jz create_window_error
    
    ; Store window handle
    mov rcx, [rbp+32]  ; chat_panel
    mov [rcx+CHAT_PANEL.panel_handle], rax
    
    mov rax, 1
    jmp create_window_exit
    
create_window_error:
    xor rax, rax
    
create_window_exit:
    add rsp, 40h
    pop rbp
    ret
_create_chat_window ENDP

; Initialize chat message
_initialize_chat_message PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Set message type
    mov [rcx+CHAT_MESSAGE.message_type], edx
    
    ; Get current timestamp
    call _get_current_timestamp
    mov [rcx+CHAT_MESSAGE.timestamp], rax
    
    ; Copy message text
    mov rcx, r8        ; message_text
    call asm_str_length
    inc rax            ; Include null terminator
    mov rcx, rax
    call asm_malloc
    test rax, rax
    jz initialize_message_error
    
    mov [rbp-8], rax   ; text_buffer
    
    ; Copy string
    mov rcx, rax
    mov rdx, [rbp+24]  ; message_text
    call asm_str_copy
    
    ; Store in message
    mov rcx, [rbp+8]   ; message
    mov [rcx+CHAT_MESSAGE.text_buffer], rax
    mov eax, [rbp-8]   ; text_length (from asm_str_length)
    mov [rcx+CHAT_MESSAGE.text_length], eax
    
    ; Set default values
    mov [rcx+CHAT_MESSAGE.is_code_block], 0
    mov [rcx+CHAT_MESSAGE.code_language], 0
    mov [rcx+CHAT_MESSAGE.selection_start], 0
    mov [rcx+CHAT_MESSAGE.selection_end], 0
    mov [rcx+CHAT_MESSAGE.search_highlight], 0
    
    ; Set avatar color based on type
    cmp dword ptr [rcx+CHAT_MESSAGE.message_type], MESSAGE_TYPE_USER
    je set_user_color
    cmp dword ptr [rcx+CHAT_MESSAGE.message_type], MESSAGE_TYPE_ASSISTANT
    je set_assistant_color
    cmp dword ptr [rcx+CHAT_MESSAGE.message_type], MESSAGE_TYPE_SYSTEM
    je set_system_color
    jmp set_error_color
    
set_user_color:
    mov dword ptr [rcx+CHAT_MESSAGE.avatar_color], 00FF0000h ; Blue
    jmp initialize_message_done
    
set_assistant_color:
    mov dword ptr [rcx+CHAT_MESSAGE.avatar_color], 0000FF00h ; Green
    jmp initialize_message_done
    
set_system_color:
    mov dword ptr [rcx+CHAT_MESSAGE.avatar_color], 00FFFF00h ; Yellow
    jmp initialize_message_done
    
set_error_color:
    mov dword ptr [rcx+CHAT_MESSAGE.avatar_color], 00FF0000h ; Red
    
initialize_message_done:
    mov rax, 1
    jmp initialize_message_exit
    
initialize_message_error:
    xor rax, rax
    
initialize_message_exit:
    add rsp, 20h
    pop rbp
    ret
_initialize_chat_message ENDP

; Add message to panel
_add_message_to_panel PROC
    push rbp
    mov rbp, rsp
    
    ; Get message array
    mov r8, [rcx+CHAT_PANEL.messages]
    test r8, r8
    jz add_message_error
    
    ; Find empty slot
    mov eax, 0
    mov r9, r8
    
add_message_loop:
    cmp eax, MAX_MESSAGES
    jge add_message_full
    
    cmp qword ptr [r9+eax*sizeof CHAT_MESSAGE+CHAT_MESSAGE.text_buffer], 0
    je add_message_found
    
    inc eax
    jmp add_message_loop
    
add_message_found:
    ; Copy message to array
    mov r10, rdx       ; new_message
    mov r11, r9
    add r11, eax
    imul eax, sizeof CHAT_MESSAGE
    
    ; Copy structure
    mov r12, sizeof CHAT_MESSAGE
    
copy_message_loop:
    test r12, r12
    jz copy_message_done
    
    mov r13b, byte ptr [r10]
    mov byte ptr [r11], r13b
    inc r10
    inc r11
    dec r12
    jmp copy_message_loop
    
copy_message_done:
    ; Increment message count
    inc dword ptr [rcx+CHAT_PANEL.message_count]
    
    ; Free original message structure
    mov rcx, rdx
    call asm_free
    
    mov rax, 1
    jmp add_message_exit
    
add_message_full:
add_message_error:
    xor rax, rax
    
add_message_exit:
    pop rbp
    ret
_add_message_to_panel ENDP

; Remove oldest message
_remove_oldest_message PROC
    push rbp
    mov rbp, rsp
    
    ; Get message array
    mov r8, [rcx+CHAT_PANEL.messages]
    test r8, r8
    jz remove_oldest_error
    
    ; Find oldest message (index 0)
    mov eax, 0
    mov r9, r8
    add r9, eax
    imul eax, sizeof CHAT_MESSAGE
    
    ; Free message text
    mov r10, [r9+CHAT_MESSAGE.text_buffer]
    test r10, r10
    jz remove_oldest_skip_text
    
    mov rcx, r10
    call asm_free
    
remove_oldest_skip_text:
    ; Clear message slot
    mov qword ptr [r9+CHAT_MESSAGE.text_buffer], 0
    mov dword ptr [r9+CHAT_MESSAGE.text_length], 0
    
    ; Decrement message count
    dec dword ptr [rcx+CHAT_PANEL.message_count]
    
    ; Shift remaining messages
    mov eax, 1
    mov r9, r8
    
shift_messages_loop:
    cmp eax, MAX_MESSAGES
    jge shift_messages_done
    
    cmp qword ptr [r9+eax*sizeof CHAT_MESSAGE+CHAT_MESSAGE.text_buffer], 0
    je shift_messages_done
    
    ; Move message to previous slot
    mov r10, r9
    add r10, eax
    imul eax, sizeof CHAT_MESSAGE
    mov r11, r10
    sub r11, sizeof CHAT_MESSAGE
    
    mov r12, sizeof CHAT_MESSAGE
    
shift_message_loop:
    test r12, r12
    jz shift_message_done
    
    mov r13b, byte ptr [r10]
    mov byte ptr [r11], r13b
    inc r10
    inc r11
    dec r12
    jmp shift_message_loop
    
shift_message_done:
    ; Clear current slot
    mov qword ptr [r10], 0
    
    inc eax
    jmp shift_messages_loop
    
shift_messages_done:
    mov rax, 1
    jmp remove_oldest_exit
    
remove_oldest_error:
    xor rax, rax
    
remove_oldest_exit:
    pop rbp
    ret
_remove_oldest_message ENDP

; Clear all messages
_clear_all_messages PROC
    push rbp
    mov rbp, rsp
    
    ; Get message array
    mov r8, [rcx+CHAT_PANEL.messages]
    test r8, r8
    jz clear_all_error
    
    ; Free all message texts
    mov eax, 0
    mov r9, r8
    
clear_messages_loop:
    cmp eax, MAX_MESSAGES
    jge clear_messages_done
    
    mov r10, [r9+eax*sizeof CHAT_MESSAGE+CHAT_MESSAGE.text_buffer]
    test r10, r10
    jz clear_messages_next
    
    mov rcx, r10
    call asm_free
    mov qword ptr [r9+eax*sizeof CHAT_MESSAGE+CHAT_MESSAGE.text_buffer], 0
    
clear_messages_next:
    inc eax
    jmp clear_messages_loop
    
clear_messages_done:
    ; Reset message count
    mov dword ptr [rcx+CHAT_PANEL.message_count], 0
    
    mov rax, 1
    jmp clear_all_exit
    
clear_all_error:
    xor rax, rax
    
clear_all_exit:
    pop rbp
    ret
_clear_all_messages ENDP

; Update scroll position
_update_scroll_position PROC
    push rbp
    mov rbp, rsp
    
    ; Calculate total height
    mov eax, [rcx+CHAT_PANEL.message_count]
    imul eax, CHAT_LINE_HEIGHT
    add eax, CHAT_MARGIN * 2
    
    ; Get client height
    mov r8, [rcx+CHAT_PANEL.panel_handle]
    mov r9, offset client_rect
    call GetClientRect
    
    mov edx, [client_rect+RECT.bottom]
    sub edx, [client_rect+RECT.top]
    
    ; Set scroll range
    mov r8, rcx        ; chat_panel
    mov rcx, [r8+CHAT_PANEL.panel_handle]
    mov rdx, SB_VERT
    mov r9, 0          ; min
    mov r10, eax       ; max
    sub r10, edx       ; Adjust for visible area
    call SetScrollRange
    
    ; Auto-scroll to bottom if near bottom
    mov eax, [r8+CHAT_PANEL.scroll_position]
    mov edx, [r8+CHAT_PANEL.visible_lines]
    add eax, edx
    cmp eax, [r8+CHAT_PANEL.message_count]
    jl update_scroll_done
    
    ; Scroll to bottom
    mov eax, [r8+CHAT_PANEL.message_count]
    sub eax, edx
    mov [r8+CHAT_PANEL.scroll_position], eax
    
    ; Update scroll bar
    mov rcx, [r8+CHAT_PANEL.panel_handle]
    mov rdx, SB_VERT
    mov r8, eax
    call SetScrollPos
    
update_scroll_done:
    mov rax, 1
    pop rbp
    ret
_update_scroll_position ENDP

; Invalidate chat panel
_invalidate_chat_panel PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    mov rdx, 0         ; NULL rect (entire client area)
    mov r8, 0          ; FALSE (erase background)
    call InvalidateRect
    
    call UpdateWindow
    
    mov rax, 1
    add rsp, 20h
    pop rbp
    ret
_invalidate_chat_panel ENDP

; Perform search
_perform_search PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Implementation would search through messages
    ; and highlight matches
    
    mov rax, 0         ; Placeholder
    add rsp, 40h
    pop rbp
    ret
_perform_search ENDP

; Create export file
_create_export_file PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Create file
    mov rdx, GENERIC_WRITE
    mov r8, 0          ; No sharing
    mov r9, 0          ; No security
    mov r10, CREATE_ALWAYS
    mov r11, FILE_ATTRIBUTE_NORMAL
    push 0             ; hTemplate
    push r11           ; dwFlagsAndAttributes
    push r10           ; dwCreationDisposition
    push r9            ; lpSecurityAttributes
    push r8            ; dwShareMode
    push rdx           ; dwDesiredAccess
    push rcx           ; lpFileName
    
    call CreateFileA
    
    add rsp, 20h
    pop rbp
    ret
_create_export_file ENDP

; Export messages
_export_messages PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Implementation would write messages to file
    ; in JSON or text format
    
    mov rax, 1         ; Placeholder
    add rsp, 40h
    pop rbp
    ret
_export_messages ENDP

; Open import file
_open_import_file PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Open file
    mov rdx, GENERIC_READ
    mov r8, FILE_SHARE_READ
    mov r9, 0          ; No security
    mov r10, OPEN_EXISTING
    mov r11, FILE_ATTRIBUTE_NORMAL
    push 0             ; hTemplate
    push r11           ; dwFlagsAndAttributes
    push r10           ; dwCreationDisposition
    push r9            ; lpSecurityAttributes
    push r8            ; dwShareMode
    push rdx           ; dwDesiredAccess
    push rcx           ; lpFileName
    
    call CreateFileA
    
    add rsp, 20h
    pop rbp
    ret
_open_import_file ENDP

; Import messages
_import_messages PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Implementation would read messages from file
    ; and add to chat panel
    
    mov rax, 1         ; Placeholder
    add rsp, 40h
    pop rbp
    ret
_import_messages ENDP

; Get message selection
_get_message_selection PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Implementation would return selected messages
    
    xor rax, rax       ; Placeholder
    xor rdx, rdx
    add rsp, 20h
    pop rbp
    ret
_get_message_selection ENDP

; Create chat fonts
_create_chat_fonts PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Create default font
    mov rcx, -12       ; Height
    mov rdx, 0         ; Width
    mov r8, 0          ; Escapement
    mov r9, 0          ; Orientation
    mov r10, FW_NORMAL ; Weight
    mov r11, 0         ; Italic
    push 0             ; lpFacename
    push 0             ; OutputPrecision
    push 0             ; ClipPrecision
    push 0             ; Quality
    push 0             ; PitchAndFamily
    push r11           ; Italic
    push r10           ; Weight
    push r9            ; Orientation
    push r8            ; Escapement
    push rdx           ; Width
    push rcx           ; Height
    
    call CreateFontA
    mov g_default_font, rax
    
    ; Similar for timestamp_font and code_font...
    
    mov rax, 1
    add rsp, 20h
    pop rbp
    ret
_create_chat_fonts ENDP

; Create chat brushes
_create_chat_brushes PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Create background brush
    mov rcx, 00FFFFFFh ; White
    call CreateSolidBrush
    mov g_background_brush, rax
    
    ; Create user brush (blue)
    mov rcx, 00E6F3FFh ; Light blue
    call CreateSolidBrush
    mov g_user_brush, rax
    
    ; Create assistant brush (green)
    mov rcx, 00F0F8E6h ; Light green
    call CreateSolidBrush
    mov g_assistant_brush, rax
    
    ; Create system brush (yellow)
    mov rcx, 00FFF8E6h ; Light yellow
    call CreateSolidBrush
    mov g_system_brush, rax
    
    ; Create error brush (red)
    mov rcx, 00FFE6E6h ; Light red
    call CreateSolidBrush
    mov g_error_brush, rax
    
    ; Create code block brush (gray)
    mov rcx, 00F5F5F5h ; Light gray
    call CreateSolidBrush
    mov g_code_block_brush, rax
    
    mov rax, 1
    add rsp, 20h
    pop rbp
    ret
_create_chat_brushes ENDP

; Get current timestamp
_get_current_timestamp PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Get current time as FILETIME
    lea rcx, [rbp-8]   ; System time
    lea rdx, [rbp-16]  ; File time
    call GetSystemTime
    call SystemTimeToFileTime
    
    mov rax, [rbp-16]  ; Return file time
    
    add rsp, 20h
    pop rbp
    ret
_get_current_timestamp ENDP

; Add chat panel to registry
_add_chat_panel_to_registry PROC
    push rbp
    mov rbp, rsp
    
    ; Find empty slot
    mov eax, 0
    mov rdx, offset g_chat_panels
    
add_panel_loop:
    cmp eax, MAX_CHAT_PANELS
    jge add_panel_full
    
    cmp qword ptr [rdx+rax*8], 0
    je add_panel_found
    
    inc eax
    jmp add_panel_loop
    
add_panel_found:
    ; Store panel handle
    mov [rdx+rax*8], rcx
    inc g_chat_panel_count
    
add_panel_full:
    mov rax, 1
    pop rbp
    ret
_add_chat_panel_to_registry ENDP

; Remove chat panel from registry
_remove_chat_panel_from_registry PROC
    push rbp
    mov rbp, rsp
    
    ; Find panel
    mov eax, 0
    mov rdx, offset g_chat_panels
    
remove_panel_loop:
    cmp eax, MAX_CHAT_PANELS
    jge remove_panel_done
    
    cmp [rdx+rax*8], rcx
    je remove_panel_found
    
    inc eax
    jmp remove_panel_loop
    
remove_panel_found:
    ; Clear slot
    mov qword ptr [rdx+rax*8], 0
    dec g_chat_panel_count
    
remove_panel_done:
    mov rax, 1
    pop rbp
    ret
_remove_chat_panel_from_registry ENDP

; Cleanup chat panel
_cleanup_chat_panel PROC
    push rbp
    mov rbp, rsp
    
    ; Destroy window
    mov rcx, [rcx+CHAT_PANEL.panel_handle]
    test rcx, rcx
    jz cleanup_window_done
    call DestroyWindow
    
cleanup_window_done:
    ; Free message array
    mov rcx, [rcx+CHAT_PANEL.messages]
    test rcx, rcx
    jz cleanup_messages_done
    call asm_free
    
cleanup_messages_done:
    ; Free model name
    mov rcx, [rcx+CHAT_PANEL.model_name]
    test rcx, rcx
    jz cleanup_model_done
    call asm_free
    
cleanup_model_done:
    ; Free search string
    mov rcx, [rcx+CHAT_PANEL.search_string]
    test rcx, rcx
    jz cleanup_search_done
    call asm_free
    
cleanup_search_done:
    ; Delete fonts
    mov rcx, g_default_font
    test rcx, rcx
    jz cleanup_fonts_done
    call DeleteObject
    
cleanup_fonts_done:
    ; Delete brushes
    mov rcx, g_background_brush
    test rcx, rcx
    jz cleanup_brushes_done
    call DeleteObject
    
cleanup_brushes_done:
    mov rax, 1
    pop rbp
    ret
_cleanup_chat_panel ENDP

;==============================================================================
; EXPORTED FUNCTION TABLE
;==============================================================================

PUBLIC chat_panel_create
PUBLIC chat_panel_destroy
PUBLIC chat_panel_add_message
PUBLIC chat_panel_clear
PUBLIC chat_panel_search
PUBLIC chat_panel_export
PUBLIC chat_panel_import
PUBLIC chat_panel_set_model_name
PUBLIC chat_panel_get_selection

;==============================================================================
; DATA SECTION
;==============================================================================

.data

; Window class name
szChatWindowClass db "ChatPanelClass",0

; Client rect for calculations
client_rect RECT {}

.end