; ============================================================================
; Dialog System Include File
; ============================================================================
; Constants and structures for dialog_system.asm
; ============================================================================

; Dialog System Functions
EXTERN DialogSystemInit:PROC
EXTERN CreateModalDialog:PROC
EXTERN DialogSystemShutdown:PROC

; Dialog Entry Structure
DIALOG_ENTRY STRUCT
    hwnd            QWORD ?
    parent_hwnd     QWORD ?
    on_command      QWORD ?
    on_init         QWORD ?
    user_data       QWORD ?
    result          DWORD ?
    is_modal        BYTE ?
    padding         BYTE 7 DUP(?)
DIALOG_ENTRY ENDS

; Dialog Constants
MAX_DIALOGS       EQU 32