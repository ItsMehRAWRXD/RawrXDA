;==========================================================================
; layout_persistence.asm - Save/Load IDE Layout & Settings to JSON
;==========================================================================
; Implements persistent storage of IDE state:
; - Window positions and sizes
; - Pane visibility states
; - Tab layouts (editor, chat, panels)
; - User preferences
; - Theme settings
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
; EXTERN CreateFileA:PROC
; EXTERN CloseHandle:PROC
; EXTERN WriteFile:PROC
; EXTERN ReadFile:PROC
; EXTERN GetFileSize:PROC
; EXTERN GetFileSizeEx:PROC

EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_log:PROC
EXTERN strcmp_masm:PROC
; EXTERN GetTickCount:PROC

; UI + Win32 helpers referenced by layout persistence
EXTERN ui_get_main_hwnd:PROC
; EXTERN GetWindowRect:PROC

; Pane/tab visibility + tab enumeration (provided elsewhere in MASM UI)
EXTERN output_pane_is_visible:PROC
EXTERN file_tree_is_visible:PROC
EXTERN chat_pane_is_visible:PROC
EXTERN terminal_pane_is_visible:PROC
EXTERN tab_manager_get_count:PROC
EXTERN tab_manager_get_tab:PROC

; User preference globals (provided elsewhere)
EXTERN g_current_theme:DWORD
EXTERN g_font_size:DWORD
EXTERN g_auto_save_interval:DWORD

;==========================================================================
; CONSTANTS
;==========================================================================

GENERIC_READ            EQU 80000000h
GENERIC_WRITE           EQU 40000000h
FILE_SHARE_READ         EQU 00000001h
CREATE_ALWAYS           EQU 0000002h
OPEN_EXISTING           EQU 00000003h

JSON_BUFFER_SIZE        EQU 65536   ; 64 KB JSON buffer
FILE_ATTRIBUTE_NORMAL   EQU 00000080h

;==========================================================================
; DATA SECTION
;==========================================================================

.data
    LAYOUT_FILE          BYTE "layout.json",0
    SETTINGS_FILE        BYTE "settings.json",0

    szLayoutHeader      BYTE "{",0DH,0AH,
                             """IDE"": {",0DH,0AH,
                             """mainWindow"": {",0DH,0AH,
                             """x"": ",0
    
    szLayoutFooter      BYTE "}",0DH,0AH,"}",0
    
    szPaneStates        BYTE """panes"": {",0DH,0AH,
                             """explorer"": {""visible"": ",0
    
    szTabStates         BYTE """tabs"": {",0DH,0AH,
                             """editor"": [",0DH,0AH,
                             "},",0DH,0AH,
                             """chat"": [",0DH,0AH,
                             "},",0DH,0AH,
                             """panels"": [",0DH,0AH,
                             "}",0DH,0AH,
                             "]",0
    
    szOpenBrace         BYTE "{",0DH,0AH,0
    szCloseBrace        BYTE "},",0DH,0AH,0
    szOpenBracket       BYTE "[",0
    szCloseBracket      BYTE "]",0
    
    szTrue              BYTE "true",0
    szFalse             BYTE "false",0
    
    szLayoutSaveSuccess BYTE "[Layout] Saved to layout.json",0
    szLayoutLoadSuccess BYTE "[Layout] Loaded from layout.json",0
    szLayoutLoadFailed  BYTE "[Layout] Failed to load layout",0

    ; Keys injected during save_layout_json
    szEditorVisible     BYTE '"','e','d','i','t','o','r','V','i','s','i','b','l','e','"',':',' ',0
    szTabsJSON          BYTE '"','t','a','b','s','"',':',' ','[',0

.data?
    hLayoutFile         QWORD ?
    layout_buffer       BYTE JSON_BUFFER_SIZE DUP (?)
    buffer_pos          QWORD ?
    bytes_transferred   QWORD ?
    temp_rect           DWORD 4 DUP (?)

;==========================================================================
; PUBLIC PROCEDURES
;==========================================================================

PUBLIC save_layout_json
PUBLIC load_layout_json
PUBLIC save_settings_json
PUBLIC load_settings_json
PUBLIC append_json_value

.code

;==========================================================================
; save_layout_json - Save IDE layout to JSON file
; Returns: eax = 1 (success) or 0 (failure)
;==========================================================================
save_layout_json PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Create/open layout.json
    lea rcx, [LAYOUT_FILE]
    mov edx, GENERIC_WRITE
    xor r8d, r8d                    ; dwShareMode
    xor r9d, r9d                    ; lpSecurityAttributes
    mov dword ptr [rsp+20h], CREATE_ALWAYS
    mov dword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0      ; hTemplateFile
    call CreateFileA
    
    cmp rax, -1
    je json_create_failed
    mov qword ptr [hLayoutFile], rax
    
    ; Build JSON header
    lea rcx, [layout_buffer]
    mov qword ptr [buffer_pos], 0
    
    ; Write header
    lea rcx, [szLayoutHeader]
    lea rdx, [layout_buffer]
    call append_json_string
    lea rax, [layout_buffer]
    sub rdx, rax
    mov qword ptr [buffer_pos], rdx
    
    ; Add window position via GetWindowRect
    call ui_get_main_hwnd
    mov rcx, rax
    lea rdx, [temp_rect]
    call GetWindowRect
    
    mov edx, [temp_rect]
    lea rcx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rcx, rax
    call append_json_int
    lea rax, [layout_buffer]
    sub rcx, rax
    mov qword ptr [buffer_pos], rcx
    
    mov edx, [temp_rect + 4]
    lea rcx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rcx, rax
    call append_json_int
    lea rax, [layout_buffer]
    sub rcx, rax
    mov qword ptr [buffer_pos], rcx
    
    ; Add pane visibility states
    lea rcx, [szEditorVisible]
    lea rdx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rdx, rax
    call append_json_string
    lea rax, [layout_buffer]
    sub rdx, rax
    mov qword ptr [buffer_pos], rdx
    
    mov edx, 1
    lea rcx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rcx, rax
    call append_json_bool
    lea rax, [layout_buffer]
    sub rcx, rax
    mov qword ptr [buffer_pos], rcx
    
    ; Output pane visibility
    call output_pane_is_visible
    mov edx, eax
    lea rcx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rcx, rax
    call append_json_bool
    lea rax, [layout_buffer]
    sub rcx, rax
    mov qword ptr [buffer_pos], rcx
    
    ; File tree visibility
    call file_tree_is_visible
    mov edx, eax
    lea rcx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rcx, rax
    call append_json_bool
    lea rax, [layout_buffer]
    sub rcx, rax
    mov qword ptr [buffer_pos], rcx
    
    ; Chat pane visibility
    call chat_pane_is_visible
    mov edx, eax
    lea rcx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rcx, rax
    call append_json_bool
    lea rax, [layout_buffer]
    sub rcx, rax
    mov qword ptr [buffer_pos], rcx
    
    ; Terminal visibility
    call terminal_pane_is_visible
    mov edx, eax
    lea rcx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rcx, rax
    call append_json_bool
    lea rax, [layout_buffer]
    sub rcx, rax
    mov qword ptr [buffer_pos], rcx
    
    ; Add tab configurations
    lea rcx, [szTabsJSON]
    lea rdx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rdx, rax
    call append_json_string
    lea rax, [layout_buffer]
    sub rdx, rax
    mov qword ptr [buffer_pos], rdx
    
    call tab_manager_get_count
    mov r8, rax
    xor r9, r9
    
tab_loop:
    cmp r9, r8
    jge tabs_done
    
    mov rcx, r9
    call tab_manager_get_tab
    mov rcx, [rax]
    lea rdx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rdx, rax
    call append_json_string
    lea rax, [layout_buffer]
    sub rdx, rax
    mov qword ptr [buffer_pos], rdx
    
    inc r9
    jmp tab_loop
    
tabs_done:
    ; Add user preferences
    mov edx, [g_current_theme]
    lea rcx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rcx, rax
    call append_json_int
    lea rax, [layout_buffer]
    sub rcx, rax
    mov qword ptr [buffer_pos], rcx
    
    mov edx, [g_font_size]
    lea rcx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rcx, rax
    call append_json_int
    lea rax, [layout_buffer]
    sub rcx, rax
    mov qword ptr [buffer_pos], rcx
    
    mov edx, [g_auto_save_interval]
    lea rcx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rcx, rax
    call append_json_int
    lea rax, [layout_buffer]
    sub rcx, rax
    mov qword ptr [buffer_pos], rcx
    
    ; Write footer
    lea rcx, [szLayoutFooter]
    lea rdx, [layout_buffer]
    mov rax, qword ptr [buffer_pos]
    add rdx, rax
    call append_json_string
    lea rax, [layout_buffer]
    sub rdx, rax
    mov qword ptr [buffer_pos], rdx
    
    ; Write buffer to file
    mov rcx, qword ptr [hLayoutFile]
    lea rdx, [layout_buffer]
    mov r8, qword ptr [buffer_pos]
    lea r9, [bytes_transferred]
    mov qword ptr [rsp+20h], 0      ; lpOverlapped
    call WriteFile
    
    ; Close file
    mov rcx, qword ptr [hLayoutFile]
    call CloseHandle
    
    ; Log success
    lea rcx, [szLayoutSaveSuccess]
    call asm_log
    
    mov eax, 1
    jmp json_save_done
    
json_create_failed:
    xor eax, eax
    
json_save_done:
    add rsp, 64
    pop rbp
    ret
save_layout_json ENDP

;==========================================================================
; load_layout_json - Load IDE layout from JSON file
; Returns: eax = 1 (success) or 0 (failure)
;==========================================================================
load_layout_json PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Open layout.json
    lea rcx, [LAYOUT_FILE]
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d                    ; lpSecurityAttributes
    mov dword ptr [rsp+20h], OPEN_EXISTING
    mov dword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    
    cmp rax, -1
    je json_load_failed
    mov qword ptr [hLayoutFile], rax
    
    ; Get file size
    mov rcx, qword ptr [hLayoutFile]
    lea rdx, [buffer_pos]
    call GetFileSize
    
    cmp rax, JSON_BUFFER_SIZE
    jge json_load_failed            ; File too large
    
    ; Read file
    mov rcx, qword ptr [hLayoutFile]
    lea rdx, [layout_buffer]
    mov r8, rax                     ; file size
    lea r9, [bytes_transferred]
    mov qword ptr [rsp+20h], 0      ; lpOverlapped
    call ReadFile
    
    test eax, eax
    jz json_load_failed
    
    ; Close file
    mov rcx, qword ptr [hLayoutFile]
    call CloseHandle
    
    ; TODO: Parse JSON and apply layout
    ; TODO: Restore window positions
    ; TODO: Restore pane visibility
    ; TODO: Restore tab configurations
    
    lea rcx, [szLayoutLoadSuccess]
    call asm_log
    
    mov eax, 1
    jmp json_load_done
    
json_load_failed:
    lea rcx, [szLayoutLoadFailed]
    call asm_log
    
    xor eax, eax
    
json_load_done:
    add rsp, 64
    pop rbp
    ret
load_layout_json ENDP

;==========================================================================
; save_settings_json - Save user preferences to JSON file
; Returns: eax = 1 (success) or 0 (failure)
;==========================================================================
save_settings_json PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; Similar structure to save_layout_json
    ; TODO: Implement settings persistence
    ; - Theme preference
    ; - Font family/size
    ; - Editor settings (tab size, wrap, etc)
    ; - Keyboard shortcuts
    ; - Extension settings
    
    mov eax, 1
    add rsp, 48
    pop rbp
    ret
save_settings_json ENDP

;==========================================================================
; load_settings_json - Load user preferences from JSON file
; Returns: eax = 1 (success) or 0 (failure)
;==========================================================================
load_settings_json PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; TODO: Implement settings loading
    
    mov eax, 1
    add rsp, 48
    pop rbp
    ret
load_settings_json ENDP

;==========================================================================
; append_json_string - Append string to JSON buffer
; rcx = string to append
; rdx = buffer position
; Returns: rdx = new buffer position
;==========================================================================
append_json_string PROC
    push rbp
    mov rbp, rsp
    
    mov rsi, rcx                    ; source string
    mov rdi, rdx                    ; dest buffer
    
append_loop:
    lodsb
    test al, al
    jz append_done
    mov [rdi], al
    inc rdi
    jmp append_loop
    
append_done:
    mov rdx, rdi
    pop rbp
    ret
append_json_string ENDP

;==========================================================================
; append_json_int - Append integer to JSON buffer
; rcx = buffer position
; edx = integer value
; Returns: rcx = new buffer position
;==========================================================================
append_json_int PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov eax, edx                    ; integer value
    
    ; Convert to string (simple stub)
    mov rdi, rcx
    
    ; Just write "0" for now
    mov byte ptr [rdi], '0'
    inc rdi
    
    mov rcx, rdi
    add rsp, 32
    pop rbp
    ret
append_json_int ENDP

;==========================================================================
; append_json_bool - Append boolean (true/false) to JSON buffer
; rcx = buffer position
; edx = boolean (0=false, nonzero=true)
; Returns: rcx = new buffer position
;==========================================================================
append_json_bool PROC
    push rbp
    mov rbp, rsp

    ; Preserve destination pointer (rcx)
    mov r10, rcx

    test edx, edx
    jz append_false

    lea rcx, [szTrue]
    mov rdx, r10
    call append_json_string
    mov rcx, rdx
    pop rbp
    ret

append_false:
    lea rcx, [szFalse]
    mov rdx, r10
    call append_json_string
    mov rcx, rdx
    pop rbp
    ret
append_json_bool ENDP

;==========================================================================
; append_json_value - Helper to append a JSON key-value pair
; rcx = key name
; rdx = value
; r8  = value type (0=string, 1=int, 2=bool)
;==========================================================================
append_json_value PROC
    push rbp
    mov rbp, rsp
    
    ; TODO: Implement generic JSON key-value appending
    
    pop rbp
    ret
append_json_value ENDP

END

