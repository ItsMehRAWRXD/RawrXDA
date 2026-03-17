; ============================================================================
; queue_viewer.asm - ListView rendering of queued build tasks
; ============================================================================

option casemap:none

; ----------------------------------------------------------------------------
; EXTERNALS
; ----------------------------------------------------------------------------
EXTERN SendMessageA:PROC
EXTERN hListView:QWORD
EXTERN Queue_GetCount:PROC
EXTERN Queue_GetItem:PROC

; ----------------------------------------------------------------------------
; CONSTANTS
; ----------------------------------------------------------------------------
LVM_DELETEALLITEMS  equ 1009h
LVM_INSERTITEM      equ 1001h
LVIF_TEXT           equ 0001h

; ----------------------------------------------------------------------------
; STRUCTURES
; ----------------------------------------------------------------------------
LV_ITEM STRUCT
    mask             dd ?
    iItem            dd ?
    iSubItem         dd ?
    state            dd ?
    stateMask        dd ?
    pszText          dq ?
    cchTextMax       dd ?
    iImage           dd ?
    lParam           dq ?
    iIndent          dd ?
    iGroupId         dd ?
    cColumns         dd ?
    puColumns        dq ?
    piColFmt         dq ?
    iGroup           dd ?
LV_ITEM ENDS

; ----------------------------------------------------------------------------
; PUBLICS
; ----------------------------------------------------------------------------
PUBLIC RefreshBuildQueue
PUBLIC QueueViewer_Update

; ----------------------------------------------------------------------------
; CODE
; ----------------------------------------------------------------------------
.code

RefreshBuildQueue PROC
    LOCAL lvi:LV_ITEM
    LOCAL count:DWORD
    LOCAL idx:DWORD

    invoke SendMessageA, hListView, LVM_DELETEALLITEMS, 0, 0

    call Queue_GetCount
    mov count, eax
    mov idx, 0

@loop_items:
    mov eax, idx
    cmp eax, count
    jae @done

    ; Fetch item text
    mov rcx, rax
    call Queue_GetItem
    test rax, rax
    jz @advance

    mov lvi.mask, LVIF_TEXT
    mov eax, idx
    mov lvi.iItem, eax
    mov lvi.iSubItem, 0
    mov lvi.pszText, rax
    mov lvi.cchTextMax, 0
    invoke SendMessageA, hListView, LVM_INSERTITEM, 0, ADDR lvi

@advance:
    inc idx
    jmp @loop_items

@done:
    ret
RefreshBuildQueue ENDP

QueueViewer_Update PROC
    call RefreshBuildQueue
    ret
QueueViewer_Update ENDP

END
