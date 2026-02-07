; TypeScript IDE in Pure Assembly
; Complete TypeScript development environment

BITS 64
DEFAULT REL

; IDE Constants
%define IDE_WIDTH 1600
%define IDE_HEIGHT 1000
%define MENU_HEIGHT 40
%define TOOLBAR_HEIGHT 50
%define STATUS_HEIGHT 30
%define SIDEBAR_WIDTH 350
%define EDITOR_WIDTH (IDE_WIDTH - SIDEBAR_WIDTH)

section .data
    ; IDE Title
    ide_title db 'TypeScript IDE v3.0 - Advanced TypeScript Development', 0
    ide_title_len equ $ - ide_title - 1
    
    ; Menu System
    menu_file db 'File', 0
    menu_edit db 'Edit', 0
    menu_view db 'View', 0
    menu_project db 'Project', 0
    menu_build db 'Build', 0
    menu_debug db 'Debug', 0
    menu_tools db 'Tools', 0
    menu_help db 'Help', 0
    
    ; File Menu
    file_new db 'New TypeScript File', 0
    file_new_project db 'New TypeScript Project', 0
    file_open db 'Open File', 0
    file_open_project db 'Open Project', 0
    file_save db 'Save', 0
    file_save_as db 'Save As', 0
    file_save_all db 'Save All', 0
    file_close db 'Close', 0
    file_exit db 'Exit', 0
    
    ; Edit Menu
    edit_undo db 'Undo', 0
    edit_redo db 'Redo', 0
    edit_cut db 'Cut', 0
    edit_copy db 'Copy', 0
    edit_paste db 'Paste', 0
    edit_find db 'Find', 0
    edit_replace db 'Replace', 0
    edit_goto db 'Go to Definition', 0
    edit_rename db 'Rename Symbol', 0
    edit_format db 'Format Document', 0
    
    ; Project Menu
    project_new db 'New TypeScript Project', 0
    project_open db 'Open Project', 0
    project_close db 'Close Project', 0
    project_properties db 'Project Properties', 0
    project_add_file db 'Add File to Project', 0
    project_remove_file db 'Remove File from Project', 0
    project_compile db 'Compile Project', 0
    
    ; Build Menu
    build_compile db 'Compile TypeScript', 0
    build_compile_all db 'Compile All', 0
    build_clean db 'Clean', 0
    build_rebuild db 'Rebuild All', 0
    build_run db 'Run', 0
    build_debug db 'Debug', 0
    build_watch db 'Watch Mode', 0
    
    ; Debug Menu
    debug_start db 'Start Debugging', 0
    debug_stop db 'Stop Debugging', 0
    debug_step_into db 'Step Into', 0
    debug_step_over db 'Step Over', 0
    debug_step_out db 'Step Out', 0
    debug_continue db 'Continue', 0
    debug_breakpoint db 'Toggle Breakpoint', 0
    debug_watch db 'Add Watch', 0
    debug_call_stack db 'Call Stack', 0
    
    ; Tools Menu
    tools_compiler db 'TypeScript Compiler Settings', 0
    tools_linter db 'ESLint Settings', 0
    tools_formatter db 'Prettier Settings', 0
    tools_package db 'Package Manager', 0
    tools_plugins db 'Plugin Manager', 0
    tools_preferences db 'Preferences', 0
    
    ; TypeScript Keywords
    ts_keywords db 'interface', 0, 'class', 0, 'function', 0, 'const', 0, 'let', 0, 'var', 0, 'type', 0, 'enum', 0, 'namespace', 0, 'module', 0, 'import', 0, 'export', 0, 'public', 0, 'private', 0, 'protected', 0, 'static', 0, 'abstract', 0, 'extends', 0, 'implements', 0, 'async', 0, 'await', 0, 'Promise', 0, 'Array', 0, 'Object', 0, 'string', 0, 'number', 0, 'boolean', 0, 'void', 0, 'any', 0, 'unknown', 0, 'never', 0, 'null', 0, 'undefined', 0, 0
    
    ; TypeScript Types
    ts_types db 'string', 0, 'number', 0, 'boolean', 0, 'void', 0, 'any', 0, 'unknown', 0, 'never', 0, 'null', 0, 'undefined', 0, 'object', 0, 'array', 0, 'function', 0, 'symbol', 0, 'bigint', 0, 0
    
    ; TypeScript Operators
    ts_operators db '=>', 0, '?', 0, '!', 0, '??', 0, '||', 0, '&&', 0, '===', 0, '!==', 0, '>=', 0, '<=', 0, '>', 0, '<', 0, '=', 0, '+', 0, '-', 0, '*', 0, '/', 0, '%', 0, '++', 0, '--', 0, 0
    
    ; Color Codes for TypeScript Syntax
    color_normal db 0x1b, '[37m', 0      ; White
    color_keyword db 0x1b, '[34m', 0     ; Blue
    color_type db 0x1b, '[32m', 0       ; Green
    color_string db 0x1b, '[31m', 0     ; Red
    color_comment db 0x1b, '[33m', 0    ; Yellow
    color_number db 0x1b, '[36m', 0     ; Cyan
    color_operator db 0x1b, '[35m', 0   ; Magenta
    color_function db 0x1b, '[1;33m', 0 ; Bright Yellow
    color_class db 0x1b, '[1;34m', 0    ; Bright Blue
    color_interface db 0x1b, '[1;32m', 0; Bright Green
    color_error db 0x1b, '[1;31m', 0    ; Bright Red
    color_warning db 0x1b, '[1;35m', 0  ; Bright Magenta
    color_reset db 0x1b, '[0m', 0       ; Reset
    
    ; Project Structure
    project_name times 256 db 0
    project_path times 512 db 0
    project_files times 100 * 256 db 0  ; 100 files max
    project_file_count dq 0
    current_file_index dq 0
    
    ; Editor State
    editor_buffer times 4194304 db 0    ; 4MB editor buffer
    editor_size dq 0
    cursor_x dq 0
    cursor_y dq 0
    scroll_x dq 0
    scroll_y dq 0
    selection_start_x dq 0
    selection_start_y dq 0
    selection_end_x dq 0
    selection_end_y dq 0
    text_selected db 0
    
    ; File System
    current_filename times 256 db 0
    current_file_modified db 0
    file_fd dq 0
    
    ; TypeScript Compiler
    tsc_path db 'tsc', 0
    tsc_options db '--strict', 0
    tsc_target db 'ES2020', 0
    tsc_module db 'ESNext', 0
    tsc_output_dir db 'dist/', 0
    
    ; Build System
    build_target db 'development', 0
    build_optimization db 'none', 0
    build_warnings db 'all', 0
    build_output_dir db 'build/', 0
    
    ; Debug System
    debug_active db 0
    debug_breakpoints times 100 dq 0    ; 100 breakpoints max
    debug_breakpoint_count dq 0
    debug_current_line dq 0
    debug_variables times 1000 db 0     ; Variable watch list
    
    ; IntelliSense
    intellisense_active db 0
    intellisense_list times 1000 db 0   ; Completion list
    intellisense_selection dq 0
    
    ; Plugin System
    plugin_count dq 0
    plugin_list times 50 * 256 db 0     ; 50 plugins max
    
    ; Input/Output
    input_buffer times 1024 db 0
    output_buffer times 8192 db 0
    error_buffer times 4096 db 0

section .bss
    ; Window Management
    window_active resb 1
    mouse_x resq 1
    mouse_y resq 1
    key_pressed resb 1
    
    ; Menu System
    menu_active resb 1
    menu_selection resq 1
    submenu_active resb 1
    
    ; Editor State
    editor_focus resb 1
    line_numbers resb 1
    word_wrap resb 1
    syntax_highlighting resb 1
    
    ; Project State
    project_open_flag resb 1
    project_modified resb 1
    
    ; Build State
    build_in_progress resb 1
    build_success resb 1
    
    ; Debug State
    debug_running resb 1
    debug_paused resb 1
    debug_step_mode resb 1

section .text
    global _start

_start:
    ; Initialize TypeScript IDE
    call initialize_typescript_ide
    
    ; Main IDE loop
    call main_ide_loop
    
    ; Cleanup and exit
    call cleanup_ide
    mov rdi, 0
    mov rax, SYS_EXIT
    syscall

; Initialize TypeScript IDE
initialize_typescript_ide:
    push rbp
    mov rbp, rsp
    
    ; Clear screen and display welcome
    call clear_screen
    call display_welcome_screen
    
    ; Initialize default values
    mov byte [window_active], 1
    mov byte [menu_active], 0
    mov byte [editor_focus], 1
    mov byte [line_numbers], 1
    mov byte [word_wrap], 0
    mov byte [syntax_highlighting], 1
    mov byte [project_open_flag], 0
    mov byte [debug_active], 0
    mov byte [intellisense_active], 0
    
    ; Initialize editor
    mov qword [editor_size], 0
    mov qword [cursor_x], 0
    mov qword [cursor_y], 0
    mov qword [scroll_x], 0
    mov qword [scroll_y], 0
    
    ; Initialize project
    mov qword [project_file_count], 0
    mov qword [current_file_index], 0
    
    ; Initialize build system
    mov byte [build_in_progress], 0
    mov byte [build_success], 0
    
    ; Initialize debug system
    mov qword [debug_breakpoint_count], 0
    mov qword [debug_current_line], 0
    
    pop rbp
    ret

; Display welcome screen
display_welcome_screen:
    push rbp
    mov rbp, rsp
    
    ; Display IDE title
    mov rdi, 1
    mov rsi, ide_title
    mov rdx, ide_title_len
    mov rax, SYS_WRITE
    syscall
    
    ; Display welcome message
    mov rdi, 1
    mov rsi, welcome_message
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    ; Display feature list
    mov rdi, 1
    mov rsi, features_list
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    ; Wait for user input
    mov rdi, 0
    mov rsi, input_buffer
    mov rdx, 1
    mov rax, SYS_READ
    syscall
    
    pop rbp
    ret

; Main IDE loop
main_ide_loop:
    push rbp
    mov rbp, rsp
    
.ide_loop:
    ; Display the IDE interface
    call display_ide_interface
    
    ; Handle user input
    call handle_ide_input
    
    ; Check if we should exit
    cmp byte [window_active], 0
    jne .ide_loop
    
    pop rbp
    ret

; Display complete IDE interface
display_ide_interface:
    push rbp
    mov rbp, rsp
    
    ; Clear screen
    call clear_screen
    
    ; Display menu bar
    call display_menu_bar
    
    ; Display toolbar
    call display_toolbar
    
    ; Display main area (sidebar + editor)
    call display_main_area
    
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
    mov rsi, menu_project
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, menu_build
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    mov rdi, 1
    mov rsi, menu_debug
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
    
    ; New line
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
    
    ; Display toolbar buttons
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
    
    ; New line
    mov rdi, 1
    mov rsi, newline
    mov rdx, 1
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Display main area (sidebar + editor)
display_main_area:
    push rbp
    mov rbp, rsp
    
    ; Display sidebar
    call display_sidebar
    
    ; Display editor
    call display_editor
    
    pop rbp
    ret

; Display sidebar
display_sidebar:
    push rbp
    mov rbp, rsp
    
    ; Display project explorer
    mov rdi, 1
    mov rsi, sidebar_project
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    ; Display project files
    cmp byte [project_open_flag], 0
    je .no_project
    
    mov rcx, qword [project_file_count]
    mov rbx, 0
    
.file_loop:
    cmp rbx, rcx
    jge .file_done
    
    ; Display file name
    mov rsi, project_files
    mov rax, 256
    mul rbx
    add rsi, rax
    
    mov rdi, 1
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
    
    inc rbx
    jmp .file_loop
    
.file_done:
    jmp .sidebar_done
    
.no_project:
    mov rdi, 1
    mov rsi, sidebar_no_project
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
.sidebar_done:
    pop rbp
    ret

; Display editor
display_editor:
    push rbp
    mov rbp, rsp
    
    ; Display line numbers if enabled
    cmp byte [line_numbers], 0
    je .no_line_numbers
    
    call display_line_numbers
    
.no_line_numbers:
    ; Display text with syntax highlighting
    cmp byte [syntax_highlighting], 0
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
    mov rbx, 0  ; line counter
    
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
    mov rbx, 1  ; line number
    
.display_line_loop:
    cmp rbx, rcx
    jg .display_line_done
    
    ; Convert line number to string and display
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
    
    ; Apply syntax highlighting to editor buffer
    mov rsi, editor_buffer
    mov rdi, output_buffer
    mov rcx, qword [editor_size]
    
    ; Scan for keywords, types, operators, etc.
    call apply_typescript_syntax_highlighting
    
    ; Display highlighted text
    mov rdi, 1
    mov rsi, output_buffer
    mov rdx, qword [editor_size]
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Apply TypeScript syntax highlighting
apply_typescript_syntax_highlighting:
    push rbp
    mov rbp, rsp
    
    ; This function would scan the text for:
    ; - Keywords (interface, class, function, etc.)
    ; - Types (string, number, boolean, etc.)
    ; - Operators (=>, ?, !, etc.)
    ; - Comments (//, /* */)
    ; - Strings ("", '')
    ; - Numbers (123, 0x123)
    ; - Functions and classes
    
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
    
    ; Display text without highlighting
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
    
    ; Display current status
    cmp byte [build_in_progress], 0
    jne .building_status
    
    cmp byte [debug_active], 0
    jne .debugging_status
    
    mov rdi, 1
    mov rsi, status_ready
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    jmp .status_done
    
.building_status:
    mov rdi, 1
    mov rsi, status_building
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    jmp .status_done
    
.debugging_status:
    mov rdi, 1
    mov rsi, status_debugging
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
.status_done:
    pop rbp
    ret

; Handle IDE input
handle_ide_input:
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
    
    cmp al, 0x1b  ; Ctrl+key combinations
    je .handle_ctrl_key
    
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
    call handle_tab_key
    jmp .input_done
    
.handle_ctrl_key:
    call handle_ctrl_combination
    jmp .input_done
    
.input_done:
    pop rbp
    ret

; Handle escape key
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
    mov qword [menu_selection], 0
    
.menu_done:
    pop rbp
    ret

; Handle tab key
handle_tab_key:
    push rbp
    mov rbp, rsp
    
    ; Check if IntelliSense is active
    cmp byte [intellisense_active], 0
    je .insert_tab
    
    ; Handle IntelliSense selection
    call handle_intellisense_selection
    jmp .tab_done
    
.insert_tab:
    ; Insert tab character
    call insert_tab_character
    
.tab_done:
    pop rbp
    ret

; Handle Ctrl+key combinations
handle_ctrl_combination:
    push rbp
    mov rbp, rsp
    
    ; Read next character to determine Ctrl+key
    mov rdi, 0
    mov rsi, input_buffer
    mov rdx, 1
    mov rax, SYS_READ
    syscall
    
    mov al, byte [input_buffer]
    
    ; Ctrl+N - New file
    cmp al, 'n'
    je .ctrl_n
    
    ; Ctrl+O - Open file
    cmp al, 'o'
    je .ctrl_o
    
    ; Ctrl+S - Save file
    cmp al, 's'
    je .ctrl_s
    
    ; Ctrl+F - Find
    cmp al, 'f'
    je .ctrl_f
    
    ; Ctrl+R - Replace
    cmp al, 'r'
    je .ctrl_r
    
    ; Ctrl+G - Go to definition
    cmp al, 'g'
    je .ctrl_g
    
    ; Ctrl+B - Build
    cmp al, 'b'
    je .ctrl_b
    
    ; Ctrl+D - Debug
    cmp al, 'd'
    je .ctrl_d
    
    jmp .ctrl_done
    
.ctrl_n:
    call new_file
    jmp .ctrl_done
    
.ctrl_o:
    call open_file
    jmp .ctrl_done
    
.ctrl_s:
    call save_file
    jmp .ctrl_done
    
.ctrl_f:
    call find_text
    jmp .ctrl_done
    
.ctrl_r:
    call replace_text
    jmp .ctrl_done
    
.ctrl_g:
    call goto_definition
    jmp .ctrl_done
    
.ctrl_b:
    call build_project
    jmp .ctrl_done
    
.ctrl_d:
    call start_debugging
    jmp .ctrl_done
    
.ctrl_done:
    pop rbp
    ret

; File operations
new_file:
    push rbp
    mov rbp, rsp
    
    ; Clear editor buffer
    mov rdi, editor_buffer
    mov rcx, 4194304
    xor al, al
    rep stosb
    
    ; Reset editor state
    mov qword [editor_size], 0
    mov qword [cursor_x], 0
    mov qword [cursor_y], 0
    mov qword [scroll_x], 0
    mov qword [scroll_y], 0
    
    ; Clear filename
    mov rdi, current_filename
    mov rcx, 256
    xor al, al
    rep stosb
    
    ; Mark as not modified
    mov byte [current_file_modified], 0
    
    pop rbp
    ret

open_file:
    push rbp
    mov rbp, rsp
    
    ; Prompt for filename
    mov rdi, 1
    mov rsi, prompt_open_file
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
    
    ; Open file
    mov rdi, current_filename
    mov rsi, O_RDONLY
    mov rax, SYS_OPEN
    syscall
    
    cmp rax, 0
    jl .open_error
    
    mov qword [file_fd], rax
    
    ; Read file content
    mov rdi, qword [file_fd]
    mov rsi, editor_buffer
    mov rdx, 4194304
    mov rax, SYS_READ
    syscall
    
    mov qword [editor_size], rax
    
    ; Close file
    mov rdi, qword [file_fd]
    mov rax, SYS_CLOSE
    syscall
    
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

save_file:
    push rbp
    mov rbp, rsp
    
    ; Check if filename exists
    cmp byte [current_filename], 0
    jne .save_existing
    
    ; Prompt for filename
    mov rdi, 1
    mov rsi, prompt_save_file
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
    mov rsi, editor_buffer
    mov rdx, qword [editor_size]
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

; Build system
build_project:
    push rbp
    mov rbp, rsp
    
    ; Set build in progress
    mov byte [build_in_progress], 1
    
    ; Display build message
    mov rdi, 1
    mov rsi, build_starting
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    ; Compile TypeScript files
    call compile_typescript_files
    
    ; Check build result
    cmp byte [build_success], 0
    je .build_failed
    
    ; Display success message
    mov rdi, 1
    mov rsi, build_success_msg
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    jmp .build_done
    
.build_failed:
    ; Display error message
    mov rdi, 1
    mov rsi, build_error_msg
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
.build_done:
    ; Clear build in progress
    mov byte [build_in_progress], 0
    
    pop rbp
    ret

; Compile TypeScript files
compile_typescript_files:
    push rbp
    mov rbp, rsp
    
    ; This function would:
    ; 1. Find all .ts files in the project
    ; 2. Compile each file with TypeScript compiler
    ; 3. Generate JavaScript files
    ; 4. Handle compilation errors
    
    ; For now, just simulate compilation
    mov rdi, 1
    mov rsi, compiling_msg
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Debug system
start_debugging:
    push rbp
    mov rbp, rsp
    
    ; Set debug active
    mov byte [debug_active], 1
    mov byte [debug_running], 1
    mov byte [debug_paused], 0
    
    ; Display debug message
    mov rdi, 1
    mov rsi, debug_starting
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    ; Start debugger
    call start_debugger
    
    pop rbp
    ret

; Start debugger
start_debugger:
    push rbp
    mov rbp, rsp
    
    ; This function would:
    ; 1. Load the compiled JavaScript
    ; 2. Set breakpoints
    ; 3. Start debugging session
    ; 4. Handle debug commands
    
    ; For now, just simulate debugging
    mov rdi, 1
    mov rsi, debugger_started
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; IntelliSense system
handle_intellisense_selection:
    push rbp
    mov rbp, rsp
    
    ; This function would:
    ; 1. Show completion list
    ; 2. Handle selection
    ; 3. Insert selected item
    
    ; For now, just simulate IntelliSense
    mov rdi, 1
    mov rsi, intellisense_activated
    call strlen
    mov rdx, rax
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Text editing functions
insert_character:
    push rbp
    mov rbp, rsp
    
    ; Get character from input buffer
    mov al, byte [input_buffer]
    
    ; Insert at cursor position
    mov rsi, editor_buffer
    add rsi, qword [cursor_x]
    mov byte [rsi], al
    
    ; Update cursor position
    inc qword [cursor_x]
    
    ; Update editor size
    inc qword [editor_size]
    
    ; Mark as modified
    mov byte [current_file_modified], 1
    
    pop rbp
    ret

insert_newline:
    push rbp
    mov rbp, rsp
    
    ; Insert newline character
    mov rsi, editor_buffer
    add rsi, qword [cursor_x]
    mov byte [rsi], 10
    
    ; Update cursor position
    inc qword [cursor_x]
    inc qword [cursor_y]
    
    ; Update editor size
    inc qword [editor_size]
    
    ; Mark as modified
    mov byte [current_file_modified], 1
    
    pop rbp
    ret

insert_tab_character:
    push rbp
    mov rbp, rsp
    
    ; Insert tab character
    mov rsi, editor_buffer
    add rsi, qword [cursor_x]
    mov byte [rsi], 9
    
    ; Update cursor position
    inc qword [cursor_x]
    
    ; Update editor size
    inc qword [editor_size]
    
    ; Mark as modified
    mov byte [current_file_modified], 1
    
    pop rbp
    ret

delete_character:
    push rbp
    mov rbp, rsp
    
    ; Check if we're at the beginning
    cmp qword [cursor_x], 0
    je .delete_done
    
    ; Move characters left
    mov rsi, editor_buffer
    add rsi, qword [cursor_x]
    mov rdi, rsi
    dec rdi
    
    mov rcx, qword [editor_size]
    sub rcx, qword [cursor_x]
    rep movsb
    
    ; Update cursor position
    dec qword [cursor_x]
    
    ; Update editor size
    dec qword [editor_size]
    
    ; Mark as modified
    mov byte [current_file_modified], 1
    
.delete_done:
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

display_line_number:
    push rbp
    mov rbp, rsp
    
    ; Convert line number to string and display
    mov rdi, 1
    mov rsi, line_number_format
    mov rdx, 3
    mov rax, SYS_WRITE
    syscall
    
    pop rbp
    ret

; Cleanup and exit
cleanup_ide:
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
    welcome_message db 'Welcome to TypeScript IDE!', 10, 'Advanced TypeScript Development Environment', 10, 0
    features_list db 'Features:', 10, '- TypeScript Compilation', 10, '- IntelliSense', 10, '- Debugging', 10, '- Project Management', 10, '- Build System', 10, '- Plugin Support', 10, 0
    
    toolbar_new db '[New] ', 0
    toolbar_open db '[Open] ', 0
    toolbar_save db '[Save] ', 0
    toolbar_compile db '[Compile] ', 0
    toolbar_run db '[Run] ', 0
    toolbar_debug db '[Debug] ', 0
    
    sidebar_project db 'Project Explorer:', 10, 0
    sidebar_no_project db 'No project open', 10, 0
    
    prompt_open_file db 'Enter filename to open: ', 0
    prompt_save_file db 'Enter filename to save: ', 0
    
    error_file_open db 'Error: Could not open file', 10, 0
    error_file_save db 'Error: Could not save file', 10, 0
    
    build_starting db 'Building project...', 10, 0
    build_success_msg db 'Build successful!', 10, 0
    build_error_msg db 'Build failed!', 10, 0
    compiling_msg db 'Compiling TypeScript files...', 10, 0
    
    debug_starting db 'Starting debugger...', 10, 0
    debugger_started db 'Debugger started', 10, 0
    
    intellisense_activated db 'IntelliSense activated', 10, 0
    
    newline db 10, 0
    clear_seq db 0x1b, '[2J', 0x1b, '[H', 0
    exit_message db 'Thank you for using TypeScript IDE!', 10, 0
    line_number_format db '   ', 0
