; =====================================================================
; Professional NASM IDE Integration
; Windows API Integration and IDE Framework
; Features: Windows Programming, Menu Integration, Build Commands
; =====================================================================

; Windows API Constants
%define WM_COMMAND                  0x0111
%define WM_CREATE                   0x0001
%define WM_DESTROY                  0x0002
%define WM_CLOSE                    0x0010

%define ID_FILE_NEW                 1001
%define ID_FILE_OPEN                1002
%define ID_FILE_SAVE                1003
%define ID_FILE_EXIT                1004

%define ID_BUILD_DEBUG              2001
%define ID_BUILD_RELEASE            2002
%define ID_BUILD_SMALL              2003
%define ID_BUILD_FAST               2004
%define ID_BUILD_CLEAN              2005

%define ID_TOOLS_NASM               3001
%define ID_TOOLS_GCC                3002
%define ID_TOOLS_DETECT             3003

%define ID_HELP_ABOUT               4001

; Build Command IDs
%define ID_BUILD_WITH_NASM_EXE      5001
%define ID_BUILD_WITH_NASM_DLL      5002
%define ID_BUILD_WITH_NASM_OBJ      5003

; Windows Structures
struc WNDCLASS
    .style          resd 1
    .lpfnWndProc    resq 1
    .cbClsExtra     resd 1
    .cbWndExtra     resd 1
    .hInstance      resq 1
    .hIcon          resq 1
    .hCursor        resq 1
    .hbrBackground  resq 1
    .lpszMenuName   resq 1
    .lpszClassName  resq 1
endstruc

struc MSG
    .hwnd           resq 1
    .message        resd 1
    .wParam         resq 1
    .lParam         resq 1
    .time           resd 1
    .pt             resq 1
endstruc

struc MENUITEMINFO
    .cbSize         resd 1
    .fMask          resd 1
    .fType          resd 1
    .fState         resd 1
    .wID            resd 1
    .hSubMenu       resq 1
    .hbmpChecked    resq 1
    .hbmpUnchecked  resq 1
    .dwItemData     resq 1
    .dwTypeData     resq 1
    .cch            resd 1
endstruc

section .data
    ; Window Class Information
    szClassName     db "NASM_IDE_CLASS", 0
    szWindowTitle   db "Professional NASM IDE", 0

    ; Menu Strings
    szFile          db "&File", 0
    szBuild         db "&Build", 0
    szTools         db "&Tools", 0
    szHelp          db "&Help", 0

    szNew           db "&New", 9, "Ctrl+N", 0
    szOpen          db "&Open...", 9, "Ctrl+O", 0
    szSave          db "&Save", 9, "Ctrl+S", 0
    szExit          db "E&xit", 0

    szBuildDebug    db "&Debug Build", 9, "F5", 0
    szBuildRelease  db "&Release Build", 9, "F6", 0
    szBuildSmall    db "S&mall Build", 9, "F7", 0
    szBuildFast     db "&Fast Build", 9, "F8", 0
    szClean         db "&Clean", 9, "Ctrl+F5", 0

    szNasmTools     db "&NASM Tools", 0
    szGccTools      db "&GCC Tools", 0
    szDetectTools   db "&Detect Tools", 0

    szAbout         db "&About", 0

    ; Build Commands
    szBuildWithNasmExe  db "Build with NASM (Executable)", 0
    szBuildWithNasmDll  db "Build with NASM (DLL)", 0
    szBuildWithNasmObj  db "Build with NASM (Object)", 0

    ; Status Messages
    szReady         db "Ready", 0
    szBuilding      db "Building...", 0
    szBuildSuccess  db "Build completed successfully", 0
    szBuildFailed   db "Build failed", 0

    ; Error Messages
    szError         db "Error", 0
    szBuildError    db "Build process failed", 0

section .bss
    ; Window Handles
    hWnd            resq 1
    hInstance       resq 1
    hMenu           resq 1

    ; Build Process Information
    hBuildProcess   resq 1
    dwBuildResult   resd 1

    ; Status Bar
    hStatus         resq 1
    statusText      resb 256

    ; File Information
    currentFile     resb 260  ; MAX_PATH

    ; Message Structure
    msg             resb MSG_size

section .text
    global WinMain
    extern RegisterClassExA
    extern CreateWindowExA
    extern ShowWindow
    extern UpdateWindow
    extern GetMessageA
    extern TranslateMessage
    extern DispatchMessageA
    extern PostQuitMessage
    extern DefWindowProcA
    extern CreateMenu
    extern AppendMenuA
    extern SetMenu
    extern CreateStatusWindowA
    extern SendMessageA
    extern MessageBoxA
    extern CreateProcessA
    extern WaitForSingleObject
    extern GetExitCodeProcess
    extern CloseHandle
    extern GetModuleHandleA
    extern GetCommandLineA

; =====================================================================
; Windows Entry Point
; =====================================================================
WinMain:
    ; Get module handle
    call GetModuleHandleA
    mov [hInstance], rax

    ; Register window class
    call RegisterWindowClass
    test rax, rax
    jz .error

    ; Create main window
    call CreateMainWindow
    test rax, rax
    jz .error

    ; Show window
    mov rcx, [hWnd]
    mov rdx, SW_SHOW
    call ShowWindow

    mov rcx, [hWnd]
    call UpdateWindow

    ; Message loop
    call MessageLoop

    ; Exit
    xor ecx, ecx
    call ExitProcess

.error:
    mov ecx, 1
    call ExitProcess

; =====================================================================
; Register Window Class
; =====================================================================
RegisterWindowClass:
    push rbp
    mov rbp, rsp
    sub rsp, WNDCLASS_size

    ; Setup WNDCLASS structure
    mov dword [rsp + WNDCLASS.style], 0
    lea rax, [WndProc]
    mov [rsp + WNDCLASS.lpfnWndProc], rax
    mov dword [rsp + WNDCLASS.cbClsExtra], 0
    mov dword [rsp + WNDCLASS.cbWndExtra], 0
    mov rax, [hInstance]
    mov [rsp + WNDCLASS.hInstance], rax
    mov qword [rsp + WNDCLASS.hIcon], 0
    mov qword [rsp + WNDCLASS.hCursor], 0
    mov qword [rsp + WNDCLASS.hbrBackground], 0
    mov qword [rsp + WNDCLASS.lpszMenuName], 0
    lea rax, [szClassName]
    mov [rsp + WNDCLASS.lpszClassName], rax

    ; Register class
    lea rcx, [rsp]
    call RegisterClassExA

    add rsp, WNDCLASS_size
    pop rbp
    ret

; =====================================================================
; Create Main Window
; =====================================================================
CreateMainWindow:
    push rbp
    mov rbp, rsp

    ; Create window
    xor rcx, rcx                    ; dwExStyle
    lea rdx, [szClassName]          ; lpClassName
    lea r8, [szWindowTitle]         ; lpWindowName
    mov r9d, 0x00CF0000            ; dwStyle (WS_OVERLAPPEDWINDOW)
    mov dword [rsp + 32], 100      ; x
    mov dword [rsp + 40], 100      ; y
    mov dword [rsp + 48], 1000     ; nWidth
    mov dword [rsp + 56], 700      ; nHeight
    mov rax, [hInstance]
    mov [rsp + 64], rax            ; hInstance
    mov qword [rsp + 72], 0        ; hMenu
    mov qword [rsp + 80], 0        ; hWndParent
    mov dword [rsp + 88], 0        ; nShowCmd
    mov dword [rsp + 96], 0        ; dwExStyle

    call CreateWindowExA

    mov [hWnd], rax

    ; Create menu
    call CreateApplicationMenu

    ; Create status bar
    call CreateStatusBar

    pop rbp
    ret

; =====================================================================
; Create Application Menu
; =====================================================================
CreateApplicationMenu:
    push rbp
    mov rbp, rsp

    ; Create main menu
    call CreateMenu
    mov [hMenu], rax

    ; File menu
    call CreateFileMenu

    ; Build menu
    call CreateBuildMenu

    ; Tools menu
    call CreateToolsMenu

    ; Help menu
    call CreateHelpMenu

    ; Set menu to window
    mov rcx, [hWnd]
    mov rdx, [hMenu]
    call SetMenu

    pop rbp
    ret

; =====================================================================
; Create File Menu
; =====================================================================
CreateFileMenu:
    push rbp
    mov rbp, rsp

    ; Create File submenu
    call CreateMenu
    mov rbx, rax

    ; Add menu items
    mov rcx, rbx
    xor rdx, rdx              ; uFlags
    mov r8d, 0x0000           ; uIDNewItem
    mov r9d, ID_FILE_NEW      ; uID
    lea rax, [szNew]
    mov [rsp + 32], rax       ; lpNewItem
    call AppendMenuA

    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_FILE_OPEN
    lea rax, [szOpen]
    mov [rsp + 32], rax
    call AppendMenuA

    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_FILE_SAVE
    lea rax, [szSave]
    mov [rsp + 32], rax
    call AppendMenuA

    ; Separator
    mov rcx, rbx
    mov rdx, 0x00000800       ; MF_SEPARATOR
    mov r8d, 0
    mov r9d, 0
    mov qword [rsp + 32], 0
    call AppendMenuA

    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_FILE_EXIT
    lea rax, [szExit]
    mov [rsp + 32], rax
    call AppendMenuA

    ; Add to main menu
    mov rcx, [hMenu]
    xor rdx, rdx
    xor r8, r8
    mov r9d, 0                ; uID
    lea rax, [szFile]
    mov [rsp + 32], rax
    mov [rsp + 40], rbx       ; hSubMenu
    call AppendMenuA

    pop rbp
    ret

; =====================================================================
; Create Build Menu
; =====================================================================
CreateBuildMenu:
    push rbp
    mov rbp, rsp

    ; Create Build submenu
    call CreateMenu
    mov rbx, rax

    ; Add build options
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_BUILD_DEBUG
    lea rax, [szBuildDebug]
    mov [rsp + 32], rax
    call AppendMenuA

    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_BUILD_RELEASE
    lea rax, [szBuildRelease]
    mov [rsp + 32], rax
    call AppendMenuA

    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_BUILD_SMALL
    lea rax, [szBuildSmall]
    mov [rsp + 32], rax
    call AppendMenuA

    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_BUILD_FAST
    lea rax, [szBuildFast]
    mov [rsp + 32], rax
    call AppendMenuA

    ; Separator
    mov rcx, rbx
    mov rdx, 0x00000800
    mov r8d, 0
    mov r9d, 0
    mov qword [rsp + 32], 0
    call AppendMenuA

    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_BUILD_CLEAN
    lea rax, [szClean]
    mov [rsp + 32], rax
    call AppendMenuA

    ; Add to main menu
    mov rcx, [hMenu]
    xor rdx, rdx
    xor r8, r8
    mov r9d, 1
    lea rax, [szBuild]
    mov [rsp + 32], rax
    mov [rsp + 40], rbx
    call AppendMenuA

    pop rbp
    ret

; =====================================================================
; Create Tools Menu
; =====================================================================
CreateToolsMenu:
    push rbp
    mov rbp, rsp

    ; Create Tools submenu
    call CreateMenu
    mov rbx, rax

    ; Add tool options
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_TOOLS_NASM
    lea rax, [szNasmTools]
    mov [rsp + 32], rax
    call AppendMenuA

    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_TOOLS_GCC
    lea rax, [szGccTools]
    mov [rsp + 32], rax
    call AppendMenuA

    ; Separator
    mov rcx, rbx
    mov rdx, 0x00000800
    mov r8d, 0
    mov r9d, 0
    mov qword [rsp + 32], 0
    call AppendMenuA

    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_TOOLS_DETECT
    lea rax, [szDetectTools]
    mov [rsp + 32], rax
    call AppendMenuA

    ; Add to main menu
    mov rcx, [hMenu]
    xor rdx, rdx
    xor r8, r8
    mov r9d, 2
    lea rax, [szTools]
    mov [rsp + 32], rax
    mov [rsp + 40], rbx
    call AppendMenuA

    pop rbp
    ret

; =====================================================================
; Create Help Menu
; =====================================================================
CreateHelpMenu:
    push rbp
    mov rbp, rsp

    ; Create Help submenu
    call CreateMenu
    mov rbx, rax

    ; Add help option
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_HELP_ABOUT
    lea rax, [szAbout]
    mov [rsp + 32], rax
    call AppendMenuA

    ; Add to main menu
    mov rcx, [hMenu]
    xor rdx, rdx
    xor r8, r8
    mov r9d, 3
    lea rax, [szHelp]
    mov [rsp + 32], rax
    mov [rsp + 40], rbx
    call AppendMenuA

    pop rbp
    ret

; =====================================================================
; Create Status Bar
; =====================================================================
CreateStatusBar:
    push rbp
    mov rbp, rsp

    ; Create status bar
    mov rcx, 0                    ; dwStyle
    lea rdx, [szReady]            ; lpszText
    mov r8, [hWnd]                ; hwndParent
    mov r9d, 0                    ; wID

    call CreateStatusWindowA
    mov [hStatus], rax

    pop rbp
    ret

; =====================================================================
; Window Procedure
; =====================================================================
WndProc:
    push rbp
    mov rbp, rsp

    ; Handle messages
    cmp edx, WM_COMMAND
    je .handle_command

    cmp edx, WM_CREATE
    je .handle_create

    cmp edx, WM_DESTROY
    je .handle_destroy

    ; Default processing
    call DefWindowProcA
    jmp .done

.handle_command:
    ; Extract command ID from wParam (low word)
    mov eax, r8d
    and eax, 0xFFFF

    ; Handle menu commands
    cmp eax, ID_FILE_NEW
    je .file_new
    cmp eax, ID_FILE_OPEN
    je .file_open
    cmp eax, ID_FILE_SAVE
    je .file_save
    cmp eax, ID_FILE_EXIT
    je .file_exit

    cmp eax, ID_BUILD_DEBUG
    je .build_debug
    cmp eax, ID_BUILD_RELEASE
    je .build_release
    cmp eax, ID_BUILD_SMALL
    je .build_small
    cmp eax, ID_BUILD_FAST
    je .build_fast
    cmp eax, ID_BUILD_CLEAN
    je .build_clean

    cmp eax, ID_TOOLS_NASM
    je .tools_nasm
    cmp eax, ID_TOOLS_GCC
    je .tools_gcc
    cmp eax, ID_TOOLS_DETECT
    je .tools_detect

    cmp eax, ID_HELP_ABOUT
    je .help_about

    jmp .default

.file_new:
    call OnFileNew
    jmp .done

.file_open:
    call OnFileOpen
    jmp .done

.file_save:
    call OnFileSave
    jmp .done

.file_exit:
    call OnFileExit
    jmp .done

.build_debug:
    call OnBuildDebug
    jmp .done

.build_release:
    call OnBuildRelease
    jmp .done

.build_small:
    call OnBuildSmall
    jmp .done

.build_fast:
    call OnBuildFast
    jmp .done

.build_clean:
    call OnBuildClean
    jmp .done

.tools_nasm:
    call OnToolsNasm
    jmp .done

.tools_gcc:
    call OnToolsGcc
    jmp .done

.tools_detect:
    call OnToolsDetect
    jmp .done

.help_about:
    call OnHelpAbout
    jmp .done

.default:
    call DefWindowProcA
    jmp .done

.handle_create:
    xor eax, eax  ; Success
    jmp .done

.handle_destroy:
    call PostQuitMessage
    xor eax, eax
    jmp .done

.done:
    pop rbp
    ret

; =====================================================================
; Message Loop
; =====================================================================
MessageLoop:
    push rbp
    mov rbp, rsp

.loop:
    ; Get message
    xor rcx, rcx              ; hWnd (NULL for all windows)
    xor rdx, rdx              ; wMsgFilterMin
    xor r8, r8                ; wMsgFilterMax
    lea r9, [msg]             ; lpMsg
    call GetMessageA

    test eax, eax
    jz .quit

    ; Translate and dispatch
    lea rcx, [msg]
    call TranslateMessage

    lea rcx, [msg]
    call DispatchMessageA

    jmp .loop

.quit:
    pop rbp
    ret

; =====================================================================
; Command Handlers
; =====================================================================

OnFileNew:
    ; Create new assembly file
    ret

OnFileOpen:
    ; Open assembly file
    ret

OnFileSave:
    ; Save current file
    ret

OnFileExit:
    ; Exit application
    mov rcx, [hWnd]
    call SendMessageA  ; WM_CLOSE
    ret

OnBuildDebug:
    ; Execute debug build
    mov rcx, 1  ; BUILD_DEBUG
    call ExecuteBuildType
    ret

OnBuildRelease:
    ; Execute release build
    mov rcx, 2  ; BUILD_RELEASE
    call ExecuteBuildType
    ret

OnBuildSmall:
    ; Execute small build
    mov rcx, 3  ; BUILD_SMALL
    call ExecuteBuildType
    ret

OnBuildFast:
    ; Execute fast build
    mov rcx, 4  ; BUILD_FAST
    call ExecuteBuildType
    ret

OnBuildClean:
    ; Clean build output
    ret

OnToolsNasm:
    ; Show NASM tools
    call ShowBuildWithNasmMenu
    ret

OnToolsGcc:
    ; Show GCC tools
    ret

OnToolsDetect:
    ; Detect available tools
    ret

OnHelpAbout:
    ; Show about dialog
    lea rcx, [szWindowTitle]
    lea rdx, [aboutText]
    xor r8, r8
    call MessageBoxA
    ret

aboutText db "Professional NASM IDE", 13, 10
          db "Advanced x86-64 Assembly Development Environment", 13, 10
          db "Build System v2.1.0", 0

; =====================================================================
; Build Execution Functions
; =====================================================================

ExecuteBuildType:
    push rbp
    mov rbp, rsp

    ; Update status
    call UpdateStatusBuilding

    ; Execute build based on type
    cmp ecx, 1
    je .debug
    cmp ecx, 2
    je .release
    cmp ecx, 3
    je .small
    cmp ecx, 4
    je .fast

    jmp .done

.debug:
    call ExecuteDebugBuild
    jmp .done

.release:
    call ExecuteReleaseBuild
    jmp .done

.small:
    call ExecuteSmallBuild
    jmp .done

.fast:
    call ExecuteFastBuild

.done:
    ; Update status based on result
    test eax, eax
    jnz .success

    call UpdateStatusBuildFailed
    jmp .exit

.success:
    call UpdateStatusBuildSuccess

.exit:
    pop rbp
    ret

ExecuteDebugBuild:
    ; Debug build implementation
    lea rcx, [debugCommand]
    call ExecuteBuildCommand
    ret

ExecuteReleaseBuild:
    ; Release build implementation
    lea rcx, [releaseCommand]
    call ExecuteBuildCommand
    ret

ExecuteSmallBuild:
    ; Small build implementation
    lea rcx, [smallCommand]
    call ExecuteBuildCommand
    ret

ExecuteFastBuild:
    ; Fast build implementation
    lea rcx, [fastCommand]
    call ExecuteBuildCommand
    ret

debugCommand   db "nasm -g -f win64 -o output.obj source.asm && gcc -g output.obj -o output.exe", 0
releaseCommand db "nasm -O2 -f win64 -o output.obj source.asm && gcc -O2 -s output.obj -o output.exe", 0
smallCommand   db "nasm -Os -f win64 -o output.obj source.asm && gcc -Os -s output.obj -o output.exe", 0
fastCommand    db "nasm -O3 -f win64 -o output.obj source.asm && gcc -O3 output.obj -o output.exe", 0

; =====================================================================
; Build Command Execution
; =====================================================================
ExecuteBuildCommand:
    push rbp
    mov rbp, rsp

    ; Create process
    sub rsp, 104

    ; Zero structures
    xor rax, rax
    lea rdi, [rsp]
    mov rcx, 26
    rep stosd

    ; Setup STARTUPINFO
    mov dword [rsp], 104
    mov dword [rsp + 60], 0x100  ; STARTF_USESTDHANDLES

    ; Setup command line
    mov rdx, rcx  ; Command string

    ; Call CreateProcessA
    lea rcx, [nullString]
    mov [rsp + 32], rcx  ; lpApplicationName
    mov [rsp + 40], rdx  ; lpCommandLine
    mov dword [rsp + 48], 0  ; dwCreationFlags
    lea r8, [rsp]        ; lpStartupInfo
    mov qword [rsp + 64], 0  ; lpProcessInformation

    call CreateProcessA

    test rax, rax
    jz .failed

    ; Get process handle
    mov rax, [rsp + 64]
    mov [hBuildProcess], rax

    ; Wait for completion
    mov rcx, [hBuildProcess]
    mov rdx, 0xFFFFFFFF
    call WaitForSingleObject

    ; Get exit code
    mov rcx, [hBuildProcess]
    lea rdx, [dwBuildResult]
    call GetExitCodeProcess

    ; Close handles
    mov rcx, [hBuildProcess]
    call CloseHandle

    mov rcx, [rsp + 72]  ; Thread handle
    call CloseHandle

    ; Return result
    mov eax, [dwBuildResult]
    test eax, eax
    jz .success

    xor eax, eax
    jmp .done

.success:
    mov eax, 1

.done:
    add rsp, 104
    pop rbp
    ret

.failed:
    xor eax, eax
    add rsp, 104
    pop rbp
    ret

; =====================================================================
; Show Build With NASM Menu
; =====================================================================
ShowBuildWithNasmMenu:
    push rbp
    mov rbp, rsp

    ; Create submenu for NASM builds
    call CreateMenu
    mov rbx, rax

    ; Add NASM build options
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_BUILD_WITH_NASM_EXE
    lea rax, [szBuildWithNasmExe]
    mov [rsp + 32], rax
    call AppendMenuA

    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_BUILD_WITH_NASM_DLL
    lea rax, [szBuildWithNasmDll]
    mov [rsp + 32], rax
    call AppendMenuA

    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 0x0000
    mov r9d, ID_BUILD_WITH_NASM_OBJ
    lea rax, [szBuildWithNasmObj]
    mov [rsp + 32], rax
    call AppendMenuA

    ; Show popup menu
    mov rcx, [hWnd]
    mov rdx, 0x0100  ; TPM_LEFTALIGN | TPM_TOPALIGN
    mov r8d, 0       ; x
    mov r9d, 0       ; y
    mov [rsp + 32], rbx  ; hMenu
    call TrackPopupMenu

    pop rbp
    ret

; =====================================================================
; Status Bar Updates
; =====================================================================

UpdateStatusBuilding:
    push rbp
    mov rbp, rsp

    ; Update status text
    mov rcx, [hStatus]
    xor rdx, rdx
    mov r8d, 0
    lea r9, [szBuilding]
    call SendMessageA

    pop rbp
    ret

UpdateStatusBuildSuccess:
    push rbp
    mov rbp, rsp

    mov rcx, [hStatus]
    xor rdx, rdx
    mov r8d, 0
    lea r9, [szBuildSuccess]
    call SendMessageA

    pop rbp
    ret

UpdateStatusBuildFailed:
    push rbp
    mov rbp, rsp

    mov rcx, [hStatus]
    xor rdx, rdx
    mov r8d, 0
    lea r9, [szBuildFailed]
    call SendMessageA

    pop rbp
    ret

; =====================================================================
; Build with NASM Functions
; =====================================================================

BuildWithNASMExe:
    push rbp
    mov rbp, rsp

    ; Execute NASM build for executable
    lea rcx, [nasmExeCommand]
    call ExecuteBuildCommand

    pop rbp
    ret

BuildWithNASMDll:
    push rbp
    mov rbp, rsp

    ; Execute NASM build for DLL
    lea rcx, [nasmDllCommand]
    call ExecuteBuildCommand

    pop rbp
    ret

BuildWithNASMObj:
    push rbp
    mov rbp, rsp

    ; Execute NASM build for object file
    lea rcx, [nasmObjCommand]
    call ExecuteBuildCommand

    pop rbp
    ret

nasmExeCommand db "nasm -f win64 -o output.obj source.asm && gcc output.obj -o output.exe -lkernel32 -luser32", 0
nasmDllCommand db "nasm -f win64 -o output.obj source.asm && gcc -shared output.obj -o output.dll", 0
nasmObjCommand db "nasm -f win64 -o output.obj source.asm", 0

; =====================================================================
; Toolchain Detection Functions
; =====================================================================

DetectAvailableTools:
    push rbp
    mov rbp, rsp

    ; Check NASM
    call DetectNASM

    ; Check GCC
    call DetectGCC

    ; Check YASM
    call DetectYASM

    pop rbp
    ret

DetectNASM:
    push rbp
    mov rbp, rsp

    ; Try to execute nasm --version
    sub rsp, 104

    xor rax, rax
    lea rdi, [rsp]
    mov rcx, 26
    rep stosd

    mov dword [rsp], 104
    mov dword [rsp + 60], 0x100

    lea rcx, [nasmVersionCmd]
    lea rdx, [rsp]

    call CreateProcessA

    test rax, rax
    jz .not_found

    ; Wait and check exit code
    mov rcx, [rsp + 64]
    mov rdx, 0xFFFFFFFF
    call WaitForSingleObject

    mov rcx, [rsp + 64]
    lea rdx, [exitCode]
    call GetExitCodeProcess

    mov eax, [exitCode]
    test eax, eax
    jz .found

.not_found:
    ; NASM not available
    mov byte [nasmAvailable], 0

.found:
    ; NASM available
    mov byte [nasmAvailable], 1

    ; Close handles
    mov rcx, [rsp + 64]
    call CloseHandle
    mov rcx, [rsp + 72]
    call CloseHandle

    add rsp, 104
    pop rbp
    ret

nasmVersionCmd db "nasm --version", 0
nasmAvailable db 0

DetectGCC:
    push rbp
    mov rbp, rsp

    ; Similar to DetectNASM
    ; Implementation would check for gcc --version

    pop rbp
    ret

DetectYASM:
    push rbp
    mov rbp, rsp

    ; Similar to DetectNASM
    ; Implementation would check for yasm --version

    pop rbp
    ret

; =====================================================================
; Windows API Integration Functions
; =====================================================================

CreatePipe:
    push rbp
    mov rbp, rsp

    ; Create pipe for process communication
    sub rsp, 32

    ; Setup SECURITY_ATTRIBUTES
    mov dword [rsp], 12
    mov qword [rsp + 8], 0

    lea rcx, [rsp]        ; lpPipeAttributes
    lea rdx, [hReadPipe]  ; hReadPipe
    lea r8, [hWritePipe]  ; hWritePipe
    mov r9d, 0            ; nSize

    call CreatePipe

    test rax, rax
    jz .failed

    mov eax, 1
    jmp .done

.failed:
    xor eax, eax

.done:
    add rsp, 32
    pop rbp
    ret

ReadPipeOutput:
    push rbp
    mov rbp, rsp

    ; Read from pipe
    sub rsp, 32

    lea rcx, [hReadPipe]
    lea rdx, [pipeBuffer]
    mov r8d, 1024
    lea r9, [bytesRead]
    mov qword [rsp + 32], 0

    call ReadFile

    test rax, rax
    jz .failed

    ; Null terminate buffer
    mov rcx, [bytesRead]
    mov byte [pipeBuffer + rcx], 0

    mov eax, 1
    jmp .done

.failed:
    xor eax, eax

.done:
    add rsp, 32
    pop rbp
    ret

; Pipe variables
hReadPipe   dq 0
hWritePipe  dq 0
bytesRead   dd 0
pipeBuffer  db 1024 dup(0)

; =====================================================================
; IDE Menu Integration Functions
; =====================================================================

UpdateBuildMenu:
    push rbp
    mov rbp, rsp

    ; Enable/disable menu items based on tool availability
    call GetMenu
    mov [hMenu], rax

    ; Check NASM availability
    cmp byte [nasmAvailable], 1
    je .nasm_available

    ; Disable NASM-related menu items
    mov rcx, [hMenu]
    mov rdx, ID_BUILD_DEBUG
    mov r8d, 0x00000001  ; MF_BYCOMMAND | MF_GRAYED
    call EnableMenuItem

    jmp .gcc_check

.nasm_available:
    ; Enable NASM-related menu items
    mov rcx, [hMenu]
    mov rdx, ID_BUILD_DEBUG
    mov r8d, 0x00000000  ; MF_BYCOMMAND | MF_ENABLED
    call EnableMenuItem

.gcc_check:
    ; Check GCC availability (similar logic)

    pop rbp
    ret

; =====================================================================
; Configuration Management
; =====================================================================

LoadBuildConfig:
    push rbp
    mov rbp, rsp

    ; Load configuration from file
    lea rcx, [configFileName]
    call LoadConfigFile

    pop rbp
    ret

SaveBuildConfig:
    push rbp
    mov rbp, rsp

    ; Save current configuration
    lea rcx, [configFileName]
    call SaveConfigFile

    pop rbp
    ret

configFileName db "nasm-ide.cfg", 0

; =====================================================================
; Professional Development Tools
; =====================================================================

ShowAssemblyInfo:
    push rbp
    mov rbp, rsp

    ; Display assembly information
    lea rcx, [szWindowTitle]
    lea rdx, [infoText]
    xor r8, r8
    call MessageBoxA

    pop rbp
    ret

infoText db "Professional NASM IDE", 13, 10
         db "Build System Integration", 13, 10
         db "Windows API Support", 13, 10
         db "Multiple Toolchains", 0

ShowBuildLog:
    push rbp
    mov rbp, rsp

    ; Display build log
    lea rcx, [buildLogText]
    call DisplayTextFile

    pop rbp
    ret

buildLogText db "build.log", 0

; =====================================================================
; Advanced Build Features
; =====================================================================

BuildWithMultipleThreads:
    push rbp
    mov rbp, rsp

    ; Multi-threaded build (simplified)
    ; Real implementation would use CreateThread

    pop rbp
    ret

BuildWithDependencyTracking:
    push rbp
    mov rbp, rsp

    ; Build with dependency analysis
    call AnalyzeDependencies
    call BuildDependencyTree
    call ExecuteParallelBuild

    pop rbp
    ret

; =====================================================================
; Cross-Platform Support
; =====================================================================

DetectPlatform:
    push rbp
    mov rbp, rsp

    ; Detect target platform
    mov eax, [platformInfo]
    and eax, 0xFF

    cmp eax, 1
    je .windows
    cmp eax, 2
    je .linux
    cmp eax, 3
    je .macos

    ; Default to Windows
.windows:
    mov dword [currentPlatform], 1
    jmp .done

.linux:
    mov dword [currentPlatform], 2
    jmp .done

.macos:
    mov dword [currentPlatform], 3

.done:
    pop rbp
    ret

platformInfo dd 1  ; 1=Windows, 2=Linux, 3=macOS
currentPlatform dd 1

; =====================================================================
; Error Handling and Recovery
; =====================================================================

HandleBuildError:
    push rbp
    mov rbp, rsp

    ; Display error dialog
    lea rcx, [szError]
    lea rdx, [szBuildError]
    mov r8d, 0x10  ; MB_ICONERROR
    call MessageBoxA

    ; Log error
    call LogBuildError

    ; Attempt recovery
    call AttemptBuildRecovery

    pop rbp
    ret

LogBuildError:
    push rbp
    mov rbp, rsp

    ; Write error to log file
    lea rcx, [errorLogFile]
    lea rdx, [errorBuffer]
    call WriteToFile

    pop rbp
    ret

errorLogFile db "build_errors.log", 0

AttemptBuildRecovery:
    push rbp
    mov rbp, rsp

    ; Try alternative build methods
    call TryAlternativeToolchain
    call TryAlternativeOptions

    pop rbp
    ret

; =====================================================================
; External Dependencies
; =====================================================================
extern ExitProcess
extern TrackPopupMenu
extern CreatePipe
extern ReadFile
extern EnableMenuItem
extern GetMenu
extern LoadConfigFile
extern SaveConfigFile
extern DisplayTextFile
extern WriteToFile
extern AnalyzeDependencies
extern BuildDependencyTree
extern ExecuteParallelBuild
extern TryAlternativeToolchain
extern TryAlternativeOptions
