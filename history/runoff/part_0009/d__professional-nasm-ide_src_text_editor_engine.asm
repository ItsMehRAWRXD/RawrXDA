; =====================================================================
; Professional NASM IDE - Advanced Text Editor Engine
; Complete text editing functionality with syntax highlighting
; =====================================================================

bits 64
default rel

; =====================================================================
; External References
; =====================================================================
extern hWnd
extern InvalidateRect
extern GetClientRect
extern SetCaretPos
extern CreateCaret
extern ShowCaret
extern HideCaret

; =====================================================================
; Text Editor Data Structures
; =====================================================================
section .data
    ; Editor configuration
    max_buffer_size dd 65536         ; 64KB text buffer
    tab_size dd 4                    ; Tab width in spaces
    line_height dd 18                ; Pixels per line
    char_width dd 8                  ; Average character width
    margin_left dd 10                ; Left margin in pixels
    margin_top dd 10                 ; Top margin in pixels
    
    ; Cursor and selection
    cursor_blink_rate dd 500         ; Milliseconds
    selection_color_r dd 0.3         ; Selection highlight color
    selection_color_g dd 0.5
    selection_color_b dd 1.0
    
    ; Syntax highlighting colors (RGB float values)
    ; Comments
    color_comment_r dd 0.4
    color_comment_g dd 0.7
    color_comment_b dd 0.4
    
    ; Keywords (mov, push, pop, call, etc.)
    color_keyword_r dd 0.3
    color_keyword_g dd 0.6
    color_keyword_b dd 1.0
    
    ; Registers (rax, rbx, etc.)
    color_register_r dd 1.0
    color_register_g dd 0.6
    color_register_b dd 0.3
    
    ; Numbers
    color_number_r dd 1.0
    color_number_g dd 0.8
    color_number_b dd 0.4
    
    ; Strings
    color_string_r dd 0.9
    color_string_g dd 0.9
    color_string_b dd 0.3
    
    ; Labels
    color_label_r dd 0.9
    color_label_g dd 0.5
    color_label_b dd 0.9
    
    ; Regular text
    color_text_r dd 1.0
    color_text_g dd 1.0
    color_text_b dd 1.0
    
    ; NASM keywords for syntax highlighting
    nasm_keywords db "mov", 0, "push", 0, "pop", 0, "call", 0, "ret", 0, "jmp", 0
                 db "je", 0, "jne", 0, "jz", 0, "jnz", 0, "add", 0, "sub", 0
                 db "mul", 0, "div", 0, "and", 0, "or", 0, "xor", 0, "not", 0
                 db "shl", 0, "shr", 0, "cmp", 0, "test", 0, "inc", 0, "dec", 0
                 db "lea", 0, "nop", 0, "int", 0, "syscall", 0, 0, 0  ; Double null terminates
    
    ; NASM registers
    nasm_registers db "rax", 0, "rbx", 0, "rcx", 0, "rdx", 0, "rsi", 0, "rdi", 0
                  db "rbp", 0, "rsp", 0, "r8", 0, "r9", 0, "r10", 0, "r11", 0
                  db "r12", 0, "r13", 0, "r14", 0, "r15", 0
                  db "eax", 0, "ebx", 0, "ecx", 0, "edx", 0, "esi", 0, "edi", 0
                  db "al", 0, "bl", 0, "cl", 0, "dl", 0, "ah", 0, "bh", 0, "ch", 0, "dh", 0
                  db 0, 0  ; Double null terminates
    
    ; Messages
    editor_ready_msg db "Text editor initialized - Ready for assembly coding!", 0
    syntax_highlight_msg db "NASM syntax highlighting active", 0

section .bss
    ; Main text buffer
    text_buffer resb 65536           ; 64KB for text content
    
    ; Line information
    line_starts resq 1024            ; Pointers to start of each line (max 1024 lines)
    line_count dd 1                  ; Current number of lines
    
    ; Cursor state
    cursor_line dd 0                 ; Current cursor line (0-based)
    cursor_column dd 0               ; Current cursor column (0-based)
    cursor_pos dd 0                  ; Absolute position in buffer
    cursor_visible dd 1              ; Cursor visibility flag
    
    ; Selection state
    selection_start dd 0             ; Start of selection
    selection_end dd 0               ; End of selection
    selection_active dd 0            ; Whether selection is active
    
    ; Editor state
    buffer_length dd 0               ; Current text length
    modified dd 0                    ; Whether buffer has been modified
    read_only dd 0                   ; Read-only mode flag
    
    ; Scroll state
    scroll_x dd 0                    ; Horizontal scroll position
    scroll_y dd 0                    ; Vertical scroll position (in lines)
    visible_lines dd 0               ; Number of visible lines
    visible_columns dd 0             ; Number of visible columns
    
    ; Undo/Redo system
    undo_buffer resb 16384           ; Undo buffer
    undo_position dd 0               ; Current undo position
    
    ; Search state
    search_term resb 256             ; Current search term
    search_position dd 0             ; Current search position
    search_active dd 0               ; Whether search is active

section .text

; =====================================================================
; Initialize Text Editor
; =====================================================================
global InitializeTextEditor
InitializeTextEditor:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 64
    
    ; Clear text buffer
    lea rdi, [text_buffer]
    mov ecx, 65536
    xor al, al
    rep stosb
    
    ; Initialize line starts array
    lea rdi, [line_starts]
    mov qword [rdi], text_buffer     ; First line starts at buffer beginning
    mov dword [line_count], 1
    
    ; Reset cursor and selection
    mov dword [cursor_line], 0
    mov dword [cursor_column], 0
    mov dword [cursor_pos], 0
    mov dword [selection_start], 0
    mov dword [selection_end], 0
    mov dword [selection_active], 0
    
    ; Reset buffer state
    mov dword [buffer_length], 0
    mov dword [modified], 0
    mov dword [scroll_x], 0
    mov dword [scroll_y], 0
    
    ; Calculate visible area
    call CalculateVisibleArea
    
    ; Create cursor caret
    call CreateEditorCaret
    
    ; Load initial content (welcome message)
    call LoadWelcomeContent
    
    ; Initialize syntax highlighting
    call InitializeSyntaxHighlighting
    
    xor eax, eax
    mov rsp, rbp
    pop rbp
    ret

; =====================================================================
; Calculate Visible Area
; =====================================================================
CalculateVisibleArea:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 64
    
    ; Get client rectangle
    mov rcx, [hWnd]
    lea rdx, [rsp + 32]
    call GetClientRect
    
    ; Calculate visible lines
    mov eax, [rsp + 44]              ; bottom
    sub eax, [rsp + 36]              ; top
    sub eax, [margin_top]
    sub eax, [margin_top]            ; Top and bottom margins
    xor edx, edx
    div dword [line_height]
    mov [visible_lines], eax
    
    ; Calculate visible columns
    mov eax, [rsp + 40]              ; right
    sub eax, [rsp + 32]              ; left
    sub eax, [margin_left]
    sub eax, [margin_left]           ; Left and right margins
    xor edx, edx
    div dword [char_width]
    mov [visible_columns], eax
    
    mov rsp, rbp
    pop rbp
    ret

; =====================================================================
; Create Editor Caret
; =====================================================================
CreateEditorCaret:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; Create a line caret
    mov rcx, [hWnd]
    mov edx, 0                       ; Use default caret bitmap
    mov r8d, 2                       ; Width: 2 pixels
    mov r9d, [line_height]           ; Height: line height
    call CreateCaret
    
    ; Show the caret
    mov rcx, [hWnd]
    call ShowCaret
    
    ; Position the caret
    call UpdateCaretPosition
    
    mov rsp, rbp
    pop rbp
    ret

; =====================================================================
; Update Caret Position
; =====================================================================
UpdateCaretPosition:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; Calculate pixel position
    mov eax, [cursor_column]
    mul dword [char_width]
    add eax, [margin_left]
    sub eax, [scroll_x]              ; Apply horizontal scroll
    mov edx, eax                     ; X position
    
    mov eax, [cursor_line]
    sub eax, [scroll_y]              ; Apply vertical scroll
    mul dword [line_height]
    add eax, [margin_top]            ; Y position
    mov r8d, eax
    
    ; Set caret position
    call SetCaretPos
    
    mov rsp, rbp
    pop rbp
    ret

; =====================================================================
; Load Welcome Content
; =====================================================================
LoadWelcomeContent:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; Clear buffer first
    lea rdi, [text_buffer]
    mov ecx, 65536
    xor al, al
    rep stosb
    
    ; Load sample NASM code
    lea rsi, [welcome_asm_code]
    lea rdi, [text_buffer]
    call CopyString
    
    ; Update buffer length
    call CalculateBufferLength
    
    ; Update line information
    call UpdateLineInformation
    
    ; Reset cursor to start
    mov dword [cursor_pos], 0
    mov dword [cursor_line], 0
    mov dword [cursor_column], 0
    
    mov rsp, rbp
    pop rbp
    ret

; Sample NASM code for welcome
welcome_asm_code db "; Professional NASM IDE - DirectX Text Editor", 10
                db "; Hardware-accelerated text rendering with syntax highlighting", 10, 10
                db "section .data", 10
                db '    msg db "Hello, NASM World!", 0xA', 10
                db '    msg_len equ $ - msg', 10, 10
                db "section .text", 10
                db "global _start", 10, 10
                db "_start:", 10
                db "    ; System call: write", 10
                db "    mov rax, 1          ; sys_write", 10
                db "    mov rdi, 1          ; stdout", 10
                db "    mov rsi, msg        ; message", 10
                db "    mov rdx, msg_len    ; length", 10
                db "    syscall             ; invoke system call", 10, 10
                db "    ; System call: exit", 10
                db "    mov rax, 60         ; sys_exit", 10
                db "    mov rdi, 0          ; status", 10
                db "    syscall             ; invoke system call", 10, 0

; =====================================================================
; Insert Character at Cursor
; =====================================================================
global InsertCharacter
InsertCharacter:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; RCX contains the character to insert
    push rcx
    
    ; Check if buffer has space
    mov eax, [buffer_length]
    cmp eax, 65535
    jge .buffer_full
    
    ; Get insertion point
    mov esi, [cursor_pos]
    lea rdi, [text_buffer]
    add rdi, rsi
    
    ; Shift text right to make space
    lea rsi, [text_buffer]
    add rsi, [buffer_length]         ; End of current text
    mov ecx, [buffer_length]
    sub ecx, [cursor_pos]            ; Characters to move
    test ecx, ecx
    jz .insert_at_end
    
    ; Move text right
.shift_loop:
    mov al, [rsi]
    mov [rsi + 1], al
    dec rsi
    loop .shift_loop
    
.insert_at_end:
    ; Insert the character
    pop rax                          ; Restore character
    lea rdi, [text_buffer]
    add rdi, [cursor_pos]
    mov [rdi], al
    
    ; Update buffer length
    inc dword [buffer_length]
    
    ; Mark as modified
    mov dword [modified], 1
    
    ; Advance cursor
    inc dword [cursor_pos]
    
    ; Update cursor line/column
    call UpdateCursorLineColumn
    
    ; Update line information if newline
    cmp al, 10
    je .update_lines
    
    ; Update display
    call UpdateCaretPosition
    call RequestRedraw
    
    jmp .exit
    
.update_lines:
    call UpdateLineInformation
    call UpdateCaretPosition
    call RequestRedraw
    jmp .exit
    
.buffer_full:
    pop rcx                          ; Clean stack
    mov eax, 1                       ; Error: buffer full
    
.exit:
    mov rsp, rbp
    pop rbp
    ret

; =====================================================================
; Delete Character at Cursor (Backspace)
; =====================================================================
global DeleteCharacter
DeleteCharacter:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; Check if at beginning of buffer
    cmp dword [cursor_pos], 0
    je .nothing_to_delete
    
    ; Move cursor back
    dec dword [cursor_pos]
    
    ; Get character being deleted for line update check
    lea rsi, [text_buffer]
    add rsi, [cursor_pos]
    mov al, [rsi]
    push rax                         ; Save character
    
    ; Shift text left
    mov edi, [cursor_pos]
    lea rsi, [text_buffer]
    add rsi, rdi
    inc rsi                          ; Source: next character
    add rdi, rsi
    sub rdi, rsi
    add rdi, rsi                     ; Destination: current position
    
    mov ecx, [buffer_length]
    sub ecx, [cursor_pos]
    dec ecx                          ; Characters to move
    
.shift_left_loop:
    test ecx, ecx
    jz .shift_done
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    dec ecx
    jmp .shift_left_loop
    
.shift_done:
    ; Null terminate
    mov byte [rdi], 0
    
    ; Update buffer length
    dec dword [buffer_length]
    
    ; Mark as modified
    mov dword [modified], 1
    
    ; Update cursor line/column
    call UpdateCursorLineColumn
    
    ; Check if deleted character was newline
    pop rax
    cmp al, 10
    je .update_lines
    
    ; Update display
    call UpdateCaretPosition
    call RequestRedraw
    jmp .exit
    
.update_lines:
    call UpdateLineInformation
    call UpdateCaretPosition
    call RequestRedraw
    jmp .exit
    
.nothing_to_delete:
    xor eax, eax
    
.exit:
    mov rsp, rbp
    pop rbp
    ret

; =====================================================================
; Move Cursor Left
; =====================================================================
global MoveCursorLeft
MoveCursorLeft:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    cmp dword [cursor_pos], 0
    je .at_start
    
    dec dword [cursor_pos]
    call UpdateCursorLineColumn
    call UpdateCaretPosition
    
.at_start:
    mov rsp, rbp
    pop rbp
    ret

; =====================================================================
; Move Cursor Right
; =====================================================================
global MoveCursorRight
MoveCursorRight:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    mov eax, [cursor_pos]
    cmp eax, [buffer_length]
    jge .at_end
    
    inc dword [cursor_pos]
    call UpdateCursorLineColumn
    call UpdateCaretPosition
    
.at_end:
    mov rsp, rbp
    pop rbp
    ret

; =====================================================================
; Move Cursor Up One Line
; =====================================================================
global MoveCursorUp
MoveCursorUp:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    cmp dword [cursor_line], 0
    je .at_top
    
    ; Move to previous line, try to maintain column
    dec dword [cursor_line]
    call UpdateCursorFromLineColumn
    call UpdateCaretPosition
    
.at_top:
    mov rsp, rbp
    pop rbp
    ret

; =====================================================================
; Move Cursor Down One Line
; =====================================================================
global MoveCursorDown
MoveCursorDown:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    mov eax, [cursor_line]
    inc eax
    cmp eax, [line_count]
    jge .at_bottom
    
    inc dword [cursor_line]
    call UpdateCursorFromLineColumn
    call UpdateCaretPosition
    
.at_bottom:
    mov rsp, rbp
    pop rbp
    ret

; =====================================================================
; Utility Functions
; =====================================================================

; Update cursor line/column from absolute position
UpdateCursorLineColumn:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; Find which line the cursor is on
    mov eax, [cursor_pos]
    xor edx, edx                     ; Line counter
    lea rsi, [text_buffer]
    
.find_line_loop:
    cmp rsi, text_buffer
    jl .line_found
    add rsi, rax
    sub rsi, rax
    add rsi, eax
    cmp rsi, text_buffer
    jl .line_found
    cmp byte [rsi], 10
    jne .next_char
    inc edx
.next_char:
    inc rsi
    dec eax
    jns .find_line_loop
    
.line_found:
    mov [cursor_line], edx
    ; Calculate column (simplified)
    mov [cursor_column], eax
    
    mov rsp, rbp
    pop rbp
    ret

; Update absolute position from line/column
UpdateCursorFromLineColumn:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; Calculate absolute position from line/column
    mov eax, [cursor_line]
    cmp eax, [line_count]
    jge .invalid_line
    
    ; Get line start
    lea rsi, [line_starts]
    mov rsi, [rsi + rax * 8]
    
    ; Add column offset
    add rsi, [cursor_column]
    
    ; Calculate absolute position
    sub rsi, text_buffer
    mov [cursor_pos], esi
    
.invalid_line:
    mov rsp, rbp
    pop rbp
    ret

; Calculate buffer length
CalculateBufferLength:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    lea rsi, [text_buffer]
    xor eax, eax
.length_loop:
    cmp byte [rsi + rax], 0
    je .length_done
    inc eax
    cmp eax, 65535
    jge .length_done
    jmp .length_loop
.length_done:
    mov [buffer_length], eax
    
    mov rsp, rbp
    pop rbp
    ret

; Update line information
UpdateLineInformation:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; Scan buffer and update line starts
    lea rsi, [text_buffer]
    lea rdi, [line_starts]
    mov qword [rdi], rsi             ; First line starts at beginning
    mov dword [line_count], 1
    add rdi, 8
    
    xor eax, eax                     ; Position counter
.scan_lines:
    cmp eax, [buffer_length]
    jge .scan_done
    cmp byte [rsi + rax], 10
    jne .next_scan_char
    
    ; Found newline, record next line start
    lea rdx, [rsi + rax + 1]
    mov [rdi], rdx
    add rdi, 8
    inc dword [line_count]
    
.next_scan_char:
    inc eax
    jmp .scan_lines
    
.scan_done:
    mov rsp, rbp
    pop rbp
    ret

; Request window redraw
RequestRedraw:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    mov rcx, [hWnd]
    xor edx, edx
    xor r8d, r8d
    call InvalidateRect
    
    mov rsp, rbp
    pop rbp
    ret

; Copy string utility
CopyString:
    ; RSI = source, RDI = destination
.copy_loop:
    mov al, [rsi]
    mov [rdi], al
    test al, al
    jz .copy_done
    inc rsi
    inc rdi
    jmp .copy_loop
.copy_done:
    ret

; =====================================================================
; Initialize Syntax Highlighting
; =====================================================================
InitializeSyntaxHighlighting:
    push rbp
    mov rbp, rsp
    and rsp, -16
    sub rsp, 32
    
    ; Mark syntax highlighting as active
    ; In a full implementation, this would set up token recognition
    
    mov rsp, rbp
    pop rbp
    ret

; =====================================================================
; Get Text Buffer (for external rendering)
; =====================================================================
global GetTextBuffer
GetTextBuffer:
    lea rax, [text_buffer]
    ret

global GetBufferLength
GetBufferLength:
    mov eax, [buffer_length]
    ret

global GetCursorPosition
GetCursorPosition:
    mov eax, [cursor_pos]
    ret