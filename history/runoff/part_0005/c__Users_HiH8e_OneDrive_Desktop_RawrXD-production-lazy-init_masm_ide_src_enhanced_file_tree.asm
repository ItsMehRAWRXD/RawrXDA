; ============================================================================
; RawrXD Agentic IDE - Enhanced File Browser (Pure MASM)
; Full recursive directory tree with all drives and folders
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\shlwapi.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\shlwapi.lib

; File type constants
FILE_TYPE_FOLDER    equ 0
FILE_TYPE_ASM       equ 1
FILE_TYPE_OBJ       equ 2
FILE_TYPE_EXE       equ 3
FILE_TYPE_DLL       equ 4
FILE_TYPE_H         equ 5
FILE_TYPE_TXT       equ 6
FILE_TYPE_OTHER     equ 7
FILE_TYPE_GGUF      equ 8

; External declarations
extrn g_hMainWindow:DWORD
extrn g_hInstance:DWORD

.data
    ; TreeView handle
    public g_hFileTree
    g_hFileTree        dd 0
    
    ; Root items for each drive
    hRootDriveA        dd 0
    hRootDriveB        dd 0
    hRootDriveC        dd 0
    hRootDriveD        dd 0
    hRootDriveE        dd 0
    hRootDriveF        dd 0
    
    ; Search filter
    szSearchFilter     db 256 dup(0)
    
    ; Strings
    szDriveC            db "C:", 0
    szDriveD            db "D:", 0
    szDriveE            db "E:", 0
    szDriveF            db "F:", 0
    szTreeClass         db "SysTreeView32", 0
    szASM               db ".asm", 0
    szOBJ               db ".obj", 0
    szEXE               db ".exe", 0
    szDLL               db ".dll", 0
    szH                 db ".h", 0
    szGGUF              db ".gguf", 0
    
    ; File info structure
    WIN32_FIND_DATA struct
        dwFileAttributes    dd ?
        ftCreationTime      dq ?
        ftLastAccessTime    dq ?
        ftLastWriteTime     dq ?
        nFileSizeHigh       dd ?
        nFileSizeLow        dd ?
        dwReserved0         dd ?
        dwReserved1         dd ?
        cFileName           db 260 dup(?)
        cAlternateFileName  db 14 dup(?)
    WIN32_FIND_DATA ends

.code

public CreateEnhancedFileTree
public RefreshFileTree
public GetFileType
public SearchFiles

; ============================================================================
; CreateEnhancedFileTree - Create enhanced tree view with all drives
; Returns: Tree handle in eax
; ============================================================================
CreateEnhancedFileTree proc
    LOCAL dwStyle:DWORD
    LOCAL hTree:DWORD
    LOCAL hDrive:DWORD
    
    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or TVS_LINESATROOT or TVS_HASLINES or TVS_HASBUTTONS
    
    ; Create TreeView control
    invoke CreateWindowEx, 0,
        offset szTreeClass,
        NULL,
        dwStyle,
        0, 0, 240, 420,
        g_hMainWindow,
        IDC_FILETREE,
        g_hInstance,
        NULL
    mov hTree, eax
    mov g_hFileTree, eax
    test eax, eax
    jz @Exit
    
    ; Add drive letters (C:, D:, E:, F:)
    call EnumerateAllDrives
    
    ; Set tree font
    invoke SendMessage, hTree, WM_SETFONT, g_hMainFont, TRUE
    
    mov eax, hTree
@Exit:
    ret
CreateEnhancedFileTree endp

; ============================================================================
; EnumerateAllDrives - Add all available drives to tree
; ============================================================================
EnumerateAllDrives proc
    LOCAL dwDrives:DWORD
    LOCAL dwMask:DWORD
    LOCAL i:DWORD
    LOCAL szDrivePath[4]:BYTE
    LOCAL tvi:TV_INSERTSTRUCT
    
    ; Get available drives
    invoke GetLogicalDrives
    mov dwDrives, eax
    
    ; Check each drive letter A-Z
    mov i, 0
    mov dwMask, 1
    
    @@DriveLoop:
        cmp i, 26
        jge @DoneWithDrives
        
        ; Test if drive is available
        test eax, dwMask
        jz @SkipDrive
        
        ; Build drive path (A:, B:, etc)
        mov eax, i
        add al, 'A'
        mov szDrivePath, al
        mov byte ptr [szDrivePath+1], ':'
        mov byte ptr [szDrivePath+2], 0
        
        ; Verify drive is accessible
        invoke PathFileExists, addr szDrivePath
        test eax, eax
        jz @SkipDrive
        
        ; Add drive to tree root
        lea eax, tvi
        mov [eax].TV_INSERTSTRUCT.hParent, TVI_ROOT
        mov [eax].TV_INSERTSTRUCT.hInsertAfter, TVI_SORT
        mov [eax].TV_INSERTSTRUCT.itemex.mask, TVIF_TEXT
        lea ecx, szDrivePath
        mov [eax].TV_INSERTSTRUCT.itemex.pszText, ecx
        
        invoke SendMessage, g_hFileTree, TVM_INSERTITEM, 0, addr tvi
        
        ; Store handle for later expansion
        cmp i, 2  ; C:
        jne @NotC
        mov hRootDriveC, eax
        jmp @SkipDrive
        
    @NotC:
        cmp i, 3  ; D:
        jne @NotD
        mov hRootDriveD, eax
        jmp @SkipDrive
        
    @NotD:
        cmp i, 4  ; E:
        jne @SkipDrive
        mov hRootDriveE, eax
        
    @SkipDrive:
        inc i
        shl dwMask, 1
        jmp @DriveLoop
        
    @DoneWithDrives:
        ret
EnumerateAllDrives endp

; ============================================================================
; RefreshFileTree - Refresh entire tree from root
; ============================================================================
public RefreshFileTree
RefreshFileTree proc
    ; Clear tree
    invoke SendMessage, g_hFileTree, TVM_DELETEITEM, 0, TVI_ROOT
    
    ; Re-enumerate drives
    call EnumerateAllDrives
    
    ret
RefreshFileTree endp

; ============================================================================
; GetFileType - Determine file type from extension
; Input: ecx = pointer to filename
; Returns: eax = file type constant
; ============================================================================
GetFileType proc filename:DWORD
    LOCAL pExt:DWORD
    
    ; Get file extension
    invoke PathFindExtension, filename
    mov pExt, eax
    
    ; Compare extensions
    lea ecx, szASM
    invoke lstrcmpi, pExt, ecx
    test eax, eax
    jz @IsASM
    
    lea ecx, szOBJ
    invoke lstrcmpi, pExt, ecx
    test eax, eax
    jz @IsOBJ
    
    lea ecx, szEXE
    invoke lstrcmpi, pExt, ecx
    test eax, eax
    jz @IsEXE
    
    lea ecx, szGGUF
    invoke lstrcmpi, pExt, ecx
    test eax, eax
    jz @IsGGUF
    
    mov eax, FILE_TYPE_OTHER
    ret
    
@IsASM:
    mov eax, FILE_TYPE_ASM
    ret
@IsOBJ:
    mov eax, FILE_TYPE_OBJ
    ret
@IsEXE:
    mov eax, FILE_TYPE_EXE
    ret
@IsGGUF:
    mov eax, FILE_TYPE_GGUF
    ret
GetFileType endp

; ============================================================================
; SearchFiles - Search for files matching pattern
; Input: ecx = search path, edx = search pattern
; Returns: Search result count in eax
; ============================================================================
SearchFiles proc searchPath:DWORD, searchPattern:DWORD
    LOCAL hFind:DWORD
    LOCAL ffd:WIN32_FIND_DATA
    LOCAL dwCount:DWORD
    
    xor dwCount, edx
    
    ; Find first file
    invoke FindFirstFile, searchPattern, addr ffd
    mov hFind, eax
    
    cmp eax, INVALID_HANDLE_VALUE
    je @NoFiles
    
    @@FindLoop:
        ; Process found file
        inc dwCount
        
        ; Find next
        invoke FindNextFile, hFind, addr ffd
        test eax, eax
        jnz @FindLoop
        
    @NoFiles:
        invoke FindClose, hFind
        mov eax, dwCount
        ret
SearchFiles endp

end