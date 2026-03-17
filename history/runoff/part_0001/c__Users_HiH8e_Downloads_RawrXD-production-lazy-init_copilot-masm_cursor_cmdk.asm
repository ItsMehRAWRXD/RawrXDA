;======================================================================
; cursor_cmdk.asm - Command Palette (50+ Commands)
;======================================================================
INCLUDE windows.inc

.CONST
CMDK_MAX_COMMANDS EQU 50

.DATA
; Simple COMMAND structure: name (QWORD), func (QWORD), desc (QWORD)
COMMAND STRUCT
    name QWORD ?
    func QWORD ?
    desc QWORD ?
ENDS

g_cmdkCommands COMMAND < \
    `Explain this code`, ExplainCode, `Explain the selected code`, \
    `Generate tests`, GenerateTests, `Create unit tests for selection`, \
    `Fix errors`, FixErrors, `Fix compiler/linter errors`, \
    `Refactor`, RefactorCode, `Refactor for clarity/performance`, \
    `Add docs`, AddDocumentation, `Add JSDoc/docstrings`, \
    `Optimize`, OptimizeCode, `Optimize performance`, \
    `Convert language`, ConvertLanguage, `Convert to another language`, \
    `Create function`, CreateFunction, `Generate function from comment`, \
    `Toggle thinking UI`, ToggleThinkingMode, `Toggle the standardized thinking box`, \
    `Set Mode: Max`, SetModeMax, `Use Max mode (high-quality model)`, \
    `Set Mode: Search Web`, SetModeSearch, `Enable web-search augmentation`, \
    `Set Mode: Turbo`, SetModeTurbo, `Use turbo/deep-research mode`, \
    `Set Mode: Auto Instant`, SetModeAuto, `Enable auto-instant thinking`, \
    `Set Mode: Legacy`, SetModeLegacy, `Use legacy compatibility mode`, \
    ; ... 40 more commands (placeholders)
>
.DATA
CMDK_WINDOW_CLASS DB "CmdKPalette",0
CMDK_PALETTE_TITLE DB "Command Palette (Ctrl+K)",0
EDIT_CLASS DB "EDIT",0
LISTBOX_CLASS DB "LISTBOX",0
EMPTY_STR DB "",0
CMDK_EXPLAIN_FMT DB "Explain this code:\n```%s```",0
CMDK_GENERATE_TESTS_FMT DB "Generate comprehensive unit tests for this code:\n```%s```\n\nUse Jest/Mocha framework.",0
CHAT_TYPE_STR DB "chat",0
promptBuffer DB 8192 DUP(?)
CMDK_THINKING_STR DB "Thinking...",0
.DATA?
g_cmdkSearchBuffer DB 256 DUP(?)
g_cmdkFilteredCount DWORD ?
g_cmdkSelectedIndex DWORD ?

.CODE

CursorCmdK_Show PROC
    LOCAL hWnd:QWORD
    
    ; Create command palette window
    invoke CreateWindowExA, \
        WS_EX_TOOLWINDOW, \
        OFFSET CMDK_WINDOW_CLASS, \
        OFFSET CMDK_PALETTE_TITLE, \
        WS_POPUP | WS_BORDER | WS_VISIBLE, \
        300, 200, 600, 400, \
        0, 0, ghInstance, 0
    
    mov hWnd, rax
    
    ; Create search box
    invoke CreateWindowExA, \
        0, OFFSET EDIT_CLASS, OFFSET EMPTY_STR, \
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, \
        10, 10, 580, 30, \
        hWnd, IDC_CMDK_SEARCH, ghInstance, 0
    
    ; Create list view for commands
    invoke CreateWindowExA, \
        WS_EX_CLIENTEDGE, OFFSET LISTBOX_CLASS, OFFSET EMPTY_STR, \
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY, \
        10, 50, 580, 340, \
        hWnd, IDC_CMDK_LIST, ghInstance, 0
    
    ; Populate all commands
    invoke CmdK_PopulateCommands
    
    ; Set focus to search box
    invoke SetFocus, IDC_CMDK_SEARCH
    
    ret
CursorCmdK_Show ENDP

CmdK_PopulateCommands PROC
    LOCAL hList:QWORD
    LOCAL i:DWORD
    
    invoke GetDlgItem, g_hCmdKWnd, IDC_CMDK_LIST
    mov hList, rax
    
    mov ecx, CMDK_MAX_COMMANDS
    mov i, 0
    .repeat
        lea rax, g_cmdkCommands
        mov rdx, i
        imul rdx, SIZEOF COMMAND
        add rax, rdx
        
        mov rdx, [rax].COMMAND.name
        invoke SendMessageA, hList, LB_ADDSTRING, 0, rdx
        
        inc i
    .untilcxz
    
    ret
CmdK_PopulateCommands ENDP

CmdK_OnSearch PROC pSearchText:QWORD
    LOCAL hList:QWORD
    LOCAL i:DWORD
    
    invoke GetDlgItem, g_hCmdKWnd, IDC_CMDK_LIST
    mov hList, rax
    
    ; Clear list
    invoke SendMessageA, hList, LB_RESETCONTENT, 0, 0
    
    ; Filter commands based on search
    mov ecx, CMDK_MAX_COMMANDS
    mov i, 0
    .repeat
        lea rax, g_cmdkCommands
        mov rdx, i
        imul rdx, SIZEOF COMMAND
        add rax, rdx
        
        ; Fuzzy match search text
        mov rcx, [rax].COMMAND.name
        mov rdx, pSearchText
        call String_FuzzyMatch
        
        .if rax != 0
            ; Add to filtered list
            mov rdx, [rax].COMMAND.name
            invoke SendMessageA, hList, LB_ADDSTRING, 0, rdx
        .endif
        
        inc i
    .untilcxz
    
    ret
CmdK_OnSearch ENDP

CmdK_ExecuteCommand PROC commandIndex:DWORD
    LOCAL i:DWORD
    
    mov i, commandIndex
    
    ; Find command in registry
    lea rax, g_cmdkCommands
    mov rdx, i
    imul rdx, SIZEOF COMMAND
    add rax, rdx
    
    ; Call function pointer
    mov rcx, [rax].COMMAND.func
    call rcx
    
    ret
CmdK_ExecuteCommand ENDP

; Command implementations
ExplainCode PROC
    invoke GetSelectedText
    test rax, rax
    jz @ret
    
    ; Build prompt
    lea rcx, promptBuffer
    lea rdx, CMDK_EXPLAIN_FMT
    mov r8, rax
    call wsprintfA
    
    ; Send to Copilot (use model router so modes and fallback are applied)
    mov rcx, OFFSET promptBuffer
    lea rdx, CHAT_TYPE_STR
    mov r8d, 512
    call ModelRouter_CallModel
    
    ret
@ret:
    ret
ExplainCode ENDP

GenerateTests PROC
    invoke GetCurrentFileContent
    test rax, rax
    jz @ret
    
    ; Build prompt for test generation
    lea rcx, promptBuffer
    lea rdx, CMDK_GENERATE_TESTS_FMT
    mov r8, rax
    call wsprintfA
    
    ; Send to Copilot
    mov rcx, OFFSET promptBuffer
    lea rdx, CHAT_TYPE_STR
    mov r8d, 1024
    call ModelRouter_CallModel
    
    ret
@ret:
    ret
GenerateTests ENDP

; Additional command implementations (placeholders)

ToggleThinkingMode PROC
    LOCAL cur:DWORD
    mov eax, dword ptr g_thinkingEnabled
    xor eax, 1
    mov dword ptr g_thinkingEnabled, eax
    .if eax == 1
        lea rcx, CMDK_THINKING_STR
        invoke ChatStreamManager_ShowThinking, rcx
    .else
        invoke ChatStreamManager_HideThinking
    .endif
    ret
ToggleThinkingMode ENDP

SetModeMax PROC
    ; Toggle MODE_FLAG_MAX
    call ModelRouter_GetMode
    mov ecx, eax
    test eax, MODE_FLAG_MAX
    jz .Lset_max
    ; clear bit
    sub ecx, MODE_FLAG_MAX
    jmp .Ldone_max
.Lset_max:
    or ecx, MODE_FLAG_MAX
.Ldone_max:
    call ModelRouter_SetMode
    ret
SetModeMax ENDP

SetModeSearch PROC
    ; Toggle MODE_FLAG_SEARCH_WEB
    call ModelRouter_GetMode
    mov ecx, eax
    test eax, MODE_FLAG_SEARCH_WEB
    jz .Lset_search
    sub ecx, MODE_FLAG_SEARCH_WEB
    jmp .Ldone_search
.Lset_search:
    or ecx, MODE_FLAG_SEARCH_WEB
.Ldone_search:
    call ModelRouter_SetMode
    ret
SetModeSearch ENDP

SetModeTurbo PROC
    ; Toggle MODE_FLAG_TURBO
    call ModelRouter_GetMode
    mov ecx, eax
    test eax, MODE_FLAG_TURBO
    jz .Lset_turbo
    sub ecx, MODE_FLAG_TURBO
    jmp .Ldone_turbo
.Lset_turbo:
    or ecx, MODE_FLAG_TURBO
.Ldone_turbo:
    call ModelRouter_SetMode
    ret
SetModeTurbo ENDP

SetModeAuto PROC
    ; Toggle MODE_FLAG_AUTO_INSTANT
    call ModelRouter_GetMode
    mov ecx, eax
    test eax, MODE_FLAG_AUTO_INSTANT
    jz .Lset_auto
    sub ecx, MODE_FLAG_AUTO_INSTANT
    jmp .Ldone_auto
.Lset_auto:
    or ecx, MODE_FLAG_AUTO_INSTANT
.Ldone_auto:
    call ModelRouter_SetMode
    ret
SetModeAuto ENDP

SetModeLegacy PROC
    ; Toggle MODE_FLAG_LEGACY
    call ModelRouter_GetMode
    mov ecx, eax
    test eax, MODE_FLAG_LEGACY
    jz .Lset_legacy
    sub ecx, MODE_FLAG_LEGACY
    jmp .Ldone_legacy
.Lset_legacy:
    or ecx, MODE_FLAG_LEGACY
.Ldone_legacy:
    call ModelRouter_SetMode
    ret
SetModeLegacy ENDP

; ... 40 more command implementations (placeholders)

END