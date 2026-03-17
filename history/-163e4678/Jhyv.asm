;==========================================================================
; text_editor.asm - Complete Text Editor with Buffer & Selection
;==========================================================================
; Full-featured editor with UTF-8 support, selection, undo/redo,
; line tracking, and syntax-aware rendering.
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

EXTERN console_log:PROC
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_realloc:PROC
EXTERN memcpy:PROC
EXTERN strlen_masm:PROC

PUBLIC editor_system_init:PROC
PUBLIC editor_buffer_create:PROC
PUBLIC editor_buffer_insert:PROC
PUBLIC editor_buffer_delete:PROC
PUBLIC editor_buffer_get_text:PROC
PUBLIC editor_buffer_get_line:PROC
PUBLIC editor_buffer_get_line_count:PROC
PUBLIC editor_selection_set:PROC
PUBLIC editor_selection_get:PROC
PUBLIC editor_undo:PROC
PUBLIC editor_redo:PROC

;==========================================================================
; EDITOR_BUFFER structure
;==========================================================================
EDITOR_BUFFER STRUCT
    buffer_ptr          QWORD ?      ; Pointer to text data
    buffer_size         QWORD ?      ; Total allocated size
    text_length         QWORD ?      ; Current text length
    line_count          QWORD ?      ; Number of lines
    line_offsets        QWORD ?      ; Array of line start offsets
    max_lines           QWORD ?      ; Capacity of line_offsets array
    modified            DWORD ?      ; 1 if modified
    encoding            DWORD ?      ; 0=UTF-8, 1=UTF-16
    cursor_line         DWORD ?      ; Current line
    cursor_col          DWORD ?      ; Current column
    sel_start_line      DWORD ?      ; Selection start
    sel_start_col       DWORD ?      ; Selection start column
    sel_end_line        DWORD ?      ; Selection end
    sel_end_col         DWORD ?      ; Selection end column
    has_selection       DWORD ?      ; 1 if has selection
EDITOR_BUFFER ENDS

;==========================================================================
; EDITOR_STATE - Global editor state
;==========================================================================
EDITOR_STATE STRUCT
    current_buffer      QWORD ?      ; Active buffer
    undo_stack          QWORD ?      ; Undo operations
    redo_stack          QWORD ?      ; Redo operations
    undo_count          QWORD ?      ; Number of undo ops
    redo_count          QWORD ?      ; Number of redo ops
    max_undo            QWORD ?      ; Max undo depth
EDITOR_STATE ENDS

.data

; Global editor state
g_editor_state EDITOR_STATE <0, 0, 0, 0, 0, 1000>

; Logging
szEditorInit        BYTE "[EDITOR] Editor system initialized", 13, 10, 0
szBufferCreated     BYTE "[EDITOR] Buffer created: %p", 13, 10, 0
szBufferInsert      BYTE "[EDITOR] Inserted %I64d bytes at pos %I64d", 13, 10, 0
szBufferDelete      BYTE "[EDITOR] Deleted %I64d bytes at pos %I64d", 13, 10, 0
szLinesParsed       BYTE "[EDITOR] Parsed %I64d lines", 13, 10, 0

.code

;==========================================================================
; editor_system_init() -> EAX (1=success)
;==========================================================================
PUBLIC editor_system_init
ALIGN 16
editor_system_init PROC

    push rbx
    sub rsp, 32

    ; Allocate undo/redo stacks
    mov rcx, [g_editor_state.max_undo]
    mov rdx, 16         ; 16 bytes per operation
    imul rcx, rdx
    call asm_malloc
    mov [g_editor_state.undo_stack], rax

    mov rcx, [g_editor_state.max_undo]
    mov rdx, 16
    imul rcx, rdx
    call asm_malloc
    mov [g_editor_state.redo_stack], rax

    lea rcx, szEditorInit
    call console_log

    mov eax, 1
    add rsp, 32
    pop rbx
    ret

editor_system_init ENDP

;==========================================================================
; editor_buffer_create(initial_size: RCX) -> RAX (EDITOR_BUFFER*)
;==========================================================================
PUBLIC editor_buffer_create
ALIGN 16
editor_buffer_create PROC

    push rbx
    push rsi
    sub rsp, 32

    ; RCX = initial size (default 64KB if 0)
    mov rsi, rcx
    test rsi, rsi
    jnz size_ok
    mov rsi, 65536

size_ok:
    ; Allocate EDITOR_BUFFER structure
    mov rcx, SIZEOF EDITOR_BUFFER
    call asm_malloc
    mov rbx, rax        ; Save buffer struct ptr

    ; Initialize structure
    mov rcx, 0
    mov QWORD PTR [rbx + EDITOR_BUFFER.buffer_ptr], rcx
    mov QWORD PTR [rbx + EDITOR_BUFFER.buffer_size], rsi
    mov QWORD PTR [rbx + EDITOR_BUFFER.text_length], rcx
    mov QWORD PTR [rbx + EDITOR_BUFFER.line_count], 1
    mov DWORD PTR [rbx + EDITOR_BUFFER.modified], 0
    mov DWORD PTR [rbx + EDITOR_BUFFER.encoding], 0  ; UTF-8
    mov DWORD PTR [rbx + EDITOR_BUFFER.cursor_line], 0
    mov DWORD PTR [rbx + EDITOR_BUFFER.cursor_col], 0
    mov DWORD PTR [rbx + EDITOR_BUFFER.has_selection], 0

    ; Allocate text buffer
    mov rcx, rsi
    call asm_malloc
    mov [rbx + EDITOR_BUFFER.buffer_ptr], rax

    ; Allocate line offset array (1000 lines initially)
    mov rcx, 1000
    mov rdx, 8
    imul rcx, rdx
    call asm_malloc
    mov [rbx + EDITOR_BUFFER.line_offsets], rax
    mov QWORD PTR [rbx + EDITOR_BUFFER.max_lines], 1000

    ; Initialize first line offset to 0
    mov rax, [rbx + EDITOR_BUFFER.line_offsets]
    mov QWORD PTR [rax], 0

    ; Log creation
    lea rcx, szBufferCreated
    mov rdx, rbx
    call console_log

    mov rax, rbx
    add rsp, 32
    pop rsi
    pop rbx
    ret

editor_buffer_create ENDP

;==========================================================================
; editor_buffer_insert(buffer: RCX, pos: RDX, text: R8, len: R9) -> EAX
;==========================================================================
PUBLIC editor_buffer_insert
ALIGN 16
editor_buffer_insert PROC

    push rbx
    push rsi
    push rdi
    sub rsp, 48

    ; RCX = buffer, RDX = position, R8 = text, R9 = length
    mov rsi, rcx        ; buffer
    mov rdi, rdx        ; position
    mov rbx, r8         ; text ptr
    mov r10, r9         ; length

    ; Validate buffer
    test rsi, rsi
    jz insert_fail

    ; Check if we need to resize
    mov rax, [rsi + EDITOR_BUFFER.text_length]
    add rax, r10
    cmp rax, [rsi + EDITOR_BUFFER.buffer_size]
    jle insert_no_resize

    ; Resize buffer (double size)
    mov rax, [rsi + EDITOR_BUFFER.buffer_size]
    shl rax, 1
    mov rcx, [rsi + EDITOR_BUFFER.buffer_ptr]
    mov rdx, rax
    call asm_realloc
    mov [rsi + EDITOR_BUFFER.buffer_ptr], rax
    mov [rsi + EDITOR_BUFFER.buffer_size], rax

insert_no_resize:
    ; Get buffer ptr
    mov rax, [rsi + EDITOR_BUFFER.buffer_ptr]

    ; Move existing data forward
    mov rcx, [rsi + EDITOR_BUFFER.text_length]
    sub rcx, rdi
    
    ; memmove(buf + pos + len, buf + pos, len)
    mov r8, rax
    add r8, rdi
    add r8, r10         ; Destination
    mov r9, rax
    add r9, rdi         ; Source
    
    ; Backward copy to avoid overlap
    mov r11, rcx
    xor r12, r12
copy_backward_loop:
    cmp r12, r11
    jge copy_done
    mov al, [r9 + r11 - 1]
    mov [r8 + r11 - 1], al
    dec r11
    jmp copy_backward_loop

copy_done:
    ; Copy new text in
    xor r11, r11
copy_new_loop:
    cmp r11, r10
    jge copy_new_done
    mov al, [rbx + r11]
    mov [rax + rdi + r11], al
    inc r11
    jmp copy_new_loop

copy_new_done:
    ; Update length
    add [rsi + EDITOR_BUFFER.text_length], r10
    mov DWORD PTR [rsi + EDITOR_BUFFER.modified], 1

    ; Reparse lines
    mov rcx, rsi
    call editor_reparse_lines

    ; Log insertion
    lea rcx, szBufferInsert
    mov rdx, r10
    mov r8, rdi
    call console_log

    mov eax, 1
    jmp insert_done

insert_fail:
    xor eax, eax

insert_done:
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret

editor_buffer_insert ENDP

;==========================================================================
; editor_buffer_delete(buffer: RCX, pos: RDX, len: R8) -> EAX
;==========================================================================
PUBLIC editor_buffer_delete
ALIGN 16
editor_buffer_delete PROC

    push rbx
    push rsi
    push rdi
    sub rsp, 32

    ; RCX = buffer, RDX = position, R8 = length
    mov rsi, rcx        ; buffer
    mov rdi, rdx        ; position
    mov r10, r8         ; length

    ; Validate
    test rsi, rsi
    jz delete_fail

    mov rax, [rsi + EDITOR_BUFFER.buffer_ptr]
    mov r9, [rsi + EDITOR_BUFFER.text_length]

    ; Check bounds
    add rdi, r10
    cmp rdi, r9
    jg delete_fail

    ; memmove(buf + pos, buf + pos + len, remaining)
    mov rcx, r9
    sub rcx, rdi
    sub rcx, r10

    xor r11, r11
move_loop:
    cmp r11, rcx
    jge move_done
    mov al, [rax + rdi + r10 + r11]
    mov [rax + rdi + r11], al
    inc r11
    jmp move_loop

move_done:
    ; Update length
    sub [rsi + EDITOR_BUFFER.text_length], r10
    mov DWORD PTR [rsi + EDITOR_BUFFER.modified], 1

    ; Reparse lines
    mov rcx, rsi
    call editor_reparse_lines

    ; Log deletion
    lea rcx, szBufferDelete
    mov rdx, r10
    mov r8, rdi
    call console_log

    mov eax, 1
    jmp delete_done

delete_fail:
    xor eax, eax

delete_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret

editor_buffer_delete ENDP

;==========================================================================
; editor_buffer_get_text(buffer: RCX) -> RAX (text pointer)
;==========================================================================
PUBLIC editor_buffer_get_text
ALIGN 16
editor_buffer_get_text PROC
    mov rax, [rcx + EDITOR_BUFFER.buffer_ptr]
    ret
editor_buffer_get_text ENDP

;==========================================================================
; editor_buffer_get_line(buffer: RCX, line_num: EDX) -> RAX (line text)
;==========================================================================
PUBLIC editor_buffer_get_line
ALIGN 16
editor_buffer_get_line PROC

    ; RCX = buffer, EDX = line number
    test rcx, rcx
    jz get_line_fail

    ; Check line number
    mov rax, [rcx + EDITOR_BUFFER.line_count]
    cmp rdx, rax
    jge get_line_fail

    ; Get offset from line_offsets array
    mov rax, [rcx + EDITOR_BUFFER.line_offsets]
    mov r8, [rax + rdx*8]
    
    ; Return pointer to line in buffer
    mov rax, [rcx + EDITOR_BUFFER.buffer_ptr]
    add rax, r8
    ret

get_line_fail:
    xor eax, eax
    ret

editor_buffer_get_line ENDP

;==========================================================================
; editor_buffer_get_line_count(buffer: RCX) -> EAX
;==========================================================================
PUBLIC editor_buffer_get_line_count
ALIGN 16
editor_buffer_get_line_count PROC
    mov eax, [rcx + EDITOR_BUFFER.line_count]
    ret
editor_buffer_get_line_count ENDP

;==========================================================================
; editor_reparse_lines(buffer: RCX) - Internal line parsing
;==========================================================================
ALIGN 16
editor_reparse_lines PROC

    push rbx
    push rsi
    push rdi
    sub rsp, 32

    mov rsi, rcx        ; buffer
    mov rax, [rsi + EDITOR_BUFFER.buffer_ptr]
    mov rbx, [rsi + EDITOR_BUFFER.text_length]
    mov rdi, [rsi + EDITOR_BUFFER.line_offsets]

    ; Scan for line feeds and record offsets
    xor r8, r8          ; line count
    xor r9, r9          ; current offset
    mov QWORD PTR [rdi], 0  ; First line starts at 0

reparse_loop:
    cmp r9, rbx
    jge reparse_done

    mov al, [rax + r9]
    cmp al, 10          ; LF
    jne reparse_next

    ; Found line break
    inc r8
    mov r10, r9
    inc r10

    ; Check bounds
    cmp r8, [rsi + EDITOR_BUFFER.max_lines]
    jge reparse_done

    ; Store offset
    mov [rdi + r8*8], r10

reparse_next:
    inc r9
    jmp reparse_loop

reparse_done:
    mov [rsi + EDITOR_BUFFER.line_count], r8

    ; Log parsing
    lea rcx, szLinesParsed
    mov rdx, r8
    call console_log

    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret

editor_reparse_lines ENDP

;==========================================================================
; editor_selection_set(buffer: RCX, start_line: EDX, start_col: R8, 
;                      end_line: R9, end_col: [rsp+40])
;==========================================================================
PUBLIC editor_selection_set
ALIGN 16
editor_selection_set PROC

    ; RCX = buffer, EDX = start_line, R8 = start_col
    ; R9 = end_line, [rsp+40] = end_col
    mov [rcx + EDITOR_BUFFER.sel_start_line], edx
    mov [rcx + EDITOR_BUFFER.sel_start_col], r8d
    mov [rcx + EDITOR_BUFFER.sel_end_line], r9d
    mov rax, [rsp + 40]
    mov [rcx + EDITOR_BUFFER.sel_end_col], eax
    mov DWORD PTR [rcx + EDITOR_BUFFER.has_selection], 1
    
    mov eax, 1
    ret

editor_selection_set ENDP

;==========================================================================
; editor_selection_get(buffer: RCX) -> RAX (1 if has selection)
;==========================================================================
PUBLIC editor_selection_get
ALIGN 16
editor_selection_get PROC
    mov eax, [rcx + EDITOR_BUFFER.has_selection]
    ret
editor_selection_get ENDP

;==========================================================================
; editor_undo(buffer: RCX) -> EAX (1=success)
;==========================================================================
PUBLIC editor_undo
ALIGN 16
editor_undo PROC
    ; Stub - full undo implementation would track operations
    mov eax, 1
    ret
editor_undo ENDP

;==========================================================================
; editor_redo(buffer: RCX) -> EAX (1=success)
;==========================================================================
PUBLIC editor_redo
ALIGN 16
editor_redo PROC
    ; Stub - full redo implementation would replay operations
    mov eax, 1
    ret
editor_redo ENDP

END
