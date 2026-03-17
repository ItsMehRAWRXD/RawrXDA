;==========================================================================
; file_tree_driver.asm - File Tree with Drive Navigation
; ==========================================================================
; Implements:
; - Drive enumeration (GetLogicalDrives)
; - Directory traversal
; - File listing
; - TreeView control population
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_PATH_LEN        EQU 260
MAX_DRIVES          EQU 26
DRIVE_TYPE_UNKNOWN  EQU 0
DRIVE_TYPE_NO_DISK  EQU 1
DRIVE_TYPE_FLOPPY   EQU 2
DRIVE_TYPE_HDD      EQU 3
DRIVE_TYPE_NETWORK  EQU 4
DRIVE_TYPE_CDROM    EQU 5
DRIVE_TYPE_RAMDISK  EQU 6

; TreeView constants
TV_FIRST            EQU 1100h
TVM_INSERTITEMA     EQU TV_FIRST + 0
TVM_DELETEITEM      EQU TV_FIRST + 1
TVM_EXPAND          EQU TV_FIRST + 2
TVE_EXPAND          EQU 2

; TreeView item structure offsets
TVITEM_MASK         EQU 0
TVITEM_HITEM        EQU 4
TVITEM_STATE        EQU 12
TVITEM_PSZTEXT      EQU 16
TVITEM_CCHTEXT      EQU 24

;==========================================================================
; STRUCTURES
;==========================================================================
TV_ITEM STRUCT
    mask            DWORD ?
    hItem           QWORD ?
    state           DWORD ?
    stateMask       DWORD ?
    pszText         QWORD ?
    cchText         DWORD ?
    iImage          DWORD ?
    iSelectedImage  DWORD ?
    cChildren       DWORD ?
    lParam          QWORD ?
TV_ITEM ENDS

TV_INSERTSTRUCT STRUCT
    hParent         QWORD ?
    hInsertAfter    QWORD ?
    item            TV_ITEM <>
TV_INSERTSTRUCT ENDS

DRIVE_INFO STRUCT
    letter          BYTE ?
    drive_type      DWORD ?
    label           BYTE 32 DUP (?)
    drive_item_h    QWORD ?
DRIVE_INFO ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Drive strings
    szDrivePath     BYTE "%c:\",0
    szDriveLabel    BYTE "%c: (%s)",0
    szRemovable     BYTE "Removable Drive",0
    szFixed         BYTE "Fixed Drive",0
    szNetwork       BYTE "Network Drive",0
    szCDROM         BYTE "CD/DVD Drive",0
    szRAMDisk       BYTE "RAM Disk",0
    szUnknown       BYTE "Unknown",0
    
    ; File system strings
    szFolder        BYTE "[+] ",0
    szFile          BYTE "[F] ",0
    szRootName      BYTE "This PC",0
    
    ; Error messages
    szDriveEnumFail BYTE "Failed to enumerate drives",0
    szTreeViewFail  BYTE "Failed to create TreeView",0

.data?
    ; Drive enumeration
    DriveList       DRIVE_INFO MAX_DRIVES DUP (<>)
    DriveCount      DWORD ?
    LogicalDrives   DWORD ?
    
    ; TreeView handle
    hFileTreeView   QWORD ?
    hRootItem       QWORD ?
    
    ; Buffers
    DrivePathBuffer BYTE MAX_PATH_LEN DUP (?)
    DriveNameBuffer BYTE 64 DUP (?)

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: file_tree_init(hParent: rcx, x: edx, y: r8d, w: r9d, h: [rsp+40]) -> rax (hwnd)
; Initialize file tree view
;==========================================================================
PUBLIC file_tree_init
file_tree_init PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 64
    
    mov rbx, rcx        ; hParent
    mov r12d, edx       ; x
    mov r13d, r8d       ; y
    mov r14d, r9d       ; width
    mov r15d, [rsp + 96] ; height
    
    ; Create TreeView control
    xor rcx, rcx        ; dwExStyle
    lea rdx, szTreeViewClass
    lea r8, szFileTreeView
    mov r9d, WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_HSCROLL or TVS_LINESATROOT
    
    mov [rsp + 32], r12d ; x
    mov [rsp + 40], r13d ; y
    mov [rsp + 48], r14d ; width
    mov [rsp + 56], r15d ; height
    mov [rsp + 64], rbx  ; hWndParent
    mov DWORD PTR [rsp + 72], IDC_FILE_TREE ; hMenu
    
    call CreateWindowExA
    test rax, rax
    jz tree_view_fail
    
    mov hFileTreeView, rax
    
    ; Enumerate drives and populate tree
    call enumerate_drives
    test eax, eax
    jz enum_fail
    
    call populate_tree_drives
    test eax, eax
    jz populate_fail
    
    mov rax, hFileTreeView
    jmp init_done
    
tree_view_fail:
    lea rcx, szTreeViewFail
    call asm_log
    xor eax, eax
    jmp init_done
    
enum_fail:
    lea rcx, szDriveEnumFail
    call asm_log
    xor eax, eax
    jmp init_done
    
populate_fail:
    xor eax, eax
    
init_done:
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
file_tree_init ENDP

;==========================================================================
; INTERNAL: enumerate_drives() -> eax
; Enumerate all logical drives
;==========================================================================
enumerate_drives PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    ; Get logical drives bitmap
    call GetLogicalDrives
    mov LogicalDrives, eax
    test eax, eax
    jz enum_fail
    
    xor ebx, ebx        ; drive index
    mov edi, eax        ; drive bitmap
    xor esi, esi        ; drive count
    
enum_loop:
    cmp ebx, 26
    jae enum_done
    
    ; Check if bit is set
    mov eax, 1
    shl eax, ebx
    test edi, eax
    jz enum_next
    
    ; Add drive to list
    mov eax, esi
    cmp eax, MAX_DRIVES
    jae enum_done
    
    mov ecx, SIZEOF DRIVE_INFO
    imul eax, ecx
    lea rax, [DriveList + rax]
    
    ; Set drive letter (A=0, B=1, ...)
    add ebx, 'A'
    mov [rax + DRIVE_INFO.letter], bl
    sub ebx, 'A'
    
    ; Get drive type
    lea rcx, [DrivePathBuffer]
    mov dl, bl
    add dl, 'A'
    mov byte ptr [rcx], dl
    mov byte ptr [rcx + 1], ':'
    mov byte ptr [rcx + 2], '\'
    mov byte ptr [rcx + 3], 0
    
    mov rcx, DrivePathBuffer
    call GetDriveTypeA
    mov [rax + DRIVE_INFO.drive_type], eax
    
    inc esi
    
enum_next:
    inc ebx
    jmp enum_loop
    
enum_done:
    mov DriveCount, esi
    mov eax, 1
    jmp enum_exit
    
enum_fail:
    xor eax, eax
    
enum_exit:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
enumerate_drives ENDP

;==========================================================================
; INTERNAL: populate_tree_drives() -> eax
; Populate TreeView with enumerated drives
;==========================================================================
populate_tree_drives PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 96
    
    xor ebx, ebx        ; drive index
    
drive_loop:
    cmp ebx, DriveCount
    jae drives_done
    
    mov eax, ebx
    mov ecx, SIZEOF DRIVE_INFO
    imul eax, ecx
    lea rsi, [DriveList + rax]
    
    ; Format drive label
    lea rdi, [DriveNameBuffer]
    mov cl, [rsi + DRIVE_INFO.letter]
    mov [rdi], cl
    mov byte ptr [rdi + 1], ':'
    mov byte ptr [rdi + 2], 0
    
    ; Create TV_INSERTSTRUCT on stack
    lea rax, [rsp]      ; TV_INSERTSTRUCT at rsp
    
    ; hParent = TVI_ROOT (0)
    mov qword ptr [rax + TV_INSERTSTRUCT.hParent], 0
    
    ; hInsertAfter = TVI_LAST (-1 as qword)
    mov qword ptr [rax + TV_INSERTSTRUCT.hInsertAfter], -1
    
    ; item.mask = TVIF_TEXT | TVIF_CHILDREN
    mov DWORD PTR [rax + TV_INSERTSTRUCT.item + TVITEM_MASK], 1 or 4
    
    ; item.pszText
    lea rcx, [DriveNameBuffer]
    mov qword ptr [rax + TV_INSERTSTRUCT.item + TVITEM_PSZTEXT], rcx
    
    ; item.cchText
    mov DWORD PTR [rax + TV_INSERTSTRUCT.item + TVITEM_CCHTEXT], 3
    
    ; item.cChildren = 1 (has subdirs)
    mov DWORD PTR [rax + TV_INSERTSTRUCT.item + 48], 1
    
    ; Send TVM_INSERTITEMA
    mov rcx, hFileTreeView
    mov edx, TVM_INSERTITEMA
    xor r8d, r8d
    lea r9, [rsp]
    call SendMessageA
    
    ; Store item handle
    mov [rsi + DRIVE_INFO.drive_item_h], rax
    
    inc ebx
    jmp drive_loop
    
drives_done:
    mov eax, 1
    
    add rsp, 96
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
populate_tree_drives ENDP

;==========================================================================
; PUBLIC: file_tree_expand_drive(drive_id: ecx) -> eax
; Expand a drive to show directories
;==========================================================================
PUBLIC file_tree_expand_drive
file_tree_expand_drive PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    cmp ecx, DriveCount
    jae expand_fail
    
    ; Get drive info
    mov eax, ecx
    mov edx, SIZEOF DRIVE_INFO
    imul eax, edx
    lea rsi, [DriveList + rax]
    
    mov rbx, [rsi + DRIVE_INFO.drive_item_h]
    test rbx, rbx
    jz expand_fail
    
    ; Expand the item
    mov rcx, hFileTreeView
    mov edx, TVM_EXPAND
    mov r8d, TVE_EXPAND
    mov r9, rbx
    call SendMessageA
    
    mov eax, 1
    jmp expand_done
    
expand_fail:
    xor eax, eax
    
expand_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
file_tree_expand_drive ENDP

;==========================================================================
; PUBLIC: file_tree_refresh() -> eax
; Refresh entire file tree (re-enumerate drives)
;==========================================================================
PUBLIC file_tree_refresh
file_tree_refresh PROC
    push rbx
    sub rsp, 32
    
    ; Clear tree
    mov rcx, hFileTreeView
    mov edx, TVM_DELETEITEM
    xor r8d, r8d
    mov r9, -1         ; TVI_ROOT
    call SendMessageA
    
    ; Re-enumerate
    call enumerate_drives
    test eax, eax
    jz refresh_fail
    
    call populate_tree_drives
    test eax, eax
    jz refresh_fail
    
    mov eax, 1
    jmp refresh_done
    
refresh_fail:
    xor eax, eax
    
refresh_done:
    add rsp, 32
    pop rbx
    ret
file_tree_refresh ENDP

;==========================================================================
; DATA SECTION - CLASS NAMES & STRINGS
;==========================================================================
.data
    szTreeViewClass  BYTE "SysTreeView32",0
    szFileTreeView   BYTE "File Tree",0

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN GetLogicalDrives:PROC
EXTERN GetDriveTypeA:PROC
EXTERN asm_log:PROC
EXTERN CreateWindowExA:PROC
EXTERN SendMessageA:PROC
EXTERN strcmp_masm:PROC

