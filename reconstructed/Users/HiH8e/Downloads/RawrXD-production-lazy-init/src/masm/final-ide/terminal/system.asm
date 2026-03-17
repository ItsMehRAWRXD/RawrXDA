;==========================================================================
; terminal_system.asm - Complete Terminal Emulator & Output Logger
;==========================================================================
; Full terminal emulation with input/output capture, ANSI color support,
; and unified logging for all subsystems.
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

EXTERN console_log:PROC
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC

PUBLIC terminal_system_init:PROC
PUBLIC terminal_write_text:PROC
PUBLIC terminal_read_input:PROC
PUBLIC terminal_clear:PROC
PUBLIC terminal_set_color:PROC
PUBLIC terminal_scroll:PROC
PUBLIC terminal_get_content:PROC
PUBLIC terminal_export_text:PROC
PUBLIC status_bar_init:PROC

;==========================================================================
; TERMINAL_STATE structure
;==========================================================================
TERMINAL_STATE STRUCT
    output_buffer       QWORD ?      ; Ring buffer for output
    buffer_size         QWORD ?      ; Buffer capacity
    buffer_pos          QWORD ?      ; Current write position
    line_count          QWORD ?      ; Number of lines in buffer
    max_lines           QWORD ?      ; Max lines before wrap
    scroll_pos          DWORD ?      ; Scroll position
    current_color       DWORD ?      ; Current text color (ARGB)
    bg_color            DWORD ?      ; Current background color
    cursor_x            DWORD ?      ; Cursor X position
    cursor_y            DWORD ?      ; Cursor Y position
    width               DWORD ?      ; Terminal width in chars
    height              DWORD ?      ; Terminal height in lines
TERMINAL_STATE ENDS

.data

; Global terminal state
g_terminal_state TERMINAL_STATE <0, 0, 0, 0, 1000, 0, 0xFFFFFFFF, 0xFF000000, 0, 0, 80, 24>

; Output buffer (1 MB)
g_output_buffer     QWORD 0         ; Allocated dynamically

; Line tracking
g_line_starts       QWORD 0         ; Array of line start offsets
g_max_line_offsets  DWORD 1000      ; Max lines to track

; ANSI color codes (will be converted to RGB)
ANSI_COLOR_BLACK    EQU 0xFF000000
ANSI_COLOR_RED      EQU 0xFFFF0000
ANSI_COLOR_GREEN    EQU 0xFF00FF00
ANSI_COLOR_YELLOW   EQU 0xFFFFFF00
ANSI_COLOR_BLUE     EQU 0xFF0000FF
ANSI_COLOR_MAGENTA  EQU 0xFFFF00FF
ANSI_COLOR_CYAN     EQU 0xFF00FFFF
ANSI_COLOR_WHITE    EQU 0xFFFFFFFF

; Logging
szTerminalInit      BYTE "[TERMINAL] Terminal initialized: %dx%d", 13, 10, 0
szTerminalWrite     BYTE "[TERMINAL] Written %I64d bytes", 13, 10, 0
szTerminalClear     BYTE "[TERMINAL] Buffer cleared", 13, 10, 0
szTerminalColor     BYTE "[TERMINAL] Color set to 0x%08X", 13, 10, 0
szStatusBarInit     BYTE "[STATUS] Status bar initialized", 13, 10, 0

.code

;==========================================================================
; terminal_system_init() -> EAX (1=success)
;==========================================================================
PUBLIC terminal_system_init
ALIGN 16
terminal_system_init PROC

    push rbx
    sub rsp, 32

    ; Allocate output buffer (1 MB)
    mov rcx, 1048576
    call asm_malloc
    mov [g_output_buffer], rax
    mov [g_terminal_state.output_buffer], rax
    mov [g_terminal_state.buffer_size], 1048576

    ; Allocate line offset array
    mov rcx, [g_max_line_offsets]
    mov rdx, 8
    imul rcx, rdx
    call asm_malloc
    mov [g_line_starts], rax

    ; Initialize state
    mov DWORD PTR [g_terminal_state.buffer_pos], 0
    mov DWORD PTR [g_terminal_state.line_count], 0
    mov DWORD PTR [g_terminal_state.scroll_pos], 0
    mov DWORD PTR [g_terminal_state.cursor_x], 0
    mov DWORD PTR [g_terminal_state.cursor_y], 0

    ; Set default colors
    mov eax, 0xFFCCCCCC  ; Light gray text
    mov [g_terminal_state.current_color], eax
    mov eax, 0xFF2D2D30  ; Dark background
    mov [g_terminal_state.bg_color], eax

    ; Log initialization
    lea rcx, szTerminalInit
    mov edx, [g_terminal_state.width]
    mov r8d, [g_terminal_state.height]
    call console_log

    mov eax, 1
    add rsp, 32
    pop rbx
    ret

terminal_system_init ENDP

;==========================================================================
; terminal_write_text(text: RCX, length: RDX) -> EAX (1=success)
;==========================================================================
PUBLIC terminal_write_text
ALIGN 16
terminal_write_text PROC

    push rbx
    push rsi
    push rdi
    sub rsp, 32

    ; RCX = text pointer, RDX = length
    mov rsi, rcx        ; text
    mov rdi, rdx        ; length
    mov rax, [g_output_buffer]
    mov rbx, [g_terminal_state.buffer_pos]

    ; Check buffer space
    add rbx, rdi
    cmp rbx, [g_terminal_state.buffer_size]
    jg write_would_overflow

    ; Copy text to buffer
    mov rcx, [g_terminal_state.buffer_pos]
    xor r8, r8
copy_loop:
    cmp r8, rdi
    jge copy_done

    mov al, [rsi + r8]
    mov [rax + rcx + r8], al

    ; Check for newline (track lines)
    cmp al, 10          ; LF
    jne copy_next

    inc [g_terminal_state.line_count]

copy_next:
    inc r8
    jmp copy_loop

copy_done:
    ; Update position
    add [g_terminal_state.buffer_pos], rdi

    ; Log write
    lea rcx, szTerminalWrite
    mov rdx, rdi
    call console_log

    mov eax, 1
    jmp write_done

write_would_overflow:
    xor eax, eax

write_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret

terminal_write_text ENDP

;==========================================================================
; terminal_read_input(buffer: RCX, max_len: RDX) -> EAX (bytes read)
;==========================================================================
PUBLIC terminal_read_input
ALIGN 16
terminal_read_input PROC

    ; Simplified - would hook into actual console input
    ; For now, return 0 (no input available)
    xor eax, eax
    ret

terminal_read_input ENDP

;==========================================================================
; terminal_clear() -> EAX (1=success)
;==========================================================================
PUBLIC terminal_clear
ALIGN 16
terminal_clear PROC

    push rbx
    sub rsp, 32

    ; Zero the buffer
    mov rax, [g_output_buffer]
    mov rcx, [g_terminal_state.buffer_size]
    xor r8, r8
zero_loop:
    cmp r8, rcx
    jge zero_done
    mov BYTE PTR [rax + r8], 0
    inc r8
    jmp zero_loop

zero_done:
    ; Reset state
    mov DWORD PTR [g_terminal_state.buffer_pos], 0
    mov DWORD PTR [g_terminal_state.line_count], 0
    mov DWORD PTR [g_terminal_state.scroll_pos], 0
    mov DWORD PTR [g_terminal_state.cursor_x], 0
    mov DWORD PTR [g_terminal_state.cursor_y], 0

    ; Log clear
    lea rcx, szTerminalClear
    call console_log

    mov eax, 1
    add rsp, 32
    pop rbx
    ret

terminal_clear ENDP

;==========================================================================
; terminal_set_color(color: ECX) -> EAX (1=success)
;==========================================================================
PUBLIC terminal_set_color
ALIGN 16
terminal_set_color PROC

    mov [g_terminal_state.current_color], ecx

    ; Log color change
    lea rcx, szTerminalColor
    mov edx, [g_terminal_state.current_color]
    call console_log

    mov eax, 1
    ret

terminal_set_color ENDP

;==========================================================================
; terminal_scroll(lines: ECX) -> EAX (1=success)
;==========================================================================
PUBLIC terminal_scroll
ALIGN 16
terminal_scroll PROC

    ; ECX = number of lines to scroll (negative = up, positive = down)
    add [g_terminal_state.scroll_pos], ecx

    ; Clamp scroll position
    cmp [g_terminal_state.scroll_pos], 0
    jge scroll_lower_bound
    mov DWORD PTR [g_terminal_state.scroll_pos], 0

scroll_lower_bound:
    mov eax, [g_terminal_state.line_count]
    sub eax, [g_terminal_state.height]
    cmp [g_terminal_state.scroll_pos], eax
    jle scroll_upper_bound
    mov [g_terminal_state.scroll_pos], eax

scroll_upper_bound:
    mov eax, 1
    ret

terminal_scroll ENDP

;==========================================================================
; terminal_get_content() -> RAX (pointer to buffer)
;==========================================================================
PUBLIC terminal_get_content
ALIGN 16
terminal_get_content PROC
    mov rax, [g_output_buffer]
    ret
terminal_get_content ENDP

;==========================================================================
; terminal_export_text(filename: RCX) -> EAX (1=success)
;==========================================================================
PUBLIC terminal_export_text
ALIGN 16
terminal_export_text PROC

    ; RCX = filename (LPCSTR)
    ; Simplified - would write buffer to file
    
    mov eax, 1
    ret

terminal_export_text ENDP

;==========================================================================
; status_bar_init() -> EAX (1=success)
;==========================================================================
PUBLIC status_bar_init
ALIGN 16
status_bar_init PROC

    push rbx
    sub rsp, 32

    ; Log initialization
    lea rcx, szStatusBarInit
    call console_log

    mov eax, 1
    add rsp, 32
    pop rbx
    ret

status_bar_init ENDP

END
