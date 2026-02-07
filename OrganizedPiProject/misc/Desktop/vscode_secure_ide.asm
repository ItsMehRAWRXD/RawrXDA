; VSCode-like Secure IDE in Assembly Language
; Enhanced with browser AI chat integration
; Compiled with NASM for x86-64 Linux

BITS 64
DEFAULT REL

; System calls
SYS_READ    equ 0
SYS_WRITE   equ 1
SYS_OPEN    equ 2
SYS_CLOSE   equ 3
SYS_EXIT    equ 60
SYS_FORK    equ 57
SYS_EXECVE  equ 59

; File descriptors
STDIN       equ 0
STDOUT      equ 1
STDERR      equ 2

; VSCode-like interface constants
SIDEBAR_WIDTH equ 40
EDITOR_WIDTH equ 80
TERMINAL_HEIGHT equ 15
STATUS_BAR_HEIGHT equ 1

section .data
    ; VSCode-like welcome
    ide_header db 27, '[2J', 27, '[H'  ; Clear screen
               db '┌─────────────────────────────────────────────────────────────────────────────┐', 10
               db '│ Secure IDE v2.0 - VSCode Style                                             │', 10
               db '│ Local AI + Browser Chat Integration                                        │', 10
               db '└─────────────────────────────────────────────────────────────────────────────┘', 10, 0
    ide_header_len equ $ - ide_header

    ; Sidebar menu
    sidebar_header db '┌─ EXPLORER ─────────────────────────┐', 10
                   db '│ 📁 workspace/                      │', 10
                   db '│   📄 main.js                       │', 10
                   db '│   📄 style.css                     │', 10
                   db '│   📄 index.html                    │', 10
                   db '├─ AI CHAT ─────────────────────────┤', 10
                   db '│ 🤖 ChatGPT                         │', 10
                   db '│ 🌟 Claude                          │', 10
                   db '│ 🚀 Kimi                            │', 10
                   db '│ 💎 Gemini                          │', 10
                   db '│ ⚡ Local AI                        │', 10
                   db '├─ COMMANDS ────────────────────────┤', 10
                   db '│ Ctrl+N  New File                   │', 10
                   db '│ Ctrl+O  Open File                  │', 10
                   db '│ Ctrl+S  Save File                  │', 10
                   db '│ Ctrl+`  Toggle Terminal            │', 10
                   db '│ Ctrl+Shift+P  Command Palette      │', 10
                   db '│ F1      AI Chat Menu               │', 10
                   db '└────────────────────────────────────┘', 10, 0
    sidebar_len equ $ - sidebar_header

    ; Editor area
    editor_header db '┌─ EDITOR ──────────────────────────────────────────────────────────────────┐', 10, 0
    editor_header_len equ $ - editor_header

    ; Status bar
    status_bar db '└─ Ln 1, Col 1 │ JavaScript │ UTF-8 │ Local AI: Ready │ 🔒 Secure ─────┘', 10, 0
    status_bar_len equ $ - status_bar

    ; AI Chat providers
    chatgpt_url db 'https://chat.openai.com', 0
    claude_url db 'https://claude.ai', 0
    kimi_url db 'https://kimi.moonshot.cn', 0
    gemini_url db 'https://gemini.google.com', 0
    
    ; Browser commands
    chrome_cmd db 'google-chrome', 0
    firefox_cmd db 'firefox', 0
    edge_cmd db 'microsoft-edge', 0

    ; Command palette
    palette_header db 27, '[2J', 27, '[H'
                   db '┌─ COMMAND PALETTE ──────────────────────────────────────────────────────────┐', 10
                   db '│ > Type command...                                                           │', 10
                   db '├─────────────────────────────────────────────────────────────────────────────┤', 10
                   db '│ 🤖 Open ChatGPT                                                            │', 10
                   db '│ 🌟 Open Claude                                                             │', 10
                   db '│ 🚀 Open Kimi                                                               │', 10
                   db '│ 💎 Open Gemini                                                             │', 10
                   db '│ ⚡ Local AI Chat                                                           │', 10
                   db '│ 📁 Open File                                                               │', 10
                   db '│ 💾 Save File                                                               │', 10
                   db '│ 🔍 Search Files                                                            │', 10
                   db '│ 🔧 Settings                                                                │', 10
                   db '│ 🔒 Security Status                                                         │', 10
                   db '└─────────────────────────────────────────────────────────────────────────────┘', 10, 0
    palette_len equ $ - palette_header

    ; Terminal interface
    terminal_header db '┌─ TERMINAL ─────────────────────────────────────────────────────────────────┐', 10
                    db '│ bash $ _                                                                    │', 10, 0
    terminal_header_len equ $ - terminal_header

    ; AI Chat interface
    local_ai_header db '┌─ LOCAL AI CHAT ────────────────────────────────────────────────────────────┐', 10
                    db '│ 🤖 Secure Local AI Assistant                                               │', 10
                    db '│ All processing happens locally - no data sent to external servers         │', 10
                    db '├─────────────────────────────────────────────────────────────────────────────┤', 10, 0
    local_ai_len equ $ - local_ai_header

    ; Browser launch messages
    launching_msg db 'Launching browser for AI chat...', 10, 0
    launching_len equ $ - launching_msg

    browser_error db 'Error: Could not launch browser', 10, 0
    browser_error_len equ $ - browser_error

section .bss
    input_buffer resb 256
    command_buffer resb 512
    file_buffer resb 4096
    current_mode resd 1  ; 0=menu, 1=editor, 2=terminal, 3=ai_chat, 4=palette
    cursor_x resd 1
    cursor_y resd 1
    terminal_visible resd 1

section .text
    global _start

_start:
    call init_vscode_ide
    call main_interface_loop

; Initialize VSCode-like IDE
init_vscode_ide:
    push rbp
    mov rbp, rsp
    
    ; Clear screen and show header
    mov rdi, STDOUT
    mov rsi, ide_header
    mov rdx, ide_header_len
    mov rax, SYS_WRITE
    syscall
    
    ; Initialize state
    mov dword [current_mode], 0
    mov dword [cursor_x], 1
    mov dword [cursor_y], 1
    mov dword [terminal_visible], 0
    
    pop rbp
    ret

; Main interface loop
main_interface_loop:
    push rbp
    mov rbp, rsp
    
.main_loop:
    call render_interface
    call handle_input
    jmp .main_loop

; Render the complete interface
render_interface:
    push rbp
    mov rbp, rsp
    
    ; Clear screen
    mov rdi, STDOUT
    mov rsi, ide_header
    mov rdx, ide_header_len
    mov rax, SYS_WRITE
    syscall
    
    ; Render sidebar
    call render_sidebar
    
    ; Render editor area
    call render_editor
    
    ; Render terminal if visible
    cmp dword [terminal_visible], 1
    je .show_terminal
    jmp .show_status
    
.show_terminal:
    call render_terminal
    
.show_status:
    call render_status_bar
    
    pop rbp
    ret

; Render sidebar
render_sidebar:
    push rbp
    mov rbp, rsp
    
    mov rdi, STDOUT
    mov rsi, sidebar_header
    mov rdx, sidebar_len
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Render editor area
render_editor:
    push rbp
    mov rbp, rsp
    
    mov rdi, STDOUT
    mov rsi, editor_header
    mov rdx, editor_header_len
    mov rax, SYS_WRITE
    syscall
    
    ; Show file content if loaded
    cmp qword [file_buffer], 0
    je .empty_editor
    
    mov rdi, STDOUT
    mov rsi, file_buffer
    mov rdx, 1024
    mov rax, SYS_WRITE
    syscall
    
.empty_editor:
    pop rbp
    ret

; Render terminal
render_terminal:
    push rbp
    mov rbp, rsp
    
    mov rdi, STDOUT
    mov rsi, terminal_header
    mov rdx, terminal_header_len
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Render status bar
render_status_bar:
    push rbp
    mov rbp, rsp
    
    mov rdi, STDOUT
    mov rsi, status_bar
    mov rdx, status_bar_len
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Handle keyboard input
handle_input:
    push rbp
    mov rbp, rsp
    
    ; Read input
    mov rdi, STDIN
    mov rsi, input_buffer
    mov rdx, 10
    mov rax, SYS_READ
    syscall
    
    ; Check for special keys
    mov al, [input_buffer]
    cmp al, 27  ; ESC sequence
    je .handle_escape
    
    cmp al, 1   ; Ctrl+A
    je .handle_ctrl_a
    
    cmp al, 14  ; Ctrl+N
    je .new_file
    
    cmp al, 15  ; Ctrl+O
    je .open_file
    
    cmp al, 19  ; Ctrl+S
    je .save_file
    
    cmp al, 96  ; Ctrl+` (backtick)
    je .toggle_terminal
    
    ; Check for F1 (AI Chat menu)
    cmp byte [input_buffer], 27
    jne .regular_input
    cmp byte [input_buffer+1], 79
    jne .regular_input
    cmp byte [input_buffer+2], 80
    je .show_ai_menu
    
.regular_input:
    ; Handle regular typing
    jmp .input_done

.handle_escape:
    ; Handle escape sequences
    jmp .input_done

.handle_ctrl_a:
    ; Select all
    jmp .input_done

.new_file:
    call handle_new_file
    jmp .input_done

.open_file:
    call handle_open_file
    jmp .input_done

.save_file:
    call handle_save_file
    jmp .input_done

.toggle_terminal:
    call toggle_terminal
    jmp .input_done

.show_ai_menu:
    call show_command_palette
    jmp .input_done

.input_done:
    pop rbp
    ret

; Show command palette
show_command_palette:
    push rbp
    mov rbp, rsp
    
    ; Display command palette
    mov rdi, STDOUT
    mov rsi, palette_header
    mov rdx, palette_len
    mov rax, SYS_WRITE
    syscall
    
    ; Get user choice
    mov rdi, STDIN
    mov rsi, input_buffer
    mov rdx, 10
    mov rax, SYS_READ
    syscall
    
    ; Process command
    mov al, [input_buffer]
    cmp al, '1'
    je .open_chatgpt
    cmp al, '2'
    je .open_claude
    cmp al, '3'
    je .open_kimi
    cmp al, '4'
    je .open_gemini
    cmp al, '5'
    je .local_ai_chat
    jmp .palette_done

.open_chatgpt:
    mov rdi, chatgpt_url
    call launch_browser
    jmp .palette_done

.open_claude:
    mov rdi, claude_url
    call launch_browser
    jmp .palette_done

.open_kimi:
    mov rdi, kimi_url
    call launch_browser
    jmp .palette_done

.open_gemini:
    mov rdi, gemini_url
    call launch_browser
    jmp .palette_done

.local_ai_chat:
    call start_local_ai_chat
    jmp .palette_done

.palette_done:
    pop rbp
    ret

; Launch browser with URL
launch_browser:
    push rbp
    mov rbp, rsp
    push rdi  ; Save URL
    
    ; Show launching message
    mov rdi, STDOUT
    mov rsi, launching_msg
    mov rdx, launching_len
    mov rax, SYS_WRITE
    syscall
    
    ; Fork process
    mov rax, SYS_FORK
    syscall
    
    cmp rax, 0
    je .child_process
    jmp .parent_process

.child_process:
    ; Try Chrome first
    pop rdi  ; Restore URL
    push rdi
    call try_launch_chrome
    cmp rax, 0
    je .launch_success
    
    ; Try Firefox
    pop rdi
    push rdi
    call try_launch_firefox
    cmp rax, 0
    je .launch_success
    
    ; Try Edge
    pop rdi
    call try_launch_edge
    
.launch_success:
    mov rdi, 0
    mov rax, SYS_EXIT
    syscall

.parent_process:
    pop rdi  ; Clean stack
    pop rbp
    ret

; Try launching Chrome
try_launch_chrome:
    push rbp
    mov rbp, rsp
    
    ; Prepare command: google-chrome URL
    mov rsi, command_buffer
    mov rdi, chrome_cmd
    call string_copy
    
    ; Add space and URL
    mov byte [rsi], ' '
    inc rsi
    pop rdi  ; Get URL
    push rdi
    call string_copy
    
    ; Execute
    mov rdi, command_buffer
    mov rsi, 0
    mov rdx, 0
    mov rax, SYS_EXECVE
    syscall
    
    pop rbp
    ret

; Try launching Firefox
try_launch_firefox:
    push rbp
    mov rbp, rsp
    
    ; Prepare command: firefox URL
    mov rsi, command_buffer
    mov rdi, firefox_cmd
    call string_copy
    
    ; Add space and URL
    mov byte [rsi], ' '
    inc rsi
    pop rdi  ; Get URL
    push rdi
    call string_copy
    
    ; Execute
    mov rdi, command_buffer
    mov rsi, 0
    mov rdx, 0
    mov rax, SYS_EXECVE
    syscall
    
    pop rbp
    ret

; Try launching Edge
try_launch_edge:
    push rbp
    mov rbp, rsp
    
    ; Prepare command: microsoft-edge URL
    mov rsi, command_buffer
    mov rdi, edge_cmd
    call string_copy
    
    ; Add space and URL
    mov byte [rsi], ' '
    inc rsi
    call string_copy  ; URL already in rdi
    
    ; Execute
    mov rdi, command_buffer
    mov rsi, 0
    mov rdx, 0
    mov rax, SYS_EXECVE
    syscall
    
    pop rbp
    ret

; Start local AI chat
start_local_ai_chat:
    push rbp
    mov rbp, rsp
    
    ; Display local AI interface
    mov rdi, STDOUT
    mov rsi, local_ai_header
    mov rdx, local_ai_len
    mov rax, SYS_WRITE
    syscall
    
    ; AI chat loop
.ai_chat_loop:
    ; Show prompt
    mov rdi, STDOUT
    mov rsi, .ai_prompt
    mov rdx, .ai_prompt_len
    mov rax, SYS_WRITE
    syscall
    
    ; Get user input
    mov rdi, STDIN
    mov rsi, input_buffer
    mov rdx, 256
    mov rax, SYS_READ
    syscall
    
    ; Check for exit
    mov rsi, input_buffer
    mov rdi, .exit_cmd
    call string_compare
    cmp rax, 0
    je .exit_ai_chat
    
    ; Process with local AI
    call process_local_ai
    
    jmp .ai_chat_loop

.exit_ai_chat:
    pop rbp
    ret

.ai_prompt db '│ You: ', 0
.ai_prompt_len equ $ - .ai_prompt
.exit_cmd db 'exit', 0

; Process local AI query
process_local_ai:
    push rbp
    mov rbp, rsp
    
    ; Simple local AI processing
    mov rdi, STDOUT
    mov rsi, .ai_thinking
    mov rdx, .ai_thinking_len
    mov rax, SYS_WRITE
    syscall
    
    ; Analyze input (simplified)
    call analyze_user_input
    
    ; Generate response
    call generate_local_response
    
    ; Display response
    mov rdi, STDOUT
    mov rsi, .ai_response_prefix
    mov rdx, .ai_response_prefix_len
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, STDOUT
    mov rsi, file_buffer
    mov rdx, 256
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

.ai_thinking db '│ 🤖 AI: Thinking...', 10, 0
.ai_thinking_len equ $ - .ai_thinking
.ai_response_prefix db '│ 🤖 AI: ', 0
.ai_response_prefix_len equ $ - .ai_response_prefix

; Analyze user input
analyze_user_input:
    push rbp
    mov rbp, rsp
    
    ; Simple keyword analysis
    mov rsi, input_buffer
    mov rdi, .code_keyword
    call string_contains
    cmp rax, 1
    je .code_query
    
    mov rsi, input_buffer
    mov rdi, .help_keyword
    call string_contains
    cmp rax, 1
    je .help_query
    
    jmp .general_query

.code_query:
    mov qword [file_buffer], .code_response
    jmp .analysis_done

.help_query:
    mov qword [file_buffer], .help_response
    jmp .analysis_done

.general_query:
    mov qword [file_buffer], .general_response

.analysis_done:
    pop rbp
    ret

.code_keyword db 'code', 0
.help_keyword db 'help', 0
.code_response db 'I can help you with code analysis, debugging, and optimization. Share your code!', 10, 0
.help_response db 'Available commands: help, code, debug, optimize, explain, refactor', 10, 0
.general_response db 'I understand. How can I assist you with your development work?', 10, 0

; Generate local AI response
generate_local_response:
    push rbp
    mov rbp, rsp
    
    ; Simple response generation based on analysis
    ; In a real implementation, this would use a local LLM
    
    pop rbp
    ret

; Toggle terminal visibility
toggle_terminal:
    push rbp
    mov rbp, rsp
    
    mov eax, [terminal_visible]
    xor eax, 1
    mov [terminal_visible], eax
    
    pop rbp
    ret

; Handle new file creation
handle_new_file:
    push rbp
    mov rbp, rsp
    
    ; Prompt for filename
    mov rdi, STDOUT
    mov rsi, .new_file_prompt
    mov rdx, .new_file_prompt_len
    mov rax, SYS_WRITE
    syscall
    
    ; Get filename
    mov rdi, STDIN
    mov rsi, input_buffer
    mov rdx, 256
    mov rax, SYS_READ
    syscall
    
    ; Create file (simplified)
    call create_new_file
    
    pop rbp
    ret

.new_file_prompt db 'Enter new filename: ', 0
.new_file_prompt_len equ $ - .new_file_prompt

; Handle file opening
handle_open_file:
    push rbp
    mov rbp, rsp
    
    ; Implementation similar to original but with VSCode-like interface
    call open_file_dialog
    
    pop rbp
    ret

; Handle file saving
handle_save_file:
    push rbp
    mov rbp, rsp
    
    ; Save current file
    call save_current_file
    
    pop rbp
    ret

; Create new file
create_new_file:
    push rbp
    mov rbp, rsp
    
    ; Create file with O_CREAT | O_WRONLY | O_TRUNC
    mov rdi, input_buffer
    mov rsi, 0x241
    mov rdx, 0644
    mov rax, SYS_OPEN
    syscall
    
    ; Close immediately (empty file)
    mov rdi, rax
    mov rax, SYS_CLOSE
    syscall
    
    pop rbp
    ret

; Open file dialog
open_file_dialog:
    push rbp
    mov rbp, rsp
    
    ; Simple file opening
    mov rdi, input_buffer
    mov rsi, 0  ; O_RDONLY
    mov rax, SYS_OPEN
    syscall
    
    cmp rax, 0
    jl .open_error
    
    ; Read file
    mov rdi, rax
    mov rsi, file_buffer
    mov rdx, 4096
    mov rax, SYS_READ
    syscall
    
    pop rbp
    ret

.open_error:
    pop rbp
    ret

; Save current file
save_current_file:
    push rbp
    mov rbp, rsp
    
    ; Implementation for saving
    
    pop rbp
    ret

; String utility functions
string_copy:
    push rbp
    mov rbp, rsp
    
.copy_loop:
    mov al, [rdi]
    mov [rsi], al
    cmp al, 0
    je .copy_done
    inc rdi
    inc rsi
    jmp .copy_loop

.copy_done:
    pop rbp
    ret

string_compare:
    push rbp
    mov rbp, rsp
    
.compare_loop:
    mov al, [rsi]
    mov bl, [rdi]
    cmp al, bl
    jne .not_equal
    cmp al, 0
    je .equal
    inc rsi
    inc rdi
    jmp .compare_loop

.not_equal:
    mov rax, 1
    pop rbp
    ret

.equal:
    mov rax, 0
    pop rbp
    ret

string_contains:
    push rbp
    mov rbp, rsp
    
    ; Simple substring search
    mov rax, 0  ; Default: not found
    
    pop rbp
    ret

; Exit IDE
exit_ide:
    mov rdi, 0
    mov rax, SYS_EXIT
    syscall