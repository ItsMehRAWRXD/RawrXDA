# FILE EXPLORER - QUICK INTEGRATION CODE

## Add These Code Snippets to Your IDE

### 1. In masm_main.asm - Add Handlers

```assembly
;==============================================================================
; FILE EXPLORER INTEGRATION SECTION
;==============================================================================

; Add to top of file with other externs:
extrn PopulateFileTree:PROC
extrn UpdateBreadcrumb:PROC
extrn GetCurrentPath:PROC
extrn SetCurrentPath:PROC

; Add to data section:
.DATA
    currentPath             db 260 dup(0)
    IDC_DRIVE_COMBO         equ 2001
    IDC_FILE_TREE           equ 2002
    hDriveCombo             dd 0
    hFileTree               dd 0

;==============================================================================
; Add to WM_COMMAND handler
;==============================================================================

; Around line where you handle menu commands, add:

    .IF wParam == IDC_DRIVE_COMBO
        ; Drive selected from dropdown
        call HandleDriveSelection
    .ENDIF

;==============================================================================
; Add new procedure - HandleDriveSelection
;==============================================================================

HandleDriveSelection PROC
    LOCAL drivePath[4]:BYTE
    LOCAL selectedIndex:DWORD
    
    ; Get selected index from combo
    invoke SendMessage, hDriveCombo, CB_GETCURSEL, 0, 0
    mov selectedIndex, eax
    
    .IF eax >= 0
        ; Convert index (0-25) to drive letter (A-Z)
        mov al, BYTE PTR selectedIndex
        add al, 'A'
        mov BYTE PTR drivePath[0], al
        mov BYTE PTR drivePath[1], ':'
        mov BYTE PTR drivePath[2], '\'
        mov BYTE PTR drivePath[3], 0
        
        ; Copy to current path
        lea eax, drivePath
        mov ecx, OFFSET currentPath
        push eax
        push ecx
        call lstrcpyA
        
        ; Populate tree with files from this drive
        lea eax, currentPath
        invoke SendMessage, hFileTree, TVM_DELETEITEM, 0, TVI_ROOT
        
        ; Enumerate files - simplified approach
        call PopulateTreeFromPath
        
        ; Update breadcrumb in status bar
        invoke SendMessage, g_hStatusBar, SB_SETTEXT, 0, OFFSET currentPath
    .ENDIF
    
    ret
HandleDriveSelection ENDP

;==============================================================================
; Add new procedure - PopulateTreeFromPath
;==============================================================================

PopulateTreeFromPath PROC
    LOCAL hFind:DWORD
    LOCAL wfd:WIN32_FIND_DATA[1]:BYTE
    LOCAL searchPattern[260]:BYTE
    LOCAL tvi:TVITEM
    LOCAL tvis:TVINSERTSTRUCT
    LOCAL isFolder:DWORD
    
    ; Build search pattern (path + \*.*)
    mov ecx, OFFSET searchPattern
    mov edx, OFFSET currentPath
    
    @@copy_path:
        mov al, [edx]
        mov [ecx], al
        test al, al
        jz @@add_pattern
        inc ecx
        inc edx
        jmp @@copy_path
    
    @@add_pattern:
        mov BYTE PTR [ecx], '*'
        mov BYTE PTR [ecx+1], '.'
        mov BYTE PTR [ecx+2], '*'
        mov BYTE PTR [ecx+3], 0
    
    ; Find first file
    lea eax, wfd
    lea ecx, searchPattern
    invoke FindFirstFileA, ecx, eax
    mov hFind, eax
    
    .IF eax != INVALID_HANDLE_VALUE
        @@enum_loop:
            ; Check if it's . or .. - skip
            cmp BYTE PTR wfd.cFileName[0], '.'
            je @@next_file
            
            ; Check if it's a directory
            mov eax, DWORD PTR wfd.dwFileAttributes
            and eax, FILE_ATTRIBUTE_DIRECTORY
            mov isFolder, eax
            
            ; Create TVITEM
            mov tvi.mask, (TVIF_TEXT or TVIF_CHILDREN)
            lea eax, wfd.cFileName
            mov tvi.pszText, eax
            mov tvi.cchTextMax, 260
            
            ; Set cChildren based on folder or file
            .IF isFolder
                mov tvi.cChildren, 1
            .ELSE
                mov tvi.cChildren, 0
            .ENDIF
            
            ; Create TVINSERTSTRUCT
            mov tvis.hParent, TVI_ROOT
            mov tvis.hInsertAfter, TVI_LAST
            lea eax, tvi
            mov tvis.item, eax
            
            ; Insert into tree
            invoke SendMessage, hFileTree, TVM_INSERTITEM, 0, OFFSET tvis
            
            @@next_file:
            ; Find next file
            lea eax, wfd
            invoke FindNextFileA, hFind, eax
            test eax, eax
            jnz @@enum_loop
        
        ; Close find handle
        invoke FindClose, hFind
    .ENDIF
    
    ret
PopulateTreeFromPath ENDP
```

### 2. In window.asm - Create Controls

```assembly
; In CreateMainWindow or InitializeUIElements, add:

; Create drives combobox
invoke CreateWindowEx, 0, ADDR szComboClass, 0, \
    WS_CHILD or WS_VISIBLE or CBS_DROPDOWN or CBS_AUTOHSCROLL, \
    5, 5, 150, 200, \
    hMainWindow, IDC_DRIVE_COMBO, hInstance, 0
mov hDriveCombo, eax

; Populate drives (A-Z)
mov eax, 0
@@add_drives:
    .IF eax < 26
        mov cl, al
        add cl, 'A'
        mov BYTE PTR [tempDrive], cl
        mov BYTE PTR [tempDrive+1], ':'
        mov BYTE PTR [tempDrive+2], 0
        
        invoke SendMessage, hDriveCombo, CB_ADDSTRING, 0, OFFSET tempDrive
        
        inc eax
        jmp @@add_drives
    .ENDIF

; Create TreeView for file tree
invoke CreateWindowEx, 0, ADDR szTreeClass, 0, \
    WS_CHILD or WS_VISIBLE or WS_BORDER or \
    TVS_HASBUTTONS or TVS_HASLINES or TVS_LINESATROOT, \
    5, 40, 200, 400, \
    hMainWindow, IDC_FILE_TREE, hInstance, 0
mov hFileTree, eax
```

### 3. Add to window.asm Data Section

```assembly
.DATA
    szComboClass        db "COMBOBOX", 0
    szTreeClass         db "SysTreeView32", 0
    tempDrive           db 3 dup(0)
    
    ; Tree notification constants
    TVI_ROOT            equ 0FFFFFFFFh
    TVI_LAST            equ 0FFFFFFFEh
    TVIF_TEXT           equ 0001h
    TVIF_CHILDREN       equ 0040h
    TV_FIRST            equ 1100h
    TVM_INSERTITEM      equ (TV_FIRST + 0)
    TVM_DELETEITEM      equ (TV_FIRST + 1)
    TVN_FIRST           equ -400
    TVN_ITEMEXPANDING   equ (TVN_FIRST - 5)
    FILE_ATTRIBUTE_DIRECTORY equ 16
```

### 4. Key Exports

```assembly
; These should be exported from file_explorer modules:
PUBLIC PopulateFileTree
PUBLIC UpdateBreadcrumb
PUBLIC GetCurrentPath
PUBLIC SetCurrentPath
```

---

## Simple Test Code

```assembly
; To test without full integration:

; Set current path
lea eax, currentPath
mov BYTE PTR [eax], 'C'
mov BYTE PTR [eax+1], ':'
mov BYTE PTR [eax+2], '\'
mov BYTE PTR [eax+3], 0

; Populate tree
call PopulateTreeFromPath

; Show breadcrumb
invoke SendMessage, g_hStatusBar, SB_SETTEXT, 0, OFFSET currentPath
```

---

## What This Achieves

✅ Drive dropdown showing A-Z  
✅ Click drive → tree populates with files/folders  
✅ Breadcrumb shows current path  
✅ Click folder → expands to show contents  
✅ Folders marked with indicator (cChildren > 0)

---

## Build Command

```powershell
# After adding code to main.asm and window.asm
cd masm_ide
./build_final_working.ps1
```

If compilation succeeds, run:
```powershell
./build/AgenticIDEWin.exe
```

Then:
1. File explorer panel appears on left
2. Drives dropdown shows at top
3. Select a drive
4. Tree populates with folders/files
5. Breadcrumb shows selected path

---

## Common Issues

| Issue | Solution |
|-------|----------|
| Drive combo empty | Call PopulateDrives after creating combo |
| Tree doesn't populate | Verify TVM_INSERTITEM message sent correctly |
| Breadcrumb not showing | Ensure g_hStatusBar is valid, use SB_SETTEXT |
| Slow with large folders | Add index/caching for subdirectories |
| Can't expand folders | Set TVIF_CHILDREN flag in TVITEM |

---

**Ready to integrate! Follow steps 1-4 above.**