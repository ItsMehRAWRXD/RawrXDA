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
    lea     rax, g_textBuf
    mov     word ptr [rax], 0
    call    UpdateScrollBar
    jmp     @ret_zero

@wm_setfocus:
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
    cmp     eax, 8
    je      @char_bs
    cmp     eax, 0Dh
    je      @char_enter
    cmp     eax, 9
    je      @char_tab
    cmp     eax, 20h
    jb      @ret_zero
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

END
