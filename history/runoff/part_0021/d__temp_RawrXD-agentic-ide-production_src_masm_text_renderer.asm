;==============================================================================
; File 26: text_renderer.asm - DirectWrite + Direct2D Render Pipeline
;==============================================================================
; Double-buffered offscreen rendering with efficient text measurement
;==============================================================================

include windows.inc

.code

;==============================================================================
; Initialize Renderer
;==============================================================================
Renderer_Init PROC hWnd:QWORD, width:DWORD, height:DWORD
    LOCAL pFactory:QWORD
    LOCAL pRenderTarget:QWORD
    LOCAL pTextFormat:QWORD
    
    ; Create Direct2D factory
    ; TODO: Use D2D1CreateFactory (requires D2D1.lib)
    ; For now, stub with GDI fallback
    
    mov [rendererHWnd], hWnd
    mov [rendererWidth], width
    mov [rendererHeight], height
    
    ; Create offscreen buffer (for double buffering)
    invoke CreateCompatibleDC, NULL
    mov [hOffscreenDC], rax
    .if rax == NULL
        LOG_ERROR "Failed to create offscreen DC"
        ret
    .endif
    
    ; Create compatible bitmap
    invoke CreateCompatibleBitmap, 
        NULL, width, height
    mov [hOffscreenBitmap], rax
    .if rax == NULL
        LOG_ERROR "Failed to create offscreen bitmap"
        ret
    .endif
    
    invoke SelectObject, [hOffscreenDC], [hOffscreenBitmap]
    
    ; Initialize text metrics
    mov [charWidth], 8      ; Monospace font width (pixels)
    mov [charHeight], 16    ; Monospace font height (pixels)
    
    LOG_INFO "Renderer initialized: %dx%d", width, height
    ret
Renderer_Init ENDP

;==============================================================================
; Begin Drawing Frame
;==============================================================================
Renderer_BeginDraw PROC
    ; Clear offscreen buffer
    invoke CreateSolidBrush, 0x00000000  ; Black background
    mov [hBgBrush], rax
    
    invoke FillRect, [hOffscreenDC], 
        ADDR renderRect, [hBgBrush]
    
    invoke DeleteObject, [hBgBrush]
    
    ret
Renderer_BeginDraw ENDP

;==============================================================================
; Draw Line with Token Styling
;==============================================================================
Renderer_DrawLine PROC lineNum:DWORD, lpLineText:QWORD, textLen:DWORD,
                      lpTokens:QWORD, tokenCount:DWORD
    LOCAL y:DWORD
    LOCAL x:DWORD
    LOCAL i:DWORD
    LOCAL token:QWORD
    LOCAL tokenStart:DWORD
    LOCAL tokenLen:DWORD
    LOCAL tokenType:DWORD
    LOCAL color:DWORD
    LOCAL hBrush:QWORD
    
    ; Calculate Y position
    mov eax, lineNum
    mov ecx, [charHeight]
    mul ecx
    mov y, eax
    
    mov x, 0
    mov i, 0
    
.loop1:
    .if i >= tokenCount
        .break
    .endif
    
    mov rax, lpTokens
    mov token, [rax + i * SIZEOF Token]
    
    mov eax, [token].Token.startOffset
    mov tokenStart, eax
    mov eax, [token].Token.length
    mov tokenLen, eax
    mov eax, [token].Token.typeId
    mov tokenType, eax
    
    ; Get color for token type
    call Renderer_GetTokenColor, tokenType
    mov color, eax
    
    ; Draw token
    invoke CreateSolidBrush, color
    mov hBrush, rax
    
    ; Draw text at position (x, y)
    invoke SetTextColor, [hOffscreenDC], color
    invoke SetBkMode, [hOffscreenDC], 0  ; TRANSPARENT
    
    invoke TextOutA, [hOffscreenDC], 
        x, y,
        QWORD PTR lpLineText + tokenStart,
        tokenLen
    
    invoke DeleteObject, hBrush
    
    ; Advance X position
    mov eax, tokenLen
    mov ecx, [charWidth]
    mul ecx
    add x, eax
    
    inc i
    .continue .loop1
.endloop1
    
    ret
Renderer_DrawLine ENDP

;==============================================================================
; Draw Gutter (Line Numbers + Breakpoints)
;==============================================================================
Renderer_DrawGutter PROC startLine:DWORD, lineCount:DWORD
    LOCAL i:DWORD
    LOCAL y:DWORD
    LOCAL lineNum:DWORD
    LOCAL numBuffer:db 16 dup(?)
    
    ; Draw line number column background
    invoke CreateSolidBrush, 0x00404040  ; Dark gray
    invoke FillRect, [hOffscreenDC], 
        ADDR gutterRect, rax
    invoke DeleteObject, rax
    
    mov i, 0
.loop1:
    .if i >= lineCount
        .break
    .endif
    
    mov eax, startLine
    add eax, i
    mov lineNum, eax
    
    ; Calculate Y position
    mov eax, i
    mov ecx, [charHeight]
    mul ecx
    mov y, eax
    
    ; Format line number
    invoke wsprintf, ADDR numBuffer, OFFSET szLineNumFormat, lineNum
    
    ; Draw line number
    invoke SetTextColor, [hOffscreenDC], 0x00808080  ; Gray text
    invoke TextOutA, [hOffscreenDC], 5, y, 
        ADDR numBuffer, eax
    
    inc i
    .continue .loop1
.endloop1
    
    ret
Renderer_DrawGutter ENDP

;==============================================================================
; Draw Selections
;==============================================================================
Renderer_DrawSelections PROC startPos:QWORD, endPos:QWORD
    LOCAL startX:DWORD
    LOCAL startY:DWORD
    LOCAL endX:DWORD
    LOCAL endY:DWORD
    LOCAL hSelBrush:QWORD
    
    ; Convert positions to screen coordinates
    call Renderer_PosToCoords, startPos
    mov startX, eax
    shr rax, 32
    mov startY, eax
    
    call Renderer_PosToCoords, endPos
    mov endX, eax
    shr rax, 32
    mov endY, eax
    
    ; Draw selection highlight
    invoke CreateSolidBrush, 0x00004080  ; Blue
    mov hSelBrush, rax
    
    invoke PatBlt, [hOffscreenDC], 
        startX, startY,
        endX - startX, 
        [charHeight],
        0x00A000C9  ; PATINVERT
    
    invoke DeleteObject, hSelBrush
    
    ret
Renderer_DrawSelections ENDP

;==============================================================================
; Draw Search Matches
;==============================================================================
Renderer_DrawSearchMatches PROC lpMatches:QWORD, matchCount:DWORD
    LOCAL i:DWORD
    LOCAL pos:QWORD
    LOCAL x:DWORD
    LOCAL y:DWORD
    LOCAL hMatchBrush:QWORD
    
    invoke CreateSolidBrush, 0x0000FFFF  ; Yellow
    mov hMatchBrush, rax
    
    mov i, 0
.loop1:
    .if i >= matchCount
        .break
    .endif
    
    mov rax, lpMatches
    mov pos, [rax + i * 8]
    
    ; Convert position to coordinates
    call Renderer_PosToCoords, pos
    mov x, eax
    shr rax, 32
    mov y, eax
    
    ; Draw highlight
    invoke PatBlt, [hOffscreenDC],
        x, y,
        [charWidth] * 4,  ; Assume 4-char search
        [charHeight],
        0x00A000C9  ; PATINVERT
    
    inc i
    .continue .loop1
.endloop1
    
    invoke DeleteObject, hMatchBrush
    
    ret
Renderer_DrawSearchMatches ENDP

;==============================================================================
; Draw Diagnostics (Error Squiggles)
;==============================================================================
Renderer_DrawDiagnostics PROC lpDiagnostics:QWORD, diagCount:DWORD
    LOCAL i:DWORD
    LOCAL diag:QWORD
    LOCAL x:DWORD
    LOCAL y:DWORD
    LOCAL endX:DWORD
    LOCAL hPen:QWORD
    LOCAL color:DWORD
    
    mov i, 0
.loop1:
    .if i >= diagCount
        .break
    .endif
    
    mov rax, lpDiagnostics
    mov diag, [rax + i * SIZEOF Diagnostic]
    
    ; Get color based on severity
    mov eax, [diag].Diagnostic.severity
    .if eax == 1
        mov color, 0x000000FF  ; Red (error)
    .elseif eax == 2
        mov color, 0x0000FFFF  ; Orange (warning)
    .else
        mov color, 0x0000FF00  ; Green (info)
    .endif
    
    ; Create wavy underline (approximation with small dashes)
    invoke CreatePen, 0, 1, color
    mov hPen, rax
    invoke SelectObject, [hOffscreenDC], hPen
    
    ; Calculate position
    mov eax, [diag].Diagnostic.column
    mov ecx, [charWidth]
    mul ecx
    mov x, eax
    
    mov eax, [diag].Diagnostic.line
    mov ecx, [charHeight]
    mul ecx
    add eax, [charHeight]
    mov y, eax
    
    mov eax, [diag].Diagnostic.endColumn
    mov ecx, [charWidth]
    mul ecx
    mov endX, eax
    
    ; Draw underline
    invoke MoveToEx, [hOffscreenDC], x, y, NULL
    invoke LineTo, [hOffscreenDC], endX, y
    
    invoke DeleteObject, hPen
    
    inc i
    .continue .loop1
.endloop1
    
    ret
Renderer_DrawDiagnostics ENDP

;==============================================================================
; End Drawing & Present
;==============================================================================
Renderer_EndDraw PROC hDestDC:QWORD
    ; Blit offscreen buffer to screen
    invoke BitBlt, hDestDC, 0, 0, 
        [rendererWidth], [rendererHeight],
        [hOffscreenDC], 0, 0,
        0x00CC0020  ; SRCCOPY
    
    ret
Renderer_EndDraw ENDP

;==============================================================================
; Get Character Metrics (Width, Height)
;==============================================================================
Renderer_GetCharMetrics PROC lpOutWidth:QWORD, lpOutHeight:QWORD
    mov rax, [charWidth]
    mov [lpOutWidth], eax
    
    mov rax, [charHeight]
    mov [lpOutHeight], eax
    
    ret
Renderer_GetCharMetrics ENDP

;==============================================================================
; Get Token Color by Type
;==============================================================================
Renderer_GetTokenColor PROC tokenType:DWORD
    mov eax, tokenType
    
    .if eax == 1
        mov eax, 0x00C586C0  ; Purple (keyword)
    .elseif eax == 2
        mov eax, 0x00CE9178  ; Orange (string)
    .elseif eax == 3
        mov eax, 0x006A9955  ; Green (comment)
    .elseif eax == 4
        mov eax, 0x00D4D4D4  ; Light gray (identifier)
    .elseif eax == 5
        mov eax, 0x00B5CEA8  ; Light green (number)
    .else
        mov eax, 0x00D4D4D4  ; Light gray (default)
    .endif
    
    ret
Renderer_GetTokenColor ENDP

;==============================================================================
; Convert Buffer Position to Screen Coordinates
;==============================================================================
Renderer_PosToCoords PROC pos:QWORD
    LOCAL lineNum:QWORD
    LOCAL colNum:QWORD
    LOCAL x:DWORD
    LOCAL y:DWORD
    
    ; TODO: Get actual line/column from buffer
    ; For now, simple calculation
    
    xor rdx, rdx
    mov rax, pos
    mov rcx, 80
    div rcx
    mov lineNum, rax
    mov colNum, rdx
    
    mov eax, [charWidth]
    mov ecx, 64  ; Gutter width
    add eax, ecx
    mov ecx, colNum
    imul eax, ecx
    mov x, eax
    
    mov eax, [charHeight]
    mov ecx, lineNum
    imul eax, ecx
    mov y, eax
    
    mov eax, x
    mov edx, y
    shl rdx, 32
    or rax, rdx
    
    ret
Renderer_PosToCoords ENDP

;==============================================================================
; Data
;==============================================================================
.data
rendererHWnd        dq ?
rendererWidth       dd 0
rendererHeight      dd 0
hOffscreenDC        dq ?
hOffscreenBitmap    dq ?
hBgBrush            dq ?
charWidth           dd 8
charHeight          dd 16

renderRect          RECT <>
gutterRect          RECT <>

szLineNumFormat     db '%6d',0

; Diagnostic structure
Diagnostic STRUCT
    line            DWORD ?
    column          DWORD ?
    message         QWORD ?
    severity        DWORD ?
    endLine         DWORD ?
    endColumn       DWORD ?
Diagnostic ENDS

END
