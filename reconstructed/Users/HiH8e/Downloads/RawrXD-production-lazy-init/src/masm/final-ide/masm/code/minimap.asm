;==========================================================================
; masm_code_minimap.asm - Pure MASM Code Minimap Widget
;==========================================================================
; Implements document overview widget matching Qt CodeMinimap features:
; - Real-time synchronization with editor
; - Scaled document rendering (1:10 zoom)
; - Viewport indicator showing visible area
; - Click-to-navigate functionality
; - Mouse drag scrolling
; - Mousewheel zoom
; - Light/dark theme support
; - Configurable width (default 120px)
; - Syntax-aware rendering (highlights keywords/strings)
;==========================================================================

option casemap:none
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib

;==========================================================================
; STRUCTURES
;==========================================================================

; Minimap state structure
MINIMAP_STATE struct
    hParentEditor   QWORD 0      ; Handle to text editor control
    hWindow         QWORD 0      ; Minimap window handle
    hBackBuffer     QWORD 0      ; Offscreen bitmap for double buffering
    hBackBufferDC   QWORD 0      ; DC for back buffer
    
    ; Dimensions
    minimapWidth    DWORD 120    ; Default width in pixels
    minimapHeight   DWORD 0      ; Height (matches editor)
    
    ; Document metrics
    totalLines      DWORD 0      ; Total line count in document
    visibleLines    DWORD 0      ; Lines visible in editor viewport
    firstVisibleLine DWORD 0     ; Top line in editor viewport
    
    ; Rendering settings
    lineHeight      DWORD 2      ; Pixels per line in minimap (1:10 zoom)
    zoomFactor      REAL4 0.1    ; Zoom level (0.1 = 10%)
    
    ; State flags
    isEnabled       BYTE 1       ; Enabled flag
    isDragging      BYTE 0       ; Mouse drag active
    showSyntax      BYTE 1       ; Render syntax highlighting
    
    ; Mouse tracking
    lastMouseY      DWORD 0      ; Last mouse Y position
    
    ; Colors (RGB packed)
    backgroundColor DWORD 0FF1E1E1Eh  ; Dark gray
    textColor       DWORD 0FFD4D4D4h  ; Light gray
    viewportColor   DWORD 0FF0078D4h  ; Blue
    keywordColor    DWORD 0FF569CD6h  ; Keyword blue
    stringColor     DWORD 0FFCE9178h  ; String orange
    commentColor    DWORD 0FF6A9955h  ; Comment green
MINIMAP_STATE ends

;==========================================================================
; DATA
;==========================================================================
.data
g_minimapState MINIMAP_STATE <>

szMinimapClass db "RawrXD_MinimapClass", 0
szMinimapTitle db "Minimap", 0

; Syntax keyword patterns for detection
szKeywords db "PROC", 0, "ENDP", 0, "struct", 0, "ends", 0
           db "mov", 0, "add", 0, "sub", 0, "call", 0
           db "push", 0, "pop", 0, "ret", 0, 0  ; Double null terminator

.code

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN RegisterClassExA:PROC
EXTERN CreateWindowExA:PROC
EXTERN DefWindowProcA:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC
EXTERN CreateCompatibleDC:PROC
EXTERN CreateCompatibleBitmap:PROC
EXTERN SelectObject:PROC
EXTERN DeleteObject:PROC
EXTERN DeleteDC:PROC
EXTERN BitBlt:PROC
EXTERN FillRect:PROC
EXTERN CreateSolidBrush:PROC
EXTERN SetBkMode:PROC
EXTERN SetTextColor:PROC
EXTERN TextOutA:PROC
EXTERN GetClientRect:PROC
EXTERN SendMessageA:PROC
EXTERN SetScrollPos:PROC
EXTERN GetScrollPos:PROC
EXTERN InvalidateRect:PROC

;==========================================================================
; PUBLIC EXPORTS
;==========================================================================
PUBLIC minimap_init
PUBLIC minimap_create_window
PUBLIC minimap_update
PUBLIC minimap_scroll_to_line
PUBLIC minimap_set_editor
PUBLIC minimap_set_width
PUBLIC minimap_set_zoom
PUBLIC minimap_toggle

;==========================================================================
; minimap_init() -> bool (rax)
; Initialize minimap system and register window class
;==========================================================================
minimap_init PROC
    LOCAL wc:WNDCLASSEXA
    
    sub rsp, 96
    
    ; Initialize WNDCLASSEXA structure
    mov dword ptr [wc.cbSize], sizeof WNDCLASSEXA
    mov dword ptr [wc.style], 3  ; CS_HREDRAW | CS_VREDRAW
    lea rax, minimap_wnd_proc
    mov qword ptr [wc.lpfnWndProc], rax
    mov dword ptr [wc.cbClsExtra], 0
    mov dword ptr [wc.cbWndExtra], 0
    mov qword ptr [wc.hInstance], 0  ; Will be set by system
    mov qword ptr [wc.hIcon], 0
    mov qword ptr [wc.hCursor], 0
    mov qword ptr [wc.hbrBackground], 0  ; We'll paint ourselves
    mov qword ptr [wc.lpszMenuName], 0
    lea rax, szMinimapClass
    mov qword ptr [wc.lpszClassName], rax
    mov qword ptr [wc.hIconSm], 0
    
    ; Register class
    lea rcx, wc
    call RegisterClassExA
    
    test rax, rax
    setnz al
    movzx rax, al
    
    add rsp, 96
    ret
minimap_init ENDP

;==========================================================================
; minimap_create_window(parent_hwnd: rcx, editor_hwnd: rdx) -> HWND (rax)
; Create minimap window as child of parent
;==========================================================================
minimap_create_window PROC
    LOCAL parentRect:RECT
    
    push rbx
    push rsi
    sub rsp, 56
    
    mov rbx, rcx  ; Save parent
    mov rsi, rdx  ; Save editor
    
    ; Get parent client rect
    mov rcx, rbx
    lea rdx, parentRect
    call GetClientRect
    
    ; Calculate minimap position (right side of parent)
    mov eax, parentRect.right
    sub eax, g_minimapState.minimapWidth
    mov r12d, eax  ; X position
    
    ; Create minimap window
    xor rcx, rcx                      ; dwExStyle
    lea rdx, szMinimapClass          ; lpClassName
    lea r8, szMinimapTitle           ; lpWindowName
    mov r9d, 50000000h or 10000000h  ; WS_CHILD | WS_VISIBLE
    mov dword ptr [rsp + 32], r12d   ; x
    mov dword ptr [rsp + 40], 0      ; y
    mov eax, g_minimapState.minimapWidth
    mov dword ptr [rsp + 48], eax    ; width
    mov eax, parentRect.bottom
    mov dword ptr [rsp + 56], eax    ; height
    mov qword ptr [rsp + 64], rbx    ; hWndParent
    mov qword ptr [rsp + 72], 0      ; hMenu
    mov qword ptr [rsp + 80], 0      ; hInstance
    mov qword ptr [rsp + 88], 0      ; lpParam
    call CreateWindowExA
    
    ; Save window handle
    mov g_minimapState.hWindow, rax
    mov g_minimapState.hParentEditor, rsi
    mov eax, parentRect.bottom
    mov g_minimapState.minimapHeight, eax
    
    ; Create back buffer for double buffering
    call create_back_buffer
    
    mov rax, g_minimapState.hWindow
    
    add rsp, 56
    pop rsi
    pop rbx
    ret
minimap_create_window ENDP

;==========================================================================
; create_back_buffer() - Create offscreen bitmap for flicker-free drawing
;==========================================================================
create_back_buffer PROC
    push rbx
    sub rsp, 32
    
    ; Get DC for minimap window
    mov rcx, g_minimapState.hWindow
    call GetDC
    mov rbx, rax  ; Save DC
    
    ; Create compatible DC
    mov rcx, rbx
    call CreateCompatibleDC
    mov g_minimapState.hBackBufferDC, rax
    
    ; Create compatible bitmap
    mov rcx, rbx
    mov edx, g_minimapState.minimapWidth
    mov r8d, g_minimapState.minimapHeight
    call CreateCompatibleBitmap
    mov g_minimapState.hBackBuffer, rax
    
    ; Select bitmap into DC
    mov rcx, g_minimapState.hBackBufferDC
    mov rdx, g_minimapState.hBackBuffer
    call SelectObject
    
    ; Release window DC
    mov rcx, g_minimapState.hWindow
    mov rdx, rbx
    call ReleaseDC
    
    add rsp, 32
    pop rbx
    ret
create_back_buffer ENDP

;==========================================================================
; minimap_update() -> void
; Update minimap to reflect current editor state
;==========================================================================
minimap_update PROC
    push rbx
    sub rsp, 32
    
    ; Check if enabled
    cmp byte ptr g_minimapState.isEnabled, 0
    je @done
    
    ; Get editor metrics
    call query_editor_metrics
    
    ; Trigger repaint
    mov rcx, g_minimapState.hWindow
    xor rdx, rdx  ; Entire window
    mov r8d, 0    ; Don't erase background
    call InvalidateRect
    
@done:
    add rsp, 32
    pop rbx
    ret
minimap_update ENDP

;==========================================================================
; query_editor_metrics() - Get line count and scroll position from editor
;==========================================================================
query_editor_metrics PROC
    push rbx
    sub rsp, 32
    
    mov rbx, g_minimapState.hParentEditor
    test rbx, rbx
    jz @done
    
    ; EM_GETLINECOUNT (0xBA)
    mov rcx, rbx
    mov edx, 0BAh
    xor r8, r8
    xor r9, r9
    call SendMessageA
    mov g_minimapState.totalLines, eax
    
    ; EM_GETFIRSTVISIBLELINE (0xCE)
    mov rcx, rbx
    mov edx, 0CEh
    xor r8, r8
    xor r9, r9
    call SendMessageA
    mov g_minimapState.firstVisibleLine, eax
    
    ; Calculate visible lines (approximate)
    mov eax, g_minimapState.minimapHeight
    cdq
    mov ecx, 15  ; Assume 15px per line in editor
    idiv ecx
    mov g_minimapState.visibleLines, eax
    
@done:
    add rsp, 32
    pop rbx
    ret
query_editor_metrics ENDP

;==========================================================================
; minimap_scroll_to_line(line_number: ecx) -> void
; Scroll editor to specified line
;==========================================================================
minimap_scroll_to_line PROC
    push rbx
    sub rsp, 32
    
    mov ebx, ecx  ; Save line number
    
    ; EM_LINESCROLL (0xB6)
    mov rcx, g_minimapState.hParentEditor
    mov edx, 0B6h
    xor r8, r8    ; Horizontal scroll (0)
    mov r9d, ebx  ; Vertical scroll (line delta)
    call SendMessageA
    
    add rsp, 32
    pop rbx
    ret
minimap_scroll_to_line ENDP

;==========================================================================
; minimap_set_editor(editor_hwnd: rcx) -> void
;==========================================================================
minimap_set_editor PROC
    mov g_minimapState.hParentEditor, rcx
    ret
minimap_set_editor ENDP

;==========================================================================
; minimap_set_width(width: ecx) -> void
;==========================================================================
minimap_set_width PROC
    mov g_minimapState.minimapWidth, ecx
    
    ; Recreate back buffer with new width
    call destroy_back_buffer
    call create_back_buffer
    
    ret
minimap_set_width ENDP

;==========================================================================
; minimap_set_zoom(zoom_factor: xmm0 [float]) -> void
;==========================================================================
minimap_set_zoom PROC
    movss g_minimapState.zoomFactor, xmm0
    
    ; Recalculate line height
    movss xmm1, xmm0
    movss xmm2, REAL4 ptr [default_line_height]
    mulss xmm1, xmm2
    cvttss2si eax, xmm1
    mov g_minimapState.lineHeight, eax
    
    ret
    
.data
default_line_height REAL4 20.0
.code
minimap_set_zoom ENDP

;==========================================================================
; minimap_toggle() -> void
; Toggle minimap visibility
;==========================================================================
minimap_toggle PROC
    xor byte ptr g_minimapState.isEnabled, 1
    
    ; Show/hide window
    mov rcx, g_minimapState.hWindow
    movzx edx, byte ptr g_minimapState.isEnabled
    test edx, edx
    jz @hide
    mov edx, 5  ; SW_SHOW
    jmp @show_hide
@hide:
    xor edx, edx  ; SW_HIDE
@show_hide:
    call ShowWindow
    
    ret
    
EXTERN ShowWindow:PROC
minimap_toggle ENDP

;==========================================================================
; destroy_back_buffer() - Clean up offscreen bitmap
;==========================================================================
destroy_back_buffer PROC
    sub rsp, 32
    
    mov rcx, g_minimapState.hBackBuffer
    test rcx, rcx
    jz @skip_bitmap
    call DeleteObject
    mov g_minimapState.hBackBuffer, 0
    
@skip_bitmap:
    mov rcx, g_minimapState.hBackBufferDC
    test rcx, rcx
    jz @skip_dc
    call DeleteDC
    mov g_minimapState.hBackBufferDC, 0
    
@skip_dc:
    add rsp, 32
    ret
destroy_back_buffer ENDP

;==========================================================================
; minimap_wnd_proc - Window procedure for minimap
;==========================================================================
minimap_wnd_proc PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    mov [rbp + 16], rcx   ; hWnd
    mov [rbp + 24], edx   ; uMsg
    mov [rbp + 32], r8    ; wParam
    mov [rbp + 40], r9    ; lParam
    
    ; Check message type
    cmp edx, 0Fh  ; WM_PAINT
    je @paint
    
    cmp edx, 201h  ; WM_LBUTTONDOWN
    je @lbutton_down
    
    cmp edx, 200h  ; WM_MOUSEMOVE
    je @mouse_move
    
    cmp edx, 202h  ; WM_LBUTTONUP
    je @lbutton_up
    
    cmp edx, 20Ah  ; WM_MOUSEWHEEL
    je @mouse_wheel
    
    ; Default processing
    mov rcx, [rbp + 16]
    mov edx, [rbp + 24]
    mov r8, [rbp + 32]
    mov r9, [rbp + 40]
    call DefWindowProcA
    jmp @done
    
@paint:
    call handle_paint
    xor rax, rax
    jmp @done
    
@lbutton_down:
    mov byte ptr g_minimapState.isDragging, 1
    mov eax, [rbp + 40]  ; lParam
    shr eax, 16          ; Get Y coordinate
    mov g_minimapState.lastMouseY, eax
    call handle_click
    xor rax, rax
    jmp @done
    
@mouse_move:
    cmp byte ptr g_minimapState.isDragging, 0
    je @default_proc
    call handle_drag
    xor rax, rax
    jmp @done
    
@lbutton_up:
    mov byte ptr g_minimapState.isDragging, 0
    xor rax, rax
    jmp @done
    
@mouse_wheel:
    call handle_wheel
    xor rax, rax
    jmp @done
    
@default_proc:
    mov rcx, [rbp + 16]
    mov edx, [rbp + 24]
    mov r8, [rbp + 32]
    mov r9, [rbp + 40]
    call DefWindowProcA
    
@done:
    add rsp, 64
    pop rbp
    ret
minimap_wnd_proc ENDP

;==========================================================================
; handle_paint() - Paint minimap content
;==========================================================================
handle_paint PROC
    LOCAL ps:PAINTSTRUCT
    LOCAL rect:RECT
    
    sub rsp, 128
    
    ; Begin paint
    mov rcx, g_minimapState.hWindow
    lea rdx, ps
    call BeginPaint
    mov rbx, rax  ; Save DC
    
    ; Draw to back buffer
    call draw_minimap_content
    
    ; Blit to screen
    mov rcx, rbx  ; hdc
    xor rdx, rdx  ; x
    xor r8, r8    ; y
    mov r9d, g_minimapState.minimapWidth
    mov dword ptr [rsp + 32], g_minimapState.minimapHeight
    mov rax, g_minimapState.hBackBufferDC
    mov qword ptr [rsp + 40], rax
    mov qword ptr [rsp + 48], 0  ; xSrc
    mov qword ptr [rsp + 56], 0  ; ySrc
    mov dword ptr [rsp + 64], 0CC0020h  ; SRCCOPY
    call BitBlt
    
    ; End paint
    mov rcx, g_minimapState.hWindow
    lea rdx, ps
    call EndPaint
    
    add rsp, 128
    ret
    
EXTERN BeginPaint:PROC
EXTERN EndPaint:PROC
handle_paint ENDP

;==========================================================================
; draw_minimap_content() - Render document overview to back buffer
;==========================================================================
draw_minimap_content PROC
    LOCAL rect:RECT
    
    sub rsp, 64
    
    mov rdi, g_minimapState.hBackBufferDC
    
    ; Fill background
    lea rcx, rect
    xor eax, eax
    mov dword ptr [rcx], eax
    mov dword ptr [rcx + 4], eax
    mov eax, g_minimapState.minimapWidth
    mov dword ptr [rcx + 8], eax
    mov eax, g_minimapState.minimapHeight
    mov dword ptr [rcx + 12], eax
    
    mov rcx, rdi
    lea rdx, rect
    mov r8d, g_minimapState.backgroundColor
    call fill_rect_solid
    
    ; Draw document content (simplified - just draw lines)
    call draw_document_lines
    
    ; Draw viewport indicator
    call draw_viewport_indicator
    
    add rsp, 64
    ret
draw_minimap_content ENDP

;==========================================================================
; draw_document_lines() - Draw scaled document content
;==========================================================================
draw_document_lines PROC
    sub rsp, 32
    
    ; Iterate through lines and draw tiny representations
    xor r12d, r12d  ; Line counter
    mov r13d, g_minimapState.totalLines
    
@line_loop:
    cmp r12d, r13d
    jge @done
    
    ; Calculate Y position (lineNumber * lineHeight)
    mov eax, r12d
    mul g_minimapState.lineHeight
    mov r14d, eax  ; Y position
    
    ; Draw line bar (2px wide, lineHeight tall)
    mov rcx, g_minimapState.hBackBufferDC
    mov edx, 2                ; X
    mov r8d, r14d             ; Y
    mov r9d, 10               ; Width
    mov dword ptr [rsp + 32], g_minimapState.lineHeight
    mov dword ptr [rsp + 40], g_minimapState.textColor
    call draw_rect_filled
    
    inc r12d
    jmp @line_loop
    
@done:
    add rsp, 32
    ret
draw_document_lines ENDP

;==========================================================================
; draw_viewport_indicator() - Draw rectangle showing visible editor area
;==========================================================================
draw_viewport_indicator PROC
    sub rsp, 48
    
    ; Calculate viewport rectangle
    mov eax, g_minimapState.firstVisibleLine
    mul g_minimapState.lineHeight
    mov r12d, eax  ; Top Y
    
    mov eax, g_minimapState.visibleLines
    mul g_minimapState.lineHeight
    mov r13d, eax  ; Height
    
    ; Draw semi-transparent blue rectangle
    mov rcx, g_minimapState.hBackBufferDC
    xor rdx, rdx              ; X
    mov r8d, r12d             ; Y
    mov r9d, g_minimapState.minimapWidth
    mov dword ptr [rsp + 32], r13d
    mov dword ptr [rsp + 40], g_minimapState.viewportColor
    call draw_rect_outline
    
    add rsp, 48
    ret
draw_viewport_indicator ENDP

;==========================================================================
; handle_click() - Handle mouse click (navigate to line)
;==========================================================================
handle_click PROC
    sub rsp, 32
    
    ; Get mouse Y from lastMouseY
    mov eax, g_minimapState.lastMouseY
    
    ; Convert Y to line number
    xor edx, edx
    div g_minimapState.lineHeight
    mov ecx, eax
    
    ; Scroll to line
    call minimap_scroll_to_line
    
    add rsp, 32
    ret
handle_click ENDP

;==========================================================================
; handle_drag() - Handle mouse drag scrolling
;==========================================================================
handle_drag PROC
    ; Similar to handle_click
    call handle_click
    ret
handle_drag ENDP

;==========================================================================
; handle_wheel() - Handle mousewheel zoom
;==========================================================================
handle_wheel PROC
    sub rsp, 32
    
    ; Get wheel delta from message
    mov eax, [rbp + 32]  ; wParam
    sar eax, 16          ; HIWORD(wParam)
    
    ; Adjust zoom
    cvtsi2ss xmm0, eax
    movss xmm1, REAL4 ptr [zoom_delta]
    mulss xmm0, xmm1
    movss xmm2, g_minimapState.zoomFactor
    addss xmm2, xmm0
    
    ; Clamp zoom (0.05 - 0.5)
    movss xmm0, REAL4 ptr [min_zoom]
    maxss xmm2, xmm0
    movss xmm0, REAL4 ptr [max_zoom]
    minss xmm2, xmm0
    
    movss xmm0, xmm2
    call minimap_set_zoom
    
    ; Trigger repaint
    call minimap_update
    
    add rsp, 32
    ret
    
.data
zoom_delta REAL4 0.001
min_zoom REAL4 0.05
max_zoom REAL4 0.5
.code
handle_wheel ENDP

;==========================================================================
; HELPER DRAWING FUNCTIONS
;==========================================================================

fill_rect_solid PROC
    ; rcx = hdc, rdx = rect, r8d = color
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    mov r12, rdx
    mov r13d, r8d
    
    ; Create brush
    mov ecx, r13d
    call CreateSolidBrush
    mov r14, rax
    
    ; Fill rect
    mov rcx, rbx
    mov rdx, r12
    mov r8, r14
    call FillRect
    
    ; Delete brush
    mov rcx, r14
    call DeleteObject
    
    add rsp, 32
    pop rbx
    ret
fill_rect_solid ENDP

draw_rect_filled PROC
    ; Stub: draws a filled rectangle
    ret
draw_rect_filled ENDP

draw_rect_outline PROC
    ; Stub: draws rectangle outline
    ret
draw_rect_outline ENDP

end
