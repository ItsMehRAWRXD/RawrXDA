; ============================================================================
; RawrXD IDE - Phase 2: File Operations System (Simplified)
; Basic file dialogs and recent files tracking
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comdlg32.inc
include \masm32\include\shell32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comdlg32.lib
includelib \masm32\lib\shell32.lib

; ============================================================================
; CONSTANTS
; ============================================================================

MAX_RECENT_FILES    equ 10
MAX_PATH_LENGTH     equ 260

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    public recentFiles
    public recentFilesCount
    public currentFilePath
    public currentFileName
    
    recentFiles         db MAX_RECENT_FILES * MAX_PATH_LENGTH dup(0)
    recentFilesCount    dd 0
    
    currentFilePath     db MAX_PATH_LENGTH dup(0)
    currentFileName     db 64 dup(0)
    
    fileNameBuffer      db MAX_PATH_LENGTH dup(0)
    
    ; Drop files buffer
    dropFilePath        db MAX_PATH_LENGTH dup(0)
    
    ; File filters
    asmFilter          db "Assembly Files (*.asm)", 0, "*.asm", 0
                       db "All Files (*.*)", 0, "*.*", 0, 0
    
    ; String constants
    szNewFile           db "New File", 0
    szFileSaved         db "File saved", 0
    szFileContent       db "File content placeholder", 0

; ============================================================================
; EXTERNAL DECLARATIONS
; ============================================================================

extrn g_hMainWindow:DWORD
extrn g_hStatusBar:DWORD

; ============================================================================
; CODE SECTION
; ============================================================================

.code

public InitializeFileOperations
public FileNew
public FileOpen
public FileSave
public FileSaveAs
public CleanupFileOperations

; ============================================================================
; InitializeFileOperations - Setup file operations system
; ============================================================================
InitializeFileOperations proc
    ; Enable drag & drop
    invoke DragAcceptFiles, g_hMainWindow, TRUE
    ret
InitializeFileOperations endp

; ============================================================================
; FileNew - Create new file
; ============================================================================
FileNew proc
    ; Clear current file info
    invoke RtlZeroMemory, offset currentFilePath, MAX_PATH_LENGTH
    invoke RtlZeroMemory, offset currentFileName, 64
    
    ; Update status bar
    invoke SetWindowText, g_hStatusBar, offset szNewFile
    
    ret
FileNew endp

; ============================================================================
; FileOpen - Open file dialog
; ============================================================================
FileOpen proc hWnd:DWORD
    LOCAL ofn:OPENFILENAME
    LOCAL result:BOOL
    
    ; Initialize OPENFILENAME structure
    mov ofn.lStructSize, sizeof OPENFILENAME
    mov eax, hWnd
    mov ofn.hwndOwner, eax
    mov ofn.hInstance, NULL
    mov ofn.lpstrFilter, offset asmFilter
    mov ofn.lpstrFile, offset fileNameBuffer
    mov ofn.nMaxFile, MAX_PATH_LENGTH
    mov ofn.Flags, OFN_FILEMUSTEXIST or OFN_PATHMUSTEXIST
    
    ; Clear filename buffer
    invoke RtlZeroMemory, offset fileNameBuffer, MAX_PATH_LENGTH
    
    ; Show open dialog
    invoke GetOpenFileName, addr ofn
    mov result, eax
    
    ; Process selected file
    .if result == TRUE
        call ProcessSelectedFile
    .endif
    
    mov eax, result
    ret
FileOpen endp

; ============================================================================
; ProcessSelectedFile - Handle file selection
; ============================================================================
ProcessSelectedFile proc
    ; Update current file info
    invoke lstrcpy, offset currentFilePath, offset fileNameBuffer
    
    ; Extract filename from path
    call ExtractFileNameFromPath
    
    ; Add to recent files
    call AddToRecentFiles
    
    ; Update status bar
    invoke SetWindowText, g_hStatusBar, offset currentFilePath
    
    ret
ProcessSelectedFile endp

; ============================================================================
; ExtractFileNameFromPath - Get filename from full path
; ============================================================================
ExtractFileNameFromPath proc
    LOCAL pFileName:DWORD
    
    ; Find last backslash
    invoke lstrlen, offset currentFilePath
    mov ecx, eax
    lea esi, currentFilePath
    add esi, ecx
    
    ; Search backwards for backslash
    @@SearchLoop:
        cmp ecx, 0
        jle @NotFound
        
        cmp byte ptr [esi], '\'
        je @FoundBackslash
        
        dec esi
        dec ecx
        jmp @@SearchLoop
    
    @FoundBackslash:
        inc esi
        mov pFileName, esi
        jmp @Found
    
    @NotFound:
        mov pFileName, offset currentFilePath
    
    @Found:
        ; Copy filename
        invoke lstrcpy, offset currentFileName, pFileName
        ret
ExtractFileNameFromPath endp

; ============================================================================
; AddToRecentFiles - Add file to recent files list
; ============================================================================
AddToRecentFiles proc
    LOCAL i:DWORD
    LOCAL found:BOOL
    
    mov found, 0
    
    ; Check if file already exists in recent files
    mov i, 0
    @@CheckLoop:
        cmp i, recentFilesCount
        jge @CheckDone
        
        ; Calculate offset: i * MAX_PATH_LENGTH
        mov eax, i
        imul eax, eax, MAX_PATH_LENGTH
        mov edx, offset recentFiles
        add edx, eax
        invoke lstrcmp, edx, offset currentFilePath
        .if eax == 0
            mov found, 1
            jmp @CheckDone
        .endif
        
        inc i
        jmp @@CheckLoop
    
    @CheckDone:
    .if found == 0
        ; Shift all entries down by one
        mov i, recentFilesCount
        .if i >= MAX_RECENT_FILES
            mov i, MAX_RECENT_FILES - 1
        .endif
        
        @@ShiftLoop:
            cmp i, 0
            jle @ShiftDone
            
            ; Calculate source and destination
            mov eax, i
            dec eax
            imul eax, eax, MAX_PATH_LENGTH
            mov esi, offset recentFiles
            add esi, eax
            
            mov eax, i
            imul eax, eax, MAX_PATH_LENGTH
            mov edi, offset recentFiles
            add edi, eax
            
            ; Copy entry
            invoke RtlMoveMemory, edi, esi, MAX_PATH_LENGTH
            
            dec i
            jmp @@ShiftLoop
        
        @ShiftDone:
        ; Add new entry at top
        invoke lstrcpy, offset recentFiles, offset currentFilePath
        
        ; Increment count if not full
        .if recentFilesCount < MAX_RECENT_FILES
            inc recentFilesCount
        .endif
    .endif
    
    ret
AddToRecentFiles endp

; ============================================================================
; FileSave - Save current file
; ============================================================================
FileSave proc
    LOCAL hFile:DWORD
    LOCAL dwBytesWritten:DWORD
    
    ; Check if we have a file path
    invoke lstrlen, offset currentFilePath
    .if eax == 0
        ; No file path, show Save As dialog
        call FileSaveAs
        ret
    .endif
    
    ; Open file for writing
    invoke CreateFile, offset currentFilePath, GENERIC_WRITE, 
                      0, NULL, CREATE_ALWAYS, 
                      FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    
    .if hFile != INVALID_HANDLE_VALUE
        ; Write placeholder content
        invoke WriteFile, hFile, offset szFileContent, 
                         sizeof szFileContent - 1, addr dwBytesWritten, NULL
        
        ; Close file
        invoke CloseHandle, hFile
        
        ; Update status
        invoke SetWindowText, g_hStatusBar, offset szFileSaved
    .endif
    
    ret
FileSave endp

; ============================================================================
; FileSaveAs - Save with dialog
; ============================================================================
FileSaveAs proc
    LOCAL ofn:OPENFILENAME
    LOCAL result:BOOL
    
    ; Initialize OPENFILENAME structure
    mov ofn.lStructSize, sizeof OPENFILENAME
    mov eax, g_hMainWindow
    mov ofn.hwndOwner, eax
    mov ofn.hInstance, NULL
    mov ofn.lpstrFilter, offset asmFilter
    mov ofn.lpstrFile, offset fileNameBuffer
    mov ofn.nMaxFile, MAX_PATH_LENGTH
    mov ofn.Flags, OFN_OVERWRITEPROMPT or OFN_PATHMUSTEXIST
    
    ; Clear filename buffer
    invoke RtlZeroMemory, offset fileNameBuffer, MAX_PATH_LENGTH
    
    ; Show save dialog
    invoke GetSaveFileName, addr ofn
    mov result, eax
    
    .if result == TRUE
        ; Update current file info
        invoke lstrcpy, offset currentFilePath, offset fileNameBuffer
        call ExtractFileNameFromPath
        call AddToRecentFiles
        call FileSave
    .endif
    
    mov eax, result
    ret
FileSaveAs endp

; ============================================================================
; CleanupFileOperations - Release resources
; ============================================================================
CleanupFileOperations proc
    ; Unregister drag & drop
    invoke DragAcceptFiles, g_hMainWindow, FALSE
    ret
CleanupFileOperations endp

; ============================================================================
; HandleDropFiles - Process dropped files
; ============================================================================
public HandleDropFiles
HandleDropFiles proc hDrop:HDROP
    LOCAL fileCount:DWORD
    LOCAL i:DWORD
    
    ; Get number of dropped files
    invoke DragQueryFile, hDrop, 0FFFFFFFFh, NULL, 0
    mov fileCount, eax
    
    ; Process each file
    mov i, 0
    @@ProcessLoop:
        cmp i, fileCount
        jge @ProcessDone
        
        ; Get file path
        invoke DragQueryFile, hDrop, i, offset dropFilePath, MAX_PATH_LENGTH
        
        ; Process the file
        invoke lstrcpy, offset currentFilePath, offset dropFilePath
        call ExtractFileNameFromPath
        call AddToRecentFiles
        
        ; Update status bar
        invoke SetWindowText, g_hStatusBar, offset currentFilePath
        
        inc i
        jmp @@ProcessLoop
    
    @ProcessDone:
    ; Release drop handle
    invoke DragFinish, hDrop
    
    ret
HandleDropFiles endp

end