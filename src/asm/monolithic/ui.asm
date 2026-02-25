; ═══════════════════════════════════════════════════════════════════
; RawrXD UI Engine — Phase X+1: Custom Editor Surface
; Pure Win32 GDI. No Edit control. Own text buffer, cursor, paint.
; Exports: UIMainLoop, CreateEditorPane, hMainWnd
; ═══════════════════════════════════════════════════════════════════

EXTERN g_hInstance:QWORD
EXTERN RegisterClassExW:PROC
EXTERN CreateWindowExW:PROC
EXTERN GetMessageW:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageW:PROC
EXTERN DefWindowProcW:PROC
EXTERN PostQuitMessage:PROC
EXTERN BeginPaint:PROC
EXTERN EndPaint:PROC
EXTERN InvalidateRect:PROC
EXTERN GetClientRect:PROC
EXTERN SetFocus:PROC
EXTERN TextOutW:PROC
EXTERN SetTextColor:PROC
EXTERN SetBkColor:PROC
EXTERN SetBkMode:PROC
EXTERN CreateFontIndirectW:PROC
EXTERN SelectObject:PROC
EXTERN DeleteObject:PROC
EXTERN FillRect:PROC
EXTERN GetStockObject:PROC
EXTERN CreateCaret:PROC
EXTERN SetCaretPos:PROC
EXTERN ShowCaret:PROC
EXTERN DestroyCaret:PROC
EXTERN HideCaret:PROC
EXTERN SetScrollInfo:PROC
EXTERN GetScrollInfo:PROC

; Clipboard
EXTERN OpenClipboard:PROC
EXTERN CloseClipboard:PROC
EXTERN EmptyClipboard:PROC
EXTERN SetClipboardData:PROC
EXTERN GetClipboardData:PROC
EXTERN GlobalAlloc:PROC
EXTERN GlobalLock:PROC
EXTERN GlobalUnlock:PROC
EXTERN GlobalFree:PROC
EXTERN GetKeyState:PROC

; Context menu
EXTERN CreatePopupMenu:PROC
EXTERN InsertMenuW:PROC
EXTERN TrackPopupMenu:PROC
EXTERN DestroyMenu:PROC
EXTERN GetCursorPos:PROC
EXTERN ScreenToClient:PROC
EXTERN LoadCursorW:PROC
EXTERN SetCursor:PROC
EXTERN AppendMenuW:PROC

PUBLIC UIMainLoop
PUBLIC CreateEditorPane
PUBLIC hMainWnd

CS_HREDRAW         equ 2
CS_VREDRAW         equ 1
WS_OVERLAPPEDWINDOW equ 0CF0000h
WS_VISIBLE         equ 10000000h
WS_CHILD           equ 40000000h
WS_VSCROLL         equ 200000h
WS_EX_CLIENTEDGE   equ 200h
CW_USEDEFAULT      equ 80000000h
COLOR_WINDOW       equ 5
SW_SHOWDEFAULT     equ 10

WM_CREATE          equ 1
WM_DESTROY         equ 2
WM_SIZE            equ 5
WM_PAINT           equ 0Fh
WM_CLOSE           equ 10h
WM_SETFOCUS        equ 7
WM_KILLFOCUS       equ 8
WM_KEYDOWN         equ 100h
WM_CHAR            equ 102h
WM_MOUSEWHEEL      equ 20Ah
WM_VSCROLL         equ 114h
WM_ERASEBKGND      equ 14h

VK_LEFT            equ 25h
VK_UP              equ 26h
VK_RIGHT           equ 27h
VK_DOWN            equ 28h
VK_HOME            equ 24h
VK_END             equ 23h
VK_DELETE          equ 2Eh
VK_PRIOR           equ 21h
VK_NEXT            equ 22h
VK_BACK            equ 8

WM_COMMAND         equ 111h
WM_RBUTTONUP       equ 205h
WM_LBUTTONDOWN     equ 201h
WM_SETCURSOR       equ 20h

TPM_LEFTALIGN      equ 0
TPM_RETURNCMD      equ 100h
MF_STRING          equ 0
MF_SEPARATOR       equ 800h
GMEM_MOVEABLE      equ 2
CF_UNICODETEXT     equ 0Dh
IDC_IBEAM          equ 32513

IDM_CUT            equ 1
IDM_COPY           equ 2
IDM_PASTE          equ 3
IDM_SELECTALL      equ 4
IDM_DELETE         equ 5

SB_VERT            equ 1
SIF_ALL            equ 17h
SIF_POS            equ 4
SB_THUMBTRACK      equ 5
SB_LINEUP          equ 0
SB_LINEDOWN        equ 1
SB_PAGEUP          equ 2
SB_PAGEDOWN        equ 3

TRANSPARENT        equ 1
OPAQUE             equ 2
FW_NORMAL          equ 400
DEFAULT_CHARSET    equ 1
CLEARTYPE_QUALITY  equ 5
FF_MODERN          equ 30h
FIXED_PITCH        equ 1
NULL_BRUSH         equ 5

GUTTER_WIDTH       equ 50
CHAR_WIDTH         equ 8
LINE_HEIGHT        equ 18
TEXT_BUF_SIZE      equ 10000h
MAX_LINES          equ 4000

WNDCLASSEXW STRUCT
    cbSize          dd ?
    style           dd ?
    lpfnWndProc     dq ?
    cbClsExtra      dd ?
    cbWndExtra      dd ?
    hInstance       dq ?
    hIcon           dq ?
    hCursor         dq ?
    hbrBackground   dq ?
    lpszMenuName    dq ?
    lpszClassName   dq ?
    hIconSm         dq ?
WNDCLASSEXW ENDS

WINMSG STRUCT
    hwnd    dq ?
    message dd ?
    wParam  dq ?
    lParam  dq ?
    time    dd ?
    pt_x    dd ?
    pt_y    dd ?
WINMSG ENDS

PAINTST STRUCT
    hdc     dq ?
    fErase  dd ?
    rc_left dd ?
    rc_top  dd ?
    rc_right dd ?
    rc_bottom dd ?
    reserved db 32 dup(?)
PAINTST ENDS

LOGFONTW STRUCT
    lfHeight         dd ?
    lfWidth          dd ?
    lfEscapement     dd ?
    lfOrientation    dd ?
    lfWeight         dd ?
    lfItalic         db ?
    lfUnderline      db ?
    lfStrikeOut      db ?
    lfCharSet        db ?
    lfOutPrecision   db ?
    lfClipPrecision  db ?
    lfQuality        db ?
    lfPitchAndFamily db ?
    lfFaceName       dw 32 dup(?)
LOGFONTW ENDS

SCROLLINFO STRUCT
    cbSize    dd ?
    fMask     dd ?
    nMin      dd ?
    nMax      dd ?
    nPage     dd ?
    nPos      dd ?
    nTrackPos dd ?
SCROLLINFO ENDS

.data
align 8
hMainWnd      dq 0
hEditorFont   dq 0
g_cursorLine  dd 0
g_cursorCol   dd 0
g_scrollY     dd 0
g_clientW     dd 960
g_clientH     dd 600
g_totalChars  dd 0
g_lineCount   dd 1
g_selStart    dd -1          ; selection anchor (char index), -1 = none
g_selEnd      dd -1          ; selection end (char index)
g_hasFocus    dd 0
align 8
g_lineOff     dd MAX_LINES dup(0)

.data?
align 8
g_textBuf     dw TEXT_BUF_SIZE dup(?)
g_numBuf      dw 16 dup(?)

.const
szClass        dw 'R','a','w','r','X','D','_','M','a','i','n',0
szTitle        dw 'R','a','w','r','X','D',' ','A','g','e','n','t','i','c',' ','I','D','E',0
szFontName     dw 'C','o','n','s','o','l','a','s',0

; Context menu strings
szCut          dw 'C','u','t',9,'C','t','r','l','+','X',0
szCopy         dw 'C','o','p','y',9,'C','t','r','l','+','C',0
szPaste        dw 'P','a','s','t','e',9,'C','t','r','l','+','V',0
szSelectAll    dw 'S','e','l','e','c','t',' ','A','l','l',9,'C','t','r','l','+','A',0
szDelete       dw 'D','e','l','e','t','e',0

; Welcome text seeded into the editor on WM_CREATE
szWelcome      dw ';',' ','R','a','w','r','X','D',' ','A','g','e','n','t','i','c'
               dw ' ','I','D','E',' ',2014h,' ','S','o','v','e','r','e','i','g','n'
               dw ' ','K','e','r','n','e','l',0Ah
               dw ';',' ','T','y','p','e',' ','h','e','r','e','.',' '
               dw 'A','l','l',' ','e','d','i','t','o','r',' ','k','e','y','s'
               dw ' ','a','r','e',' ','l','i','v','e','.',0Ah
               dw 0Ah
               dw 0

.code

UIMainLoop PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 0F0h
    .allocstack 0F0h
    .endprolog
    wndClass equ rbp-50h
    mov     dword ptr [wndClass], 80
    mov     dword ptr [wndClass+4], CS_HREDRAW or CS_VREDRAW
    lea     rax, WndProc
    mov     qword ptr [wndClass+8], rax
    xor     eax, eax
    mov     dword ptr [wndClass+10h], eax
    mov     dword ptr [wndClass+14h], eax
    mov     rax, g_hInstance
    mov     qword ptr [wndClass+18h], rax
    xor     eax, eax
    mov     qword ptr [wndClass+20h], rax
    mov     qword ptr [wndClass+28h], rax
    mov     qword ptr [wndClass+30h], COLOR_WINDOW + 1
    mov     qword ptr [wndClass+38h], rax
    lea     rcx, szClass
    mov     qword ptr [wndClass+40h], rcx
    xor     eax, eax
    mov     qword ptr [wndClass+48h], rax
    lea     rcx, [wndClass]
    call    RegisterClassExW
    test    ax, ax
    jz      @exit
    xor     ecx, ecx
    lea     rdx, szClass
    lea     r8, szTitle
    mov     r9d, WS_OVERLAPPEDWINDOW or WS_VISIBLE or WS_VSCROLL
    mov     dword ptr [rsp+20h], CW_USEDEFAULT
    mov     dword ptr [rsp+28h], CW_USEDEFAULT
    mov     dword ptr [rsp+30h], 960
    mov     dword ptr [rsp+38h], 600
    xor     eax, eax
    mov     qword ptr [rsp+40h], rax
    mov     qword ptr [rsp+48h], rax
    mov     rax, g_hInstance
    mov     qword ptr [rsp+50h], rax
    mov     qword ptr [rsp+58h], 0
    call    CreateWindowExW
    mov     hMainWnd, rax
    test    rax, rax
    jz      @exit
    msgBuf equ rbp-90h
@msg_loop:
    lea     rcx, [msgBuf]
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    call    GetMessageW
    test    eax, eax
    jz      @exit
    lea     rcx, [msgBuf]
    call    TranslateMessage
    lea     rcx, [msgBuf]
    call    DispatchMessageW
    jmp     @msg_loop
@exit:
    mov     rcx, hEditorFont
    test    rcx, rcx
    jz      @no_font
    call    DeleteObject
@no_font:
    xor     eax, eax
    lea     rsp, [rbp]
    pop     rbp
    ret
UIMainLoop ENDP

WndProc PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 148h
    .allocstack 148h
    .endprolog
    mov     qword ptr [rbp-8], rcx
    mov     dword ptr [rbp-0Ch], edx
    mov     qword ptr [rbp-18h], r8
    mov     qword ptr [rbp-20h], r9
    cmp     edx, WM_CREATE
    je      @wm_create
    cmp     edx, WM_DESTROY
    je      @wm_destroy
    cmp     edx, WM_PAINT
    je      @wm_paint
    cmp     edx, WM_CHAR
    je      @wm_char
    cmp     edx, WM_KEYDOWN
    je      @wm_keydown
    cmp     edx, WM_SIZE
    je      @wm_size
    cmp     edx, WM_SETFOCUS
    je      @wm_setfocus
    cmp     edx, WM_KILLFOCUS
    je      @wm_killfocus
    cmp     edx, WM_ERASEBKGND
    je      @wm_erasebkgnd
    cmp     edx, WM_VSCROLL
    je      @wm_vscroll
    cmp     edx, WM_MOUSEWHEEL
    je      @wm_mousewheel
    cmp     edx, WM_RBUTTONUP
    je      @wm_rbuttonup
    cmp     edx, WM_COMMAND
    je      @wm_command
    cmp     edx, WM_LBUTTONDOWN
    je      @wm_lbuttondown
    cmp     edx, WM_SETCURSOR
    je      @wm_setcursor
    mov     rcx, qword ptr [rbp-8]
    mov     edx, dword ptr [rbp-0Ch]
    mov     r8, qword ptr [rbp-18h]
    mov     r9, qword ptr [rbp-20h]
    call    DefWindowProcW
    jmp     @wp_ret

@wm_create:
    mov     rax, qword ptr [rbp-8]
    mov     hMainWnd, rax
    lea     rdi, [rbp-0C0h]
    xor     eax, eax
    mov     ecx, 24
    rep     stosd
    lea     rdi, [rbp-0C0h]
    mov     dword ptr [rdi], 0FFFFFFF2h
    mov     dword ptr [rdi+10h], FW_NORMAL
    mov     byte  ptr [rdi+17h], DEFAULT_CHARSET
    mov     byte  ptr [rdi+1Ah], CLEARTYPE_QUALITY
    mov     byte  ptr [rdi+1Bh], FIXED_PITCH or FF_MODERN
    lea     rsi, szFontName
    lea     rdi, [rbp-0C0h+1Ch]
    mov     ecx, 9
    rep     movsw
    lea     rcx, [rbp-0C0h]
    call    CreateFontIndirectW
    mov     hEditorFont, rax
    mov     g_totalChars, 0
    mov     g_lineCount, 1
    mov     g_cursorLine, 0
    mov     g_cursorCol, 0
    mov     g_scrollY, 0

    ; ── Seed the editor with welcome text ──────────────────────
    ;    So the user sees a live surface, not a blank void.
    lea     rdi, g_textBuf
    lea     rsi, szWelcome
    xor     ecx, ecx
@@seed_copy:
    mov     ax, word ptr [rsi + rcx*2]
    mov     word ptr [rdi + rcx*2], ax
    test    ax, ax
    jz      @@seed_done
    inc     ecx
    jmp     @@seed_copy
@@seed_done:
    mov     g_totalChars, ecx
    call    RebuildLineTable

    call    UpdateScrollBar

    ; Set focus to self so keyboard input works immediately
    mov     rcx, hMainWnd
    call    SetFocus
    jmp     @ret_zero

@wm_setfocus:
    mov     g_hasFocus, 1
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 2
    mov     r9d, LINE_HEIGHT
    call    CreateCaret
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    call    ShowCaret
    jmp     @ret_zero

@wm_killfocus:
    mov     g_hasFocus, 0
    mov     rcx, qword ptr [rbp-8]
    call    DestroyCaret
    jmp     @ret_zero

@wm_size:
    mov     eax, dword ptr [rbp-20h]
    movzx   ecx, ax
    mov     g_clientW, ecx
    shr     eax, 16
    mov     g_clientH, eax
    call    UpdateScrollBar
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@wm_erasebkgnd:
    mov     eax, 1
    jmp     @wp_ret

@wm_char:
    mov     eax, dword ptr [rbp-18h]
    ; ── Ctrl+key intercepts ──
    cmp     eax, 1                     ; Ctrl+A = Select All
    je      @ctrl_a
    cmp     eax, 3                     ; Ctrl+C = Copy
    je      @ctrl_c
    cmp     eax, 16h                   ; Ctrl+V = Paste
    je      @ctrl_v
    cmp     eax, 18h                   ; Ctrl+X = Cut
    je      @ctrl_x
    cmp     eax, 8
    je      @char_bs
    cmp     eax, 0Dh
    je      @char_enter
    cmp     eax, 9
    je      @char_tab
    cmp     eax, 20h
    jb      @ret_zero
    ; ── If selection active, delete it first, then insert ──
    call    DeleteSelection
    mov     ebx, eax
    mov     ecx, g_totalChars
    cmp     ecx, TEXT_BUF_SIZE - 2
    jge     @ret_zero
    mov     ecx, g_cursorLine
    lea     rsi, g_lineOff
    mov     esi, dword ptr [rsi + rcx*4]
    add     esi, g_cursorCol
    lea     rdi, g_textBuf
    mov     ecx, g_totalChars
@@ins_shift:
    cmp     ecx, esi
    jle     @@ins_write
    mov     ax, word ptr [rdi + rcx*2 - 2]
    mov     word ptr [rdi + rcx*2], ax
    dec     ecx
    jmp     @@ins_shift
@@ins_write:
    mov     word ptr [rdi + rsi*2], bx
    inc     g_totalChars
    mov     ecx, g_totalChars
    mov     word ptr [rdi + rcx*2], 0
    inc     g_cursorCol
    call    RebuildLineTable
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@char_enter:
    call    DeleteSelection
    mov     ecx, g_totalChars
    cmp     ecx, TEXT_BUF_SIZE - 2
    jge     @ret_zero
    mov     ecx, g_cursorLine
    lea     rsi, g_lineOff
    mov     esi, dword ptr [rsi + rcx*4]
    add     esi, g_cursorCol
    lea     rdi, g_textBuf
    mov     ecx, g_totalChars
@@ent_shift:
    cmp     ecx, esi
    jle     @@ent_write
    mov     ax, word ptr [rdi + rcx*2 - 2]
    mov     word ptr [rdi + rcx*2], ax
    dec     ecx
    jmp     @@ent_shift
@@ent_write:
    mov     word ptr [rdi + rsi*2], 0Ah
    inc     g_totalChars
    mov     ecx, g_totalChars
    mov     word ptr [rdi + rcx*2], 0
    inc     g_cursorLine
    mov     g_cursorCol, 0
    call    RebuildLineTable
    call    UpdateScrollBar
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@char_bs:
    ; If selection exists, delete it instead of single char
    cmp     g_selStart, -1
    je      @bs_nosel
    call    DeleteSelection
    call    RebuildLineTable
    call    UpdateScrollBar
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero
@bs_nosel:
    mov     eax, g_cursorCol
    or      eax, g_cursorLine
    jz      @ret_zero
    mov     ecx, g_cursorLine
    lea     rsi, g_lineOff
    mov     esi, dword ptr [rsi + rcx*4]
    add     esi, g_cursorCol
    dec     esi
    js      @bs_prev
    lea     rdi, g_textBuf
    cmp     word ptr [rdi + rsi*2], 0Ah
    jne     @bs_normal
@bs_prev:
    mov     ecx, g_cursorLine
    test    ecx, ecx
    jz      @ret_zero
    dec     ecx
    mov     g_cursorLine, ecx
    lea     rax, g_lineOff
    mov     edx, dword ptr [rax + rcx*4]
    mov     ebx, dword ptr [rax + rcx*4 + 4]
    sub     ebx, edx
    dec     ebx
    js      @bs_col0
    mov     g_cursorCol, ebx
    jmp     @bs_recalc
@bs_col0:
    mov     g_cursorCol, 0
@bs_recalc:
    mov     ecx, g_cursorLine
    lea     rax, g_lineOff
    mov     esi, dword ptr [rax + rcx*4]
    add     esi, g_cursorCol
    jmp     @bs_shift
@bs_normal:
    dec     g_cursorCol
@bs_shift:
    lea     rdi, g_textBuf
    mov     ecx, esi
    inc     ecx
@@bs_loop:
    cmp     ecx, g_totalChars
    jge     @bs_done
    mov     ax, word ptr [rdi + rcx*2]
    mov     word ptr [rdi + rcx*2 - 2], ax
    inc     ecx
    jmp     @@bs_loop
@bs_done:
    dec     g_totalChars
    mov     ecx, g_totalChars
    lea     rdi, g_textBuf
    mov     word ptr [rdi + rcx*2], 0
    call    RebuildLineTable
    call    UpdateScrollBar
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@char_tab:
    mov     ebx, 4
@tab_loop:
    mov     ecx, g_totalChars
    cmp     ecx, TEXT_BUF_SIZE - 2
    jge     @tab_done
    mov     ecx, g_cursorLine
    lea     rsi, g_lineOff
    mov     esi, dword ptr [rsi + rcx*4]
    add     esi, g_cursorCol
    lea     rdi, g_textBuf
    mov     ecx, g_totalChars
@@tab_shift:
    cmp     ecx, esi
    jle     @@tab_write
    mov     ax, word ptr [rdi + rcx*2 - 2]
    mov     word ptr [rdi + rcx*2], ax
    dec     ecx
    jmp     @@tab_shift
@@tab_write:
    mov     word ptr [rdi + rsi*2], 20h
    inc     g_totalChars
    mov     ecx, g_totalChars
    mov     word ptr [rdi + rcx*2], 0
    inc     g_cursorCol
    call    RebuildLineTable
    dec     ebx
    jnz     @tab_loop
@tab_done:
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

; ── Ctrl+key handlers ────────────────────────────────────────────
@ctrl_a:
    ; Select All — same as IDM_SELECTALL
    mov     g_selStart, 0
    mov     eax, g_totalChars
    mov     g_selEnd, eax
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@ctrl_c:
    ; Copy — delegate to WM_COMMAND copy logic
    mov     dword ptr [rbp-18h], IDM_COPY
    jmp     @cmd_copy

@ctrl_x:
    ; Cut — delegate to WM_COMMAND cut logic
    mov     dword ptr [rbp-18h], IDM_CUT
    jmp     @cmd_cut

@ctrl_v:
    ; Paste — delegate to WM_COMMAND paste logic
    jmp     @cmd_paste

@wm_keydown:
    mov     eax, dword ptr [rbp-18h]
    cmp     eax, VK_LEFT
    je      @k_left
    cmp     eax, VK_RIGHT
    je      @k_right
    cmp     eax, VK_UP
    je      @k_up
    cmp     eax, VK_DOWN
    je      @k_down
    cmp     eax, VK_HOME
    je      @k_home
    cmp     eax, VK_END
    je      @k_end
    cmp     eax, VK_DELETE
    je      @k_del
    cmp     eax, VK_PRIOR
    je      @k_pgup
    cmp     eax, VK_NEXT
    je      @k_pgdn
    jmp     @ret_zero

@k_left:
    mov     eax, g_cursorCol
    test    eax, eax
    jz      @kl_prev
    dec     g_cursorCol
    jmp     @k_move
@kl_prev:
    mov     eax, g_cursorLine
    test    eax, eax
    jz      @ret_zero
    dec     g_cursorLine
    call    GetCurLineLen
    mov     g_cursorCol, eax
    jmp     @k_move

@k_right:
    call    GetCurLineLen
    mov     ecx, g_cursorCol
    cmp     ecx, eax
    jl      @kr_same
    mov     ecx, g_cursorLine
    inc     ecx
    cmp     ecx, g_lineCount
    jge     @ret_zero
    mov     g_cursorLine, ecx
    mov     g_cursorCol, 0
    jmp     @k_move
@kr_same:
    inc     g_cursorCol
    jmp     @k_move

@k_up:
    mov     eax, g_cursorLine
    test    eax, eax
    jz      @ret_zero
    dec     g_cursorLine
    call    GetCurLineLen
    cmp     g_cursorCol, eax
    jle     @k_move
    mov     g_cursorCol, eax
    jmp     @k_move

@k_down:
    mov     eax, g_cursorLine
    inc     eax
    cmp     eax, g_lineCount
    jge     @ret_zero
    mov     g_cursorLine, eax
    call    GetCurLineLen
    cmp     g_cursorCol, eax
    jle     @k_move
    mov     g_cursorCol, eax
    jmp     @k_move

@k_home:
    mov     g_cursorCol, 0
    jmp     @k_move

@k_end:
    call    GetCurLineLen
    mov     g_cursorCol, eax
    jmp     @k_move

@k_del:
    mov     ecx, g_cursorLine
    lea     rsi, g_lineOff
    mov     esi, dword ptr [rsi + rcx*4]
    add     esi, g_cursorCol
    cmp     esi, g_totalChars
    jge     @ret_zero
    lea     rdi, g_textBuf
    mov     ecx, esi
    inc     ecx
@@del_loop:
    cmp     ecx, g_totalChars
    jge     @del_done
    mov     ax, word ptr [rdi + rcx*2]
    mov     word ptr [rdi + rcx*2 - 2], ax
    inc     ecx
    jmp     @@del_loop
@del_done:
    dec     g_totalChars
    mov     ecx, g_totalChars
    lea     rdi, g_textBuf
    mov     word ptr [rdi + rcx*2], 0
    call    RebuildLineTable
    call    UpdateScrollBar
    call    GetCurLineLen
    cmp     g_cursorCol, eax
    jle     @del_paint
    mov     g_cursorCol, eax
@del_paint:
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@k_pgup:
    mov     eax, g_clientH
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    sub     g_cursorLine, eax
    jns     @pgu_ok
    mov     g_cursorLine, 0
@pgu_ok:
    call    GetCurLineLen
    cmp     g_cursorCol, eax
    jle     @k_move
    mov     g_cursorCol, eax
    jmp     @k_move

@k_pgdn:
    mov     eax, g_clientH
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    add     eax, g_cursorLine
    mov     ecx, g_lineCount
    dec     ecx
    cmp     eax, ecx
    jle     @pgd_ok
    mov     eax, ecx
@pgd_ok:
    test    eax, eax
    jns     @pgd_set
    xor     eax, eax
@pgd_set:
    mov     g_cursorLine, eax
    call    GetCurLineLen
    cmp     g_cursorCol, eax
    jle     @k_move
    mov     g_cursorCol, eax
    jmp     @k_move

@k_move:
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@wm_vscroll:
    mov     eax, dword ptr [rbp-18h]
    and     eax, 0FFFFh
    cmp     eax, SB_LINEUP
    je      @vs_up
    cmp     eax, SB_LINEDOWN
    je      @vs_dn
    cmp     eax, SB_PAGEUP
    je      @vs_pu
    cmp     eax, SB_PAGEDOWN
    je      @vs_pd
    cmp     eax, SB_THUMBTRACK
    je      @vs_thumb
    jmp     @ret_zero
@vs_up:
    mov     eax, g_scrollY
    test    eax, eax
    jz      @ret_zero
    dec     g_scrollY
    jmp     @vs_repaint
@vs_dn:
    mov     eax, g_scrollY
    inc     eax
    cmp     eax, g_lineCount
    jge     @ret_zero
    mov     g_scrollY, eax
    jmp     @vs_repaint
@vs_pu:
    mov     eax, g_clientH
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    sub     g_scrollY, eax
    jns     @vs_repaint
    mov     g_scrollY, 0
    jmp     @vs_repaint
@vs_pd:
    mov     eax, g_clientH
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    add     eax, g_scrollY
    mov     ecx, g_lineCount
    cmp     eax, ecx
    jl      @vs_pd_ok
    lea     eax, [ecx-1]
@vs_pd_ok:
    test    eax, eax
    jns     @vs_pd_s
    xor     eax, eax
@vs_pd_s:
    mov     g_scrollY, eax
    jmp     @vs_repaint
@vs_thumb:
    mov     eax, dword ptr [rbp-18h]
    shr     eax, 16
    mov     g_scrollY, eax
@vs_repaint:
    call    UpdateScrollBar
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@wm_mousewheel:
    mov     eax, dword ptr [rbp-18h]
    shr     eax, 16
    movsx   eax, ax
    cdq
    mov     ecx, 120
    idiv    ecx
    imul    eax, 3
    neg     eax
    add     eax, g_scrollY
    test    eax, eax
    jns     @mw_max
    xor     eax, eax
@mw_max:
    mov     ecx, g_lineCount
    dec     ecx
    test    ecx, ecx
    jns     @mw_clamp
    xor     ecx, ecx
@mw_clamp:
    cmp     eax, ecx
    jle     @mw_set
    mov     eax, ecx
@mw_set:
    mov     g_scrollY, eax
    call    UpdateScrollBar
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

; ── WM_LBUTTONDOWN: click-to-position caret ─────────────────────
@wm_lbuttondown:
    ; lParam is in [rbp-20h]: low word = X, high word = Y
    mov     rax, qword ptr [rbp-20h]
    movzx   ecx, ax              ; ecx = X (client)
    shr     rax, 16
    movzx   edx, ax              ; edx = Y (client)

    ; --- convert Y to line ---
    xor     eax, eax
    mov     eax, edx
    cdq
    mov     esi, LINE_HEIGHT
    idiv    esi                  ; eax = Y / LINE_HEIGHT
    add     eax, g_scrollY       ; eax = line index (0-based)
    ; clamp to [0, g_lineCount-1]
    test    eax, eax
    jns     @lb_miny
    xor     eax, eax
@lb_miny:
    mov     edi, g_lineCount
    dec     edi
    test    edi, edi
    jns     @lb_maxy
    xor     edi, edi
@lb_maxy:
    cmp     eax, edi
    jle     @lb_sety
    mov     eax, edi
@lb_sety:
    mov     g_cursorLine, eax

    ; --- convert X to column ---
    sub     ecx, GUTTER_WIDTH
    sub     ecx, 4               ; left padding
    test    ecx, ecx
    jns     @lb_colok
    xor     ecx, ecx
@lb_colok:
    cdq
    xor     edx, edx
    mov     eax, ecx
    mov     esi, CHAR_WIDTH
    div     esi                  ; eax = col
    ; clamp to line length
    push    rax
    call    GetCurLineLen         ; returns eax = length of current line
    mov     edi, eax
    pop     rax
    cmp     eax, edi
    jle     @lb_setx
    mov     eax, edi
@lb_setx:
    mov     g_cursorCol, eax

    ; clear selection
    mov     g_selStart, -1
    mov     g_selEnd, -1

    ; set keyboard focus to this window
    mov     rcx, qword ptr [rbp-8]
    call    SetFocus

    ; update caret + repaint
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

; ── WM_RBUTTONUP: show context menu ─────────────────────────────
@wm_rbuttonup:
    ; Get cursor screen pos
    lea     rcx, [rbp-0D0h]  ; POINT struct
    call    GetCursorPos
    ; Create popup menu
    call    CreatePopupMenu
    mov     rbx, rax          ; hMenu

    ; Append items: AppendMenuW(hMenu, MF_STRING, id, text)
    mov     rcx, rbx
    xor     edx, edx          ; MF_STRING = 0
    mov     r8d, IDM_CUT
    lea     r9, szCut
    call    AppendMenuW

    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, IDM_COPY
    lea     r9, szCopy
    call    AppendMenuW

    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, IDM_PASTE
    lea     r9, szPaste
    call    AppendMenuW

    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, IDM_SELECTALL
    lea     r9, szSelectAll
    call    AppendMenuW

    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, IDM_DELETE
    lea     r9, szDelete
    call    AppendMenuW

    ; TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RETURNCMD, x, y, 0, hWnd, 0)
    sub     rsp, 16                ; two extra stack slots for params 6-7
    mov     qword ptr [rsp+40], 0   ; lpRect = NULL
    mov     rax, qword ptr [rbp-8]
    mov     qword ptr [rsp+32], rax ; hWnd
    mov     rcx, rbx
    mov     edx, TPM_LEFTALIGN or TPM_RETURNCMD
    mov     r8d, dword ptr [rbp-0D0h]   ; x
    mov     r9d, dword ptr [rbp-0CCh]   ; y
    call    TrackPopupMenu
    add     rsp, 16
    mov     esi, eax          ; save command ID

    ; Destroy menu
    mov     rcx, rbx
    call    DestroyMenu

    ; If a command was chosen, send WM_COMMAND
    test    esi, esi
    jz      @ret_zero
    mov     rcx, qword ptr [rbp-8]
    mov     edx, WM_COMMAND
    movzx   r8d, si
    xor     r9d, r9d
    call    DefWindowProcW
    jmp     @ret_zero

; ── WM_COMMAND: context menu actions ─────────────────────────────
@wm_command:
    mov     eax, dword ptr [rbp-18h]    ; wParam
    and     eax, 0FFFFh                 ; low word = command ID
    cmp     eax, IDM_PASTE
    je      @cmd_paste
    cmp     eax, IDM_COPY
    je      @cmd_copy
    cmp     eax, IDM_CUT
    je      @cmd_cut
    cmp     eax, IDM_SELECTALL
    je      @cmd_selall
    cmp     eax, IDM_DELETE
    je      @cmd_delete
    jmp     @ret_zero

@cmd_paste:
    ; --- paste from clipboard ---
    mov     rcx, qword ptr [rbp-8]
    call    OpenClipboard
    test    eax, eax
    jz      @ret_zero
    mov     ecx, CF_UNICODETEXT
    call    GetClipboardData
    test    rax, rax
    jz      @cmd_pclose
    mov     rcx, rax
    call    GlobalLock
    test    rax, rax
    jz      @cmd_pclose
    mov     rsi, rax            ; rsi = pwszClipboard

    ; Count clip chars
    xor     ecx, ecx
@cmd_plen:
    cmp     word ptr [rsi + rcx*2], 0
    je      @cmd_pins
    inc     ecx
    jmp     @cmd_plen
@cmd_pins:
    ; ecx = clip length, insert at cursor position
    mov     edi, ecx            ; save clip len
    ; compute insertion offset
    mov     eax, g_cursorLine
    lea     rdx, g_lineOff
    mov     eax, dword ptr [rdx + rax*4]
    add     eax, g_cursorCol    ; eax = char offset
    ; shift buffer right by edi chars
    mov     r8d, g_totalChars
    sub     r8d, eax            ; r8d = chars to move
    lea     r10, g_textBuf
    lea     r11d, [r8d + edi]   ; not used, just for clarity
    ; memmove tail right
    mov     ecx, r8d
    test    ecx, ecx
    jle     @cmd_pcopy
    lea     rdx, [r10 + rax*2]           ; src
    lea     r11, [rdx + rdi*2]           ; dst (shifted right)
    ; copy backwards
    dec     ecx
@cmd_pmov:
    mov     r9w, word ptr [rdx + rcx*2]
    mov     word ptr [r11 + rcx*2], r9w
    dec     ecx
    jns     @cmd_pmov
@cmd_pcopy:
    ; copy clipboard text into gap
    lea     rdx, g_textBuf
    lea     rdx, [rdx + rax*2]
    xor     ecx, ecx
@cmd_pcloop:
    cmp     ecx, edi
    jge     @cmd_pupd
    mov     r9w, word ptr [rsi + rcx*2]
    mov     word ptr [rdx + rcx*2], r9w
    inc     ecx
    jmp     @cmd_pcloop
@cmd_pupd:
    add     g_totalChars, edi
    ; unlock + close
    mov     rcx, rsi
    call    GlobalUnlock
@cmd_pclose:
    call    CloseClipboard
    ; rebuild + repaint
    call    RebuildLineTable
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@cmd_copy:
@cmd_cut:
    ; --- copy/cut selected text ---
    mov     eax, g_selStart
    cmp     eax, -1
    je      @ret_zero
    mov     edi, g_selEnd
    cmp     eax, edi
    jle     @cmd_cc_ok
    xchg    eax, edi
@cmd_cc_ok:
    ; eax = start, edi = end
    mov     esi, edi
    sub     esi, eax            ; esi = selection length
    test    esi, esi
    jle     @ret_zero
    push    rax                 ; save start offset

    ; Allocate global mem
    mov     ecx, 42h            ; GMEM_MOVEABLE | GMEM_ZEROINIT
    lea     edx, [esi*2 + 2]   ; bytes = (len+1)*2
    call    GlobalAlloc
    test    rax, rax
    jz      @cmd_cc_fail
    mov     rbx, rax            ; hMem
    mov     rcx, rax
    call    GlobalLock
    test    rax, rax
    jz      @cmd_cc_fail
    mov     rdi, rax            ; pDst
    pop     rax                 ; start offset
    lea     r10, g_textBuf
    lea     r10, [r10 + rax*2]       ; r10 = &textBuf[start]
    xor     ecx, ecx
@cmd_ccloop:
    cmp     ecx, esi
    jge     @cmd_ccend
    mov     r9w, word ptr [r10 + rcx*2]
    mov     word ptr [rdi + rcx*2], r9w
    inc     ecx
    jmp     @cmd_ccloop
@cmd_ccend:
    mov     word ptr [rdi + rcx*2], 0
    mov     rcx, rbx
    call    GlobalUnlock

    ; Set clipboard
    mov     rcx, qword ptr [rbp-8]
    call    OpenClipboard
    test    eax, eax
    jz      @ret_zero
    call    EmptyClipboard
    mov     ecx, CF_UNICODETEXT
    mov     rdx, rbx
    call    SetClipboardData
    call    CloseClipboard

    ; If cut (IDM_CUT), check original command
    mov     eax, dword ptr [rbp-18h]
    and     eax, 0FFFFh
    cmp     eax, IDM_CUT
    jne     @ret_zero
    ; Delete selected text
    mov     eax, g_selStart
    mov     edi, g_selEnd
    cmp     eax, edi
    jle     @cmd_cutok
    xchg    eax, edi
@cmd_cutok:
    mov     ecx, edi
    sub     ecx, eax            ; ecx = chars to delete
    ; shift buffer left
    lea     r10, g_textBuf
    mov     edx, g_totalChars
    sub     edx, edi            ; edx = trailing chars
    test    edx, edx
    jle     @cmd_cutdone
    lea     r11, [r10 + rdi*2]   ; r11 = &textBuf[end]
    lea     r10, [r10 + rax*2]   ; r10 = &textBuf[start]
    xor     esi, esi
@cmd_cutmov:
    cmp     esi, edx
    jge     @cmd_cutdone
    mov     r9w, word ptr [r11 + rsi*2]
    mov     word ptr [r10 + rsi*2], r9w
    inc     esi
    jmp     @cmd_cutmov
@cmd_cutdone:
    sub     g_totalChars, ecx
    mov     g_selStart, -1
    mov     g_selEnd, -1
    call    RebuildLineTable
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero
@cmd_cc_fail:
    pop     rax
    jmp     @ret_zero

@cmd_selall:
    ; Select entire buffer
    mov     g_selStart, 0
    mov     eax, g_totalChars
    mov     g_selEnd, eax
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@cmd_delete:
    ; Delete selection
    mov     eax, g_selStart
    cmp     eax, -1
    je      @ret_zero
    mov     edi, g_selEnd
    cmp     eax, edi
    jle     @cmd_delok
    xchg    eax, edi
@cmd_delok:
    mov     ecx, edi
    sub     ecx, eax
    test    ecx, ecx
    jle     @ret_zero
    lea     r10, g_textBuf
    mov     edx, g_totalChars
    sub     edx, edi
    test    edx, edx
    jle     @cmd_deldone
    lea     r11, [r10 + rdi*2]   ; r11 = &textBuf[end]
    lea     r10, [r10 + rax*2]   ; r10 = &textBuf[start]
    xor     esi, esi
@cmd_delmov:
    cmp     esi, edx
    jge     @cmd_deldone
    mov     r9w, word ptr [r11 + rsi*2]
    mov     word ptr [r10 + rsi*2], r9w
    inc     esi
    jmp     @cmd_delmov
@cmd_deldone:
    sub     g_totalChars, ecx
    mov     g_selStart, -1
    mov     g_selEnd, -1
    call    RebuildLineTable
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

; ── WM_SETCURSOR: set I-beam cursor in client area ──────────────
@wm_setcursor:
    ; Only change cursor if in client area (LOWORD(lParam) == HTCLIENT == 1)
    mov     eax, dword ptr [rbp-20h]
    and     eax, 0FFFFh
    cmp     eax, 1               ; HTCLIENT
    jne     @wm_setcursor_def
    ; Load I-beam cursor
    xor     ecx, ecx            ; hInstance = NULL (system cursor)
    mov     edx, 32513           ; IDC_IBEAM
    call    LoadCursorW
    test    rax, rax
    jz      @ret_zero
    mov     rcx, rax
    call    SetCursor
    ; Return TRUE to prevent DefWindowProc from resetting cursor
    mov     eax, 1
    jmp     @wp_ret
@wm_setcursor_def:
    mov     rcx, qword ptr [rbp-8]
    mov     edx, dword ptr [rbp-0Ch]
    mov     r8, qword ptr [rbp-18h]
    mov     r9, qword ptr [rbp-20h]
    call    DefWindowProcW
    jmp     @wp_ret

@wm_paint:
    mov     rcx, qword ptr [rbp-8]
    lea     rdx, [rbp-60h]
    call    BeginPaint
    mov     rbx, rax
    mov     rcx, rbx
    mov     rdx, hEditorFont
    test    rdx, rdx
    jz      @p_nofont
    call    SelectObject
@p_nofont:
    mov     rcx, rbx
    mov     edx, OPAQUE
    call    SetBkMode
    mov     dword ptr [rbp-0F0h], 0
    mov     dword ptr [rbp-0ECh], 0
    mov     eax, g_clientW
    mov     dword ptr [rbp-0E8h], eax
    mov     eax, g_clientH
    mov     dword ptr [rbp-0E4h], eax
    mov     rcx, rbx
    lea     rdx, [rbp-0F0h]
    mov     r8d, COLOR_WINDOW + 1
    call    FillRect
    mov     dword ptr [rbp-0F0h], 0
    mov     dword ptr [rbp-0ECh], 0
    mov     dword ptr [rbp-0E8h], GUTTER_WIDTH
    mov     eax, g_clientH
    mov     dword ptr [rbp-0E4h], eax
    mov     rcx, rbx
    lea     rdx, [rbp-0F0h]
    mov     r8d, 16
    call    FillRect
    mov     eax, g_clientH
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    inc     eax

    mov     edi, eax
    mov     esi, g_scrollY
@p_line:
    test    edi, edi
    jz      @p_done
    cmp     esi, g_lineCount
    jge     @p_done
    mov     rcx, rbx
    mov     edx, 00808080h
    call    SetTextColor
    mov     rcx, rbx
    mov     edx, 00F0F0F0h
    call    SetBkColor
    lea     rdi, g_numBuf
    mov     eax, esi
    inc     eax
    call    FormatLineNum
    mov     rcx, rbx
    mov     edx, 4
    mov     r8d, esi
    sub     r8d, g_scrollY
    imul    r8d, LINE_HEIGHT
    lea     r9, g_numBuf
    mov     dword ptr [rsp+20h], 5
    call    TextOutW
    mov     rcx, rbx
    xor     edx, edx
    call    SetTextColor
    mov     rcx, rbx
    mov     edx, 00FFFFFFh
    call    SetBkColor
    lea     rax, g_lineOff
    mov     ecx, dword ptr [rax + rsi*4]
    mov     edx, esi
    inc     edx
    cmp     edx, g_lineCount
    jge     @p_lastln
    mov     edx, dword ptr [rax + rdx*4]
    sub     edx, ecx
    dec     edx
    jmp     @p_text
@p_lastln:
    mov     edx, g_totalChars
    sub     edx, ecx
@p_text:
    test    edx, edx
    jle     @p_next
    cmp     edx, 200
    jle     @p_lenok
    mov     edx, 200
@p_lenok:
    mov     dword ptr [rbp-0D0h], ecx
    mov     dword ptr [rbp-0D4h], edx
    mov     rcx, rbx
    mov     edx, GUTTER_WIDTH + 4
    mov     r8d, esi
    sub     r8d, g_scrollY
    imul    r8d, LINE_HEIGHT
    mov     eax, dword ptr [rbp-0D0h]
    lea     r9, g_textBuf
    lea     r9, [r9 + rax*2]
    mov     eax, dword ptr [rbp-0D4h]
    mov     dword ptr [rsp+20h], eax
    call    TextOutW
@p_next:
    inc     esi
    dec     edi
    jmp     @p_line
@p_done:
    mov     rcx, qword ptr [rbp-8]
    lea     rdx, [rbp-60h]
    call    EndPaint
    jmp     @ret_zero

@wm_destroy:
    xor     ecx, ecx
    call    PostQuitMessage
    jmp     @ret_zero

@ret_zero:
    xor     eax, eax
@wp_ret:
    lea     rsp, [rbp]
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
WndProc ENDP

RebuildLineTable PROC
    lea     rdi, g_textBuf
    lea     rdx, g_lineOff
    xor     ecx, ecx
    mov     dword ptr [rdx], 0
    inc     ecx
    xor     eax, eax
@rlt_loop:
    cmp     eax, g_totalChars
    jge     @rlt_done
    cmp     word ptr [rdi + rax*2], 0Ah
    jne     @rlt_next
    lea     r8d, [eax+1]
    cmp     ecx, MAX_LINES
    jge     @rlt_done
    mov     dword ptr [rdx + rcx*4], r8d
    inc     ecx
@rlt_next:
    inc     eax
    jmp     @rlt_loop
@rlt_done:
    mov     g_lineCount, ecx
    ret
RebuildLineTable ENDP

GetCurLineLen PROC
    mov     ecx, g_cursorLine
    lea     rax, g_lineOff
    mov     edx, dword ptr [rax + rcx*4]
    inc     ecx
    cmp     ecx, g_lineCount
    jge     @gcl_last
    mov     eax, dword ptr [rax + rcx*4]
    sub     eax, edx
    dec     eax
    ret
@gcl_last:
    mov     eax, g_totalChars
    sub     eax, edx
    ret
GetCurLineLen ENDP

PositionCaret PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    mov     eax, g_cursorCol
    imul    eax, CHAR_WIDTH
    add     eax, GUTTER_WIDTH + 4
    mov     ecx, eax
    mov     edx, g_cursorLine
    sub     edx, g_scrollY
    imul    edx, LINE_HEIGHT
    call    SetCaretPos
    add     rsp, 28h
    ret
PositionCaret ENDP

EnsureCursorVisible PROC
    mov     eax, g_clientH
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    test    eax, eax
    jz      @ecv_done
    mov     ecx, eax
    mov     eax, g_cursorLine
    cmp     eax, g_scrollY
    jl      @ecv_up
    mov     edx, g_scrollY
    add     edx, ecx
    dec     edx
    cmp     eax, edx
    jg      @ecv_dn
    jmp     @ecv_done
@ecv_up:
    mov     g_scrollY, eax
    jmp     @ecv_done
@ecv_dn:
    mov     edx, eax
    sub     edx, ecx
    inc     edx
    test    edx, edx
    jns     @ecv_set
    xor     edx, edx
@ecv_set:
    mov     g_scrollY, edx
@ecv_done:
    ret
EnsureCursorVisible ENDP

UpdateScrollBar PROC FRAME
    sub     rsp, 48h
    .allocstack 48h
    .endprolog
    lea     rax, [rsp+20h]
    mov     dword ptr [rax], 28
    mov     dword ptr [rax+4], SIF_ALL
    mov     dword ptr [rax+8], 0
    mov     ecx, g_lineCount
    mov     dword ptr [rax+0Ch], ecx
    mov     ecx, g_clientH
    push    rax
    mov     eax, ecx
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    mov     ecx, eax
    pop     rax
    mov     dword ptr [rax+10h], ecx
    mov     ecx, g_scrollY
    mov     dword ptr [rax+14h], ecx
    mov     dword ptr [rax+18h], 0
    mov     rcx, hMainWnd
    mov     edx, SB_VERT
    lea     r8, [rsp+20h]
    mov     r9d, 1
    call    SetScrollInfo
    add     rsp, 48h
    ret
UpdateScrollBar ENDP

FormatLineNum PROC
    lea     rdi, g_numBuf
    mov     word ptr [rdi],    20h
    mov     word ptr [rdi+2],  20h
    mov     word ptr [rdi+4],  20h
    mov     word ptr [rdi+6],  20h
    mov     word ptr [rdi+8],  20h
    mov     word ptr [rdi+10], 0
    lea     rdi, [rdi+8]
    mov     ecx, 0
@fln_loop:
    xor     edx, edx
    mov     r8d, 10
    div     r8d
    add     edx, 30h
    mov     word ptr [rdi], dx
    sub     rdi, 2
    inc     ecx
    test    eax, eax
    jnz     @fln_loop
    ret
FormatLineNum ENDP

CreateEditorPane PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    mov     rax, hMainWnd
    add     rsp, 28h
    ret
CreateEditorPane ENDP

; ── DeleteSelection: removes selected text, updates cursor ───────
; Returns: eax = number of chars deleted (0 if no selection)
; Side effects: updates g_totalChars, g_cursorLine, g_cursorCol,
;               clears g_selStart/g_selEnd, calls RebuildLineTable
DeleteSelection PROC
    mov     eax, g_selStart
    cmp     eax, -1
    je      @ds_none
    mov     edi, g_selEnd
    ; normalize: eax = min, edi = max
    cmp     eax, edi
    jle     @ds_ordered
    xchg    eax, edi
@ds_ordered:
    mov     ecx, edi
    sub     ecx, eax            ; ecx = chars to delete
    test    ecx, ecx
    jle     @ds_none
    push    rcx                 ; save delete count

    ; shift buffer left: move [edi..totalChars) to [eax..)
    lea     r10, g_textBuf
    mov     edx, g_totalChars
    sub     edx, edi            ; edx = trailing chars
    test    edx, edx
    jle     @ds_shifted
    lea     r11, [r10 + rdi*2]  ; src = &textBuf[end]
    lea     r10, [r10 + rax*2]  ; dst = &textBuf[start]
    xor     esi, esi
@ds_movloop:
    cmp     esi, edx
    jge     @ds_shifted
    mov     r9w, word ptr [r11 + rsi*2]
    mov     word ptr [r10 + rsi*2], r9w
    inc     esi
    jmp     @ds_movloop
@ds_shifted:
    pop     rcx
    sub     g_totalChars, ecx
    ; null-terminate
    mov     edx, g_totalChars
    lea     r10, g_textBuf
    mov     word ptr [r10 + rdx*2], 0

    ; clear selection
    mov     g_selStart, -1
    mov     g_selEnd, -1

    ; update cursor position to deletion start
    ; Find which line and column 'eax' (start offset) corresponds to
    push    rax                 ; save start offset
    call    RebuildLineTable
    pop     rax                 ; restore start offset

    ; Find cursor line: scan g_lineOff to find which line contains offset eax
    lea     rdx, g_lineOff
    xor     ecx, ecx            ; line index
@ds_findline:
    inc     ecx
    cmp     ecx, g_lineCount
    jge     @ds_lastline
    cmp     eax, dword ptr [rdx + rcx*4]
    jge     @ds_findline
    dec     ecx
    mov     g_cursorLine, ecx
    mov     ecx, dword ptr [rdx + rcx*4]
    sub     eax, ecx
    mov     g_cursorCol, eax
    jmp     @ds_ret
@ds_lastline:
    dec     ecx
    mov     g_cursorLine, ecx
    mov     ecx, dword ptr [rdx + rcx*4]
    sub     eax, ecx
    mov     g_cursorCol, eax
@ds_ret:
    mov     eax, 1              ; return nonzero = selection was deleted
    ret
@ds_none:
    xor     eax, eax            ; return 0 = no selection
    ret
DeleteSelection ENDP

END
