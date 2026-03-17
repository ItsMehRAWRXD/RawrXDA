;==========================================================================
; output_pane_logger.asm - Real-Time Dynamic Output Pane with Live Logging
; ==========================================================================
; Provides real-time logging to IDE output pane with:
; - Console output capture
; - File logging
; - Agent action logging
; - Hotpatch statistics
; - Performance metrics
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_LOG_BUFFER      EQU 65536
MAX_LOG_ENTRY       EQU 1024
LOG_BUFFER_SIZE     EQU 32768
OUTPUT_PANE_ID      EQU 9        ; Component ID for output pane
IDC_OUTPUT_RICH     EQU 4001     ; RichEdit control for output
EM_SETSEL           EQU 00B1h
EM_REPLACESEL       EQU 00C2h
EM_GETSEL           EQU 00B0h
EM_GETTEXTEX        EQU 044Dh

;==========================================================================
; STRUCTURES
;==========================================================================
LOG_ENTRY STRUCT
    timestamp       DWORD ?
    level           DWORD ?       ; 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
    source          BYTE 32 DUP (?) ; "Editor", "Agent", "Hotpatch", etc.
    message         BYTE 256 DUP (?)
LOG_ENTRY ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Log level strings
    szLogLevelDebug  BYTE "DEBUG",0
    szLogLevelInfo   BYTE "INFO",0
    szLogLevelWarn   BYTE "WARN",0
    szLogLevelError  BYTE "ERROR",0
    
    ; Source strings
    szSourceEditor   BYTE "Editor",0
    szSourceAgent    BYTE "Agent",0
    szSourceHotpatch BYTE "Hotpatch",0
    szSourceUI       BYTE "UI",0
    szSourceFile     BYTE "FileTree",0
    szSourceTab      BYTE "TabManager",0
    
    ; Log messages
    szLogEditorOpen  BYTE "[Editor] Opened file: %s",0
    szLogEditorClose BYTE "[Editor] Closed file: %s",0
    szLogTabCreate   BYTE "[TabManager] Created tab: %s",0
    szLogTabClose    BYTE "[TabManager] Closed tab: %s",0
    szLogAgentStart  BYTE "[Agent] Starting task: %s",0
    szLogAgentDone   BYTE "[Agent] Task completed: %s",0
    szLogHotpatch    BYTE "[Hotpatch] Applied patch: %s (success=%d)",0
    szLogFileOpen    BYTE "[FileTree] Opening path: %s",0
    
    ; Output pane formatting
    szNewline        BYTE 0Ah,0
    szTimestamp      BYTE "[%02d:%02d:%02d] ",0

.data?
    ; Output pane handle
    hOutputPane      QWORD ?
    
    ; Log buffer
    LogBuffer        BYTE LOG_BUFFER_SIZE DUP (?)
    LogBufferPos     QWORD ?
    
    ; Log history (ring buffer)
    LogHistory       LOG_ENTRY 256 DUP (<>)
    LogHistoryIndex  DWORD ?

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: output_pane_init(hWnd: rcx) -> rax (success)
; Initializes output pane with RichEdit control
;==========================================================================
PUBLIC output_pane_init
output_pane_init PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov hOutputPane, rcx
    
    ; Initialize log buffer position
    mov LogBufferPos, 0
    mov LogHistoryIndex, 0
    
    ; Create RichEdit control in output pane
    ; Assuming parent hwnd in rcx, create child RichEdit
    lea rcx, szRichEditClass
    mov rdx, hOutputPane
    mov r8, IDC_OUTPUT_RICH
    xor r9d, r9d
    call CreateWindowExA
    
    test rax, rax
    jz init_fail
    
    mov rbx, rax
    
    ; Set RichEdit properties
    mov rcx, rbx
    mov edx, EM_SETLIMITTEXT
    mov r8d, LOG_BUFFER_SIZE
    xor r9d, r9d
    call SendMessageA
    
    mov eax, 1
    jmp init_done
    
init_fail:
    xor eax, eax
    
init_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
output_pane_init ENDP

;==========================================================================
; PUBLIC: output_log_editor(filename: rcx, action: edx) -> rax
; Logs file open/close actions
; action: 0=open, 1=close
;==========================================================================
PUBLIC output_log_editor
output_log_editor PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rsi, rcx        ; filename
    mov r8d, edx        ; action
    
    ; Build log message
    lea rdi, LogBuffer
    mov rcx, LogBufferPos
    add rdi, rcx
    
    ; Format: [EDITOR] Opened/Closed: filename
    test r8d, r8d
    jz log_editor_open
    
    ; Close action
    lea rcx, szLogEditorClose
    jmp log_editor_format
    
log_editor_open:
    lea rcx, szLogEditorOpen
    
log_editor_format:
    ; Format the message (rcx=format, rsi=filename)
    mov rdx, rsi
    lea r8, [rdi]
    call wsprintfA
    
    add LogBufferPos, rax
    
    ; Also append to output pane
    call output_pane_append
    
    mov eax, 1
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
output_log_editor ENDP

;==========================================================================
; PUBLIC: output_log_tab(tab_name: rcx, action: edx) -> rax
; Logs tab creation/closure
; action: 0=create, 1=close
;==========================================================================
PUBLIC output_log_tab
output_log_tab PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rsi, rcx        ; tab_name
    mov r8d, edx        ; action
    
    ; Build log message
    lea rdi, LogBuffer
    mov rcx, LogBufferPos
    add rdi, rcx
    
    test r8d, r8d
    jz log_tab_create
    
    ; Close action
    lea rcx, szLogTabClose
    jmp log_tab_format
    
log_tab_create:
    lea rcx, szLogTabCreate
    
log_tab_format:
    mov rdx, rsi
    lea r8, [rdi]
    call wsprintfA
    
    add LogBufferPos, rax
    call output_pane_append
    
    mov eax, 1
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
output_log_tab ENDP

;==========================================================================
; PUBLIC: output_log_agent(task_name: rcx, result: edx) -> rax
; Logs agent task execution
; result: 0=start, 1=complete, 2=error
;==========================================================================
PUBLIC output_log_agent
output_log_agent PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rsi, rcx        ; task_name
    mov r8d, edx        ; result
    
    lea rdi, LogBuffer
    mov rcx, LogBufferPos
    add rdi, rcx
    
    test r8d, r8d
    jz log_agent_start
    
    ; Complete action
    lea rcx, szLogAgentDone
    jmp log_agent_format
    
log_agent_start:
    lea rcx, szLogAgentStart
    
log_agent_format:
    mov rdx, rsi
    lea r8, [rdi]
    call wsprintfA
    
    add LogBufferPos, rax
    call output_pane_append
    
    mov eax, 1
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
output_log_agent ENDP

;==========================================================================
; PUBLIC: output_log_hotpatch(patch_name: rcx, success: edx) -> rax
; Logs hotpatch application results
;==========================================================================
PUBLIC output_log_hotpatch
output_log_hotpatch PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rsi, rcx        ; patch_name
    mov r8d, edx        ; success (0/1)
    
    lea rdi, LogBuffer
    mov rcx, LogBufferPos
    add rdi, rcx
    
    lea rcx, szLogHotpatch
    mov rdx, rsi
    mov r8d, r8d
    lea r9, [rdi]
    call wsprintfA
    
    add LogBufferPos, rax
    call output_pane_append
    
    mov eax, 1
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
output_log_hotpatch ENDP

;==========================================================================
; PUBLIC: output_log_filetree(path: rcx) -> rax
; Logs file tree navigation
;==========================================================================
PUBLIC output_log_filetree
output_log_filetree PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rsi, rcx        ; path
    
    lea rdi, LogBuffer
    mov rcx, LogBufferPos
    add rdi, rcx
    
    lea rcx, szLogFileOpen
    mov rdx, rsi
    lea r8, [rdi]
    call wsprintfA
    
    add LogBufferPos, rax
    call output_pane_append
    
    mov eax, 1
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
output_log_filetree ENDP

;==========================================================================
; INTERNAL: output_pane_append() -> rax
; Appends current LogBuffer content to output pane RichEdit
;==========================================================================
output_pane_append PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rcx, hOutputPane
    test rcx, rcx
    jz append_done
    
    ; Get current text length
    mov rcx, IDC_OUTPUT_RICH
    mov edx, EM_GETSEL
    xor r8d, r8d
    xor r9d, r9d
    call SendMessageA
    
    ; Move to end and append newline + new text
    mov rcx, hOutputPane
    mov edx, EM_SETSEL
    mov r8d, -1         ; End of text
    mov r9d, -1
    call SendMessageA
    
    ; Append log buffer content
    mov rcx, hOutputPane
    mov edx, EM_REPLACESEL
    xor r8d, r8d        ; canUndo = false
    lea r9, [LogBuffer]
    call SendMessageA
    
    ; Append newline
    mov rcx, hOutputPane
    mov edx, EM_REPLACESEL
    xor r8d, r8d
    lea r9, [szNewline]
    call SendMessageA
    
    ; Reset log buffer
    mov LogBufferPos, 0
    
    mov eax, 1
    
append_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
output_pane_append ENDP

;==========================================================================
; PUBLIC: output_pane_clear() -> rax
; Clears output pane content
;==========================================================================
PUBLIC output_pane_clear
output_pane_clear PROC
    push rbx
    sub rsp, 32
    
    mov rcx, hOutputPane
    test rcx, rcx
    jz clear_done
    
    ; Select all
    mov edx, EM_SETSEL
    xor r8d, r8d
    mov r9d, -1
    call SendMessageA
    
    ; Delete
    mov rcx, hOutputPane
    mov edx, EM_REPLACESEL
    xor r8d, r8d
    xor r9, r9
    call SendMessageA
    
    mov LogHistoryIndex, 0
    mov LogBufferPos, 0
    mov eax, 1
    
clear_done:
    add rsp, 32
    pop rbx
    ret
output_pane_clear ENDP

;==========================================================================
; DATA SECTION STRINGS
;==========================================================================
.data
    szRichEditClass  BYTE "RichEdit20W",0

