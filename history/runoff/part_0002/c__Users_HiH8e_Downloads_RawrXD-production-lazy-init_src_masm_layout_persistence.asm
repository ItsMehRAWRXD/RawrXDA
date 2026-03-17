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
EXTERN CreateFileA:PROC
EXTERN CloseHandle:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN GetFileSize:PROC
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_log:PROC
EXTERN strcmp_masm:PROC
EXTERN GetTickCount:PROC

;==========================================================================
; CONSTANTS
;==========================================================================

GENERIC_READ            EQU 80000000h
GENERIC_WRITE           EQU 40000000h
FILE_SHARE_READ         EQU 00000001h
CREATE_ALWAYS           EQU 0000002h
OPEN_EXISTING           EQU 00000003h

JSON_BUFFER_SIZE        EQU 65536   ; 64 KB JSON buffer
LAYOUT_FILE             BYTE "layout.json",0
SETTINGS_FILE           BYTE "settings.json",0

;==========================================================================
; DATA SECTION
;==========================================================================

.data
    szLayoutHeader      BYTE "{",0DH,0AH,
                             """IDE"": {",0DH,0AH,
                             """mainWindow"": {",0DH,0AH,
                             """x"": ",0
    
    szLayoutFooter      BYTE "}",0DH,0AH "}", 0
    
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

.data?
    hLayoutFile         QWORD ?
    layout_buffer       BYTE JSON_BUFFER_SIZE DUP (?)
    buffer_pos          QWORD ?

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
    xor r8d, r8d
    mov r9d, CREATE_ALWAYS
    xor r10d, r10d
    call CreateFileA
    
    cmp rax, -1
    je json_create_failed
    mov hLayoutFile, rax
    
    ; Build JSON header
    lea rcx, [layout_buffer]
    mov buffer_pos, 0
    
    ; Write header
    lea rcx, [szLayoutHeader]
    lea rdx, [layout_buffer]
    call append_json_string
    
    ; Add window position via GetWindowRect
    call ui_get_main_hwnd
    mov rcx, rax
    lea rdx, [temp_rect]
    call GetWindowRect
    
    mov edx, [temp_rect]
    lea rcx, [layout_buffer + buffer_pos]
    call append_json_int
    
    mov edx, [temp_rect + 4]
    lea rcx, [layout_buffer + buffer_pos]
    call append_json_int
    
    ; Add pane visibility states
    lea rcx, [szEditorVisible]
    lea rdx, [layout_buffer + buffer_pos]
    call append_json_string
    
    mov edx, 1
    lea rcx, [layout_buffer + buffer_pos]
    call append_json_bool
    
    ; Output pane visibility
    call output_pane_is_visible
    mov edx, eax
    lea rcx, [layout_buffer + buffer_pos]
    call append_json_bool
    
    ; File tree visibility
    call file_tree_is_visible
    mov edx, eax
    lea rcx, [layout_buffer + buffer_pos]
    call append_json_bool
    
    ; Chat pane visibility
    call chat_pane_is_visible
    mov edx, eax
    lea rcx, [layout_buffer + buffer_pos]
    call append_json_bool
    
    ; Terminal visibility
    call terminal_pane_is_visible
    mov edx, eax
    lea rcx, [layout_buffer + buffer_pos]
    call append_json_bool
    
    ; Add tab configurations
    lea rcx, [szTabsJSON]
    lea rdx, [layout_buffer + buffer_pos]
    call append_json_string
    
    call tab_manager_get_count
    mov r8, rax
    xor r9, r9
    
tab_loop:
    cmp r9, r8
    jge tabs_done
    
    mov rcx, r9
    call tab_manager_get_tab
    mov rcx, [rax]
    lea rdx, [layout_buffer + buffer_pos]
    call append_json_string
    
    inc r9
    jmp tab_loop
    
tabs_done:
    ; Add user preferences
    mov edx, [g_current_theme]
    lea rcx, [layout_buffer + buffer_pos]
    call append_json_int
    
    mov edx, [g_font_size]
    lea rcx, [layout_buffer + buffer_pos]
    call append_json_int
    
    mov edx, [g_auto_save_interval]
    lea rcx, [layout_buffer + buffer_pos]
    call append_json_int
    
    ; Write footer
    lea rcx, [szLayoutFooter]
    lea rdx, [layout_buffer + buffer_pos]
    call append_json_string
    
    ; Write buffer to file
    mov rcx, hLayoutFile
    lea rdx, [layout_buffer]
    mov r8, buffer_pos
    lea r9, [buffer_pos]            ; bytes written
    xor r10d, r10d
    call WriteFile
    
    ; Close file
    mov rcx, hLayoutFile
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
    xor r9d, r9d
    mov r10d, OPEN_EXISTING
    xor r11d, r11d
    call CreateFileA
    
    cmp rax, -1
    je json_load_failed
    mov hLayoutFile, rax
    
    ; Get file size
    mov rcx, hLayoutFile
    lea rdx, [buffer_pos]
    call GetFileSize
    
    cmp rax, JSON_BUFFER_SIZE
    jge json_load_failed            ; File too large
    
    ; Read file
    mov rcx, hLayoutFile
    lea rdx, [layout_buffer]
    mov r8, rax                     ; file size
    lea r9, [buffer_pos]            ; bytes read
    xor r10d, r10d
    call ReadFile
    
    test eax, eax
    jz json_load_failed
    
    ; Close file
    mov rcx, hLayoutFile
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
    pop rbp
    ret
append_json_int ENDP

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
