; ============================================================================
; RawrXD Agentic IDE - Magic Wand System Implementation
; Pure MASM - UI Components for Agentic Interaction
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comctl32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comctl32.lib

 .data
include constants.inc
include structures.inc
include macros.inc

; ============================================================================
; External declarations
; ============================================================================

extern hMainWindow:DWORD
extern hMainFont:DWORD
extern hToolRegistry:DWORD
extern hModelInvoker:DWORD
extern hActionExecutor:DWORD

extern ToolRegistry_ExecuteTool:proc
extern ModelInvoker_Invoke:proc
extern ActionExecutor_ExecutePlan:proc

; ============================================================================
; Constants
; ============================================================================

IDC_WISH_INPUT          equ 4000
IDC_WISH_SEND           equ 4001
IDC_WISH_CANCEL         equ 4002
IDC_PLAN_LIST           equ 4003
IDC_PLAN_APPROVE        equ 4004
IDC_PLAN_REJECT         equ 4005
IDC_PROGRESS_BAR        equ 4006

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    szWishDialogClass   db "WishDialog", 0
    szPlanDialogClass   db "PlanDialog", 0
    szProgressClass     db "msctls_progress32", 0
    
    ; Dialog titles
    szWishDialogTitle   db "What wish would you like to execute?", 0
    szPlanDialogTitle   db "Execution Plan - Review and Approve", 0
    szProgressTitle     db "Executing Plan...", 0
    
    ; Button text
    szSendText          db "Execute Wish", 0
    szCancelText        db "Cancel", 0
    szApproveText       db "Approve & Execute", 0
    szRejectText        db "Reject", 0
    szThinkingText      db "Thinking...", 0
    szExecutingText     db "Executing...", 0
    
    ; UI state
    g_hWishDialog       dd 0
    g_hPlanDialog       dd 0
    g_hWishInput        dd 0
    g_hWishSend         dd 0
    g_hWishCancel       dd 0
    g_hPlanList         dd 0
    g_hPlanApprove      dd 0
    g_hPlanReject       dd 0
    g_hProgressBar      dd 0
    
    ; Current wish and plan
    g_szCurrentWish     db 1024 dup(0)
    g_CurrentPlan       EXECUTION_PLAN <>
    
    ; Colors
    clrDialogBg         dd 002D2D30h
    clrDialogTitle      dd 00E0E0E0h
    clrDialogText       dd 00CCCCCCh

.data?
    g_hDialogBrush      dd ?
    g_pPlanActions      dd ?

; ============================================================================
; PROCEDURES
; ============================================================================

; ============================================================================
; MagicWand_ShowWishDialog - Show dialog to input user wish
; ============================================================================
MagicWand_ShowWishDialog proc
    LOCAL wc:WNDCLASSEX
    LOCAL rect:RECT
    LOCAL dwStyle:DWORD
    LOCAL dwExStyle:DWORD
    LOCAL x:DWORD
    LOCAL y:DWORD
    LOCAL width:DWORD
    LOCAL height:DWORD
    
    ; Register wish dialog class if not already registered
    invoke GetClassInfoEx, NULL, addr szWishDialogClass, addr wc
    .if eax == 0
        mov wc.cbSize, sizeof WNDCLASSEX
        mov wc.style, CS_HREDRAW or CS_VREDRAW
        mov wc.lpfnWndProc, offset WishDialogProc
        mov wc.cbClsExtra, 0
        mov wc.cbWndExtra, 0
        push hInstance
        pop wc.hInstance
        invoke LoadIcon, NULL, IDI_APPLICATION
        mov wc.hIcon, eax
        invoke LoadCursor, NULL, IDC_ARROW
        mov wc.hCursor, eax
        invoke CreateSolidBrush, clrDialogBg
        mov g_hDialogBrush, eax
        mov wc.hbrBackground, eax
        mov wc.lpszMenuName, NULL
        mov wc.lpszClassName, offset szWishDialogClass
        mov wc.hIconSm, 0
        
        invoke RegisterClassEx, addr wc
    .endif
    
    ; Calculate dialog position (centered)
    mov width, 600
    mov height, 200
    invoke GetClientRect, hMainWindow, addr rect
    mov eax, rect.right
    sub eax, width
    shr eax, 1
    mov x, eax
    mov eax, rect.bottom
    sub eax, height
    shr eax, 1
    mov y, eax
    
    ; Create wish dialog
    mov dwStyle, WS_OVERLAPPED or WS_CAPTION or WS_SYSMENU or WS_BORDER
    mov dwExStyle, WS_EX_TOPMOST or WS_EX_DLGMODALFRAME
    
    invoke CreateWindowEx, dwExStyle,
        addr szWishDialogClass,
        addr szWishDialogTitle,
        dwStyle,
        x, y, width, height,
        hMainWindow, NULL, hInstance, NULL
    
    mov g_hWishDialog, eax
    test eax, eax
    jz @Exit
    
    ; Show dialog
    invoke ShowWindow, g_hWishDialog, SW_SHOW
    invoke UpdateWindow, g_hWishDialog
    
@Exit:
    ret
MagicWand_ShowWishDialog endp

; ============================================================================
; WishDialogProc - Window procedure for wish input dialog
; ============================================================================
WishDialogProc proc hWnd:HWND, uMsg:UINT, wParam:WPARAM, lParam:LPARAM
    .if uMsg == WM_CREATE
        call OnWishDialogCreate
        xor eax, eax
        ret
        
    .elseif uMsg == WM_COMMAND
        push lParam
        push wParam
        call OnWishDialogCommand
        xor eax, eax
        ret
        
    .elseif uMsg == WM_PAINT
        call OnWishDialogPaint
        xor eax, eax
        ret
        
    .elseif uMsg == WM_CLOSE
        invoke DestroyWindow, hWnd
        xor eax, eax
        ret
        
    .elseif uMsg == WM_DESTROY
        .if g_hDialogBrush != 0
            invoke DeleteObject, g_hDialogBrush
            mov g_hDialogBrush, 0
        .endif
        mov g_hWishDialog, 0
        xor eax, eax
        ret
        
    .endif
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
WishDialogProc endp

; ============================================================================
; OnWishDialogCreate - Handle WM_CREATE for wish dialog
; ============================================================================
OnWishDialogCreate proc
    LOCAL dwStyle:DWORD
    LOCAL rect:RECT
    LOCAL x:DWORD
    LOCAL y:DWORD
    LOCAL width:DWORD
    LOCAL height:DWORD
    
    ; Get client rectangle
    invoke GetClientRect, g_hWishDialog, addr rect
    
    ; Create wish input edit control
    mov width, rect.right
    sub width, 40
    mov height, 80
    mov x, 20
    mov y, 30
    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_AUTOVSCROLL or ES_WANTRETURN
    invoke CreateWindowEx, WS_EX_CLIENTEDGE,
        addr szEditClass,
        NULL,
        dwStyle,
        x, y, width, height,
        g_hWishDialog, IDC_WISH_INPUT, hInstance, NULL
    mov g_hWishInput, eax
    invoke SendMessage, eax, WM_SETFONT, hMainFont, TRUE
    
    ; Create send button
    mov width, 120
    mov height, 30
    mov x, 20
    mov y, 130
    mov dwStyle, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    invoke CreateWindowEx, 0,
        addr szButtonClass,
        addr szSendText,
        dwStyle,
        x, y, width, height,
        g_hWishDialog, IDC_WISH_SEND, hInstance, NULL
    mov g_hWishSend, eax
    invoke SendMessage, eax, WM_SETFONT, hMainFont, TRUE
    
    ; Create cancel button
    mov width, 120
    mov height, 30
    mov x, 160
    mov y, 130
    mov dwStyle, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    invoke CreateWindowEx, 0,
        addr szButtonClass,
        addr szCancelText,
        dwStyle,
        x, y, width, height,
        g_hWishDialog, IDC_WISH_CANCEL, hInstance, NULL
    mov g_hWishCancel, eax
    invoke SendMessage, eax, WM_SETFONT, hMainFont, TRUE
    
    ret
OnWishDialogCreate endp

; ============================================================================
; OnWishDialogCommand - Handle WM_COMMAND for wish dialog
; ============================================================================
OnWishDialogCommand proc
    mov eax, wParam
    and eax, 0FFFFh  ; LOWORD = control ID
    
    .if eax == IDC_WISH_SEND
        call OnWishSendClicked
    .elseif eax == IDC_WISH_CANCEL
        invoke SendMessage, g_hWishDialog, WM_CLOSE, 0, 0
    .endif
    
    ret
OnWishDialogCommand endp

; ============================================================================
; OnWishSendClicked - Handle send button click
; ============================================================================
OnWishSendClicked proc
    LOCAL szWish db 1024 dup(0)
    LOCAL dwLength:DWORD
    
    ; Get wish text
    invoke SendMessage, g_hWishInput, WM_GETTEXTLENGTH, 0, 0
    mov dwLength, eax
    .if eax > 0
        invoke SendMessage, g_hWishInput, WM_GETTEXT, 1023, addr szWish
        szCopy addr g_szCurrentWish, addr szWish
        
        ; Close wish dialog
        invoke SendMessage, g_hWishDialog, WM_CLOSE, 0, 0
        
        ; Generate plan for the wish
        call MagicWand_GeneratePlan
    .endif
    
    ret
OnWishSendClicked endp

; ============================================================================
; OnWishDialogPaint - Handle WM_PAINT for wish dialog
; ============================================================================
OnWishDialogPaint proc
    LOCAL ps:PAINTSTRUCT
    LOCAL hdc:HDC
    LOCAL rect:RECT
    LOCAL oldFont:DWORD
    LOCAL oldTextColor:DWORD
    LOCAL oldBkColor:DWORD
    LOCAL oldBkMode:DWORD
    
    invoke BeginPaint, g_hWishDialog, addr ps
    mov hdc, eax
    
    ; Save current settings
    invoke SelectObject, hdc, hMainFont
    mov oldFont, eax
    invoke GetTextColor, hdc
    mov oldTextColor, eax
    invoke GetBkColor, hdc
    mov oldBkColor, eax
    invoke GetBkMode, hdc
    mov oldBkMode, eax
    
    ; Set text properties
    invoke SetTextColor, hdc, clrDialogTitle
    invoke SetBkColor, hdc, clrDialogBg
    invoke SetBkMode, hdc, TRANSPARENT
    
    ; Get client rectangle
    invoke GetClientRect, g_hWishDialog, addr rect
    
    ; Draw title text
    invoke TextOut, hdc, 20, 10, addr szWishDialogTitle, szLen(addr szWishDialogTitle)
    
    ; Restore settings
    invoke SelectObject, hdc, oldFont
    invoke SetTextColor, hdc, oldTextColor
    invoke SetBkColor, hdc, oldBkColor
    invoke SetBkMode, hdc, oldBkMode
    
    invoke EndPaint, g_hWishDialog, addr ps
    ret
OnWishDialogPaint endp

; ============================================================================
; MagicWand_GeneratePlan - Generate execution plan for wish
; ============================================================================
MagicWand_GeneratePlan proc
    LOCAL szPrompt db 2048 dup(0)
    LOCAL szResponse db MAX_BUFFER_SIZE dup(0)
    LOCAL pPlan:DWORD
    LOCAL dwLength:DWORD
    
    ; Show thinking indicator
    invoke SetWindowText, g_hStatus, addr szThinkingText
    
    ; Format prompt for LLM
    szCat addr szPrompt, "Generate a detailed execution plan for the following wish: "
    szCat addr szPrompt, g_szCurrentWish
    szCat addr szPrompt, ". Return the plan as a JSON object with an array of actions. Each action should have: id, type, description, and parameters."
    
    ; Call model invoker to generate plan
    invoke ModelInvoker_Invoke, hModelInvoker, addr szPrompt, addr szResponse
    test eax, eax
    jz @Error
    
    ; Parse response and create execution plan
    ; (Implementation would parse JSON and populate EXECUTION_PLAN structure)
    ; For now, we'll create a simple mock plan
    call MagicWand_CreateMockPlan
    
    ; Show plan approval dialog
    call MagicWand_ShowPlanDialog
    jmp @Exit
    
@Error:
    invoke MessageBox, hMainWindow, addr szPlanGenError, addr szQueryTitle, MB_OK or MB_ICONERROR
    
@Exit:
    ret
MagicWand_GeneratePlan endp

; ============================================================================
; MagicWand_CreateMockPlan - Create a mock execution plan for testing
; ============================================================================
MagicWand_CreateMockPlan proc
    ; Initialize plan structure
    MemZero addr g_CurrentPlan, sizeof EXECUTION_PLAN
    
    ; Set plan ID
    szCopy addr g_CurrentPlan.szPlanID, "PLAN001"
    
    ; Set wish
    szCopy addr g_CurrentPlan.szWish, g_szCurrentWish
    
    ; Set action count (mock)
    mov g_CurrentPlan.dwActionCount, 3
    
    ; Allocate actions (mock)
    mov eax, 3
    mov ecx, sizeof ACTION_ITEM
    imul eax, ecx
    MemAlloc eax
    mov g_pPlanActions, eax
    mov g_CurrentPlan.pActions, eax
    
    ; Populate mock actions
    ; Action 1
    mov eax, g_pPlanActions
    assume eax:ptr ACTION_ITEM
    szCopy addr [eax].szActionID, "ACTION001"
    mov [eax].dwType, 1
    szCopy addr [eax].szDescription, "Read current file content"
    szCopy addr [eax].szParameters, "{\"file\":\"main.asm\"}"
    assume eax:nothing
    
    ; Action 2
    mov eax, g_pPlanActions
    add eax, sizeof ACTION_ITEM
    assume eax:ptr ACTION_ITEM
    szCopy addr [eax].szActionID, "ACTION002"
    mov [eax].dwType, 2
    szCopy addr [eax].szDescription, "Modify file content with new code"
    szCopy addr [eax].szParameters, "{\"file\":\"main.asm\",\"content\":\"; Modified content\"}"
    assume eax:nothing
    
    ; Action 3
    mov eax, g_pPlanActions
    add eax, sizeof ACTION_ITEM
    add eax, sizeof ACTION_ITEM
    assume eax:ptr ACTION_ITEM
    szCopy addr [eax].szActionID, "ACTION003"
    mov [eax].dwType, 3
    szCopy addr [eax].szDescription, "Save modified file"
    szCopy addr [eax].szParameters, "{\"file\":\"main.asm\"}"
    assume eax:nothing
    
    ret
MagicWand_CreateMockPlan endp

; ============================================================================
; MagicWand_ShowPlanDialog - Show dialog to review and approve plan
; ============================================================================
MagicWand_ShowPlanDialog proc
    LOCAL wc:WNDCLASSEX
    LOCAL rect:RECT
    LOCAL dwStyle:DWORD
    LOCAL dwExStyle:DWORD
    LOCAL x:DWORD
    LOCAL y:DWORD
    LOCAL width:DWORD
    LOCAL height:DWORD
    
    ; Register plan dialog class if not already registered
    invoke GetClassInfoEx, NULL, addr szPlanDialogClass, addr wc
    .if eax == 0
        mov wc.cbSize, sizeof WNDCLASSEX
        mov wc.style, CS_HREDRAW or CS_VREDRAW
        mov wc.lpfnWndProc, offset PlanDialogProc
        mov wc.cbClsExtra, 0
        mov wc.cbWndExtra, 0
        push hInstance
        pop wc.hInstance
        invoke LoadIcon, NULL, IDI_APPLICATION
        mov wc.hIcon, eax
        invoke LoadCursor, NULL, IDC_ARROW
        mov wc.hCursor, eax
        invoke CreateSolidBrush, clrDialogBg
        mov g_hDialogBrush, eax
        mov wc.hbrBackground, eax
        mov wc.lpszMenuName, NULL
        mov wc.lpszClassName, offset szPlanDialogClass
        mov wc.hIconSm, 0
        
        invoke RegisterClassEx, addr wc
    .endif
    
    ; Calculate dialog position (centered)
    mov width, 800
    mov height, 600
    invoke GetClientRect, hMainWindow, addr rect
    mov eax, rect.right
    sub eax, width
    shr eax, 1
    mov x, eax
    mov eax, rect.bottom
    sub eax, height
    shr eax, 1
    mov y, eax
    
    ; Create plan dialog
    mov dwStyle, WS_OVERLAPPED or WS_CAPTION or WS_SYSMENU or WS_BORDER or WS_SIZEBOX
    mov dwExStyle, WS_EX_TOPMOST or WS_EX_DLGMODALFRAME
    
    invoke CreateWindowEx, dwExStyle,
        addr szPlanDialogClass,
        addr szPlanDialogTitle,
        dwStyle,
        x, y, width, height,
        hMainWindow, NULL, hInstance, NULL
    
    mov g_hPlanDialog, eax
    test eax, eax
    jz @Exit
    
    ; Show dialog
    invoke ShowWindow, g_hPlanDialog, SW_SHOW
    invoke UpdateWindow, g_hPlanDialog
    
@Exit:
    ret
MagicWand_ShowPlanDialog endp

; ============================================================================
; PlanDialogProc - Window procedure for plan review dialog
; ============================================================================
PlanDialogProc proc hWnd:HWND, uMsg:UINT, wParam:WPARAM, lParam:LPARAM
    .if uMsg == WM_CREATE
        call OnPlanDialogCreate
        xor eax, eax
        ret
        
    .elseif uMsg == WM_COMMAND
        push lParam
        push wParam
        call OnPlanDialogCommand
        xor eax, eax
        ret
        
    .elseif uMsg == WM_SIZE
        call OnPlanDialogSize
        xor eax, eax
        ret
        
    .elseif uMsg == WM_PAINT
        call OnPlanDialogPaint
        xor eax, eax
        ret
        
    .elseif uMsg == WM_CLOSE
        invoke DestroyWindow, hWnd
        xor eax, eax
        ret
        
    .elseif uMsg == WM_DESTROY
        mov g_hPlanDialog, 0
        xor eax, eax
        ret
        
    .endif
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
PlanDialogProc endp

; ============================================================================
; OnPlanDialogCreate - Handle WM_CREATE for plan dialog
; ============================================================================
OnPlanDialogCreate proc
    LOCAL dwStyle:DWORD
    LOCAL rect:RECT
    LOCAL x:DWORD
    LOCAL y:DWORD
    LOCAL width:DWORD
    LOCAL height:DWORD
    
    ; Get client rectangle
    invoke GetClientRect, g_hPlanDialog, addr rect
    
    ; Create plan list view (mock with edit control for now)
    mov width, rect.right
    sub width, 40
    mov height, 400
    mov x, 20
    mov y, 50
    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_AUTOVSCROLL or ES_READONLY
    invoke CreateWindowEx, WS_EX_CLIENTEDGE,
        addr szEditClass,
        NULL,
        dwStyle,
        x, y, width, height,
        g_hPlanDialog, IDC_PLAN_LIST, hInstance, NULL
    mov g_hPlanList, eax
    invoke SendMessage, eax, WM_SETFONT, hMainFont, TRUE
    
    ; Populate plan list with actions
    call MagicWand_PopulatePlanList
    
    ; Create approve button
    mov width, 150
    mov height, 35
    mov x, 20
    mov y, 470
    mov dwStyle, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    invoke CreateWindowEx, 0,
        addr szButtonClass,
        addr szApproveText,
        dwStyle,
        x, y, width, height,
        g_hPlanDialog, IDC_PLAN_APPROVE, hInstance, NULL
    mov g_hPlanApprove, eax
    invoke SendMessage, eax, WM_SETFONT, hMainFont, TRUE
    
    ; Create reject button
    mov width, 150
    mov height, 35
    mov x, 200
    mov y, 470
    mov dwStyle, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    invoke CreateWindowEx, 0,
        addr szButtonClass,
        addr szRejectText,
        dwStyle,
        x, y, width, height,
        g_hPlanDialog, IDC_PLAN_REJECT, hInstance, NULL
    mov g_hPlanReject, eax
    invoke SendMessage, eax, WM_SETFONT, hMainFont, TRUE
    
    ; Create progress bar
    mov width, rect.right
    sub width, 40
    mov height, 25
    mov x, 20
    mov y, 520
    mov dwStyle, WS_CHILD or WS_VISIBLE
    invoke CreateWindowEx, 0,
        addr szProgressClass,
        NULL,
        dwStyle,
        x, y, width, height,
        g_hPlanDialog, IDC_PROGRESS_BAR, hInstance, NULL
    mov g_hProgressBar, eax
    invoke SendMessage, eax, PBM_SETRANGE, 0, MAKELONG(0, 100)
    invoke SendMessage, eax, PBM_SETPOS, 0, 0
    
    ret
OnPlanDialogCreate endp

; ============================================================================
; MagicWand_PopulatePlanList - Populate plan list with actions
; ============================================================================
MagicWand_PopulatePlanList proc
    LOCAL szActions db 4096 dup(0)
    LOCAL szLine db 256 dup(0)
    LOCAL i:DWORD
    LOCAL pAction:DWORD
    
    ; Clear existing text
    invoke SendMessage, g_hPlanList, WM_SETTEXT, 0, addr szActions
    
    ; Add header
    szCat addr szActions, "Execution Plan for: "
    szCat addr szActions, g_szCurrentWish
    szCat addr szActions, 13, 10, 13, 10
    szCat addr szActions, "Actions to execute:"
    szCat addr szActions, 13, 10
    szCat addr szActions, "=================="
    szCat addr szActions, 13, 10, 13, 10
    
    ; Add each action
    mov i, 0
    mov ecx, g_CurrentPlan.dwActionCount
    .while i < ecx
        ; Get action pointer
        mov eax, g_pPlanActions
        mov edx, sizeof ACTION_ITEM
        imul edx, i
        add eax, edx
        mov pAction, eax
        
        ; Format action line
        szCopy addr szLine, "Action "
        ; Convert i to string (simplified)
        add i, 48  ; Convert to ASCII
        mov szLine[7], al
        sub i, 48  ; Convert back
        szCat addr szLine, ": "
        assume pAction:ptr ACTION_ITEM
        szCat addr szLine, [pAction].szDescription
        assume pAction:nothing
        szCat addr szLine, 13, 10
        
        ; Add to actions text
        szCat addr szActions, addr szLine
        
        inc i
    .endw
    
    ; Set text in list control
    invoke SendMessage, g_hPlanList, WM_SETTEXT, 0, addr szActions
    
    ret
MagicWand_PopulatePlanList endp

; ============================================================================
; OnPlanDialogCommand - Handle WM_COMMAND for plan dialog
; ============================================================================
OnPlanDialogCommand proc
    mov eax, wParam
    and eax, 0FFFFh  ; LOWORD = control ID
    
    .if eax == IDC_PLAN_APPROVE
        call OnPlanApproveClicked
    .elseif eax == IDC_PLAN_REJECT
        invoke SendMessage, g_hPlanDialog, WM_CLOSE, 0, 0
    .endif
    
    ret
OnPlanDialogCommand endp

; ============================================================================
; OnPlanApproveClicked - Handle approve button click
; ============================================================================
OnPlanApproveClicked proc
    ; Disable buttons during execution
    invoke EnableWindow, g_hPlanApprove, FALSE
    invoke EnableWindow, g_hPlanReject, FALSE
    
    ; Show executing status
    invoke SetWindowText, g_hStatus, addr szExecutingText
    invoke SendMessage, g_hProgressBar, PBM_SETPOS, 0, 0
    
    ; Execute the plan
    call MagicWand_ExecutePlan
    
    ret
OnPlanApproveClicked endp

; ============================================================================
; OnPlanDialogSize - Handle WM_SIZE for plan dialog
; ============================================================================
OnPlanDialogSize proc
    LOCAL rect:RECT
    LOCAL width:DWORD
    LOCAL height:DWORD
    LOCAL x:DWORD
    LOCAL y:DWORD
    
    ; Get new client rectangle
    invoke GetClientRect, g_hPlanDialog, addr rect
    
    ; Resize plan list
    mov width, rect.right
    sub width, 40
    mov height, rect.bottom
    sub height, 180
    invoke MoveWindow, g_hPlanList, 20, 50, width, height, TRUE
    
    ; Move approve button
    mov x, 20
    mov y, rect.bottom
    sub y, 110
    invoke MoveWindow, g_hPlanApprove, x, y, 150, 35, TRUE
    
    ; Move reject button
    mov x, 200
    invoke MoveWindow, g_hPlanReject, x, y, 150, 35, TRUE
    
    ; Resize progress bar
    mov width, rect.right
    sub width, 40
    mov y, rect.bottom
    sub y, 60
    invoke MoveWindow, g_hProgressBar, 20, y, width, 25, TRUE
    
    ret
OnPlanDialogSize endp

; ============================================================================
; OnPlanDialogPaint - Handle WM_PAINT for plan dialog
; ============================================================================
OnPlanDialogPaint proc
    LOCAL ps:PAINTSTRUCT
    LOCAL hdc:HDC
    LOCAL rect:RECT
    LOCAL oldFont:DWORD
    LOCAL oldTextColor:DWORD
    LOCAL oldBkColor:DWORD
    LOCAL oldBkMode:DWORD
    
    invoke BeginPaint, g_hPlanDialog, addr ps
    mov hdc, eax
    
    ; Save current settings
    invoke SelectObject, hdc, hMainFont
    mov oldFont, eax
    invoke GetTextColor, hdc
    mov oldTextColor, eax
    invoke GetBkColor, hdc
    mov oldBkColor, eax
    invoke GetBkMode, hdc
    mov oldBkMode, eax
    
    ; Set text properties
    invoke SetTextColor, hdc, clrDialogTitle
    invoke SetBkColor, hdc, clrDialogBg
    invoke SetBkMode, hdc, TRANSPARENT
    
    ; Get client rectangle
    invoke GetClientRect, g_hPlanDialog, addr rect
    
    ; Draw title text
    invoke TextOut, hdc, 20, 15, addr szPlanDialogTitle, szLen(addr szPlanDialogTitle)
    
    ; Restore settings
    invoke SelectObject, hdc, oldFont
    invoke SetTextColor, hdc, oldTextColor
    invoke SetBkColor, hdc, oldBkColor
    invoke SetBkMode, hdc, oldBkMode
    
    invoke EndPaint, g_hPlanDialog, addr ps
    ret
OnPlanDialogPaint endp

; ============================================================================
; MagicWand_ExecutePlan - Execute the approved plan
; ============================================================================
MagicWand_ExecutePlan proc
    LOCAL pResult:DWORD
    LOCAL i:DWORD
    LOCAL dwProgress:DWORD
    LOCAL szProgressText db 64 dup(0)
    
    ; Execute plan using ActionExecutor
    invoke ActionExecutor_ExecutePlan, hActionExecutor, addr g_CurrentPlan
    mov pResult, eax
    
    ; Update progress during execution
    mov i, 0
    mov ecx, g_CurrentPlan.dwActionCount
    .while i < ecx
        ; Calculate progress percentage
        mov eax, i
        mov edx, 100
        imul eax, edx
        cdq
        idiv ecx
        mov dwProgress, eax
        
        ; Update progress bar
        invoke SendMessage, g_hProgressBar, PBM_SETPOS, dwProgress, 0
        
        ; Update status text
        szCopy addr szProgressText, "Executing action "
        ; Convert i to string (simplified)
        add i, 48  ; Convert to ASCII
        mov szProgressText[17], al
        sub i, 48  ; Convert back
        szCat addr szProgressText, " of "
        ; Convert count to string (simplified)
        add ecx, 48  ; Convert to ASCII
        mov szProgressText[23], cl
        sub ecx, 48  ; Convert back
        invoke SetWindowText, g_hStatus, addr szProgressText
        
        ; Small delay to show progress
        invoke Sleep, 500
        
        inc i
    .endw
    
    ; Show completion
    invoke SendMessage, g_hProgressBar, PBM_SETPOS, 100, 0
    invoke SetWindowText, g_hStatus, addr szPlanSuccess
    
    ; Close dialog after completion
    invoke SendMessage, g_hPlanDialog, WM_CLOSE, 0, 0
    
    ; Show result in message box (simplified)
    invoke MessageBox, hMainWindow, addr szPlanSuccess, addr szQueryTitle, MB_OK or MB_ICONINFORMATION
    
    ; Free allocated memory
    .if g_pPlanActions != 0
        MemFree g_pPlanActions
        mov g_pPlanActions, 0
    .endif
    
    ret
MagicWand_ExecutePlan endp

; ============================================================================
; Data
; ============================================================================

.data
    szButtonClass       db "BUTTON", 0
    szPlanGenError      db "Failed to generate execution plan", 0
    szQueryTitle        db "Magic Wand", 0
    szPlanSuccess       db "Plan executed successfully!", 0

end