; ASM++ IDE Project Templates
; Pre-built project templates for common assembly development scenarios

BITS 64
DEFAULT REL

section .data
    ; Console Application Template
    console_template db '; Console Application Template', 10
                    db '; Created by ASM++ IDE', 10, 10
                    db 'BITS 64', 10
                    db 'DEFAULT REL', 10, 10
                    db 'section .data', 10
                    db '    msg db "Hello, World!", 10, 0', 10
                    db '    msg_len equ $ - msg - 1', 10, 10
                    db 'section .text', 10
                    db '    global _start', 10, 10
                    db '_start:', 10
                    db '    ; Display message', 10
                    db '    mov rdi, 1      ; STDOUT', 10
                    db '    mov rsi, msg', 10
                    db '    mov rdx, msg_len', 10
                    db '    mov rax, 1      ; SYS_WRITE', 10
                    db '    syscall', 10, 10
                    db '    ; Exit', 10
                    db '    mov rdi, 0', 10
                    db '    mov rax, 60     ; SYS_EXIT', 10
                    db '    syscall', 10, 0
    
    ; Static Library Template
    library_template db '; Static Library Template', 10
                     db '; Created by ASM++ IDE', 10, 10
                     db 'BITS 64', 10
                     db 'DEFAULT REL', 10, 10
                     db 'section .data', 10
                     db '    ; Library data', 10, 10
                     db 'section .text', 10
                     db '    global library_function', 10, 10
                     db '; Library function', 10
                     db 'library_function:', 10
                     db '    push rbp', 10
                     db '    mov rbp, rsp', 10, 10
                     db '    ; Function implementation', 10
                     db '    mov rax, 0      ; Return value', 10, 10
                     db '    pop rbp', 10
                     db '    ret', 10, 0
    
    ; Dynamic Library Template
    dll_template db '; Dynamic Library Template', 10
                 db '; Created by ASM++ IDE', 10, 10
                 db 'BITS 64', 10
                 db 'DEFAULT REL', 10, 10
                 db 'section .data', 10
                 db '    ; DLL data', 10, 10
                 db 'section .text', 10
                 db '    global DllMain', 10
                 db '    global exported_function', 10, 10
                 db '; DLL entry point', 10
                 db 'DllMain:', 10
                 db '    mov rax, 1      ; Return TRUE', 10
                 db '    ret', 10, 10
                 db '; Exported function', 10
                 db 'exported_function:', 10
                 db '    push rbp', 10
                 db '    mov rbp, rsp', 10, 10
                 db '    ; Function implementation', 10
                 db '    mov rax, 0      ; Return value', 10, 10
                 db '    pop rbp', 10
                 db '    ret', 10, 0
    
    ; Kernel Module Template
    kernel_template db '; Kernel Module Template', 10
                   db '; Created by ASM++ IDE', 10, 10
                   db 'BITS 64', 10
                   db 'DEFAULT REL', 10, 10
                   db 'section .text', 10
                   db '    global kernel_entry', 10, 10
                   db '; Kernel entry point', 10
                   db 'kernel_entry:', 10
                   db '    ; Initialize kernel module', 10
                   db '    call init_kernel', 10, 10
                   db '    ; Main kernel loop', 10
                   db '.kernel_loop:', 10
                   db '    ; Process kernel tasks', 10
                   db '    call process_kernel_tasks', 10
                   db '    jmp .kernel_loop', 10, 10
                   db '; Initialize kernel', 10
                   db 'init_kernel:', 10
                   db '    push rbp', 10
                   db '    mov rbp, rsp', 10, 10
                   db '    ; Kernel initialization code', 10, 10
                   db '    pop rbp', 10
                   db '    ret', 10, 10
                   db '; Process kernel tasks', 10
                   db 'process_kernel_tasks:', 10
                   db '    push rbp', 10
                   db '    mov rbp, rsp', 10, 10
                   db '    ; Task processing code', 10, 10
                   db '    pop rbp', 10
                   db '    ret', 10, 0
    
    ; Bootloader Template
    bootloader_template db '; Bootloader Template', 10
                       db '; Created by ASM++ IDE', 10, 10
                       db 'BITS 16', 10
                       db 'DEFAULT REL', 10, 10
                       db 'section .text', 10
                       db '    global _start', 10, 10
                       db '_start:', 10
                       db '    ; Set up segment registers', 10
                       db '    mov ax, 0x07C0', 10
                       db '    mov ds, ax', 10
                       db '    mov es, ax', 10, 10
                       db '    ; Display boot message', 10
                       db '    mov si, boot_msg', 10
                       db '    call print_string', 10, 10
                       db '    ; Load kernel', 10
                       db '    call load_kernel', 10, 10
                       db '    ; Jump to kernel', 10
                       db '    jmp 0x1000:0x0000', 10, 10
                       db '; Print string function', 10
                       db 'print_string:', 10
                       db '    push ax', 10
                       db '    push bx', 10, 10
                       db '.print_loop:', 10
                       db '    lodsb', 10
                       db '    cmp al, 0', 10
                       db '    je .print_done', 10
                       db '    mov ah, 0x0E', 10
                       db '    int 0x10', 10
                       db '    jmp .print_loop', 10, 10
                       db '.print_done:', 10
                       db '    pop bx', 10
                       db '    pop ax', 10
                       db '    ret', 10, 10
                       db '; Load kernel function', 10
                       db 'load_kernel:', 10
                       db '    ; Kernel loading code', 10
                       db '    ret', 10, 10
                       db 'section .data', 10
                       db '    boot_msg db "Bootloader starting...", 10, 13, 0', 10, 0
    
    ; Game Engine Template
    game_template db '; Game Engine Template', 10
                 db '; Created by ASM++ IDE', 10, 10
                 db 'BITS 64', 10
                 db 'DEFAULT REL', 10, 10
                 db 'section .data', 10
                 db '    ; Game data', 10
                 db '    game_title db "ASM++ Game Engine", 0', 10
                 db '    game_running db 1', 10, 10
                 db 'section .text', 10
                 db '    global _start', 10, 10
                 db '_start:', 10
                 db '    ; Initialize game', 10
                 db '    call init_game', 10, 10
                 db '    ; Main game loop', 10
                 db '.game_loop:', 10
                 db '    ; Handle input', 10
                 db '    call handle_input', 10, 10
                 db '    ; Update game state', 10
                 db '    call update_game', 10, 10
                 db '    ; Render frame', 10
                 db '    call render_frame', 10, 10
                 db '    ; Check if game should continue', 10
                 db '    cmp byte [game_running], 0', 10
                 db '    jne .game_loop', 10, 10
                 db '    ; Exit game', 10
                 db '    mov rdi, 0', 10
                 db '    mov rax, 60', 10
                 db '    syscall', 10, 10
                 db '; Initialize game', 10
                 db 'init_game:', 10
                 db '    push rbp', 10
                 db '    mov rbp, rsp', 10, 10
                 db '    ; Game initialization code', 10, 10
                 db '    pop rbp', 10
                 db '    ret', 10, 10
                 db '; Handle input', 10
                 db 'handle_input:', 10
                 db '    push rbp', 10
                 db '    mov rbp, rsp', 10, 10
                 db '    ; Input handling code', 10, 10
                 db '    pop rbp', 10
                 db '    ret', 10, 10
                 db '; Update game', 10
                 db 'update_game:', 10
                 db '    push rbp', 10
                 db '    mov rbp, rsp', 10, 10
                 db '    ; Game update code', 10, 10
                 db '    pop rbp', 10
                 db '    ret', 10, 10
                 db '; Render frame', 10
                 db 'render_frame:', 10
                 db '    push rbp', 10
                 db '    mov rbp, rsp', 10, 10
                 db '    ; Rendering code', 10, 10
                 db '    pop rbp', 10
                 db '    ret', 10, 0
    
    ; Template names
    template_names db 'Console Application', 0
                  db 'Static Library', 0
                  db 'Dynamic Library', 0
                  db 'Kernel Module', 0
                  db 'Bootloader', 0
                  db 'Game Engine', 0
    
    ; Template descriptions
    template_descriptions db 'Simple console application with main function', 0
                         db 'Static library with reusable functions', 0
                         db 'Dynamic library with exported functions', 0
                         db 'Kernel module for system programming', 0
                         db 'Bootloader for system startup', 0
                         db 'Game engine with main loop and rendering', 0

section .text
    global get_template
    global get_template_count
    global get_template_name
    global get_template_description

; Get template by index
get_template:
    push rbp
    mov rbp, rsp
    
    ; Check if index is valid
    cmp rdi, 0
    jl .invalid_index
    cmp rdi, 5
    jg .invalid_index
    
    ; Get template based on index
    cmp rdi, 0
    je .console_template
    cmp rdi, 1
    je .library_template
    cmp rdi, 2
    je .dll_template
    cmp rdi, 3
    je .kernel_template
    cmp rdi, 4
    je .bootloader_template
    cmp rdi, 5
    je .game_template
    
.console_template:
    mov rax, console_template
    jmp .template_done
    
.library_template:
    mov rax, library_template
    jmp .template_done
    
.dll_template:
    mov rax, dll_template
    jmp .template_done
    
.kernel_template:
    mov rax, kernel_template
    jmp .template_done
    
.bootloader_template:
    mov rax, bootloader_template
    jmp .template_done
    
.game_template:
    mov rax, game_template
    jmp .template_done
    
.invalid_index:
    mov rax, 0
    
.template_done:
    pop rbp
    ret

; Get template count
get_template_count:
    mov rax, 6
    ret

; Get template name by index
get_template_name:
    push rbp
    mov rbp, rsp
    
    ; Check if index is valid
    cmp rdi, 0
    jl .invalid_name_index
    cmp rdi, 5
    jg .invalid_name_index
    
    ; Get template name based on index
    mov rax, template_names
    jmp .name_done
    
.invalid_name_index:
    mov rax, 0
    
.name_done:
    pop rbp
    ret

; Get template description by index
get_template_description:
    push rbp
    mov rbp, rsp
    
    ; Check if index is valid
    cmp rdi, 0
    jl .invalid_desc_index
    cmp rdi, 5
    jg .invalid_desc_index
    
    ; Get template description based on index
    mov rax, template_descriptions
    jmp .desc_done
    
.invalid_desc_index:
    mov rax, 0
    
.desc_done:
    pop rbp
    ret
