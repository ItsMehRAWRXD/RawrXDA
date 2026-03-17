; RawrXD Agentic IDE - File Tree Implementation (Pure MASM)
; Working version with folder navigation support

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\shell32.inc
include \masm32\include\shlwapi.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\shell32.lib
includelib \masm32\lib\shlwapi.lib

.data
include constants.inc

extrn g_hInstance:DWORD
extrn g_hMainWindow:DWORD

public FileTree_OnNotify

; Minimal tree item structures to avoid dependency collisions
FT_TVITEM struct
    mask_field        dd ?
    hItem_field       dd ?
    state_field       dd ?
    stateMask_field   dd ?
    pszText_field     dd ?
    cchTextMax_field  dd ?
    iImage_field      dd ?
    iSelectedImage_field dd ?
    cChildren_field   dd ?
    lParam_field      dd ?
FT_TVITEM ends

FT_TVINSERTSTRUCT struct
    hParent_field         dd ?
    hInsertAfter_field    dd ?
    item_field            FT_TVITEM <>
FT_TVINSERTSTRUCT ends

; Item types for lParam tagging (bits 0-7)
ITEM_TYPE_DRIVE  equ 1
ITEM_TYPE_FOLDER equ 2
ITEM_TYPE_FILE   equ 3

; Populated flag (bit 16) - set after first expansion
ITEM_FLAG_POPULATED equ 10000h

; Data section
.data
    szRootText        db "Project",0
    szDrive           db 4 dup(0)
    szFindPattern     db "\*",0
    szBackslash       db "\",0
    szDot             db ".",0
    szDotDot          db "..",0

public g_hFileTree
    g_hFileTree       dd 0
    g_hRootItem       dd 0
    szSearchPath      db MAX_PATH dup(0)
    findData          WIN32_FIND_DATA <>

public CreateFileTree
public RefreshFileTree

; ============================================================================
; PROCEDURES
; ============================================================================

AddTreeItem proto :DWORD, :DWORD, :DWORD
EnsureTrailingSlash proto :DWORD
BuildPathForItem proto :DWORD, :DWORD
FileTree_OnNotify proto :DWORD

.code

; ---------------------------------------------------------------
; CreateFileTree - creates tree view with drives and folder support
; Returns: handle to tree view in eax
; ---------------------------------------------------------------
CreateFileTree proc
    LOCAL dwStyle:DWORD
    LOCAL dwDrives:DWORD
    LOCAL i:DWORD

    ; Create tree view control
    mov dwStyle, WS_CHILD or WS_VISIBLE or TVS_HASBUTTONS or TVS_HASLINES or TVS_LINESATROOT or TVS_SHOWSELALWAYS
    invoke CreateWindowEx, 0, addr szTreeClass, NULL, dwStyle, 0, 0, 200, 400, g_hMainWindow, IDC_FILETREE, g_hInstance, NULL
    mov g_hFileTree, eax
    .if eax == 0
        xor eax, eax
        ret
    .endif

    ; Add root item and tag as folder
    push ITEM_TYPE_FOLDER
    push offset szRootText
    push TVI_ROOT
    call AddTreeItem
    mov g_hRootItem, eax

    ; Enumerate drives
    invoke GetLogicalDrives
    mov dwDrives, eax
    mov i, 0
    
    jmp @CheckLoop
    
@DriveLoop:
    mov eax, 1
    mov ecx, i
    shl eax, cl
    and eax, dwDrives
    .if eax != 0
        ; Build drive string
        mov al, 'A'
        add al, BYTE PTR i
        mov BYTE PTR szDrive[0], al
        mov BYTE PTR szDrive[1], ':'
        mov BYTE PTR szDrive[2], 92
        mov BYTE PTR szDrive[3], 0
        
        ; Add drive to tree and tag as drive
        push ITEM_TYPE_DRIVE
        push offset szDrive
        push g_hRootItem
        call AddTreeItem
    .endif
    
    inc i
    
@CheckLoop:
    cmp i, 26
    jl @DriveLoop
    
    mov eax, g_hFileTree
    ret
CreateFileTree endp

; ---------------------------------------------------------------
; RefreshFileTree - clears and rebuilds the entire tree view
; ---------------------------------------------------------------
RefreshFileTree proc
    LOCAL dwDrives:DWORD
    LOCAL i:DWORD
    
    ; Guard: ensure tree exists
    cmp g_hFileTree, 0
    je @RefreshDone
    
    ; Clear all items
    invoke SendMessage, g_hFileTree, TVM_DELETEITEM, 0, TVI_ROOT
    
    ; Add root item
    push ITEM_TYPE_FOLDER
    push offset szRootText
    push TVI_ROOT
    call AddTreeItem
    mov g_hRootItem, eax
    
    ; Re-enumerate drives
    invoke GetLogicalDrives
    mov dwDrives, eax
    mov i, 0
    
    jmp @RefreshCheckLoop
    
@RefreshDriveLoop:
    mov eax, 1
    mov ecx, i
    shl eax, cl
    and eax, dwDrives
    .if eax != 0
        ; Build drive string (A: B: C: etc)
        mov al, 'A'
        add al, BYTE PTR i
        mov BYTE PTR szDrive[0], al
        mov BYTE PTR szDrive[1], ':'
        mov BYTE PTR szDrive[2], 92
        mov BYTE PTR szDrive[3], 0
        
        ; Add drive item tagged as ITEM_TYPE_DRIVE
        push ITEM_TYPE_DRIVE
        push offset szDrive
        push g_hRootItem
        call AddTreeItem
    .endif
    
    inc i
    
@RefreshCheckLoop:
    cmp i, 26
    jl @RefreshDriveLoop
    
@RefreshDone:
    ret
RefreshFileTree endp

; ---------------------------------------------------------------
; AddTreeItem - helper to insert an item with lParam tagging
; Returns: new tree item handle in eax
; ---------------------------------------------------------------
AddTreeItem proc hParent:DWORD, pszText:DWORD, dwType:DWORD
    LOCAL tvins:FT_TVINSERTSTRUCT

    mov eax, hParent
    mov tvins.hParent_field, eax
    mov tvins.hInsertAfter_field, TVI_LAST

    mov eax, TVIF_TEXT or TVIF_PARAM
    mov tvins.item_field.mask_field, eax
    mov tvins.item_field.state_field, 0
    mov tvins.item_field.stateMask_field, 0

    mov eax, pszText
    mov tvins.item_field.pszText_field, eax
    mov tvins.item_field.cchTextMax_field, 0
    mov tvins.item_field.iImage_field, 0
    mov tvins.item_field.iSelectedImage_field, 0

    mov eax, dwType
    mov tvins.item_field.lParam_field, eax
    mov tvins.item_field.cChildren_field, 1
    cmp eax, ITEM_TYPE_FILE
    jne @notFile
    mov tvins.item_field.cChildren_field, 0
@notFile:

    lea eax, tvins
    push eax
    push 0
    push TVM_INSERTITEM
    push g_hFileTree
    call SendMessage
    ret
AddTreeItem endp

; ---------------------------------------------------------------
; EnsureTrailingSlash - appends "\" if missing
; ---------------------------------------------------------------
EnsureTrailingSlash proc pszBuffer:DWORD
    invoke lstrlen, pszBuffer
    mov ecx, eax
    mov edx, pszBuffer
    .if ecx == 0
        mov BYTE PTR [edx], 92
        mov BYTE PTR [edx+1], 0
    .else
        mov al, BYTE PTR [edx+ecx-1]
        .if al != 92
            mov BYTE PTR [edx+ecx], 92
            mov BYTE PTR [edx+ecx+1], 0
        .endif
    .endif
    ret
EnsureTrailingSlash endp

; ---------------------------------------------------------------
; BuildPathForItem - builds full filesystem path for a tree item
; Input: hItem (tree item), pBuffer -> output buffer (MAX_PATH)
; ---------------------------------------------------------------
BuildPathForItem proc hItem:DWORD, pBuffer:DWORD
    LOCAL curItem:DWORD
    LOCAL tvget:FT_TVITEM
    LOCAL szTmp[260]:BYTE
    LOCAL szAcc[260]:BYTE
    LOCAL szNew[260]:BYTE

    ; initialize accumulator to empty
    mov eax, pBuffer
    mov BYTE PTR [eax], 0
    lea eax, szAcc
    mov BYTE PTR [eax], 0

    mov eax, hItem
    mov curItem, eax

    ; loop up to root, prepending each segment
@loop:
    cmp curItem, 0
    je @done

    ; get text of current item
    mov tvget.mask_field, TVIF_TEXT
    lea eax, szTmp
    mov tvget.pszText_field, eax
    mov tvget.cchTextMax_field, 259
    mov eax, curItem
    mov tvget.hItem_field, eax

    ; Send TVM_GETITEM
    lea eax, tvget
    push eax
    push 0
    push (WM_USER + 12) ; TVM_GETITEM
    push g_hFileTree
    call SendMessage

    ; prepend szTmp + '\\' + szAcc into szAcc
    lea eax, szTmp
    lea edx, szAcc
    invoke lstrlen, edx
    mov ecx, eax ; preserve szTmp ptr
    mov ebx, edx ; preserve szAcc ptr
    ; szNew = szTmp + '\\' + szAcc
    lea edx, szNew
    invoke lstrcpy, edx, ecx
    invoke lstrlen, edx
    mov esi, edx
    add esi, eax
    mov BYTE PTR [esi], 92
    mov BYTE PTR [esi+1], 0
    invoke lstrcat, edx, ebx
    ; copy back to accumulator
    lea eax, szAcc
    invoke lstrcpy, eax, edx

    ; move to parent
    push 0
    push 3 ; TVGN_PARENT
    push (WM_USER + 10) ; TVM_GETNEXTITEM
    push g_hFileTree
    call SendMessage
    mov curItem, eax
    jmp @loop

@done:
    ; copy accumulator to output buffer
    lea eax, szAcc
    mov edx, pBuffer
    invoke lstrcpy, edx, eax
    ret
BuildPathForItem endp

; ---------------------------------------------------------------
; FileTree_OnNotify - handles WM_NOTIFY for the file tree
; Input: pNMHDR -> NMHDR*
; ---------------------------------------------------------------
FileTree_OnNotify proc pNMHDR:DWORD
    LOCAL codeVal:DWORD
    LOCAL fromWnd:DWORD
    LOCAL hSel:DWORD

    mov eax, pNMHDR
    assume eax:ptr NMHDR
    mov edx, [eax].code
    mov codeVal, edx
    mov edx, [eax].hwndFrom
    mov fromWnd, edx
    assume eax:nothing

    ; only handle notifications from our tree
    mov eax, fromWnd
    cmp eax, g_hFileTree
    jne @done

    ; on double-click or item expand, populate folder
    ; NM_DBLCLK == -3, TVN_ITEMEXPANDING approx == -405
    cmp codeVal, -3
    jne @checkSel

    ; get current caret item
    push 0
    push 9 ; TVGN_CARET
    push (WM_USER + 10) ; TVM_GETNEXTITEM
    push g_hFileTree
    call SendMessage
    mov hSel, eax
    cmp hSel, 0
    je @done

    ; build path and populate
    lea edx, szSearchPath
    push edx
    push hSel
    call BuildPathForItem
    push edx
    push hSel
    call PopulateDirectory
    jmp @done

@checkSel:
    cmp codeVal, -405
    jne @done
    ; same as double-click: get caret and populate
    push 0
    push 9
    push (WM_USER + 10)
    push g_hFileTree
    call SendMessage
    mov hSel, eax
    cmp hSel, 0
    je @done
    lea edx, szSearchPath
    push edx
    push hSel
    call BuildPathForItem
    push edx
    push hSel
    call PopulateDirectory
    jmp @done

@done:
    ret
FileTree_OnNotify endp

; ---------------------------------------------------------------
; IsItemPopulated - check if item was already populated
; Input: hItem
; Returns: 1 if populated, 0 if not
; ---------------------------------------------------------------
IsItemPopulated proc hItem:DWORD
    LOCAL tvget:FT_TVITEM

    mov tvget.mask_field, TVIF_PARAM
    mov eax, hItem
    mov tvget.hItem_field, eax

    lea eax, tvget
    push eax
    push 0
    push (WM_USER + 12) ; TVM_GETITEM
    push g_hFileTree
    call SendMessage

    mov eax, tvget.lParam_field
    and eax, ITEM_FLAG_POPULATED
    shr eax, 16
    ret
IsItemPopulated endp

; ---------------------------------------------------------------
; MarkItemPopulated - set the populated flag on an item
; Input: hItem
; ---------------------------------------------------------------
MarkItemPopulated proc hItem:DWORD
    LOCAL tvget:FT_TVITEM

    ; First get current lParam
    mov tvget.mask_field, TVIF_PARAM
    mov eax, hItem
    mov tvget.hItem_field, eax

    lea eax, tvget
    push eax
    push 0
    push (WM_USER + 12) ; TVM_GETITEM
    push g_hFileTree
    call SendMessage

    ; Set populated flag
    mov eax, tvget.lParam_field
    or eax, ITEM_FLAG_POPULATED
    mov tvget.lParam_field, eax

    lea eax, tvget
    push eax
    push 0
    push (WM_USER + 13) ; TVM_SETITEM
    push g_hFileTree
    call SendMessage
    ret
MarkItemPopulated endp

; ---------------------------------------------------------------
; PopulateDirectory - populates a directory with its contents
; Parameters: hItem - handle to tree item, pszPath - path to directory
; Checks for lazy-loading flag before populating
; ---------------------------------------------------------------
PopulateDirectory proc hItem:DWORD, pszPath:DWORD
    LOCAL hFind:DWORD

    ; Check if already populated (lazy-loading)
    push hItem
    call IsItemPopulated
    cmp eax, 0
    jne @Done

    ; Mark as populated before we start
    push hItem
    call MarkItemPopulated

    ; Build search path with trailing backslash
    invoke lstrcpy, addr szSearchPath, pszPath
    invoke EnsureTrailingSlash, addr szSearchPath
    invoke lstrcat, addr szSearchPath, addr szFindPattern
    
    ; Find first file
    invoke FindFirstFile, addr szSearchPath, addr findData
    mov hFind, eax
    cmp eax, INVALID_HANDLE_VALUE
    je @Done
    
    @FindLoop:
    ; Skip "." and ".."
    invoke lstrcmp, addr findData.cFileName, addr szDot
    .if eax != 0
        invoke lstrcmp, addr findData.cFileName, addr szDotDot
        .if eax != 0
            ; Determine item type and insert with lParam tagging
            test findData.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY
            lea eax, findData.cFileName
            .if ZERO?
                push ITEM_TYPE_FILE
                push eax
                push hItem
                call AddTreeItem
            .else
                push ITEM_TYPE_FOLDER
                push eax
                push hItem
                call AddTreeItem
            .endif
        .endif
    .endif
    
    ; Find next file
    invoke FindNextFile, hFind, addr findData
    test eax, eax
    jnz @FindLoop
    
    invoke FindClose, hFind
    
@Done:
    ret
PopulateDirectory endp

end