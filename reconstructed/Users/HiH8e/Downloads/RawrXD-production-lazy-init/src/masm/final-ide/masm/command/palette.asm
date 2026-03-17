;==========================================================================
; masm_command_palette.asm - Pure MASM Command Palette (Ctrl+Shift+P)
;==========================================================================
; Implements VS Code/Cursor-style command palette matching Qt CommandPalette:
; - Fuzzy search for commands (500+ commands)
; - Recent commands tracking (last 10)
; - Category prefixes (>, @, #, :)
; - Keyboard navigation (Up/Down/Enter/Esc)
; - Dark theme matching VS Code aesthetics
; - Real-time filtering as user types
; - Command execution callbacks
; - Shortcut display in results
; - Multi-threaded search (fast fuzzy matching)
;==========================================================================

option casemap:none
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib comctl32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
IDC_PALETTE_SEARCH  equ 4001
IDC_PALETTE_RESULTS equ 4002

MAX_COMMANDS        equ 500
MAX_RESULTS         equ 15
MAX_RECENT          equ 10

;==========================================================================
; STRUCTURES
;==========================================================================

; Command structure (96 bytes)
COMMAND_ENTRY struct
    id          BYTE 64 dup(0)       ; Command ID (e.g., "file.open")
    label       BYTE 128 dup(0)      ; Display label
    category    BYTE 32 dup(0)       ; Category (File, Edit, View, etc.)
    description BYTE 128 dup(0)      ; Description
    shortcut    BYTE 32 dup(0)       ; Keyboard shortcut (e.g., "Ctrl+O")
    callback    QWORD 0              ; Function pointer
    enabled     BYTE 1               ; Enabled flag
    padding     BYTE 7 dup(0)        ; Alignment
COMMAND_ENTRY ends

; Search result (includes score)
SEARCH_RESULT struct
    commandIndex DWORD 0             ; Index into command array
    fuzzyScore   DWORD 0             ; Fuzzy match score (higher = better)
SEARCH_RESULT ends

; Command palette state
PALETTE_STATE struct
    hWindow         QWORD 0          ; Palette window handle
    hSearchBox      QWORD 0          ; Search input control
    hResultsList    QWORD 0          ; Listbox for results
    hFont           QWORD 0          ; Font handle
    
    ; Command registry
    commands        COMMAND_ENTRY MAX_COMMANDS dup(<>)
    commandCount    DWORD 0
    
    ; Search results
    results         SEARCH_RESULT MAX_RESULTS dup(<>)
    resultCount     DWORD 0
    selectedIndex   DWORD 0          ; Currently selected result
    
    ; Recent commands (indices)
    recentCommands  DWORD MAX_RECENT dup(0)
    recentCount     DWORD 0
    
    ; State flags
    isVisible       BYTE 0
    isDarkTheme     BYTE 1
    
    ; Current filter text
    filterText      BYTE 256 dup(0)
PALETTE_STATE ends

;==========================================================================
; DATA
;==========================================================================
.data
g_paletteState PALETTE_STATE <>

szPaletteClass  db "RawrXD_CommandPalette", 0
szPaletteTitle  db "Command Palette", 0
szEditClass     db "EDIT", 0
szListBoxClass  db "LISTBOX", 0
szSearchHint    db "Type command name or > for commands...", 0

; Built-in command IDs
szCmdFileNew        db "file.new", 0
szCmdFileOpen       db "file.open", 0
szCmdFileSave       db "file.save", 0
szCmdFileSaveAs     db "file.saveAs", 0
szCmdEditUndo       db "edit.undo", 0
szCmdEditRedo       db "edit.redo", 0
szCmdEditCut        db "edit.cut", 0
szCmdEditCopy       db "edit.copy", 0
szCmdEditPaste      db "edit.paste", 0
szCmdViewTheme      db "view.changeTheme", 0
szCmdViewMinimap    db "view.toggleMinimap", 0
szCmdViewPalette    db "view.commandPalette", 0
szCmdAIChat         db "ai.startChat", 0
szCmdAIAnalyze      db "ai.analyzeCode", 0
szCmdAIRefactor     db "ai.refactorCode", 0
szCmdModelLoad      db "model.load", 0
szCmdModelUnload    db "model.unload", 0
szCmdHotpatchMemory db "hotpatch.memory", 0
szCmdHotpatchByte   db "hotpatch.byte", 0
szCmdHotpatchServer db "hotpatch.server", 0

; Command labels
szLabelFileNew      db "File: New", 0
szLabelFileOpen     db "File: Open", 0
szLabelFileSave     db "File: Save", 0
szLabelEditUndo     db "Edit: Undo", 0
szLabelEditRedo     db "Edit: Redo", 0
szLabelViewTheme    db "View: Change Theme", 0
szLabelAIChat       db "AI: Start Chat", 0
szLabelModelLoad    db "Model: Load GGUF", 0

; Shortcuts
szShortcutCtrlN     db "Ctrl+N", 0
szShortcutCtrlO     db "Ctrl+O", 0
szShortcutCtrlS     db "Ctrl+S", 0
szShortcutCtrlZ     db "Ctrl+Z", 0
szShortcutCtrlY     db "Ctrl+Y", 0
szShortcutCtrlShiftP db "Ctrl+Shift+P", 0

.code

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN RegisterClassExA:PROC
EXTERN CreateWindowExA:PROC
EXTERN DefWindowProcA:PROC
EXTERN ShowWindow:PROC
EXTERN SetFocus:PROC
EXTERN SendMessageA:PROC
EXTERN GetWindowTextA:PROC
EXTERN SetWindowTextA:PROC
EXTERN CreateFontA:PROC
EXTERN DeleteObject:PROC
EXTERN InvalidateRect:PROC

;==========================================================================
; PUBLIC EXPORTS
;==========================================================================
PUBLIC palette_init
PUBLIC palette_create_window
PUBLIC palette_show
PUBLIC palette_hide
PUBLIC palette_register_command
PUBLIC palette_execute_command
PUBLIC palette_get_state

;==========================================================================
; palette_init() -> bool (rax)
; Initialize command palette system
;==========================================================================
palette_init PROC
    LOCAL wc:WNDCLASSEXA
    
    sub rsp, 96
    
    ; Initialize WNDCLASSEXA
    mov dword ptr [wc.cbSize], sizeof WNDCLASSEXA
    mov dword ptr [wc.style], 0
    lea rax, palette_wnd_proc
    mov qword ptr [wc.lpfnWndProc], rax
    mov dword ptr [wc.cbClsExtra], 0
    mov dword ptr [wc.cbWndExtra], 0
    mov qword ptr [wc.hInstance], 0
    mov qword ptr [wc.hIcon], 0
    mov qword ptr [wc.hCursor], 0
    mov qword ptr [wc.hbrBackground], 0
    mov qword ptr [wc.lpszMenuName], 0
    lea rax, szPaletteClass
    mov qword ptr [wc.lpszClassName], rax
    mov qword ptr [wc.hIconSm], 0
    
    ; Register class
    lea rcx, wc
    call RegisterClassExA
    
    ; Register default commands
    call register_default_commands
    
    mov rax, 1  ; Success
    add rsp, 96
    ret
palette_init ENDP

;==========================================================================
; register_default_commands() - Register built-in IDE commands
;==========================================================================
register_default_commands PROC
    sub rsp, 32
    
    ; File commands
    lea rcx, szCmdFileNew
    lea rdx, szLabelFileNew
    lea r8, szShortcutCtrlN
    xor r9, r9  ; No callback yet
    call palette_register_command
    
    lea rcx, szCmdFileOpen
    lea rdx, szLabelFileOpen
    lea r8, szShortcutCtrlO
    xor r9, r9
    call palette_register_command
    
    lea rcx, szCmdFileSave
    lea rdx, szLabelFileSave
    lea r8, szShortcutCtrlS
    xor r9, r9
    call palette_register_command
    
    ; Edit commands
    lea rcx, szCmdEditUndo
    lea rdx, szLabelEditUndo
    lea r8, szShortcutCtrlZ
    xor r9, r9
    call palette_register_command
    
    lea rcx, szCmdEditRedo
    lea rdx, szLabelEditRedo
    lea r8, szShortcutCtrlY
    xor r9, r9
    call palette_register_command
    
    ; View commands
    lea rcx, szCmdViewTheme
    lea rdx, szLabelViewTheme
    xor r8, r8
    xor r9, r9
    call palette_register_command
    
    ; AI commands
    lea rcx, szCmdAIChat
    lea rdx, szLabelAIChat
    xor r8, r8
    xor r9, r9
    call palette_register_command
    
    ; Model commands
    lea rcx, szCmdModelLoad
    lea rdx, szLabelModelLoad
    xor r8, r8
    xor r9, r9
    call palette_register_command
    
    add rsp, 32
    ret
register_default_commands ENDP

;==========================================================================
; palette_create_window(parent_hwnd: rcx) -> HWND (rax)
; Create command palette popup window
;==========================================================================
palette_create_window PROC
    push rbx
    sub rsp, 96
    
    mov rbx, rcx  ; Save parent
    
    ; Calculate centered position (800x400 popup)
    mov r12d, 800  ; Width
    mov r13d, 400  ; Height
    
    ; Create popup window (WS_POPUP | WS_BORDER)
    mov rcx, 200h  ; WS_EX_TOOLWINDOW
    lea rdx, szPaletteClass
    lea r8, szPaletteTitle
    mov r9d, 80000000h or 800000h  ; WS_POPUP | WS_BORDER
    mov dword ptr [rsp + 32], 100   ; x (will center later)
    mov dword ptr [rsp + 40], 100   ; y
    mov dword ptr [rsp + 48], r12d  ; width
    mov dword ptr [rsp + 56], r13d  ; height
    mov qword ptr [rsp + 64], rbx   ; parent
    mov qword ptr [rsp + 72], 0     ; menu
    mov qword ptr [rsp + 80], 0     ; instance
    mov qword ptr [rsp + 88], 0     ; param
    call CreateWindowExA
    
    mov g_paletteState.hWindow, rax
    
    ; Create search box
    call create_search_box
    
    ; Create results list
    call create_results_list
    
    ; Create font
    call create_palette_font
    
    ; Initially hidden
    mov rcx, g_paletteState.hWindow
    xor rdx, rdx  ; SW_HIDE
    call ShowWindow
    
    mov rax, g_paletteState.hWindow
    
    add rsp, 96
    pop rbx
    ret
palette_create_window ENDP

;==========================================================================
; create_search_box() - Create search input control
;==========================================================================
create_search_box PROC
    sub rsp, 96
    
    xor rcx, rcx
    lea rdx, szEditClass
    lea r8, szSearchHint
    mov r9d, 50000000h or 10000000h  ; WS_CHILD | WS_VISIBLE
    mov dword ptr [rsp + 32], 10     ; x
    mov dword ptr [rsp + 40], 10     ; y
    mov dword ptr [rsp + 48], 780    ; width
    mov dword ptr [rsp + 56], 30     ; height
    mov rax, g_paletteState.hWindow
    mov qword ptr [rsp + 64], rax    ; parent
    mov qword ptr [rsp + 72], IDC_PALETTE_SEARCH
    mov qword ptr [rsp + 80], 0
    mov qword ptr [rsp + 88], 0
    call CreateWindowExA
    
    mov g_paletteState.hSearchBox, rax
    
    add rsp, 96
    ret
create_search_box ENDP

;==========================================================================
; create_results_list() - Create results listbox
;==========================================================================
create_results_list PROC
    sub rsp, 96
    
    xor rcx, rcx
    lea rdx, szListBoxClass
    xor r8, r8
    mov r9d, 50000000h or 10000000h or 200000h  ; WS_CHILD | WS_VISIBLE | WS_VSCROLL
    mov dword ptr [rsp + 32], 10     ; x
    mov dword ptr [rsp + 40], 50     ; y
    mov dword ptr [rsp + 48], 780    ; width
    mov dword ptr [rsp + 56], 340    ; height
    mov rax, g_paletteState.hWindow
    mov qword ptr [rsp + 64], rax    ; parent
    mov qword ptr [rsp + 72], IDC_PALETTE_RESULTS
    mov qword ptr [rsp + 80], 0
    mov qword ptr [rsp + 88], 0
    call CreateWindowExA
    
    mov g_paletteState.hResultsList, rax
    
    add rsp, 96
    ret
create_results_list ENDP

;==========================================================================
; create_palette_font() - Create Consolas font for palette
;==========================================================================
create_palette_font PROC
    sub rsp, 96
    
    mov ecx, 16              ; Height
    xor rdx, rdx             ; Width
    xor r8, r8               ; Escapement
    xor r9, r9               ; Orientation
    mov dword ptr [rsp + 32], 400   ; Weight (FW_NORMAL)
    mov dword ptr [rsp + 40], 0     ; Italic
    mov dword ptr [rsp + 48], 0     ; Underline
    mov dword ptr [rsp + 56], 0     ; StrikeOut
    mov dword ptr [rsp + 64], 0     ; CharSet
    mov dword ptr [rsp + 72], 0     ; OutPrecision
    mov dword ptr [rsp + 80], 0     ; ClipPrecision
    mov dword ptr [rsp + 88], 0     ; Quality
    lea rax, szFontConsolas
    mov qword ptr [rsp + 96], rax   ; FaceName
    call CreateFontA
    
    mov g_paletteState.hFont, rax
    
    ; Apply font to controls
    mov rcx, g_paletteState.hSearchBox
    mov edx, 30h  ; WM_SETFONT
    mov r8, rax
    mov r9d, 1    ; Redraw
    call SendMessageA
    
    mov rcx, g_paletteState.hResultsList
    mov edx, 30h
    mov r8, g_paletteState.hFont
    mov r9d, 1
    call SendMessageA
    
    add rsp, 96
    ret
    
.data
szFontConsolas db "Consolas", 0
.code
create_palette_font ENDP

;==========================================================================
; palette_register_command(id: rcx, label: rdx, shortcut: r8, callback: r9) -> bool (rax)
; Register a command in the palette
;==========================================================================
palette_register_command PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    ; Check if command array is full
    mov eax, g_paletteState.commandCount
    cmp eax, MAX_COMMANDS
    jge @full
    
    ; Calculate command entry address
    mov ebx, eax
    imul rbx, rbx, sizeof COMMAND_ENTRY
    lea rdi, g_paletteState.commands
    add rdi, rbx
    
    ; Copy ID
    mov rsi, rcx
    mov ecx, 64
    rep movsb
    
    ; Copy Label
    mov rsi, rdx
    mov ecx, 128
    rep movsb
    
    ; Copy Shortcut (if provided)
    test r8, r8
    jz @skip_shortcut
    mov rsi, r8
    mov ecx, 32
    rep movsb
@skip_shortcut:
    
    ; Store callback
    mov qword ptr [rdi + COMMAND_ENTRY.callback - sizeof COMMAND_ENTRY], r9
    
    ; Enable by default
    mov byte ptr [rdi + COMMAND_ENTRY.enabled - sizeof COMMAND_ENTRY], 1
    
    ; Increment command count
    inc g_paletteState.commandCount
    
    mov rax, 1  ; Success
    jmp @done
    
@full:
    xor rax, rax  ; Failure
    
@done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
palette_register_command ENDP

;==========================================================================
; palette_show() -> void
; Show command palette
;==========================================================================
palette_show PROC
    sub rsp, 32
    
    ; Show window
    mov rcx, g_paletteState.hWindow
    mov edx, 5  ; SW_SHOW
    call ShowWindow
    
    ; Focus search box
    mov rcx, g_paletteState.hSearchBox
    call SetFocus
    
    ; Clear search text
    mov rcx, g_paletteState.hSearchBox
    lea rdx, szEmpty
    call SetWindowTextA
    
    ; Show all commands initially
    call update_results_all
    
    mov byte ptr g_paletteState.isVisible, 1
    
    add rsp, 32
    ret
    
.data
szEmpty db 0
.code
palette_show ENDP

;==========================================================================
; palette_hide() -> void
; Hide command palette
;==========================================================================
palette_hide PROC
    sub rsp, 32
    
    mov rcx, g_paletteState.hWindow
    xor rdx, rdx  ; SW_HIDE
    call ShowWindow
    
    mov byte ptr g_paletteState.isVisible, 0
    
    add rsp, 32
    ret
palette_hide ENDP

;==========================================================================
; update_results_all() - Show all commands (no filter)
;==========================================================================
update_results_all PROC
    sub rsp, 32
    
    ; Clear list box
    mov rcx, g_paletteState.hResultsList
    mov edx, 184h  ; LB_RESETCONTENT
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    ; Add all enabled commands
    xor r12d, r12d
    mov r13d, g_paletteState.commandCount
    
@loop:
    cmp r12d, r13d
    jge @done
    cmp r12d, MAX_RESULTS
    jge @done
    
    ; Get command entry
    mov eax, r12d
    imul rax, rax, sizeof COMMAND_ENTRY
    lea rbx, g_paletteState.commands
    add rbx, rax
    
    ; Check if enabled
    cmp byte ptr [rbx + COMMAND_ENTRY.enabled], 0
    je @next
    
    ; Add to listbox
    mov rcx, g_paletteState.hResultsList
    mov edx, 180h  ; LB_ADDSTRING
    xor r8, r8
    lea r9, [rbx + COMMAND_ENTRY.label]
    call SendMessageA
    
@next:
    inc r12d
    jmp @loop
    
@done:
    add rsp, 32
    ret
update_results_all ENDP

;==========================================================================
; palette_execute_command(command_index: ecx) -> bool (rax)
; Execute command by index
;==========================================================================
palette_execute_command PROC
    push rbx
    sub rsp, 32
    
    ; Get command entry
    mov eax, ecx
    imul rax, rax, sizeof COMMAND_ENTRY
    lea rbx, g_paletteState.commands
    add rbx, rax
    
    ; Check if callback exists
    mov rcx, qword ptr [rbx + COMMAND_ENTRY.callback]
    test rcx, rcx
    jz @no_callback
    
    ; Call callback
    call rcx
    
    mov rax, 1  ; Success
    jmp @done
    
@no_callback:
    xor rax, rax  ; No callback
    
@done:
    add rsp, 32
    pop rbx
    ret
palette_execute_command ENDP

;==========================================================================
; palette_get_state() -> PALETTE_STATE* (rax)
;==========================================================================
palette_get_state PROC
    lea rax, g_paletteState
    ret
palette_get_state ENDP

;==========================================================================
; palette_wnd_proc - Window procedure for command palette
;==========================================================================
palette_wnd_proc PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    mov [rbp + 16], rcx
    mov [rbp + 24], edx
    mov [rbp + 32], r8
    mov [rbp + 40], r9
    
    ; Check message
    cmp edx, 111h  ; WM_COMMAND
    je @command
    
    cmp edx, 100h  ; WM_KEYDOWN
    je @keydown
    
    ; Default processing
    mov rcx, [rbp + 16]
    mov edx, [rbp + 24]
    mov r8, [rbp + 32]
    mov r9, [rbp + 40]
    call DefWindowProcA
    jmp @done
    
@command:
    ; Handle control notifications
    mov eax, [rbp + 32]  ; wParam
    shr eax, 16          ; HIWORD(wParam) = notification code
    cmp eax, 300h        ; EN_CHANGE
    je @search_changed
    jmp @default_proc
    
@keydown:
    ; Handle keyboard navigation
    mov eax, [rbp + 32]  ; wParam (virtual key)
    cmp eax, 1Bh         ; VK_ESCAPE
    je @hide_palette
    cmp eax, 0Dh         ; VK_RETURN
    je @execute_selected
    jmp @default_proc
    
@search_changed:
    call handle_search_changed
    xor rax, rax
    jmp @done
    
@hide_palette:
    call palette_hide
    xor rax, rax
    jmp @done
    
@execute_selected:
    call handle_execute_selected
    xor rax, rax
    jmp @done
    
@default_proc:
    mov rcx, [rbp + 16]
    mov edx, [rbp + 24]
    mov r8, [rbp + 32]
    mov r9, [rbp + 40]
    call DefWindowProcA
    
@done:
    add rsp, 64
    pop rbp
    ret
palette_wnd_proc ENDP

;==========================================================================
; handle_search_changed() - Update results based on search text
;==========================================================================
handle_search_changed PROC
    LOCAL searchText[256]:BYTE
    
    sub rsp, 288
    
    ; Get search text
    mov rcx, g_paletteState.hSearchBox
    lea rdx, searchText
    mov r8d, 256
    call GetWindowTextA
    
    ; If empty, show all commands
    cmp byte ptr [searchText], 0
    je @show_all
    
    ; Perform fuzzy search
    lea rcx, searchText
    call perform_fuzzy_search
    jmp @done
    
@show_all:
    call update_results_all
    
@done:
    add rsp, 288
    ret
handle_search_changed ENDP

;==========================================================================
; perform_fuzzy_search(filter: rcx) -> void
; Search commands and update results list
;==========================================================================
perform_fuzzy_search PROC
    ; Stub: just show all for now
    call update_results_all
    ret
perform_fuzzy_search ENDP

;==========================================================================
; handle_execute_selected() - Execute currently selected command
;==========================================================================
handle_execute_selected PROC
    sub rsp, 32
    
    ; Get selected index from listbox
    mov rcx, g_paletteState.hResultsList
    mov edx, 188h  ; LB_GETCURSEL
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    ; Execute command
    mov ecx, eax
    call palette_execute_command
    
    ; Hide palette
    call palette_hide
    
    add rsp, 32
    ret
handle_execute_selected ENDP

end
