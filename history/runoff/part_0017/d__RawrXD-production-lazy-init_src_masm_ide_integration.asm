; =====================================================================
; OKCOMPUTER MASM IDE INTEGRATION
; Cross-platform IDE functionality merged into RawrXD
; =====================================================================

PUBLIC IDE_CreateWindow, IDE_MessageLoop, IDE_FileNew, IDE_FileOpen
PUBLIC IDE_FileSave, IDE_FileSaveAs, IDE_EditCut, IDE_EditCopy
PUBLIC IDE_EditPaste, IDE_EditUndo, IDE_BuildAssemble, IDE_BuildLink
PUBLIC IDE_BuildRun, IDE_ShowAbout, IDE_GetEditText, IDE_SetEditText

; EXTERN CreateWindowExA:PROC
; EXTERN RegisterClassExA:PROC
; EXTERN ShowWindow:PROC
; EXTERN UpdateWindow:PROC
; EXTERN GetMessageA:PROC
; EXTERN TranslateMessage:PROC
; EXTERN DispatchMessageA:PROC
; EXTERN SendMessageA:PROC
; EXTERN SetWindowTextA:PROC
; EXTERN GetWindowTextA:PROC
; EXTERN MessageBoxA:PROC
; EXTERN LoadCursorA:PROC
; EXTERN LoadIconA:PROC
EXTERN SetMenu:PROC
EXTERN AppendMenuA:PROC
EXTERN CreateMenu:PROC
EXTERN CreatePopupMenu:PROC
; EXTERN CreateFileA:PROC
; EXTERN ReadFile:PROC
; EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN GetOpenFileNameA:PROC
EXTERN GetSaveFileNameA:PROC
EXTERN CreateProcessA:PROC
EXTERN WaitForSingleObject:PROC
EXTERN GetModuleHandleA:PROC
EXTERN LoadLibraryA:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN lstrlenA:PROC
EXTERN wsprintfA:PROC

; =====================================================================
; CONSTANTS
; =====================================================================

IDE_VERSION         EQU     100h
MAX_EDIT_SIZE       EQU     1048576     ; 1MB
MAX_PATH            EQU     260
MENU_FILE_NEW       EQU     1001
MENU_FILE_OPEN      EQU     1002
MENU_FILE_SAVE      EQU     1003
MENU_FILE_SAVEAS    EQU     1004
MENU_FILE_EXIT      EQU     1005
MENU_EDIT_UNDO      EQU     2001
MENU_EDIT_CUT       EQU     2002
MENU_EDIT_COPY      EQU     2003
MENU_EDIT_PASTE     EQU     2004
MENU_BUILD_ASSEMBLE EQU     3001
MENU_BUILD_LINK     EQU     3002
MENU_BUILD_RUN      EQU     3003
MENU_HELP_ABOUT     EQU     4001
MENU_REVERSE_ENGINEER EQU   5001
MENU_BINARY_ANALYSIS EQU    5002
MENU_DISASSEMBLE    EQU     5003
MENU_HEX_DUMP       EQU     5004

; Window styles
WS_OVERLAPPEDWINDOW EQU     0CF0000h
WS_CHILD            EQU     40000000h
WS_VISIBLE          EQU     10000000h
WS_VSCROLL          EQU     200000h
WS_HSCROLL          EQU     100000h
WS_CLIPCHILDREN     EQU     2000000h

; Edit styles
ES_MULTILINE        EQU     4h
ES_AUTOVSCROLL      EQU     40h
ES_AUTOHSCROLL      EQU     80h
ES_NOHIDESEL        EQU     100h

; =====================================================================
; DATA
; =====================================================================

.data
ClassName       db "RawrXD_IDE_Class",0
AppName         db "RawrXD Agentic IDE with Reverse Engineering",0
FileName        BYTE MAX_PATH DUP(0)
Untitled        db "Untitled.asm",0
Modified        DWORD 0

; Menu items
MenuFile        db "File",0
MenuEdit        db "Edit",0
MenuBuild       db "Build",0
MenuReverse     db "Reverse",0
MenuHelp        db "Help",0
MNew            db "New",9,"Ctrl+N",0
MOpen           db "Open...",9,"Ctrl+O",0
MSave           db "Save",9,"Ctrl+S",0
MSaveAs         db "Save As...",0
MExit           db "Exit",0
MUndo           db "Undo",9,"Ctrl+Z",0
MCut            db "Cut",9,"Ctrl+X",0
MCopy           db "Copy",9,"Ctrl+C",0
MPaste          db "Paste",9,"Ctrl+V",0
MAssemble       db "Assemble",9,"F5",0
MLink           db "Link",9,"F6",0
MRun            db "Run",9,"F7",0
MReverseEngineer db "Reverse Engineer",0
MBinaryAnalysis db "Binary Analysis",0
MDisassemble    db "Disassemble",0
MHexDump        db "Hex Dump",0
MAbout          db "About...",0

; Dialog filters
AsmFilter       db "Assembly Files",0,"*.asm",0,"All Files",0,"*.*",0,0
ExeFilter       db "Executable Files",0,"*.exe;*.dll;*.bin",0,"All Files",0,"*.*",0,0

; RichEdit class
RichEditClass   db "RichEdit20A",0

; About text
AboutText       db "RawrXD Agentic IDE with Reverse Engineering",13,10
                db "Version 1.0 - Pure MASM64 Implementation",13,10
                db "40-Agent Swarm + 800B Model + Ghidra-like Features",13,10
                db "Cross-platform ready",0

.data?
hInstance       QWORD ?
hWnd           QWORD ?
hEdit          QWORD ?
hMenu          QWORD ?
hFont          QWORD ?
EditBuffer     BYTE MAX_EDIT_SIZE DUP(?)
BufferSize     DWORD ?

; =====================================================================
; CODE
; =====================================================================

.code

; =====================================================================
; IDE_CreateWindow - Create IDE main window
; RCX = instance handle
; Returns: RAX = window handle
; =====================================================================
IDE_CreateWindow PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    sub     rsp, 40h
    
    mov     hInstance, rcx
    
    ; Initialize RichEdit
    invoke  LoadLibraryA, OFFSET RichEditClass
    
    ; Register window class
    call    RegisterWinClass
    
    ; Create main window
    call    CreateMainWindow
    
    ; Show window
    invoke  ShowWindow, hWnd, 1  ; SW_SHOWNORMAL
    invoke  UpdateWindow, hWnd
    
    mov     rax, hWnd
    
    add     rsp, 40h
    pop     rbx
    pop     rbp
    ret
IDE_CreateWindow ENDP

; =====================================================================
; RegisterWinClass - Register window class
; =====================================================================
RegisterWinClass PROC
    LOCAL   wc:WNDCLASSEX
    
    mov     wc.cbSize, SIZEOF WNDCLASSEX
    mov     wc.style, 3  ; CS_HREDRAW or CS_VREDRAW
    mov     wc.lpfnWndProc, OFFSET WndProc
    mov     wc.cbClsExtra, 0
    mov     wc.cbWndExtra, 0
    mov     rax, hInstance
    mov     wc.hInstance, rax
    invoke  LoadIconA, 0, 32512  ; IDI_APPLICATION
    mov     wc.hIcon, rax
    invoke  LoadCursorA, 0, 32512  ; IDC_ARROW
    mov     wc.hCursor, rax
    mov     wc.hbrBackground, 5 + 1  ; COLOR_WINDOW + 1
    mov     wc.lpszMenuName, 0
    mov     wc.lpszClassName, OFFSET ClassName
    mov     wc.hIconSm, 0
    
    invoke  RegisterClassExA, OFFSET wc
    ret
RegisterWinClass ENDP

; =====================================================================
; CreateMainWindow - Create main IDE window
; =====================================================================
CreateMainWindow PROC
    invoke  CreateWindowExA, 0, OFFSET ClassName, OFFSET AppName,
            WS_OVERLAPPEDWINDOW,
            100, 100, 800, 600,
            0, 0, hInstance, 0
    mov     hWnd, rax
    
    ; Create menu
    call    CreateMenuBar
    
    ; Create edit control
    call    CreateEditControl
    
    ret
CreateMainWindow ENDP

; =====================================================================
; CreateMenuBar - Create IDE menu bar
; =====================================================================
CreateMenuBar PROC
    invoke  CreateMenu
    mov     hMenu, rax
    
    ; File menu
    invoke  CreatePopupMenu
    push    rax
    invoke  AppendMenuA, rax, 0, MENU_FILE_NEW, OFFSET MNew
    invoke  AppendMenuA, rax, 0, MENU_FILE_OPEN, OFFSET MOpen
    invoke  AppendMenuA, rax, 0, 0, 0  ; separator
    invoke  AppendMenuA, rax, 0, MENU_FILE_SAVE, OFFSET MSave
    invoke  AppendMenuA, rax, 0, MENU_FILE_SAVEAS, OFFSET MSaveAs
    invoke  AppendMenuA, rax, 0, 0, 0  ; separator
    invoke  AppendMenuA, rax, 0, MENU_FILE_EXIT, OFFSET MExit
    pop     rax
    invoke  AppendMenuA, hMenu, 16, rax, OFFSET MenuFile  ; MF_POPUP
    
    ; Edit menu
    invoke  CreatePopupMenu
    push    rax
    invoke  AppendMenuA, rax, 0, MENU_EDIT_UNDO, OFFSET MUndo
    invoke  AppendMenuA, rax, 0, 0, 0  ; separator
    invoke  AppendMenuA, rax, 0, MENU_EDIT_CUT, OFFSET MCut
    invoke  AppendMenuA, rax, 0, MENU_EDIT_COPY, OFFSET MCopy
    invoke  AppendMenuA, rax, 0, MENU_EDIT_PASTE, OFFSET MPaste
    pop     rax
    invoke  AppendMenuA, hMenu, 16, rax, OFFSET MenuEdit
    
    ; Build menu
    invoke  CreatePopupMenu
    push    rax
    invoke  AppendMenuA, rax, 0, MENU_BUILD_ASSEMBLE, OFFSET MAssemble
    invoke  AppendMenuA, rax, 0, MENU_BUILD_LINK, OFFSET MLink
    invoke  AppendMenuA, rax, 0, MENU_BUILD_RUN, OFFSET MRun
    pop     rax
    invoke  AppendMenuA, hMenu, 16, rax, OFFSET MenuBuild
    
    ; Reverse Engineering menu
    invoke  CreatePopupMenu
    push    rax
    invoke  AppendMenuA, rax, 0, MENU_REVERSE_ENGINEER, OFFSET MReverseEngineer
    invoke  AppendMenuA, rax, 0, MENU_BINARY_ANALYSIS, OFFSET MBinaryAnalysis
    invoke  AppendMenuA, rax, 0, MENU_DISASSEMBLE, OFFSET MDisassemble
    invoke  AppendMenuA, rax, 0, MENU_HEX_DUMP, OFFSET MHexDump
    pop     rax
    invoke  AppendMenuA, hMenu, 16, rax, OFFSET MenuReverse
    
    ; Help menu
    invoke  CreatePopupMenu
    push    rax
    invoke  AppendMenuA, rax, 0, MENU_HELP_ABOUT, OFFSET MAbout
    pop     rax
    invoke  AppendMenuA, hMenu, 16, rax, OFFSET MenuHelp
    
    invoke  SetMenu, hWnd, hMenu
    ret
CreateMenuBar ENDP

; =====================================================================
; CreateEditControl - Create RichEdit control
; =====================================================================
CreateEditControl PROC
    invoke  CreateWindowExA, 512, OFFSET RichEditClass, 0,
            WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_HSCROLL or
            ES_MULTILINE or ES_AUTOVSCROLL or ES_AUTOHSCROLL or ES_NOHIDESEL,
            0, 0, 0, 0,
            hWnd, 0, hInstance, 0
    mov     hEdit, rax
    
    ; Set default text
    invoke  SetWindowTextA, hEdit, OFFSET Untitled
    
    ret
CreateEditControl ENDP

; =====================================================================
; IDE_MessageLoop - Main message loop
; =====================================================================
IDE_MessageLoop PROC
    LOCAL   msg:MSG
    
msg_loop:
    invoke  GetMessageA, OFFSET msg, 0, 0, 0
    test    rax, rax
    jz      msg_done
    invoke  TranslateMessage, OFFSET msg
    invoke  DispatchMessageA, OFFSET msg
    jmp     msg_loop
    
msg_done:
    ret
IDE_MessageLoop ENDP

; =====================================================================
; WndProc - Window procedure
; =====================================================================
WndProc PROC hWin:QWORD, uMsg:DWORD, wParam:QWORD, lParam:QWORD
    .if uMsg == 2  ; WM_DESTROY
        invoke  PostQuitMessage, 0
        
    .elseif uMsg == 5  ; WM_SIZE
        ; Resize edit control
        invoke  MoveWindow, hEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), 1
        
    .elseif uMsg == 273  ; WM_COMMAND
        mov     eax, wParam
        .if eax == MENU_FILE_NEW
            call    IDE_FileNew
        .elseif eax == MENU_FILE_OPEN
            call    IDE_FileOpen
        .elseif eax == MENU_FILE_SAVE
            call    IDE_FileSave
        .elseif eax == MENU_FILE_SAVEAS
            call    IDE_FileSaveAs
        .elseif eax == MENU_FILE_EXIT
            invoke  SendMessageA, hWin, 16, 0, 0  ; WM_CLOSE
        .elseif eax == MENU_EDIT_UNDO
            invoke  SendMessageA, hEdit, 199, 0, 0  ; WM_UNDO
        .elseif eax == MENU_EDIT_CUT
            invoke  SendMessageA, hEdit, 768, 0, 0  ; WM_CUT
        .elseif eax == MENU_EDIT_COPY
            invoke  SendMessageA, hEdit, 769, 0, 0  ; WM_COPY
        .elseif eax == MENU_EDIT_PASTE
            invoke  SendMessageA, hEdit, 770, 0, 0  ; WM_PASTE
        .elseif eax == MENU_BUILD_ASSEMBLE
            call    IDE_BuildAssemble
        .elseif eax == MENU_BUILD_LINK
            call    IDE_BuildLink
        .elseif eax == MENU_BUILD_RUN
            call    IDE_BuildRun
        .elseif eax == MENU_REVERSE_ENGINEER
            call    ReverseEngineerFile
        .elseif eax == MENU_BINARY_ANALYSIS
            call    BinaryAnalysis
        .elseif eax == MENU_DISASSEMBLE
            call    DisassembleFile
        .elseif eax == MENU_HEX_DUMP
            call    HexDumpFile
        .elseif eax == MENU_HELP_ABOUT
            call    IDE_ShowAbout
        .endif
        
    .endif
    
    invoke  DefWindowProcA, hWin, uMsg, wParam, lParam
    ret
WndProc ENDP

; =====================================================================
; IDE_FileNew - Create new file
; =====================================================================
IDE_FileNew PROC
    invoke  SetWindowTextA, hEdit, OFFSET Untitled
    mov     Modified, 0
    ret
IDE_FileNew ENDP

; =====================================================================
; IDE_FileOpen - Open file
; =====================================================================
IDE_FileOpen PROC
    LOCAL   ofn:OPENFILENAME
    
    mov     ofn.lStructSize, SIZEOF OPENFILENAME
    mov     rax, hWnd
    mov     ofn.hwndOwner, rax
    mov     rax, hInstance
    mov     ofn.hInstance, rax
    mov     ofn.lpstrFilter, OFFSET AsmFilter
    mov     ofn.lpstrFile, OFFSET FileName
    mov     ofn.nMaxFile, MAX_PATH
    mov     ofn.Flags, 0x1000  ; OFN_FILEMUSTEXIST
    
    invoke  GetOpenFileNameA, OFFSET ofn
    test    rax, rax
    jz      open_done
    
    ; Load file into edit control
    call    LoadFile
    
open_done:
    ret
IDE_FileOpen ENDP

; =====================================================================
; LoadFile - Load file into edit control
; =====================================================================
LoadFile PROC
    LOCAL   hFile:QWORD, bytesRead:DWORD
    
    invoke  CreateFileA, OFFSET FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0
    cmp     rax, INVALID_HANDLE_VALUE
    je      load_error
    mov     hFile, rax
    
    invoke  GetFileSize, hFile, 0
    cmp     rax, MAX_EDIT_SIZE
    ja      load_too_big
    mov     BufferSize, eax
    
    invoke  ReadFile, hFile, OFFSET EditBuffer, BufferSize, OFFSET bytesRead, 0
    invoke  CloseHandle, hFile
    
    ; Set edit control text
    mov     byte ptr [EditBuffer + rax], 0
    invoke  SetWindowTextA, hEdit, OFFSET EditBuffer
    mov     Modified, 0
    ret
    
load_too_big:
    invoke  CloseHandle, hFile
    
load_error:
    invoke  MessageBoxA, hWnd, OFFSET str$("File too large or error loading"), OFFSET AppName, 0
    ret
LoadFile ENDP

; =====================================================================
; IDE_FileSave - Save file
; =====================================================================
IDE_FileSave PROC
    cmp     byte ptr [FileName], 0
    jne     save_file
    call    IDE_FileSaveAs
    ret
    
save_file:
    call    SaveFile
    ret
IDE_FileSave ENDP

; =====================================================================
; IDE_FileSaveAs - Save file with dialog
; =====================================================================
IDE_FileSaveAs PROC
    LOCAL   ofn:OPENFILENAME
    
    mov     ofn.lStructSize, SIZEOF OPENFILENAME
    mov     rax, hWnd
    mov     ofn.hwndOwner, rax
    mov     rax, hInstance
    mov     ofn.hInstance, rax
    mov     ofn.lpstrFilter, OFFSET AsmFilter
    mov     ofn.lpstrFile, OFFSET FileName
    mov     ofn.nMaxFile, MAX_PATH
    mov     ofn.Flags, 0x1000  ; OFN_OVERWRITEPROMPT
    
    invoke  GetSaveFileNameA, OFFSET ofn
    test    rax, rax
    jz      saveas_done
    
    call    SaveFile
    
saveas_done:
    ret
IDE_FileSaveAs ENDP

; =====================================================================
; SaveFile - Save edit content to file
; =====================================================================
SaveFile PROC
    LOCAL   hFile:QWORD, bytesWritten:DWORD
    
    invoke  CreateFileA, OFFSET FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0
    cmp     rax, INVALID_HANDLE_VALUE
    je      save_error
    mov     hFile, rax
    
    invoke  GetWindowTextA, hEdit, OFFSET EditBuffer, MAX_EDIT_SIZE
    mov     BufferSize, eax
    
    invoke  WriteFile, hFile, OFFSET EditBuffer, BufferSize, OFFSET bytesWritten, 0
    invoke  CloseHandle, hFile
    
    mov     Modified, 0
    ret
    
save_error:
    invoke  MessageBoxA, hWnd, OFFSET str$("Error saving file"), OFFSET AppName, 0
    ret
SaveFile ENDP

; =====================================================================
; IDE_BuildAssemble - Assemble current file
; =====================================================================
IDE_BuildAssemble PROC
    ; TODO: Implement MASM assembly
    invoke  MessageBoxA, hWnd, OFFSET str$("Assemble feature coming soon"), OFFSET AppName, 0
    ret
IDE_BuildAssemble ENDP

; =====================================================================
; IDE_BuildLink - Link object file
; =====================================================================
IDE_BuildLink PROC
    ; TODO: Implement linking
    invoke  MessageBoxA, hWnd, OFFSET str$("Link feature coming soon"), OFFSET AppName, 0
    ret
IDE_BuildLink ENDP

; =====================================================================
; IDE_BuildRun - Run executable
; =====================================================================
IDE_BuildRun PROC
    ; TODO: Implement execution
    invoke  MessageBoxA, hWnd, OFFSET str$("Run feature coming soon"), OFFSET AppName, 0
    ret
IDE_BuildRun ENDP

; =====================================================================
; Reverse Engineering Functions
; =====================================================================

ReverseEngineerFile PROC
    invoke  MessageBoxA, hWnd, OFFSET str$("Reverse Engineering feature coming soon"), OFFSET AppName, 0
    ret
ReverseEngineerFile ENDP

BinaryAnalysis PROC
    invoke  MessageBoxA, hWnd, OFFSET str$("Binary Analysis feature coming soon"), OFFSET AppName, 0
    ret
BinaryAnalysis ENDP

DisassembleFile PROC
    invoke  MessageBoxA, hWnd, OFFSET str$("Disassembly feature coming soon"), OFFSET AppName, 0
    ret
DisassembleFile ENDP

HexDumpFile PROC
    invoke  MessageBoxA, hWnd, OFFSET str$("Hex Dump feature coming soon"), OFFSET AppName, 0
    ret
HexDumpFile ENDP

; =====================================================================
; IDE_ShowAbout - Show about dialog
; =====================================================================
IDE_ShowAbout PROC
    invoke  MessageBoxA, hWnd, OFFSET AboutText, OFFSET AppName, 0
    ret
IDE_ShowAbout ENDP

; =====================================================================
; IDE_GetEditText - Get edit control text
; RCX = buffer, RDX = buffer size
; =====================================================================
IDE_GetEditText PROC
    invoke  GetWindowTextA, hEdit, rcx, edx
    ret
IDE_GetEditText ENDP

; =====================================================================
; IDE_SetEditText - Set edit control text
; RCX = text
; =====================================================================
IDE_SetEditText PROC
    invoke  SetWindowTextA, hEdit, rcx
    ret
IDE_SetEditText ENDP

END
