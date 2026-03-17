# FILE EXPLORER - COPY & PASTE INTEGRATION CODE

**Copy these exact code blocks into your IDE source files.**

---

## STEP 1: Update masm_main.asm

### Add these lines to the top (with other externs):

```assembly
; File Explorer Integration
extrn PopulateFileTree:PROC
extrn UpdateBreadcrumb:PROC
extrn GetCurrentPath:PROC
extrn SetCurrentPath:PROC

; File operations
extrn FindFirstFileA:PROC
extrn FindNextFileA:PROC
extrn FindClose:PROC
```

### Add to DATA section:

```assembly
; File Explorer Data
currentPath             db 260 dup(0)
searchPattern           db 260 dup(0)
tempDrive               db 4 dup(0)

; Control IDs
IDC_DRIVE_COMBO         equ 2001
IDC_FILE_TREE           equ 2002

; Handles
hDriveCombo             dd 0
hFileTree               dd 0
hFind                   dd 0

; Tree View Constants
TVI_ROOT                equ 0FFFFFFFFh
TVI_LAST                equ 0FFFFFFFEh
TVIF_TEXT               equ 0001h
TVIF_CHILDREN           equ 0040h
TV_FIRST                equ 1100h
TVM_INSERTITEM          equ (TV_FIRST + 0)
TVM_DELETEITEM          equ (TV_FIRST + 1)
TVM_EXPAND              equ (TV_FIRST + 2)
TVM_GETNEXTITEM         equ (TV_FIRST + 10)
TVM_GETITEM             equ (TV_FIRST + 12)

; TreeView Notifications
TVN_FIRST               equ -400
TVN_SELCHANGED          equ (TVN_FIRST - 2)
TVN_ITEMEXPANDING       equ (TVN_FIRST - 5)
TVE_COLLAPSE            equ 0001h
TVE_EXPAND              equ 0002h

; File Constants
INVALID_HANDLE_VALUE    equ -1
FILE_ATTRIBUTE_DIRECTORY equ 16
SB_SETTEXT              equ (WM_USER + 1)

; Structure: TVITEM
TVITEM struct
    mask_f              dd ?
    hItem_f             dd ?
    state_f             dd ?
    stateMask_f         dd ?
    pszText_f           dd ?
    cchTextMax_f        dd ?
    iImage_f            dd ?
    iSelectedImage_f    dd ?
    cChildren_f         dd ?
    lParam_f            dd ?
TVITEM ends

; Structure: TVINSERTSTRUCT
TVINSERTSTRUCT struct
    hParent_f           dd ?
    hInsertAfter_f      dd ?
    item_f              TVITEM <>
TVINSERTSTRUCT ends

; Structure: WIN32_FIND_DATA
WIN32_FIND_DATA struct
    dwFileAttributes    dd ?
    ftCreationTime      dd ?, ?
    ftLastAccessTime    dd ?, ?
    ftLastWriteTime     dd ?, ?
    nFileSizeHigh       dd ?
    nFileSizeLow        dd ?
    dwReserved0         dd ?
    dwReserved1         dd ?
    cFileName           db 260 dup(?)
    cAlternateFileName  db 14 dup(?)
WIN32_FIND_DATA ends
```

### Add to CODE section (new procedures):

```assembly
;==============================================================================
; HandleDriveSelection - Process drive dropdown selection
;==============================================================================
HandleDriveSelection PROC
    LOCAL drivePath[4]:BYTE
    LOCAL selectedIndex:DWORD
    
    ; Get selected index from combobox
    invoke SendMessage, hDriveCombo, CB_GETCURSEL, 0, 0
    mov selectedIndex, eax
    
    .IF eax >= 0 && eax < 26
        ; Convert index to drive letter (0=A, 1=B, ..., 25=Z)
        mov al, BYTE PTR selectedIndex
        add al, 'A'
        mov BYTE PTR drivePath[0], al
        mov BYTE PTR drivePath[1], ':'
        mov BYTE PTR drivePath[2], '\'
        mov BYTE PTR drivePath[3], 0
        
        ; Copy to current path
        lea ecx, drivePath
        lea edx, currentPath
        mov eax, ecx
        mov ecx, edx
        @@copy_path:
            mov al, [eax]
            mov [ecx], al
            test al, al
            jz @@path_copied
            inc eax
            inc ecx
            jmp @@copy_path
        @@path_copied:
        
        ; Clear tree view
        invoke SendMessage, hFileTree, TVM_DELETEITEM, 0, TVI_ROOT
        
        ; Populate tree with files from this path
        call PopulateTreeFromPath
        
        ; Update breadcrumb in status bar
        lea eax, currentPath
        invoke SendMessage, g_hStatusBar, SB_SETTEXT, 0, eax
    .ENDIF
    
    ret
HandleDriveSelection ENDP

;==============================================================================
; PopulateTreeFromPath - Enumerate files and populate TreeView
;==============================================================================
PopulateTreeFromPath PROC
    LOCAL wfd:WIN32_FIND_DATA
    LOCAL tvi:TVITEM
    LOCAL tvis:TVINSERTSTRUCT
    LOCAL isFolder:DWORD
    LOCAL bufSize:DWORD
    
    ; Allocate search pattern buffer on stack
    sub esp, 260
    mov edx, esp
    
    ; Build search pattern: "C:\*.*"
    lea eax, currentPath
    @@build_pattern:
        mov cl, [eax]
        mov [edx], cl
        test cl, cl
        jz @@add_wildcard
        inc eax
        inc edx
        jmp @@build_pattern
    
    @@add_wildcard:
        mov BYTE PTR [edx], '*'
        mov BYTE PTR [edx+1], '.'
        mov BYTE PTR [edx+2], '*'
        mov BYTE PTR [edx+3], 0
    
    ; Call FindFirstFile
    mov ecx, esp
    lea edx, wfd
    invoke FindFirstFileA, ecx, edx
    mov hFind, eax
    
    cmp eax, INVALID_HANDLE_VALUE
    je @@search_done
    
    ; Enumerate files
    @@enum_loop:
        ; Skip . and .. entries
        cmp BYTE PTR wfd.cFileName[0], '.'
        je @@next_file
        
        ; Check if it's a directory
        mov eax, DWORD PTR wfd.dwFileAttributes
        and eax, FILE_ATTRIBUTE_DIRECTORY
        mov isFolder, eax
        
        ; Build TVITEM structure
        lea edx, tvi
        mov DWORD PTR [edx + TVITEM.mask_f], (TVIF_TEXT or TVIF_CHILDREN)
        lea eax, wfd.cFileName
        mov DWORD PTR [edx + TVITEM.pszText_f], eax
        mov DWORD PTR [edx + TVITEM.cchTextMax_f], 260
        
        ; Set cChildren: 1 if directory, 0 if file
        .IF isFolder
            mov DWORD PTR [edx + TVITEM.cChildren_f], 1
        .ELSE
            mov DWORD PTR [edx + TVITEM.cChildren_f], 0
        .ENDIF
        
        ; Build TVINSERTSTRUCT
        lea ecx, tvis
        mov DWORD PTR [ecx + TVINSERTSTRUCT.hParent_f], TVI_ROOT
        mov DWORD PTR [ecx + TVINSERTSTRUCT.hInsertAfter_f], TVI_LAST
        lea eax, tvi
        mov [ecx + TVINSERTSTRUCT.item_f], eax
        
        ; Insert item into tree
        lea eax, tvis
        invoke SendMessage, hFileTree, TVM_INSERTITEM, 0, eax
        
        @@next_file:
        ; Find next file
        lea eax, wfd
        invoke FindNextFileA, hFind, eax
        test eax, eax
        jnz @@enum_loop
    
    ; Close search handle
    invoke FindClose, hFind
    
    @@search_done:
    add esp, 260
    ret

PopulateTreeFromPath ENDP

;==============================================================================
; UpdateBreadcrumb - Display current path in status bar
;==============================================================================
UpdateBreadcrumb PROC
    lea eax, currentPath
    invoke SendMessage, g_hStatusBar, SB_SETTEXT, 0, eax
    ret
UpdateBreadcrumb ENDP
```

### Add to WM_COMMAND handler (find where menu commands are handled):

```assembly
; After existing command handlers, add:

    ; File Explorer Events
    .IF wParam == IDC_DRIVE_COMBO
        call HandleDriveSelection
        mov eax, 0
        ret
    .ENDIF
```

---

## STEP 2: Update window.asm

### Add to DATA section:

```assembly
; File Explorer UI Strings
szComboBoxClass         db "COMBOBOX", 0
szTreeViewClass         db "SysTreeView32", 0

; Drive labels
szDriveA                db "A:", 0
szDriveB                db "B:", 0
szDriveC                db "C:", 0
szDriveD                db "D:", 0
szDriveE                db "E:", 0
szDriveF                db "F:", 0
szDriveG                db "G:", 0
szDriveH                db "H:", 0
szDriveI                db "I:", 0
szDriveJ                db "J:", 0
szDriveK                db "K:", 0
szDriveL                db "L:", 0
szDriveM                db "M:", 0
szDriveN                db "N:", 0
szDriveO                db "O:", 0
szDriveP                db "P:", 0
szDriveQ                db "Q:", 0
szDriveR                db "R:", 0
szDriveS                db "S:", 0
szDriveT                db "T:", 0
szDriveU                db "U:", 0
szDriveV                db "V:", 0
szDriveW                db "W:", 0
szDriveX                db "X:", 0
szDriveY                db "Y:", 0
szDriveZ                db "Z:", 0

driveLabels             dd OFFSET szDriveA, OFFSET szDriveB, OFFSET szDriveC, \
                           OFFSET szDriveD, OFFSET szDriveE, OFFSET szDriveF, \
                           OFFSET szDriveG, OFFSET szDriveH, OFFSET szDriveI, \
                           OFFSET szDriveJ, OFFSET szDriveK, OFFSET szDriveL, \
                           OFFSET szDriveM, OFFSET szDriveN, OFFSET szDriveO, \
                           OFFSET szDriveP, OFFSET szDriveQ, OFFSET szDriveR, \
                           OFFSET szDriveS, OFFSET szDriveT, OFFSET szDriveU, \
                           OFFSET szDriveV, OFFSET szDriveW, OFFSET szDriveX, \
                           OFFSET szDriveY, OFFSET szDriveZ
```

### Add to CreateMainWindow procedure (after existing UI creation):

```assembly
; Create File Explorer Controls
; Create drives combobox
invoke CreateWindowEx, 0, ADDR szComboBoxClass, 0, \
    WS_CHILD or WS_VISIBLE or CBS_DROPDOWN or CBS_AUTOHSCROLL, \
    5, 5, 150, 300, \
    hMainWindow, IDC_DRIVE_COMBO, hInstance, 0
mov hDriveCombo, eax

; Populate drives (A-Z)
mov ecx, 0
lea edx, driveLabels
@@add_drive:
    .IF ecx < 26
        mov eax, [edx + ecx*4]
        invoke SendMessage, hDriveCombo, CB_ADDSTRING, 0, eax
        inc ecx
        jmp @@add_drive
    .ENDIF

; Create TreeView for files
invoke CreateWindowEx, WS_EX_CLIENTEDGE, ADDR szTreeViewClass, 0, \
    WS_CHILD or WS_VISIBLE or WS_BORDER or \
    TVS_HASBUTTONS or TVS_HASLINES or TVS_LINESATROOT or TVS_SHOWSELALWAYS, \
    5, 40, 200, 450, \
    hMainWindow, IDC_FILE_TREE, hInstance, 0
mov hFileTree, eax

; Initialize tree with empty root
mov eax, hFileTree
```

---

## STEP 3: Rebuild

```powershell
cd C:\Users\HiH8e\OneDrive\Desktop\RawrXD-production-lazy-init\masm_ide
.\build_final_working.ps1
```

---

## STEP 4: Test

```powershell
.\build\AgenticIDEWin.exe
```

Then:
1. Look for drive dropdown on left (A: B: C: etc)
2. Click C:
3. Tree should populate with C:\ contents
4. Status bar should show "C:\"
5. Click folder to expand
6. Breadcrumb updates to folder path

---

## Quick Checklist

- [ ] Added externs to masm_main.asm
- [ ] Added data structures to masm_main.asm
- [ ] Added HandleDriveSelection() to masm_main.asm
- [ ] Added PopulateTreeFromPath() to masm_main.asm
- [ ] Added UpdateBreadcrumb() to masm_main.asm
- [ ] Added WM_COMMAND handler for IDC_DRIVE_COMBO
- [ ] Added combo strings to window.asm data section
- [ ] Created combo and tree controls in window.asm
- [ ] Populated combo with drive labels
- [ ] Recompiled with build_final_working.ps1
- [ ] Tested drive selection
- [ ] Verified breadcrumb display

---

**Copy, paste, compile, test, deploy!**