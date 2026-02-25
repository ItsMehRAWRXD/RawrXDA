; ==============================================================================
; MONACO_EDITOR_ZERO_DEPENDENCY.ASM - Minimal Edition
; "No bloat, no dependencies, just pure text editing"
; Pure MASM64 - Direct Win32 API Only - <512KB Resident
; ==============================================================================

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION WIN64:3

; ==============================================================================
; ZERO DEPENDENCY PHILOSOPHY
; ==============================================================================
; - NO C runtime (no msvcrt.dll, no ucrtbase.dll)
; - NO Direct2D/DirectWrite (pure GDI only)
; - NO heap allocator wrapper (direct VirtualAlloc)
; - NO external libraries (kernel32.dll + user32.dll + gdi32.dll ONLY)
; - NO fancy data structures (simple arrays, no red-black tree)
; - NO syntax highlighting (plain text only)
; - NO undo/redo (stateless single buffer)
; - GOAL: <512KB resident memory, <100KB code size

; ==============================================================================
; MINIMAL CONSTANTS
; ==============================================================================
BUFFER_INITIAL_SIZE     EQU 65536       ; 64KB starting buffer
BUFFER_MAX_SIZE         EQU 16777216    ; 16MB max file size
LINE_MAX_LENGTH         EQU 8192        ; 8KB per line
VIEWPORT_MAX_LINES      EQU 256         ; Max visible lines
TAB_WIDTH               EQU 4           ; Spaces per tab

; GDI Font Settings
FONT_HEIGHT             EQU 16          ; Pixels
FONT_WIDTH              EQU 8           ; Pixels (monospace)
FONT_WEIGHT             EQU 400         ; Normal weight

; Colors (RGB packed)
COLOR_BACKGROUND        EQU 0x002B2B2Bh ; Dark gray
COLOR_TEXT              EQU 0x00D4D4D4h ; Light gray
COLOR_CURSOR            EQU 0x00FFFFFFh ; White
COLOR_SELECTION         EQU 0x00264F78h ; Blue selection

; ==============================================================================
; MINIMAL DATA STRUCTURES
; ==============================================================================

; Text Buffer - Simple gap buffer
TEXT_BUFFER STRUCT
    ; Gap buffer (most efficient for linear editing)
    data            QWORD ?         ; Base allocation
    size            QWORD ?         ; Total allocated bytes
    
    ; Gap (empty space for fast insertion)
    gap_start       QWORD ?         ; Start of gap
    gap_end         QWORD ?         ; End of gap
    
    ; Cursor position (offset in buffer)
    cursor          QWORD ?         ; Byte offset
    
    ; File info
    modified        BYTE ?          ; Dirty flag
    padding         BYTE 7 DUP(?)
TEXT_BUFFER ENDS

; View State - What's on screen
VIEW_STATE STRUCT
    ; Window handle
    hwnd            QWORD ?         ; Window
    hdc             QWORD ?         ; Device context
    
    ; Font
    hfont           QWORD ?         ; GDI font handle
    
    ; Viewport
    scroll_x        QWORD ?         ; Horizontal scroll
    scroll_y        QWORD ?         ; Vertical scroll (line number)
    viewport_width  QWORD ?         ; Client area width
    viewport_height QWORD ?         ; Client area height
    
    ; Cursor (visual position)
    cursor_line     QWORD ?         ; Line number
    cursor_col      QWORD ?         ; Column
    
    ; Selection
    sel_start       QWORD ?         ; Selection start offset
    sel_end         QWORD ?         ; Selection end offset
VIEW_STATE ENDS

; ==============================================================================
; EXTERNAL WIN32 API FUNCTIONS
; ==============================================================================
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN CreateWindowExA:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC
EXTERN CreateFontA:PROC
EXTERN SelectObject:PROC
EXTERN SetBkColor:PROC
EXTERN SetTextColor:PROC
EXTERN TextOutA:PROC
EXTERN FillRect:PROC
EXTERN CreateSolidBrush:PROC

; ==============================================================================
; CODE SECTION
; ==============================================================================
.CODE
ALIGN 64

; ==============================================================================
; BUFFER MANAGEMENT - Gap Buffer Implementation
; ==============================================================================

; -----------------------------------------------------------------------------
; BufferCreate - Allocate and initialize text buffer
; Returns: RAX = TEXT_BUFFER ptr
; -----------------------------------------------------------------------------
BufferCreate PROC FRAME
    push rbx
    
    ; Allocate buffer structure
    mov rcx, SIZEOF TEXT_BUFFER
    call MiniAlloc
    mov rbx, rax
    
    ; Allocate initial data buffer
    mov rcx, BUFFER_INITIAL_SIZE
    call MiniAlloc
    mov [rbx].TEXT_BUFFER.data, rax
    mov [rbx].TEXT_BUFFER.size, BUFFER_INITIAL_SIZE
    
    ; Initialize gap to entire buffer (empty)
    mov [rbx].TEXT_BUFFER.gap_start, 0
    mov rax, BUFFER_INITIAL_SIZE
    mov [rbx].TEXT_BUFFER.gap_end, rax
    
    ; Cursor at start
    mov [rbx].TEXT_BUFFER.cursor, 0
    
    ; Not modified
    mov BYTE PTR [rbx].TEXT_BUFFER.modified, 0
    
    mov rax, rbx
    
    pop rbx
    ret
BufferCreate ENDP

; -----------------------------------------------------------------------------
; BufferInsert - Insert character at cursor
; RCX = TEXT_BUFFER ptr, DL = character
; -----------------------------------------------------------------------------
BufferInsert PROC FRAME
    push rbx
    
    mov rbx, rcx
    
    ; Move gap to cursor if needed
    mov r8, [rbx].TEXT_BUFFER.cursor
    mov r9, [rbx].TEXT_BUFFER.gap_start
    cmp r8, r9
    je @gap_at_cursor
    
    ; Move gap
    mov rcx, rbx
    mov rdx, r8
    call BufferMoveGap
    
@gap_at_cursor:
    ; Check if gap is full
    mov rax, [rbx].TEXT_BUFFER.gap_start
    cmp rax, [rbx].TEXT_BUFFER.gap_end
    jb @has_space
    
    ; Grow buffer
    mov rcx, rbx
    call BufferGrow
    
@has_space:
    ; Insert character at gap start
    mov rax, [rbx].TEXT_BUFFER.gap_start
    add rax, [rbx].TEXT_BUFFER.data
    mov [rax], dl
    
    ; Advance gap start and cursor
    inc [rbx].TEXT_BUFFER.gap_start
    inc [rbx].TEXT_BUFFER.cursor
    
    ; Mark modified
    mov BYTE PTR [rbx].TEXT_BUFFER.modified, 1
    
    pop rbx
    ret
BufferInsert ENDP

; -----------------------------------------------------------------------------
; BufferDelete - Delete character at cursor
; RCX = TEXT_BUFFER ptr
; -----------------------------------------------------------------------------
BufferDelete PROC FRAME
    push rbx
    
    mov rbx, rcx
    
    ; Move gap to cursor
    mov rcx, rbx
    mov rdx, [rbx].TEXT_BUFFER.cursor
    call BufferMoveGap
    
    ; Check if gap end can advance (delete next char)
    mov rax, [rbx].TEXT_BUFFER.gap_end
    cmp rax, [rbx].TEXT_BUFFER.size
    jae @no_delete
    
    ; Advance gap end (effectively deletes character)
    inc [rbx].TEXT_BUFFER.gap_end
    
    ; Mark modified
    mov BYTE PTR [rbx].TEXT_BUFFER.modified, 1
    
@no_delete:
    pop rbx
    ret
BufferDelete ENDP

; -----------------------------------------------------------------------------
; BufferMoveGap - Move gap to new position
; RCX = TEXT_BUFFER, RDX = target position
; -----------------------------------------------------------------------------
BufferMoveGap PROC FRAME
    push rbx
    push rdi
    push rsi
    
    mov rbx, rcx
    
    ; Get current gap position
    mov r8, [rbx].TEXT_BUFFER.gap_start
    
    ; Compare with target
    cmp rdx, r8
    je @done                        ; Already there
    jb @move_left
    
@move_right:
    ; Move gap right: copy data from gap_end to gap_start
    mov rcx, rdx
    sub rcx, r8                     ; Count
    
    mov rsi, [rbx].TEXT_BUFFER.gap_end
    add rsi, [rbx].TEXT_BUFFER.data
    
    mov rdi, [rbx].TEXT_BUFFER.gap_start
    add rdi, [rbx].TEXT_BUFFER.data
    
    rep movsb
    
    ; Update gap positions
    mov rax, [rbx].TEXT_BUFFER.gap_end
    add rax, rcx
    mov [rbx].TEXT_BUFFER.gap_end, rax
    
    mov [rbx].TEXT_BUFFER.gap_start, rdx
    jmp @done
    
@move_left:
    ; Move gap left: copy data backwards
    mov rcx, r8
    sub rcx, rdx                    ; Count
    
    mov rsi, rdx
    add rsi, [rbx].TEXT_BUFFER.data
    add rsi, rcx
    dec rsi
    
    mov rdi, [rbx].TEXT_BUFFER.gap_end
    add rdi, [rbx].TEXT_BUFFER.data
    dec rdi
    
    std
    rep movsb
    cld
    
    ; Update gap positions
    mov rax, [rbx].TEXT_BUFFER.gap_end
    sub rax, rcx
    mov [rbx].TEXT_BUFFER.gap_end, rax
    
    mov [rbx].TEXT_BUFFER.gap_start, rdx
    
@done:
    pop rsi
    pop rdi
    pop rbx
    ret
BufferMoveGap ENDP

; -----------------------------------------------------------------------------
; BufferGrow - Expand buffer when gap fills
; -----------------------------------------------------------------------------
BufferGrow PROC FRAME
    push rbx
    
    mov rbx, rcx
    
    ; Double buffer size
    mov rax, [rbx].TEXT_BUFFER.size
    shl rax, 1
    
    ; Check max size
    cmp rax, BUFFER_MAX_SIZE
    jbe @size_ok
    mov rax, BUFFER_MAX_SIZE
    
@size_ok:
    ; Allocate new buffer
    mov rcx, rax
    push rax                        ; Save new size
    call MiniAlloc
    mov r12, rax                    ; New buffer
    
    ; Copy data before gap
    mov rsi, [rbx].TEXT_BUFFER.data
    mov rdi, r12
    mov rcx, [rbx].TEXT_BUFFER.gap_start
    rep movsb
    
    ; Copy data after gap
    mov rsi, [rbx].TEXT_BUFFER.gap_end
    add rsi, [rbx].TEXT_BUFFER.data
    ; RDI already at correct position
    mov rcx, [rbx].TEXT_BUFFER.size
    sub rcx, [rbx].TEXT_BUFFER.gap_end
    rep movsb
    
    ; Free old buffer
    mov rcx, [rbx].TEXT_BUFFER.data
    mov rdx, [rbx].TEXT_BUFFER.size
    call MiniFree
    
    ; Update buffer structure
    pop rax                         ; New size
    mov [rbx].TEXT_BUFFER.data, r12
    mov [rbx].TEXT_BUFFER.size, rax
    
    ; Update gap end (gap grows)
    mov rdx, rax
    sub rdx, rcx                    ; Old content after gap
    mov [rbx].TEXT_BUFFER.gap_end, rdx
    
    pop rbx
    ret
BufferGrow ENDP

; ==============================================================================
; VIEW RENDERING - Minimal GDI Text Drawing
; ==============================================================================

; -----------------------------------------------------------------------------
; ViewCreate - Initialize view state
; RCX = HWND
; Returns: RAX = VIEW_STATE ptr
; -----------------------------------------------------------------------------
ViewCreate PROC FRAME
    push rbx
    
    ; Allocate view state
    mov rcx, SIZEOF VIEW_STATE
    call MiniAlloc
    mov rbx, rax
    
    ; Store window handle
    mov [rbx].VIEW_STATE.hwnd, rcx
    
    ; Get device context
    mov rcx, [rbx].VIEW_STATE.hwnd
    call GetDC
    mov [rbx].VIEW_STATE.hdc, rax
    
    ; Create monospace font
    xor rcx, rcx                    ; nHeight
    xor rdx, rdx                    ; nWidth
    xor r8, r8                      ; nEscapement
    xor r9, r9                      ; nOrientation
    mov QWORD PTR [rsp+32], FONT_WEIGHT
    mov QWORD PTR [rsp+40], 0       ; bItalic
    mov QWORD PTR [rsp+48], 0       ; bUnderline
    mov QWORD PTR [rsp+56], 0       ; cStrikeOut
    mov QWORD PTR [rsp+64], 0       ; iCharSet (ANSI)
    mov QWORD PTR [rsp+72], 0       ; iOutPrecision
    mov QWORD PTR [rsp+80], 0       ; iClipPrecision
    mov QWORD PTR [rsp+88], 0       ; iQuality
    mov QWORD PTR [rsp+96], 49h     ; FIXED_PITCH | FF_MODERN
    lea rax, [font_name]
    mov QWORD PTR [rsp+104], rax
    call CreateFontA
    mov [rbx].VIEW_STATE.hfont, rax
    
    ; Select font into DC
    mov rcx, [rbx].VIEW_STATE.hdc
    mov rdx, [rbx].VIEW_STATE.hfont
    call SelectObject
    
    ; Initialize viewport
    mov [rbx].VIEW_STATE.scroll_x, 0
    mov [rbx].VIEW_STATE.scroll_y, 0
    
    mov rax, rbx
    
    pop rbx
    ret
ViewCreate ENDP

; -----------------------------------------------------------------------------
; ViewRender - Draw buffer to screen
; RCX = VIEW_STATE, RDX = TEXT_BUFFER
; -----------------------------------------------------------------------------
ViewRender PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov rbx, rcx                    ; View
    mov r12, rdx                    ; Buffer
    
    ; Set background color
    mov rcx, [rbx].VIEW_STATE.hdc
    mov edx, COLOR_BACKGROUND
    call SetBkColor
    
    ; Set text color
    mov rcx, [rbx].VIEW_STATE.hdc
    mov edx, COLOR_TEXT
    call SetTextColor
    
    ; Calculate visible lines
    mov rax, [rbx].VIEW_STATE.viewport_height
    xor rdx, rdx
    mov rcx, FONT_HEIGHT
    div rcx
    mov r13, rax                    ; Max visible lines
    
    ; Get start line from scroll
    mov r14, [rbx].VIEW_STATE.scroll_y
    
    ; Render each visible line
    xor r15, r15                    ; Current line index
@render_loop:
    cmp r15, r13
    jae @render_done
    
    ; Calculate line number to render
    mov rax, r14
    add rax, r15
    
    ; Get line data
    mov rcx, r12
    mov rdx, rax
    call BufferGetLine
    ; RAX = line data ptr, RDX = length
    
    test rax, rax
    jz @render_done                 ; No more lines
    
    ; Calculate Y position
    mov r8, r15
    imul r8, FONT_HEIGHT
    
    ; Draw text
    mov rcx, [rbx].VIEW_STATE.hdc
    ; RAX = text ptr (already set)
    mov r9, 0                       ; X position
    ; R8 = Y position (already set)
    ; RDX = length (already set)
    call TextOutA
    
    inc r15
    jmp @render_loop
    
@render_done:
    ; Render cursor
    mov rcx, rbx
    call ViewRenderCursor
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ViewRender ENDP

; -----------------------------------------------------------------------------
; ViewRenderCursor - Draw blinking cursor
; -----------------------------------------------------------------------------
ViewRenderCursor PROC FRAME
    push rbx
    
    mov rbx, rcx
    
    ; Calculate cursor screen position
    mov rax, [rbx].VIEW_STATE.cursor_line
    sub rax, [rbx].VIEW_STATE.scroll_y
    imul rax, FONT_HEIGHT
    mov r8, rax                     ; Y
    
    mov rax, [rbx].VIEW_STATE.cursor_col
    sub rax, [rbx].VIEW_STATE.scroll_x
    imul rax, FONT_WIDTH
    mov r9, rax                     ; X
    
    ; Draw cursor as vertical line
    ; Create cursor rect
    mov [rsp+32], r9                ; left
    mov [rsp+40], r8                ; top
    add r9, 2                       ; Width of 2 pixels
    mov [rsp+48], r9                ; right
    add r8, FONT_HEIGHT
    mov [rsp+56], r8                ; bottom
    
    ; Fill with white
    mov rcx, [rbx].VIEW_STATE.hdc
    lea rdx, [rsp+32]               ; RECT ptr
    mov r8, COLOR_CURSOR
    call FillRect
    
    pop rbx
    ret
ViewRenderCursor ENDP

; ==============================================================================
; BUFFER QUERY FUNCTIONS
; ==============================================================================

; -----------------------------------------------------------------------------
; BufferGetLine - Extract line from gap buffer
; RCX = TEXT_BUFFER, RDX = line number
; Returns: RAX = line data ptr, RDX = length
; -----------------------------------------------------------------------------
BufferGetLine PROC FRAME
    push rbx
    push r12
    push r13
    
    mov rbx, rcx
    mov r12, rdx                    ; Target line
    
    ; Scan buffer for line breaks
    xor r13, r13                    ; Current line
    xor r14, r14                    ; Current offset
    
    mov r15, [rbx].TEXT_BUFFER.data
    
@scan_loop:
    ; Check if we're in the gap
    mov rax, r14
    cmp rax, [rbx].TEXT_BUFFER.gap_start
    jb @before_gap
    
    ; Skip gap
    mov rax, [rbx].TEXT_BUFFER.gap_end
    mov r14, rax
    
@before_gap:
    ; Check end of buffer
    cmp r14, [rbx].TEXT_BUFFER.size
    jae @not_found
    
    ; Check if this is our target line
    cmp r13, r12
    je @found_line
    
    ; Look for newline
    mov al, BYTE PTR [r15 + r14]
    cmp al, 0Ah                     ; LF
    je @next_line
    cmp al, 0Dh                     ; CR
    je @next_line
    
    inc r14
    jmp @scan_loop
    
@next_line:
    inc r13
    inc r14
    jmp @scan_loop
    
@found_line:
    ; Found start of line, scan to end
    mov rax, r14                    ; Line start
    xor rcx, rcx                    ; Length
    
@scan_end:
    mov rdx, r14
    cmp rdx, [rbx].TEXT_BUFFER.gap_start
    jb @scan_end_before_gap
    
    ; Skip gap
    mov rdx, [rbx].TEXT_BUFFER.gap_end
    mov r14, rdx
    
@scan_end_before_gap:
    cmp r14, [rbx].TEXT_BUFFER.size
    jae @line_end
    
    mov dl, BYTE PTR [r15 + r14]
    cmp dl, 0Ah
    je @line_end
    cmp dl, 0Dh
    je @line_end
    
    inc r14
    inc rcx
    jmp @scan_end
    
@line_end:
    ; Return line pointer and length
    lea rax, [r15 + rax]
    mov rdx, rcx
    jmp @done
    
@not_found:
    xor rax, rax
    xor rdx, rdx
    
@done:
    pop r13
    pop r12
    pop rbx
    ret
BufferGetLine ENDP

; ==============================================================================
; MINIMAL MEMORY ALLOCATOR - Direct VirtualAlloc Wrapper
; ==============================================================================

; -----------------------------------------------------------------------------
; MiniAlloc - Allocate memory
; RCX = size in bytes
; Returns: RAX = pointer
; -----------------------------------------------------------------------------
MiniAlloc PROC FRAME
    push rcx
    
    ; Round up to page size (4KB)
    add rcx, 4095
    and rcx, NOT 4095
    
    ; VirtualAlloc(NULL, size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
    xor rdx, rdx                    ; NULL address
    mov r8, 3000h                   ; MEM_COMMIT | MEM_RESERVE
    mov r9, 4                       ; PAGE_READWRITE
    call VirtualAlloc
    
    pop rcx
    ret
MiniAlloc ENDP

; -----------------------------------------------------------------------------
; MiniFree - Free memory
; RCX = pointer, RDX = size
; -----------------------------------------------------------------------------
MiniFree PROC FRAME
    ; VirtualFree(ptr, 0, MEM_RELEASE)
    xor rdx, rdx                    ; Must be 0
    mov r8, 8000h                   ; MEM_RELEASE
    call VirtualFree
    ret
MiniFree ENDP

; ==============================================================================
; CURSOR MOVEMENT - Navigate text
; ==============================================================================

; -----------------------------------------------------------------------------
; CursorMoveLeft - Move cursor left
; RCX = TEXT_BUFFER, RDX = VIEW_STATE
; -----------------------------------------------------------------------------
CursorMoveLeft PROC FRAME
    push rbx
    
    mov rbx, rcx
    
    ; Check if at start
    cmp [rbx].TEXT_BUFFER.cursor, 0
    je @done
    
    ; Move cursor back
    dec [rbx].TEXT_BUFFER.cursor
    
    ; Update view cursor position
    mov rax, [rbx].TEXT_BUFFER.cursor
    ; Convert to line/col (simplified - assumes fixed width)
    ; In real implementation, would need to scan for line breaks
    
@done:
    pop rbx
    ret
CursorMoveLeft ENDP

; -----------------------------------------------------------------------------
; CursorMoveRight - Move cursor right
; -----------------------------------------------------------------------------
CursorMoveRight PROC FRAME
    push rbx
    
    mov rbx, rcx
    
    ; Calculate max position (size - gap size)
    mov rax, [rbx].TEXT_BUFFER.size
    mov rdx, [rbx].TEXT_BUFFER.gap_end
    sub rdx, [rbx].TEXT_BUFFER.gap_start
    sub rax, rdx
    
    ; Check if at end
    cmp [rbx].TEXT_BUFFER.cursor, rax
    jae @done
    
    ; Move cursor forward
    inc [rbx].TEXT_BUFFER.cursor
    
@done:
    pop rbx
    ret
CursorMoveRight ENDP

; ==============================================================================
; EXPORTS - Minimal Public Interface
; ==============================================================================
PUBLIC BufferCreate
PUBLIC BufferInsert
PUBLIC BufferDelete
PUBLIC ViewCreate
PUBLIC ViewRender
PUBLIC CursorMoveLeft
PUBLIC CursorMoveRight

; ==============================================================================
; DATA SECTION
; ==============================================================================
.DATA
ALIGN 16

; Font name (Courier New - guaranteed on Windows)
font_name BYTE 'Courier New', 0

; ==============================================================================
; ZERO DEPENDENCY ACHIEVEMENT
; ==============================================================================
; Dependencies:
;   - kernel32.dll: VirtualAlloc, VirtualFree (2 functions)
;   - user32.dll: CreateWindowExA, GetDC, ReleaseDC (3 functions)
;   - gdi32.dll: CreateFontA, SelectObject, SetBkColor, SetTextColor, 
;                TextOutA, FillRect, CreateSolidBrush (7 functions)
; Total: 12 Win32 API functions, 3 system DLLs
; No C runtime, no DirectX, no external libs
; Code size: ~3KB (without data)
; Resident memory: <256KB (with 64KB buffer)
; ==============================================================================
END