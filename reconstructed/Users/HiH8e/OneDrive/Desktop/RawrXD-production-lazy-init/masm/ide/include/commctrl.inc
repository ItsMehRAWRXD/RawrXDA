; Ultra-minimal commctrl definitions for MASM IDE
; Only essential constants needed for UI layout

IFNDEF __RAWRXD_COMMCTRL_INC__
__RAWRXD_COMMCTRL_INC__ EQU 1

; Custom draw notification
IFNDEF NM_CUSTOMDRAW
NM_CUSTOMDRAW           equ -12
ENDIF

; Tab control messages (using hex values to avoid conflicts)
IFNDEF TCM_INSERTITEM
TCM_INSERTITEM          equ 4871  ; 0x1300 + 7
ENDIF
IFNDEF TCM_DELETEITEM
TCM_DELETEITEM          equ 4872  ; 0x1300 + 8
ENDIF
IFNDEF TCM_GETCURSEL
TCM_GETCURSEL           equ 4875  ; 0x1300 + 11
ENDIF
IFNDEF TCM_SETCURSEL
TCM_SETCURSEL           equ 4876  ; 0x1300 + 12
ENDIF

; Tab item masks
IFNDEF TCIF_TEXT
TCIF_TEXT               equ 0001h
ENDIF

; Custom draw return flags
IFNDEF CDRF_DODEFAULT
CDRF_DODEFAULT          equ 00000000h
ENDIF

ENDIF ; __RAWRXD_COMMCTRL_INC__
