; Win32IDE_Sidebar.asm - Minimal MASM64 Sidebar
; RawrXD IDE Component - Zero External Dependencies

.code

; External imports (kernel32, user32)
EXTERN CreateWindowExA : PROC
EXTERN RegisterClassExA : PROC
EXTERN DefWindowProcA : PROC
EXTERN SendMessageA : PROC
EXTERN PostMessageA : PROC
EXTERN GetMessageA : PROC
EXTERN DispatchMessageA : PROC
EXTERN TranslateMessage : PROC
EXTERN LoadIconA : PROC
EXTERN LoadCursorA : PROC
EXTERN GetStockObject : PROC
EXTERN SetWindowLongPtrA : PROC
EXTERN GetWindowLongPtrA : PROC
EXTERN SetParent : PROC
EXTERN ShowWindow : PROC
EXTERN UpdateWindow : PROC
EXTERN CreateFontA : PROC
EXTERN CreateSolidBrush : PROC
EXTERN DeleteObject : PROC
EXTERN FillRect : PROC
EXTERN GetClientRect : PROC
EXTERN MoveWindow : PROC
EXTERN CreateProcessA : PROC
EXTERN WaitForSingleObject : PROC
EXTERN GetExitCodeProcess : PROC
EXTERN CloseHandle : PROC
EXTERN CreatePipe : PROC
EXTERN SetHandleInformation : PROC
EXTERN ReadFile : PROC
EXTERN WriteFile : PROC
EXTERN GetCurrentProcessId : PROC
EXTERN OutputDebugStringA : PROC
EXTERN GetModuleHandleA : PROC
EXTERN lstrcmpiA : PROC
EXTERN lstrcpyA : PROC
EXTERN lstrcatA : PROC
EXTERN wsprintfA : PROC
EXTERN GlobalAlloc : PROC
EXTERN GlobalFree : PROC
EXTERN memset : PROC
EXTERN memcpy : PROC
EXTERN InitializeCriticalSection : PROC
EXTERN EnterCriticalSection : PROC
EXTERN LeaveCriticalSection : PROC
EXTERN DwmSetWindowAttribute : PROC
EXTERN SetBkColor : PROC
EXTERN SetTextColor : PROC
EXTERN SetTextAlign : PROC
EXTERN lstrlenA : PROC
EXTERN TextOutA : PROC
EXTERN BeginPaint : PROC
EXTERN EndPaint : PROC
EXTERN GetDC : PROC
EXTERN ReleaseDC : PROC

; Constants
WM_CREATE               EQU 0001h
WM_SIZE                 EQU 0005h
WM_PAINT                EQU 000Fh
WM_COMMAND              EQU 0111h
WM_NOTIFY               EQU 004Eh
WM_USER                 EQU 0400h
WM_DESTROY              EQU 0002h
WM_DRAWITEM             EQU 002Bh
WM_LBUTTONUP            EQU 0202h

WS_CHILD                EQU 40000000h
WS_VISIBLE              EQU 10000000h
WS_TABSTOP              EQU 00010000h
WS_BORDER               EQU 00800000h
WS_CLIPCHILDREN         EQU 02000000h

WC_TREEVIEWA            EQU "SysTreeView32",0
WC_LISTVIEWA            EQU "SysListView32",0
WC_EDITA                EQU "Edit",0
WC_BUTTONA              EQU "Button",0
WC_COMBOBOXA            EQU "ComboBox",0

TVS_HASBUTTONS          EQU 0001h
TVS_HASLINES            EQU 0002h
TVS_LINESATROOT         EQU 0004h
TVS_EDITLABELS          EQU 0008h
TVS_SHOWSELALWAYS       EQU 0020h

; Combined style constants
ACTIVITY_BAR_STYLE      EQU 5000000Bh  ; WS_CHILD | WS_VISIBLE | BS_OWNERDRAW
TREEVIEW_STYLE          EQU 50800027h  ; WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS

; Sidebar struct offsets
SB_HWND                 EQU 0   ; 8 bytes - main handle

; =============================================================================
; SidebarCreate - Factory function called by IDE
; rcx = hwndParent, edx = x, r8d = y, r9d = width, [rsp+28] = height
; Returns: rax = hwndSidebar
; =============================================================================
SidebarCreate PROC EXPORT
    ; Simple stub - just return success for now
    mov rax, 1
    ret
SidebarCreate ENDP

END