;==============================================================================
; File 17: quick_actions.asm - Floating Action Buttons
;==============================================================================
include windows.inc

.code
;==============================================================================
; Create Quick Actions Bar
;==============================================================================
QuickActions_Create PROC
    invoke CreateWindowEx, 0x00000008 or 0x00000020,
        OFFSET szQuickActionsClass,
        NULL,
        0x80000000,
        50, 200, 50, 200,
        [hMainWnd], NULL, [hInstance], NULL
    mov [hQuickActions], rax
    
    ; Set transparency
    invoke SetLayeredWindowAttributes, rax, 0, 200, 0x00000002
    
    ; Create buttons
    call QuickActions_CreateButton, 0, OFFSET szBtnAgent, 2001
    call QuickActions_CreateButton, 1, OFFSET szBtnFix, 2002
    call QuickActions_CreateButton, 2, OFFSET szBtnTest, 2003
    call QuickActions_CreateButton, 3, OFFSET szBtnDoc, 2004
    
    LOG_INFO "Quick actions bar created"
    
    ret
QuickActions_Create ENDP

;==============================================================================
; Create Action Button
;==============================================================================
QuickActions_CreateButton PROC index:DWORD, lpText:QWORD, cmdId:DWORD
    LOCAL y:DWORD
    
    mov eax, index
    mov ecx, 60
    mul ecx
    add eax, 10
    mov [y], eax
    
    invoke CreateWindowEx, 0, OFFSET szButtonClass, lpText,
        0x00000004 or 0x00000080,  ; BS_OWNERDRAW, WS_VISIBLE
        5, [y], 40, 40,
        [hQuickActions], cmdId, [hInstance], NULL
    
    ret
QuickActions_CreateButton ENDP

;==============================================================================
; Data
;==============================================================================
.data
szQuickActionsClass db 'RawrXD_QuickActions',0
szButtonClass       db 'BUTTON',0
hQuickActions       dq ?

szBtnAgent          db 'A',0
szBtnFix            db 'F',0
szBtnTest           db 'T',0
szBtnDoc            db 'D',0

END
