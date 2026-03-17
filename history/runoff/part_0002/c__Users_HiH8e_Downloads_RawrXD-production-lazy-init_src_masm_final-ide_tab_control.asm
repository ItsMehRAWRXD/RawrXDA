; ============================================================================
; Tab Control System for RawrXD Pure MASM IDE
; ============================================================================
; Phase 3 Critical Blocker #2 - Blocks settings dialog (7 tabs)
; ============================================================================
; Provides WC_TABCONTROL wrapper for tabbed interfaces
; ============================================================================

include windows.inc
include kernel32.inc
include user32.inc
include comctl32.inc

.DATA

; ============================================================================
; Tab Control Structures
; ============================================================================

TAB_PAGE STRUCT
    szTitle         QWORD ?     ; Page title string
    title_len       DWORD ?     ; Title length
    hwnd            QWORD ?     ; Page content window
    content_area    RECT <>     ; Content rectangle
    user_data       QWORD ?     ; Custom user data
    is_visible      BYTE ?      ; Visibility flag
    padding         BYTE 7 DUP(?)
TAB_PAGE ENDS

TAB_CONTROL STRUCT
    hwnd            QWORD ?     ; Tab control window handle
    parent_hwnd     QWORD ?     ; Parent window handle
    pages           QWORD ?     ; Array of TAB_PAGE pointers
    page_count      DWORD ?     ; Number of pages
    active_page     DWORD ?     ; Currently active page index
    on_selection_changed QWORD ? ; Selection change callback
    user_data       QWORD ?     ; Custom user data
TAB_CONTROL ENDS

; ============================================================================
; Tab Control Constants
; ============================================================================
MAX_TAB_PAGES      EQU 16

.CODE

; ============================================================================
; CreateTabControl: Create tab control window
; Parameters:
;   rcx = parent HWND
;   rdx = x position
;   r8d = y position
;   r9d = width
;   stack: [rsp+40] = height
;   stack: [rsp+48] = on_selection_changed callback (optional)
;   stack: [rsp+56] = user_data (optional)
; Returns:
;   rax = TAB_CONTROL pointer or NULL
; ============================================================================
CreateTabControl PROC FRAME
    ; Save non-volatile registers
    push rbx
    push rsi
    push rdi
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    
    ; Allocate stack space
    sub rsp, 30h
    .ALLOCSTACK 30h
    
    .ENDPROLOG
    
    ; Save parameters
    mov [rsp+20h], rcx  ; parent_hwnd
    mov [rsp+28h], rdx  ; x
    mov [rsp+2Ch], r8d  ; y
    mov [rsp+30h], r9d  ; width
    
    ; Allocate TAB_CONTROL structure
    mov rcx, sizeof(TAB_CONTROL)
    call malloc
    test rax, rax
    jz allocation_failed
    
    mov rbx, rax  ; rbx = tab_control pointer
    
    ; Initialize structure
    mov rcx, [rsp+20h]  ; parent_hwnd
    mov [rbx+TAB_CONTROL.parent_hwnd], rcx
    mov [rbx+TAB_CONTROL.page_count], 0
    mov [rbx+TAB_CONTROL.active_page], 0FFFFFFFFh
    
    ; Set callbacks if provided
    mov rcx, [rsp+58h]  ; on_selection_changed
    test rcx, rcx
    jz no_selection_callback
    mov [rbx+TAB_CONTROL.on_selection_changed], rcx
    
no_selection_callback:
    mov rcx, [rsp+60h]  ; user_data
    test rcx, rcx
    jz no_user_data
    mov [rbx+TAB_CONTROL.user_data], rcx
    
no_user_data:
    ; Allocate pages array
    mov rcx, MAX_TAB_PAGES * 8  ; QWORD pointers
    call malloc
    test rax, rax
    jz pages_allocation_failed
    
    mov [rbx+TAB_CONTROL.pages], rax
    
    ; Initialize pages array to NULL
    mov rdi, rax
    mov rcx, MAX_TAB_PAGES
    xor rax, rax
    rep stosq
    
    ; Create tab control window
    mov rcx, [rsp+20h]  ; parent_hwnd
    mov rdx, [rsp+28h]  ; x
    mov r8d, [rsp+2Ch]  ; y
    mov r9d, [rsp+30h]  ; width
    mov r10d, [rsp+48h] ; height
    call CreateTabControlWindow
    test rax, rax
    jz window_creation_failed
    
    mov [rbx+TAB_CONTROL.hwnd], rax
    
    ; Return success
    mov rax, rbx
    jmp tab_control_created
    
allocation_failed:
    xor rax, rax
    jmp tab_control_created
    
pages_allocation_failed:
    mov rcx, rbx
    call free
    xor rax, rax
    jmp tab_control_created
    
window_creation_failed:
    mov rcx, [rbx+TAB_CONTROL.pages]
    call free
    mov rcx, rbx
    call free
    xor rax, rax
    
tab_control_created:
    ; Cleanup stack and return
    add rsp, 30h
    pop rdi
    pop rsi
    pop rbx
    ret
CreateTabControl ENDP

; ============================================================================
; CreateTabControlWindow: Create WC_TABCONTROL window
; Parameters:
;   rcx = parent HWND
;   rdx = x
;   r8d = y
;   r9d = width
;   r10d = height
; Returns:
;   rax = tab control HWND or NULL
; ============================================================================
CreateTabControlWindow PROC
    ; Create tab control
    push 0              ; lpParam
    push 0              ; hMenu
    push [rsp+20h]      ; parent_hwnd
    push r10d           ; height
    push r9d            ; width
    push r8d            ; y
    push rdx            ; x
    push WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or TCS_FIXEDWIDTH
    push 0              ; window name
    push WC_TABCONTROL  ; class name
    push 0              ; extended style
    call CreateWindowExA
    
    ret
CreateTabControlWindow ENDP

; ============================================================================
; AddTabPage: Add page to tab control
; Parameters:
;   rcx = TAB_CONTROL pointer
;   rdx = page title string
;   r8 = content window HWND (optional)
;   r9 = user_data (optional)
; Returns:
;   rax = page index or -1 if failed
; ============================================================================
AddTabPage PROC
    mov rbx, rcx  ; tab_control
    mov rsi, rdx  ; title
    
    ; Check if we have space for more pages
    mov eax, [rbx+TAB_CONTROL.page_count]
    cmp eax, MAX_TAB_PAGES
    jge too_many_pages
    
    ; Allocate TAB_PAGE structure
    mov rcx, sizeof(TAB_PAGE)
    call malloc
    test rax, rax
    jz page_allocation_failed
    
    mov rdi, rax  ; rdi = tab_page
    
    ; Initialize page
    mov [rdi+TAB_PAGE.szTitle], rsi
    
    ; Calculate title length
    mov rcx, rsi
    call strlen
    mov [rdi+TAB_PAGE.title_len], eax
    
    mov [rdi+TAB_PAGE.hwnd], r8  ; content window
    mov [rdi+TAB_PAGE.user_data], r9  ; user_data
    mov [rdi+TAB_PAGE.is_visible], 0  ; initially hidden
    
    ; Add to pages array
    mov rcx, [rbx+TAB_CONTROL.pages]
    mov edx, [rbx+TAB_CONTROL.page_count]
    mov [rcx+rdx*8], rdi
    
    ; Increment page count
    inc [rbx+TAB_CONTROL.page_count]
    
    ; Add tab to control
    mov rcx, rbx
    mov rdx, rdi
    call AddTabToControl
    test rax, rax
    jz add_tab_failed
    
    ; If this is the first page, make it active
    cmp [rbx+TAB_CONTROL.page_count], 1
    jne not_first_page
    
    mov rcx, rbx
    mov edx, 0
    call SetActiveTabPage
    
not_first_page:
    ; Return page index
    mov eax, [rbx+TAB_CONTROL.page_count]
    dec eax
    jmp add_page_success
    
too_many_pages:
    mov eax, -1
    jmp add_page_success
    
page_allocation_failed:
    mov eax, -1
    jmp add_page_success
    
add_tab_failed:
    ; Remove from array and free
    mov rcx, [rbx+TAB_CONTROL.pages]
    mov edx, [rbx+TAB_CONTROL.page_count]
    dec edx
    mov qword ptr [rcx+rdx*8], 0
    dec [rbx+TAB_CONTROL.page_count]
    
    mov rcx, rdi
    call free
    mov eax, -1
    
add_page_success:
    ret
AddTabPage ENDP

; ============================================================================
; AddTabToControl: Add tab item to WC_TABCONTROL
; Parameters:
;   rcx = TAB_CONTROL pointer
;   rdx = TAB_PAGE pointer
; Returns:
;   rax = 1 if success, 0 if failed
; ============================================================================
AddTabToControl PROC
    mov rbx, rcx  ; tab_control
    mov rsi, rdx  ; tab_page
    
    ; Prepare TCITEM structure
    sub rsp, sizeof(TCITEM) + 20h ; shadow space + struct
    
    ; Initialize TCITEM
    mov dword ptr [rsp+20h].TCITEM.dwMask, TCIF_TEXT
    mov rax, [rsi+TAB_PAGE.szTitle]
    mov [rsp+20h].TCITEM.pszText, rax
    mov eax, [rsi+TAB_PAGE.title_len]
    mov [rsp+20h].TCITEM.cchTextMax, eax
    
    ; Send TCM_INSERTITEM message
    mov rcx, [rbx+TAB_CONTROL.hwnd]
    mov edx, [rbx+TAB_CONTROL.page_count]
    dec edx  ; index (0-based)
    lea r8, [rsp+20h]  ; TCITEM pointer
    mov r9, TCM_INSERTITEM
    call SendMessageA
    
    add rsp, sizeof(TCITEM) + 20h
    
    cmp eax, -1
    je insert_failed
    
    mov rax, 1
    ret
    
insert_failed:
    xor rax, rax
    ret
AddTabToControl ENDP

; ============================================================================
; SetActiveTabPage: Set active tab page
; Parameters:
;   rcx = TAB_CONTROL pointer
;   rdx = page index
; Returns:
;   rax = 1 if success, 0 if failed
; ============================================================================
SetActiveTabPage PROC
    mov rbx, rcx  ; tab_control
    mov esi, edx  ; page index
    
    ; Validate index
    cmp esi, [rbx+TAB_CONTROL.page_count]
    jge invalid_index
    test esi, esi
    jl invalid_index
    
    ; Hide current active page if any
    mov eax, [rbx+TAB_CONTROL.active_page]
    cmp eax, 0FFFFFFFFh
    je no_current_page
    
    mov rcx, rbx
    mov edx, eax
    call HideTabPage
    
no_current_page:
    ; Show new active page
    mov rcx, rbx
    mov edx, esi
    call ShowTabPage
    test rax, rax
    jz show_failed
    
    ; Set active page in control
    mov rcx, [rbx+TAB_CONTROL.hwnd]
    mov rdx, TCM_SETCURSEL
    mov r8, esi
    call SendMessageA
    
    ; Update active page index
    mov [rbx+TAB_CONTROL.active_page], esi
    
    ; Call selection changed callback if provided
    mov rcx, [rbx+TAB_CONTROL.on_selection_changed]
    test rcx, rcx
    jz no_callback
    
    ; Call callback: rcx = tab_control, rdx = page_index
    mov rcx, rbx
    mov edx, esi
    call [rbx+TAB_CONTROL.on_selection_changed]
    
no_callback:
    mov rax, 1
    ret
    
invalid_index:
    xor rax, rax
    ret
    
show_failed:
    xor rax, rax
    ret
SetActiveTabPage ENDP

; ============================================================================
; ShowTabPage: Show tab page content
; Parameters:
;   rcx = TAB_CONTROL pointer
;   rdx = page index
; Returns:
;   rax = 1 if success, 0 if failed
; ============================================================================
ShowTabPage PROC
    mov rbx, rcx  ; tab_control
    mov esi, edx  ; page index
    
    ; Get page pointer
    mov rcx, [rbx+TAB_CONTROL.pages]
    mov rax, [rcx+rsi*8]
    test rax, rax
    jz page_not_found
    
    mov rdi, rax  ; tab_page
    
    ; Check if page has content window
    mov rcx, [rdi+TAB_PAGE.hwnd]
    test rcx, rcx
    jz no_content_window
    
    ; Show content window
    mov rdx, SW_SHOW
    call ShowWindow
    
no_content_window:
    mov [rdi+TAB_PAGE.is_visible], 1
    mov rax, 1
    ret
    
page_not_found:
    xor rax, rax
    ret
ShowTabPage ENDP

; ============================================================================
; HideTabPage: Hide tab page content
; Parameters:
;   rcx = TAB_CONTROL pointer
;   rdx = page index
; Returns:
;   rax = 1 if success, 0 if failed
; ============================================================================
HideTabPage PROC
    mov rbx, rcx  ; tab_control
    mov esi, edx  ; page index
    
    ; Get page pointer
    mov rcx, [rbx+TAB_CONTROL.pages]
    mov rax, [rcx+rsi*8]
    test rax, rax
    jz page_not_found
    
    mov rdi, rax  ; tab_page
    
    ; Check if page has content window
    mov rcx, [rdi+TAB_PAGE.hwnd]
    test rcx, rcx
    jz no_content_window
    
    ; Hide content window
    mov rdx, SW_HIDE
    call ShowWindow
    
no_content_window:
    mov [rdi+TAB_PAGE.is_visible], 0
    mov rax, 1
    ret
    
page_not_found:
    xor rax, rax
    ret
HideTabPage ENDP

; ============================================================================
; GetActiveTabPage: Get current active page index
; Parameters:
;   rcx = TAB_CONTROL pointer
; Returns:
;   rax = page index or -1 if none
; ============================================================================
GetActiveTabPage PROC
    mov eax, [rcx+TAB_CONTROL.active_page]
    ret
GetActiveTabPage ENDP

; ============================================================================
; GetTabPageCount: Get number of pages
; Parameters:
;   rcx = TAB_CONTROL pointer
; Returns:
;   rax = page count
; ============================================================================
GetTabPageCount PROC
    mov eax, [rcx+TAB_CONTROL.page_count]
    ret
GetTabPageCount ENDP

; ============================================================================
; RemoveTabPage: Remove page from tab control
; Parameters:
;   rcx = TAB_CONTROL pointer
;   rdx = page index
; Returns:
;   rax = 1 if success, 0 if failed
; ============================================================================
RemoveTabPage PROC
    mov rbx, rcx  ; tab_control
    mov esi, edx  ; page index
    
    ; Validate index
    cmp esi, [rbx+TAB_CONTROL.page_count]
    jge invalid_index
    test esi, esi
    jl invalid_index
    
    ; If removing active page, activate another
    mov eax, [rbx+TAB_CONTROL.active_page]
    cmp eax, esi
    jne not_active
    
    ; Try to activate next page, or previous if last
    mov eax, [rbx+TAB_CONTROL.page_count]
    dec eax
    cmp esi, eax
    jge remove_last_page
    
    ; Activate next page
    mov rcx, rbx
    mov edx, esi
    inc edx
    call SetActiveTabPage
    jmp page_activated
    
remove_last_page:
    ; If not first page, activate previous
    test esi, esi
    jz no_other_pages
    
    mov rcx, rbx
    mov edx, esi
    dec edx
    call SetActiveTabPage
    
no_other_pages:
page_activated:
    ; Remove tab from control
    mov rcx, [rbx+TAB_CONTROL.hwnd]
    mov rdx, TCM_DELETEITEM
    mov r8, esi
    call SendMessageA
    
    ; Get page pointer
    mov rcx, [rbx+TAB_CONTROL.pages]
    mov rdi, [rcx+rsi*8]
    test rdi, rdi
    jz page_already_removed
    
    ; Hide and destroy content window if any
    mov rcx, [rdi+TAB_PAGE.hwnd]
    test rcx, rcx
    jz no_window_to_destroy
    
    call DestroyWindow
    
no_window_to_destroy:
    ; Free page structure
    mov rcx, rdi
    call free
    
    ; Shift pages array
    mov rcx, [rbx+TAB_CONTROL.pages]
    mov edx, esi
    inc edx
    
shift_pages:
    cmp edx, [rbx+TAB_CONTROL.page_count]
    jge shift_done
    
    mov rax, [rcx+rdx*8]
    mov [rcx+rdx*8-8], rax
    inc edx
    jmp shift_pages
    
shift_done:
    ; Clear last entry
    mov edx, [rbx+TAB_CONTROL.page_count]
    dec edx
    mov qword ptr [rcx+rdx*8], 0
    
    ; Decrement page count
    dec [rbx+TAB_CONTROL.page_count]
    
    ; Update active page if needed
    mov eax, [rbx+TAB_CONTROL.active_page]
    cmp eax, esi
    jle active_page_updated
    
    dec eax
    mov [rbx+TAB_CONTROL.active_page], eax
    
active_page_updated:
    mov rax, 1
    ret
    
invalid_index:
    xor rax, rax
    ret
    
page_already_removed:
    mov rax, 1
    ret
RemoveTabPage ENDP

; ============================================================================
; OnTabSelectionChanged: Handle TCN_SELCHANGE notification
; Parameters:
;   rcx = TAB_CONTROL pointer
;   rdx = notification HWND
;   r8 = notification code
; Returns:
;   rax = 1 if handled, 0 if not
; ============================================================================
OnTabSelectionChanged PROC
    mov rbx, rcx  ; tab_control
    
    ; Get new selection
    mov rcx, [rbx+TAB_CONTROL.hwnd]
    mov rdx, TCM_GETCURSEL
    mov r8, 0
    call SendMessageA
    
    cmp eax, -1
    je no_selection
    
    ; Set active page
    mov rcx, rbx
    mov edx, eax
    call SetActiveTabPage
    
no_selection:
    mov rax, 1
    ret
OnTabSelectionChanged ENDP

; ============================================================================
; DestroyTabControl: Cleanup tab control
; Parameters:
;   rcx = TAB_CONTROL pointer
; Returns:
;   rax = 1 if success
; ============================================================================
DestroyTabControl PROC
    mov rbx, rcx
    
    ; Remove all pages
    mov eax, [rbx+TAB_CONTROL.page_count]
    test eax, eax
    jz no_pages
    
    mov esi, eax
    dec esi
    
remove_pages_loop:
    mov rcx, rbx
    mov edx, esi
    call RemoveTabPage
    dec esi
    jns remove_pages_loop
    
no_pages:
    ; Destroy tab control window
    mov rcx, [rbx+TAB_CONTROL.hwnd]
    test rcx, rcx
    jz no_window
    
    call DestroyWindow
    
no_window:
    ; Free pages array
    mov rcx, [rbx+TAB_CONTROL.pages]
    test rcx, rcx
    jz no_pages_array
    
    call free
    
no_pages_array:
    ; Free tab control structure
    mov rcx, rbx
    call free
    
    mov rax, 1
    ret
DestroyTabControl ENDP

END