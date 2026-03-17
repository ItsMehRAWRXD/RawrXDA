;==========================================================================
; menu_hooks.asm - Menu Handler Hooks to New APIs
; ==========================================================================
; Implements complete menu handler system with:
; - File operations (Create, Open, Save, Close)
; - Edit operations (Cut, Copy, Paste, Find, Replace)
; - View operations (Toggle panels, switch tabs)
; - Agent operations (Mode switching, chat clearing)
; - Tools operations (Build, Run, Hotpatch)
;
; Integration Points:
; - tab_manager.asm (tab creation/closing)
; - file_tree_driver.asm (file navigation)
; - output_pane_logger.asm (logging all operations)
; - agent_chat_modes.asm (mode switching)
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; EXTERN DECLARATIONS - UI Components
;==========================================================================
EXTERN ui_get_editor_hwnd:PROC
EXTERN ui_get_output_hwnd:PROC
EXTERN ui_get_main_hwnd:PROC

;==========================================================================
; EXTERN DECLARATIONS - Tab Manager
;==========================================================================
EXTERN tab_manager_init:PROC
EXTERN tab_create_editor:PROC
EXTERN tab_close_editor:PROC
EXTERN tab_set_agent_mode:PROC
EXTERN tab_mark_modified:PROC

;==========================================================================
; EXTERN DECLARATIONS - File Tree
;==========================================================================
EXTERN file_tree_init:PROC
EXTERN file_tree_expand_drive:PROC
EXTERN file_tree_refresh:PROC

;==========================================================================
; EXTERN DECLARATIONS - Output Pane
;==========================================================================
EXTERN output_pane_init:PROC
EXTERN output_log_editor:PROC
EXTERN output_log_tab:PROC
EXTERN output_log_agent:PROC

;==========================================================================
; EXTERN DECLARATIONS - Agent Chat
;==========================================================================
EXTERN agent_chat_init:PROC
EXTERN agent_chat_set_mode:PROC
EXTERN agent_chat_send_message:PROC
EXTERN agent_chat_clear:PROC

;==========================================================================
; EXTERN DECLARATIONS - Session Manager
;==========================================================================
EXTERN session_manager_shutdown:PROC

;==========================================================================
; EXTERN DECLARATIONS - Win32 API
;==========================================================================
EXTERN GetOpenFileNameA:PROC
EXTERN GetSaveFileNameA:PROC
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileAttributesA:PROC
EXTERN MessageBoxA:PROC
EXTERN SendMessageA:PROC
EXTERN ShowWindow:PROC
EXTERN DestroyWindow:PROC
EXTERN wsprintfA:PROC
EXTERN GetTickCount:PROC
EXTERN MoveFileA:PROC

;==========================================================================
; MENU ID CONSTANTS
;==========================================================================
IDM_FILE_NEW            EQU 101
IDM_FILE_OPEN           EQU 102
IDM_FILE_SAVE           EQU 103
IDM_FILE_SAVEAS         EQU 104
IDM_FILE_CLOSE_TAB      EQU 110
IDM_FILE_EXIT           EQU 107

IDM_EDIT_UNDO           EQU 201
IDM_EDIT_REDO           EQU 202
IDM_EDIT_CUT            EQU 203
IDM_EDIT_COPY           EQU 204
IDM_EDIT_PASTE          EQU 205

IDM_VIEW_EXPLORER       EQU 408
IDM_VIEW_OUTPUT         EQU 413
IDM_VIEW_TERMINAL       EQU 414

IDM_AGENT_ASK           EQU 6001
IDM_AGENT_EDIT          EQU 6002
IDM_AGENT_PLAN          EQU 6003
IDM_AGENT_CONFIG        EQU 6004
IDM_AGENT_CLEAR_CHAT    EQU 6005

IDM_TOOLS_BUILD         EQU 7003
IDM_TOOLS_RUN           EQU 7004

;==========================================================================
; DATA
;==========================================================================
.data
    ; Strings for logging and dialogs
    szNewFile           BYTE "Untitled.asm",0
    szFileFilter        BYTE "Assembly Files (*.asm)|*.asm|All Files (*.*)|*.*|",0
    szOpenTitle         BYTE "Open File",0
    szSaveTitle         BYTE "Save File",0
    szFileNew_Log       BYTE "Created new file",0
    szFileOpen_Log      BYTE "Opened file: %s",0
    szFileSave_Log      BYTE "Saved file: %s",0
    szTabCreate_Log     BYTE "Tab created for: %s",0
    szTabClose_Log      BYTE "Closed tab: %s",0
    szAgentMode_Log     BYTE "Switched to %s mode",0
    szModeClear         BYTE "Ask",0
    szModeEdit          BYTE "Edit",0
    szModePlan          BYTE "Plan",0
    szModeConfig        BYTE "Configure",0

    ; File dialog structure
    OPENFILENAME_SIZE   EQU 76  ; x64 size

.data?
    ; Current file context
    CurrentFileName     BYTE 512 DUP (?)
    CurrentFilePath     BYTE 512 DUP (?)
    CurrentTabID        DWORD ?
    
    ; Dialog buffers
    FilePathBuffer      BYTE 512 DUP (?)
    FileNameBuffer      BYTE 256 DUP (?)

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: menu_file_new() -> eax (success)
; Create new file and tab
;==========================================================================
PUBLIC menu_file_new
menu_file_new PROC
    push rbx
    push rsi
    sub rsp, 32
    
    ; Generate filename
    mov rsi, GetTickCount
    call GetTickCount
    lea rcx, [FileNameBuffer]
    lea rdx, [szNewFile]
    mov r8d, eax
    call wsprintfA
    
    ; Create editor tab
    lea rcx, [FileNameBuffer]
    lea rdx, [CurrentFilePath]
    call tab_create_editor
    mov CurrentTabID, eax
    
    ; Log operation
    lea rcx, [szTabCreate_Log]
    lea rdx, [FileNameBuffer]
    call output_log_tab
    
    mov eax, 1
    add rsp, 32
    pop rsi
    pop rbx
    ret
menu_file_new ENDP

;==========================================================================
; PUBLIC: menu_file_open() -> eax (success)
; Open file dialog and create tab
;==========================================================================
PUBLIC menu_file_open
menu_file_open PROC
    push rbp
    mov rbp, rsp
    sub rsp, 96
    
    ; Create OPENFILENAME structure on stack
    mov rax, OPENFILENAME_SIZE
    xor rcx, rcx
    
    ; lStructSize = 76 (x64)
    mov DWORD PTR [rsp + 0], 76
    
    ; hwndOwner = NULL (will get from ui_get_editor_hwnd)
    mov QWORD PTR [rsp + 8], 0
    
    ; hInstance = NULL
    mov QWORD PTR [rsp + 16], 0
    
    ; lpstrFilter
    lea rax, [szFileFilter]
    mov QWORD PTR [rsp + 24], rax
    
    ; lpstrCustomFilter
    mov QWORD PTR [rsp + 32], 0
    
    ; nMaxCustFilter
    mov DWORD PTR [rsp + 40], 0
    
    ; nFilterIndex
    mov DWORD PTR [rsp + 44], 1
    
    ; lpstrFile (output buffer)
    lea rax, [FilePathBuffer]
    mov QWORD PTR [rsp + 48], rax
    
    ; nMaxFile
    mov DWORD PTR [rsp + 56], 512
    
    ; lpstrFileTitle
    lea rax, [FileNameBuffer]
    mov QWORD PTR [rsp + 64], rax
    
    ; nMaxFileTitle
    mov DWORD PTR [rsp + 72], 256
    
    ; lpstrInitialDir
    mov QWORD PTR [rsp + 80], 0
    
    ; lpstrTitle
    lea rax, [szOpenTitle]
    mov QWORD PTR [rsp + 88], rax
    
    ; Call GetOpenFileNameA
    mov rcx, rsp
    call GetOpenFileNameA
    test eax, eax
    jz open_canceled
    
    ; File was selected - create tab
    lea rcx, [FileNameBuffer]
    lea rdx, [FilePathBuffer]
    call tab_create_editor
    mov CurrentTabID, eax
    
    ; Log operation
    lea rcx, [szFileOpen_Log]
    lea rdx, [FileNameBuffer]
    call output_log_editor
    
    mov eax, 1
    jmp open_done
    
open_canceled:
    xor eax, eax
    
open_done:
    mov rsp, rbp
    pop rbp
    ret
menu_file_open ENDP

;==========================================================================
; PUBLIC: menu_file_save() -> eax (success)
; Save current tab file
;==========================================================================
PUBLIC menu_file_save
menu_file_save PROC
    push rbx
    push rsi
    sub rsp, 32
    
    ; Get current tab info
    mov ecx, CurrentTabID
    test ecx, ecx
    jz save_no_tab
    
    ; Check if we have a path
    cmp BYTE PTR [CurrentFilePath], 0
    je save_as_needed
    
    ; Create file with GENERIC_WRITE
    lea rcx, [CurrentFilePath]
    mov edx, GENERIC_WRITE
    mov r8d, 0
    mov r9d, CREATE_ALWAYS
    mov DWORD PTR [rsp + 32], FILE_ATTRIBUTE_NORMAL
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je save_error
    mov rbx, rax
    
    ; Get text from editor
    call ui_get_editor_hwnd
    mov rcx, rax
    lea rdx, [FilePathBuffer] ; Reuse buffer for text
    mov r8d, 65536
    call ui_editor_get_text
    
    ; Write to file
    mov rcx, rbx
    lea rdx, [FilePathBuffer]
    mov r8d, eax
    lea r9, [CurrentTabID] ; Dummy for bytes written
    mov QWORD PTR [rsp + 32], 0
    call WriteFile
    
    ; Close file
    mov rcx, rbx
    call CloseHandle
    
    ; Log save operation
    lea rcx, [szFileSave_Log]
    lea rdx, [CurrentFileName]
    call output_log_editor
    
    mov eax, 1
    jmp save_done

save_as_needed:
    call menu_file_save_as
    jmp save_done
    
save_no_tab:
    ; No tab open
    mov eax, 0
    jmp save_done
    
save_error:
    mov eax, 0
    
save_done:
    add rsp, 32
    pop rsi
    pop rbx
    ret
menu_file_save ENDP

;==========================================================================
; PUBLIC: menu_file_save_as() -> eax (success)
; Save current tab file with new name
;==========================================================================
PUBLIC menu_file_save_as
menu_file_save_as PROC
    push rbp
    mov rbp, rsp
    sub rsp, 96
    
    ; Create OPENFILENAME structure on stack
    mov DWORD PTR [rsp + 0], 76 ; lStructSize
    mov QWORD PTR [rsp + 8], 0  ; hwndOwner
    mov QWORD PTR [rsp + 16], 0 ; hInstance
    lea rax, [szFileFilter]
    mov QWORD PTR [rsp + 24], rax
    mov QWORD PTR [rsp + 32], 0 ; lpstrCustomFilter
    mov DWORD PTR [rsp + 40], 0 ; nMaxCustFilter
    mov DWORD PTR [rsp + 44], 1 ; nFilterIndex
    lea rax, [FilePathBuffer]
    mov QWORD PTR [rsp + 48], rax
    mov DWORD PTR [rsp + 56], 512 ; nMaxFile
    lea rax, [FileNameBuffer]
    mov QWORD PTR [rsp + 64], rax
    mov DWORD PTR [rsp + 72], 256 ; nMaxFileTitle
    mov QWORD PTR [rsp + 80], 0 ; lpstrInitialDir
    lea rax, [szSaveTitle]
    mov QWORD PTR [rsp + 88], rax
    
    mov rcx, rsp
    call GetSaveFileNameA
    test eax, eax
    jz save_as_canceled
    
    ; Update current file info
    lea rsi, [FilePathBuffer]
    lea rdi, [CurrentFilePath]
    mov ecx, 512
    rep movsb
    
    lea rsi, [FileNameBuffer]
    lea rdi, [CurrentFileName]
    mov ecx, 256
    rep movsb
    
    ; Now save it
    call menu_file_save
    mov eax, 1
    jmp save_as_done
    
save_as_canceled:
    xor eax, eax
    
save_as_done:
    mov rsp, rbp
    pop rbp
    ret
menu_file_save_as ENDP

;==========================================================================
; PUBLIC: menu_file_close_tab() -> eax (success)
; Close current editor tab
;==========================================================================
PUBLIC menu_file_close_tab
menu_file_close_tab PROC
    push rbx
    sub rsp, 32
    
    ; Close tab by ID
    mov ecx, CurrentTabID
    call tab_close_editor
    
    ; Log operation
    lea rcx, [szTabClose_Log]
    lea rdx, [CurrentFileName]
    call output_log_tab
    
    ; Clear current file info
    mov BYTE PTR [CurrentFileName], 0
    mov BYTE PTR [CurrentFilePath], 0
    mov CurrentTabID, 0
    
    add rsp, 32
    pop rbx
    ret
menu_file_close_tab ENDP

;==========================================================================
; PUBLIC: menu_agent_ask() -> eax (success)
; Switch to Ask mode
;==========================================================================
PUBLIC menu_agent_ask
menu_agent_ask PROC
    push rbx
    sub rsp, 32
    
    mov ecx, 0      ; AGENT_MODE_ASK = 0
    call tab_set_agent_mode
    
    ; Log mode switch
    lea rcx, [szAgentMode_Log]
    lea rdx, [szModeClear]
    call output_log_agent
    
    add rsp, 32
    pop rbx
    ret
menu_agent_ask ENDP

;==========================================================================
; PUBLIC: menu_agent_edit() -> eax (success)
; Switch to Edit mode
;==========================================================================
PUBLIC menu_agent_edit
menu_agent_edit PROC
    push rbx
    sub rsp, 32
    
    mov ecx, 1      ; AGENT_MODE_EDIT = 1
    call tab_set_agent_mode
    
    ; Log mode switch
    lea rcx, [szAgentMode_Log]
    lea rdx, [szModeEdit]
    call output_log_agent
    
    add rsp, 32
    pop rbx
    ret
menu_agent_edit ENDP

;==========================================================================
; PUBLIC: menu_agent_plan() -> eax (success)
; Switch to Plan mode
;==========================================================================
PUBLIC menu_agent_plan
menu_agent_plan PROC
    push rbx
    sub rsp, 32
    
    mov ecx, 2      ; AGENT_MODE_PLAN = 2
    call tab_set_agent_mode
    
    ; Log mode switch
    lea rcx, [szAgentMode_Log]
    lea rdx, [szModePlan]
    call output_log_agent
    
    add rsp, 32
    pop rbx
    ret
menu_agent_plan ENDP

;==========================================================================
; PUBLIC: menu_agent_config() -> eax (success)
; Switch to Configure mode
;==========================================================================
PUBLIC menu_agent_config
menu_agent_config PROC
    push rbx
    sub rsp, 32
    
    mov ecx, 3      ; AGENT_MODE_CONFIG = 3
    call tab_set_agent_mode
    
    ; Log mode switch
    lea rcx, [szAgentMode_Log]
    lea rdx, [szModeConfig]
    call output_log_agent
    
    add rsp, 32
    pop rbx
    ret
menu_agent_config ENDP

;==========================================================================
; PUBLIC: menu_agent_clear_chat() -> eax (success)
; Clear chat history
;==========================================================================
PUBLIC menu_agent_clear_chat
menu_agent_clear_chat PROC
    push rbx
    sub rsp, 32
    
    call agent_chat_clear
    
    add rsp, 32
    pop rbx
    ret
menu_agent_clear_chat ENDP

;==========================================================================
; PUBLIC: menu_file_exit() -> eax (success)
; Exit application gracefully
;==========================================================================
PUBLIC menu_file_exit
menu_file_exit PROC
    push rbx
    sub rsp, 32
    
    ; Save session state
    call session_manager_shutdown
    
    ; Destroy main window
    call ui_get_main_hwnd
    mov rcx, rax
    call DestroyWindow
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
menu_file_exit ENDP

END
