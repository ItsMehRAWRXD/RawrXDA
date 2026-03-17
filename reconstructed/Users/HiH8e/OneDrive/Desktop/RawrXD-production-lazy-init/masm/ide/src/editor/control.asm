;==============================================================================
; RawrXD Custom Editor Control - Pure MASM Implementation
; Monaco-like features: Syntax highlighting, Multi-cursor, Code folding
;==============================================================================

; External references from other phases
; (Symbols are available via includes in rawrxd_ide_main.asm)

;==============================================================================
; CONSTANTS
;==============================================================================
LINE_HEIGHT         equ 16
CHAR_WIDTH          equ 8
EDITOR_BG_COLOR     equ 001E1E1Eh  ; VS Code Dark
EDITOR_TEXT_COLOR   equ 00D4D4D4h
CURSOR_COLOR        equ 00FFFFFFh
SELECTION_COLOR     equ 00264F78h

;==============================================================================
; DATA
;==============================================================================
.data
szEditorClass       db 'RawrXDEditor',0
hEditorFont         dd ?

.data?
hMainEditorWnd      dd ?
scrollPos           POINT <>
caretPos            POINT <>
selectionStart      POINT <>
isSelecting         dd ?

.code

;==============================================================================
; Editor Window Procedure
;==============================================================================
EditorWndProc proc uses ebx esi edi hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    local ps:PAINTSTRUCT
    local hdc:HDC
    local rect:RECT
    
    .if uMsg == WM_CREATE
        invoke CreateFont, 14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, \
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, \
                         DEFAULT_QUALITY, FIXED_PITCH or FF_MODERN, CSTR("Consolas")
        mov hEditorFont, eax
        mov eax, 0
        ret
        
    .elseif uMsg == WM_PAINT
        invoke BeginPaint, hWnd, addr ps
        mov hdc, eax
        invoke DrawEditor, hdc
        invoke EndPaint, hWnd, addr ps
        mov eax, 0
        ret
        
    .elseif uMsg == WM_KEYDOWN
        invoke HandleKeyDown, hWnd, wParam, lParam
        mov eax, 0
        ret
        
    .elseif uMsg == WM_CHAR
        invoke HandleChar, hWnd, wParam, lParam
        mov eax, 0
        ret
        
    .elseif uMsg == WM_LBUTTONDOWN
        invoke HandleLButtonDown, hWnd, wParam, lParam
        mov eax, 0
        ret
        
    .elseif uMsg == WM_MOUSEMOVE
        invoke HandleMouseMove, hWnd, wParam, lParam
        mov eax, 0
        ret
        
    .elseif uMsg == WM_LBUTTONUP
        invoke HandleLButtonUp, hWnd, wParam, lParam
        mov eax, 0
        ret
        
    .elseif uMsg == WM_SETFOCUS
        invoke CreateCaret, hWnd, NULL, 2, LINE_HEIGHT
        invoke ShowCaret, hWnd
        invoke UpdateCaretPos, hWnd
        mov eax, 0
        ret
        
    .elseif uMsg == WM_KILLFOCUS
        invoke HideCaret, hWnd
        invoke DestroyCaret
        mov eax, 0
        ret
        
    .endif
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
EditorWndProc endp

;==============================================================================
; Rendering Engine
;==============================================================================
DrawEditor proc uses ebx esi edi hdc:HDC
    local rect:RECT
    local line:DWORD
    local yPos:DWORD
    local hOldFont:HFONT
    
    invoke GetClientRect, hMainEditorWnd, addr rect
    
    ; Fill background
    invoke CreateSolidBrush, EDITOR_BG_COLOR
    push eax
    invoke FillRect, hdc, addr rect, eax
    pop eax
    invoke DeleteObject, eax
    
    invoke SelectObject, hdc, hEditorFont
    mov hOldFont, eax
    
    invoke SetBkMode, hdc, TRANSPARENT
    
    mov line, 0
    mov yPos, 0
    
    .while line < totalLines && yPos < rect.bottom
        invoke DrawLine, hdc, line, yPos
        add yPos, LINE_HEIGHT
        inc line
    .endw
    
    invoke SelectObject, hdc, hOldFont
    ret
DrawEditor endp

DrawLine proc uses ebx esi edi hdc:HDC, line:DWORD, yPos:DWORD
    local lineText:DWORD
    local lineLen:DWORD
    local xPos:DWORD
    local lineStartOffset:DWORD
    
    ; Get line info from lineBuffer
    mov eax, line
    mov ebx, 20 ; SIZEOF LINE_INFO
    mul ebx
    lea esi, lineBuffer
    add esi, eax
    assume esi:ptr LINE_INFO
    
    mov eax, [esi].nOffset
    mov lineStartOffset, eax
    lea ebx, textBuffer
    add eax, ebx
    mov lineText, eax
    mov eax, [esi].nLen
    mov lineLen, eax
    
    ; Syntax highlighting using tokens from Phase 2
    extern tokens:DWORD
    extern tokenCount:DWORD
    
    mov xPos, 0
    mov edi, 0 ; token index
    
    .while edi < tokenCount
        mov eax, edi
        mov ebx, 12 ; SIZEOF TOKEN
        mul ebx
        lea esi, tokens
        add esi, eax
        assume esi:ptr TOKEN
        
        ; Check if token is on this line
        mov eax, [esi].startPos
        mov ebx, lineStartOffset
        .if eax >= ebx
            add ebx, lineLen
            .if eax < ebx
                ; Token is on this line
                ; Set color based on tokenType
                mov eax, [esi].tokenType
                .if eax == TOKEN_KEYWORD
                    invoke SetTextColor, hdc, 00569CD6h ; Blue
                .elseif eax == TOKEN_INSTRUCTION
                    invoke SetTextColor, hdc, 00DCDCAAh ; Yellow
                .elseif eax == TOKEN_REGISTER
                    invoke SetTextColor, hdc, 009CDCFEh ; Light Blue
                .elseif eax == TOKEN_DIRECTIVE
                    invoke SetTextColor, hdc, 00C586C0h ; Purple
                .elseif eax == TOKEN_STRING
                    invoke SetTextColor, hdc, 00CE9178h ; Orange
                .elseif eax == TOKEN_COMMENT
                    invoke SetTextColor, hdc, 006A9955h ; Green
                .else
                    invoke SetTextColor, hdc, EDITOR_TEXT_COLOR
                .endif
                
                ; Calculate x position
                mov eax, [esi].startPos
                sub eax, lineStartOffset
                imul eax, CHAR_WIDTH
                mov xPos, eax
                
                ; Draw token text
                mov eax, [esi].startPos
                lea ebx, textBuffer
                add eax, ebx
                invoke TextOut, hdc, xPos, yPos, eax, [esi].nLen
            .endif
        .endif
        inc edi
    .endw
    
    ret
DrawLine endp

;==============================================================================
; Input Handling
;==============================================================================
HandleKeyDown proc hWnd:DWORD, vKey:DWORD, lParam:DWORD
    .if vKey == VK_LEFT
        ; Move caret left
    .elseif vKey == VK_RIGHT
        ; Move caret right
    .elseif vKey == VK_UP
        ; Move caret up
    .elseif vKey == VK_DOWN
        ; Move caret down
    .elseif vKey == VK_BACK
        ; Backspace
    .elseif vKey == VK_DELETE
        ; Delete
    .elseif vKey == VK_RETURN
        ; New line
    .endif
    
    invoke UpdateCaretPos, hWnd
    invoke InvalidateRect, hWnd, NULL, TRUE
    ret
HandleKeyDown endp

HandleChar proc hWnd:DWORD, char:DWORD, lParam:DWORD
    ; Insert character into textBuffer
    ; Update lineBuffer
    
    ; Trigger LSP analysis
    invoke AnalyzeDocument, NULL, addr textBuffer, textLength
    
    ; Update enhancements
    extern hLineNumberWnd:DWORD
    extern hMinimapWnd:DWORD
    invoke InvalidateRect, hLineNumberWnd, NULL, TRUE
    invoke InvalidateRect, hMinimapWnd, NULL, TRUE
    
    invoke InvalidateRect, hWnd, NULL, TRUE
    ret
HandleChar endp

HandleLButtonDown proc hWnd:DWORD, wParam:DWORD, lParam:DWORD
    ; Set caret position based on mouse coordinates
    ret
HandleLButtonDown endp

HandleMouseMove proc hWnd:DWORD, wParam:DWORD, lParam:DWORD
    ; Handle selection
    ret
HandleMouseMove endp

HandleLButtonUp proc hWnd:DWORD, wParam:DWORD, lParam:DWORD
    ret
HandleLButtonUp endp

UpdateCaretPos proc hWnd:DWORD
    local x:DWORD
    local y:DWORD
    local cursorIndex:DWORD
    
    mov cursorIndex, 0
    .while cursorIndex < activeCursors
        mov eax, cursorIndex
        mov ebx, 20 ; SIZEOF TEXT_CURSOR
        mul ebx
        lea esi, cursors
        add esi, eax
        assume esi:ptr TEXT_CURSOR
        
        .if [esi].active == 1
            ; Calculate screen coordinates
            mov eax, [esi].column
            imul eax, CHAR_WIDTH
            mov x, eax
            
            mov eax, [esi].line
            imul eax, LINE_HEIGHT
            mov y, eax
            
            ; For now, only the primary cursor gets the Windows caret
            mov eax, cursorIndex
            .if eax == primaryCursor
                invoke SetCaretPos, x, y
            .endif
        .endif
        inc cursorIndex
    .endw
    ret
UpdateCaretPos endp

;==============================================================================
; Initialization
;==============================================================================
InitEditorControl proc uses ebx esi edi hInst:HINSTANCE
    local wc:WNDCLASSEX
    
    mov wc.cbSize, SIZEOF WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW or CS_DBLCLKS
    mov wc.lpfnWndProc, offset EditorWndProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov eax, hInst
    mov wc.hInstance, eax
    mov wc.hIcon, NULL
    invoke LoadCursor, NULL, IDC_IBEAM
    mov wc.hCursor, eax
    mov wc.hbrBackground, NULL
    mov wc.lpszMenuName, NULL
    mov wc.lpszClassName, offset szEditorClass
    mov wc.hIconSm, NULL
    
    invoke RegisterClassEx, addr wc
    ret
InitEditorControl endp

end
