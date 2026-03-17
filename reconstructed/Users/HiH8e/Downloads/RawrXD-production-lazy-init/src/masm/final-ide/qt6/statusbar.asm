; ==========================================================================
; MASM Qt6 Component: Status Bar
; ==========================================================================
; Status bar displaying file info, cursor position, and mode indicators.
;
; Features:
;   - File name and modification status (*)
;   - Cursor position (line:column)
;   - File size_val in_val bytes
;   - Line ending mode (CRLF, LF, CR)
;   - Character encoding (UTF-8, ASCII)
;   - Mode indicators (INSERT, NORMAL, VISUAL)
;   - Zoom level percentage
;
; Architecture:
;   - STATUS_BAR structure (inherits OBJECT_BASE)
;   - STATUS_SEGMENT (left, center, right panels)
;   - Update on cursor move, file load, modification
;
; ==========================================================================

option casemap:none

; External memory functions (provided by malloc_wrapper.asm)
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN realloc:PROC
EXTERN memset:PROC

; Win32 API
EXTERN CreateWindowExA:PROC
EXTERN DestroyWindow:PROC
EXTERN SetWindowPos:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC
EXTERN TextOutA:PROC
EXTERN CreateFontA:PROC
EXTERN DeleteObject:PROC

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib

; Define OBJECT_BASE structure (from qt6_foundation)
OBJECT_BASE STRUCT
    obj_vmt          QWORD ?
    obj_hwnd         QWORD ?
    obj_parent       QWORD ?
    obj_children     QWORD ?
    obj_child_count  DWORD ?
    obj_flags        DWORD ?
    obj_user_data    QWORD ?
OBJECT_BASE ENDS

FLAG_VISIBLE         EQU 00000001h
FLAG_DIRTY           EQU 00000008h

;==========================================================================
; STRUCTURES
;=========================================================================

; Status bar segment
STATUS_SEGMENT STRUCT
    text_ptr        QWORD ?         ; Pointer to text buffer
    text_len        DWORD ?         ; Text length
    x               DWORD ?         ; Pixel position
    width_val       DWORD ?         ; Segment width_val
    color_bg        DWORD ?         ; Background color
    color_text      DWORD ?         ; Text color
    alignment       DWORD ?         ; ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT
STATUS_SEGMENT ENDS

; Status bar
STATUS_BAR STRUCT
    ; OBJECT_BASE fields
    obj_vmt          QWORD ?
    obj_hwnd         QWORD ?
    obj_parent       QWORD ?
    obj_children     QWORD ?
    obj_child_count  DWORD ?
    obj_flags        DWORD ?
    obj_user_data    QWORD ?
    
    ; Segments
    left_segment    STATUS_SEGMENT <>
    center_segment  STATUS_SEGMENT <>
    right_segment   STATUS_SEGMENT <>
    
    ; Text buffers (pre-allocated)
    left_text       BYTE 256 DUP(0)
    center_text     BYTE 256 DUP(0)
    right_text      BYTE 256 DUP(0)
    
    ; Dimensions
    hwnd            QWORD ?         ; Window handle
    x               DWORD ?         ; Position
    y               DWORD ?         ; Position
    width_val       DWORD ?         ; Bar width_val
    height          DWORD ?         ; Bar height (24 pixels)
    
    ; File state (references from editor)
    file_name_ptr   QWORD ?         ; Current file name
    file_size       QWORD ?         ; File size_val in_val bytes
    is_modified     DWORD ?         ; Modified flag (*)
    
    ; Cursor state
    cursor_line     DWORD ?         ; Current line (1-based display)
    cursor_col      DWORD ?         ; Current column (1-based display)
    
    ; Mode
    mode            DWORD ?         ; MODE_INSERT, MODE_NORMAL, MODE_VISUAL
    zoom_level      DWORD ?         ; 100%, 110%, 120%, etc.
    
    ; Encoding
    line_ending     DWORD ?         ; ENDING_CRLF, ENDING_LF, ENDING_CR
    encoding        DWORD ?         ; ENC_ASCII, ENC_UTF8
    
    ; Font
    font_handle     QWORD ?         ; GDI font
    brush_bg        QWORD ?         ; Background brush
    brush_text      QWORD ?         ; Text brush
    
    ; Flags
    flags           DWORD ?         ; FLAG_VISIBLE, FLAG_DIRTY
STATUS_BAR ENDS

;==========================================================================
; CONSTANTS
;==========================================================================

; Alignment
ALIGN_LEFT          EQU 0
ALIGN_CENTER        EQU 1
ALIGN_RIGHT         EQU 2

; Modes
MODE_NORMAL         EQU 0
MODE_INSERT         EQU 1
MODE_VISUAL         EQU 2

; Line endings
ENDING_CRLF         EQU 0           ; Windows
ENDING_LF           EQU 1           ; Unix/Linux
ENDING_CR           EQU 2           ; Old Mac

; Encoding
ENC_ASCII           EQU 0
ENC_UTF8            EQU 1

; Colors
COLOR_STATUSBAR_BG  EQU 0xF0F0F0    ; Light gray
COLOR_STATUSBAR_TX  EQU 0x000000    ; Black
COLOR_MODIFIED_TX   EQU 0xFF0000    ; Red (for *)
; Flags
; FLAG_VISIBLE and FLAG_DIRTY already defined in OBJECT_BASE section above

;==========================================================================
; PUBLIC FUNCTIONS
;===========================================================================

PUBLIC statusbar_create
PUBLIC statusbar_destroy
PUBLIC statusbar_update_cursor
PUBLIC statusbar_update_file
PUBLIC statusbar_update_mode
PUBLIC statusbar_set_zoom
PUBLIC statusbar_paint
PUBLIC statusbar_on_mouse

;==========================================================================
; IMPLEMENTATION
;==========================================================================

.CODE

; =============== statusbar_create ===============
; Create status bar instance
; Inputs:  rcx = hwnd, rdx = y position, r8 = width_val
; Outputs: rax = STATUS_BAR ptr
statusbar_create PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; TODO: Allocate STATUS_BAR structure (~1024 bytes with text buffers)
    ; TODO: Initialize segments:
    ;   - left: file name, file size_val (30% of width_val)
    ;   - center: cursor position, mode indicator (40% of width_val)
    ;   - right: zoom, encoding, line ending (30% of width_val)
    ; TODO: Create font (Segoe UI, 10pt)
    ; TODO: Create brushes (background, text)
    ; TODO: Set hwnd, width_val, height (24)
    ; TODO: Set flags = FLAG_VISIBLE
    
    xor rax, rax
    add rsp, 32
    pop rbp
    ret
statusbar_create ENDP

; =============== statusbar_destroy ===============
; Destroy status bar and free resources
; Inputs:  rcx = STATUS_BAR ptr
; Outputs: rax = success (1) or failure (0)
statusbar_destroy PROC
    push rbp
    mov rbp, rsp
    
    ; TODO: Delete font_handle (DeleteObject)
    ; TODO: Delete brush_bg, brush_text (DeleteObject)
    ; TODO: Free STATUS_BAR structure
    
    mov rax, 1
    pop rbp
    ret
statusbar_destroy ENDP

; =============== statusbar_update_cursor ===============
; Update cursor position display
; Inputs:  rcx = STATUS_BAR ptr, rdx = line (0-based), r8 = column (0-based)
; Outputs: rax = success (1) or failure (0)
statusbar_update_cursor PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; TODO: Convert 0-based line/col to 1-based for display
    ; TODO: Format center_text as "line:col" (e.g., "42:15")
    ; TODO: Set FLAG_DIRTY
    
    mov rax, 1
    add rsp, 32
    pop rbp
    ret
statusbar_update_cursor ENDP

; =============== statusbar_update_file ===============
; Update file name and size_val display
; Inputs:  rcx = STATUS_BAR ptr, rdx = file name (LPSTR), r8 = file size_val (QWORD), r9 = is_modified flag
; Outputs: rax = success (1) or failure (0)
statusbar_update_file PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; TODO: Extract file name from full path (last part after \)
    ; TODO: If is_modified, append "*"
    ; TODO: Format left_text as "filename* - 1024 bytes"
    ; TODO: Set FLAG_DIRTY
    
    mov rax, 1
    add rsp, 32
    pop rbp
    ret
statusbar_update_file ENDP

; =============== statusbar_update_mode ===============
; Update mode indicator
; Inputs:  rcx = STATUS_BAR ptr, rdx = mode (MODE_NORMAL, MODE_INSERT, MODE_VISUAL)
; Outputs: rax = success (1) or failure (0)
statusbar_update_mode PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; TODO: Set status_bar->mode = rdx
    ; TODO: Format right_text to include mode string:
    ;   - MODE_NORMAL → "NORMAL"
    ;   - MODE_INSERT → "INSERT"
    ;   - MODE_VISUAL → "VISUAL"
    ; TODO: Include zoom_level, encoding, line_ending
    ; TODO: Set FLAG_DIRTY
    
    mov rax, 1
    add rsp, 32
    pop rbp
    ret
statusbar_update_mode ENDP

; =============== statusbar_set_zoom ===============
; Set zoom level
; Inputs:  rcx = STATUS_BAR ptr, rdx = zoom percentage (100, 110, 120, etc.)
; Outputs: rax = success (1) or failure (0)
statusbar_set_zoom PROC
    push rbp
    mov rbp, rsp
    
    ; TODO: Clamp zoom to range [50, 200]
    ; TODO: Set zoom_level = rdx
    ; TODO: Update right_text with new zoom (e.g., "120%")
    ; TODO: Set FLAG_DIRTY
    ; TODO: Emit zoom_changed signal (if using Qt signals)
    
    mov rax, 1
    pop rbp
    ret
statusbar_set_zoom ENDP

; =============== statusbar_paint ===============
; Paint status bar to screen
; Inputs:  rcx = STATUS_BAR ptr, rdx = hwnd, r8 = hdc
; Outputs: rax = success (1) or failure (0)
statusbar_paint PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; TODO: Fill background (COLOR_STATUSBAR_BG)
    ; TODO: Draw vertical separators between segments
    ; TODO: Draw left segment text (file name, modified flag)
    ; TODO: Draw center segment text (cursor position, mode)
    ; TODO: Draw right segment text (zoom, encoding, line ending)
    ; TODO: Clear FLAG_DIRTY
    
    mov rax, 1
    add rsp, 32
    pop rbp
    ret
statusbar_paint ENDP

; =============== statusbar_on_mouse ===============
; Handle mouse click on status bar
; Inputs:  rcx = STATUS_BAR ptr, rdx = x, r8 = y, r9 = button (1=left, 2=right)
; Outputs: rax = segment clicked (0=left, 1=center, 2=right, -1=none)
statusbar_on_mouse PROC
    push rbp
    mov rbp, rsp
    
    ; TODO: Check if y in_val bar range (y >= statusbar->y && y < statusbar->y + 24)
    ; TODO: Check which segment was clicked based on x
    ; TODO: If right segment + left click → show zoom menu
    ; TODO: If right segment + right click → show encoding/line-ending menu
    
    mov rax, -1
    pop rbp
    ret
statusbar_on_mouse ENDP

; =============== Helper: format_file_size ===============
; Format file size_val as human-readable string
; Inputs:  rcx = size_val (QWORD), rdx = buffer ptr
; Outputs: rax = string length
format_file_size PROC
    push rbp
    mov rbp, rsp
    
    ; TODO: If size_val < 1024: format as "1234 bytes"
    ; TODO: If size_val < 1024*1024: format as "12.3 KB"
    ; TODO: If size_val < 1024*1024*1024: format as "123.4 MB"
    ; TODO: Return length of formatted string
    
    xor rax, rax
    pop rbp
    ret
format_file_size ENDP

END
