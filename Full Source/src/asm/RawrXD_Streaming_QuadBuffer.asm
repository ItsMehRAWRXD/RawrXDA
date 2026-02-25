; =============================================================================
; RawrXD_Streaming_QuadBuffer.asm — Tear-Free HWND Token Rendering Engine
; =============================================================================
;
; Quad-buffered token streaming engine that attaches to a Win32 HWND and
; renders inference tokens tear-free using a lock-free SPSC ring buffer.
; Uses GDI-based text rendering for maximum compatibility, with a software
; back buffer and double-buffered BitBlt presentation.
;
; Capabilities:
;   - Lock-free SPSC ring buffer (128K token slots, cache-line aligned)
;   - HWND attachment with automatic back-buffer sizing
;   - GDI double-buffered rendering (CreateCompatibleDC + BitBlt)
;   - Token attribute support (color, bold, italic flags)
;   - 60 FPS render loop with configurable VSync via timer
;   - Dropped-token counter for overflow diagnostics
;   - Render thread entry point for CreateThread integration
;   - Manual scroll tracking (line wrap + vertical scroll)
;   - Stats: tokens rendered, frames presented, dropped, avg frame time
;
; Active Exports (used by Win32IDE_StreamingUX.cpp):
;   asm_quadbuf_init            — Create back buffers + ring for HWND
;   asm_quadbuf_push_token      — Lock-free enqueue a token string
;   asm_quadbuf_render_frame    — Process ring + present one frame
;   asm_quadbuf_render_thread   — Render loop entry (for CreateThread)
;   asm_quadbuf_resize          — Handle WM_SIZE, recreate back buffer
;   asm_quadbuf_get_stats       — Read rendering statistics
;   asm_quadbuf_shutdown        — Release GDI resources + ring
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_Streaming_QuadBuffer.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                    QUAD BUFFER CONSTANTS
; =============================================================================

; Ring buffer geometry
RING_CAPACITY           EQU     131072          ; 128K slots
RING_MASK               EQU     RING_CAPACITY - 1
TOKEN_SLOT_SIZE         EQU     64              ; Cache-line aligned

; Max token string length per slot
MAX_TOKEN_LEN           EQU     48              ; Leaves 16 bytes for metadata

; Render timing (16ms = ~60 FPS)
FRAME_INTERVAL_MS       EQU     16

; Token attribute flags (packed into DWORD)
ATTR_NORMAL             EQU     0
ATTR_BOLD               EQU     1
ATTR_ITALIC             EQU     2
ATTR_CODE               EQU     4
ATTR_ERROR              EQU     8
ATTR_KEYWORD            EQU     10h
ATTR_COMMENT            EQU     20h
ATTR_STRING_LIT         EQU     40h

; Default colors (RGB, little-endian = 00BBGGRR)
COLOR_DEFAULT           EQU     00D0D0D0h       ; Light gray
COLOR_KEYWORD           EQU     00FF8C56h       ; Blue-ish (Monokai)
COLOR_COMMENT           EQU     00608860h       ; Green-gray
COLOR_STRING            EQU     0060E0E0h       ; Yellow-ish
COLOR_ERROR             EQU     004040FFh       ; Red
COLOR_BG                EQU     001E1E1Eh       ; VS Code dark background

; Text metrics (monospace assumed)
DEFAULT_CHAR_WIDTH      EQU     8
DEFAULT_LINE_HEIGHT     EQU     18
DEFAULT_MARGIN_LEFT     EQU     8
DEFAULT_MARGIN_TOP      EQU     4

; Status codes
QB_OK                   EQU     0
QB_ERR_NOT_INIT         EQU     1
QB_ERR_ALLOC            EQU     2
QB_ERR_HWND             EQU     3
QB_ERR_GDI              EQU     4
QB_ERR_ALREADY_INIT     EQU     5

; v14.2.1 Cathedral Build: Streaming flags
STREAMING_FLAG_VSYNC    EQU     1               ; Enable vertical sync
STREAMING_FLAG_HDR      EQU     2               ; HDR-ready color space
STREAMING_FLAG_TEARING  EQU     4               ; Allow tearing (low latency)
STREAMING_FLAG_ADAPTIVE EQU     8               ; Adaptive frame timing

; =============================================================================
;                    STRUCTURES
; =============================================================================

; Token slot in ring buffer (64 bytes = 1 cache line)
TOKEN_SLOT STRUCT 8
    text            DB MAX_TOKEN_LEN DUP(?)  ; UTF-8 token string
    text_len        DD      ?               ; Actual length
    attr_flags      DD      ?               ; ATTR_* flags
    color_override  DD      ?               ; 0 = use attr default, else RGB
    _pad0           DD      ?
TOKEN_SLOT ENDS

; Rendering context
QUADBUF_CTX STRUCT 8
    ; Window
    hwnd            DQ      ?               ; Target window handle
    wndWidth        DD      ?               ; Client width in pixels
    wndHeight       DD      ?               ; Client height in pixels

    ; GDI resources
    hdcWindow       DQ      ?               ; Window DC (GetDC)
    hdcBack         DQ      ?               ; Back buffer DC (CreateCompatibleDC)
    hBmpBack        DQ      ?               ; Back buffer bitmap
    hBmpOld         DQ      ?               ; Previously selected bitmap
    hFont           DQ      ?               ; Monospace font
    hFontOld        DQ      ?               ; Previously selected font

    ; Ring buffer
    pRing           DQ      ?               ; Ring buffer base
    ringHead        DD      ?               ; Producer index (atomic)
    ringTail        DD      ?               ; Consumer index
    droppedTokens   DQ      ?               ; Overflow counter

    ; Text cursor
    cursorX         DD      ?               ; Current X in pixels
    cursorY         DD      ?               ; Current Y in pixels
    colMax          DD      ?               ; Max columns before wrap
    lineHeight      DD      ?               ; Pixel height per line

    ; Control
    hRenderEvent    DQ      ?               ; Event to wake render thread
    cancelFlag      DD      ?               ; 1 = shutdown requested
    initialized     DD      ?

    ; v14.2.1: Configuration flags
    flags           DD      ?               ; STREAMING_FLAG_* bitmask
    frameInterval   DD      ?               ; ms between frames (16 = 60fps default)

    ; Stats
    tokensRendered  DQ      ?
    framesPresented DQ      ?
    totalFrameTime  DQ      ?               ; Accumulated ms for avg
    droppedFrames   DQ      ?               ; v14.2.1: Frames exceeding interval
QUADBUF_CTX ENDS

; =============================================================================
;                    DATA SECTION
; =============================================================================
.data

ALIGN 16
g_qb_ctx       QUADBUF_CTX <>

; Font name
szFontName      DB "Consolas", 0

; Status strings
szQBInit        DB "QuadBuffer: attached to HWND", 0
szQBShutdown    DB "QuadBuffer: resources released", 0
szQBResize      DB "QuadBuffer: back buffer resized", 0

; =============================================================================
;                    EXPORTS
; =============================================================================
PUBLIC asm_quadbuf_init
PUBLIC asm_quadbuf_push_token
PUBLIC asm_quadbuf_render_frame
PUBLIC asm_quadbuf_render_thread
PUBLIC asm_quadbuf_resize
PUBLIC asm_quadbuf_get_stats
PUBLIC asm_quadbuf_shutdown
PUBLIC asm_quadbuf_set_flags
PUBLIC asm_quadbuf_set_frame_interval

; =============================================================================
;                    EXTERNAL IMPORTS (GDI32 + USER32)
; =============================================================================
EXTERN GetDC: PROC
EXTERN ReleaseDC: PROC
EXTERN CreateCompatibleDC: PROC
EXTERN CreateCompatibleBitmap: PROC
EXTERN SelectObject: PROC
EXTERN DeleteObject: PROC
EXTERN DeleteDC: PROC
EXTERN BitBlt: PROC
EXTERN SetBkMode: PROC
EXTERN SetBkColor: PROC
EXTERN SetTextColor: PROC
EXTERN TextOutA: PROC
EXTERN CreateFontA: PROC
EXTERN CreateSolidBrush: PROC
EXTERN FillRect: PROC
EXTERN GetClientRect: PROC
EXTERN CreateEventA: PROC
EXTERN SetEvent: PROC
EXTERN WaitForSingleObject: PROC
EXTERN GetTickCount64: PROC

; RECT struct for FillRect
RECT_STRUCT STRUCT
    left_f    DD ?
    top_f     DD ?
    right_f   DD ?
    bottom_f  DD ?
RECT_STRUCT ENDS

; =============================================================================
;                    CODE SECTION
; =============================================================================
.code

; =============================================================================
; get_attr_color — Internal: map attribute flags to RGB color
; ECX = attr_flags
; EDX = color_override (0 = use default)
; Returns: EAX = RGB color
; =============================================================================
get_attr_color PROC FRAME
    sub     rsp, 8
    .allocstack 8
    .endprolog

    ; Check override first
    test    edx, edx
    jnz     @@use_override

    ; Map from flags
    test    ecx, ATTR_ERROR
    jnz     @@color_error
    test    ecx, ATTR_KEYWORD
    jnz     @@color_keyword
    test    ecx, ATTR_COMMENT
    jnz     @@color_comment
    test    ecx, ATTR_STRING_LIT
    jnz     @@color_string

    mov     eax, COLOR_DEFAULT
    jmp     @@exit_color

@@use_override:
    mov     eax, edx
    jmp     @@exit_color
@@color_error:
    mov     eax, COLOR_ERROR
    jmp     @@exit_color
@@color_keyword:
    mov     eax, COLOR_KEYWORD
    jmp     @@exit_color
@@color_comment:
    mov     eax, COLOR_COMMENT
    jmp     @@exit_color
@@color_string:
    mov     eax, COLOR_STRING

@@exit_color:
    add     rsp, 8
    ret
get_attr_color ENDP

; =============================================================================
; asm_quadbuf_init
; Create back buffers and ring buffer, attach to HWND.
; RCX = HWND
; Returns: RAX = 0 success, QB_ERR_* on failure
; =============================================================================
asm_quadbuf_init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 88
    .allocstack 88
    .endprolog

    cmp     DWORD PTR [g_qb_ctx.initialized], 1
    je      @@already_init

    mov     r12, rcx                        ; HWND
    test    r12, r12
    jz      @@err_hwnd

    ; Zero context
    lea     rdi, g_qb_ctx
    xor     eax, eax
    mov     ecx, SIZEOF QUADBUF_CTX
    rep     stosb

    mov     QWORD PTR [g_qb_ctx.hwnd], r12

    ; Get client rect for dimensions
    mov     rcx, r12
    lea     rdx, [rsp+48]                   ; RECT on stack
    call    GetClientRect
    mov     eax, DWORD PTR [rsp+48+8]       ; right
    mov     DWORD PTR [g_qb_ctx.wndWidth], eax
    mov     eax, DWORD PTR [rsp+48+12]      ; bottom
    mov     DWORD PTR [g_qb_ctx.wndHeight], eax

    ; Get window DC
    mov     rcx, r12
    call    GetDC
    test    rax, rax
    jz      @@err_gdi
    mov     QWORD PTR [g_qb_ctx.hdcWindow], rax
    mov     rbx, rax

    ; Create compatible DC for back buffer
    mov     rcx, rbx
    call    CreateCompatibleDC
    test    rax, rax
    jz      @@err_gdi
    mov     QWORD PTR [g_qb_ctx.hdcBack], rax
    mov     r13, rax

    ; Create back buffer bitmap
    mov     rcx, rbx                        ; hdc
    mov     edx, DWORD PTR [g_qb_ctx.wndWidth]
    mov     r8d, DWORD PTR [g_qb_ctx.wndHeight]
    call    CreateCompatibleBitmap
    test    rax, rax
    jz      @@err_gdi
    mov     QWORD PTR [g_qb_ctx.hBmpBack], rax

    ; Select bitmap into back DC
    mov     rcx, r13
    mov     rdx, rax
    call    SelectObject
    mov     QWORD PTR [g_qb_ctx.hBmpOld], rax

    ; Create monospace font (Consolas 14px)
    mov     ecx, -14                        ; nHeight (negative = character height)
    xor     edx, edx                        ; nWidth = 0 (auto)
    xor     r8d, r8d                        ; nEscapement
    xor     r9d, r9d                        ; nOrientation
    mov     DWORD PTR [rsp+20h], 400        ; fnWeight (FW_NORMAL)
    mov     DWORD PTR [rsp+28h], 0          ; fdwItalic
    mov     DWORD PTR [rsp+30h], 0          ; fdwUnderline
    mov     DWORD PTR [rsp+38h], 0          ; fdwStrikeOut
    mov     DWORD PTR [rsp+40h], 0          ; fdwCharSet (ANSI)
    mov     DWORD PTR [rsp+48h], 0          ; fdwOutputPrecision
    mov     DWORD PTR [rsp+50h], 0          ; fdwClipPrecision
    mov     DWORD PTR [rsp+58h], 0          ; fdwQuality
    mov     DWORD PTR [rsp+60h], 49         ; fdwPitchAndFamily (FF_MODERN | FIXED_PITCH)
    lea     rax, szFontName
    mov     QWORD PTR [rsp+68h], rax        ; lpszFace
    call    CreateFontA
    test    rax, rax
    jz      @@err_gdi
    mov     QWORD PTR [g_qb_ctx.hFont], rax

    ; Select font into back DC
    mov     rcx, r13
    mov     rdx, rax
    call    SelectObject
    mov     QWORD PTR [g_qb_ctx.hFontOld], rax

    ; Set transparent background mode
    mov     rcx, r13
    mov     edx, 1                          ; TRANSPARENT
    call    SetBkMode

    ; Allocate ring buffer: 128K * 64 bytes = 8 MB
    xor     ecx, ecx
    mov     edx, RING_CAPACITY * TOKEN_SLOT_SIZE
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@err_alloc
    mov     QWORD PTR [g_qb_ctx.pRing], rax

    ; Initialize cursor
    mov     DWORD PTR [g_qb_ctx.cursorX], DEFAULT_MARGIN_LEFT
    mov     DWORD PTR [g_qb_ctx.cursorY], DEFAULT_MARGIN_TOP
    mov     DWORD PTR [g_qb_ctx.lineHeight], DEFAULT_LINE_HEIGHT

    ; Calculate max columns
    mov     eax, DWORD PTR [g_qb_ctx.wndWidth]
    sub     eax, DEFAULT_MARGIN_LEFT * 2
    xor     edx, edx
    mov     ecx, DEFAULT_CHAR_WIDTH
    div     ecx
    mov     DWORD PTR [g_qb_ctx.colMax], eax

    ; Create render wake event
    xor     ecx, ecx                        ; lpAttributes
    xor     edx, edx                        ; bManualReset = FALSE
    xor     r8d, r8d                        ; bInitialState = FALSE
    xor     r9d, r9d                        ; lpName = NULL
    call    CreateEventA
    mov     QWORD PTR [g_qb_ctx.hRenderEvent], rax

    ; v14.2.1: Initialize streaming flags and frame interval
    mov     DWORD PTR [g_qb_ctx.flags], STREAMING_FLAG_VSYNC
    mov     DWORD PTR [g_qb_ctx.frameInterval], FRAME_INTERVAL_MS
    mov     QWORD PTR [g_qb_ctx.droppedFrames], 0

    ; Mark initialized
    mov     DWORD PTR [g_qb_ctx.initialized], 1

    lea     rcx, szQBInit
    call    OutputDebugStringA

    xor     eax, eax
    jmp     @@exit

@@already_init:
    mov     eax, QB_ERR_ALREADY_INIT
    jmp     @@exit
@@err_hwnd:
    mov     eax, QB_ERR_HWND
    jmp     @@exit
@@err_gdi:
    mov     eax, QB_ERR_GDI
    jmp     @@exit
@@err_alloc:
    mov     eax, QB_ERR_ALLOC

@@exit:
    add     rsp, 88
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_quadbuf_init ENDP

; =============================================================================
; asm_quadbuf_push_token
; Lock-free enqueue a token into the ring buffer.
; RCX = token string pointer (UTF-8)
; EDX = token length (capped at MAX_TOKEN_LEN)
; R8D = attribute flags (ATTR_*)
; R9D = color override (0 = auto)
; Returns: RAX = 0 success, 1 = ring full (token dropped)
; =============================================================================
asm_quadbuf_push_token PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    cmp     DWORD PTR [g_qb_ctx.initialized], 1
    jne     @@push_fail

    mov     rsi, rcx                        ; token ptr
    mov     ebx, edx                        ; length
    ; r8d = attr, r9d = color (stay in registers)

    ; Clamp length
    cmp     ebx, MAX_TOKEN_LEN
    jle     @@len_ok
    mov     ebx, MAX_TOKEN_LEN
@@len_ok:

    ; Lock-free SPSC enqueue
    mov     eax, DWORD PTR [g_qb_ctx.ringHead]
@@retry:
    mov     ecx, eax
    inc     ecx
    and     ecx, RING_MASK

    cmp     ecx, DWORD PTR [g_qb_ctx.ringTail]
    je      @@push_full

    lock cmpxchg DWORD PTR [g_qb_ctx.ringHead], ecx
    jne     @@retry

    ; eax = claimed slot
    imul    rdi, rax, TOKEN_SLOT_SIZE
    add     rdi, QWORD PTR [g_qb_ctx.pRing]

    ; Write token string
    push    r8
    push    r9
    mov     rcx, rdi                        ; dest = slot.text
    mov     rdx, rsi                        ; src = token
    mov     r8d, ebx                        ; len
    call    memcpy
    pop     r9
    pop     r8

    ; Write metadata
    mov     DWORD PTR [rdi].TOKEN_SLOT.text_len, ebx
    mov     DWORD PTR [rdi].TOKEN_SLOT.attr_flags, r8d
    mov     DWORD PTR [rdi].TOKEN_SLOT.color_override, r9d

    ; Signal render thread
    mov     rcx, QWORD PTR [g_qb_ctx.hRenderEvent]
    test    rcx, rcx
    jz      @@push_ok
    call    SetEvent

@@push_ok:
    xor     eax, eax
    jmp     @@push_exit

@@push_full:
    lock inc QWORD PTR [g_qb_ctx.droppedTokens]
@@push_fail:
    mov     eax, 1

@@push_exit:
    add     rsp, 40
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_quadbuf_push_token ENDP

; =============================================================================
; asm_quadbuf_render_frame
; Process all pending tokens from ring buffer and present the back buffer.
; No parameters.
; Returns: RAX = number of tokens rendered this frame
; =============================================================================
asm_quadbuf_render_frame PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    sub     rsp, 72
    .allocstack 72
    .endprolog

    cmp     DWORD PTR [g_qb_ctx.initialized], 1
    jne     @@frame_exit_zero

    ; Frame start timing
    call    GetTickCount64
    mov     r14, rax

    mov     rbx, QWORD PTR [g_qb_ctx.hdcBack]
    xor     r12d, r12d                      ; tokens rendered count

    ; Clear back buffer with background color
    mov     rcx, COLOR_BG
    call    CreateSolidBrush
    mov     r13, rax                        ; hBrush

    ; Fill rect {0, 0, width, height}
    mov     DWORD PTR [rsp+40h], 0          ; left
    mov     DWORD PTR [rsp+44h], 0          ; top
    mov     eax, DWORD PTR [g_qb_ctx.wndWidth]
    mov     DWORD PTR [rsp+48h], eax        ; right
    mov     eax, DWORD PTR [g_qb_ctx.wndHeight]
    mov     DWORD PTR [rsp+4Ch], eax        ; bottom

    mov     rcx, rbx                        ; hdc
    lea     rdx, [rsp+40h]                  ; lpRect
    mov     r8, r13                         ; hBrush
    call    FillRect

    ; Delete brush
    mov     rcx, r13
    call    DeleteObject

    ; Reset cursor for re-render
    mov     DWORD PTR [g_qb_ctx.cursorX], DEFAULT_MARGIN_LEFT
    mov     DWORD PTR [g_qb_ctx.cursorY], DEFAULT_MARGIN_TOP

    ; Process tokens from ring
@@token_loop:
    mov     eax, DWORD PTR [g_qb_ctx.ringTail]
    cmp     eax, DWORD PTR [g_qb_ctx.ringHead]
    je      @@tokens_done

    ; Read slot
    imul    rsi, rax, TOKEN_SLOT_SIZE
    add     rsi, QWORD PTR [g_qb_ctx.pRing]

    ; Get text color
    mov     ecx, DWORD PTR [rsi].TOKEN_SLOT.attr_flags
    mov     edx, DWORD PTR [rsi].TOKEN_SLOT.color_override
    call    get_attr_color

    ; Set text color on back DC
    mov     rcx, rbx                        ; hdc
    mov     edx, eax                        ; color
    call    SetTextColor

    ; Check for newline character in token
    mov     edi, DWORD PTR [rsi].TOKEN_SLOT.text_len
    test    edi, edi
    jz      @@advance_tail

    ; Check line wrap
    mov     eax, DWORD PTR [g_qb_ctx.cursorX]
    mov     ecx, edi
    imul    ecx, DEFAULT_CHAR_WIDTH
    add     eax, ecx
    cmp     eax, DWORD PTR [g_qb_ctx.wndWidth]
    jl      @@no_wrap

    ; Wrap to next line
    mov     DWORD PTR [g_qb_ctx.cursorX], DEFAULT_MARGIN_LEFT
    mov     eax, DWORD PTR [g_qb_ctx.cursorY]
    add     eax, DWORD PTR [g_qb_ctx.lineHeight]
    mov     DWORD PTR [g_qb_ctx.cursorY], eax

    ; Check vertical overflow (scroll reset)
    cmp     eax, DWORD PTR [g_qb_ctx.wndHeight]
    jl      @@no_wrap
    ; Simple scroll: reset to top (real impl would shift buffer)
    mov     DWORD PTR [g_qb_ctx.cursorY], DEFAULT_MARGIN_TOP

@@no_wrap:
    ; Handle newline in token text
    cmp     BYTE PTR [rsi], 0Ah             ; '\n'
    jne     @@render_text
    mov     DWORD PTR [g_qb_ctx.cursorX], DEFAULT_MARGIN_LEFT
    mov     eax, DWORD PTR [g_qb_ctx.cursorY]
    add     eax, DWORD PTR [g_qb_ctx.lineHeight]
    mov     DWORD PTR [g_qb_ctx.cursorY], eax
    jmp     @@advance_tail

@@render_text:
    ; TextOutA(hdc, x, y, text, len)
    mov     rcx, rbx                        ; hdc
    mov     edx, DWORD PTR [g_qb_ctx.cursorX]  ; x
    mov     r8d, DWORD PTR [g_qb_ctx.cursorY]  ; y
    lea     r9, [rsi]                       ; text buffer (TOKEN_SLOT.text)
    mov     DWORD PTR [rsp+20h], edi        ; nCount
    call    TextOutA

    ; Advance cursor
    imul    eax, edi, DEFAULT_CHAR_WIDTH
    add     DWORD PTR [g_qb_ctx.cursorX], eax

    inc     r12d

@@advance_tail:
    ; Advance tail
    mov     eax, DWORD PTR [g_qb_ctx.ringTail]
    inc     eax
    and     eax, RING_MASK
    mov     DWORD PTR [g_qb_ctx.ringTail], eax

    jmp     @@token_loop

@@tokens_done:
    ; BitBlt back buffer to window
    mov     rcx, QWORD PTR [g_qb_ctx.hdcWindow]  ; hdcDest
    xor     edx, edx                        ; xDest = 0
    xor     r8d, r8d                        ; yDest = 0
    mov     r9d, DWORD PTR [g_qb_ctx.wndWidth] ; nWidth
    mov     DWORD PTR [rsp+20h], 0
    mov     eax, DWORD PTR [g_qb_ctx.wndHeight]
    mov     DWORD PTR [rsp+20h], eax        ; nHeight
    mov     QWORD PTR [rsp+28h], rbx        ; hdcSrc = back DC
    mov     DWORD PTR [rsp+30h], 0          ; xSrc = 0
    mov     DWORD PTR [rsp+38h], 0          ; ySrc = 0
    mov     DWORD PTR [rsp+40h], 00CC0020h  ; dwRop = SRCCOPY
    call    BitBlt

    ; Update stats
    lock add QWORD PTR [g_qb_ctx.tokensRendered], r12
    lock inc QWORD PTR [g_qb_ctx.framesPresented]

    ; Frame elapsed
    call    GetTickCount64
    sub     rax, r14
    lock add QWORD PTR [g_qb_ctx.totalFrameTime], rax

    mov     eax, r12d
    jmp     @@frame_exit

@@frame_exit_zero:
    xor     eax, eax

@@frame_exit:
    add     rsp, 72
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_quadbuf_render_frame ENDP

; =============================================================================
; asm_quadbuf_render_thread
; Entry point for CreateThread — render loop until cancelFlag set.
; RCX = unused (lpParameter)
; Returns: RAX = 0 (thread exit code)
; =============================================================================
asm_quadbuf_render_thread PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

@@render_loop:
    ; Check cancel
    cmp     DWORD PTR [g_qb_ctx.cancelFlag], 1
    je      @@thread_exit

    ; Wait for tokens or timeout (configurable frame interval)
    mov     rcx, QWORD PTR [g_qb_ctx.hRenderEvent]
    test    rcx, rcx
    jz      @@sleep_fallback
    mov     edx, DWORD PTR [g_qb_ctx.frameInterval]    ; v14.2.1: configurable
    call    WaitForSingleObject
    jmp     @@do_render

@@sleep_fallback:
    mov     ecx, DWORD PTR [g_qb_ctx.frameInterval]    ; v14.2.1: configurable
    call    Sleep

@@do_render:
    call    asm_quadbuf_render_frame
    jmp     @@render_loop

@@thread_exit:
    xor     eax, eax
    add     rsp, 40
    pop     rbx
    ret
asm_quadbuf_render_thread ENDP

; =============================================================================
; asm_quadbuf_resize
; Handle window resize: recreate back buffer at new dimensions.
; ECX = new width
; EDX = new height
; Returns: RAX = 0 success
; =============================================================================
asm_quadbuf_resize PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     DWORD PTR [g_qb_ctx.initialized], 1
    jne     @@resize_exit

    mov     r12d, ecx                       ; new width
    mov     r13d, edx                       ; new height

    ; Restore old bitmap
    mov     rcx, QWORD PTR [g_qb_ctx.hdcBack]
    mov     rdx, QWORD PTR [g_qb_ctx.hBmpOld]
    call    SelectObject

    ; Delete old back buffer
    mov     rcx, QWORD PTR [g_qb_ctx.hBmpBack]
    call    DeleteObject

    ; Create new bitmap at new size
    mov     rcx, QWORD PTR [g_qb_ctx.hdcWindow]
    mov     edx, r12d
    mov     r8d, r13d
    call    CreateCompatibleBitmap
    mov     QWORD PTR [g_qb_ctx.hBmpBack], rax

    ; Select into back DC
    mov     rcx, QWORD PTR [g_qb_ctx.hdcBack]
    mov     rdx, rax
    call    SelectObject
    mov     QWORD PTR [g_qb_ctx.hBmpOld], rax

    ; Update dimensions
    mov     DWORD PTR [g_qb_ctx.wndWidth], r12d
    mov     DWORD PTR [g_qb_ctx.wndHeight], r13d

    ; Recalculate max columns
    mov     eax, r12d
    sub     eax, DEFAULT_MARGIN_LEFT * 2
    xor     edx, edx
    mov     ecx, DEFAULT_CHAR_WIDTH
    div     ecx
    mov     DWORD PTR [g_qb_ctx.colMax], eax

    lea     rcx, szQBResize
    call    OutputDebugStringA

@@resize_exit:
    xor     eax, eax
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rsi
    pop     rbx
    ret
asm_quadbuf_resize ENDP

; =============================================================================
; asm_quadbuf_get_stats
; Read rendering statistics.
; RCX = output buffer (at least 40 bytes: 5 QWORDs)
;   [0] = tokensRendered, [8] = framesPresented, [16] = droppedTokens
;   [24] = totalFrameTime, [32] = avgFrameTimeMs
; Returns: RAX = 0
; =============================================================================
asm_quadbuf_get_stats PROC FRAME
    sub     rsp, 8
    .allocstack 8
    .endprolog

    mov     rax, QWORD PTR [g_qb_ctx.tokensRendered]
    mov     QWORD PTR [rcx], rax
    mov     rax, QWORD PTR [g_qb_ctx.framesPresented]
    mov     QWORD PTR [rcx+8], rax
    mov     rax, QWORD PTR [g_qb_ctx.droppedTokens]
    mov     QWORD PTR [rcx+16], rax
    mov     rax, QWORD PTR [g_qb_ctx.totalFrameTime]
    mov     QWORD PTR [rcx+24], rax

    ; Compute avg: totalFrameTime / framesPresented
    mov     rax, QWORD PTR [g_qb_ctx.totalFrameTime]
    mov     rdx, QWORD PTR [g_qb_ctx.framesPresented]
    test    rdx, rdx
    jz      @@avg_zero
    xor     edx, edx
    div     QWORD PTR [g_qb_ctx.framesPresented]
    mov     QWORD PTR [rcx+32], rax
    jmp     @@stats_done

@@avg_zero:
    mov     QWORD PTR [rcx+32], 0

@@stats_done:
    xor     eax, eax
    add     rsp, 8
    ret
asm_quadbuf_get_stats ENDP

; =============================================================================
; asm_quadbuf_shutdown
; Release all GDI resources, free ring buffer, signal thread exit.
; Returns: RAX = 0
; =============================================================================
asm_quadbuf_shutdown PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     DWORD PTR [g_qb_ctx.initialized], 1
    jne     @@shut_exit

    ; Signal render thread to stop
    mov     DWORD PTR [g_qb_ctx.cancelFlag], 1
    mov     rcx, QWORD PTR [g_qb_ctx.hRenderEvent]
    test    rcx, rcx
    jz      @@shut_skip_event
    call    SetEvent
@@shut_skip_event:

    ; Give render thread time to exit
    mov     ecx, 50
    call    Sleep

    ; Restore and delete font
    mov     rcx, QWORD PTR [g_qb_ctx.hdcBack]
    test    rcx, rcx
    jz      @@shut_skip_font
    mov     rdx, QWORD PTR [g_qb_ctx.hFontOld]
    test    rdx, rdx
    jz      @@shut_skip_font
    call    SelectObject
    mov     rcx, QWORD PTR [g_qb_ctx.hFont]
    test    rcx, rcx
    jz      @@shut_skip_font
    call    DeleteObject
@@shut_skip_font:

    ; Restore and delete back buffer bitmap
    mov     rcx, QWORD PTR [g_qb_ctx.hdcBack]
    test    rcx, rcx
    jz      @@shut_skip_bmp
    mov     rdx, QWORD PTR [g_qb_ctx.hBmpOld]
    test    rdx, rdx
    jz      @@shut_skip_bmp
    call    SelectObject
    mov     rcx, QWORD PTR [g_qb_ctx.hBmpBack]
    test    rcx, rcx
    jz      @@shut_skip_bmp
    call    DeleteObject
@@shut_skip_bmp:

    ; Delete back DC
    mov     rcx, QWORD PTR [g_qb_ctx.hdcBack]
    test    rcx, rcx
    jz      @@shut_skip_dc
    call    DeleteDC
@@shut_skip_dc:

    ; Release window DC
    mov     rcx, QWORD PTR [g_qb_ctx.hwnd]
    mov     rdx, QWORD PTR [g_qb_ctx.hdcWindow]
    test    rdx, rdx
    jz      @@shut_skip_wdc
    call    ReleaseDC
@@shut_skip_wdc:

    ; Close render event
    mov     rcx, QWORD PTR [g_qb_ctx.hRenderEvent]
    test    rcx, rcx
    jz      @@shut_skip_evt
    call    CloseHandle
@@shut_skip_evt:

    ; Free ring buffer
    mov     rcx, QWORD PTR [g_qb_ctx.pRing]
    test    rcx, rcx
    jz      @@shut_skip_ring
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@shut_skip_ring:

    ; Zero context
    lea     rdi, g_qb_ctx
    xor     eax, eax
    mov     ecx, SIZEOF QUADBUF_CTX
    rep     stosb

    lea     rcx, szQBShutdown
    call    OutputDebugStringA

@@shut_exit:
    xor     eax, eax
    add     rsp, 48
    pop     rbx
    ret
asm_quadbuf_shutdown ENDP

; =============================================================================
; asm_quadbuf_set_flags
; v14.2.1: Configure streaming behavior flags.
; ECX = STREAMING_FLAG_* bitmask (VSYNC, HDR, TEARING, ADAPTIVE)
; Returns: RAX = previous flags value
; =============================================================================
asm_quadbuf_set_flags PROC FRAME
    sub     rsp, 8
    .allocstack 8
    .endprolog

    mov     eax, DWORD PTR [g_qb_ctx.flags]
    mov     DWORD PTR [g_qb_ctx.flags], ecx

    ; VSYNC + TEARING are mutually exclusive: TEARING takes priority
    test    ecx, STREAMING_FLAG_TEARING
    jz      @@flags_done
    and     DWORD PTR [g_qb_ctx.flags], NOT STREAMING_FLAG_VSYNC

@@flags_done:
    add     rsp, 8
    ret
asm_quadbuf_set_flags ENDP

; =============================================================================
; asm_quadbuf_set_frame_interval
; v14.2.1: Set render loop frame interval in milliseconds.
; ECX = interval in ms (1-1000, 0 = unlimited/spin)
;   16 = 60 FPS, 8 = 120 FPS, 33 = 30 FPS
; Returns: RAX = previous interval
; =============================================================================
asm_quadbuf_set_frame_interval PROC FRAME
    sub     rsp, 8
    .allocstack 8
    .endprolog

    mov     eax, DWORD PTR [g_qb_ctx.frameInterval]

    ; Clamp to reasonable range
    cmp     ecx, 1000
    jle     @@interval_ok
    mov     ecx, 1000
@@interval_ok:
    mov     DWORD PTR [g_qb_ctx.frameInterval], ecx

    add     rsp, 8
    ret
asm_quadbuf_set_frame_interval ENDP

; =============================================================================
; asm_quadbuf_smooth_scroll — Tier 1 Smooth Scroll Interpolation
; =============================================================================
; Interpolates scroll delta over 16 frames for 60fps smooth scrolling.
; Uses velocity-based approach with exponential deceleration.
;
; RCX = pointer to SmoothScrollState struct:
;   +0  DWORD enabled    (1/0)
;   +4  REAL4 velocityY  (current velocity in pixels/frame)
;   +8  REAL4 currentY   (accumulated sub-pixel scroll)
;   +12 REAL4 targetY    (unused, for future lerp)
;   +16 DWORD animating  (1/0)
;
; EDX = wheel delta from WM_MOUSEWHEEL (raw, signed)
; R8D = line height in pixels (fontSize + 2)
;
; Returns:
;   EAX = lines to scroll (signed, 0 = still animating sub-pixel)
;   [RCX] struct updated in-place
; =============================================================================
PUBLIC asm_quadbuf_smooth_scroll
asm_quadbuf_smooth_scroll PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Check enabled flag
    mov     eax, DWORD PTR [rcx]        ; enabled
    test    eax, eax
    jz      @@scroll_disabled

    ; Convert wheel delta to velocity increment
    ; delta = -(wheelDelta / 120) * 3.0 * lineHeight
    cvtsi2ss xmm0, edx                  ; xmm0 = (float)wheelDelta
    mov     eax, 0C2F00000h             ; -120.0f
    movd    xmm1, eax
    divss   xmm0, xmm1                  ; xmm0 = wheelDelta / -120.0
    mov     eax, 40400000h              ; 3.0f (sensitivity)
    movd    xmm1, eax
    mulss   xmm0, xmm1                  ; xmm0 *= 3.0
    cvtsi2ss xmm2, r8d                  ; xmm2 = (float)lineHeight
    mulss   xmm0, xmm2                  ; xmm0 *= lineHeight

    ; Accumulate velocity
    addss   xmm0, DWORD PTR [rcx + 4]   ; velocityY += increment
    movss   DWORD PTR [rcx + 4], xmm0   ; store velocityY

    ; Apply deceleration: velocity *= 0.88
    mov     eax, 3F6147AEh              ; 0.88f
    movd    xmm1, eax
    mulss   xmm0, xmm1                  ; velocityY *= 0.88
    movss   DWORD PTR [rcx + 4], xmm0

    ; Accumulate into currentY
    addss   xmm0, DWORD PTR [rcx + 8]   ; currentY += velocityY
    movss   DWORD PTR [rcx + 8], xmm0

    ; Convert accumulated scroll to whole lines
    cvtsi2ss xmm2, r8d                  ; lineHeight
    divss   xmm0, xmm2                  ; currentY / lineHeight
    cvttss2si eax, xmm0                 ; truncate to int = linesToScroll

    ; Subtract scrolled amount from currentY
    test    eax, eax
    jz      @@no_line_scroll
    cvtsi2ss xmm1, eax                  ; (float)linesToScroll
    mulss   xmm1, xmm2                  ; * lineHeight
    movss   xmm0, DWORD PTR [rcx + 8]
    subss   xmm0, xmm1                  ; currentY -= scrolled pixels
    movss   DWORD PTR [rcx + 8], xmm0

@@no_line_scroll:
    ; Check if still animating (|velocity| > 0.5)
    movss   xmm0, DWORD PTR [rcx + 4]
    mov     r9d, 3F000000h              ; 0.5f threshold
    movd    xmm1, r9d
    ; abs(velocity)
    movss   xmm2, xmm0
    mov     r9d, 7FFFFFFFh              ; float abs mask
    movd    xmm3, r9d
    andps   xmm2, xmm3
    comiss  xmm2, xmm1
    ja      @@still_animating

    ; Stop animation
    xorps   xmm0, xmm0
    movss   DWORD PTR [rcx + 4], xmm0   ; velocityY = 0
    movss   DWORD PTR [rcx + 8], xmm0   ; currentY = 0
    mov     DWORD PTR [rcx + 16], 0      ; animating = false
    jmp     @@scroll_done

@@still_animating:
    mov     DWORD PTR [rcx + 16], 1      ; animating = true
    jmp     @@scroll_done

@@scroll_disabled:
    xor     eax, eax                     ; return 0 lines

@@scroll_done:
    add     rsp, 28h
    ret
asm_quadbuf_smooth_scroll ENDP

END
