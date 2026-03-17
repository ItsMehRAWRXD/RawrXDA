; ============================================================================
; RawrXD IDE - Phase 2: File Operations System
; Professional file dialogs, recent files tracking, drag & drop support
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

MAX_RECENT_FILES    equ 20
MAX_PATH_LENGTH     equ 260

FILE_OP_NEW         equ 1
FILE_OP_OPEN        equ 2
FILE_OP_SAVE        equ 3
FILE_OP_SAVEAS      equ 4

; ============================================================================
; STRUCTURES
; ============================================================================

RECENT_FILE_ENTRY struct
    filePath        db MAX_PATH_LENGTH dup(?)
    fileName        db 64 dup(?)
    lastAccessed    FILETIME <>
    fileSize        dd ?
RECENT_FILE_ENTRY ends

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    public recentFiles
    public recentFilesCount
    public currentFilePath
    public currentFileName
    public isFileModified
    
    recentFiles         RECENT_FILE_ENTRY MAX_RECENT_FILES dup(<>)
    recentFilesCount    dd 0
    
    currentFilePath     db MAX_PATH_LENGTH dup(0)
    currentFileName     db 64 dup(0)
    isFileModified      db 0
    
    fileNameBuffer      db MAX_PATH_LENGTH dup(0)
    
    ; File filters
    asmFilter          db "Assembly Files (*.asm)", 0, "*.asm", 0
                       db "All Files (*.*)", 0, "*.*", 0, 0
    
    ; Registry keys
    recentFilesKey     db "Software\RawrXD\RecentFiles", 0

; ============================================================================
; EXTERNAL DECLARATIONS
; ============================================================================

extrn g_hMainWindow:DWORD
extrn g_hStatusBar:DWORD
extrn g_hEditor:DWORD

; ============================================================================
; CODE SECTION
; ============================================================================

.code

public InitializeFileOperations
public FileNew
public FileOpen
public FileSave
public FileSaveAs
public AddToRecentFiles
public LoadRecentFilesFromRegistry
public SaveRecentFilesToRegistry
public CleanupFileOperations

; ============================================================================
; InitializeFileOperations - Setup file operations system
; ============================================================================
InitializeFileOperations proc
    ; Load recent files from registry
    call LoadRecentFilesFromRegistry
    
    ; Enable drag & drop
    invoke DragAcceptFiles, g_hMainWindow, TRUE
    
    ret
InitializeFileOperations endp

; ============================================================================
; FileNew - Create new file
; ============================================================================
FileNew proc
    ; Check if current file is modified
    cmp isFileModified, 0
    je @NoSaveNeeded
    ; Prompt to save changes
    invoke MessageBox, g_hMainWindow, offset szPromptSave, offset szPromptTitle, MB_YESNOCANCEL or MB_ICONQUESTION
    cmp eax, IDYES
    je @DoSave
    cmp eax, IDCANCEL
    je @Cancel
    jmp @NoSaveNeeded
@DoSave:
    call FileSave
    jmp @NoSaveNeeded
@Cancel:
    ret
    
@NoSaveNeeded:
    ; Clear current file info
    invoke RtlZeroMemory, offset currentFilePath, MAX_PATH_LENGTH
    invoke RtlZeroMemory, offset currentFileName, 64
    mov isFileModified, 0
    
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
        
        .if byte ptr [esi] == '\\'
            inc esi
            mov pFileName, esi
            jmp @Found
        .endif
        
        dec esi
        dec ecx
        jmp @@SearchLoop
    
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
        
        lea eax, recentFiles[i].filePath
        invoke lstrcmp, eax, offset currentFilePath
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
            imul eax, sizeof RECENT_FILE_ENTRY
            lea esi, recentFiles[eax]
            
            mov eax, i
            imul eax, sizeof RECENT_FILE_ENTRY
            lea edi, recentFiles[eax]
            
            ; Copy entry
            invoke RtlMoveMemory, edi, esi, sizeof RECENT_FILE_ENTRY
            
            dec i
            jmp @@ShiftLoop
        
        @ShiftDone:
        ; Add new entry at top
        invoke lstrcpy, offset recentFiles[0].filePath, offset currentFilePath
        invoke lstrcpy, offset recentFiles[0].fileName, offset currentFileName
        
        ; Get current time
        invoke GetSystemTimeAsFileTime, offset recentFiles[0].lastAccessed
        
        ; Increment count if not full
        .if recentFilesCount < MAX_RECENT_FILES
            inc recentFilesCount
        .endif
        
        ; Save to registry
        call SaveRecentFilesToRegistry
    .endif
    
    ret
AddToRecentFiles endp

; ============================================================================
; LoadRecentFilesFromRegistry - Load recent files from registry
; ============================================================================
LoadRecentFilesFromRegistry proc
    LOCAL hKey:DWORD
    LOCAL i:DWORD
    LOCAL dwType:DWORD
    LOCAL dwSize:DWORD
    LOCAL szValueName:db 32 dup(0)
    
    ; Open registry key
    invoke RegOpenKeyEx, HKEY_CURRENT_USER, offset recentFilesKey, 
                         0, KEY_READ, addr hKey
    
    .if eax == ERROR_SUCCESS
        ; Read count
        mov dwSize, sizeof DWORD
        invoke RegQueryValueEx, hKey, offset szCount, NULL, 
                                addr dwType, addr recentFilesCount, 
                                addr dwSize
        
        ; Read each recent file
        mov i, 0
        @@ReadLoop:
            cmp i, recentFilesCount
            jge @ReadDone
            
            ; Create value name for file path
            invoke wsprintf, offset szValueName, offset szFileFormat, i
            
            ; Read file path
            mov dwSize, MAX_PATH_LENGTH
            invoke RegQueryValueEx, hKey, offset szValueName, NULL, 
                                    addr dwType, offset recentFiles[i].filePath, 
                                    addr dwSize
            
            ; Create value name for file name
            invoke wsprintf, offset szValueName, offset szNameFormat, i
            
            ; Read file name
            mov dwSize, 64
            invoke RegQueryValueEx, hKey, offset szValueName, NULL, 
                                    addr dwType, offset recentFiles[i].fileName, 
                                    addr dwSize
            
            inc i
            jmp @@ReadLoop
        
        @ReadDone:
        invoke RegCloseKey, hKey
    .endif
    
    ret
LoadRecentFilesFromRegistry endp

; ============================================================================
; SaveRecentFilesToRegistry - Save recent files to registry
; ============================================================================
SaveRecentFilesToRegistry proc
    LOCAL hKey:DWORD
    LOCAL i:DWORD
    LOCAL dwDisposition:DWORD
    LOCAL szValueName:db 32 dup(0)
    
    ; Create/open registry key
    invoke RegCreateKeyEx, HKEY_CURRENT_USER, offset recentFilesKey, 
                           0, NULL, REG_OPTION_NON_VOLATILE, 
                           KEY_WRITE, NULL, addr hKey, addr dwDisposition
    
    .if eax == ERROR_SUCCESS
        ; Write count
        invoke RegSetValueEx, hKey, offset szCount, 0, REG_DWORD, 
                              addr recentFilesCount, sizeof DWORD
        
        ; Write each recent file
        mov i, 0
        @@WriteLoop:
            cmp i, recentFilesCount
            jge @WriteDone
            
            ; Create value name for file path
            invoke wsprintf, offset szValueName, offset szFileFormat, i
            
            ; Write file path
            invoke lstrlen, offset recentFiles[i].filePath
            inc eax
            invoke RegSetValueEx, hKey, offset szValueName, 0, REG_SZ, 
                                  offset recentFiles[i].filePath, eax
            
            ; Create value name for file name
            invoke wsprintf, offset szValueName, offset szNameFormat, i
            
            ; Write file name
            invoke lstrlen, offset recentFiles[i].fileName
            inc eax
            invoke RegSetValueEx, hKey, offset szValueName, 0, REG_SZ, 
                                  offset recentFiles[i].fileName, eax
            
            inc i
            jmp @@WriteLoop
        
        @WriteDone:
        invoke RegCloseKey, hKey
    .endif
    
    ret
SaveRecentFilesToRegistry endp

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
        ; Try to get editor text if editor window exists
        .if g_hEditor != 0
            ; Get editor text length
            invoke SendMessage, g_hEditor, WM_GETTEXTLENGTH, 0, 0
            mov edx, eax
            cmp edx, 0
            je @WriteEmpty
            
            ; Allocate buffer for editor text
            invoke LocalAlloc, LMEM_FIXED, edx
            test eax, eax
            jz @WriteEmpty
            mov esi, eax
            
            ; Get text from editor
            invoke SendMessage, g_hEditor, WM_GETTEXT, edx, esi
            
            ; Write text to file
            invoke WriteFile, hFile, esi, edx, addr dwBytesWritten, NULL
            
            ; Free buffer
            invoke LocalFree, esi
            jmp @FileWritten
        .endif
        
@WriteEmpty:
        ; No editor or empty; truncate file to zero
        invoke SetFilePointer, hFile, 0, NULL, FILE_BEGIN
        invoke SetEndOfFile, hFile
        
@FileWritten:
        ; Close file
        invoke CloseHandle, hFile
        
        ; Update modified flag
        mov isFileModified, 0
        
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
    ; Save recent files to registry
    call SaveRecentFilesToRegistry
    
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
    LOCAL filePath:db MAX_PATH_LENGTH dup(0)
    
    ; Get number of dropped files
    invoke DragQueryFile, hDrop, 0FFFFFFFFh, NULL, 0
    mov fileCount, eax
    
    ; Process each file
    mov i, 0
    @@ProcessLoop:
        cmp i, fileCount
        jge @ProcessDone
        
        ; Get file path
        invoke DragQueryFile, hDrop, i, offset filePath, MAX_PATH_LENGTH
        
        ; Process the file
        invoke lstrcpy, offset currentFilePath, offset filePath
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

; ============================================================================
; DATA STRINGS
; ============================================================================

szNewFile           db "New File", 0
szFileSaved         db "File saved", 0
szPromptSave        db "You have unsaved changes. Save before continuing?",0
szPromptTitle       db "RawrXD IDE",0
szCount            db "Count", 0
szFileFormat       db "File%d", 0
szNameFormat       db "Name%d", 0

end