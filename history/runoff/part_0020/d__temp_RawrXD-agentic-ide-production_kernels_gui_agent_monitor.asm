; ============================================================================
; agent_monitor.asm - Displays agents from registry in a list view
; ============================================================================

option casemap:none

; ----------------------------------------------------------------------------
; EXTERNALS
; ----------------------------------------------------------------------------
EXTERN CreateWindowExA:PROC
EXTERN SendMessageA:PROC
EXTERN hMainWnd:QWORD
EXTERN hInstance:QWORD
EXTERN AgentRegistry_Count:PROC
EXTERN AgentRegistry_Retrieve:PROC

; ----------------------------------------------------------------------------
; CONSTANTS
; ----------------------------------------------------------------------------
NULL                equ 0
WS_CHILD            equ 40000000h
WS_VISIBLE          equ 10000000h
LVS_REPORT          equ 0001h
LVS_SHOWSELALWAYS   equ 0008h
LVM_INSERTCOLUMN    equ 101Bh
LVM_INSERTITEM      equ 1001h
LVM_DELETEALLITEMS  equ 1009h
LVM_SETITEM         equ 1006h
LVIF_TEXT           equ 0001h
CW_USEDEFAULT       equ 80000000h

AGENT_NAME_OFFSET       equ 4
AGENT_BACKGROUND_OFFSET equ 12

; ----------------------------------------------------------------------------
; STRUCTURES
; ----------------------------------------------------------------------------
LV_COLUMN STRUCT
    mask             dd ?
    fmt              dd ?
    cx               dd ?
    pszText          dq ?
    cchTextMax       dd ?
    iSubItem         dd ?
    iImage           dd ?
    iOrder           dd ?
LV_COLUMN ENDS

LV_ITEM STRUCT
    mask             dd ?
    iItem            dd ?
    iSubItem         dd ?
    state            dd ?
    stateMask        dd ?
    pszText          dq ?
    cchTextMax       dd ?
    iImage           dd ?
    lParam           dq ?
    iIndent          dd ?
    iGroupId         dd ?
    cColumns         dd ?
    puColumns        dq ?
    piColFmt         dq ?
    iGroup           dd ?
LV_ITEM ENDS

; ----------------------------------------------------------------------------
; PUBLICS
; ----------------------------------------------------------------------------
PUBLIC AgentMonitor_Init
PUBLIC AgentMonitor_Populate
PUBLIC hAgentList

; ----------------------------------------------------------------------------
; DATA
; ----------------------------------------------------------------------------
.data
hAgentList      dq 0
szListViewClass db 'SysListView32',0
colAgentID      db 'ID',0
colAgentName    db 'Name',0
colAgentBackground db 'Background',0

; ----------------------------------------------------------------------------
; CODE
; ----------------------------------------------------------------------------
.code

AgentMonitor_Init PROC
    LOCAL col:LV_COLUMN

    ; Create list view for agents
    invoke CreateWindowExA, 0, ADDR szListViewClass, NULL,
        WS_CHILD or WS_VISIBLE or LVS_REPORT or LVS_SHOWSELALWAYS,
        20, 400, 1160, 350, hMainWnd, 201, hInstance, NULL
    mov hAgentList, rax

    ; Add ID column
    mov col.mask, 1 or 2 or 4           ; LVCF_FMT | LVCF_WIDTH | LVCF_TEXT
    mov col.fmt, 0
    mov col.cx, 80
    mov col.pszText, OFFSET colAgentID
    mov col.cchTextMax, LENGTHOF colAgentID
    mov col.iSubItem, 0
    invoke SendMessageA, hAgentList, LVM_INSERTCOLUMN, 0, ADDR col

    ; Name column
    mov col.cx, 200
    mov col.pszText, OFFSET colAgentName
    mov col.iSubItem, 1
    invoke SendMessageA, hAgentList, LVM_INSERTCOLUMN, 1, ADDR col

    ; Background column
    mov col.cx, 400
    mov col.pszText, OFFSET colAgentBackground
    mov col.iSubItem, 2
    invoke SendMessageA, hAgentList, LVM_INSERTCOLUMN, 2, ADDR col

    ret
AgentMonitor_Init ENDP

AgentMonitor_Populate PROC
    LOCAL lvi:LV_ITEM
    LOCAL count:DWORD
    LOCAL idx:DWORD
    LOCAL agentPtr:QWORD

    invoke SendMessageA, hAgentList, LVM_DELETEALLITEMS, 0, 0

    call AgentRegistry_Count
    mov count, eax
    mov idx, 0

@loop_agents:
    mov eax, idx
    cmp eax, count
    jae @done

    ; Assume agent IDs start at 1 and are sequential
    mov ecx, idx
    inc ecx
    call AgentRegistry_Retrieve
    mov agentPtr, rax
    test rax, rax
    jz @advance

    ; Insert row with ID text placeholder
    mov lvi.mask, LVIF_TEXT
    mov eax, idx
    mov lvi.iItem, eax
    mov lvi.iSubItem, 0
    mov lvi.pszText, OFFSET colAgentID
    mov lvi.cchTextMax, 0
    invoke SendMessageA, hAgentList, LVM_INSERTITEM, 0, ADDR lvi

    ; Update name column
    mov lvi.iSubItem, 1
    mov rax, agentPtr
    mov rax, [rax + AGENT_NAME_OFFSET]
    mov lvi.pszText, rax
    invoke SendMessageA, hAgentList, LVM_SETITEM, 0, ADDR lvi

    ; Update background column
    mov lvi.iSubItem, 2
    mov rax, agentPtr
    mov rax, [rax + AGENT_BACKGROUND_OFFSET]
    mov lvi.pszText, rax
    invoke SendMessageA, hAgentList, LVM_SETITEM, 0, ADDR lvi

@advance:
    inc idx
    jmp @loop_agents

@done:
    ret
AgentMonitor_Populate ENDP

END
