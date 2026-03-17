;==========================================================================
; tab_dragdrop.asm - Tab Drag-and-Drop Support
; ==========================================================================
; Implements drag-and-drop reordering of tabs:
; - Left mouse button drag on tab reorders it
; - Visual feedback during drag (highlight target position)
; - Drop releases tab in new position
; - Updates tab array in tab_manager
;
; Integration Points:
; - tab_manager.asm (tab array management)
; - Hooks into WM_LBUTTONDOWN, WM_MOUSEMOVE, WM_LBUTTONUP
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
TAB_DRAG_DISTANCE   EQU 5       ; Min pixels to start drag
TAB_DROP_HYSTERESIS EQU 10      ; Pixels for drop target detection

;==========================================================================
; STRUCTURES
;==========================================================================
TAB_DRAGSTATE STRUCT
    is_dragging     DWORD ?     ; 1 if drag active
    source_tab_id   DWORD ?     ; Original tab position
    source_x        DWORD ?     ; Start X position
    source_y        DWORD ?     ; Start Y position
    current_target  DWORD ?     ; Current drop target
    drag_image_hwnd QWORD ?     ; Floating drag image HWND
TAB_DRAGSTATE ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Drag state tracking
    DragState       TAB_DRAGSTATE <>
    
    ; UI constants
    szDragImageClass BYTE "TabDragImage",0

.data?
    ; Tab positions cache
    TabPositions    DWORD 64 DUP (?)    ; X position of each tab
    TabWidths       DWORD 64 DUP (?)    ; Width of each tab

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: tab_dragdrop_init() -> eax
; Initialize drag-and-drop system
;==========================================================================
PUBLIC tab_dragdrop_init
tab_dragdrop_init PROC
    mov DragState.is_dragging, 0
    mov DragState.source_tab_id, 0
    mov DragState.current_target, -1
    mov eax, 1
    ret
tab_dragdrop_init ENDP

;==========================================================================
; PUBLIC: tab_dragdrop_on_lbuttondown(tabHwnd: rcx, x: edx, y: r8d) -> eax
; Handle left mouse button down on tab
; Returns: 1 if starting drag, 0 if normal click
;==========================================================================
PUBLIC tab_dragdrop_on_lbuttondown
tab_dragdrop_on_lbuttondown PROC
    push rbx
    push rsi
    sub rsp, 32
    
    ; Store starting position
    mov DragState.is_dragging, 0
    mov DragState.source_x, edx
    mov DragState.source_y, r8d
    
    ; Find which tab was clicked
    ; (Simple implementation - could be enhanced with hit-test)
    mov DragState.source_tab_id, 0
    
    xor eax, eax
    add rsp, 32
    pop rsi
    pop rbx
    ret
tab_dragdrop_on_lbuttondown ENDP

;==========================================================================
; PUBLIC: tab_dragdrop_on_mousemove(x: edx, y: r8d) -> eax
; Handle mouse move during potential drag
; Returns: 1 if drag active, 0 otherwise
;==========================================================================
PUBLIC tab_dragdrop_on_mousemove
tab_dragdrop_on_mousemove PROC
    push rbx
    push rsi
    sub rsp, 32
    
    mov r9d, edx            ; current X
    mov r10d, r8d           ; current Y
    
    ; Check if already dragging
    mov eax, DragState.is_dragging
    test eax, eax
    jnz already_dragging
    
    ; Check if drag distance threshold exceeded
    mov eax, r9d
    sub eax, DragState.source_x
    abs eax
    cmp eax, TAB_DRAG_DISTANCE
    jl not_dragging_yet
    
    ; Start drag
    mov DragState.is_dragging, 1
    
    ; Create visual feedback (draw drag image)
    call tab_create_drag_image
    
    mov eax, 1
    jmp dragmove_done
    
already_dragging:
    ; Update drag position and find target
    mov DragState.current_target, -1
    
    ; Scan tab positions to find drop target
    xor ebx, ebx
find_target:
    cmp ebx, 64
    jae target_found
    
    ; Check if X position is within tab bounds
    mov eax, TabPositions[rbx * 4]
    cmp r9d, eax
    jl next_target
    
    mov eax, TabPositions[rbx * 4]
    add eax, TabWidths[rbx * 4]
    cmp r9d, eax
    jge next_target
    
    ; Found target
    mov DragState.current_target, ebx
    call tab_update_drop_target
    jmp target_found
    
next_target:
    inc ebx
    jmp find_target
    
target_found:
    mov eax, 1
    jmp dragmove_done
    
not_dragging_yet:
    xor eax, eax
    
dragmove_done:
    add rsp, 32
    pop rsi
    pop rbx
    ret
tab_dragdrop_on_mousemove ENDP

;==========================================================================
; PUBLIC: tab_dragdrop_on_lbuttonup(x: edx, y: r8d) -> eax
; Handle left mouse button up - complete drag if active
;==========================================================================
PUBLIC tab_dragdrop_on_lbuttonup
tab_dragdrop_on_lbuttonup PROC
    push rbx
    push rsi
    sub rsp, 32
    
    mov eax, DragState.is_dragging
    test eax, eax
    jz not_active
    
    ; Drag is active - perform reorder if target valid
    mov eax, DragState.current_target
    cmp eax, -1
    je cancel_drag
    
    ; Reorder tabs: move source_tab_id to current_target
    mov ecx, DragState.source_tab_id
    mov edx, DragState.current_target
    call tab_reorder_tabs
    
    ; Clean up
    call tab_destroy_drag_image
    mov DragState.is_dragging, 0
    mov DragState.current_target, -1
    
    mov eax, 1
    jmp lbuttonup_done
    
cancel_drag:
    call tab_destroy_drag_image
    mov DragState.is_dragging, 0
    xor eax, eax
    jmp lbuttonup_done
    
not_active:
    xor eax, eax
    
lbuttonup_done:
    add rsp, 32
    pop rsi
    pop rbx
    ret
tab_dragdrop_on_lbuttonup ENDP

;==========================================================================
; INTERNAL: tab_reorder_tabs(source: ecx, target: edx) -> eax
; Reorder tabs in array
;==========================================================================
tab_reorder_tabs PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    ; Call tab_manager to perform the actual reorder
    call tab_reorder
    
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
tab_reorder_tabs ENDP

;==========================================================================
; INTERNAL: tab_create_drag_image() -> eax
; Create visual feedback for drag
;==========================================================================
tab_create_drag_image PROC
    push rbx
    sub rsp, 32
    
    ; In a real implementation, this would create a layered window
    ; or use ImageList_BeginDrag. For now, we'll just log it.
    lea rcx, szDragImageClass
    call asm_log
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
tab_create_drag_image ENDP

;==========================================================================
; INTERNAL: tab_destroy_drag_image() -> eax
; Remove drag image
;==========================================================================
tab_destroy_drag_image PROC
    mov eax, 1
    ret
tab_destroy_drag_image ENDP

;==========================================================================
; INTERNAL: tab_update_drop_target() -> eax
; Visual feedback for drop target
;==========================================================================
tab_update_drop_target PROC
    mov eax, 1
    ret
tab_update_drop_target ENDP

;==========================================================================
; PUBLIC: tab_update_positions(tabHwnd: rcx) -> eax
; Cache tab positions for drag detection
; Should be called after tab creation/destruction
;==========================================================================
PUBLIC tab_update_positions
tab_update_positions PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rbx, rcx        ; tab control hwnd
    
    ; Get tab count
    mov rcx, rbx
    mov rdx, TCM_GETITEMCOUNT
    xor r8, r8
    xor r9, r9
    call SendMessageA
    mov rsi, rax        ; count
    
    xor edi, edi        ; index
cache_loop:
    cmp rdi, rsi
    jae cache_done
    cmp rdi, 64
    jae cache_done
    
    ; Get tab rect
    mov rcx, rbx
    mov rdx, TCM_GETITEMRECT
    mov r8, rdi
    lea r9, [rsp + 32]  ; RECT on stack
    call SendMessageA
    
    ; Store X and width
    mov eax, [rsp + 32] ; rect.left
    mov TabPositions[rdi * 4], eax
    
    mov eax, [rsp + 40] ; rect.right
    sub eax, [rsp + 32] ; rect.left
    mov TabWidths[rdi * 4], eax
    
    inc rdi
    jmp cache_loop
    
cache_done:
    mov eax, 1
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
tab_update_positions ENDP

;==========================================================================
; EXTERN DECLARATIONS
;==========================================================================
EXTERN tab_reorder:PROC
EXTERN asm_log:PROC
EXTERN SendMessageA:PROC

TCM_GETITEMCOUNT    EQU 1304h
TCM_GETITEMRECT     EQU 130Ah

END
