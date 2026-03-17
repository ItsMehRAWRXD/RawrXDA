; =============================================================================
; RawrXD_Native_UI.asm - v15.2.0-TRIPANE
; Pure x64 Win32 Windowing - 3-Pane Layout Skeleton (Explorer | Editor | Chat)
; No .NET, No C++, Zero Bloat
; =============================================================================
OPTION CASEMAP:NONE

include windows.inc
include user32.inc
include kernel32.inc
include gdi32.inc
include comctl32.inc

includelib user32.lib
includelib kernel32.lib
includelib gdi32.lib
includelib comctl32.lib

.data
    szClassName     DB "RawrXD_NativeUI", 0
    szTitle         DB "RawrXD Sovereign IDE v15.2.1-RESIZE (x64)", 0
    szTreeView      DB "SysTreeView32", 0
    szEdit          DB "EDIT", 0
    szRichEditLib   DB "msftedit.dll", 0
    szRichEdit      DB "RICHEDIT50W", 0
    szOpenedFile    DB "// Hook triggered: File opened from TreeView!", 13, 10, "// RichEdit successfully updated.", 0
    
    PUBLIC hExplorer
    PUBLIC hEditor
    PUBLIC hChat
    PUBLIC hRuntime
    
    ; Window Handles
    hExplorer       QWORD 0
    hEditor         QWORD 0
    hChat           QWORD 0
    hRuntime        QWORD 0
    hStatus         QWORD 0
    
    ; UI DockManager State
    pUiState        QWORD 0


.code

extern DockManager_Init : PROC
extern DockManager_LayoutPass : PROC
extern DockManager_ApplyGeometry : PROC
extern DockManager_OnMouseMove : PROC
extern DockManager_OnLButtonDown : PROC
extern DockManager_OnLButtonUp : PROC
extern ReleaseCapture : PROC

; =============================================================================
; RawrXD_UI_CreateControls
; =============================================================================
RawrXD_UI_CreateControls PROC FRAME
    ; RCX = Parent hWnd
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    sub rsp, 60h
    .endprolog

    mov rbx, rcx            ; rbx = hParent

    ; Load RichEdit library
    extern LoadLibraryA : PROC
    lea rcx, szRichEditLib
    call LoadLibraryA

    ; 1. Explorer (SysTreeView32)
    xor r9d, r9d
    mov QWORD PTR [rsp+20h], 0       ; x
    mov QWORD PTR [rsp+28h], 0       ; y
    mov QWORD PTR [rsp+30h], 0       ; nWidth
    mov QWORD PTR [rsp+38h], 0       ; nHeight
    mov QWORD PTR [rsp+40h], rbx     ; hWndParent
    mov QWORD PTR [rsp+48h], 101     ; hMenu/ID
    mov QWORD PTR [rsp+50h], 0       ; hInstance
    mov QWORD PTR [rsp+58h], 0       ; lpParam

    mov r8d, 50000000h or 0002h or 0001h or 00800000h ; WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | WS_BORDER
    mov rcx, 0                      ; dwExStyle
    lea rdx, szTreeView             ; Class Name
    lea r8, szTitle                 ; Title (dummy)
    xor r9d, r9d                    ; x, y, w, h handled in Resize (already 0)

    extern CreateWindowExA : PROC
    call CreateWindowExA
    mov [rel hExplorer], rax

    ; 2. Editor (RichEdit)
    mov QWORD PTR [rsp+20h], 0
    mov QWORD PTR [rsp+28h], 0
    mov QWORD PTR [rsp+30h], 0
    mov QWORD PTR [rsp+38h], 0
    mov QWORD PTR [rsp+40h], rbx
    mov QWORD PTR [rsp+48h], 102
    mov QWORD PTR [rsp+50h], 0
    mov QWORD PTR [rsp+58h], 0

    ; WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE = 50a00000h | 00200000h | 00100000h | 0004h = 50b00004h
    mov r8d, 50b00004h
    mov rcx, 0
    lea rdx, szRichEdit
    lea r8, szTitle
    call CreateWindowExA
    mov [rel hEditor], rax

    ; Set RichEdit Event Mask to ENM_SCROLL | ENM_UPDATE | ENM_CHANGE to fix line counter sync
    mov rcx, rax                ; hEditor
    mov edx, 0445h              ; EM_SETEVENTMASK
    xor r8d, r8d                ; None
    mov r9d, 00010004h          ; ENM_UPDATE (0x00010000) | ENM_SCROLL (0x00000004)
    extern SendMessageA : PROC
    call SendMessageA

    ; 3. Chat (EDIT)
    mov QWORD PTR [rsp+20h], 0
    mov QWORD PTR [rsp+28h], 0
    mov QWORD PTR [rsp+30h], 0
    mov QWORD PTR [rsp+38h], 0
    mov QWORD PTR [rsp+40h], rbx
    mov QWORD PTR [rsp+48h], 103
    mov QWORD PTR [rsp+50h], 0
    mov QWORD PTR [rsp+58h], 0

    ; WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY = 50a00000h | 00200000h | 0004h | 0800h = 50a00804h
    mov r8d, 50a00804h
    mov rcx, 0
    lea rdx, szEdit
    lea r8, szTitle
    call CreateWindowExA
    mov [rel hChat], rax

    ; 4. Runtime (RichEdit Bottom Terminal)
    mov QWORD PTR [rsp+20h], 0
    mov QWORD PTR [rsp+28h], 0
    mov QWORD PTR [rsp+30h], 0
    mov QWORD PTR [rsp+38h], 0
    mov QWORD PTR [rsp+40h], rbx
    mov QWORD PTR [rsp+48h], 104
    mov QWORD PTR [rsp+50h], 0
    mov QWORD PTR [rsp+58h], 0

    mov r8d, 50b00004h    ; WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE
    mov rcx, 0
    lea rdx, szRichEdit
    lea r8, szTitle
    call CreateWindowExA
    mov [rel hRuntime], rax

    ; Set RichEdit Event Mask
    mov rcx, rax                ; hRuntime
    mov edx, 0445h              ; EM_SETEVENTMASK
    xor r8d, r8d                ; None
    mov r9d, 00010004h          ; ENM_UPDATE | ENM_SCROLL
    call SendMessageA

    add rsp, 60h
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
RawrXD_UI_CreateControls ENDP

.data
    szStatusClass DB "msctls_statusbar32", 0

; =============================================================================
; RawrXD_UI_WndProc - The Core Router
; =============================================================================
RawrXD_UI_WndProc PROC FRAME
    ; rcx = hwnd, rdx = umsg, r8 = wparam, r9 = lparam
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    .endprolog
    
    mov rbx, rcx                ; hwnd
    mov rsi, rdx                ; umsg
    mov rdi, r8                 ; wparam
    
    cmp edx, 0001h              ; WM_CREATE
    je @@on_create
    
    cmp edx, 000Fh              ; WM_PAINT
    je @@on_paint
    
    cmp edx, 0014h              ; WM_ERASEBKGND
    je @@on_erasebkgnd
    
    cmp edx, 0312h              ; WM_HOTKEY
    je @@on_hotkey

    cmp edx, 0111h              ; WM_COMMAND
    je @@on_command
    
    cmp edx, 004Eh              ; WM_NOTIFY
    je @@on_notify

    cmp edx, 0133h              ; WM_CTLCOLOREDIT
    je @@on_ctlcoloredit
    
    cmp edx, 0134h              ; WM_CTLCOLORSTATIC
    je @@on_ctlcoloredit

    cmp edx, 0005h              ; WM_SIZE
    je @@on_size
    
    cmp edx, 0201h              ; WM_LBUTTONDOWN
    je @@on_lbuttondown
    
    cmp edx, 0200h              ; WM_MOUSEMOVE
    je @@on_mousemove
    
    cmp edx, 0202h              ; WM_LBUTTONUP
    je @@on_lbuttonup
    
    cmp edx, 0002h              ; WM_DESTROY
    je @@on_destroy
    
    extern DefWindowProcA : PROC
    call DefWindowProcA
    jmp @@exit

@@on_erasebkgnd:
    mov rax, 1
    jmp @@exit

@@on_paint:
    sub rsp, 80h    ; stack space
    lea rdx, [rsp+20h] ; PAINTSTRUCT pointer
    mov rcx, rbx       ; hwnd
    extern BeginPaint : PROC
    call BeginPaint
    ; rax is hdc
    mov rdx, rax ; HDC
    mov rcx, [rel pUiState]
    extern DockManager_VirtualPaintChrome : PROC
    call DockManager_VirtualPaintChrome
    lea rdx, [rsp+20h]
    mov rcx, rbx
    extern EndPaint : PROC
    call EndPaint
    add rsp, 80h
    xor rax, rax
    jmp @@exit

@@on_create:
    mov rcx, rbx
    call RawrXD_UI_CreateControls
    
    ; Init DockManager
    mov rcx, rbx
    call DockManager_Init
    mov [rel pUiState], rax
    
    ; Register Ctrl+Enter Hotkey (id=1, MOD_CONTROL=2, VK_RETURN=0x0D)
    mov rcx, rbx
    mov edx, 1
    mov r8d, 2
    mov r9d, 0Dh
    extern RegisterHotKey : PROC
    call RegisterHotKey

    xor rax, rax
    jmp @@exit

@@on_hotkey:
    cmp r8d, 1
    jne @@exit
    extern Core_ExecuteAgentAction : PROC
    sub rsp, 32
    call Core_ExecuteAgentAction
    add rsp, 32
    xor rax, rax
    jmp @@exit

@@on_command:
    ; r8 = wParam (HIWORD=notify_code, LOWORD=id), r9 = lParam (hwnd from)
    mov eax, r8d
    shr eax, 16                 ; eax = notification code
    cmp eax, 0602h              ; EN_VSCROLL
    je @@sync_scroll
    cmp eax, 0400h              ; EN_UPDATE
    je @@sync_scroll
    jmp @@exit

@@sync_scroll:
    ; Force repaint of the whole client area to sync any overlay/line counter
    extern InvalidateRect : PROC
    mov rcx, rbx
    xor edx, edx
    mov r8d, 1
    call InvalidateRect
    xor rax, rax
    jmp @@exit

@@on_notify:
    ; r8 = wParam (Control ID), r9 = lParam (PNMHDR)
    mov eax, dword ptr [r9+10h] ; NMHDR.code is at offset 16 in x64
    
    ; NM_DBLCLK is -3 (0xFFFFFFFD)
    cmp eax, -3
    jne @@notify_done
    
    ; Ensure the notification is from hExplorer
    mov rcx, qword ptr [r9]     ; NMHDR.hwndFrom
    cmp rcx, [rel hExplorer]
    jne @@notify_done
    
    ; SendMessageA(hEditor, WM_SETTEXT, 0, szOpenedFile)
    mov rcx, [rel hEditor]
    mov edx, 000Ch              ; WM_SETTEXT
    xor r8d, r8d
    lea r9, szOpenedFile
    extern SendMessageA : PROC
    call SendMessageA
    
@@notify_done:
    xor rax, rax
    jmp @@exit

@@on_ctlcoloredit:
    ; r8 = hdc, r9 = hwnd
    ; Set text color to light grey (0x00CCCCCC) and background to dark grey (0x001E1E1E)
    mov rcx, r8
    mov edx, 00CCCCCCh
    extern SetTextColor : PROC
    call SetTextColor
    
    mov rcx, r8
    mov edx, 001E1E1Eh
    extern SetBkColor : PROC
    call SetBkColor
    
    ; Return a dark brush
    extern CreateSolidBrush : PROC
    mov ecx, 001E1E1Eh
    call CreateSolidBrush
    jmp @@exit

@@on_size:
    sub rsp, 40h
    
    ; Update Status Bar
    mov rcx, [rel hStatus]
    mov edx, 0005h              ; WM_SIZE
    xor r8d, r8d
    xor r9d, r9d
    extern SendMessageA : PROC
    call SendMessageA

    ; DockManager Geometry Update
    lea rdx, [rsp+20h]          ; RECT pointer
    mov rcx, rbx                ; hWnd
    extern GetClientRect : PROC
    call GetClientRect
    
    mov rcx, [rel pUiState]
    lea rdx, [rsp+20h]
    call DockManager_LayoutPass

    mov rcx, [rel pUiState]
    call DockManager_ApplyGeometry
    
    add rsp, 40h
    xor rax, rax
    jmp @@exit

@@on_lbuttondown:
    mov rcx, [rel pUiState]
    mov eax, r9d
    and eax, 0FFFFh             ; xPos (LOWORD)
    mov edx, eax
    mov eax, r9d
    shr eax, 16                 ; yPos (HIWORD)
    mov r8d, eax
    sub rsp, 32
    call DockManager_OnLButtonDown
    add rsp, 32
    jmp @@handled_mouse

@@on_mousemove:
    mov rcx, [rel pUiState]
    mov eax, r9d
    and eax, 0FFFFh             ; xPos (LOWORD)
    mov edx, eax
    mov eax, r9d
    shr eax, 16                 ; yPos (HIWORD)
    mov r8d, eax
    sub rsp, 32
    call DockManager_OnMouseMove
    add rsp, 32
    jmp @@handled_mouse

@@on_lbuttonup:
    mov rcx, [rel pUiState]
    mov eax, r9d
    and eax, 0FFFFh             ; xPos (LOWORD)
    mov edx, eax
    mov eax, r9d
    shr eax, 16                 ; yPos (HIWORD)
    mov r8d, eax
    sub rsp, 32
    call DockManager_OnLButtonUp
    call ReleaseCapture
    add rsp, 32
    jmp @@handled_mouse

@@on_destroy:
    extern PostQuitMessage : PROC
    mov ecx, 0
    call PostQuitMessage
    xor rax, rax

@@handled_mouse:
    xor rax, rax
    jmp @@exit

@@exit:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
RawrXD_UI_WndProc ENDP

END
