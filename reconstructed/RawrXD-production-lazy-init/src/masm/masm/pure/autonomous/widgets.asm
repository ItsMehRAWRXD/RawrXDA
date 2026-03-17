; ============================================================================
; AUTONOMOUS_WIDGETS.ASM - Pure MASM64 Win32 Widget Implementation
; ============================================================================
; Implements autonomous IDE feature widgets using pure Win32 API
; No Qt dependencies - integrates with ui_masm.asm and qt_pane_system.asm
; ============================================================================

.code

; External Win32 API declarations
extern CreateWindowExA: proc
extern DestroyWindow: proc
extern ShowWindow: proc
extern UpdateWindow: proc
extern SetWindowTextA: proc
extern GetWindowTextA: proc
extern SendMessageA: proc
extern LoadIconA: proc
extern LoadCursorA: proc
extern GetClientRect: proc
extern InvalidateRect: proc
extern SetWindowLongPtrA: proc
extern GetWindowLongPtrA: proc
extern GlobalAlloc: proc
extern GlobalFree: proc

; Common controls (ListView, ProgressBar, RichEdit)
extern InitCommonControlsEx: proc

; Win32 constants
GMEM_FIXED          equ 0
GMEM_ZEROINIT       equ 40h
WS_CHILD            equ 40000000h
WS_VISIBLE          equ 10000000h
WS_BORDER           equ 800000h
WS_VSCROLL          equ 200000h
WS_HSCROLL          equ 100000h
ES_MULTILINE        equ 4h
ES_READONLY         equ 800h
ES_AUTOVSCROLL      equ 40h
ES_AUTOHSCROLL      equ 80h
BS_PUSHBUTTON       equ 0h
BS_DEFPUSHBUTTON    equ 1h
SW_SHOW             equ 5
SW_HIDE             equ 0
CW_USEDEFAULT       equ 80000000h
WM_SETTEXT          equ 0Ch
TRUE                equ 1
FALSE               equ 0

; ListView styles and messages
LVS_REPORT          equ 1h
LVS_SINGLESEL       equ 4h
LVS_SHOWSELALWAYS   equ 8h
LVM_INSERTCOLUMNA   equ 1000h + 27
LVM_INSERTITEMA     equ 1000h + 7
LVM_SETITEMA        equ 1000h + 6
LVM_GETITEMA        equ 1000h + 5
LVM_DELETEALLITEMS  equ 1000h + 9
LVCF_TEXT           equ 4h
LVCF_WIDTH          equ 2h
LVIF_TEXT           equ 1h
LVIF_PARAM          equ 4h

; ProgressBar styles and messages
PBS_SMOOTH          equ 1h
PBM_SETRANGE        equ 400h + 1
PBM_SETPOS          equ 400h + 2

; RichEdit messages
EM_SETCHARFORMAT    equ 1000h + 68
EM_STREAMIN         equ 1000h + 73
EM_STREAMOUT        equ 1000h + 74
EM_SETBKGNDCOLOR    equ 1000h + 67

; Button control notifications
BN_CLICKED          equ 0

; Window messages
WM_CREATE           equ 1h
WM_DESTROY          equ 2h
WM_PAINT            equ 0Fh
WM_COMMAND          equ 111h
WM_CTLCOLORBTN      equ 135h
WM_CTLCOLORSTATIC   equ 138h

; Control IDs (matching ui_masm.asm conventions)
IDC_SUGGESTION_PREVIEW      equ 5001
IDC_SUGGESTION_BTN_ACCEPT   equ 5002
IDC_SUGGESTION_BTN_REJECT   equ 5003
IDC_SECURITY_LISTVIEW       equ 5010
IDC_SECURITY_BTN_FIX        equ 5011
IDC_SECURITY_STATIC_SEVERITY equ 5012
IDC_OPTIMIZATION_LISTVIEW   equ 5020
IDC_OPTIMIZATION_PROGRESSBAR equ 5021
IDC_OPTIMIZATION_BTN_APPLY  equ 5022

; Include structures from autonomous_features.asm
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib comctl32.lib

; External structure definitions (from autonomous_features.asm)
AUTONOMOUS_SUGGESTION struct
    suggestionId        db 64 dup(?)
    suggestionType      db 32 dup(?)
    filePath            db 260 dup(?)
    lineNumber          dd ?
    originalCode        db 512 dup(?)
    suggestedCode       db 512 dup(?)
    explanation         db 512 dup(?)
    confidence          real8 ?
    wasAccepted         dd ?
    timestampLow        dd ?
    timestampHigh       dd ?
    pBenefitsList       dq ?
    benefitsCount       dd ?
    pMetadataMap        dq ?
    metadataCount       dd ?
    reserved1           dd ?
    reserved2           dd ?
AUTONOMOUS_SUGGESTION ends

SECURITY_ISSUE struct
    issueId             db 64 dup(?)
    severity            db 16 dup(?)
    issueType           db 64 dup(?)
    filePath            db 260 dup(?)
    lineNumber          dd ?
    vulnerableCode      db 512 dup(?)
    description         db 512 dup(?)
    suggestedFix        db 512 dup(?)
    cveReference        db 64 dup(?)
    riskScore           real8 ?
    pAffectedComponents dq ?
    componentCount      dd ?
    reserved1           dd ?
    reserved2           dd ?
SECURITY_ISSUE ends

PERFORMANCE_OPTIMIZATION struct
    optimizationId      db 64 dup(?)
    optimizationType    db 32 dup(?)
    filePath            db 260 dup(?)
    lineNumber          dd ?
    currentImplementation db 512 dup(?)
    optimizedImplementation db 512 dup(?)
    reasoning           db 512 dup(?)
    expectedSpeedup     real8 ?
    expectedMemorySaving dq ?
    confidence          real8 ?
    reserved1           dd ?
    reserved2           dd ?
PERFORMANCE_OPTIMIZATION ends

SUGGESTION_WIDGET struct
    paneID              dd ?
    hWnd                dq ?
    dwFlags             dd ?
    dwDockMode          dd ?
    pSuggestion         dq ?
    hBtnAccept          dq ?
    hBtnReject          dq ?
    hEditPreview        dq ?
    reserved            dd ?
SUGGESTION_WIDGET ends

SECURITY_ALERT_WIDGET struct
    paneID              dd ?
    hWnd                dq ?
    dwFlags             dd ?
    dwDockMode          dd ?
    pIssue              dq ?
    hListView           dq ?
    hStaticSeverity     dq ?
    hBtnFix             dq ?
    reserved            dd ?
SECURITY_ALERT_WIDGET ends

OPTIMIZATION_PANEL_WIDGET struct
    paneID              dd ?
    hWnd                dq ?
    dwFlags             dd ?
    dwDockMode          dd ?
    pOptimization       dq ?
    hListView           dq ?
    hProgressBar        dq ?
    hBtnApply           dq ?
    reserved            dd ?
OPTIMIZATION_PANEL_WIDGET ends

RECT STRUCT
    left    DWORD ?
    top     DWORD ?
    right   DWORD ?
    bottom  DWORD ?
RECT ENDS

LVCOLUMNA struct
    _mask       DWORD ?    ; renamed from 'mask' (reserved word)
    fmt         DWORD ?
    _cx         DWORD ?    ; renamed from 'cx' (register name)
    pszText     QWORD ?
    cchTextMax  DWORD ?
    iSubItem    DWORD ?
LVCOLUMNA ends

LVITEMA struct
    _mask       DWORD ?    ; renamed from 'mask' (reserved word)
    iItem       DWORD ?
    iSubItem    DWORD ?
    state       DWORD ?
    stateMask   DWORD ?
    pszText     QWORD ?
    cchTextMax  DWORD ?
    iImage      DWORD ?
    lParam      QWORD ?
LVITEMA ends

.data
    ; Class names
    szRichEditClass     BYTE "RichEdit20A",0
    szEditClass         BYTE "EDIT",0
    szButtonClass       BYTE "BUTTON",0
    szStaticClass       BYTE "STATIC",0
    szListViewClass     BYTE "SysListView32",0
    szProgressBarClass  BYTE "msctls_progress32",0
    
    ; Widget titles
    szSuggestionTitle   BYTE "Autonomous Suggestion",0
    szSecurityTitle     BYTE "Security Alert",0
    szOptimizationTitle BYTE "Performance Optimization",0
    
    ; Button labels
    szAccept            BYTE "Accept",0
    szReject            BYTE "Reject",0
    szApplyFix          BYTE "Apply Fix",0
    szApplyOptimization BYTE "Apply Optimization",0
    
    ; ListView column headers
    szColIssue          BYTE "Issue",0
    szColSeverity       BYTE "Severity",0
    szColType           BYTE "Type",0
    szColLocation       BYTE "Location",0
    szColOptimization   BYTE "Optimization",0
    szColSpeedup        BYTE "Expected Speedup",0
    szColMemory         BYTE "Memory Saved",0
    
    ; Severity labels
    szCritical          BYTE "CRITICAL",0
    szHigh              BYTE "HIGH",0
    szMedium            BYTE "MEDIUM",0
    szLow               BYTE "LOW",0
    
    ; Format strings
    szLineFormat        BYTE "Line %d",0
    szSpeedupFormat     BYTE "%.2fx",0
    szMemoryFormat      BYTE "%lld bytes",0
    szConfidenceFormat  BYTE "Confidence: %.0f%%",0

    ; Global widget arrays (from autonomous_features.asm)
    extern g_SuggestionWidgets:DWORD
    extern g_SuggestionCount:DWORD
    extern g_SecurityAlertWidgets:DWORD
    extern g_SecurityAlertCount:DWORD
    extern g_OptimizationWidgets:DWORD
    extern g_OptimizationCount:DWORD
    
    ; Theme colors
    extern g_clrSuggestionAccent:DWORD
    extern g_clrSecurityCritical:DWORD
    extern g_clrSecurityHigh:DWORD
    extern g_clrSecurityMedium:DWORD
    extern g_clrSecurityLow:DWORD
    extern g_clrOptimizationGood:DWORD

.code

; ============================================================================
; SUGGESTION WIDGET IMPLEMENTATION
; ============================================================================

; SuggestionWidget_Create - Create autonomous suggestion widget
; Parameters:
;   RCX: hParentWnd (parent window handle)
;   RDX: pSuggestion (pointer to AUTONOMOUS_SUGGESTION)
;   R8:  x position
;   R9:  y position
; Stack: width, height
; Returns: Pointer to SUGGESTION_WIDGET in RAX
public SuggestionWidget_Create
SuggestionWidget_Create proc
    LOCAL widget:QWORD
    LOCAL hWnd:QWORD
    LOCAL hPreview:QWORD
    LOCAL hBtnAccept:QWORD
    LOCAL hBtnReject:QWORD
    LOCAL clientRect:RECT
    
    push rbx
    push rsi
    push rdi
    sub rsp, 80h                               ; Extra stack space for Win32 calls
    
    mov rbx, rcx                               ; Save hParentWnd
    mov rsi, rdx                               ; Save pSuggestion
    mov rdi, r8                                ; Save x
    
    ; Allocate widget structure
    mov rcx, GMEM_FIXED or GMEM_ZEROINIT
    mov rdx, sizeof SUGGESTION_WIDGET
    call GlobalAlloc
    test rax, rax
    jz @SugCreateFailed
    mov widget, rax
    
    ; Create main widget window (child of parent)
    sub rsp, 20h
    push 0                                     ; lpParam
    push 0                                     ; hInstance
    push 0                                     ; hMenu
    push rbx                                   ; hParent
    mov r9d, [rsp + 0B0h]                      ; height from original stack
    mov r8d, [rsp + 0A8h]                      ; width from original stack
    mov edx, [rsp + 98h]                       ; y from saved r9
    mov ecx, edi                               ; x
    push rcx                                   ; x
    push rdx                                   ; y
    push r8                                    ; width
    push r9                                    ; height
    lea rax, szStaticClass
    push rax                                   ; lpClassName
    lea rax, szSuggestionTitle
    push rax                                   ; lpWindowName
    push WS_CHILD or WS_VISIBLE or WS_BORDER   ; dwStyle
    push 0                                     ; dwExStyle
    call CreateWindowExA
    add rsp, 60h
    
    test rax, rax
    jz @SugCreateFailed
    mov hWnd, rax
    
    ; Store window handle in widget
    mov rcx, widget
    mov [rcx].SUGGESTION_WIDGET.hWnd, rax
    mov [rcx].SUGGESTION_WIDGET.pSuggestion, rsi
    
    ; Create RichEdit preview control (top 70% of widget)
    sub rsp, 20h
    push 0                                     ; lpParam
    push 0                                     ; hInstance
    push IDC_SUGGESTION_PREVIEW                ; hMenu (control ID)
    push hWnd                                  ; hParent
    mov r9d, [rsp + 0B0h]                      ; height
    imul r9d, 70
    mov eax, 100
    cdq
    idiv r9d
    mov r9d, eax                               ; height * 0.7
    mov r8d, [rsp + 0A8h]                      ; width
    sub r8d, 20                                ; Subtract margins
    mov edx, 10                                ; y offset
    mov ecx, 10                                ; x offset
    push 10                                    ; x
    push 10                                    ; y
    push r8                                    ; width
    push r9                                    ; height
    lea rax, szRichEditClass
    push rax                                   ; lpClassName
    push 0                                     ; lpWindowName
    push WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_READONLY or ES_AUTOVSCROLL
    push 0                                     ; dwExStyle
    call CreateWindowExA
    add rsp, 60h
    
    mov hPreview, rax
    mov rcx, widget
    mov [rcx].SUGGESTION_WIDGET.hEditPreview, rax
    
    ; Populate preview with suggestion code
    test rax, rax
    jz @SkipPreview
    
    ; Set preview text: originalCode -> suggestedCode
    ; TODO: Use EM_SETCHARFORMAT for syntax highlighting
    mov rcx, hPreview
    mov rdx, WM_SETTEXT
    lea r8, [rsi].AUTONOMOUS_SUGGESTION.suggestedCode
    call SendMessageA
    
@SkipPreview:
    ; Create Accept button (bottom-left, 30% height section)
    sub rsp, 20h
    mov rax, widget
    mov rcx, [rax].SUGGESTION_WIDGET.hWnd
    
    push 0                                     ; lpParam
    push 0                                     ; hInstance
    push IDC_SUGGESTION_BTN_ACCEPT             ; control ID
    push rcx                                   ; hParent (widget window)
    push 30                                    ; height
    mov r8d, [rsp + 0C8h]                      ; original width
    shr r8d, 1                                 ; width / 2
    sub r8d, 25                                ; (width/2) - 25
    push r8                                    ; width
    mov r9d, [rsp + 0D0h]                      ; original height
    imul r9d, 70
    mov eax, 100
    cdq
    idiv r9d
    add eax, 15                                ; y = (height * 0.7) + 15
    push rax                                   ; y
    push 10                                    ; x
    lea rax, szButtonClass
    push rax                                   ; lpClassName
    lea rax, szAccept
    push rax                                   ; lpWindowName
    push WS_CHILD or WS_VISIBLE or BS_DEFPUSHBUTTON
    push 0
    call CreateWindowExA
    add rsp, 60h
    
    mov hBtnAccept, rax
    mov rcx, widget
    mov [rcx].SUGGESTION_WIDGET.hBtnAccept, rax
    
    ; Create Reject button (bottom-right)
    sub rsp, 20h
    mov rax, widget
    mov rcx, [rax].SUGGESTION_WIDGET.hWnd
    
    push 0                                     ; lpParam
    push 0                                     ; hInstance
    push IDC_SUGGESTION_BTN_REJECT             ; control ID
    push rcx                                   ; hParent
    push 30                                    ; height
    mov r8d, [rsp + 0C8h]                      ; original width
    shr r8d, 1
    sub r8d, 25                                ; (width/2) - 25
    push r8                                    ; width
    mov r9d, [rsp + 0D0h]                      ; original height
    imul r9d, 70
    mov eax, 100
    cdq
    idiv r9d
    add eax, 15
    push rax                                   ; y
    mov r8d, [rsp + 0D8h]
    shr r8d, 1
    add r8d, 5                                 ; x = (width/2) + 5
    push r8                                    ; x
    lea rax, szButtonClass
    push rax
    lea rax, szReject
    push rax
    push WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    push 0
    call CreateWindowExA
    add rsp, 60h
    
    mov hBtnReject, rax
    mov rcx, widget
    mov [rcx].SUGGESTION_WIDGET.hBtnReject, rax
    
    ; Add to global widget array
    mov eax, g_SuggestionCount
    cmp eax, 32
    jge @SugArrayFull
    
    mov rcx, widget
    lea rdx, g_SuggestionWidgets
    mov [rdx + rax*8], rcx
    inc g_SuggestionCount
    
@SugArrayFull:
    mov rax, widget
    add rsp, 80h
    pop rdi
    pop rsi
    pop rbx
    ret
    
@SugCreateFailed:
    xor rax, rax
    add rsp, 80h
    pop rdi
    pop rsi
    pop rbx
    ret
SuggestionWidget_Create endp

; SuggestionWidget_Destroy - Destroy suggestion widget
; Parameters:
;   RCX: pWidget (pointer to SUGGESTION_WIDGET)
; Returns: Nothing
public SuggestionWidget_Destroy
SuggestionWidget_Destroy proc
    test rcx, rcx
    jz @SugDestroyDone
    
    push rbx
    sub rsp, 20h
    
    mov rbx, rcx
    
    ; Destroy child windows
    mov rcx, [rbx].SUGGESTION_WIDGET.hEditPreview
    test rcx, rcx
    jz @SugSkipPreview
    call DestroyWindow
    
@SugSkipPreview:
    mov rcx, [rbx].SUGGESTION_WIDGET.hBtnAccept
    test rcx, rcx
    jz @SugSkipAccept
    call DestroyWindow
    
@SugSkipAccept:
    mov rcx, [rbx].SUGGESTION_WIDGET.hBtnReject
    test rcx, rcx
    jz @SugSkipReject
    call DestroyWindow
    
@SugSkipReject:
    ; Destroy main window
    mov rcx, [rbx].SUGGESTION_WIDGET.hWnd
    test rcx, rcx
    jz @SugSkipMain
    call DestroyWindow
    
@SugSkipMain:
    ; Free widget structure
    mov rcx, rbx
    call GlobalFree
    
    add rsp, 20h
    pop rbx
    
@SugDestroyDone:
    ret
SuggestionWidget_Destroy endp

; SuggestionWidget_OnAccept - Handle Accept button click
; Parameters:
;   RCX: pWidget
; Returns: Nothing
public SuggestionWidget_OnAccept
SuggestionWidget_OnAccept proc
    push rbx
    sub rsp, 20h
    
    mov rbx, rcx
    
    ; Mark suggestion as accepted
    mov rax, [rbx].SUGGESTION_WIDGET.pSuggestion
    test rax, rax
    jz @AcceptDone
    
    mov [rax].AUTONOMOUS_SUGGESTION.wasAccepted, TRUE
    
    ; TODO: Apply code change to editor
    ; TODO: Send notification to main window
    ; TODO: Close widget
    
@AcceptDone:
    add rsp, 20h
    pop rbx
    ret
SuggestionWidget_OnAccept endp

; SuggestionWidget_OnReject - Handle Reject button click
; Parameters:
;   RCX: pWidget
; Returns: Nothing
public SuggestionWidget_OnReject
SuggestionWidget_OnReject proc
    push rbx
    sub rsp, 20h
    
    mov rbx, rcx
    
    ; Mark suggestion as rejected
    mov rax, [rbx].SUGGESTION_WIDGET.pSuggestion
    test rax, rax
    jz @RejectDone
    
    mov [rax].AUTONOMOUS_SUGGESTION.wasAccepted, FALSE
    
    ; TODO: Close widget
    ; TODO: Send rejection metric
    
@RejectDone:
    add rsp, 20h
    pop rbx
    ret
SuggestionWidget_OnReject endp

; ============================================================================
; SECURITY ALERT WIDGET IMPLEMENTATION
; ============================================================================

; SecurityAlertWidget_Create - Create security alert widget
; Parameters:
;   RCX: hParentWnd
;   RDX: pIssue (pointer to SECURITY_ISSUE)
;   R8:  x
;   R9:  y
; Stack: width, height
; Returns: Pointer to SECURITY_ALERT_WIDGET in RAX
public SecurityAlertWidget_Create
SecurityAlertWidget_Create proc
    ; TODO: Full implementation similar to SuggestionWidget_Create
    ; Key features:
    ; - ListView with columns: Issue, Severity, Type, Location
    ; - Color-coded severity (use LVITEM lParam to store color)
    ; - Static control for severity indicator
    ; - "Apply Fix" button
    ; - Integration with g_SecurityAlertWidgets array
    
    xor rax, rax                               ; Stub - return NULL for now
    ret
SecurityAlertWidget_Create endp

; ============================================================================
; OPTIMIZATION PANEL WIDGET IMPLEMENTATION
; ============================================================================

; OptimizationPanelWidget_Create - Create optimization panel widget
; Parameters:
;   RCX: hParentWnd
;   RDX: pOptimization (pointer to PERFORMANCE_OPTIMIZATION)
;   R8:  x
;   R9:  y
; Stack: width, height
; Returns: Pointer to OPTIMIZATION_PANEL_WIDGET in RAX
public OptimizationPanelWidget_Create
OptimizationPanelWidget_Create proc
    ; TODO: Full implementation
    ; Key features:
    ; - ListView with columns: Optimization, Expected Speedup, Memory Saved
    ; - ProgressBar for speedup visualization (scaled to 10x max)
    ; - "Apply Optimization" button
    ; - Integration with g_OptimizationWidgets array
    
    xor rax, rax                               ; Stub - return NULL for now
    ret
OptimizationPanelWidget_Create endp

; ============================================================================
; UTILITY FUNCTIONS
; ============================================================================

; GetSeverityColor - Get color for security severity level
; Parameters:
;   RCX: lpSeverity (pointer to severity string)
; Returns: COLORREF in EAX
public GetSeverityColor
GetSeverityColor proc
    push rsi
    sub rsp, 20h
    
    mov rsi, rcx
    
    ; Compare severity string
    lea rdx, szCritical
    call CompareStringNoCase
    test eax, eax
    jnz @ReturnCritical
    
    lea rdx, szHigh
    call CompareStringNoCase
    test eax, eax
    jnz @ReturnHigh
    
    lea rdx, szMedium
    call CompareStringNoCase
    test eax, eax
    jnz @ReturnMedium
    
    ; Default to low
    mov eax, g_clrSecurityLow
    jmp @ColorDone
    
@ReturnCritical:
    mov eax, g_clrSecurityCritical
    jmp @ColorDone
    
@ReturnHigh:
    mov eax, g_clrSecurityHigh
    jmp @ColorDone
    
@ReturnMedium:
    mov eax, g_clrSecurityMedium
    
@ColorDone:
    add rsp, 20h
    pop rsi
    ret
GetSeverityColor endp

; CompareStringNoCase - Case-insensitive string comparison
; Parameters:
;   RSI: string1
;   RDX: string2
; Returns: 1 in EAX if equal, 0 if different
CompareStringNoCase proc
    push rbx
    xor rax, rax
    xor rbx, rbx
    
@CompareLoop:
    mov al, byte ptr [rsi]
    mov bl, byte ptr [rdx]
    
    ; Convert to uppercase
    cmp al, 'a'
    jb @CheckB
    cmp al, 'z'
    ja @CheckB
    sub al, 32
    
@CheckB:
    cmp bl, 'a'
    jb @DoCompare
    cmp bl, 'z'
    ja @DoCompare
    sub bl, 32
    
@DoCompare:
    cmp al, bl
    jne @NotEqual
    
    test al, al                                ; Check for null terminator
    jz @Equal
    
    inc rsi
    inc rdx
    jmp @CompareLoop
    
@Equal:
    mov eax, 1
    jmp @CompareDone
    
@NotEqual:
    xor eax, eax
    
@CompareDone:
    pop rbx
    ret
CompareStringNoCase endp

end
