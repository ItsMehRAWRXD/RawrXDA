;======================================================================
; RawrXD IDE - Embedded Debugger UI
; Integrated debugger panel with breakpoints, variables, stack
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
; Debugger UI state
g_hDebuggerPanel        DQ ?
g_hBreakpointList       DQ ?
g_hVariableList         DQ ?
g_hStackList            DQ ?
g_hRegistersView        DQ ?
g_hMemoryView           DQ ?
g_hDisassemblyView      DQ ?
g_hConsoleOutput        DQ ?
g_debuggerTabs          DQ 7  ; 7 tabs: Breakpoints, Variables, Stack, Registers, Memory, Disassembly, Console

; Debugger state
g_currentBreakpoint     DQ -1
g_totalBreakpoints      DQ 0
g_totalVariables        DQ 0
g_totalStackFrames      DQ 0

; Structures
DEBUGGER_BREAKPOINT STRUCT
    address             DQ ?
    enabled             DQ ?
    hitCount            DQ ?
    condition[256]      DB 256 DUP(0)
    fileName[260]       DB 260 DUP(0)
    lineNumber          DQ ?
DEBUGGER_BREAKPOINT ENDS

DEBUGGER_VARIABLE STRUCT
    name[64]            DB 64 DUP(0)
    type[32]            DB 32 DUP(0)
    value[256]          DB 256 DUP(0)
    address             DQ ?
    size                DQ ?
DEBUGGER_VARIABLE ENDS

DEBUGGER_STACK_FRAME STRUCT
    address             DQ ?
    funcName[128]       DB 128 DUP(0)
    fileName[260]       DB 260 DUP(0)
    lineNumber          DQ ?
    paramCount          DQ ?
DEBUGGER_STACK_FRAME ENDS

; Arrays
g_breakpoints[100 * (SIZEOF DEBUGGER_BREAKPOINT)] DQ 100*SIZEOF DEBUGGER_BREAKPOINT DUP(0)
g_variables[64 * (SIZEOF DEBUGGER_VARIABLE)] DQ 64*SIZEOF DEBUGGER_VARIABLE DUP(0)
g_stackFrames[32 * (SIZEOF DEBUGGER_STACK_FRAME)] DQ 32*SIZEOF DEBUGGER_STACK_FRAME DUP(0)

.CODE

;----------------------------------------------------------------------
; RawrXD_Debugger_Create - Create debugger UI panel
;----------------------------------------------------------------------
RawrXD_Debugger_Create PROC hParent:QWORD, x:QWORD, y:QWORD, cx:QWORD, cy:QWORD
    LOCAL hFont:QWORD
    LOCAL logFont:LOGFONTA
    
    ; Create main debugger panel
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "STATIC",
        NULL,
        WS_CHILD OR WS_VISIBLE,
        x, y, cx, cy,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hDebuggerPanel, rax
    
    ; Create toolbar
    INVOKE CreateWindowEx,
        0,
        "ToolbarWindow32",
        NULL,
        WS_CHILD OR WS_VISIBLE,
        x, y, cx, 28,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    ; Add debug buttons: Continue, Step Over, Step Into, Step Out, Stop
    INVOKE RawrXD_Debugger_CreateToolbar, rax
    
    ; Create tab control for different debug views
    INVOKE CreateWindowEx,
        0,
        "SysTabControl32",
        NULL,
        WS_CHILD OR WS_VISIBLE,
        x, y + 28, cx, cy - 28,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    ; Add tabs
    INVOKE RawrXD_Debugger_CreateTabs, rax
    
    ; Create tab pages
    ; Tab 1: Breakpoints List
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "SysListView32",
        NULL,
        WS_CHILD OR WS_VISIBLE OR LVS_REPORT OR LVS_SINGLESEL,
        x, y + 55, cx, cy - 60,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hBreakpointList, rax
    INVOKE RawrXD_Debugger_InitBreakpointList, rax
    
    ; Tab 2: Variables
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "SysListView32",
        NULL,
        WS_CHILD OR LVS_REPORT OR LVS_SINGLESEL,
        x, y + 55, cx, cy - 60,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hVariableList, rax
    INVOKE RawrXD_Debugger_InitVariableList, rax
    
    ; Tab 3: Call Stack
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "SysListView32",
        NULL,
        WS_CHILD OR LVS_REPORT OR LVS_SINGLESEL,
        x, y + 55, cx, cy - 60,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hStackList, rax
    INVOKE RawrXD_Debugger_InitStackList, rax
    
    ; Tab 4: Registers
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "SysListView32",
        NULL,
        WS_CHILD OR LVS_REPORT OR LVS_SINGLESEL,
        x, y + 55, cx, cy - 60,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hRegistersView, rax
    INVOKE RawrXD_Debugger_InitRegistersList, rax
    
    ; Tab 5: Memory
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "EDIT",
        NULL,
        WS_CHILD OR ES_MULTILINE OR ES_READONLY OR WS_VSCROLL,
        x, y + 55, cx, cy - 60,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hMemoryView, rax
    
    ; Tab 6: Disassembly
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "EDIT",
        NULL,
        WS_CHILD OR ES_MULTILINE OR ES_READONLY OR WS_VSCROLL,
        x, y + 55, cx, cy - 60,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hDisassemblyView, rax
    
    ; Tab 7: Debug Console
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "RICHEDIT50W",
        NULL,
        WS_CHILD OR ES_MULTILINE OR ES_READONLY OR WS_VSCROLL,
        x, y + 55, cx, cy - 60,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hConsoleOutput, rax
    
    ; Set monospace font
    INVOKE RtlZeroMemory, ADDR logFont, SIZEOF LOGFONTA
    mov logFont.lfHeight, -11
    mov logFont.lfWeight, FW_NORMAL
    INVOKE lstrcpyA, ADDR logFont.lfFaceName, OFFSET szMonoFont
    
    INVOKE CreateFontIndirectA, ADDR logFont
    mov hFont, rax
    
    INVOKE SendMessage, g_hBreakpointList, WM_SETFONT, hFont, TRUE
    INVOKE SendMessage, g_hVariableList, WM_SETFONT, hFont, TRUE
    INVOKE SendMessage, g_hStackList, WM_SETFONT, hFont, TRUE
    INVOKE SendMessage, g_hRegistersView, WM_SETFONT, hFont, TRUE
    INVOKE SendMessage, g_hMemoryView, WM_SETFONT, hFont, TRUE
    INVOKE SendMessage, g_hDisassemblyView, WM_SETFONT, hFont, TRUE
    INVOKE SendMessage, g_hConsoleOutput, WM_SETFONT, hFont, TRUE
    
    ; Initial message
    INVOKE RawrXD_Debugger_Log, OFFSET szDebuggerReady
    
    xor eax, eax
    ret
    
RawrXD_Debugger_Create ENDP

;----------------------------------------------------------------------
; RawrXD_Debugger_CreateToolbar - Create debug toolbar
;----------------------------------------------------------------------
RawrXD_Debugger_CreateToolbar PROC hToolbar:QWORD
    LOCAL tb:TBBUTTON
    
    ; Continue (F5)
    INVOKE RtlZeroMemory, ADDR tb, SIZEOF TBBUTTON
    mov tb.iBitmap, 0
    mov tb.idCommand, IDC_DEBUG_CONTINUE
    mov tb.fsState, TBSTATE_ENABLED
    mov tb.fsStyle, TBSTYLE_BUTTON
    INVOKE SendMessage, hToolbar, TB_ADDBUTTONS, 1, ADDR tb
    
    ; Step Over (F10)
    INVOKE RtlZeroMemory, ADDR tb, SIZEOF TBBUTTON
    mov tb.iBitmap, 1
    mov tb.idCommand, IDC_DEBUG_STEPOVER
    mov tb.fsState, TBSTATE_ENABLED
    INVOKE SendMessage, hToolbar, TB_ADDBUTTONS, 1, ADDR tb
    
    ; Step Into (F11)
    INVOKE RtlZeroMemory, ADDR tb, SIZEOF TBBUTTON
    mov tb.iBitmap, 2
    mov tb.idCommand, IDC_DEBUG_STEPINTO
    mov tb.fsState, TBSTATE_ENABLED
    INVOKE SendMessage, hToolbar, TB_ADDBUTTONS, 1, ADDR tb
    
    ; Step Out (Shift+F11)
    INVOKE RtlZeroMemory, ADDR tb, SIZEOF TBBUTTON
    mov tb.iBitmap, 3
    mov tb.idCommand, IDC_DEBUG_STEPOUT
    mov tb.fsState, TBSTATE_ENABLED
    INVOKE SendMessage, hToolbar, TB_ADDBUTTONS, 1, ADDR tb
    
    ; Stop (Shift+F5)
    INVOKE RtlZeroMemory, ADDR tb, SIZEOF TBBUTTON
    mov tb.iBitmap, 4
    mov tb.idCommand, IDC_DEBUG_STOP
    mov tb.fsState, TBSTATE_ENABLED
    INVOKE SendMessage, hToolbar, TB_ADDBUTTONS, 1, ADDR tb
    
    ; Pause
    INVOKE RtlZeroMemory, ADDR tb, SIZEOF TBBUTTON
    mov tb.iBitmap, 5
    mov tb.idCommand, IDC_DEBUG_PAUSE
    mov tb.fsState, TBSTATE_ENABLED
    INVOKE SendMessage, hToolbar, TB_ADDBUTTONS, 1, ADDR tb
    
    ret
    
RawrXD_Debugger_CreateToolbar ENDP

;----------------------------------------------------------------------
; RawrXD_Debugger_CreateTabs - Create tab pages
;----------------------------------------------------------------------
RawrXD_Debugger_CreateTabs PROC hTabControl:QWORD
    LOCAL tc:TCITEM
    
    INVOKE RtlZeroMemory, ADDR tc, SIZEOF TCITEM
    mov tc.mask, TCIF_TEXT
    
    mov tc.pszText, OFFSET szTabBreakpoints
    INVOKE SendMessage, hTabControl, TCM_INSERTITEM, 0, ADDR tc
    
    mov tc.pszText, OFFSET szTabVariables
    INVOKE SendMessage, hTabControl, TCM_INSERTITEM, 1, ADDR tc
    
    mov tc.pszText, OFFSET szTabStack
    INVOKE SendMessage, hTabControl, TCM_INSERTITEM, 2, ADDR tc
    
    mov tc.pszText, OFFSET szTabRegisters
    INVOKE SendMessage, hTabControl, TCM_INSERTITEM, 3, ADDR tc
    
    mov tc.pszText, OFFSET szTabMemory
    INVOKE SendMessage, hTabControl, TCM_INSERTITEM, 4, ADDR tc
    
    mov tc.pszText, OFFSET szTabDisassembly
    INVOKE SendMessage, hTabControl, TCM_INSERTITEM, 5, ADDR tc
    
    mov tc.pszText, OFFSET szTabConsole
    INVOKE SendMessage, hTabControl, TCM_INSERTITEM, 6, ADDR tc
    
    ret
    
RawrXD_Debugger_CreateTabs ENDP

;----------------------------------------------------------------------
; RawrXD_Debugger_InitBreakpointList - Initialize breakpoints list
;----------------------------------------------------------------------
RawrXD_Debugger_InitBreakpointList PROC hListView:QWORD
    LOCAL lvc:LVCOLUMN
    
    INVOKE RtlZeroMemory, ADDR lvc, SIZEOF LVCOLUMN
    mov lvc.mask, LVCF_TEXT OR LVCF_WIDTH
    
    mov lvc.cx, 40
    mov lvc.pszText, OFFSET szColEnabled
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 0, ADDR lvc
    
    mov lvc.cx, 100
    mov lvc.pszText, OFFSET szColAddress
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 1, ADDR lvc
    
    mov lvc.cx, 200
    mov lvc.pszText, OFFSET szColFile
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 2, ADDR lvc
    
    mov lvc.cx, 80
    mov lvc.pszText, OFFSET szColLine
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 3, ADDR lvc
    
    mov lvc.cx, 60
    mov lvc.pszText, OFFSET szColHits
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 4, ADDR lvc
    
    ret
    
RawrXD_Debugger_InitBreakpointList ENDP

;----------------------------------------------------------------------
; RawrXD_Debugger_InitVariableList - Initialize variables list
;----------------------------------------------------------------------
RawrXD_Debugger_InitVariableList PROC hListView:QWORD
    LOCAL lvc:LVCOLUMN
    
    INVOKE RtlZeroMemory, ADDR lvc, SIZEOF LVCOLUMN
    mov lvc.mask, LVCF_TEXT OR LVCF_WIDTH
    
    mov lvc.cx, 100
    mov lvc.pszText, OFFSET szColName
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 0, ADDR lvc
    
    mov lvc.cx, 80
    mov lvc.pszText, OFFSET szColType
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 1, ADDR lvc
    
    mov lvc.cx, 200
    mov lvc.pszText, OFFSET szColValue
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 2, ADDR lvc
    
    mov lvc.cx, 100
    mov lvc.pszText, OFFSET szColAddress
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 3, ADDR lvc
    
    ret
    
RawrXD_Debugger_InitVariableList ENDP

;----------------------------------------------------------------------
; RawrXD_Debugger_InitStackList - Initialize stack list
;----------------------------------------------------------------------
RawrXD_Debugger_InitStackList PROC hListView:QWORD
    LOCAL lvc:LVCOLUMN
    
    INVOKE RtlZeroMemory, ADDR lvc, SIZEOF LVCOLUMN
    mov lvc.mask, LVCF_TEXT OR LVCF_WIDTH
    
    mov lvc.cx, 100
    mov lvc.pszText, OFFSET szColFunction
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 0, ADDR lvc
    
    mov lvc.cx, 200
    mov lvc.pszText, OFFSET szColFile
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 1, ADDR lvc
    
    mov lvc.cx, 60
    mov lvc.pszText, OFFSET szColLine
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 2, ADDR lvc
    
    ret
    
RawrXD_Debugger_InitStackList ENDP

;----------------------------------------------------------------------
; RawrXD_Debugger_InitRegistersList - Initialize registers list
;----------------------------------------------------------------------
RawrXD_Debugger_InitRegistersList PROC hListView:QWORD
    LOCAL lvc:LVCOLUMN
    LOCAL lvi:LVITEM
    LOCAL i:QWORD
    
    INVOKE RtlZeroMemory, ADDR lvc, SIZEOF LVCOLUMN
    mov lvc.mask, LVCF_TEXT OR LVCF_WIDTH
    
    mov lvc.cx, 80
    mov lvc.pszText, OFFSET szColRegister
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 0, ADDR lvc
    
    mov lvc.cx, 150
    mov lvc.pszText, OFFSET szColValue
    INVOKE SendMessage, hListView, LVM_INSERTCOLUMN, 1, ADDR lvc
    
    ; Add register rows
    mov i, 0
@@add_regs:
    cmp i, 16  ; RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP, R8-R15
    jge @@done
    
    INVOKE RtlZeroMemory, ADDR lvi, SIZEOF LVITEM
    mov lvi.mask, LVIF_TEXT
    mov lvi.iItem, i
    mov lvi.pszText, OFFSET szRegNames + i*4
    
    INVOKE SendMessage, hListView, LVM_INSERTITEM, 0, ADDR lvi
    
    inc i
    jmp @@add_regs
    
@@done:
    ret
    
RawrXD_Debugger_InitRegistersList ENDP

;----------------------------------------------------------------------
; RawrXD_Debugger_SetBreakpoint - Set breakpoint at address/line
;----------------------------------------------------------------------
RawrXD_Debugger_SetBreakpoint PROC pszFile:QWORD, dwLine:QWORD, dwAddress:QWORD
    LOCAL pBreakpoint:QWORD
    LOCAL idx:QWORD
    
    cmp g_totalBreakpoints, 100
    jge @@limit
    
    mov idx, g_totalBreakpoints
    imul idx, SIZEOF DEBUGGER_BREAKPOINT
    mov pBreakpoint, OFFSET g_breakpoints
    add pBreakpoint, idx
    
    ; Set breakpoint info
    mov DEBUGGER_BREAKPOINT.address[pBreakpoint], dwAddress
    mov DEBUGGER_BREAKPOINT.enabled[pBreakpoint], 1
    mov DEBUGGER_BREAKPOINT.hitCount[pBreakpoint], 0
    mov DEBUGGER_BREAKPOINT.lineNumber[pBreakpoint], dwLine
    
    INVOKE lstrcpyA, DEBUGGER_BREAKPOINT.fileName[pBreakpoint], pszFile
    
    inc g_totalBreakpoints
    
    ; Add to list view
    INVOKE RawrXD_Debugger_AddBreakpointToList, pBreakpoint
    
    ; Log
    INVOKE lstrcpyA, OFFSET szBpMsg, OFFSET szBPSetAt
    INVOKE RawrXD_Util_IntToStr, dwLine, OFFSET szBpMsg + 50
    INVOKE RawrXD_Debugger_Log, OFFSET szBpMsg
    
    xor eax, eax
    ret
    
@@limit:
    INVOKE RawrXD_Output_Error, OFFSET szMaxBreakpoints
    mov eax, -1
    ret
    
RawrXD_Debugger_SetBreakpoint ENDP

;----------------------------------------------------------------------
; RawrXD_Debugger_AddBreakpointToList - Add BP to list view
;----------------------------------------------------------------------
RawrXD_Debugger_AddBreakpointToList PROC pBreakpoint:QWORD
    LOCAL lvi:LVITEM
    LOCAL szAddress[32]:BYTE
    LOCAL szLine[16]:BYTE
    LOCAL szHits[16]:BYTE
    
    ; Convert values to strings
    INVOKE RawrXD_Util_HexToStr, DEBUGGER_BREAKPOINT.address[pBreakpoint], ADDR szAddress
    INVOKE RawrXD_Util_IntToStr, DEBUGGER_BREAKPOINT.lineNumber[pBreakpoint], ADDR szLine
    INVOKE RawrXD_Util_IntToStr, DEBUGGER_BREAKPOINT.hitCount[pBreakpoint], ADDR szHits
    
    ; Add to list
    INVOKE RtlZeroMemory, ADDR lvi, SIZEOF LVITEM
    mov lvi.mask, LVIF_TEXT
    mov lvi.iItem, g_totalBreakpoints
    mov lvi.pszText, OFFSET szEnabled
    INVOKE SendMessage, g_hBreakpointList, LVM_INSERTITEM, 0, ADDR lvi
    
    ; Set columns
    mov lvi.iSubItem, 1
    mov lvi.pszText, ADDR szAddress
    INVOKE SendMessage, g_hBreakpointList, LVM_SETITEM, 0, ADDR lvi
    
    mov lvi.iSubItem, 2
    mov lvi.pszText, DEBUGGER_BREAKPOINT.fileName[pBreakpoint]
    INVOKE SendMessage, g_hBreakpointList, LVM_SETITEM, 0, ADDR lvi
    
    mov lvi.iSubItem, 3
    mov lvi.pszText, ADDR szLine
    INVOKE SendMessage, g_hBreakpointList, LVM_SETITEM, 0, ADDR lvi
    
    mov lvi.iSubItem, 4
    mov lvi.pszText, ADDR szHits
    INVOKE SendMessage, g_hBreakpointList, LVM_SETITEM, 0, ADDR lvi
    
    ret
    
RawrXD_Debugger_AddBreakpointToList ENDP

;----------------------------------------------------------------------
; RawrXD_Debugger_RemoveBreakpoint - Remove breakpoint by index
;----------------------------------------------------------------------
RawrXD_Debugger_RemoveBreakpoint PROC dwIndex:QWORD
    cmp dwIndex, 0
    jl @@invalid
    cmp dwIndex, g_totalBreakpoints
    jge @@invalid
    
    ; Remove from list view
    INVOKE SendMessage, g_hBreakpointList, LVM_DELETEITEM, dwIndex, 0
    
    ; Mark as disabled (could optimize by compacting)
    mov rax, dwIndex
    imul rax, SIZEOF DEBUGGER_BREAKPOINT
    mov DEBUGGER_BREAKPOINT.enabled[OFFSET g_breakpoints + rax], 0
    
    dec g_totalBreakpoints
    
    xor eax, eax
    ret
    
@@invalid:
    mov eax, -1
    ret
    
RawrXD_Debugger_RemoveBreakpoint ENDP

;----------------------------------------------------------------------
; RawrXD_Debugger_UpdateVariables - Update variables display
;----------------------------------------------------------------------
RawrXD_Debugger_UpdateVariables PROC
    LOCAL i:QWORD
    LOCAL pVar:QWORD
    LOCAL lvi:LVITEM
    
    ; Clear previous variables
    INVOKE SendMessage, g_hVariableList, LVM_DELETEALLITEMS, 0, 0
    
    ; Add variables from g_variables array
    mov i, 0
@@add_vars:
    cmp i, g_totalVariables
    jge @@done
    
    mov rax, i
    imul rax, SIZEOF DEBUGGER_VARIABLE
    mov pVar, OFFSET g_variables
    add pVar, rax
    
    INVOKE RtlZeroMemory, ADDR lvi, SIZEOF LVITEM
    mov lvi.mask, LVIF_TEXT
    mov lvi.iItem, i
    mov lvi.pszText, DEBUGGER_VARIABLE.name[pVar]
    INVOKE SendMessage, g_hVariableList, LVM_INSERTITEM, 0, ADDR lvi
    
    ; Set type column
    mov lvi.iSubItem, 1
    mov lvi.pszText, DEBUGGER_VARIABLE.type[pVar]
    INVOKE SendMessage, g_hVariableList, LVM_SETITEM, 0, ADDR lvi
    
    ; Set value column
    mov lvi.iSubItem, 2
    mov lvi.pszText, DEBUGGER_VARIABLE.value[pVar]
    INVOKE SendMessage, g_hVariableList, LVM_SETITEM, 0, ADDR lvi
    
    inc i
    jmp @@add_vars
    
@@done:
    ret
    
RawrXD_Debugger_UpdateVariables ENDP

;----------------------------------------------------------------------
; RawrXD_Debugger_Log - Log message to debug console
;----------------------------------------------------------------------
RawrXD_Debugger_Log PROC pszMessage:QWORD
    ; Append to console output
    INVOKE SendMessage, g_hConsoleOutput, EM_REPLACESEL, FALSE, pszMessage
    INVOKE SendMessage, g_hConsoleOutput, EM_REPLACESEL, FALSE, OFFSET szNewline
    
    ; Scroll to end
    mov rcx, -1
    INVOKE SendMessage, g_hConsoleOutput, EM_SETSEL, rcx, rcx
    INVOKE SendMessage, g_hConsoleOutput, EM_SCROLL, SB_BOTTOM, 0
    
    ret
    
RawrXD_Debugger_Log ENDP

; String literals
szMonoFont              DB "Consolas", 0
szTabBreakpoints        DB "Breakpoints", 0
szTabVariables          DB "Variables", 0
szTabStack              DB "Call Stack", 0
szTabRegisters          DB "Registers", 0
szTabMemory             DB "Memory", 0
szTabDisassembly        DB "Disassembly", 0
szTabConsole            DB "Console", 0
szColEnabled            DB "Enabled", 0
szColAddress            DB "Address", 0
szColFile               DB "File", 0
szColLine               DB "Line", 0
szColHits               DB "Hits", 0
szColName               DB "Name", 0
szColType               DB "Type", 0
szColValue              DB "Value", 0
szColFunction           DB "Function", 0
szColRegister           DB "Register", 0
szEnabled               DB "✓", 0
szBPSetAt               DB "Breakpoint set at line ", 0
szBpMsg[256]            DB 256 DUP(0)
szMaxBreakpoints        DB "Maximum breakpoints reached", 0
szDebuggerReady         DB "Debugger ready. Set breakpoints to begin debugging.", 0
szRegNames              DB "RAX", "RBX", "RCX", "RDX", "RSI", "RDI", "RBP", "RSP"
                        DB "R8 ", "R9 ", "R10", "R11", "R12", "R13", "R14", "R15"
szNewline               DB 13, 10, 0

; Debug command IDs
IDC_DEBUG_CONTINUE      EQU 5001
IDC_DEBUG_STEPOVER      EQU 5002
IDC_DEBUG_STEPINTO      EQU 5003
IDC_DEBUG_STEPOUT       EQU 5004
IDC_DEBUG_STOP          EQU 5005
IDC_DEBUG_PAUSE         EQU 5006

END
