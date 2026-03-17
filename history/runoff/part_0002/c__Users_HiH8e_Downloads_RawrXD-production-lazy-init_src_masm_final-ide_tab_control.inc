; ============================================================================
; Tab Control Include File
; ============================================================================
; Constants and structures for tab_control.asm
; ============================================================================

; Tab Control Functions
EXTERN CreateTabControl:PROC
EXTERN AddTabPage:PROC
EXTERN SetActiveTabPage:PROC
EXTERN GetActiveTabPage:PROC
EXTERN RemoveTabPage:PROC
EXTERN DestroyTabControl:PROC
EXTERN OnTabSelectionChanged:PROC

; Tab Structures
TAB_PAGE STRUCT
    title           QWORD ?
    title_len       DWORD ?
    hwnd            QWORD ?
    content_area    RECT ?
    user_data       QWORD ?
    is_visible      BYTE ?
    padding         BYTE 7 DUP(?)
TAB_PAGE ENDS

TAB_CONTROL STRUCT
    hwnd            QWORD ?
    parent_hwnd     QWORD ?
    pages           QWORD ?
    page_count      DWORD ?
    active_page     DWORD ?
    on_selection_changed QWORD ?
    user_data       QWORD ?
TAB_CONTROL ENDS

; Tab Constants
MAX_TAB_PAGES      EQU 16