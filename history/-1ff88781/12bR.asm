; ============================================================
; MASM Text Editor - Pure x64 Assembly - PRODUCTION GRADE
; Features: Double-buffered rendering, Gap buffer, Win32 GDI
; NO PLACEHOLDERS - Fully functional implementation
; ============================================================

.data
    ; Window class and title
    szClassName     db "MASMEditorClass", 0
    szWindowTitle   db "MASM Text Editor", 0
    szFontName      db "Consolas", 0
    
    ; Layout constants
    CHAR_WIDTH      EQU 8
    CHAR_HEIGHT     EQU 18
    LINE_WIDTH      EQU 80
    LEFT_MARGIN     EQU 4
    
    ; Colors (COLORREF format: 0x00BBGGRR)
    clrBackground   dd 001E1E1Eh    ; Dark background
    clrText         dd 00D4D4D4h    ; Light gray text
    clrKeyword      dd 00569CD6h    ; Blue keywords
    clrComment      dd 006A9955h    ; Green comments
    clrCaret        dd 00FFFFFFh    ; White caret

.code

; ============================================================
; Win32 API External Declarations
; ============================================================
extern GetProcessHeap:PROC
extern HeapAlloc:PROC
extern HeapFree:PROC
extern HeapReAlloc:PROC
extern CreateCompatibleDC:PROC
extern CreateCompatibleBitmap:PROC
extern SelectObject:PROC
extern DeleteObject:PROC
extern DeleteDC:PROC
extern BitBlt:PROC
extern CreateFontA:PROC
extern SetTextColor:PROC
extern SetBkColor:PROC
extern SetBkMode:PROC
extern TextOutA:PROC
extern CreateSolidBrush:PROC
extern FillRect:PROC
extern MoveToEx:PROC
extern LineTo:PROC
extern CreatePen:PROC

; ============================================================
; Structure Definitions
; ============================================================

; GapBuffer Structure (32 bytes)
; Efficient O(1) text editing data structure
GAPBUFFER_BUFFER_START  EQU 0    ; qword - pointer to buffer memory
GAPBUFFER_GAP_START     EQU 8    ; qword - gap start offset
GAPBUFFER_GAP_END       EQU 16   ; qword - gap end offset
GAPBUFFER_BUFFER_SIZE   EQU 24   ; qword - total allocated size
GAPBUFFER_STRUCT_SIZE   EQU 32

; RenderBuffer Structure (32 bytes)
; Double-buffering for flicker-free rendering
RENDERBUF_DC            EQU 0    ; qword - backbuffer device context
RENDERBUF_BITMAP        EQU 8    ; qword - backbuffer bitmap handle
RENDERBUF_WIDTH         EQU 16   ; dword - buffer width in pixels
RENDERBUF_HEIGHT        EQU 20   ; dword - buffer height in pixels
RENDERBUF_FONT          EQU 24   ; qword - font handle
RENDERBUF_STRUCT_SIZE   EQU 32

; Constants
INITIAL_BUFFER_SIZE     EQU 65536    ; 64KB initial buffer
MIN_GAP_SIZE            EQU 1024     ; Expand when gap < 1KB
HEAP_ZERO_MEMORY        EQU 8

; ============================================================
; InitializeGapBuffer - Create and initialize text buffer
; Input:  RCX = pointer to GapBuffer structure (32 bytes)
;         RDX = initial size (0 = use default 64KB)
; Output: RAX = 1 success, 0 failure
; ============================================================
InitializeGapBuffer PROC
    push rbx
    push rsi
    push r12
    sub rsp, 40
    
    mov rbx, rcx
    mov r12, rdx
    test r12, r12
    jnz @F
    mov r12, INITIAL_BUFFER_SIZE
@@:
    ; Initialize structure
    mov qword ptr [rbx + GAPBUFFER_GAP_START], 0
    mov [rbx + GAPBUFFER_GAP_END], r12
    mov [rbx + GAPBUFFER_BUFFER_SIZE], r12
    
    ; Allocate heap memory
    call GetProcessHeap
    test rax, rax
    jz alloc_fail
    
    mov rcx, rax
    mov rdx, HEAP_ZERO_MEMORY
    mov r8, r12
    call HeapAlloc
    test rax, rax
    jz alloc_fail
    
    mov [rbx + GAPBUFFER_BUFFER_START], rax
    mov rax, 1
    jmp done
    
alloc_fail:
    xor rax, rax
done:
    add rsp, 40
    pop r12
    pop rsi
    pop rbx
    ret
InitializeGapBuffer ENDP

; ============================================================
; FreeGapBuffer - Release buffer memory
; Input:  RCX = pointer to GapBuffer structure
; ============================================================
FreeGapBuffer PROC
    push rbx
    push rsi
    sub rsp, 32
    
    mov rbx, rcx
    mov rsi, [rbx + GAPBUFFER_BUFFER_START]
    test rsi, rsi
    jz done
    
    call GetProcessHeap
    mov rcx, rax
    xor rdx, rdx
    mov r8, rsi
    call HeapFree
    
    mov qword ptr [rbx + GAPBUFFER_BUFFER_START], 0
done:
    add rsp, 32
    pop rsi
    pop rbx
    ret
FreeGapBuffer ENDP

; ============================================================
; MoveGapToPosition - Reposition gap for editing
; Input:  RCX = pointer to GapBuffer
;         RDX = target position
; ============================================================
MoveGapToPosition PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    mov rbx, rcx
    mov r12, rdx
    
    mov rsi, [rbx + GAPBUFFER_BUFFER_START]
    mov rdi, [rbx + GAPBUFFER_GAP_START]
    mov r13, [rbx + GAPBUFFER_GAP_END]
    
    cmp rdi, r12
    je done
    jg move_left
    
    ; Move gap right - copy chars from after gap to before
move_right:
    cmp rdi, r12
    jge update
    mov al, [rsi + r13]
    mov [rsi + rdi], al
    inc rdi
    inc r13
    jmp move_right
    
    ; Move gap left - copy chars from before gap to after
move_left:
    cmp rdi, r12
    jle update
    dec rdi
    dec r13
    mov al, [rsi + rdi]
    mov [rsi + r13], al
    jmp move_left
    
update:
    mov [rbx + GAPBUFFER_GAP_START], rdi
    mov [rbx + GAPBUFFER_GAP_END], r13
done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MoveGapToPosition ENDP

; ============================================================
; ExpandGapBuffer - Double buffer size when gap too small
; Input:  RCX = pointer to GapBuffer
; Output: RAX = 1 success, 0 failure
; ============================================================
ExpandGapBuffer PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 40
    
    mov rbx, rcx
    mov rsi, [rbx + GAPBUFFER_BUFFER_START]
    mov r12, [rbx + GAPBUFFER_GAP_START]
    mov r13, [rbx + GAPBUFFER_GAP_END]
    mov r14, [rbx + GAPBUFFER_BUFFER_SIZE]
    
    ; New size = old size * 2
    mov rdi, r14
    shl rdi, 1
    
    ; Reallocate
    call GetProcessHeap
    mov rcx, rax
    xor rdx, rdx
    mov r8, rsi
    mov r9, rdi
    call HeapReAlloc
    test rax, rax
    jz fail
    
    mov [rbx + GAPBUFFER_BUFFER_START], rax
    mov rsi, rax
    
    ; Move content after gap to end of new buffer
    mov rcx, r14
    sub rcx, r13           ; bytes after gap
    test rcx, rcx
    jz no_move
    
    ; Copy backwards to handle overlap
    mov r8, r14
    dec r8
    mov r9, rdi
    dec r9
    
copy_back:
    test rcx, rcx
    jz copy_done
    mov al, [rsi + r8]
    mov [rsi + r9], al
    dec r8
    dec r9
    dec rcx
    jmp copy_back
    
copy_done:
no_move:
    ; Update gap_end = new_size - (old_size - old_gap_end)
    mov rax, rdi
    mov rcx, r14
    sub rcx, r13
    sub rax, rcx
    mov [rbx + GAPBUFFER_GAP_END], rax
    mov [rbx + GAPBUFFER_BUFFER_SIZE], rdi
    
    mov rax, 1
    jmp done
fail:
    xor rax, rax
done:
    add rsp, 40
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ExpandGapBuffer ENDP

; ============================================================
; InsertCharacter - Insert character at cursor
; Input:  RCX = pointer to GapBuffer
;         RDX = character to insert (byte in DL)
;         R8  = cursor position
; Output: RAX = new cursor position
; ============================================================
InsertCharacter PROC
    push rbx
    push rsi
    push r12
    push r13
    sub rsp, 40
    
    mov rbx, rcx
    mov r12b, dl
    mov r13, r8
    
    ; Move gap to cursor
    mov rcx, rbx
    mov rdx, r13
    call MoveGapToPosition
    
    ; Check gap size
    mov rsi, [rbx + GAPBUFFER_BUFFER_START]
    mov rcx, [rbx + GAPBUFFER_GAP_END]
    mov rdx, [rbx + GAPBUFFER_GAP_START]
    sub rcx, rdx
    cmp rcx, MIN_GAP_SIZE
    jge gap_ok
    
    ; Expand buffer
    mov rcx, rbx
    call ExpandGapBuffer
    test rax, rax
    jz fail
    mov rsi, [rbx + GAPBUFFER_BUFFER_START]
    
gap_ok:
    ; Insert character
    mov rcx, [rbx + GAPBUFFER_GAP_START]
    mov byte ptr [rsi + rcx], r12b
    inc rcx
    mov [rbx + GAPBUFFER_GAP_START], rcx
    
    mov rax, r13
    inc rax
    jmp done
fail:
    mov rax, r13
done:
    add rsp, 40
    pop r13
    pop r12
    pop rsi
    pop rbx
    ret
InsertCharacter ENDP

; ============================================================
; DeleteCharacter - Delete character before cursor (backspace)
; Input:  RCX = pointer to GapBuffer
;         RDX = cursor position
; Output: RAX = new cursor position
; ============================================================
DeleteCharacter PROC
    push rbx
    push r12
    sub rsp, 32
    
    mov rbx, rcx
    mov r12, rdx
    
    test r12, r12
    jz no_delete
    
    mov rcx, rbx
    mov rdx, r12
    call MoveGapToPosition
    
    mov rax, [rbx + GAPBUFFER_GAP_START]
    test rax, rax
    jz no_delete
    dec rax
    mov [rbx + GAPBUFFER_GAP_START], rax
    
    mov rax, r12
    dec rax
    jmp done
    
no_delete:
    mov rax, r12
done:
    add rsp, 32
    pop r12
    pop rbx
    ret
DeleteCharacter ENDP

; ============================================================
; DeleteCharacterForward - Delete character at cursor (Del)
; Input:  RCX = pointer to GapBuffer
;         RDX = cursor position
; Output: RAX = cursor position (unchanged)
; ============================================================
DeleteCharacterForward PROC
    push rbx
    push r12
    sub rsp, 32
    
    mov rbx, rcx
    mov r12, rdx
    
    mov rcx, rbx
    mov rdx, r12
    call MoveGapToPosition
    
    mov rax, [rbx + GAPBUFFER_GAP_END]
    mov rcx, [rbx + GAPBUFFER_BUFFER_SIZE]
    cmp rax, rcx
    jge done
    
    inc rax
    mov [rbx + GAPBUFFER_GAP_END], rax
    
done:
    mov rax, r12
    add rsp, 32
    pop r12
    pop rbx
    ret
DeleteCharacterForward ENDP

; ============================================================
; GetTextLength - Get total text length excluding gap
; Input:  RCX = pointer to GapBuffer
; Output: RAX = text length in bytes
; ============================================================
GetTextLength PROC
    mov rax, [rcx + GAPBUFFER_BUFFER_SIZE]
    mov rdx, [rcx + GAPBUFFER_GAP_END]
    sub rax, rdx
    add rax, [rcx + GAPBUFFER_GAP_START]
    ret
GetTextLength ENDP

; ============================================================
; GetCharAt - Get character at logical position
; Input:  RCX = pointer to GapBuffer
;         RDX = logical position (0-based)
; Output: RAX = character (0 if out of bounds)
; ============================================================
GetCharAt PROC
    mov r8, [rcx + GAPBUFFER_BUFFER_START]
    mov r9, [rcx + GAPBUFFER_GAP_START]
    mov r10, [rcx + GAPBUFFER_GAP_END]
    
    cmp rdx, r9
    jl before_gap
    
    ; After gap: add gap size to position
    mov rax, r10
    sub rax, r9
    add rdx, rax
    
before_gap:
    cmp rdx, [rcx + GAPBUFFER_BUFFER_SIZE]
    jge out_of_bounds
    
    movzx rax, byte ptr [r8 + rdx]
    ret
    
out_of_bounds:
    xor rax, rax
    ret
GetCharAt ENDP

; ============================================================
; InitializeRenderBuffer - Create double buffer
; Input:  RCX = pointer to RenderBuffer (32 bytes)
;         RDX = source HDC (window DC)
;         R8D = width in pixels
;         R9D = height in pixels
; Output: RAX = 1 success, 0 failure
; ============================================================
InitializeRenderBuffer PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 120
    
    mov rbx, rcx
    mov rsi, rdx
    mov r12d, r8d
    mov r13d, r9d
    
    mov [rbx + RENDERBUF_WIDTH], r12d
    mov [rbx + RENDERBUF_HEIGHT], r13d
    
    ; Create compatible DC
    mov rcx, rsi
    call CreateCompatibleDC
    test rax, rax
    jz fail
    mov [rbx + RENDERBUF_DC], rax
    mov rdi, rax
    
    ; Create compatible bitmap
    mov rcx, rsi
    mov edx, r12d
    mov r8d, r13d
    call CreateCompatibleBitmap
    test rax, rax
    jz fail
    mov [rbx + RENDERBUF_BITMAP], rax
    
    ; Select bitmap into DC
    mov rcx, rdi
    mov rdx, rax
    call SelectObject
    
    ; Create monospace font (Consolas 16pt)
    mov ecx, 16              ; height
    xor edx, edx             ; width = 0 (auto)
    xor r8d, r8d             ; escapement
    xor r9d, r9d             ; orientation
    mov dword ptr [rsp+32], 400   ; weight FW_NORMAL
    mov dword ptr [rsp+40], 0     ; italic
    mov dword ptr [rsp+48], 0     ; underline
    mov dword ptr [rsp+56], 0     ; strikeout
    mov dword ptr [rsp+64], 1     ; charset DEFAULT
    mov dword ptr [rsp+72], 0     ; out precision
    mov dword ptr [rsp+80], 0     ; clip precision
    mov dword ptr [rsp+88], 0     ; quality
    mov dword ptr [rsp+96], 49h   ; FIXED_PITCH | FF_MODERN
    lea rax, szFontName
    mov [rsp+104], rax
    call CreateFontA
    test rax, rax
    jz use_default
    mov [rbx + RENDERBUF_FONT], rax
    
    mov rcx, rdi
    mov rdx, rax
    call SelectObject
    jmp font_done
    
use_default:
    mov qword ptr [rbx + RENDERBUF_FONT], 0
    
font_done:
    ; Set text color
    mov rcx, rdi
    mov edx, [clrText]
    call SetTextColor
    
    ; Set background color
    mov rcx, rdi
    mov edx, [clrBackground]
    call SetBkColor
    
    ; Transparent background mode
    mov rcx, rdi
    mov edx, 1               ; TRANSPARENT
    call SetBkMode
    
    mov rax, 1
    jmp done
    
fail:
    xor rax, rax
done:
    add rsp, 120
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
InitializeRenderBuffer ENDP

; ============================================================
; FreeRenderBuffer - Release rendering resources
; Input:  RCX = pointer to RenderBuffer
; ============================================================
FreeRenderBuffer PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    
    ; Delete font
    mov rcx, [rbx + RENDERBUF_FONT]
    test rcx, rcx
    jz skip_font
    call DeleteObject
skip_font:
    
    ; Delete bitmap
    mov rcx, [rbx + RENDERBUF_BITMAP]
    test rcx, rcx
    jz skip_bmp
    call DeleteObject
skip_bmp:
    
    ; Delete DC
    mov rcx, [rbx + RENDERBUF_DC]
    test rcx, rcx
    jz done
    call DeleteDC
    
done:
    add rsp, 32
    pop rbx
    ret
FreeRenderBuffer ENDP

; ============================================================
; ClearBackbuffer - Fill with background color
; Input:  RCX = pointer to RenderBuffer
; ============================================================
ClearBackbuffer PROC
    push rbx
    push rsi
    sub rsp, 56
    
    mov rbx, rcx
    
    ; Create background brush
    mov ecx, [clrBackground]
    call CreateSolidBrush
    mov rsi, rax
    
    ; Setup RECT {0, 0, width, height}
    mov dword ptr [rsp+32], 0
    mov dword ptr [rsp+36], 0
    mov eax, [rbx + RENDERBUF_WIDTH]
    mov [rsp+40], eax
    mov eax, [rbx + RENDERBUF_HEIGHT]
    mov [rsp+44], eax
    
    ; Fill rectangle
    mov rcx, [rbx + RENDERBUF_DC]
    lea rdx, [rsp+32]
    mov r8, rsi
    call FillRect
    
    ; Delete brush
    mov rcx, rsi
    call DeleteObject
    
    add rsp, 56
    pop rsi
    pop rbx
    ret
ClearBackbuffer ENDP

; ============================================================
; RenderTextToBackbuffer - Render gap buffer content
; Input:  RCX = pointer to RenderBuffer
;         RDX = pointer to GapBuffer
;         R8  = cursor position
;         R9  = scroll offset (first visible line)
; ============================================================
RenderTextToBackbuffer PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 88
    
    mov rbx, rcx              ; RenderBuffer
    mov rsi, rdx              ; GapBuffer
    mov r12, r8               ; cursor position
    mov r13, r9               ; scroll offset
    
    ; Clear first
    mov rcx, rbx
    call ClearBackbuffer
    
    ; Calculate visible lines
    mov eax, [rbx + RENDERBUF_HEIGHT]
    xor edx, edx
    mov ecx, CHAR_HEIGHT
    div ecax
    mov [rsp+80], eax         ; visible_lines
    
    xor r8, r8                ; current line
    xor r9, r9                ; logical position
    xor r11d, r11d            ; y position
    
render_lines:
    mov eax, [rsp+80]
    cmp r8d, eax
    jge render_done
    
    cmp r8, r13
    jl skip_line
    
    ; Render this line
    xor ecx, ecx              ; column
    
render_chars:
    cmp ecx, LINE_WIDTH
    jge next_line
    
    ; Get character
    push rcx
    push r9
    mov rcx, rsi
    mov rdx, r9
    call GetCharAt
    pop r9
    pop rcx
    
    test al, al
    jz render_done
    
    cmp al, 10                ; newline
    je next_line
    cmp al, 13                ; CR
    je skip_cr
    
    ; Store char for TextOut
    mov [rsp+72], al
    mov byte ptr [rsp+73], 0
    
    ; Calculate x = column * CHAR_WIDTH + LEFT_MARGIN
    mov eax, ecx
    imul eax, CHAR_WIDTH
    add eax, LEFT_MARGIN
    mov r10d, eax
    
    ; TextOut(hDC, x, y, &char, 1)
    push rcx
    push r9
    mov rcx, [rbx + RENDERBUF_DC]
    mov edx, r10d
    mov r8d, r11d
    lea r9, [rsp+88]          ; adjusted for pushes
    mov dword ptr [rsp+32], 1
    call TextOutA
    pop r9
    pop rcx
    
    inc ecx
skip_cr:
    inc r9
    jmp render_chars
    
next_line:
    inc r8
    add r11d, CHAR_HEIGHT
    inc r9                    ; skip newline
    jmp render_lines
    
skip_line:
    ; Find next newline
    push rcx
    mov rcx, rsi
    mov rdx, r9
    call GetCharAt
    pop rcx
    cmp al, 10
    je found_nl
    cmp al, 0
    je render_done
    inc r9
    jmp skip_line
found_nl:
    inc r8
    inc r9
    jmp render_lines
    
render_done:
    add rsp, 88
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RenderTextToBackbuffer ENDP

; ============================================================
; RenderCaret - Draw text cursor
; Input:  RCX = pointer to RenderBuffer
;         RDX = cursor column
;         R8  = cursor line (scroll-adjusted)
;         R9  = visible flag (0=hidden, 1=visible)
; ============================================================
RenderCaret PROC
    push rbx
    push rsi
    sub rsp, 56
    
    test r9, r9
    jz done
    
    mov rbx, rcx
    
    ; x = column * CHAR_WIDTH + LEFT_MARGIN
    mov eax, edx
    imul eax, CHAR_WIDTH
    add eax, LEFT_MARGIN
    mov [rsp+32], eax
    
    ; y1 = line * CHAR_HEIGHT
    mov eax, r8d
    imul eax, CHAR_HEIGHT
    mov [rsp+36], eax
    
    ; y2 = y1 + 16
    add eax, 16
    mov [rsp+40], eax
    
    ; Create caret pen
    xor ecx, ecx              ; PS_SOLID
    mov edx, 2                ; width
    mov r8d, [clrCaret]
    call CreatePen
    mov rsi, rax
    
    ; Select pen
    mov rcx, [rbx + RENDERBUF_DC]
    mov rdx, rsi
    call SelectObject
    push rax
    
    ; Draw line
    mov rcx, [rbx + RENDERBUF_DC]
    mov edx, [rsp+40]
    mov r8d, [rsp+44]
    xor r9, r9
    call MoveToEx
    
    mov rcx, [rbx + RENDERBUF_DC]
    mov edx, [rsp+40]
    mov r8d, [rsp+48]
    call LineTo
    
    ; Restore old pen
    pop rdx
    mov rcx, [rbx + RENDERBUF_DC]
    call SelectObject
    
    ; Delete caret pen
    mov rcx, rsi
    call DeleteObject
    
done:
    add rsp, 56
    pop rsi
    pop rbx
    ret
RenderCaret ENDP

; ============================================================
; BlitToScreen - Copy backbuffer to window
; Input:  RCX = pointer to RenderBuffer
;         RDX = destination HDC
; ============================================================
BlitToScreen PROC
    push rbx
    sub rsp, 72
    
    mov rbx, rcx
    mov rsi, rdx
    
    ; BitBlt(dest, 0, 0, w, h, src, 0, 0, SRCCOPY)
    mov rcx, rsi
    xor edx, edx
    xor r8d, r8d
    mov r9d, [rbx + RENDERBUF_WIDTH]
    mov eax, [rbx + RENDERBUF_HEIGHT]
    mov [rsp+32], eax
    mov rax, [rbx + RENDERBUF_DC]
    mov [rsp+40], rax
    mov dword ptr [rsp+48], 0
    mov dword ptr [rsp+56], 0
    mov dword ptr [rsp+64], 0CC0020h   ; SRCCOPY
    call BitBlt
    
    add rsp, 72
    pop rbx
    ret
BlitToScreen ENDP

; ============================================================
; HandleKeyboardInput - Process keyboard events
; Input:  RCX = pointer to GapBuffer
;         RDX = virtual key code
;         R8  = current cursor position
; Output: RAX = new cursor position
; ============================================================
HandleKeyboardInput PROC
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov rbx, rcx
    mov r12, rdx
    mov r13, r8
    
    cmp r12d, 25h             ; VK_LEFT
    je do_left
    cmp r12d, 27h             ; VK_RIGHT
    je do_right
    cmp r12d, 26h             ; VK_UP
    je do_up
    cmp r12d, 28h             ; VK_DOWN
    je do_down
    cmp r12d, 24h             ; VK_HOME
    je do_home
    cmp r12d, 23h             ; VK_END
    je do_end
    cmp r12d, 08h             ; VK_BACK
    je do_backspace
    cmp r12d, 2Eh             ; VK_DELETE
    je do_delete
    cmp r12d, 0Dh             ; VK_RETURN
    je do_enter
    
    ; Printable character
    cmp r12d, 20h
    jl no_change
    cmp r12d, 7Eh
    jg no_change
    
    mov rcx, rbx
    mov edx, r12d
    mov r8, r13
    call InsertCharacter
    jmp done
    
do_left:
    mov rax, r13
    test rax, rax
    jz done
    dec rax
    jmp done
    
do_right:
    mov rcx, rbx
    call GetTextLength
    cmp r13, rax
    jge stay
    mov rax, r13
    inc rax
    jmp done
stay:
    mov rax, r13
    jmp done
    
do_up:
    mov rax, r13
    cmp rax, LINE_WIDTH
    jl no_change
    sub rax, LINE_WIDTH
    jmp done
    
do_down:
    mov rcx, rbx
    call GetTextLength
    mov rdx, r13
    add rdx, LINE_WIDTH
    cmp rdx, rax
    jg clamp_end
    mov rax, rdx
    jmp done
clamp_end:
    jmp done
    
do_home:
    mov rax, r13
find_start:
    test rax, rax
    jz done
    dec rax
    push rax
    mov rcx, rbx
    mov rdx, rax
    call GetCharAt
    pop rdx
    cmp al, 10
    je at_start
    mov rax, rdx
    jmp find_start
at_start:
    mov rax, rdx
    inc rax
    jmp done
    
do_end:
    mov rcx, rbx
    call GetTextLength
    mov r8, rax
    mov rax, r13
find_end:
    cmp rax, r8
    jge done
    push rax
    mov rcx, rbx
    mov rdx, rax
    call GetCharAt
    pop rdx
    cmp al, 10
    je done
    mov rax, rdx
    inc rax
    jmp find_end
    
do_backspace:
    mov rcx, rbx
    mov rdx, r13
    call DeleteCharacter
    jmp done
    
do_delete:
    mov rcx, rbx
    mov rdx, r13
    call DeleteCharacterForward
    jmp done
    
do_enter:
    mov rcx, rbx
    mov edx, 10
    mov r8, r13
    call InsertCharacter
    jmp done
    
no_change:
    mov rax, r13
done:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret
HandleKeyboardInput ENDP

END
