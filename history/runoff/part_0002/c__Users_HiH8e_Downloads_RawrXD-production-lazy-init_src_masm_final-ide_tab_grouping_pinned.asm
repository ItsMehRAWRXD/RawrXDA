;==========================================================================
; tab_grouping_pinned.asm - Tab Grouping and Pinned Tab Management
; ==========================================================================
; Features:
; - Group tabs by project name
; - Visual indicators for group membership
; - Pin/unpin frequently used tabs
; - Auto-collapse unused groups
; - Drag to reorder within groups
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_TAB_GROUPS      EQU 16
MAX_TABS_PER_GROUP  EQU 32
MAX_PROJECT_NAME    EQU 64

; Tab states
TAB_STATE_NORMAL    EQU 0
TAB_STATE_PINNED    EQU 1
TAB_STATE_MODIFIED  EQU 2
TAB_STATE_ACTIVE    EQU 4

; Group states
GROUP_STATE_EXPANDED EQU 1
GROUP_STATE_COLLAPSED EQU 0

;==========================================================================
; STRUCTURES
;==========================================================================
TAB_ENTRY STRUCT
    hwnd            QWORD ?
    label           BYTE 256 DUP (?)
    file_path       BYTE 512 DUP (?)
    state           DWORD ?             ; Pinned, Modified, Active
    project_id      DWORD ?             ; Group this tab belongs to
TAB_ENTRY ENDS

TAB_GROUP STRUCT
    project_name    BYTE MAX_PROJECT_NAME DUP (?)
    group_id        DWORD ?
    is_expanded     DWORD ?
    tab_count       DWORD ?
    pinned_count    DWORD ?
    color_id        DWORD ?             ; For visual distinction
    tabs            QWORD ?             ; Pointer to tab array
    hGroupControl   QWORD ?             ; Group UI element
GROUP ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Group management
    TabGroups           TAB_GROUP MAX_TAB_GROUPS DUP (<>)
    GroupCount          DWORD 0
    
    ; Tab entries
    AllTabs             TAB_ENTRY 128 DUP (<>)
    TabCount            DWORD 0
    
    ; Pinned tabs
    PinnedTabIndices    DWORD 32 DUP (0)
    PinnedTabCount      DWORD 0
    
    ; Colors for groups (COLORREF)
    GroupColors         DWORD 0FF0000h, 00FF000h, 00000FFh, 0FFFFh, 0FF00FFh, 0FFFF00h
    
    ; Strings
    szGroupPrefix       BYTE "[",0
    szGroupSuffix       BYTE "] ",0
    szPinnedIcon        BYTE "📌 ",0

.data?
    ; Current active group
    CurrentGroupId      DWORD ?
    
    ; Last click time (for double-click)
    LastClickTime       QWORD ?

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: tab_grouping_init() -> rax (success)
; Initialize tab grouping system
;==========================================================================
PUBLIC tab_grouping_init
tab_grouping_init PROC
    push rbx
    sub rsp, 32
    
    ; Zero group array
    lea rcx, TabGroups
    xor edx, edx
    mov r8d, MAX_TAB_GROUPS * (SIZE TAB_GROUP) / 8
    
.zero_groups:
    cmp r8d, 0
    je .zero_done
    mov QWORD PTR [rcx + rdx], 0
    add rdx, 8
    dec r8d
    jmp .zero_groups
    
.zero_done:
    mov GroupCount, 0
    mov TabCount, 0
    mov PinnedTabCount, 0
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rbx
    ret
tab_grouping_init ENDP

;==========================================================================
; PUBLIC: tab_create_group(project_name: rcx) -> eax (group_id)
; Create new tab group for project
;==========================================================================
PUBLIC tab_create_group
tab_create_group PROC
    push rbx
    
    mov rbx, GroupCount
    cmp rbx, MAX_TAB_GROUPS
    jge .group_limit
    
    ; Create new group
    imul eax, ebx, SIZE TAB_GROUP
    lea rax, TabGroups[rax]
    
    ; Copy project name
    lea rdx, [rax]                      ; project_name offset
    mov r8d, MAX_PROJECT_NAME
    call copy_string_group
    
    ; Initialize group
    mov DWORD PTR [rax + MAX_PROJECT_NAME], ebx  ; group_id = GroupCount
    mov DWORD PTR [rax + MAX_PROJECT_NAME + 4], GROUP_STATE_EXPANDED
    mov DWORD PTR [rax + MAX_PROJECT_NAME + 8], 0  ; tab_count
    mov DWORD PTR [rax + MAX_PROJECT_NAME + 12], 0 ; pinned_count
    
    ; Assign color
    mov edx, ebx
    and edx, 5                          ; Modulo 6 for colors
    mov eax, GroupColors[edx * 4]
    mov DWORD PTR [rax + MAX_PROJECT_NAME + 16], eax
    
    mov eax, ebx                        ; Return group_id
    inc GroupCount
    
    pop rbx
    ret
    
.group_limit:
    mov eax, -1                         ; Error: too many groups
    pop rbx
    ret
tab_create_group ENDP

;==========================================================================
; PUBLIC: tab_add_to_group(group_id: ecx, tab_entry: rdx) -> eax
; Add tab to group
;==========================================================================
PUBLIC tab_add_to_group
tab_add_to_group PROC
    push rbx
    push rdi
    sub rsp, 32
    
    mov ebx, ecx                        ; ebx = group_id
    mov rdi, rdx                        ; rdi = tab_entry
    
    ; Validate group_id
    cmp ebx, GroupCount
    jge .invalid_group
    
    ; Get group structure
    imul eax, ebx, SIZE TAB_GROUP
    lea rsi, TabGroups[rax]
    
    ; Get current tab count in group
    mov eax, DWORD PTR [rsi + MAX_PROJECT_NAME + 8]
    cmp eax, MAX_TABS_PER_GROUP
    jge .group_full
    
    ; Add tab to AllTabs
    mov eax, TabCount
    cmp eax, 128
    jge .tabs_full
    
    imul edx, eax, SIZE TAB_ENTRY
    lea rax, AllTabs[rdx]
    
    ; Copy tab entry
    mov rcx, rdi
    mov r8d, SIZE TAB_ENTRY
    call copy_memory_tab
    
    ; Set project_id
    mov DWORD PTR [rax + (256 + 512 + 0)], ebx  ; Set project_id field
    
    ; Update group tab count
    mov eax, DWORD PTR [rsi + MAX_PROJECT_NAME + 8]
    inc eax
    mov DWORD PTR [rsi + MAX_PROJECT_NAME + 8], eax
    
    inc TabCount
    
    mov eax, TabCount
    dec eax                             ; Return tab index
    add rsp, 32
    pop rdi
    pop rbx
    ret
    
.invalid_group:
    mov eax, -1
    add rsp, 32
    pop rdi
    pop rbx
    ret
    
.group_full:
    mov eax, -2
    add rsp, 32
    pop rdi
    pop rbx
    ret
    
.tabs_full:
    mov eax, -3
    add rsp, 32
    pop rdi
    pop rbx
    ret
tab_add_to_group ENDP

;==========================================================================
; PUBLIC: tab_pin(tab_index: ecx) -> eax (success)
; Pin/unpin a tab (keep it open at top)
;==========================================================================
PUBLIC tab_pin
tab_pin PROC
    push rbx
    
    ; Validate tab index
    cmp ecx, TabCount
    jge .invalid_tab
    
    ; Get tab entry
    imul eax, ecx, SIZE TAB_ENTRY
    lea rax, AllTabs[rax]
    
    ; Toggle pinned state
    mov edx, DWORD PTR [rax + (256 + 512)]  ; state field
    xor edx, TAB_STATE_PINNED
    mov DWORD PTR [rax + (256 + 512)], edx
    
    ; Update pinned array
    bt edx, 0                           ; Test pinned bit
    jc .now_pinned
    
    ; Unpinned - remove from pinned array
    xor eax, eax
    mov ebx, PinnedTabCount
    
.find_and_remove:
    cmp eax, ebx
    jge .remove_done
    
    cmp DWORD PTR PinnedTabIndices[rax * 4], ecx
    je .found_to_remove
    
    inc eax
    jmp .find_and_remove
    
.found_to_remove:
    ; Shift array
    mov edx, eax
    inc eax
    
.shift_loop:
    cmp eax, PinnedTabCount
    jge .remove_done
    
    mov ecx, DWORD PTR PinnedTabIndices[rax * 4]
    mov DWORD PTR PinnedTabIndices[rdx * 4], ecx
    
    inc eax
    inc edx
    jmp .shift_loop
    
.remove_done:
    dec PinnedTabCount
    jmp .pin_success
    
.now_pinned:
    ; Add to pinned array
    cmp PinnedTabCount, 32
    jge .pin_success
    
    mov eax, PinnedTabCount
    mov DWORD PTR PinnedTabIndices[rax * 4], ecx
    inc PinnedTabCount
    
.pin_success:
    mov eax, 1
    pop rbx
    ret
    
.invalid_tab:
    xor eax, eax
    pop rbx
    ret
tab_pin ENDP

;==========================================================================
; PUBLIC: tab_group_toggle_collapse(group_id: ecx) -> eax
; Toggle group expansion state
;==========================================================================
PUBLIC tab_group_toggle_collapse
tab_group_toggle_collapse PROC
    ; Get group
    imul eax, ecx, SIZE TAB_GROUP
    lea rax, TabGroups[rax]
    
    ; Toggle expanded state
    mov edx, DWORD PTR [rax + MAX_PROJECT_NAME + 4]
    xor edx, 1
    mov DWORD PTR [rax + MAX_PROJECT_NAME + 4], edx
    
    mov eax, edx                        ; Return new state
    ret
tab_group_toggle_collapse ENDP

;==========================================================================
; PUBLIC: tab_get_pinned_list(buffer: rcx, max_count: edx) -> eax
; Get list of pinned tab indices
;==========================================================================
PUBLIC tab_get_pinned_list
tab_get_pinned_list PROC
    mov rsi, rcx                        ; rsi = buffer
    mov edi, edx                        ; edi = max_count
    xor eax, eax
    
.copy_loop:
    cmp eax, PinnedTabCount
    jge .copy_done
    cmp eax, edi
    jge .copy_done
    
    mov edx, DWORD PTR PinnedTabIndices[rax * 4]
    mov DWORD PTR [rsi + rax * 4], edx
    
    inc eax
    jmp .copy_loop
    
.copy_done:
    ret
tab_get_pinned_list ENDP

;==========================================================================
; PRIVATE: copy_string_group(dst: rdx, src: rcx, max: r8d) -> void
;==========================================================================
PRIVATE copy_string_group
copy_string_group PROC
    xor eax, eax
    
.copy_loop:
    cmp eax, r8d
    jge .done
    
    movzx ebx, BYTE PTR [rcx + rax]
    mov BYTE PTR [rdx + rax], bl
    
    test bl, bl
    je .done
    
    inc eax
    jmp .copy_loop
    
.done:
    mov BYTE PTR [rdx + rax], 0
    ret
copy_string_group ENDP

;==========================================================================
; PRIVATE: copy_memory_tab(dst: rax, src: rcx, size: r8d) -> void
;==========================================================================
PRIVATE copy_memory_tab
copy_memory_tab PROC
    xor edx, edx
    
.copy_loop:
    cmp edx, r8d
    jge .copy_done
    
    movzx ebx, BYTE PTR [rcx + rdx]
    mov BYTE PTR [rax + rdx], bl
    
    inc edx
    jmp .copy_loop
    
.copy_done:
    ret
copy_memory_tab ENDP

END
