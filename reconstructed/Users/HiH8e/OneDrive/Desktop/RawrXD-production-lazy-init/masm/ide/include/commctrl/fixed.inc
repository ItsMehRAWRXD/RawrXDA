; commctrl_fixed.inc - Complete definitions for tree view functionality

; Tree view constants
TVI_ROOT        equ 0xFFFFFFFF
TVI_LAST        equ 0xFFFFFFFF
TVIF_TEXT       equ 0x0001
TVM_INSERTITEM  equ (WM_USER + 50)
TVS_HASBUTTONS  equ 0x0001
TVS_HASLINES    equ 0x0002
TVS_LINESATROOT equ 0x0004
TVS_SHOWSELALWAYS equ 0x0020

; TVITEM structure (simplified)
TVITEM struct
    mask        dd ?
    hItem       dd ?
    state       dd ?
    stateMask   dd ?
    pszText     dd ?
    cchTextMax  dd ?
    iImage      dd ?
    iSelectedImage dd ?
    cChildren   dd ?
    lParam      dd ?
TVITEM ends

; TVINSERTSTRUCT
TVINSERTSTRUCT struct
    hParent         dd ?
    hInsertAfter    dd ?
    item            TVITEM <>
TVINSERTSTRUCT ends