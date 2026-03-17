;==============================================================================
; File 8: model_manager_dialog.asm - Model Download & Management
;==============================================================================
include windows.inc

IDD_MODEL_MANAGER equ 6002

.code
;==============================================================================
; Show Model Manager Dialog
;==============================================================================
ModelManager_Show PROC
    invoke CreateWindowEx, WS_EX_DLGMODALFRAME,
        OFFSET szDialogClass, OFFSET szModelManagerTitle,
        WS_POPUP or WS_CAPTION or WS_SYSMENU,
        200, 150, 700, 500,
        [hMainWnd], NULL, [hInstance], NULL
    
    LOG_INFO "Model manager dialog created"
    
    ret
ModelManager_Show ENDP

;==============================================================================
; Populate Model List
;==============================================================================
ModelManager_PopulateList PROC hListView:QWORD
    LOCAL lvitem:LVITEM
    
    ; Get list view handle
    mov [hModelList], hListView
    
    ; Set extended styles
    invoke SendMessage, hListView, 0x1000 + 36, 0,
        0x00000004 or 0x00000010 or 0x01000000
    
    ; Insert columns
    mov [lvcol.mask], 1 or 4
    mov [lvcol.cx], 200
    mov [lvcol.pszText], OFFSET szColModelName
    invoke SendMessage, hListView, 0x1000 + 27, 0,
        ADDR lvcol
    
    LOG_INFO "Model list populated"
    
    ret
ModelManager_PopulateList ENDP

;==============================================================================
; Data
;==============================================================================
.data
szDialogClass        db '#32770',0
szModelManagerTitle  db 'Model Manager',0
szColModelName       db 'Model Name',0
hModelList           dq ?
lvcol               LVCOLUMN <>

END
