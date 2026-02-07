; Main Assembly Text Editor with GUI Framework
; Complete text editor with modern GUI features

BITS 64
DEFAULT REL

; GUI Constants
%define WINDOW_WIDTH 1400
%define WINDOW_HEIGHT 900
%define MENU_HEIGHT 40
%define TOOLBAR_HEIGHT 50
%define STATUS_HEIGHT 30
%define SIDEBAR_WIDTH 300
%define EDITOR_WIDTH (WINDOW_WIDTH - SIDEBAR_WIDTH)

; System calls
%define SYS_READ 0
%define SYS_WRITE 1
%define SYS_OPEN 2
%define SYS_CLOSE 3
%define SYS_EXIT 60
%define SYS_MMAP 9
%define SYS_MUNMAP 11
%define SYS_POLL 7

section .data
    ; GUI Framework Data
    gui_title db 'Assembly Text Editor v2.0 - Modern GUI Framework', 0
    gui_title_len equ $ - gui_title - 1
    
    ; Menu System
    menu_file db 'File', 0
    menu_edit db 'Edit', 0
    menu_view db 'View', 0
    menu_tools db 'Tools', 0
    menu_help db 'Help', 0
    
    ; File Menu
    file_new db 'New', 0
    file_open db 'Open', 0
    file_save db 'Save', 0
    file_save_as db 'Save As', 0
    file_exit db 'Exit', 0
    
    ; Edit Menu
    edit_undo db 'Undo', 0
    edit_redo db 'Redo', 0
    edit_cut db 'Cut', 0
    edit_copy db 'Copy', 0
    edit_paste db 'Paste', 0
    edit_find db 'Find', 0
    edit_replace db 'Replace', 0
    
    ; View Menu
    view_line_numbers db 'Line Numbers', 0
    view_word_wrap db 'Word Wrap', 0
    view_syntax_highlighting db 'Syntax Highlighting', 0
    view_sidebar db 'Sidebar', 0
    
    ; Tools Menu
    tools_compile db 'Compile', 0
    tools_run db 'Run', 0
    tools_debug db 'Debug', 0
    tools_format db 'Format Code', 0
    
    ; GUI Elements
    toolbar_new db '[New] ', 0
    toolbar_open db '[Open] ', 0
    toolbar_save db '[Save] ', 0
    toolbar_compile db '[Compile] ', 0
    toolbar_run db '[Run] ', 0
    toolbar_debug db '[Debug] ', 0
    
    ; Status Messages
    status_ready db 'Ready', 0
    status_modified db 'Modified', 0
    status_compiling db 'Compiling...', 0
    status_running db 'Running...', 0
    
    ; Editor State
    current_filename times 256 db 0
    editor_buffer times 2097152 db 0    ; 2MB buffer
    editor_size dq 0
    cursor_x dq 0
    cursor_y dq 0
    scroll_x dq 0
    scroll_y dq 0
    
    ; GUI State
    window_active db 1
    menu_active db 0
    sidebar_visible db 1
    line_numbers_enabled db 1
    syntax_highlighting_enabled db 1
    word_wrap_enabled db 0
    
    ; Input/Output
    input_buffer times 1024 db 0
    output_buffer times 8192 db 0

section .text
    global _start

_start:
    call initialize_gui
    call main_gui_loop
    call cleanup_gui
    mov rdi, 0
    mov rax, SYS_EXIT
    syscall

; Initialize GUI
initialize_gui:
    push rbp
    mov rbp, rsp
    
    call clear_screen
    call display_welcome
    
    ; Initialize editor state
    mov qword [editor_size], 0
    mov qword [cursor_x], 0
    mov qword [cursor_y], 0
    mov qword [scroll_x], 0
    mov qword [scroll_y], 0
    
    pop rbp
    ret

; Main GUI loop
main_gui_loop:
    push rbp
    mov rbp, rsp
    
.loop:
    call display_gui
    call handle_input
    cmp byte [window_active], 0
    jne .loop
    
    pop rbp
    ret

; Display complete GUI
display_gui:
    push rbp
    mov rbp, rsp
    
    call clear_screen
    call display_menu_bar
    call display_toolbar
    call display_main_area
    call display_status_bar
    
    pop rbp
    ret

; Display menu bar
display_menu_bar:
    push rbp
    mov rbp, rsp
    
    mov rdi, 1
    mov rsi, menu_file
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, menu_edit
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, menu_view
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, menu_tools
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, menu_help
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, newline
    mov rdx, 1
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Display toolbar
display_toolbar:
    push rbp
    mov rbp, rsp
    
    mov rdi, 1
    mov rsi, toolbar_new
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, toolbar_open
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, toolbar_save
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, toolbar_compile
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, toolbar_run
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, toolbar_debug
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, newline
    mov rdx, 1
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Display main area
display_main_area:
    push rbp
    mov rbp, rsp
    
    cmp byte [sidebar_visible], 0
    je .no_sidebar
    
    call display_sidebar
    
.no_sidebar:
    call display_editor
    
    pop rbp
    ret

; Display sidebar
display_sidebar:
    push rbp
    mov rbp, rsp
    
    mov rdi, 1
    mov rsi, sidebar_title
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, sidebar_files
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Display editor
display_editor:
    push rbp
    mov rbp, rsp
    
    cmp byte [line_numbers_enabled], 0
    je .no_line_numbers
    
    call display_line_numbers
    
.no_line_numbers:
    cmp byte [syntax_highlighting_enabled], 0
    je .no_syntax_highlighting
    
    call display_syntax_highlighted_text
    jmp .editor_done
    
.no_syntax_highlighting:
    call display_plain_text
    
.editor_done:
    pop rbp
    ret

; Display line numbers
display_line_numbers:
    push rbp
    mov rbp, rsp
    
    ; Calculate line count
    mov rsi, editor_buffer
    mov rcx, qword [editor_size]
    mov rbx, 0
    
.line_count_loop:
    cmp rcx, 0
    je .line_count_done
    
    cmp byte [rsi], 10
    jne .not_newline
    inc rbx
    
.not_newline:
    inc rsi
    dec rcx
    jmp .line_count_loop
    
.line_count_done:
    ; Display line numbers
    mov rcx, rbx
    mov rbx, 1
    
.display_line_loop:
    cmp rbx, rcx
    jg .display_line_done
    
    call display_line_number
    inc rbx
    jmp .display_line_loop
    
.display_line_done:
    pop rbp
    ret

; Display syntax highlighted text
display_syntax_highlighted_text:
    push rbp
    mov rbp, rsp
    
    call apply_syntax_highlighting
    
    mov rdi, 1
    mov rsi, output_buffer
    mov rdx, qword [editor_size]
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Apply syntax highlighting
apply_syntax_highlighting:
    push rbp
    mov rbp, rsp
    
    mov rsi, editor_buffer
    mov rdi, output_buffer
    mov rcx, qword [editor_size]
    
    ; Scan for keywords, strings, comments, etc.
    call scan_and_highlight
    
    pop rbp
    ret

; Scan and highlight text
scan_and_highlight:
    push rbp
    mov rbp, rsp
    
    ; This function would scan the text for:
    ; - Keywords (mov, add, sub, etc.)
    ; - Registers (rax, rbx, rcx, etc.)
    ; - Strings ("text")
    ; - Comments (;)
    ; - Numbers (0x123, 456)
    
    ; For now, just copy the text
    mov rsi, editor_buffer
    mov rdi, output_buffer
    mov rcx, qword [editor_size]
    rep movsb
    
    pop rbp
    ret

; Display plain text
display_plain_text:
    push rbp
    mov rbp, rsp
    
    mov rdi, 1
    mov rsi, editor_buffer
    mov rdx, qword [editor_size]
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Display status bar
display_status_bar:
    push rbp
    mov rbp, rsp
    
    mov rdi, 1
    mov rsi, status_ready
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Handle input
handle_input:
    push rbp
    mov rbp, rsp
    
    mov rdi, 0
    mov rsi, input_buffer
    mov rdx, 1024
    mov rax, SYS_READ
    syscall
    
    mov al, byte [input_buffer]
    
    cmp al, 0x1b  ; Escape
    je .handle_escape
    
    cmp al, 0x0d  ; Enter
    je .handle_enter
    
    cmp al, 0x08  ; Backspace
    je .handle_backspace
    
    cmp al, 0x09  ; Tab
    je .handle_tab
    
    call insert_character
    jmp .input_done
    
.handle_escape:
    call handle_escape_key
    jmp .input_done
    
.handle_enter:
    call insert_newline
    jmp .input_done
    
.handle_backspace:
    call delete_character
    jmp .input_done
    
.handle_tab:
    call insert_tab
    jmp .input_done
    
.input_done:
    pop rbp
    ret

; Handle escape key
handle_escape_key:
    push rbp
    mov rbp, rsp
    
    cmp byte [menu_active], 0
    je .activate_menu
    mov byte [menu_active], 0
    jmp .menu_done
    
.activate_menu:
    mov byte [menu_active], 1
    
.menu_done:
    pop rbp
    ret

; Insert character
insert_character:
    push rbp
    mov rbp, rsp
    
    mov al, byte [input_buffer]
    mov rsi, editor_buffer
    add rsi, qword [cursor_x]
    mov byte [rsi], al
    
    inc qword [cursor_x]
    inc qword [editor_size]
    
    pop rbp
    ret

; Insert newline
insert_newline:
    push rbp
    mov rbp, rsp
    
    mov rsi, editor_buffer
    add rsi, qword [cursor_x]
    mov byte [rsi], 10
    
    inc qword [cursor_x]
    inc qword [cursor_y]
    inc qword [editor_size]
    
    pop rbp
    ret

; Insert tab
insert_tab:
    push rbp
    mov rbp, rsp
    
    mov rsi, editor_buffer
    add rsi, qword [cursor_x]
    mov byte [rsi], 9
    
    inc qword [cursor_x]
    inc qword [editor_size]
    
    pop rbp
    ret

; Delete character
delete_character:
    push rbp
    mov rbp, rsp
    
    cmp qword [cursor_x], 0
    je .delete_done
    
    mov rsi, editor_buffer
    add rsi, qword [cursor_x]
    mov rdi, rsi
    dec rdi
    
    mov rcx, qword [editor_size]
    sub rcx, qword [cursor_x]
    rep movsb
    
    dec qword [cursor_x]
    dec qword [editor_size]
    
.delete_done:
    pop rbp
    ret

; Display line number
display_line_number:
    push rbp
    mov rbp, rsp
    
    mov rdi, 1
    mov rsi, line_number_format
    mov rdx, 3
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Utility functions
clear_screen:
    push rbp
    mov rbp, rsp
    
    mov rdi, 1
    mov rsi, clear_seq
    mov rdx, 4
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

display_welcome:
    push rbp
    mov rbp, rsp
    
    mov rdi, 1
    mov rsi, gui_title
    mov rdx, gui_title_len
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, newline
    mov rdx, 1
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

strlen:
    push rbp
    mov rbp, rsp
    
    mov rax, 0
    mov rdi, rsi
    
.loop:
    cmp byte [rdi], 0
    je .done
    inc rax
    inc rdi
    jmp .loop
    
.done:
    pop rbp
    ret

cleanup_gui:
    push rbp
    mov rbp, rsp
    
    call clear_screen
    
    mov rdi, 1
    mov rsi, exit_message
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Data section
section .data
    sidebar_title db 'Project Explorer:', 10, 0
    sidebar_files db 'Files:', 10, 0
    newline db 10, 0
    clear_seq db 0x1b, '[2J', 0x1b, '[H', 0
    line_number_format db '   ', 0
    exit_message db 'Thank you for using Assembly Text Editor!', 10, 0
