;==========================================================================
; custom_window.asm - Zero-Dependency Window Management for RawrXD IDE
; ==========================================================================
; Implements window creation, messaging, and UI without user32.lib
; Uses direct VGA graphics and keyboard/mouse interrupts
;==========================================================================

option casemap:none

;==========================================================================
; VGA GRAPHICS CONSTANTS
;==========================================================================
VGA_BASE                equ 0A0000h
VGA_TEXT_BASE           equ 0B8000h
VGA_MODE_13H            equ 13h
VGA_MODE_3              equ 3h
VGA_WIDTH               equ 80
VGA_HEIGHT              equ 25
VGA_CHAR_SIZE           equ 2

;==========================================================================
; KEYBOARD/MOUSE CONSTANTS
;==========================================================================
KEYBOARD_PORT           equ 60h
MOUSE_PORT              equ 60h
PS2_CONTROLLER          equ 64h

;==========================================================================
; WINDOW STRUCTURES
;==========================================================================
WINDOW STRUCT
    x                   DWORD ?
    y                   DWORD ?
    width               DWORD ?
    height              DWORD ?
    title               QWORD ?
    buffer              QWORD ?
    parent              QWORD ?
    children            QWORD ?
    next                QWORD ?
WINDOW ENDS

EVENT STRUCT
    type                DWORD ?
    x                   DWORD ?
    y                   DWORD ?
    key                 DWORD ?
    button              DWORD ?
    window              QWORD ?
EVENT ENDS

;==========================================================================
; GLOBAL VARIABLES
;==========================================================================
.data
    main_window         QWORD ?
    event_queue         QWORD ?
    queue_head          DWORD 0
    queue_tail          DWORD 0
    mouse_x             DWORD 0
    mouse_y             DWORD 0
    mouse_buttons       DWORD 0
    keyboard_state      BYTE 256 DUP(0)
    
    ; Window class registry
    window_list         QWORD ?
    window_count        DWORD 0
    
    ; VGA palette for 16 colors
    vga_palette         BYTE 0,0,0, 0,0,42, 0,42,0, 0,42,42
                        BYTE 42,0,0, 42,0,42, 42,21,0, 42,42,42
                        BYTE 21,21,21, 21,21,63, 21,63,21, 21,63,63
                        BYTE 63,21,21, 63,21,63, 63,63,21, 63,63,63

;==========================================================================
; INITIALIZATION
;==========================================================================
.code

;--------------------------------------------------------------------------
; init_vga_graphics - Initialize VGA graphics mode
;--------------------------------------------------------------------------
init_vga_graphics PROC
    ; Set VGA mode 3 (80x25 text)
    mov ax, 3
    int 10h
    
    ; Clear screen
    mov ax, 0600h
    xor cx, cx
    mov dx, 184Fh
    mov bh, 7
    int 10h
    
    ; Hide cursor
    mov ah, 1
    mov ch, 20h
    int 10h
    
    ret
init_vga_graphics ENDP

;--------------------------------------------------------------------------
; init_input_system - Initialize keyboard and mouse
;--------------------------------------------------------------------------
init_input_system PROC
    ; Enable keyboard
    mov al, 0AEh
    out PS2_CONTROLLER, al
    
    ; Enable mouse (if present)
    call enable_ps2_mouse
    
    ret
init_input_system ENDP

;--------------------------------------------------------------------------
; enable_ps2_mouse - Enable PS/2 mouse
;--------------------------------------------------------------------------
enable_ps2_mouse PROC
    ; Enable auxiliary device
    mov al, 0A8h
    out PS2_CONTROLLER, al
    
    ; Enable mouse
    mov al, 0F4h
    call send_mouse_command
    
    ret
enable_ps2_mouse ENDP

;--------------------------------------------------------------------------
; send_mouse_command - Send command to mouse
; al = command
;--------------------------------------------------------------------------
send_mouse_command PROC
    ; Wait for input buffer empty
    mov cx, 1000
wait_mouse_ready:
    in al, PS2_CONTROLLER
    test al, 2
    jz mouse_ready
    loop wait_mouse_ready
mouse_ready:
    mov al, 0D4h
    out PS2_CONTROLLER, al
    
    ; Wait again
    mov cx, 1000
wait_mouse_ready2:
    in al, PS2_CONTROLLER
    test al, 2
    jz mouse_ready2
    loop wait_mouse_ready2
mouse_ready2:
    mov al, 0F4h
    out KEYBOARD_PORT, al
    
    ret
send_mouse_command ENDP

;==========================================================================
; WINDOW MANAGEMENT
;==========================================================================

;--------------------------------------------------------------------------
; create_window - Create a new window
; rcx = x, rdx = y, r8 = width, r9 = height
; Returns: window handle in rax
;--------------------------------------------------------------------------
create_window PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Allocate window structure
    mov rcx, sizeof WINDOW
    call sys_alloc_memory
    mov rsi, rax
    
    ; Initialize window
    mov DWORD PTR [rsi + WINDOW.x], ecx
    mov DWORD PTR [rsi + WINDOW.y], edx
    mov DWORD PTR [rsi + WINDOW.width], r8d
    mov DWORD PTR [rsi + WINDOW.height], r9d
    mov QWORD PTR [rsi + WINDOW.title], 0
    mov QWORD PTR [rsi + WINDOW.parent], 0
    mov QWORD PTR [rsi + WINDOW.children], 0
    mov QWORD PTR [rsi + WINDOW.next], 0
    
    ; Allocate window buffer
    mov eax, r8d
    mul r9d
    mov rcx, rax
    call sys_alloc_memory
    mov QWORD PTR [rsi + WINDOW.buffer], rax
    
    ; Add to window list
    mov rax, window_list
    test rax, rax
    jnz add_to_list
    mov window_list, rsi
    jmp window_created
    
add_to_list:
    mov rdi, rax
find_list_end:
    mov rax, QWORD PTR [rdi + WINDOW.next]
    test rax, rax
    jz add_window
    mov rdi, rax
    jmp find_list_end
    
add_window:
    mov QWORD PTR [rdi + WINDOW.next], rsi
    
window_created:
    inc DWORD PTR window_count
    mov rax, rsi
    
    leave
    ret
create_window ENDP

;--------------------------------------------------------------------------
; destroy_window - Destroy a window
; rcx = window handle
;--------------------------------------------------------------------------
destroy_window PROC
    push rbp
    mov rbp, rsp
    
    mov rsi, rcx
    
    ; Free window buffer
    mov rcx, QWORD PTR [rsi + WINDOW.buffer]
    call sys_free_memory
    
    ; Remove from window list
    mov rax, window_list
    cmp rax, rsi
    jne find_window
    mov rax, QWORD PTR [rsi + WINDOW.next]
    mov window_list, rax
    jmp free_window
    
find_window:
    mov rdi, rax
find_window_loop:
    mov rax, QWORD PTR [rdi + WINDOW.next]
    cmp rax, rsi
    je remove_window
    test rax, rax
    jz free_window
    mov rdi, rax
    jmp find_window_loop
    
remove_window:
    mov rax, QWORD PTR [rsi + WINDOW.next]
    mov QWORD PTR [rdi + WINDOW.next], rax
    
free_window:
    ; Free window structure
    mov rcx, rsi
    call sys_free_memory
    
    dec DWORD PTR window_count
    
    leave
    ret
destroy_window ENDP

;==========================================================================
; GRAPHICS RENDERING
;==========================================================================

;--------------------------------------------------------------------------
; draw_window - Draw a window to screen
; rcx = window handle
;--------------------------------------------------------------------------
draw_window PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov rsi, rcx
    
    ; Calculate screen position
    mov eax, DWORD PTR [rsi + WINDOW.y]
    mov ebx, VGA_WIDTH
    mul ebx
    add eax, DWORD PTR [rsi + WINDOW.x]
    mov edi, eax
    
    ; Get window dimensions
    mov ecx, DWORD PTR [rsi + WINDOW.height]
    mov edx, DWORD PTR [rsi + WINDOW.width]
    
    ; Get window buffer
    mov r8, QWORD PTR [rsi + WINDOW.buffer]
    
draw_window_loop_y:
    push rcx
    push rdi
    mov ecx, edx
    
draw_window_loop_x:
    mov al, BYTE PTR [r8]
    mov ah, 7  ; Gray on black
    mov WORD PTR [VGA_TEXT_BASE + rdi*2], ax
    inc r8
    inc rdi
    loop draw_window_loop_x
    
    pop rdi
    pop rcx
    add edi, VGA_WIDTH
    loop draw_window_loop_y
    
    leave
    ret
draw_window ENDP

;--------------------------------------------------------------------------
; clear_screen - Clear the entire screen
;--------------------------------------------------------------------------
clear_screen PROC
    mov rdi, VGA_TEXT_BASE
    mov rcx, VGA_WIDTH * VGA_HEIGHT
    mov ax, 0720h  ; Space with gray attribute
    rep stosw
    ret
clear_screen ENDP

;==========================================================================
; INPUT PROCESSING
;==========================================================================

;--------------------------------------------------------------------------
; poll_input - Poll keyboard and mouse input
; Returns: event in rax (0 if no event)
;--------------------------------------------------------------------------
poll_input PROC
    push rbp
    mov rbp, rsp
    
    ; Check keyboard
    in al, PS2_CONTROLLER
    test al, 1
    jz check_mouse
    
    ; Read keyboard scan code
    in al, KEYBOARD_PORT
    call process_keyboard
    test rax, rax
    jnz input_done
    
check_mouse:
    ; Check mouse
    in al, PS2_CONTROLLER
    test al, 1
    jz no_input
    
    ; Read mouse data
    in al, MOUSE_PORT
    call process_mouse
    test rax, rax
    jnz input_done
    
no_input:
    xor rax, rax
    
input_done:
    leave
    ret
poll_input ENDP

;--------------------------------------------------------------------------
; process_keyboard - Process keyboard input
; al = scan code
; Returns: event in rax
;--------------------------------------------------------------------------
process_keyboard PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Allocate event
    mov rcx, sizeof EVENT
    call sys_alloc_memory
    mov rsi, rax
    
    ; Set event type
    mov DWORD PTR [rsi + EVENT.type], 1  ; KEY_EVENT
    mov DWORD PTR [rsi + EVENT.key], eax
    
    ; Find focused window
    mov rax, main_window
    mov QWORD PTR [rsi + EVENT.window], rax
    
    mov rax, rsi
    leave
    ret
process_keyboard ENDP

;--------------------------------------------------------------------------
; process_mouse - Process mouse input
; al = mouse data
; Returns: event in rax
;--------------------------------------------------------------------------
process_mouse PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; This is simplified - real implementation would parse mouse packets
    
    ; Allocate event
    mov rcx, sizeof EVENT
    call sys_alloc_memory
    mov rsi, rax
    
    ; Set event type
    mov DWORD PTR [rsi + EVENT.type], 2  ; MOUSE_EVENT
    mov DWORD PTR [rsi + EVENT.x], eax   ; Simplified
    mov DWORD PTR [rsi + EVENT.y], 0
    
    ; Find window under mouse
    call find_window_at
    mov QWORD PTR [rsi + EVENT.window], rax
    
    mov rax, rsi
    leave
    ret
process_mouse ENDP

;--------------------------------------------------------------------------
; find_window_at - Find window at coordinates
; Returns: window handle in rax
;--------------------------------------------------------------------------
find_window_at PROC
    mov rax, window_list
    
find_window_loop:
    test rax, rax
    jz no_window
    
    ; Check if coordinates are within window
    ; Simplified - always return main window
    mov rax, main_window
    ret
    
no_window:
    xor rax, rax
    ret
find_window_at ENDP

;==========================================================================
; MESSAGE PUMP
;==========================================================================

;--------------------------------------------------------------------------
; message_loop - Main message processing loop
;--------------------------------------------------------------------------
message_loop PROC
message_loop_start:
    ; Poll for input
    call poll_input
    test rax, rax
    jz no_event
    
    ; Process event
    mov rcx, rax
    call dispatch_event
    
    ; Free event
    call sys_free_memory
    
no_event:
    ; Redraw if needed
    call redraw_all
    
    ; Small delay
    mov rcx, 10
    call sys_sleep
    
    jmp message_loop_start
    
    ret
message_loop ENDP

;--------------------------------------------------------------------------
; dispatch_event - Dispatch event to window
; rcx = event
;--------------------------------------------------------------------------
dispatch_event PROC
    push rbp
    mov rbp, rsp
    
    mov rsi, rcx
    mov rax, QWORD PTR [rsi + EVENT.window]
    test rax, rax
    jz dispatch_done
    
    ; Call window event handler
    ; This would call a function pointer in the window structure
    
dispatch_done:
    leave
    ret
dispatch_event ENDP

;--------------------------------------------------------------------------
; redraw_all - Redraw all windows
;--------------------------------------------------------------------------
redraw_all PROC
    push rbp
    mov rbp, rsp
    
    ; Clear screen
    call clear_screen
    
    ; Draw all windows
    mov rax, window_list
redraw_loop:
    test rax, rax
    jz redraw_done
    
    mov rcx, rax
    call draw_window
    
    mov rax, QWORD PTR [rax + WINDOW.next]
    jmp redraw_loop
    
redraw_done:
    leave
    ret
redraw_all ENDP

;==========================================================================
; TEXT OUTPUT
;==========================================================================

;--------------------------------------------------------------------------
; draw_text - Draw text in a window
; rcx = window, rdx = x, r8 = y, r9 = text
;--------------------------------------------------------------------------
draw_text PROC
    push rbp
    mov rbp, rsp
    
    mov rsi, rcx
    mov edi, r8d
    mov ebx, DWORD PTR [rsi + WINDOW.width]
    mul ebx
    add eax, edx
    mov edi, eax
    
    ; Get window buffer offset
    mov rax, QWORD PTR [rsi + WINDOW.buffer]
    add rax, rdi
    
    ; Copy text
    mov rsi, r9
text_copy_loop:
    mov al, BYTE PTR [rsi]
    test al, al
    jz text_done
    mov BYTE PTR [rax], al
    inc rax
    inc rsi
    jmp text_copy_loop
    
text_done:
    leave
    ret
draw_text ENDP

;==========================================================================
; EXPORTS
;==========================================================================
PUBLIC init_vga_graphics
PUBLIC init_input_system
PUBLIC create_window
PUBLIC destroy_window
PUBLIC draw_window
PUBLIC clear_screen
PUBLIC poll_input
PUBLIC message_loop
PUBLIC draw_text

END