;==============================================================================
; FILE_EXPLORER_INTEGRATION.ASM - Wire file explorer into main IDE
;==============================================================================

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

;==============================================================================
; IMPORTS FROM OTHER MODULES
;==============================================================================

extrn CreateFileExplorer:PROC
extrn RefreshFileTree:PROC
extrn PopulatePath:PROC
extrn OnDriveSelected:PROC
extrn OnTreeItemSelected:PROC
extrn OnTreeItemExpanding:PROC
extrn UpdateBreadcrumb:PROC
extrn GetCurrentPath:PROC

extrn g_hMainWindow:DWORD
extrn g_hInstance:DWORD

;==============================================================================
; CONSTANTS
;==============================================================================

IDC_DRIVE_COMBO         EQU 2001
IDC_FILE_TREE           EQU 2002
IDC_STATUS_BAR          EQU 2003

; Notification codes
WM_NOTIFY               EQU 78h

TVN_FIRST               EQU -400
TVN_SELCHANGED          EQU (TVN_FIRST - 2)
TVN_ITEMEXPANDING       EQU (TVN_FIRST - 5)
TVE_EXPAND              EQU 0002h

;==============================================================================
; DATA
;==============================================================================

.DATA

hFileExplorer           DD 0
szExplorerReady         DB "File explorer ready", 0

;==============================================================================
; CODE
;==============================================================================

.CODE

PUBLIC InitializeFileExplorerUI
PUBLIC HandleFileExplorerNotifications
PUBLIC HandleDriveComboChange

;==============================================================================
; InitializeFileExplorerUI - Create and init file explorer in main window
;==============================================================================
InitializeFileExplorerUI PROC PUBLIC hMainWnd:DWORD, x:DWORD, y:DWORD, width:DWORD, height:DWORD

    ; Initialize common controls
    push 0
    call InitCommonControls
    
    ; Create file explorer
    push height
    push width
    push y
    push x
    push hMainWnd
    call CreateFileExplorer
    mov hFileExplorer, eax
    
    ; Initial breadcrumb update
    call UpdateBreadcrumb
    
    mov eax, 1
    ret

InitializeFileExplorerUI ENDP

;==============================================================================
; HandleFileExplorerNotifications - Process tree notifications
;==============================================================================
HandleFileExplorerNotifications PROC PUBLIC wParam:DWORD, lParam:DWORD

    LOCAL pNMHdr:DWORD
    LOCAL idCtrl:DWORD
    LOCAL code:DWORD
    LOCAL pNMTree:DWORD
    LOCAL itemHandle:DWORD
    LOCAL action:DWORD
    
    ; Get notification structure
    mov eax, lParam
    mov pNMHdr, eax
    
    ; Extract idCtrlFrom and code
    mov eax, [pNMHdr]           ; hwndFrom
    mov eax, [pNMHdr + 4]       ; idFrom
    mov idCtrl, eax
    mov eax, [pNMHdr + 8]       ; code
    mov code, eax
    
    ; Check if it's from file tree
    .IF idCtrl == IDC_FILE_TREE
        
        .IF code == TVN_SELCHANGED
            ; Tree item selected
            mov eax, lParam
            add eax, 12                 ; Offset to TV_ITEM handle
            mov eax, [eax]
            mov itemHandle, eax
            
            push itemHandle
            call OnTreeItemSelected
            
            mov eax, 0                  ; Return 0 (handled)
            ret
        
        .ELSEIF code == TVN_ITEMEXPANDING
            ; Tree item expanding
            mov eax, lParam
            add eax, 12                 ; Offset to action
            mov eax, [eax]
            mov action, eax
            
            .IF action == TVE_EXPAND
                mov eax, lParam
                add eax, 8              ; Offset to item handle
                mov eax, [eax]
                mov itemHandle, eax
                
                push itemHandle
                call OnTreeItemExpanding
            .ENDIF
            
            mov eax, 0                  ; Return 0 (handled)
            ret
        
        .ENDIF
    
    .ELSEIF idCtrl == IDC_DRIVE_COMBO
        
        ; Drive selected (on dropdown change)
        call HandleDriveComboChange
        
        mov eax, 0
        ret
    
    .ENDIF
    
    mov eax, 0
    ret

HandleFileExplorerNotifications ENDP

;==============================================================================
; HandleDriveComboChange - Process drive selection
;==============================================================================
HandleDriveComboChange PROC PUBLIC

    LOCAL hCombo:DWORD
    LOCAL selectedIndex:DWORD
    LOCAL drivePath[4]:BYTE
    
    ; Get combo handle
    mov eax, g_hMainWindow
    push IDC_DRIVE_COMBO
    push eax
    call GetDlgItem
    mov hCombo, eax
    
    ; Get selected index
    push CB_GETCURSEL
    push hCombo
    call SendMessageA
    mov selectedIndex, eax
    
    .IF eax >= 0
        ; Call drive selection handler
        push selectedIndex
        call OnDriveSelected
        
        ; Refresh tree view
        call RefreshFileTree
    .ENDIF
    
    ret

HandleDriveComboChange ENDP

;==============================================================================
; EXTERNAL IMPORTS
;==============================================================================

extrn InitCommonControls:PROC
extrn GetDlgItem:PROC
extrn SendMessageA:PROC

end
