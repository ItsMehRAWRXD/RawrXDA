; ============================================================================
; PANESYSTEM.ASM - IDE Pane System (3-Pane Layout: Editor/Browser/Chat)
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

PUBLIC IDEPaneSystem_Initialize
PUBLIC IDEPaneSystem_CreateDefaultLayout

; Pane structure
IdePane STRUCT
    hWindow             dd ?
    dwX                 dd ?
    dwY                 dd ?
    dwWidth             dd ?
    dwHeight            dd ?
    dwType              dd ?    ; 0=Editor, 1=Browser, 2=Chat
    szTitle             dd ?
IdePane ENDS

.data
    g_Panes[3]          IdePane <>
    g_dwPaneCount       dd 0

.code

; ============================================================================
; IDEPaneSystem_Initialize - Initialize pane system
; ============================================================================
IDEPaneSystem_Initialize PROC
    push ebx
    
    ; Initialize pane structures
    mov [g_dwPaneCount], 0
    
    mov eax, 1
    pop ebx
    ret
IDEPaneSystem_Initialize ENDP

; ============================================================================
; IDEPaneSystem_CreateDefaultLayout - Create 3-pane default layout
; Input: ECX = parent window handle
; Output: EAX = number of panes created
; ============================================================================
IDEPaneSystem_CreateDefaultLayout PROC hParent:DWORD
    LOCAL hPane:DWORD
    LOCAL dwClientWidth:DWORD
    LOCAL dwClientHeight:DWORD
    LOCAL rc:RECT
    push ebx
    push esi
    push edi
    
    ; Get client area size
    invoke GetClientRect, hParent, ADDR rc
    mov eax, [rc.right]
    mov dwClientWidth, eax
    mov eax, [rc.bottom]
    mov dwClientHeight, eax
    
    ; Create Editor pane (left 60%)
    invoke CreateWindowExA, 0, "STATIC", "Editor", \
        WS_CHILD or WS_VISIBLE or WS_BORDER, \
        0, 0, dwClientWidth * 3 / 5, dwClientHeight, \
        hParent, 1000h, 0, 0
    mov hPane, eax
    
    ; Create Browser pane (right top 50%)
    invoke CreateWindowExA, 0, "STATIC", "Browser", \
        WS_CHILD or WS_VISIBLE or WS_BORDER, \
        dwClientWidth * 3 / 5, 0, dwClientWidth * 2 / 5, dwClientHeight / 2, \
        hParent, 1001h, 0, 0
    
    ; Create Chat pane (right bottom 50%)
    invoke CreateWindowExA, 0, "STATIC", "Chat (Agent Max Mode Available)", \
        WS_CHILD or WS_VISIBLE or WS_BORDER, \
        dwClientWidth * 3 / 5, dwClientHeight / 2, dwClientWidth * 2 / 5, dwClientHeight / 2, \
        hParent, 1002h, 0, 0
    
    mov [g_dwPaneCount], 3
    mov eax, 3
    
    pop edi
    pop esi
    pop ebx
    ret
IDEPaneSystem_CreateDefaultLayout ENDP

END
