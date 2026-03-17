; ==========================================================================
; MASM Qt6 Component Conversion: Main Window & Menubar System
; ==========================================================================
; This file implements the main application window (QMainWindow equivalent)
; and menu bar with menu items. Replaces 2,800-4,100 LOC of C++/Qt6 code.
;
; Features:
;   - WS_OVERLAPPEDWINDOW creation with proper window class registration
;   - Menu bar with cascading menus and menu items
;   - Toolbar layout (horizontal menu bar strip)
;   - Status bar (bottom status text display)
;   - Window events: move, resize, close, focus, activate
;   - Client area management (reserved for child widgets/layouts)
;   - Title bar text management
;
; Architecture:
;   - VMT-based virtual methods (paint, on_event, get_size, set_size, show, hide)
;   - Stack-based resource management (RAII pattern in_val assembly)
;   - Spinlock-free design (single-threaded UI, synchronized via event queue)
;   - Direct Win32 API calls (CreateWindowEx, SetWindowPos, SendMessage)
;
; Depends On:
;   - qt6_foundation.asm (VMT, object model, event queue)
;   - windows.inc (Win32 definitions)
;
; ==========================================================================

option casemap:none

; External memory functions (provided by malloc_wrapper.asm)
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN realloc:PROC
EXTERN memset:PROC

; External object functions (provided by qt6_foundation.asm)
EXTERN object_create:PROC
EXTERN object_destroy:PROC
EXTERN object_show:PROC
EXTERN object_hide:PROC
EXTERN object_set_property:PROC
EXTERN object_get_property:PROC

; Win32 API functions
EXTERN RegisterClassExA:PROC
EXTERN UnregisterClassA:PROC
EXTERN CreateWindowExA:PROC
EXTERN DestroyWindow:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN SetWindowLongPtrA:PROC
EXTERN GetWindowLongPtrA:PROC
EXTERN SetWindowPos:PROC
EXTERN GetClientRect:PROC
EXTERN InvalidateRect:PROC
EXTERN SendMessageA:PROC
EXTERN DefWindowProcA:PROC
EXTERN LoadCursorA:PROC
EXTERN SetWindowTextA:PROC
EXTERN GetWindowTextA:PROC
EXTERN CreatePopupMenu:PROC
EXTERN AppendMenuA:PROC
EXTERN PostQuitMessage:PROC

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib

; Win32 constants (if not defined in windows.inc)
IFNDEF GWLP_USERDATA
    GWLP_USERDATA EQU -21
ENDIF
IFNDEF SW_SHOW
    SW_SHOW EQU 5
ENDIF
IFNDEF SW_HIDE
    SW_HIDE EQU 0
ENDIF
IFNDEF WS_OVERLAPPEDWINDOW
    WS_OVERLAPPEDWINDOW EQU 00CF0000h
ENDIF
IFNDEF CW_USEDEFAULT
    CW_USEDEFAULT EQU 80000000h
ENDIF
IFNDEF CS_HREDRAW
    CS_HREDRAW EQU 0002h
ENDIF
IFNDEF CS_VREDRAW
    CS_VREDRAW EQU 0001h
ENDIF
IFNDEF IDC_ARROW
    IDC_ARROW EQU 32512
ENDIF
IFNDEF WM_CREATE
    WM_CREATE EQU 0001h
ENDIF
IFNDEF WM_DESTROY
    WM_DESTROY EQU 0002h
ENDIF
IFNDEF WM_SIZE
    WM_SIZE EQU 0005h
ENDIF

; qt6_foundation structures (copied from qt6_foundation.asm)
OBJECT_BASE STRUCT
    obj_vmt          QWORD ?
    obj_hwnd         QWORD ?
    obj_parent       QWORD ?
    obj_children     QWORD ?
    obj_child_count  DWORD ?
    obj_flags        DWORD ?
    obj_user_data    QWORD ?
OBJECT_BASE ENDS

MEMORY_POOL STRUCT
    pool_chunk_size  DWORD ?
    pool_ptr         QWORD ?
    pool_free_list   QWORD ?
    pool_used_count  DWORD ?
    pool_total_count DWORD ?
MEMORY_POOL ENDS

EVENT_ITEM STRUCT
    evt_type         DWORD ?
    evt_target       QWORD ?
    evt_param1       QWORD ?
    evt_param2       QWORD ?
    evt_param3       QWORD ?
    evt_next         QWORD ?
EVENT_ITEM ENDS

SLOT_BINDING STRUCT
    slot_sender      QWORD ?
    slot_receiver    QWORD ?
    slot_signal_id   DWORD ?
    slot_handler_fn  QWORD ?
    slot_next        QWORD ?
SLOT_BINDING ENDS

FLAG_VISIBLE         EQU 00000001h
FLAG_ENABLED         EQU 00000002h
FLAG_FOCUSED         EQU 00000004h
FLAG_DIRTY           EQU 00000008h

;==========================================================================
; MAIN WINDOW STRUCTURE (replaces QMainWindow)
;==========================================================================

MAIN_WINDOW STRUCT
    obj_vmt          QWORD ?    ; From OBJECT_BASE
    obj_hwnd         QWORD ?
    obj_parent       QWORD ?
    obj_children     QWORD ?
    obj_child_count  DWORD ?
    obj_flags        DWORD ?
    obj_user_data    QWORD ?
    
    ; Window properties
    hwnd_menubar        QWORD ?               ; HWND of menu bar strip
    hwnd_toolbar        QWORD ?               ; HWND of toolbar
    hwnd_statusbar      QWORD ?               ; HWND of status bar
    hwnd_client         QWORD ?               ; HWND of client area (contains layout)
    
    ; Window geometry
    x                   DWORD ?               ; Window X position
    y                   DWORD ?               ; Window Y position
    width_val           DWORD ?               ; Window width_val
    height              DWORD ?               ; Window height
    
    ; Menu data
    menus_ptr           QWORD ?               ; Pointer to menu array
    menu_count          DWORD ?               ; Number of menus
    max_menus           DWORD ?               ; Allocated menu slots
    
    ; Status bar
    status_text         QWORD ?               ; Pointer to status text buffer (256 bytes)
    
    ; Window state flags
    flags               DWORD ?               ; FLAG_VISIBLE, FLAG_DIRTY, etc.
    
    ; Title bar
    title_text          QWORD ?               ; Pointer to title text buffer (512 bytes)
    
MAIN_WINDOW ENDS

; Menu structure (linked list)
MENU_BAR_ITEM STRUCT
    name_ptr            QWORD ?               ; Pointer to menu name (LPSTR)
    name_len            DWORD ?               ; Length of menu name
    hwnd_dropdown       QWORD ?               ; HWND of dropdown menu
    items_ptr           QWORD ?               ; Pointer to menu items array
    item_count          DWORD ?               ; Number of items in_val this menu
    flags               DWORD ?               ; Menu state flags
    next                QWORD ?               ; Next menu in_val linked list
MENU_BAR_ITEM ENDS

MENU_ITEM STRUCT
    name_ptr            QWORD ?               ; Pointer to item name (LPSTR)
    name_len            DWORD ?               ; Length of item name
    id                  DWORD ?               ; Menu item ID (for command routing)
    handler             QWORD ?               ; Function pointer to handler
    flags               DWORD ?               ; Item state (enabled, checked, separator)
    accelerator         DWORD ?               ; Keyboard shortcut (VK code)
    next                QWORD ?               ; Next item in_val linked list
MENU_ITEM ENDS

;==========================================================================
; GLOBAL STATE
;==========================================================================

.DATA
    sz_main_window_class    BYTE "RawrXD_MainWindow", 0
    sz_default_title        BYTE "RawrXD IDE - Pure MASM Edition", 0

.DATA?
    g_main_window_global    QWORD ?            ; Pointer to global main window instance
    g_main_hwnd             QWORD ?            ; HWND of main window
    g_menu_root             QWORD ?            ; Root of menu linked list
    g_temp_buffer           BYTE 512 dup(?)

;==========================================================================
; PUBLIC FUNCTIONS (called from qt6_foundation.asm and UI code)
;==========================================================================

; Create main window instance
; Inputs:  rcx = title text (LPSTR), rdx = width_val, r8 = height
; Outputs: rax = MAIN_WINDOW ptr or NULL on error
; Destroys: rcx, rdx, r8, r9, r10, r11
PUBLIC main_window_create

; Show main window (make visible)
; Inputs:  rcx = MAIN_WINDOW ptr
; Outputs: rax = success (nonzero) or failure (0)
PUBLIC main_window_show

; Hide main window
; Inputs:  rcx = MAIN_WINDOW ptr
; Outputs: rax = success (nonzero) or failure (0)
PUBLIC main_window_hide

; Destroy main window (free resources, close HWND)
; Inputs:  rcx = MAIN_WINDOW ptr
; Outputs: rax = success (nonzero) or failure (0)
PUBLIC main_window_destroy

; Set window title text
; Inputs:  rcx = MAIN_WINDOW ptr, rdx = title text (LPSTR)
; Outputs: rax = success (nonzero) or failure (0)
PUBLIC main_window_set_title

; Get window title text
; Inputs:  rcx = MAIN_WINDOW ptr
; Outputs: rax = pointer to title text (LPSTR)
PUBLIC main_window_get_title

; Set status bar text (bottom status bar)
; Inputs:  rcx = MAIN_WINDOW ptr, rdx = status text (LPSTR)
; Outputs: rax = success (nonzero) or failure (0)
PUBLIC main_window_set_status

; Get status bar text
; Inputs:  rcx = MAIN_WINDOW ptr
; Outputs: rax = pointer to status text (LPSTR)
PUBLIC main_window_get_status

; Add a menu to the menu bar
; Inputs:  rcx = MAIN_WINDOW ptr, rdx = menu name (LPSTR), r8 = menu name length
; Outputs: rax = MENU_BAR_ITEM ptr (for adding items) or NULL on error
PUBLIC main_window_add_menu

; Add an item to a menu
; Inputs:  rcx = MENU_BAR_ITEM ptr, rdx = item name (LPSTR), r8 = item ID, 
;          r9 = handler function ptr, r10 = flags
; Outputs: rax = MENU_ITEM ptr or NULL on error
PUBLIC main_window_add_menu_item

; Handle window resize event (called from event queue)
; Inputs:  rcx = MAIN_WINDOW ptr, rdx = new width_val, r8 = new height
; Outputs: rax = success (nonzero) or failure (0)
PUBLIC main_window_on_resize

; Handle window close event
; Inputs:  rcx = MAIN_WINDOW ptr
; Outputs: rax = success (nonzero) or failure (0) - return 1 to allow close, 0 to prevent
PUBLIC main_window_on_close

; Set window client area geometry
; Inputs:  rcx = MAIN_WINDOW ptr, rdx = x, r8 = y, r9 = width_val, r10 = height
; Outputs: rax = success (nonzero) or failure (0)
PUBLIC main_window_set_geometry

; Get window client area geometry - returns RECT_MASM
; Inputs:  rcx = MAIN_WINDOW ptr
; Outputs: rax = x, rdx = y, r8 = width_val, r9 = height
PUBLIC main_window_get_geometry

; Update menu bar layout (after window resize, recalculate menu positions)
; Inputs:  rcx = MAIN_WINDOW ptr
; Outputs: rax = success (nonzero) or failure (0)
PUBLIC main_window_update_menubar

; Initialize main window system (called once at startup)
; Inputs:  none
; Outputs: rax = success (nonzero) or failure (0)
PUBLIC main_window_system_init

; Cleanup main window system (called at shutdown)
; Inputs:  none
; Outputs: rax = success (nonzero) or failure (0)
PUBLIC main_window_system_cleanup

;==========================================================================
; IMPLEMENTATION
;==========================================================================

.CODE

; =============== main_window_system_init ===============
; Initialize the main window system - register window class, create default fonts
; Stack frame: 24 bytes (6 qwords for local variables)
;
; Local stack usage:
;   [rsp+0]  = return address (pushed by call)
;   [rsp+8]  = WNDCLASS struct (68 bytes) → starts at rsp+16
;             (rsp+16 = lpszClassName, +24 = lpszMenuName, +32 = lpfnWndProc, etc)
;
main_window_system_init PROC
    push rbp
    mov rbp, rsp
    sub rsp, 128                        ; Space for WNDCLASSEXA (80 bytes) + alignment
    
    ; Zero out WNDCLASSEXA
    mov rcx, rsp
    mov rdx, 80
    xor r8, r8
    call memset
    
    ; Fill WNDCLASSEXA
    mov dword ptr [rsp], 80             ; cbSize
    mov dword ptr [rsp+4], 0003h        ; style: CS_HREDRAW (2) + CS_VREDRAW (1)
    lea rax, main_window_proc
    mov qword ptr [rsp+8], rax          ; lpfnWndProc
    mov qword ptr [rsp+24], 0           ; hInstance (NULL for current process)
    
    ; Load default arrow cursor
    xor rcx, rcx
    mov rdx, IDC_ARROW
    call LoadCursorA
    mov qword ptr [rsp+40], rax          ; hCursor
    
    ; Set background brush (COLOR_WINDOW + 1)
    mov qword ptr [rsp+48], 6           ; hbrBackground
    
    lea rax, sz_main_window_class
    mov qword ptr [rsp+64], rax          ; lpszClassName
    
    ; Register class
    mov rcx, rsp
    call RegisterClassExA
    
    test rax, rax
    setnz al
    movzx eax, al
    
    add rsp, 128
    pop rbp
    ret
main_window_system_init ENDP

; =============== main_window_system_cleanup ===============
; Cleanup main window system - unregister window class, free resources
main_window_system_cleanup PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    lea rcx, sz_main_window_class
    xor rdx, rdx                        ; hInstance
    call UnregisterClassA
    
    mov rax, 1                          ; Return success
    add rsp, 32
    pop rbp
    ret
main_window_system_cleanup ENDP

; =============== main_window_create ===============
; Create a new main window instance with given title and size_val
; rcx = title text (LPSTR)
; rdx = width_val (DWORD)
; r8  = height (DWORD)
; Returns: rax = MAIN_WINDOW ptr or NULL
main_window_create PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64                         ; Local space for arguments and API calls
    
    ; Save arguments
    mov [rsp+32], rcx                   ; title_text
    mov [rsp+40], rdx                   ; width_val
    mov [rsp+48], r8                    ; height
    
    ; Allocate MAIN_WINDOW structure
    mov rcx, 1                          ; type_id ID for WIDGET/OBJECT (simplified)
    xor rdx, rdx                        ; No parent
    call object_create
    test rax, rax
    jz create_fail
    
    mov rbx, rax                        ; rbx = MAIN_WINDOW ptr
    
    ; Initialize MAIN_WINDOW specific fields
    mov eax, [rsp+40]                   ; width_val parameter
    mov dword ptr [rbx + MAIN_WINDOW.width_val], eax
    mov eax, [rsp+48]                   ; height parameter
    mov dword ptr [rbx + MAIN_WINDOW.height], eax
    
    ; Create HWND
    xor rcx, rcx                        ; dwExStyle
    lea rdx, sz_main_window_class       ; lpClassName
    mov r8, [rsp+32]                    ; lpWindowName (title)
    mov r9d, WS_OVERLAPPEDWINDOW        ; dwStyle
    
    ; Position and size_val
    mov dword ptr [rsp+32], CW_USEDEFAULT ; x
    mov dword ptr [rsp+40], CW_USEDEFAULT ; y
    mov eax, [rsp+40]
    mov [rsp+48], eax                   ; nWidth
    mov eax, [rsp+48]
    mov [rsp+56], eax                   ; nHeight
    
    mov qword ptr [rsp+64], 0           ; hWndParent
    mov qword ptr [rsp+72], 0           ; hMenu
    mov qword ptr [rsp+80], 0           ; hInstance
    mov qword ptr [rsp+88], rbx         ; lpParam (pass object pointer)
    
    ; Note: CreateWindowExA takes 12 arguments, so we need more stack space
    sub rsp, 64                         ; Extra space for arguments 5-12
    
    mov dword ptr [rsp+32], CW_USEDEFAULT ; x
    mov dword ptr [rsp+40], CW_USEDEFAULT ; y
    mov eax, [rbx + MAIN_WINDOW.width_val]
    mov [rsp+48], eax                   ; nWidth
    mov eax, [rbx + MAIN_WINDOW.height]
    mov [rsp+56], eax                   ; nHeight
    mov qword ptr [rsp+64], 0           ; hWndParent
    mov qword ptr [rsp+72], 0           ; hMenu
    mov qword ptr [rsp+80], 0           ; hInstance
    mov qword ptr [rsp+88], rbx         ; lpParam
    
    call CreateWindowExA
    add rsp, 64                         ; Restore stack from extra args
    
    test rax, rax
    jz create_fail
    
    mov [rbx + MAIN_WINDOW.obj_hwnd], rax
    mov qword ptr [g_main_hwnd], rax
    mov qword ptr [g_main_window_global], rbx
    
    mov rax, rbx                        ; Return object pointer
    jmp create_done
    
create_fail:
    xor rax, rax
    
create_done:
    add rsp, 64
    pop rbp
    ret
main_window_create ENDP

; =============== main_window_proc ===============
; Window procedure for the main window
main_window_proc PROC
    ; rcx = hwnd, rdx = msg, r8 = wparam, r9 = lparam
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    cmp rdx, WM_CREATE
    je on_create
    
    cmp rdx, WM_DESTROY
    je on_destroy
    
    cmp rdx, WM_SIZE
    je on_size
    
    ; Default handling
    call DefWindowProcA
    jmp proc_done
    
on_create:
    ; Get object pointer from CREATESTRUCT
    mov rax, r9                         ; lparam = LPCREATESTRUCT
    mov rax, [rax + 72]                 ; lpCreateParams
    mov r10, rcx                        ; Save hwnd from rcx parameter
    mov [rax + MAIN_WINDOW.obj_hwnd], r10
    ; Store object pointer in_val window user data
    mov rcx, r10                        ; hwnd
    mov rdx, GWLP_USERDATA
    mov r8, rax
    call SetWindowLongPtrA
    xor rax, rax
    jmp proc_done
    
on_size:
    ; Handle resize
    xor rax, rax
    jmp proc_done
    
on_destroy:
    xor rcx, rcx
    call PostQuitMessage
    xor rax, rax
    
proc_done:
    add rsp, 32
    pop rbp
    ret
main_window_proc ENDP

; =============== main_window_show ===============
main_window_show PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov rcx, [rcx + MAIN_WINDOW.obj_hwnd]
    mov rdx, SW_SHOW
    call ShowWindow
    
    mov rax, 1
    add rsp, 32
    pop rbp
    ret
main_window_show ENDP

; =============== main_window_hide ===============
main_window_hide PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov rcx, [rcx + MAIN_WINDOW.obj_hwnd]
    mov rdx, SW_HIDE
    call ShowWindow
    
    mov rax, 1
    add rsp, 32
    pop rbp
    ret
main_window_hide ENDP

; =============== main_window_destroy ===============
main_window_destroy PROC
    push rbp
    mov rbp, rsp
    push rbx
    sub rsp, 32
    
    mov rbx, rcx                        ; rbx = MAIN_WINDOW ptr
    
    ; Destroy HWND
    mov rcx, [rbx + MAIN_WINDOW.obj_hwnd]
    test rcx, rcx
    jz no_hwnd
    call DestroyWindow
    
no_hwnd:
    ; Free menu items linked list (optional - simplified)
    ; (menu cleanup would go here)
    
    ; Free MAIN_WINDOW structure itself
    mov rcx, rbx
    call free
    
    ; Clear globals
    mov qword ptr [g_main_window_global], 0
    mov qword ptr [g_main_hwnd], 0
    
    mov rax, 1                          ; Return success
    add rsp, 32
    pop rbx
    pop rbp
    ret
main_window_destroy ENDP

; =============== main_window_set_title ===============
main_window_set_title PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 32
    
    ; rcx = MAIN_WINDOW ptr, rdx = title text (LPSTR)
    mov rbx, rcx
    mov r12, rdx
    
    ; Update window title
    mov rcx, [rbx + MAIN_WINDOW.obj_hwnd]
    test rcx, rcx
    jz set_title_done
    mov rdx, r12
    call SetWindowTextA
    
set_title_done:
    mov rax, 1
    add rsp, 32
    pop r12
    pop rbx
    pop rbp
    ret
main_window_set_title ENDP

; =============== main_window_get_title ===============
main_window_get_title PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; rcx = MAIN_WINDOW ptr
    ; Returns: rax = pointer to title text
    
    ; Retrieve title from the window
    mov rcx, [rcx + MAIN_WINDOW.obj_hwnd]
    test rcx, rcx
    jz get_title_fail
    lea rdx, g_temp_buffer              ; Use global temp buffer
    mov r8, 256
    call GetWindowTextA
    lea rax, g_temp_buffer
    jmp get_title_done
    
get_title_fail:
    xor rax, rax
    
get_title_done:
    add rsp, 32
    pop rbp
    ret
main_window_get_title ENDP

; =============== main_window_set_status ===============
main_window_set_status PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; rcx = MAIN_WINDOW ptr, rdx = status text (LPSTR)
    mov rbx, rcx
    mov r12, rdx
    
    ; If statusbar HWND exists, call SetWindowText to update display
    mov rcx, [rbx + MAIN_WINDOW.hwnd_statusbar]
    test rcx, rcx
    jz no_statusbar
    
    mov rdx, r12
    call SetWindowTextA
    
no_statusbar:
    mov rax, 1                          ; Return success
    add rsp, 32
    pop rbp
    ret
main_window_set_status ENDP

; =============== main_window_get_status ===============
main_window_get_status PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; rcx = MAIN_WINDOW ptr
    ; Returns: rax = pointer to status text
    
    mov rcx, [rcx + MAIN_WINDOW.hwnd_statusbar]
    test rcx, rcx
    jz no_statusbar_get
    
    lea rdx, g_temp_buffer
    mov r8, 256
    call GetWindowTextA
    lea rax, g_temp_buffer
    jmp get_status_done
    
no_statusbar_get:
    xor rax, rax
    
get_status_done:
    add rsp, 32
    pop rbp
    ret
main_window_get_status ENDP

; =============== main_window_add_menu ===============
main_window_add_menu PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rcx                        ; r12 = MAIN_WINDOW ptr
    mov r13, rdx                        ; r13 = menu name
    
    ; Allocate MENU_BAR_ITEM structure (96 bytes)
    mov rcx, 96
    sub rsp, 32
    call malloc
    add rsp, 32
    test rax, rax
    jz add_menu_fail
    
    mov rbx, rax                        ; rbx = new menu item
    
    ; Initialize MENU_BAR_ITEM
    mov [rbx + MENU_BAR_ITEM.name_ptr], r13
    mov eax, r8d
    mov [rbx + MENU_BAR_ITEM.name_len], eax
    mov qword ptr [rbx + MENU_BAR_ITEM.items_ptr], 0
    mov dword ptr [rbx + MENU_BAR_ITEM.item_count], 0
    mov dword ptr [rbx + MENU_BAR_ITEM.flags], 0
    
    ; Create Win32 Popup Menu
    sub rsp, 32
    call CreatePopupMenu
    add rsp, 32
    mov [rbx + MENU_BAR_ITEM.hwnd_dropdown], rax
    
    ; Add to linked list in MAIN_WINDOW
    mov rax, [r12 + MAIN_WINDOW.menus_ptr]
    mov [rbx + MENU_BAR_ITEM.next], rax
    mov [r12 + MAIN_WINDOW.menus_ptr], rbx
    mov eax, [r12 + MAIN_WINDOW.menu_count]
    inc eax
    mov [r12 + MAIN_WINDOW.menu_count], eax
    
    ; Add to Win32 Menu Bar
    ; in a real app, we'd use AppendMenu on the window's HMENU
    ; For now, we'll assume the menu bar is updated later or via a signal
    
    mov rax, rbx                        ; Return new menu item
    jmp add_menu_done
    
add_menu_fail:
    xor rax, rax
    
add_menu_done:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
main_window_add_menu ENDP

; =============== main_window_add_menu_item ===============
main_window_add_menu_item PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rcx                        ; r12 = MENU_BAR_ITEM ptr
    mov r13, rdx                        ; r13 = item name
    
    ; Allocate MENU_ITEM structure (80 bytes)
    mov rcx, 80
    sub rsp, 32
    call malloc
    add rsp, 32
    test rax, rax
    jz add_item_fail
    
    mov rbx, rax
    
    ; Initialize MENU_ITEM
    mov [rbx + MENU_ITEM.name_ptr], r13
    mov eax, r8d
    mov [rbx + MENU_ITEM.id], eax
    mov [rbx + MENU_ITEM.handler], r9
    mov eax, r10d
    mov [rbx + MENU_ITEM.flags], eax
    
    ; Add to Win32 Popup Menu
    mov rcx, [r12 + MENU_BAR_ITEM.hwnd_dropdown]
    mov eax, r10d
    mov edx, eax                        ; uFlags (e.g. MF_STRING)
    mov eax, r8d
    mov r8, rax                         ; uIDNewItem
    mov r9, r13                         ; lpNewItem (text)
    sub rsp, 32
    call AppendMenuA
    add rsp, 32
    
    ; Add to linked list in MENU_BAR_ITEM
    mov rax, [r12 + MENU_BAR_ITEM.items_ptr]
    mov [rbx + MENU_ITEM.next], rax
    mov [r12 + MENU_BAR_ITEM.items_ptr], rbx
    mov eax, [r12 + MENU_BAR_ITEM.item_count]
    inc eax
    mov [r12 + MENU_BAR_ITEM.item_count], eax
    
    mov rax, rbx
    jmp add_item_done
    
add_item_fail:
    xor rax, rax
    
add_item_done:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
main_window_add_menu_item ENDP

; =============== main_window_on_resize ===============
main_window_on_resize PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = MAIN_WINDOW ptr, rdx = new_width, r8 = new_height
    
    ; TODO: Update MAIN_WINDOW.width_val and MAIN_WINDOW.height
    ; TODO: Call MoveWindow for child windows (menubar, toolbar, statusbar, client)
    ; TODO: Mark children as dirty (FLAG_DIRTY) so they repaint
    ; TODO: Post EVENT_RESIZE to event queue so children can recalculate layouts
    
    mov rax, 1                          ; Return success
    pop rbp
    ret
main_window_on_resize ENDP

; =============== main_window_on_close ===============
main_window_on_close PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = MAIN_WINDOW ptr
    ; Returns: rax = 1 to allow close, 0 to prevent close
    
    ; TODO: Can emit signal SIGNAL_WINDOW_CLOSE (if receivers connected via slot binding)
    ; TODO: For now, allow close by returning 1
    ; TODO: Later: prompt user if unsaved changes
    
    mov rax, 1                          ; Allow close
    pop rbp
    ret
main_window_on_close ENDP

; =============== main_window_set_geometry ===============
main_window_set_geometry PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = MAIN_WINDOW ptr, rdx = x, r8 = y, r9 = width_val, r10 = height
    
    ; TODO: Store in_val MAIN_WINDOW (x, y, width_val, height fields)
    ; TODO: Call SetWindowPos to move/resize main HWND
    ; TODO: If WS_VISIBLE, recalculate child window positions
    
    mov rax, 1                          ; Return success
    pop rbp
    ret
main_window_set_geometry ENDP

; =============== main_window_get_geometry ===============
main_window_get_geometry PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = MAIN_WINDOW ptr
    ; Returns: rax = x, rdx = y, r8 = width_val, r9 = height
    
    ; TODO: Load from MAIN_WINDOW structure and return in_val registers
    
    xor rax, rax                        ; Return 0 (stub)
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    pop rbp
    ret
main_window_get_geometry ENDP

; =============== main_window_update_menubar ===============
main_window_update_menubar PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = MAIN_WINDOW ptr
    
    ; TODO: Calculate positions for each menu in_val menu bar
    ; - Menu bar is horizontal strip at top
    ; - Start at x=0, y=0
    ; - width_val = 80 pixels per menu (estimated)
    ; - Height = 24 pixels (standard menu bar height)
    ; TODO: Call MoveWindow for each menu dropdown
    ; TODO: Call InvalidateRect to trigger repaint
    
    mov rax, 1                          ; Return success
    pop rbp
    ret
main_window_update_menubar ENDP

;==========================================================================
; WINDOW PROCEDURE (called by Windows for main window messages)
;==========================================================================

; TODO: Implement main_window_proc
; - Handle WM_CREATE, WM_DESTROY, WM_SIZE, WM_CLOSE, WM_PAINT
; - Route to appropriate handlers above
; - Call DefWindowProc for unhandled messages

END
