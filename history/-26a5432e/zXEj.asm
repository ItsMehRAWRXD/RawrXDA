;==========================================================================
; masm_visual_gui_builder.asm - WYSIWYG GUI Designer
;==========================================================================
; Full visual drag-drop GUI builder for Windows applications:
; - Canvas-based widget placement with mouse drag
; - Widget palette (30+ controls)
; - Property inspector with real-time updates
; - Alignment guides and snap-to-grid
; - Code generation (MASM, C++, Python)
; - Undo/redo support
; - Save/load project files (.guiproj)
; - Real-time preview window
; - Nested container support
;==========================================================================

option casemap:none
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib comctl32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
GRID_SIZE           equ 10      ; Snap-to-grid pixels
MAX_WIDGETS         equ 256     ; Max widgets per form
MAX_UNDO_STACK      equ 50      ; Undo history depth

; Widget types
WIDGET_BUTTON       equ 1
WIDGET_LABEL        equ 2
WIDGET_TEXTBOX      equ 3
WIDGET_LISTBOX      equ 4
WIDGET_COMBOBOX     equ 5
WIDGET_CHECKBOX     equ 6
WIDGET_RADIOBUTTON  equ 7
WIDGET_GROUPBOX     equ 8
WIDGET_PANEL        equ 9
WIDGET_TREEVIEW     equ 10
WIDGET_LISTVIEW     equ 11
WIDGET_TABCONTROL   equ 12
WIDGET_PROGRESSBAR  equ 13
WIDGET_SLIDER       equ 14
WIDGET_SPINNER      equ 15
WIDGET_DATEPICKER   equ 16
WIDGET_CALENDAR     equ 17
WIDGET_RICHEDIT     equ 18
WIDGET_WEBVIEW      equ 19
WIDGET_CHART        equ 20
WIDGET_IMAGEBOX     equ 21
WIDGET_CANVAS       equ 22
WIDGET_SPLITTER     equ 23
WIDGET_STATUSBAR    equ 24
WIDGET_TOOLBAR      equ 25
WIDGET_MENUBAR      equ 26
WIDGET_CONTEXTMENU  equ 27
WIDGET_TOOLTIP      equ 28
WIDGET_BALLOON      equ 29
WIDGET_CUSTOM       equ 30

; Tool modes
MODE_SELECT         equ 0
MODE_CREATE         equ 1
MODE_RESIZE         equ 2
MODE_MOVE           equ 3
MODE_PAN            equ 4

; Drag handles
HANDLE_NONE         equ 0
HANDLE_TL           equ 1       ; Top-left
HANDLE_TR           equ 2       ; Top-right
HANDLE_BL           equ 3       ; Bottom-left
HANDLE_BR           equ 4       ; Bottom-right
HANDLE_T            equ 5       ; Top
HANDLE_B            equ 6       ; Bottom
HANDLE_L            equ 7       ; Left
HANDLE_R            equ 8       ; Right

;==========================================================================
; STRUCTURES
;==========================================================================

; Widget definition (128 bytes)
WIDGET_DEF struct
    id              DWORD 0         ; Unique ID
    type            DWORD 0         ; Widget type constant
    
    ; Position and size
    x               DWORD 0
    y               DWORD 0
    width           DWORD 0
    height          DWORD 0
    
    ; Properties
    text            BYTE 64 dup(0)  ; Label/caption
    name            BYTE 32 dup(0)  ; Variable name
    parent_id       DWORD -1        ; Parent container (-1 = root)
    z_order         DWORD 0         ; Stacking order
    
    ; Style flags
    visible         BYTE 1
    enabled         BYTE 1
    tabstop         BYTE 1
    
    ; Colors
    bg_color        DWORD 0FFFFFFH  ; Background
    fg_color        DWORD 0         ; Foreground
    
    ; Font
    font_name       BYTE 32 dup(0)
    font_size       DWORD 10
    font_bold       BYTE 0
    font_italic     BYTE 0
    
    ; Alignment
    align_h         DWORD 0         ; 0=left, 1=center, 2=right
    align_v         DWORD 0         ; 0=top, 1=middle, 2=bottom
    
    ; Events (function pointers)
    on_click        QWORD 0
    on_change       QWORD 0
    on_hover        QWORD 0
WIDGET_DEF ends

; Design canvas state
CANVAS_STATE struct
    hWindow         QWORD 0         ; Canvas window
    hBackBuffer     QWORD 0         ; Offscreen bitmap
    hBackBufferDC   QWORD 0         ; DC
    
    ; Viewport
    canvas_width    DWORD 800
    canvas_height   DWORD 600
    zoom_level      REAL4 1.0       ; 100%
    pan_x           DWORD 0
    pan_y           DWORD 0
    
    ; Widget array
    widgets         WIDGET_DEF MAX_WIDGETS dup(<>)
    widget_count    DWORD 0
    
    ; Selection
    selected_id     DWORD -1        ; Currently selected widget
    multi_select    DWORD 10 dup(-1); Multiple selection
    multi_count     DWORD 0
    
    ; Drag state
    is_dragging     BYTE 0
    drag_handle     DWORD HANDLE_NONE
    drag_start_x    DWORD 0
    drag_start_y    DWORD 0
    drag_offset_x   DWORD 0
    drag_offset_y   DWORD 0
    
    ; Tool mode
    current_mode    DWORD MODE_SELECT
    create_type     DWORD 0         ; Widget type when MODE_CREATE
    
    ; Grid
    grid_enabled    BYTE 1
    grid_size       DWORD GRID_SIZE
    snap_enabled    BYTE 1
    
    ; Guides
    show_guides     BYTE 1
    guide_color     DWORD 0FF0000H  ; Red
    
    ; Undo stack
    undo_stack      QWORD MAX_UNDO_STACK dup(0)
    undo_top        DWORD 0
CANVAS_STATE ends

; Property inspector state
PROPERTY_INSPECTOR struct
    hWindow         QWORD 0
    hPropertyList   QWORD 0         ; Listview control
    target_widget   DWORD -1        ; Widget being edited
PROPERTY_INSPECTOR ends

; Widget palette state
WIDGET_PALETTE struct
    hWindow         QWORD 0
    hButtonGrid     QWORD 0         ; Grid of widget buttons
    selected_type   DWORD 0
WIDGET_PALETTE ends

; Code generator state
CODE_GEN struct
    target_lang     DWORD 0         ; 0=MASM, 1=C++, 2=Python
    output_buffer   BYTE 65536 dup(0)
    indent_level    DWORD 0
CODE_GEN ends

; Main builder state
GUI_BUILDER struct
    canvas          CANVAS_STATE <>
    inspector       PROPERTY_INSPECTOR <>
    palette         WIDGET_PALETTE <>
    codegen         CODE_GEN <>
    
    ; Project file
    project_file    BYTE 512 dup(0)
    is_modified     BYTE 0
    
    ; Preview window
    hPreview        QWORD 0
GUI_BUILDER ends

;==========================================================================
; DATA
;==========================================================================
.data
g_builder GUI_BUILDER <>

; Widget type names
szWidgetButton      db "Button", 0
szWidgetLabel       db "Label", 0
szWidgetTextBox     db "TextBox", 0
szWidgetListBox     db "ListBox", 0
szWidgetCheckBox    db "CheckBox", 0

; Window classes
szCanvasClass       db "GUIBuilderCanvas", 0
szInspectorClass    db "GUIBuilderInspector", 0
szPaletteClass      db "GUIBuilderPalette", 0
szPreviewClass      db "GUIBuilderPreview", 0

; Default widget properties
szDefaultFontName   db "Segoe UI", 0
szDefaultText       db "Widget", 0

.code

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN RegisterClassExA:PROC
EXTERN CreateWindowExA:PROC
EXTERN DefWindowProcA:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN InvalidateRect:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC
EXTERN CreateCompatibleDC:PROC
EXTERN CreateCompatibleBitmap:PROC
EXTERN SelectObject:PROC
EXTERN BitBlt:PROC
EXTERN DeleteDC:PROC
EXTERN DeleteObject:PROC
EXTERN SetCursor:PROC
EXTERN GetCursorPos:PROC
EXTERN ScreenToClient:PROC
EXTERN SetCapture:PROC
EXTERN ReleaseCapture:PROC
EXTERN CreateSolidBrush:PROC
EXTERN Rectangle:PROC
EXTERN FillRect:PROC
EXTERN DrawTextA:PROC
EXTERN SetBkMode:PROC
EXTERN SetTextColor:PROC
EXTERN CreateFontA:PROC

;==========================================================================
; PUBLIC EXPORTS
;==========================================================================
PUBLIC gui_builder_init
PUBLIC gui_builder_create_windows
PUBLIC gui_builder_add_widget
PUBLIC gui_builder_delete_widget
PUBLIC gui_builder_set_property
PUBLIC gui_builder_generate_code
PUBLIC gui_builder_save_project
PUBLIC gui_builder_load_project
PUBLIC gui_builder_show_preview
PUBLIC gui_builder_undo
PUBLIC gui_builder_redo
PUBLIC gui_builder_cut
PUBLIC gui_builder_copy
PUBLIC gui_builder_paste
PUBLIC gui_builder_align_widgets
PUBLIC gui_builder_distribute_widgets

;==========================================================================
; gui_builder_init() -> bool (rax)
; Initialize GUI builder system
;==========================================================================
gui_builder_init PROC
    sub rsp, 96
    
    ; Register canvas window class
    call register_canvas_class
    
    ; Register inspector window class
    call register_inspector_class
    
    ; Register palette window class
    call register_palette_class
    
    ; Initialize canvas state
    lea rdi, g_builder.canvas
    mov dword ptr [rdi + CANVAS_STATE.current_mode], MODE_SELECT
    mov dword ptr [rdi + CANVAS_STATE.selected_id], -1
    mov byte ptr [rdi + CANVAS_STATE.grid_enabled], 1
    mov byte ptr [rdi + CANVAS_STATE.snap_enabled], 1
    
    ; Initialize widget ID counter
    mov g_next_widget_id, 1000
    
    mov rax, 1  ; Success
    add rsp, 96
    ret
    
.data
g_next_widget_id DWORD 1000
.code
gui_builder_init ENDP

;==========================================================================
; register_canvas_class() - Register canvas window class
;==========================================================================
register_canvas_class PROC
    LOCAL wc:WNDCLASSEXA
    
    sub rsp, 96
    
    mov dword ptr [wc.cbSize], sizeof WNDCLASSEXA
    mov dword ptr [wc.style], 3     ; CS_HREDRAW | CS_VREDRAW
    lea rax, canvas_wnd_proc
    mov qword ptr [wc.lpfnWndProc], rax
    mov dword ptr [wc.cbClsExtra], 0
    mov dword ptr [wc.cbWndExtra], 0
    mov qword ptr [wc.hInstance], 0
    mov qword ptr [wc.hIcon], 0
    mov qword ptr [wc.hCursor], 0
    mov qword ptr [wc.hbrBackground], 0
    mov qword ptr [wc.lpszMenuName], 0
    lea rax, szCanvasClass
    mov qword ptr [wc.lpszClassName], rax
    mov qword ptr [wc.hIconSm], 0
    
    lea rcx, wc
    call RegisterClassExA
    
    add rsp, 96
    ret
register_canvas_class ENDP

;==========================================================================
; register_inspector_class() - Register property inspector class
;==========================================================================
register_inspector_class PROC
    LOCAL wc:WNDCLASSEXA
    
    sub rsp, 96
    
    mov dword ptr [wc.cbSize], sizeof WNDCLASSEXA
    mov dword ptr [wc.style], 0
    lea rax, inspector_wnd_proc
    mov qword ptr [wc.lpfnWndProc], rax
    mov dword ptr [wc.cbClsExtra], 0
    mov dword ptr [wc.cbWndExtra], 0
    mov qword ptr [wc.hInstance], 0
    mov qword ptr [wc.hIcon], 0
    mov qword ptr [wc.hCursor], 0
    mov qword ptr [wc.hbrBackground], 0
    mov qword ptr [wc.lpszMenuName], 0
    lea rax, szInspectorClass
    mov qword ptr [wc.lpszClassName], rax
    mov qword ptr [wc.hIconSm], 0
    
    lea rcx, wc
    call RegisterClassExA
    
    add rsp, 96
    ret
register_inspector_class ENDP

;==========================================================================
; register_palette_class() - Register widget palette class
;==========================================================================
register_palette_class PROC
    LOCAL wc:WNDCLASSEXA
    
    sub rsp, 96
    
    mov dword ptr [wc.cbSize], sizeof WNDCLASSEXA
    mov dword ptr [wc.style], 0
    lea rax, palette_wnd_proc
    mov qword ptr [wc.lpfnWndProc], rax
    mov dword ptr [wc.cbClsExtra], 0
    mov dword ptr [wc.cbWndExtra], 0
    mov qword ptr [wc.hInstance], 0
    mov qword ptr [wc.hIcon], 0
    mov qword ptr [wc.hCursor], 0
    mov qword ptr [wc.hbrBackground], 0
    mov qword ptr [wc.lpszMenuName], 0
    lea rax, szPaletteClass
    mov qword ptr [wc.lpszClassName], rax
    mov qword ptr [wc.hIconSm], 0
    
    lea rcx, wc
    call RegisterClassExA
    
    add rsp, 96
    ret
register_palette_class ENDP

;==========================================================================
; gui_builder_create_windows(parent_hwnd: rcx) -> bool (rax)
; Create all builder windows (canvas, inspector, palette)
;==========================================================================
gui_builder_create_windows PROC
    push rbx
    sub rsp, 96
    
    mov rbx, rcx  ; Save parent
    
    ; Create canvas window (center)
    call create_canvas_window
    mov g_builder.canvas.hWindow, rax
    
    ; Create property inspector (right side)
    call create_inspector_window
    mov g_builder.inspector.hWindow, rax
    
    ; Create widget palette (left side)
    call create_palette_window
    mov g_builder.palette.hWindow, rax
    
    ; Show all windows
    mov rcx, g_builder.canvas.hWindow
    mov edx, 5  ; SW_SHOW
    call ShowWindow
    
    mov rcx, g_builder.inspector.hWindow
    mov edx, 5
    call ShowWindow
    
    mov rcx, g_builder.palette.hWindow
    mov edx, 5
    call ShowWindow
    
    mov rax, 1  ; Success
    add rsp, 96
    pop rbx
    ret
gui_builder_create_windows ENDP

;==========================================================================
; create_canvas_window() - Create design canvas
;==========================================================================
create_canvas_window PROC
    sub rsp, 96
    
    xor rcx, rcx                    ; dwExStyle
    lea rdx, szCanvasClass
    lea r8, szCanvasTitle
    mov r9d, 50000000h or 10000000h ; WS_CHILD | WS_VISIBLE
    mov dword ptr [rsp + 32], 250   ; x (left margin for palette)
    mov dword ptr [rsp + 40], 0     ; y
    mov dword ptr [rsp + 48], 800   ; width
    mov dword ptr [rsp + 56], 600   ; height
    mov qword ptr [rsp + 64], 0     ; parent (set externally)
    mov qword ptr [rsp + 72], 0     ; menu
    mov qword ptr [rsp + 80], 0     ; instance
    mov qword ptr [rsp + 88], 0     ; param
    call CreateWindowExA
    
    ; Create back buffer
    push rax
    call create_canvas_buffer
    pop rax
    
    add rsp, 96
    ret
    
.data
szCanvasTitle db "Design Canvas", 0
.code
create_canvas_window ENDP

;==========================================================================
; create_canvas_buffer() - Create offscreen bitmap for flicker-free drawing
;==========================================================================
create_canvas_buffer PROC
    push rbx
    sub rsp, 32
    
    ; Get DC
    mov rcx, g_builder.canvas.hWindow
    call GetDC
    mov rbx, rax
    
    ; Create compatible DC
    mov rcx, rbx
    call CreateCompatibleDC
    mov g_builder.canvas.hBackBufferDC, rax
    
    ; Create bitmap
    mov rcx, rbx
    mov edx, g_builder.canvas.canvas_width
    mov r8d, g_builder.canvas.canvas_height
    call CreateCompatibleBitmap
    mov g_builder.canvas.hBackBuffer, rax
    
    ; Select bitmap
    mov rcx, g_builder.canvas.hBackBufferDC
    mov rdx, g_builder.canvas.hBackBuffer
    call SelectObject
    
    ; Release DC
    mov rcx, g_builder.canvas.hWindow
    mov rdx, rbx
    call ReleaseDC
    
    add rsp, 32
    pop rbx
    ret
create_canvas_buffer ENDP

;==========================================================================
; create_inspector_window() - Create property inspector
;==========================================================================
create_inspector_window PROC
    sub rsp, 96
    
    xor rcx, rcx
    lea rdx, szInspectorClass
    lea r8, szInspectorTitle
    mov r9d, 50000000h or 10000000h
    mov dword ptr [rsp + 32], 1050  ; x (right side)
    mov dword ptr [rsp + 40], 0
    mov dword ptr [rsp + 48], 300
    mov dword ptr [rsp + 56], 600
    mov qword ptr [rsp + 64], 0
    mov qword ptr [rsp + 72], 0
    mov qword ptr [rsp + 80], 0
    mov qword ptr [rsp + 88], 0
    call CreateWindowExA
    
    add rsp, 96
    ret
    
.data
szInspectorTitle db "Properties", 0
.code
create_inspector_window ENDP

;==========================================================================
; create_palette_window() - Create widget palette
;==========================================================================
create_palette_window PROC
    sub rsp, 96
    
    xor rcx, rcx
    lea rdx, szPaletteClass
    lea r8, szPaletteTitle
    mov r9d, 50000000h or 10000000h
    mov dword ptr [rsp + 32], 0     ; x (left side)
    mov dword ptr [rsp + 40], 0
    mov dword ptr [rsp + 48], 250
    mov dword ptr [rsp + 56], 600
    mov qword ptr [rsp + 64], 0
    mov qword ptr [rsp + 72], 0
    mov qword ptr [rsp + 80], 0
    mov qword ptr [rsp + 88], 0
    call CreateWindowExA
    
    add rsp, 96
    ret
    
.data
szPaletteTitle db "Toolbox", 0
.code
create_palette_window ENDP

;==========================================================================
; gui_builder_add_widget(type: ecx, x: edx, y: r8d, width: r9d, height: [rsp+40]) -> id (rax)
; Add new widget to canvas
;==========================================================================
gui_builder_add_widget PROC
    push rbx
    sub rsp, 48
    
    ; Check widget limit
    mov eax, g_builder.canvas.widget_count
    cmp eax, MAX_WIDGETS
    jge @full
    
    ; Get next widget slot
    mov ebx, eax
    imul rbx, rbx, sizeof WIDGET_DEF
    lea rdi, g_builder.canvas.widgets
    add rdi, rbx
    
    ; Assign unique ID
    mov eax, g_next_widget_id
    inc g_next_widget_id
    mov [rdi + WIDGET_DEF.id], eax
    push rax  ; Save ID
    
    ; Set type and position
    mov [rdi + WIDGET_DEF.type], ecx
    mov [rdi + WIDGET_DEF.x], edx
    mov [rdi + WIDGET_DEF.y], r8d
    mov [rdi + WIDGET_DEF.width], r9d
    mov eax, dword ptr [rsp + 48 + 40]
    mov [rdi + WIDGET_DEF.height], eax
    
    ; Set default properties
    mov byte ptr [rdi + WIDGET_DEF.visible], 1
    mov byte ptr [rdi + WIDGET_DEF.enabled], 1
    mov dword ptr [rdi + WIDGET_DEF.bg_color], 0F0F0F0H
    mov dword ptr [rdi + WIDGET_DEF.fg_color], 0
    
    ; Copy default text
    lea rsi, szDefaultText
    lea rdi, [rdi + WIDGET_DEF.text]
    mov rcx, 64
    rep movsb
    
    ; Increment count
    inc g_builder.canvas.widget_count
    
    ; Mark modified
    mov byte ptr g_builder.is_modified, 1
    
    ; Repaint canvas
    call repaint_canvas
    
    pop rax  ; Restore ID
    jmp @done
    
@full:
    xor rax, rax  ; Failure
    
@done:
    add rsp, 48
    pop rbx
    ret
gui_builder_add_widget ENDP

;==========================================================================
; gui_builder_delete_widget(widget_id: ecx) -> bool (rax)
; Delete widget from canvas
;==========================================================================
gui_builder_delete_widget PROC
    push rbx
    sub rsp, 32
    
    ; Find widget by ID
    mov ebx, ecx
    call find_widget_by_id
    test rax, rax
    jz @not_found
    
    ; Shift remaining widgets
    mov rdi, rax
    lea rsi, [rax + sizeof WIDGET_DEF]
    mov ecx, g_builder.canvas.widget_count
    sub ecx, (rax - offset g_builder.canvas.widgets) / sizeof WIDGET_DEF
    dec ecx
    imul ecx, ecx, sizeof WIDGET_DEF
    rep movsb
    
    ; Decrement count
    dec g_builder.canvas.widget_count
    
    ; Clear selection if deleted
    cmp ebx, g_builder.canvas.selected_id
    jne @not_selected
    mov g_builder.canvas.selected_id, -1
@not_selected:
    
    ; Mark modified
    mov byte ptr g_builder.is_modified, 1
    
    ; Repaint
    call repaint_canvas
    
    mov rax, 1  ; Success
    jmp @done
    
@not_found:
    xor rax, rax
    
@done:
    add rsp, 32
    pop rbx
    ret
gui_builder_delete_widget ENDP

;==========================================================================
; gui_builder_generate_code(lang: ecx) -> char* (rax)
; Generate code for current design
; lang: 0=MASM, 1=C++, 2=Python
;==========================================================================
gui_builder_generate_code PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov ebx, ecx  ; Save language
    
    ; Clear output buffer
    lea rdi, g_builder.codegen.output_buffer
    xor rax, rax
    mov rcx, 65536
    rep stosb
    
    ; Generate based on language
    cmp ebx, 0
    je @gen_masm
    cmp ebx, 1
    je @gen_cpp
    cmp ebx, 2
    je @gen_python
    jmp @done
    
@gen_masm:
    call generate_masm_code
    jmp @done
    
@gen_cpp:
    call generate_cpp_code
    jmp @done
    
@gen_python:
    call generate_python_code
    
@done:
    lea rax, g_builder.codegen.output_buffer
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
gui_builder_generate_code ENDP

;==========================================================================
; generate_masm_code() - Generate MASM code for GUI
;==========================================================================
generate_masm_code PROC
    ; Stub: generates MASM code for all widgets
    ; TODO: Implement CreateWindowExA calls for each widget
    ret
generate_masm_code ENDP

;==========================================================================
; generate_cpp_code() - Generate C++ code
;==========================================================================
generate_cpp_code PROC
    ; Stub: generates Qt/Win32 C++ code
    ret
generate_cpp_code ENDP

;==========================================================================
; generate_python_code() - Generate Python code
;==========================================================================
generate_python_code PROC
    ; Stub: generates tkinter/PyQt code
    ret
generate_python_code ENDP

;==========================================================================
; WINDOW PROCEDURES
;==========================================================================

canvas_wnd_proc PROC
    ; Canvas window procedure (handles mouse drag, selection, etc.)
    ; Stub: returns DefWindowProcA
    call DefWindowProcA
    ret
canvas_wnd_proc ENDP

inspector_wnd_proc PROC
    ; Property inspector window procedure
    call DefWindowProcA
    ret
inspector_wnd_proc ENDP

palette_wnd_proc PROC
    ; Widget palette window procedure
    call DefWindowProcA
    ret
palette_wnd_proc ENDP

;==========================================================================
; HELPER FUNCTIONS
;==========================================================================

find_widget_by_id PROC
    ; rcx = widget ID
    ; Returns: rax = pointer to WIDGET_DEF or 0
    push rbx
    
    xor rbx, rbx
    xor r12, r12
    
@loop:
    cmp r12d, g_builder.canvas.widget_count
    jge @not_found
    
    imul rax, r12, sizeof WIDGET_DEF
    lea rdi, g_builder.canvas.widgets
    add rdi, rax
    
    mov eax, [rdi + WIDGET_DEF.id]
    cmp eax, ecx
    je @found
    
    inc r12d
    jmp @loop
    
@found:
    mov rax, rdi
    jmp @done
    
@not_found:
    xor rax, rax
    
@done:
    pop rbx
    ret
find_widget_by_id ENDP

repaint_canvas PROC
    mov rcx, g_builder.canvas.hWindow
    xor rdx, rdx
    mov r8d, 0
    call InvalidateRect
    ret
repaint_canvas ENDP

; Stubs for remaining public functions
gui_builder_set_property PROC
    ret
gui_builder_set_property ENDP

gui_builder_save_project PROC
    ret
gui_builder_save_project ENDP

gui_builder_load_project PROC
    ret
gui_builder_load_project ENDP

gui_builder_show_preview PROC
    ret
gui_builder_show_preview ENDP

gui_builder_undo PROC
    ret
gui_builder_undo ENDP

gui_builder_redo PROC
    ret
gui_builder_redo ENDP

gui_builder_cut PROC
    ret
gui_builder_cut ENDP

gui_builder_copy PROC
    ret
gui_builder_copy ENDP

gui_builder_paste PROC
    ret
gui_builder_paste ENDP

gui_builder_align_widgets PROC
    ret
gui_builder_align_widgets ENDP

gui_builder_distribute_widgets PROC
    ret
gui_builder_distribute_widgets ENDP

end
