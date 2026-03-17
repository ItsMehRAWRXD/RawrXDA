; winuser.inc - Minimal User API definitions for assembly
; This is a stub that includes common user window and clipboard definitions
; For production, use full Windows SDK includes

; Do not include windows.inc again - assume it's already included
; This file just adds the user-specific definitions

; Clipboard functions (already defined in windows.inc but repeated for clarity)
; These are EXTERN declarations typically used with:
; EXTERN GetClipboardData:PROC
; EXTERN OpenClipboard:PROC
; EXTERN CloseClipboard:PROC
; EXTERN GlobalLock:PROC
; EXTERN GlobalUnlock:PROC

; Message Box constants
MB_OK             EQU 0h
MB_OKCANCEL       EQU 1h
MB_YESNO          EQU 4h
MB_ICONINFORMATION EQU 40h
MB_ICONWARNING    EQU 30h
MB_ICONSTOP       EQU 10h

IDOK              EQU 1
IDCANCEL          EQU 2
IDYES             EQU 6
IDNO              EQU 7

; Window class atom
WC_OVERLAPPEDWINDOW EQU 0h

; Window creation flags
CW_USEDEFAULT     EQU 80000000h

; Window message constants (if not already defined in windows.inc)
WM_PAINT          EQU 000Fh
WM_KEYDOWN        EQU 0100h
WM_KEYUP          EQU 0101h
WM_LBUTTONDOWN    EQU 0201h
WM_LBUTTONUP      EQU 0202h
WM_MOUSEMOVE      EQU 0200h

; Keyboard key codes
VK_ESCAPE         EQU 001Bh
VK_RETURN         EQU 000Dh
VK_TAB            EQU 0009h

; End of winuser.inc
