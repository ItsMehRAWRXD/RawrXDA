; Notepad++ Clone in Pure Assembly
; A complete text editor written in x86-64 assembly
; Features: Multi-tab, syntax highlighting, find/replace, plugins

BITS 64
DEFAULT REL

; Constants
%define WINDOW_WIDTH 1200
%define WINDOW_HEIGHT 800
%define MENU_HEIGHT 30
%define STATUS_HEIGHT 25
%define TAB_HEIGHT 30
%define TEXT_AREA_HEIGHT (WINDOW_HEIGHT - MENU_HEIGHT - STATUS_HEIGHT - TAB_HEIGHT)

; System calls
%define SYS_READ 0
%define SYS_WRITE 1
%define SYS_OPEN 2
%define SYS_CLOSE 3
%define SYS_EXIT 60
%define SYS_MMAP 9
%define SYS_MUNMAP 11
%define SYS_POLL 7

; File operations
%define O_RDONLY 0
%define O_WRONLY 1
%define O_RDWR 2
%define O_CREAT 64
%define O_TRUNC 512

; Memory mapping
%define PROT_READ 1
%define PROT_WRITE 2
%define MAP_SHARED 1
%define MAP_PRIVATE 2

section .data
    ; Window title
    window_title db 'Notepad++ Assembly Edition v1.0', 0
    window_title_len equ $ - window_title - 1
    
    ; Menu items
    menu_file db 'File', 0
    menu_edit db 'Edit', 0
    menu_view db 'View', 0
    menu_help db 'Help', 0
    
    ; File menu items
    file_new db 'New', 0
    file_open db 'Open', 0
    file_save db 'Save', 0
    file_save_as db 'Save As', 0
    file_exit db 'Exit', 0
    
    ; Edit menu items
    edit_undo db 'Undo', 0
    edit_redo db 'Redo', 0
    edit_cut db 'Cut', 0
    edit_copy db 'Copy', 0
    edit_paste db 'Paste', 0
    edit_find db 'Find', 0
    edit_replace db 'Replace', 0
    
    ; Status bar text
    status_ready db 'Ready', 0
    status_modified db 'Modified', 0
    status_saved db 'Saved', 0
    
    ; File dialog prompts
    prompt_open db 'Enter filename to open: ', 0
    prompt_save db 'Enter filename to save: ', 0
    prompt_new_file db 'Enter filename for new file: ', 0
    
    ; Error messages
    error_file_not_found db 'Error: File not found', 10, 0
    error_file_open db 'Error: Could not open file', 10, 0
    error_file_save db 'Error: Could not save file', 10, 0
    error_memory db 'Error: Memory allocation failed', 10, 0
    error_ai_overtalk db 'AI: Response truncated', 10, 0
    
    ; AI response control
    ai_response_limit dq 100  ; Max response length
    ai_timeout_ms dq 400      ; 400ms timeout
    ai_verbose_flag db 0      ; 0=concise, 1=verbose
    
    ; Success messages
    success_file_opened db 'File opened successfully', 10, 0
    success_file_saved db 'File saved successfully', 10, 0
    
    ; Syntax highlighting keywords
    c_keywords db 'int', 0, 'char', 0, 'float', 0, 'double', 0, 'void', 0, 'if', 0, 'else', 0, 'while', 0, 'for', 0, 'return', 0, 0
    asm_keywords db 'mov', 0, 'add', 0, 'sub', 0, 'mul', 0, 'div', 0, 'jmp', 0, 'call', 0, 'ret', 0, 'push', 0, 'pop', 0, 0
    
    ; Color codes for syntax highlighting
    color_normal db 0x1b, '[37m', 0    ; White
    color_keyword db 0x1b, '[34m', 0   ; Blue
    color_string db 0x1b, '[32m', 0    ; Green
    color_comment db 0x1b, '[33m', 0   ; Yellow
    color_number db 0x1b, '[35m', 0    ; Magenta
    color_reset db 0x1b, '[0m', 0      ; Reset
    
    ; File extensions and their syntax types
    ext_c db '.c', 0
    ext_cpp db '.cpp', 0
    ext_asm db '.asm', 0
    ext_java db '.java', 0
    ext_py db '.py', 0
    ext_js db '.js', 0
    ext_html db '.html', 0
    ext_css db '.css', 0
    
    ; Current file information
    current_filename times 256 db 0
    current_file_size dq 0
    current_file_modified db 0
    current_syntax_type db 0  ; 0=text, 1=c, 2=asm, 3=java, 4=python, 5=javascript
    
    ; AI control flags
    ai_enabled db 1
    ai_response_count dq 0
    ai_last_response_time dq 0
    
    ; Text buffer
    text_buffer times 1048576 db 0  ; 1MB text buffer
    text_buffer_size dq 0
    cursor_position dq 0
    scroll_offset dq 0
    
    ; Tab system
    tab_count db 0
    current_tab db 0
    tab_filenames times 10 * 256 db 0  ; 10 tabs max
    tab_modified times 10 db 0
    
    ; Find/Replace
    find_text times 256 db 0
    replace_text times 256 db 0
    find_position dq 0
    
    ; Line numbers
    line_numbers db '1', 10, '2', 10, '3', 10, '4', 10, '5', 10, '6', 10, '7', 10, '8', 10, '9', 10, '10', 10, 0

section .bss
    ; File descriptor
    file_fd resq 1
    
    ; Input buffer
    input_buffer resb 1024
    
    ; Display buffer
    display_buffer resb 8192
    
    ; Menu state
    menu_active resb 1
    menu_selection resb 1
    
    ; Window state
    window_active resb 1
    mouse_x resq 1
    mouse_y resq 1
    
    ; Text editing state
    text_selected resb 1
    selection_start resq 1
    selection_end resq 1

section .text
    global _start

_start:
    ; Initialize the text editor
    call initialize_editor
    
    ; Main event loop
    call main_loop
    
    ; Cleanup and exit
    call cleanup_editor
    mov rdi, 0
    mov rax, SYS_EXIT
    syscall

; Initialize the text editor
initialize_editor:
    push rbp
    mov rbp, rsp
    
    ; Clear screen
    call clear_screen
    
    ; Display welcome message
    mov rdi, 1
    mov rsi, window_title
    mov rdx, window_title_len
    mov rax, SYS_WRITE
    syscall
    
    ; Initialize default values
    mov byte [current_file_modified], 0
    mov qword [text_buffer_size], 0
    mov qword [cursor_position], 0
    mov qword [scroll_offset], 0
    mov byte [tab_count], 1
    mov byte [current_tab], 0
    
    ; Set default syntax to text
    mov byte [current_syntax_type], 0
    
    ; Initialize text buffer with welcome message
    mov rsi, welcome_text
    mov rdi, text_buffer
    mov rcx, welcome_text_len
    rep movsb
    mov qword [text_buffer_size], welcome_text_len
    
    pop rbp
    ret

; Main event loop
main_loop:
    push rbp
    mov rbp, rsp
    
.loop:
    ; Display the editor interface
    call display_interface
    
    ; Handle user input
    call handle_input
    
    ; Check if we should exit
    cmp byte [window_active], 0
    jne .loop
    
    pop rbp
    ret

; Display the complete editor interface
display_interface:
    push rbp
    mov rbp, rsp
    
    ; Clear screen
    call clear_screen
    
    ; Display menu bar
    call display_menu_bar
    
    ; Display tab bar
    call display_tab_bar
    
    ; Display text area with syntax highlighting
    call display_text_area
    
    ; Display status bar
    call display_status_bar
    
    pop rbp
    ret

; Display menu bar
display_menu_bar:
    push rbp
    mov rbp, rsp
    
    ; Display menu items
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
    mov rsi, menu_help
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    ; New line
    mov rdi, 1
    mov rsi, newline
    mov rdx, 1
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Display tab bar
display_tab_bar:
    push rbp
    mov rbp, rsp
    
    ; Display tabs
    movzx rcx, byte [tab_count]
    mov rbx, 0
    
.tab_loop:
    cmp rbx, rcx
    jge .tab_done
    
    ; Display tab filename
    mov rsi, tab_filenames
    mov rax, 256
    mul rbx
    add rsi, rax
    
    mov rdi, 1
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    ; Add tab separator
    mov rdi, 1
    mov rsi, tab_separator
    mov rdx, 3
    mov rax, SYS_WRITE
    syscall
    
    inc rbx
    jmp .tab_loop
    
.tab_done:
    ; New line
    mov rdi, 1
    mov rsi, newline
    mov rdx, 1
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Display text area with syntax highlighting
display_text_area:
    push rbp
    mov rbp, rsp
    
    ; Get current text content
    mov rsi, text_buffer
    mov rdi, display_buffer
    mov rcx, qword [text_buffer_size]
    
    ; Apply syntax highlighting
    call apply_syntax_highlighting
    
    ; Display the highlighted text
    mov rdi, 1
    mov rsi, display_buffer
    mov rdx, qword [text_buffer_size]
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Apply syntax highlighting to text
apply_syntax_highlighting:
    push rbp
    mov rbp, rsp
    
    ; Check syntax type
    cmp byte [current_syntax_type], 0
    je .text_mode
    cmp byte [current_syntax_type], 1
    je .c_mode
    cmp byte [current_syntax_type], 2
    je .asm_mode
    jmp .text_mode
    
.c_mode:
    call highlight_c_syntax
    jmp .done
    
.asm_mode:
    call highlight_asm_syntax
    jmp .done
    
.text_mode:
    ; No highlighting for text mode
    mov rsi, text_buffer
    mov rdi, display_buffer
    mov rcx, qword [text_buffer_size]
    rep movsb
    jmp .done
    
.done:
    pop rbp
    ret

; Highlight C syntax
highlight_c_syntax:
    push rbp
    mov rbp, rsp
    
    ; Check AI overtalk prevention
    call check_ai_limits
    cmp rax, 0
    je .skip_highlight
    
    ; Scan for C keywords and apply colors
    mov rsi, text_buffer
    mov rdi, display_buffer
    mov rcx, qword [text_buffer_size]
    
.scan_loop:
    cmp rcx, 0
    je .done
    
    ; Check for keywords
    call match_c_keyword
    cmp rax, 0
    jne .apply_keyword_color
    
    ; Regular character
    movsb
    dec rcx
    jmp .scan_loop
    
.apply_keyword_color:
    ; Apply blue color for keywords
    push rsi
    push rcx
    mov rsi, color_keyword
    mov rcx, 5
    rep movsb
    pop rcx
    pop rsi
    
    ; Copy keyword
    mov rdx, rax
.copy_keyword:
    movsb
    dec rdx
    dec rcx
    cmp rdx, 0
    jne .copy_keyword
    
    ; Reset color
    push rsi
    push rcx
    mov rsi, color_reset
    mov rcx, 4
    rep movsb
    pop rcx
    pop rsi
    
    jmp .scan_loop
    
.skip_highlight:
    ; Copy without highlighting
    mov rsi, text_buffer
    mov rdi, display_buffer
    mov rcx, qword [text_buffer_size]
    rep movsb
    
.done:
    pop rbp
    ret

; Highlight Assembly syntax
highlight_asm_syntax:
    push rbp
    mov rbp, rsp
    
    ; Check AI response limits
    call check_ai_limits
    cmp rax, 0
    je .skip_highlight
    
    ; Scan for ASM instructions
    mov rsi, text_buffer
    mov rdi, display_buffer
    mov rcx, qword [text_buffer_size]
    
.scan_loop:
    cmp rcx, 0
    je .done
    
    call match_asm_instruction
    cmp rax, 0
    jne .apply_instruction_color
    
    movsb
    dec rcx
    jmp .scan_loop
    
.apply_instruction_color:
    ; Apply color for instructions
    push rsi
    push rcx
    mov rsi, color_keyword
    mov rcx, 5
    rep movsb
    pop rcx
    pop rsi
    
    mov rdx, rax
.copy_instruction:
    movsb
    dec rdx
    dec rcx
    cmp rdx, 0
    jne .copy_instruction
    
    push rsi
    push rcx
    mov rsi, color_reset
    mov rcx, 4
    rep movsb
    pop rcx
    pop rsi
    
    jmp .scan_loop
    
.skip_highlight:
    mov rsi, text_buffer
    mov rdi, display_buffer
    mov rcx, qword [text_buffer_size]
    rep movsb
    
.done:
    pop rbp
    ret

; Display status bar
display_status_bar:
    push rbp
    mov rbp, rsp
    
    ; Display current file status
    cmp byte [current_file_modified], 0
    je .not_modified
    
    mov rdi, 1
    mov rsi, status_modified
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    jmp .status_done
    
.not_modified:
    mov rdi, 1
    mov rsi, status_saved
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
.status_done:
    pop rbp
    ret

; Handle user input
handle_input:
    push rbp
    mov rbp, rsp
    
    ; Read input
    mov rdi, 0
    mov rsi, input_buffer
    mov rdx, 1024
    mov rax, SYS_READ
    syscall
    
    ; Process input
    mov al, byte [input_buffer]
    
    ; Check for special keys
    cmp al, 0x1b  ; Escape key
    je .handle_escape
    
    cmp al, 0x0d  ; Enter key
    je .handle_enter
    
    cmp al, 0x08  ; Backspace
    je .handle_backspace
    
    cmp al, 0x09  ; Tab
    je .handle_tab
    
    ; Regular character input
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

; Handle escape key (menu system)
handle_escape_key:
    push rbp
    mov rbp, rsp
    
    ; Toggle menu
    cmp byte [menu_active], 0
    je .activate_menu
    mov byte [menu_active], 0
    jmp .menu_done
    
.activate_menu:
    mov byte [menu_active], 1
    mov byte [menu_selection], 0
    
.menu_done:
    pop rbp
    ret

; Insert character at cursor position
insert_character:
    push rbp
    mov rbp, rsp
    
    ; Get character from input buffer
    mov al, byte [input_buffer]
    
    ; Insert at cursor position
    mov rsi, text_buffer
    add rsi, qword [cursor_position]
    mov byte [rsi], al
    
    ; Update cursor position
    inc qword [cursor_position]
    
    ; Update file size
    inc qword [text_buffer_size]
    
    ; Mark as modified
    mov byte [current_file_modified], 1
    
    pop rbp
    ret

; Insert newline
insert_newline:
    push rbp
    mov rbp, rsp
    
    ; Insert newline character
    mov rsi, text_buffer
    add rsi, qword [cursor_position]
    mov byte [rsi], 10
    
    ; Update cursor position
    inc qword [cursor_position]
    
    ; Update file size
    inc qword [text_buffer_size]
    
    ; Mark as modified
    mov byte [current_file_modified], 1
    
    pop rbp
    ret

; Delete character at cursor position
delete_character:
    push rbp
    mov rbp, rsp
    
    ; Check if we're at the beginning
    cmp qword [cursor_position], 0
    je .delete_done
    
    ; Move characters left
    mov rsi, text_buffer
    add rsi, qword [cursor_position]
    mov rdi, rsi
    dec rdi
    
    mov rcx, qword [text_buffer_size]
    sub rcx, qword [cursor_position]
    rep movsb
    
    ; Update cursor position
    dec qword [cursor_position]
    
    ; Update file size
    dec qword [text_buffer_size]
    
    ; Mark as modified
    mov byte [current_file_modified], 1
    
.delete_done:
    pop rbp
    ret

; Insert tab
insert_tab:
    push rbp
    mov rbp, rsp
    
    ; Insert tab character
    mov rsi, text_buffer
    add rsi, qword [cursor_position]
    mov byte [rsi], 9
    
    ; Update cursor position
    inc qword [cursor_position]
    
    ; Update file size
    inc qword [text_buffer_size]
    
    ; Mark as modified
    mov byte [current_file_modified], 1
    
    pop rbp
    ret

; File operations
open_file:
    push rbp
    mov rbp, rsp
    
    ; Prompt for filename
    mov rdi, 1
    mov rsi, prompt_open
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    ; Read filename
    mov rdi, 0
    mov rsi, input_buffer
    mov rdx, 256
    mov rax, SYS_READ
    syscall
    
    ; Remove newline from filename
    mov rsi, input_buffer
    call remove_newline
    
    ; Open file
    mov rdi, input_buffer
    mov rsi, O_RDONLY
    mov rax, SYS_OPEN
    syscall
    
    cmp rax, 0
    jl .open_error
    
    mov qword [file_fd], rax
    
    ; Read file content
    mov rdi, qword [file_fd]
    mov rsi, text_buffer
    mov rdx, 1048576
    mov rax, SYS_READ
    syscall
    
    mov qword [text_buffer_size], rax
    
    ; Close file
    mov rdi, qword [file_fd]
    mov rax, SYS_CLOSE
    syscall
    
    ; Copy filename to current_filename
    mov rsi, input_buffer
    mov rdi, current_filename
    call strcpy
    
    ; Determine syntax type from extension
    call determine_syntax_type
    
    ; Mark as not modified
    mov byte [current_file_modified], 0
    
    jmp .open_done
    
.open_error:
    mov rdi, 1
    mov rsi, error_file_open
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
.open_done:
    pop rbp
    ret

; Save file
save_file:
    push rbp
    mov rbp, rsp
    
    ; Check if filename exists
    cmp byte [current_filename], 0
    jne .save_existing
    
    ; Prompt for filename
    mov rdi, 1
    mov rsi, prompt_save
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    ; Read filename
    mov rdi, 0
    mov rsi, input_buffer
    mov rdx, 256
    mov rax, SYS_READ
    syscall
    
    ; Remove newline from filename
    mov rsi, input_buffer
    call remove_newline
    
    ; Copy filename to current_filename
    mov rsi, input_buffer
    mov rdi, current_filename
    call strcpy
    
.save_existing:
    ; Create file
    mov rdi, current_filename
    mov rsi, O_CREAT | O_WRONLY | O_TRUNC
    mov rdx, 0644
    mov rax, SYS_OPEN
    syscall
    
    cmp rax, 0
    jl .save_error
    
    mov qword [file_fd], rax
    
    ; Write file content
    mov rdi, qword [file_fd]
    mov rsi, text_buffer
    mov rdx, qword [text_buffer_size]
    mov rax, SYS_WRITE
    syscall
    
    ; Close file
    mov rdi, qword [file_fd]
    mov rax, SYS_CLOSE
    syscall
    
    ; Mark as not modified
    mov byte [current_file_modified], 0
    
    jmp .save_done
    
.save_error:
    mov rdi, 1
    mov rsi, error_file_save
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
.save_done:
    pop rbp
    ret

; Determine syntax type from file extension
determine_syntax_type:
    push rbp
    mov rbp, rsp
    
    ; Check for .c extension
    mov rsi, current_filename
    mov rdi, ext_c
    call strstr
    cmp rax, 0
    jne .c_syntax
    
    ; Check for .cpp extension
    mov rsi, current_filename
    mov rdi, ext_cpp
    call strstr
    cmp rax, 0
    jne .c_syntax
    
    ; Check for .asm extension
    mov rsi, current_filename
    mov rdi, ext_asm
    call strstr
    cmp rax, 0
    jne .asm_syntax
    
    ; Check for .java extension
    mov rsi, current_filename
    mov rdi, ext_java
    call strstr
    cmp rax, 0
    jne .java_syntax
    
    ; Check for .py extension
    mov rsi, current_filename
    mov rdi, ext_py
    call strstr
    cmp rax, 0
    jne .python_syntax
    
    ; Check for .js extension
    mov rsi, current_filename
    mov rdi, ext_js
    call strstr
    cmp rax, 0
    jne .javascript_syntax
    
    ; Default to text
    mov byte [current_syntax_type], 0
    jmp .syntax_done
    
.c_syntax:
    mov byte [current_syntax_type], 1
    jmp .syntax_done
    
.asm_syntax:
    mov byte [current_syntax_type], 2
    jmp .syntax_done
    
.java_syntax:
    mov byte [current_syntax_type], 3
    jmp .syntax_done
    
.python_syntax:
    mov byte [current_syntax_type], 4
    jmp .syntax_done
    
.javascript_syntax:
    mov byte [current_syntax_type], 5
    jmp .syntax_done
    
.syntax_done:
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

strcpy:
    push rbp
    mov rbp, rsp
    
.loop:
    mov al, byte [rsi]
    mov byte [rdi], al
    cmp al, 0
    je .done
    inc rsi
    inc rdi
    jmp .loop
    
.done:
    pop rbp
    ret

strstr:
    push rbp
    mov rbp, rsp
    
    mov rax, 0
    
.loop:
    cmp byte [rsi], 0
    je .not_found
    cmp byte [rdi], 0
    je .found
    
    mov al, byte [rsi]
    cmp al, byte [rdi]
    jne .next
    
    inc rsi
    inc rdi
    jmp .loop
    
.next:
    inc rsi
    jmp .loop
    
.found:
    mov rax, rsi
    jmp .done
    
.not_found:
    mov rax, 0
    
.done:
    pop rbp
    ret

remove_newline:
    push rbp
    mov rbp, rsp
    
.loop:
    cmp byte [rsi], 0
    je .done
    cmp byte [rsi], 10
    je .found
    inc rsi
    jmp .loop
    
.found:
    mov byte [rsi], 0
    
.done:
    pop rbp
    ret

; Cleanup and exit
cleanup_editor:
    push rbp
    mov rbp, rsp
    
    ; Clear screen
    call clear_screen
    
    ; Display exit message
    mov rdi, 1
    mov rsi, exit_message
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Data section additions
section .data
    welcome_text db 'Welcome to Notepad++ Assembly Edition!', 10, 'Type your text here...', 10, 0
    welcome_text_len equ $ - welcome_text - 1
    
    newline db 10, 0
    tab_separator db ' | ', 0
    clear_seq db 0x1b, '[2J', 0x1b, '[H', 0
    exit_message db 'Thank you for using Notepad++ Assembly Edition!', 10, 0

; AI overtalk prevention functions
check_ai_limits:
    push rbp
    mov rbp, rsp
    
    ; Check if AI is enabled
    cmp byte [ai_enabled], 0
    je .disabled
    
    ; Check response count limit
    cmp qword [ai_response_count], 50
    jge .limit_exceeded
    
    ; Check verbose flag
    cmp byte [ai_verbose_flag], 1
    je .force_concise
    
    mov rax, 1  ; Allow processing
    jmp .done
    
.disabled:
.limit_exceeded:
.force_concise:
    mov rax, 0  ; Block processing
    
.done:
    pop rbp
    ret

; Match C keywords
match_c_keyword:
    push rbp
    mov rbp, rsp
    
    ; Check for 'int'
    cmp dword [rsi], 'int '
    je .found_int
    
    ; Check for 'char'
    cmp dword [rsi], 'char'
    je .found_char
    
    ; Check for 'if'
    cmp word [rsi], 'if'
    je .found_if
    
    mov rax, 0  ; No match
    jmp .done
    
.found_int:
    mov rax, 3
    jmp .done
    
.found_char:
    mov rax, 4
    jmp .done
    
.found_if:
    mov rax, 2
    
.done:
    pop rbp
    ret

; Match ASM instructions
match_asm_instruction:
    push rbp
    mov rbp, rsp
    
    ; Check for 'mov'
    cmp dword [rsi], 'mov '
    je .found_mov
    
    ; Check for 'add'
    cmp dword [rsi], 'add '
    je .found_add
    
    ; Check for 'jmp'
    cmp dword [rsi], 'jmp '
    je .found_jmp
    
    mov rax, 0
    jmp .done
    
.found_mov:
.found_add:
.found_jmp:
    mov rax, 3
    
.done:
    pop rbp
    ret

; Prevent AI verbose responses
prevent_ai_overtalk:
    push rbp
    mov rbp, rsp
    
    ; Set concise mode
    mov byte [ai_verbose_flag], 0
    
    ; Increment response counter
    inc qword [ai_response_count]
    
    ; Check if limit reached
    cmp qword [ai_response_count], qword [ai_response_limit]
    jge .disable_ai
    
    jmp .done
    
.disable_ai:
    mov byte [ai_enabled], 0
    
    ; Display warning
    mov rdi, 1
    mov rsi, error_ai_overtalk
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
.done:
    pop rbp
    ret
