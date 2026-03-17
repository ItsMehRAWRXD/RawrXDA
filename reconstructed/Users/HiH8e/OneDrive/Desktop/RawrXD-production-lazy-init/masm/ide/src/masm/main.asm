; ============================================================================
; RawrXD Agentic IDE - Pure MASM Entry Point
; Converted from C++ masm_main.cpp to pure MASM
; ============================================================================
.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; External declarations for engine functions and globals
Engine_Initialize proto :DWORD
Engine_Run proto

extrn g_hInstance:DWORD
extrn g_hMainWindow:DWORD
extrn g_hMainFont:DWORD
extrn g_hStatusBar:DWORD

.data
include constants.inc
include structures.inc
include macros.inc

szStartMsg db "RawrXD Win32 MASM IDE - Starting...", 13, 10, 0
szFailMsg  db "Failed to initialize engine", 13, 10, 0

;==============================================================================
; FILE EXPLORER DATA
;==============================================================================

currentPath             db 260 dup(0)
searchPattern           db 260 dup(0)
tempDrive               db 4 dup(0)

; Simple TVINSERTSTRUCT definition
TVINSERTSTRUCT struct
    hParent             dd ?
    hInsertAfter        dd ?
    item                dd ?
TVINSERTSTRUCT ends

; Custom TVITEM definition to avoid conflicts
MY_TV_ITEM struct
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
MY_TV_ITEM ends

; Control IDs
IDC_DRIVE_COMBO         equ 2001
IDC_FILE_TREE           equ 2002

; Handles
hDriveCombo             dd 0
hFileTree               dd 0
hFind                   dd 0

; Tree View Constants (already in structures.inc - don't redefine)
; TVI_ROOT                equ 0FFFFFFFFh
; TVI_LAST                equ 0FFFFFFFEh
TVIF_TEXT               equ 0001h
TVIF_CHILDREN           equ 0040h
TV_FIRST                equ 1100h
TVM_INSERTITEM          equ (TV_FIRST + 0)
TVM_DELETEITEM          equ (TV_FIRST + 1)

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

; ============================================================================
; WinMain - Entry point (converted from C++)
; ============================================================================

.code
WinMain proc hInstance:DWORD, hPrevInstance:DWORD, lpCmdLine:DWORD, nCmdShow:DWORD
    LOCAL bInitialized:DWORD
    
    ; Output startup message (equivalent to std::cout)
    push 0                     ; lpReserved
    push 0                     ; lpNumberOfCharsWritten
    push sizeof szStartMsg - 1 ; nNumberOfCharsToWrite
    push offset szStartMsg     ; lpBuffer
    push STD_OUTPUT_HANDLE
    call GetStdHandle
    push eax                   ; hConsoleOutput
    call WriteConsoleA
    
    ; Initialize the engine (equivalent to engine.initialize(hInstance))
    invoke Engine_Initialize, hInstance
    mov bInitialized, eax
    
    .if bInitialized == 0
        ; Output error message (equivalent to std::cerr)
        push 0                     ; lpReserved
        push 0                     ; lpNumberOfCharsWritten
        push sizeof szFailMsg - 1  ; nNumberOfCharsToWrite
        push offset szFailMsg      ; lpBuffer
        push STD_ERROR_HANDLE
        call GetStdHandle
        push eax                   ; hConsoleOutput
        call WriteConsoleA
        mov eax, 1  ; Return error code
        ret
    .endif
    
    ; Run the main message loop (equivalent to engine.run())
    call Engine_Run
    mov eax, eax  ; Return the result
    
    ret
WinMain endp

;==============================================================================
; FILE EXPLORER STUBS
;==============================================================================

PUBLIC HandleDriveSelection
PUBLIC hDriveCombo
PUBLIC hFileTree

;==============================================================================
; HandleDriveSelection - Full implementation with error handling
;==============================================================================
HandleDriveSelection PROC
    LOCAL drivePath[4]:BYTE
    LOCAL selectedIndex:DWORD
    LOCAL success:DWORD
    
    ; Get selected index from combobox
    invoke SendMessageA, hDriveCombo, CB_GETCURSEL, 0, 0
    mov selectedIndex, eax
    
    ; Validate index
    .IF eax < 0 || eax >= 26
        ; Invalid drive index - just return
        mov eax, FALSE
        ret
    .ENDIF
    
    ; Convert index to drive letter (0=A, 1=B, ..., 25=Z)
    mov al, BYTE PTR selectedIndex
    add al, 'A'
    mov BYTE PTR drivePath[0], al
    mov BYTE PTR drivePath[1], ':'
    mov BYTE PTR drivePath[2], '\'
    mov BYTE PTR drivePath[3], 0
    
    ; Update current path
    lea eax, drivePath
    lea edx, currentPath
    @@copy_path:
        mov al, [eax]
        mov [edx], al
        test al, al
        jz @@path_copied
        inc eax
        inc edx
        jmp @@copy_path
    @@path_copied:
    
    ; Clear tree view
    invoke SendMessageA, hFileTree, TVM_DELETEITEM, 0, TVI_ROOT
    
    ; Populate tree with files from this path
    call PopulateTreeFromPath
    
    ; Update breadcrumb in status bar
    lea eax, currentPath
    invoke SendMessageA, g_hStatusBar, SB_SETTEXT, 0, eax
    
    mov eax, TRUE
    ret
HandleDriveSelection ENDP

; Provide alias for window.asm calling convention
PUBLIC _HandleDriveSelection
_HandleDriveSelection PROC C
    call HandleDriveSelection
    ret
_HandleDriveSelection ENDP

PUBLIC __HandleDriveSelection
__HandleDriveSelection PROC C
    call HandleDriveSelection
    ret
__HandleDriveSelection ENDP

;==============================================================================
; PopulateTreeFromPath - Enumerate files and populate TreeView
;==============================================================================
PopulateTreeFromPath PROC
    LOCAL wfd:WIN32_FIND_DATA
    LOCAL tvi:MY_TV_ITEM
    LOCAL tvis:TVINSERTSTRUCT
    LOCAL isFolder:DWORD
    
    ; Build search pattern: "C:\*.*"
    call BuildSearchPattern
    
    ; Find first file
    lea eax, wfd
    lea ecx, searchPattern
    invoke FindFirstFileA, ecx, eax
    mov hFind, eax
    
    .IF eax == INVALID_HANDLE_VALUE
        ; No files found - this is valid (empty directory)
        mov eax, TRUE
        ret
    .ENDIF
    
    ; Enumerate files
    @@enum_loop:
        ; Skip . and .. entries
        cmp BYTE PTR wfd.cFileName[0], '.'
        je @@next_file
        
        ; Check if it's a directory
        mov eax, DWORD PTR wfd.dwFileAttributes
        and eax, FILE_ATTRIBUTE_DIRECTORY
        mov isFolder, eax
        
        ; Build MY_TV_ITEM structure
        mov [tvi].mask_f, (TVIF_TEXT or TVIF_CHILDREN)
        lea eax, wfd.cFileName
        mov [tvi].pszText_f, eax
        mov [tvi].cchTextMax_f, 260
        
        ; Set cChildren: 1 if directory, 0 if file
        .IF isFolder
            mov [tvi].cChildren_f, 1
        .ELSE
            mov [tvi].cChildren_f, 0
        .ENDIF
        
        ; Build TVINSERTSTRUCT using simple structure
        mov [tvis].hParent, TVI_ROOT
        mov [tvis].hInsertAfter, TVI_LAST
        lea eax, tvi
        mov [tvis].item, eax
        
        ; Insert item into tree
        lea eax, tvis
        invoke SendMessageA, hFileTree, TVM_INSERTITEM, 0, eax
        
        @@next_file:
        ; Find next file
        lea eax, wfd
        invoke FindNextFileA, hFind, eax
        test eax, eax
        jnz @@enum_loop
    
    ; Close search handle
    invoke FindClose, hFind
    
    mov eax, TRUE
    ret

PopulateTreeFromPath ENDP;==============================================================================
; BuildSearchPattern - Build "C:\*.*" pattern from current path
;==============================================================================
BuildSearchPattern PROC
    LOCAL i:DWORD
    
    mov i, 0
    lea ecx, currentPath
    lea edx, searchPattern
    
    @@copy_path:
        mov al, [ecx]
        mov [edx], al
        test al, al
        jz @@add_wildcard
        inc ecx
        inc edx
        jmp @@copy_path
    
    @@add_wildcard:
        mov BYTE PTR [edx], 92  ; Backslash character
        inc edx
        mov BYTE PTR [edx], '*'
        inc edx
        mov BYTE PTR [edx], '.'
        inc edx
        mov BYTE PTR [edx], '*'
        inc edx
        mov BYTE PTR [edx], 0
    
    ret

BuildSearchPattern ENDP

end WinMain