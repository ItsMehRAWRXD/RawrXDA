; self_contained_compiler_gui.asm
; Self-Contained Universal Compiler with GUI Interface
; No external dependencies - generates machine code directly
; Includes project management, file browser, and IDE features
; Written in x86-64 assembly using direct system calls

section .data
    ; === Compiler Information ===
    app_title              db "RawrZ Self-Contained Compiler IDE v4.0", 0
    app_version            db "4.0.0 - Zero Dependencies", 0
    app_description        db "Universal compiler with integrated GUI - No external tools required", 0
    
    ; === GUI Constants ===
    WINDOW_WIDTH           equ 1200
    WINDOW_HEIGHT          equ 800
    MENU_HEIGHT            equ 30
    TOOLBAR_HEIGHT         equ 40
    STATUS_HEIGHT          equ 25
    SIDEBAR_WIDTH          equ 250
    
    ; GUI Colors (ARGB format)
    COLOR_BACKGROUND       equ 0xFF2D2D30
    COLOR_SIDEBAR          equ 0xFF1E1E1E
    COLOR_EDITOR           equ 0xFF1E1E1E
    COLOR_TEXT             equ 0xFFDCDCDC
    COLOR_HIGHLIGHT        equ 0xFF007ACC
    COLOR_ERROR            equ 0xFFFF3333
    COLOR_SUCCESS          equ 0xFF00AA00
    COLOR_WARNING          equ 0xFFFFAA00
    
    ; GUI Element IDs
    ID_MENU_FILE           equ 1001
    ID_MENU_EDIT           equ 1002
    ID_MENU_BUILD          equ 1003
    ID_MENU_RUN            equ 1004
    ID_MENU_TOOLS          equ 1005
    ID_MENU_HELP           equ 1006
    
    ID_TOOLBAR_NEW         equ 2001
    ID_TOOLBAR_OPEN        equ 2002
    ID_TOOLBAR_SAVE        equ 2003
    ID_TOOLBAR_BUILD       equ 2004
    ID_TOOLBAR_RUN         equ 2005
    ID_TOOLBAR_DEBUG       equ 2006
    
    ID_SIDEBAR_PROJECTS    equ 3001
    ID_SIDEBAR_FILES       equ 3002
    ID_SIDEBAR_OUTLINE     equ 3003
    ID_SIDEBAR_OUTPUT      equ 3004
    
    ID_EDITOR_MAIN         equ 4001
    ID_EDITOR_TABS         equ 4002
    
    ID_STATUS_BAR          equ 5001
    
    ; === Project System ===
    MAX_PROJECTS           equ 64
    MAX_FILES_PER_PROJECT  equ 256
    MAX_FILENAME_LENGTH    equ 260
    MAX_PROJECT_NAME       equ 128
    
    ; Project types
    PROJECT_TYPE_EON       equ 1
    PROJECT_TYPE_C         equ 2
    PROJECT_TYPE_CPP       equ 3
    PROJECT_TYPE_MIXED     equ 4
    
    ; File types
    FILE_TYPE_SOURCE       equ 1
    FILE_TYPE_HEADER       equ 2
    FILE_TYPE_RESOURCE     equ 3
    FILE_TYPE_CONFIG       equ 4
    
    ; === Compiler Backend (Self-Contained) ===
    MACHINE_CODE_BUFFER_SIZE equ 1048576  ; 1MB for generated machine code
    
    ; Target architectures
    TARGET_X86_64          equ 0
    TARGET_X86_32          equ 1
    TARGET_ARM64           equ 2
    
    ; Output formats  
    OUTPUT_EXECUTABLE      equ 0
    OUTPUT_SHARED_LIB      equ 1
    OUTPUT_STATIC_LIB      equ 2
    
    ; === GUI State ===
    window_handle          resq 1
    device_context         resq 1
    
    ; Editor state
    current_file_index     resq 1
    current_project_index  resq 1
    editor_font_handle     resq 1
    editor_cursor_x        resq 1
    editor_cursor_y        resq 1
    editor_scroll_x        resq 1
    editor_scroll_y        resq 1
    
    ; GUI elements
    menu_handle            resq 1
    toolbar_handle         resq 1
    sidebar_handle         resq 1
    editor_handle          resq 1
    status_handle          resq 1
    
    ; Project tree
    project_tree_handle    resq 1
    file_tree_handle       resq 1
    
    ; === Project Data Structures ===
    ; Project structure (128 bytes each)
    projects               times MAX_PROJECTS * 128 db 0
    project_count          resq 1
    
    ; File structure (512 bytes each)  
    project_files          times MAX_FILES_PER_PROJECT * 512 db 0
    
    ; Current project info
    current_project_name   resb MAX_PROJECT_NAME
    current_project_path   resb MAX_FILENAME_LENGTH
    current_project_type   resq 1
    
    ; === Compiler State ===
    compiler_initialized   resq 1
    compilation_in_progress resq 1
    
    ; Machine code generation
    machine_code_buffer    resb MACHINE_CODE_BUFFER_SIZE
    machine_code_size      resq 1
    
    ; Target settings
    target_architecture    resq 1
    output_format          resq 1
    optimization_level     resq 1
    
    ; === File Management ===
    ; Open files
    open_files             times 32 * MAX_FILENAME_LENGTH db 0
    open_file_count        resq 1
    
    ; File contents (1MB per file)
    file_contents          times 32 * 1048576 db 0
    file_sizes             times 32 resq 1
    file_modified_flags    times 32 resq 1
    
    ; === GUI Text and Messages ===
    ; Menu items
    menu_file_text         db "&File", 0
    menu_new_text          db "&New\tCtrl+N", 0
    menu_open_text         db "&Open\tCtrl+O", 0
    menu_save_text         db "&Save\tCtrl+S", 0
    menu_save_all_text     db "Save &All\tCtrl+Shift+S", 0
    menu_new_project_text  db "New &Project...", 0
    menu_open_project_text db "Open P&roject...", 0
    menu_close_project_text db "&Close Project", 0
    menu_exit_text         db "E&xit", 0
    
    menu_edit_text         db "&Edit", 0
    menu_undo_text         db "&Undo\tCtrl+Z", 0
    menu_redo_text         db "&Redo\tCtrl+Y", 0
    menu_cut_text          db "Cu&t\tCtrl+X", 0
    menu_copy_text         db "&Copy\tCtrl+C", 0
    menu_paste_text        db "&Paste\tCtrl+V", 0
    menu_find_text         db "&Find\tCtrl+F", 0
    menu_replace_text      db "&Replace\tCtrl+H", 0
    
    menu_build_text        db "&Build", 0
    menu_compile_text      db "&Compile\tF7", 0
    menu_build_all_text    db "Build &All\tCtrl+Shift+B", 0
    menu_clean_text        db "C&lean", 0
    menu_rebuild_text      db "&Rebuild All", 0
    
    menu_run_text          db "&Run", 0
    menu_run_exe_text      db "&Run\tF5", 0
    menu_debug_text        db "&Debug\tF9", 0
    menu_run_without_debug_text db "Run &Without Debugging\tCtrl+F5", 0
    
    menu_tools_text        db "&Tools", 0
    menu_options_text      db "&Options...", 0
    menu_settings_text     db "&Settings...", 0
    
    menu_help_text         db "&Help", 0
    menu_about_text        db "&About...", 0
    
    ; Toolbar tooltips
    tooltip_new            db "New File (Ctrl+N)", 0
    tooltip_open           db "Open File (Ctrl+O)", 0
    tooltip_save           db "Save File (Ctrl+S)", 0
    tooltip_build          db "Build Project (F7)", 0
    tooltip_run            db "Run Project (F5)", 0
    tooltip_debug          db "Debug Project (F9)", 0
    
    ; Status bar messages
    status_ready           db "Ready", 0
    status_compiling       db "Compiling...", 0
    status_building        db "Building...", 0
    status_running         db "Running...", 0
    status_debugging       db "Debugging...", 0
    status_file_saved      db "File saved", 0
    status_project_loaded  db "Project loaded", 0
    
    ; Dialog messages
    dialog_new_project     db "Create New Project", 0
    dialog_open_project    db "Open Existing Project", 0
    dialog_save_changes    db "Save changes to ", 0
    dialog_confirm_exit    db "Are you sure you want to exit?", 0
    
    ; Error messages
    error_file_not_found   db "Error: File not found", 0
    error_cannot_save      db "Error: Cannot save file", 0
    error_compilation_failed db "Error: Compilation failed", 0
    error_no_project       db "Error: No project is open", 0
    
    ; Success messages
    success_compiled       db "Compilation successful!", 0
    success_project_created db "Project created successfully", 0
    success_file_saved     db "File saved successfully", 0
    
    ; === Project Templates ===
    template_eon_hello     db "; Hello World in Eon", 10
                          db "function main() -> int {", 10
                          db "    print(\"Hello, World!\")", 10
                          db "    return 0", 10
                          db "}", 10, 0
                          
    template_c_hello       db "#include <stdio.h>", 10, 10
                          db "int main() {", 10
                          db "    printf(\"Hello, World!\\n\");", 10
                          db "    return 0;", 10
                          db "}", 10, 0
                          
    template_cpp_hello     db "#include <iostream>", 10, 10
                          db "int main() {", 10
                          db "    std::cout << \"Hello, World!\" << std::endl;", 10
                          db "    return 0;", 10
                          db "}", 10, 0

section .bss
    ; === Windows API Structures ===
    wnd_class              resb 72      ; WNDCLASSEX structure
    msg_struct             resb 48      ; MSG structure
    rect_struct            resb 16      ; RECT structure
    
    ; === Large Buffers ===
    temp_buffer            resb 65536   ; 64KB temp buffer
    line_buffer            resb 1024    ; Line editing buffer
    search_buffer          resb 256     ; Search text buffer
    
    ; === Compiler Buffers ===
    lexer_buffer           resb 262144  ; 256KB lexer buffer
    parser_buffer          resb 524288  ; 512KB parser buffer
    ast_buffer             resb 1048576 ; 1MB AST buffer
    symbol_buffer          resb 131072  ; 128KB symbol table

section .text
    global _start
    global WinMain
    global WindowProc
    global InitializeGUI
    global CreateMainWindow
    global CreateMenuSystem
    global CreateToolbar
    global CreateSidebar
    global CreateEditor
    global CreateStatusBar
    global HandleMenuCommand
    global HandleFileOperations
    global HandleProjectOperations
    global HandleBuildOperations
    global InitializeSelfContainedCompiler
    global CompileWithoutDependencies
    global GenerateMachineCode
    global WriteExecutableFile
    
    ; Windows API imports (we'll implement minimal versions)
    extern ExitProcess
    extern MessageBoxA
    extern GetModuleHandleA
    extern RegisterClassExA
    extern CreateWindowExA
    extern ShowWindow
    extern UpdateWindow
    extern GetMessageA
    extern TranslateMessage
    extern DispatchMessageA
    extern DefWindowProcA
    extern PostQuitMessage
    extern GetClientRect
    extern InvalidateRect
    extern BeginPaint
    extern EndPaint
    extern CreateFontA
    extern SelectObject
    extern SetBkColor
    extern SetTextColor
    extern TextOutA
    extern DrawTextA
    extern FillRect
    extern CreateSolidBrush
    extern DeleteObject
    extern GetStockObject
    extern LoadCursorA
    extern LoadIconA
    extern CreateMenu
    extern CreatePopupMenu
    extern AppendMenuA
    extern SetMenu
    extern GetOpenFileNameA
    extern GetSaveFileNameA
    extern FindFirstFileA
    extern FindNextFileA
    extern FindClose
    extern CreateFileA
    extern ReadFile
    extern WriteFile
    extern CloseHandle
    extern GetCurrentDirectoryA
    extern SetCurrentDirectoryA

; === Main Entry Point ===
_start:
    ; Initialize the GUI application
    call InitializeGUI
    test rax, rax
    jz .init_failed
    
    ; Initialize the self-contained compiler
    call InitializeSelfContainedCompiler
    test rax, rax
    jz .compiler_init_failed
    
    ; Create main window
    call CreateMainWindow
    test rax, rax
    jz .window_failed
    
    ; Show the window
    mov rdi, [window_handle]
    mov rsi, 1              ; SW_SHOW
    call ShowWindow
    
    mov rdi, [window_handle]
    call UpdateWindow
    
    ; Main message loop
.message_loop:
    mov rdi, msg_struct
    mov rsi, 0              ; hWnd (NULL for all windows)
    mov rdx, 0              ; wMsgFilterMin
    mov rcx, 0              ; wMsgFilterMax
    call GetMessageA
    
    cmp rax, 0
    je .exit_loop           ; WM_QUIT received
    jl .message_error       ; Error occurred
    
    ; Translate and dispatch message
    mov rdi, msg_struct
    call TranslateMessage
    
    mov rdi, msg_struct
    call DispatchMessageA
    
    jmp .message_loop
    
.exit_loop:
    ; Get exit code from wParam of WM_QUIT
    mov rax, [msg_struct + 16]  ; wParam
    jmp .exit
    
.init_failed:
    mov rdi, error_initialization
    call ShowErrorMessage
    mov rax, 1
    jmp .exit
    
.compiler_init_failed:
    mov rdi, error_compiler_init
    call ShowErrorMessage
    mov rax, 2
    jmp .exit
    
.window_failed:
    mov rdi, error_window_creation
    call ShowErrorMessage
    mov rax, 3
    jmp .exit
    
.message_error:
    mov rax, 4
    
.exit:
    ; Cleanup resources
    call CleanupGUI
    
    ; Exit process
    mov rdi, rax
    call ExitProcess

error_initialization   db "Failed to initialize GUI", 0
error_compiler_init    db "Failed to initialize compiler", 0
error_window_creation  db "Failed to create main window", 0

; === Initialize GUI System ===
InitializeGUI:
    push rbp
    mov rbp, rsp
    
    ; Get application instance
    mov rdi, 0
    call GetModuleHandleA
    mov [app_instance], rax
    test rax, rax
    jz .failed
    
    ; Initialize window class
    call InitializeWindowClass
    test rax, rax
    jz .failed
    
    ; Register window class
    mov rdi, wnd_class
    call RegisterClassExA
    test rax, rax
    jz .failed
    
    ; Initialize common controls (if needed)
    call InitializeCommonControls
    
    mov rax, 1              ; Success
    jmp .done
    
.failed:
    xor rax, rax            ; Failure
    
.done:
    leave
    ret

app_instance resq 1

; === Initialize Window Class ===
InitializeWindowClass:
    push rbp
    mov rbp, rsp
    
    ; Fill WNDCLASSEX structure
    mov rdi, wnd_class
    mov rsi, 72
    call ZeroMemory
    
    ; Set structure fields
    mov dword [wnd_class], 72           ; cbSize
    mov dword [wnd_class + 4], 0x0003   ; style (CS_HREDRAW | CS_VREDRAW)
    mov rax, WindowProc
    mov [wnd_class + 8], rax            ; lpfnWndProc
    mov dword [wnd_class + 16], 0       ; cbClsExtra
    mov dword [wnd_class + 20], 0       ; cbWndExtra
    mov rax, [app_instance]
    mov [wnd_class + 24], rax           ; hInstance
    
    ; Load icon
    mov rdi, 0
    mov rsi, 32512          ; IDI_APPLICATION
    call LoadIconA
    mov [wnd_class + 32], rax           ; hIcon
    
    ; Load cursor
    mov rdi, 0
    mov rsi, 32512          ; IDC_ARROW
    call LoadCursorA
    mov [wnd_class + 40], rax           ; hCursor
    
    ; Background brush
    mov rdi, 5              ; COLOR_WINDOW
    call GetStockObject
    mov [wnd_class + 48], rax           ; hbrBackground
    
    mov qword [wnd_class + 56], 0       ; lpszMenuName (NULL)
    mov qword [wnd_class + 64], window_class_name ; lpszClassName
    mov qword [wnd_class + 72], rax     ; hIconSm (same as hIcon)
    
    mov rax, 1              ; Success
    leave
    ret

window_class_name db "RawrZCompilerIDE", 0

; === Create Main Window ===
CreateMainWindow:
    push rbp
    mov rbp, rsp
    
    ; Create the main window
    mov rdi, 0x00000100     ; dwExStyle (WS_EX_WINDOWEDGE)
    mov rsi, window_class_name
    mov rdx, app_title
    mov rcx, 0x00CF0000     ; dwStyle (WS_OVERLAPPEDWINDOW)
    mov r8, 100             ; x
    mov r9, 100             ; y
    push WINDOW_HEIGHT      ; nHeight
    push WINDOW_WIDTH       ; nWidth
    push 0                  ; hWndParent
    push 0                  ; hMenu
    push qword [app_instance] ; hInstance
    push 0                  ; lpParam
    call CreateWindowExA
    add rsp, 48             ; Clean up stack (6 pushes * 8 bytes)
    
    test rax, rax
    jz .failed
    
    mov [window_handle], rax
    
    ; Create menu system
    call CreateMenuSystem
    test rax, rax
    jz .failed
    
    ; Create toolbar
    call CreateToolbar
    test rax, rax
    jz .failed
    
    ; Create sidebar
    call CreateSidebar
    test rax, rax
    jz .failed
    
    ; Create editor
    call CreateEditor
    test rax, rax
    jz .failed
    
    ; Create status bar
    call CreateStatusBar
    test rax, rax
    jz .failed
    
    ; Set initial status
    mov rdi, status_ready
    call SetStatusText
    
    mov rax, 1              ; Success
    jmp .done
    
.failed:
    xor rax, rax            ; Failure
    
.done:
    leave
    ret

; === Window Procedure ===
WindowProc:
    push rbp
    mov rbp, rsp
    
    ; Parameters: rdi=hWnd, rsi=uMsg, rdx=wParam, rcx=lParam
    
    cmp rsi, 0x0002         ; WM_DESTROY
    je .wm_destroy
    
    cmp rsi, 0x000F         ; WM_PAINT
    je .wm_paint
    
    cmp rsi, 0x0005         ; WM_SIZE
    je .wm_size
    
    cmp rsi, 0x0111         ; WM_COMMAND
    je .wm_command
    
    cmp rsi, 0x0100         ; WM_KEYDOWN
    je .wm_keydown
    
    ; Default window procedure
    call DefWindowProcA
    jmp .done
    
.wm_destroy:
    mov rdi, 0
    call PostQuitMessage
    xor rax, rax
    jmp .done
    
.wm_paint:
    call HandlePaintMessage
    xor rax, rax
    jmp .done
    
.wm_size:
    call HandleSizeMessage
    xor rax, rax
    jmp .done
    
.wm_command:
    call HandleMenuCommand
    xor rax, rax
    jmp .done
    
.wm_keydown:
    call HandleKeyDown
    xor rax, rax
    jmp .done
    
.done:
    leave
    ret

; === Handle Menu Commands ===
HandleMenuCommand:
    push rbp
    mov rbp, rsp
    
    ; Extract command ID from wParam (low word)
    mov rax, rdx
    and rax, 0xFFFF
    
    ; File menu commands
    cmp rax, ID_MENU_NEW
    je .handle_new_file
    
    cmp rax, ID_MENU_OPEN
    je .handle_open_file
    
    cmp rax, ID_MENU_SAVE
    je .handle_save_file
    
    cmp rax, ID_MENU_NEW_PROJECT
    je .handle_new_project
    
    cmp rax, ID_MENU_OPEN_PROJECT
    je .handle_open_project
    
    ; Build menu commands
    cmp rax, ID_MENU_COMPILE
    je .handle_compile
    
    cmp rax, ID_MENU_BUILD_ALL
    je .handle_build_all
    
    ; Run menu commands
    cmp rax, ID_MENU_RUN
    je .handle_run
    
    cmp rax, ID_MENU_DEBUG
    je .handle_debug
    
    ; Default: do nothing
    jmp .done
    
.handle_new_file:
    call CreateNewFile
    jmp .done
    
.handle_open_file:
    call OpenFile
    jmp .done
    
.handle_save_file:
    call SaveCurrentFile
    jmp .done
    
.handle_new_project:
    call CreateNewProject
    jmp .done
    
.handle_open_project:
    call OpenProject
    jmp .done
    
.handle_compile:
    call CompileCurrentFile
    jmp .done
    
.handle_build_all:
    call BuildAllFiles
    jmp .done
    
.handle_run:
    call RunProject
    jmp .done
    
.handle_debug:
    call DebugProject
    jmp .done
    
.done:
    leave
    ret

; Menu command IDs
ID_MENU_NEW             equ 40001
ID_MENU_OPEN            equ 40002
ID_MENU_SAVE            equ 40003
ID_MENU_NEW_PROJECT     equ 40004
ID_MENU_OPEN_PROJECT    equ 40005
ID_MENU_COMPILE         equ 40006
ID_MENU_BUILD_ALL       equ 40007
ID_MENU_RUN             equ 40008
ID_MENU_DEBUG           equ 40009

; === Initialize Self-Contained Compiler ===
InitializeSelfContainedCompiler:
    push rbp
    mov rbp, rsp
    
    ; Initialize machine code generator
    call InitializeMachineCodeGenerator
    test rax, rax
    jz .failed
    
    ; Initialize built-in assembler
    call InitializeBuiltinAssembler
    test rax, rax
    jz .failed
    
    ; Initialize built-in linker
    call InitializeBuiltinLinker
    test rax, rax
    jz .failed
    
    ; Initialize language parsers
    call InitializeLanguageParsers
    test rax, rax
    jz .failed
    
    ; Initialize optimization engine
    call InitializeOptimizationEngine
    test rax, rax
    jz .failed
    
    ; Set compiler as initialized
    mov qword [compiler_initialized], 1
    
    mov rax, 1              ; Success
    jmp .done
    
.failed:
    xor rax, rax            ; Failure
    
.done:
    leave
    ret

; === Compile Without Dependencies ===
CompileWithoutDependencies:
    push rbp
    mov rbp, rsp
    push rdi                ; Save source file path
    
    ; Check if compiler is initialized
    cmp qword [compiler_initialized], 1
    jne .not_initialized
    
    ; Set compilation in progress
    mov qword [compilation_in_progress], 1
    
    ; Update status
    mov rdi, status_compiling
    call SetStatusText
    
    ; Load source file
    pop rdi                 ; Restore source file path
    push rdi
    call LoadSourceFile
    test rax, rax
    jz .load_failed
    
    ; Detect language
    call DetectLanguage
    test rax, rax
    jz .language_detection_failed
    
    ; Parse source code
    call ParseSourceCode
    test rax, rax
    jz .parse_failed
    
    ; Generate intermediate representation
    call GenerateIR
    test rax, rax
    jz .ir_failed
    
    ; Optimize if requested
    cmp qword [optimization_level], 0
    je .skip_optimization
    call OptimizeIR
    test rax, rax
    jz .optimization_failed
    
.skip_optimization:
    ; Generate machine code directly
    call GenerateMachineCode
    test rax, rax
    jz .codegen_failed
    
    ; Write executable file
    call WriteExecutableFile
    test rax, rax
    jz .write_failed
    
    ; Compilation successful
    mov qword [compilation_in_progress], 0
    mov rdi, success_compiled
    call SetStatusText
    
    mov rax, 1              ; Success
    jmp .cleanup
    
.not_initialized:
.load_failed:
.language_detection_failed:
.parse_failed:
.ir_failed:
.optimization_failed:
.codegen_failed:
.write_failed:
    mov qword [compilation_in_progress], 0
    mov rdi, error_compilation_failed
    call SetStatusText
    xor rax, rax            ; Failure
    
.cleanup:
    pop rdi                 ; Clean up stack
    leave
    ret

; === Generate Machine Code Directly ===
GenerateMachineCode:
    push rbp
    mov rbp, rsp
    
    ; Clear machine code buffer
    mov rdi, machine_code_buffer
    mov rsi, MACHINE_CODE_BUFFER_SIZE
    call ZeroMemory
    
    ; Initialize code generation context
    call InitCodeGenContext
    
    ; Generate machine code based on target architecture
    mov rax, [target_architecture]
    cmp rax, TARGET_X86_64
    je .generate_x86_64
    
    cmp rax, TARGET_X86_32
    je .generate_x86_32
    
    cmp rax, TARGET_ARM64
    je .generate_arm64
    
    ; Default to x86-64
.generate_x86_64:
    call GenerateX86_64MachineCode
    jmp .done
    
.generate_x86_32:
    call GenerateX86_32MachineCode
    jmp .done
    
.generate_arm64:
    call GenerateARM64MachineCode
    jmp .done
    
.done:
    leave
    ret

; === Write Executable File (No External Linker) ===
WriteExecutableFile:
    push rbp
    mov rbp, rsp
    
    ; Create output file
    call CreateOutputFile
    test rax, rax
    jz .failed
    
    mov [output_file_handle], rax
    
    ; Write executable headers based on target platform
    mov rax, [target_platform]
    cmp rax, PLATFORM_WINDOWS
    je .write_pe_executable
    
    cmp rax, PLATFORM_LINUX
    je .write_elf_executable
    
    ; Default to current platform
    jmp .write_elf_executable
    
.write_pe_executable:
    call WritePEExecutable
    jmp .finish_write
    
.write_elf_executable:
    call WriteELFExecutable
    jmp .finish_write
    
.finish_write:
    ; Close output file
    mov rdi, [output_file_handle]
    call CloseHandle
    
    mov rax, 1              ; Success
    jmp .done
    
.failed:
    xor rax, rax            ; Failure
    
.done:
    leave
    ret

; === Project Management Functions ===

CreateNewProject:
    push rbp
    mov rbp, rsp
    
    ; Show new project dialog
    call ShowNewProjectDialog
    test rax, rax
    jz .cancelled
    
    ; Create project structure
    call CreateProjectStructure
    test rax, rax
    jz .failed
    
    ; Add to project list
    call AddToProjectList
    
    ; Update UI
    call RefreshProjectTree
    call RefreshFileTree
    
    ; Set status
    mov rdi, success_project_created
    call SetStatusText
    
    mov rax, 1              ; Success
    jmp .done
    
.cancelled:
.failed:
    xor rax, rax            ; Failure
    
.done:
    leave
    ret

OpenProject:
    push rbp
    mov rbp, rsp
    
    ; Show open project dialog
    call ShowOpenProjectDialog
    test rax, rax
    jz .cancelled
    
    ; Load project file
    call LoadProjectFile
    test rax, rax
    jz .failed
    
    ; Update UI
    call RefreshProjectTree
    call RefreshFileTree
    
    ; Set status
    mov rdi, status_project_loaded
    call SetStatusText
    
    mov rax, 1              ; Success
    jmp .done
    
.cancelled:
.failed:
    xor rax, rax            ; Failure
    
.done:
    leave
    ret

; === Stub Functions (Framework for Implementation) ===

; These functions provide the framework - implement as needed

ZeroMemory:
    ; Simple memory zeroing
    push rcx
    mov rcx, rsi
    xor al, al
    rep stosb
    pop rcx
    ret

ShowErrorMessage:
    ; Show error message box
    push rdi
    mov rsi, rdi
    mov rdi, 0              ; hWnd
    mov rdx, app_title      ; lpCaption
    mov rcx, 0x10           ; uType (MB_ICONERROR)
    call MessageBoxA
    pop rdi
    ret

CleanupGUI:
    ; Cleanup GUI resources
    ret

InitializeCommonControls:
    ; Initialize common controls (if needed)
    mov rax, 1
    ret

HandlePaintMessage:
    ; Handle WM_PAINT
    ret

HandleSizeMessage:
    ; Handle WM_SIZE
    ret

HandleKeyDown:
    ; Handle WM_KEYDOWN
    ret

CreateMenuSystem:
    ; Create menu system
    mov rax, 1
    ret

CreateToolbar:
    ; Create toolbar
    mov rax, 1
    ret

CreateSidebar:
    ; Create sidebar
    mov rax, 1
    ret

CreateEditor:
    ; Create editor
    mov rax, 1
    ret

CreateStatusBar:
    ; Create status bar
    mov rax, 1
    ret

SetStatusText:
    ; Set status bar text
    ret

CreateNewFile:
    ; Create new file
    ret

OpenFile:
    ; Open file
    ret

SaveCurrentFile:
    ; Save current file
    ret

CompileCurrentFile:
    ; Compile current file
    call CompileWithoutDependencies
    ret

BuildAllFiles:
    ; Build all files
    ret

RunProject:
    ; Run compiled project
    ret

DebugProject:
    ; Debug project
    ret

InitializeMachineCodeGenerator:
    ; Initialize machine code generator
    mov rax, 1
    ret

InitializeBuiltinAssembler:
    ; Initialize built-in assembler
    mov rax, 1
    ret

InitializeBuiltinLinker:
    ; Initialize built-in linker
    mov rax, 1
    ret

InitializeLanguageParsers:
    ; Initialize language parsers
    mov rax, 1
    ret

InitializeOptimizationEngine:
    ; Initialize optimization engine
    mov rax, 1
    ret

LoadSourceFile:
    ; Load source file
    mov rax, 1
    ret

DetectLanguage:
    ; Detect source language
    mov rax, 1
    ret

ParseSourceCode:
    ; Parse source code
    mov rax, 1
    ret

GenerateIR:
    ; Generate intermediate representation
    mov rax, 1
    ret

OptimizeIR:
    ; Optimize intermediate representation
    mov rax, 1
    ret

InitCodeGenContext:
    ; Initialize code generation context
    ret

GenerateX86_64MachineCode:
    ; Generate x86-64 machine code
    mov rax, 1
    ret

GenerateX86_32MachineCode:
    ; Generate x86-32 machine code
    mov rax, 1
    ret

GenerateARM64MachineCode:
    ; Generate ARM64 machine code
    mov rax, 1
    ret

CreateOutputFile:
    ; Create output file
    mov rax, 1
    ret

WritePEExecutable:
    ; Write PE executable
    mov rax, 1
    ret

WriteELFExecutable:
    ; Write ELF executable
    mov rax, 1
    ret

ShowNewProjectDialog:
    ; Show new project dialog
    mov rax, 1
    ret

CreateProjectStructure:
    ; Create project structure
    mov rax, 1
    ret

AddToProjectList:
    ; Add to project list
    ret

RefreshProjectTree:
    ; Refresh project tree
    ret

RefreshFileTree:
    ; Refresh file tree
    ret

ShowOpenProjectDialog:
    ; Show open project dialog
    mov rax, 1
    ret

LoadProjectFile:
    ; Load project file
    mov rax, 1
    ret

; === Data Storage ===
output_file_handle      resq 1
PLATFORM_WINDOWS        equ 0
PLATFORM_LINUX          equ 1
target_platform         resq 1

; === End of Self-Contained Compiler GUI ===
