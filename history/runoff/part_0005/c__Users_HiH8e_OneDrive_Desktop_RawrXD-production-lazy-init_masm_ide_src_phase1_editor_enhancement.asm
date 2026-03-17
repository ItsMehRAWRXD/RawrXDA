;==============================================================================
; RawrXD Editor Enhancement Engine - Pure MASM Implementation
; Phase 1: Line numbers, minimap, bracket matching, code folding, multi-cursor
; Version: 2.0.0 Enterprise
; Zero External Dependencies - 100% Pure MASM
;==============================================================================

;==============================================================================
; CONSTANTS
;==============================================================================
MAX_LINE_LENGTH     equ 512
MAX_LINES           equ 10000
MAX_FOLD_LEVELS     equ 32
MAX_CURSORS         equ 16
MINIMAP_WIDTH       equ 120
BRACKET_TYPES       equ 6
MAX_TABS            equ 16
MAX_COMMANDS        equ 256
MAX_KEYBINDINGS     equ 128

; Token types for syntax highlighting
TOKEN_KEYWORD       equ 1
TOKEN_REGISTER      equ 2
TOKEN_INSTRUCTION   equ 3
TOKEN_DIRECTIVE     equ 4
TOKEN_STRING        equ 5
TOKEN_COMMENT       equ 6
TOKEN_NUMBER        equ 7
TOKEN_IDENTIFIER    equ 8
TOKEN_OPERATOR      equ 9

; Completion contexts
COMPLETE_INSTRUCTION equ 1
COMPLETE_REGISTER    equ 2
COMPLETE_DIRECTIVE   equ 3
COMPLETE_SYMBOL      equ 4
COMPLETE_LABEL       equ 5

;==============================================================================
; MASM32 compatibility helpers
;==============================================================================

; Inline C-string literal for INVOKE without switching segments.
; NOTE: Expands into the current code stream; use inside procedures.
CSTR MACRO text:VARARG
    LOCAL __skip, __str
    jmp __skip
    __str db text,0
__skip:
    EXITM <OFFSET __str>
ENDM

;==============================================================================
; STRUCTURES
;==============================================================================
TEXT_CURSOR struct
    line        dd ?
    column      dd ?
    anchorLine  dd ?
    anchorCol   dd ?
    active      dd ?
TEXT_CURSOR ends

LINE_INFO struct
    nLen        dd ?
    foldLevel   dd ?
    foldState   dd ?    ; 0=expanded, 1=collapsed
    visible     dd ?
    nOffset     dd ?    ; offset in text buffer
LINE_INFO ends

TOKEN struct
    startPos    dd ?
    nLen        dd ?
    tokenType   dd ?
TOKEN ends

FOLD_REGION struct
    startLine   dd ?
    endLine     dd ?
    level       dd ?
    collapsed   dd ?
FOLD_REGION ends

MINIMAP_LINE struct
    yPos        dd ?
    hash        dd ?    ; simple hash for line content
    nLen        dd ?
MINIMAP_LINE ends

TAB_CONTROL struct
    hWnd        dd ?
    filePath    db MAX_PATH dup(?)
    modified    dd ?
    lineCount   dd ?
    cursorPos   POINT <>
TAB_CONTROL ends

COMMAND_ITEM struct
    szName      db 64 dup(?)
    szCategory  db 32 dup(?)
    szHotkey    db 16 dup(?)
    commandId   dd ?
    enabled     dd ?
COMMAND_ITEM ends

KEY_BINDING struct
    modifiers   dd ?    ; Ctrl, Alt, Shift flags
    vkey        dd ?    ; Virtual key code
    commandId   dd ?
    description db 64 dup(?)
KEY_BINDING ends

SPLIT_PANE struct
    hWnd        dd ?
    splitType   dd ?    ; 0=horizontal, 1=vertical
    splitPos    dd ?
    minSize     dd ?
    maxSize     dd ?
    leftPane    dd ?
    rightPane   dd ?
    isSplit     dd ?
SPLIT_PANE ends

CONTEXT_MENU struct
    hMenu       dd ?
    itemCount   dd ?
    items       dd 64 dup(?)
CONTEXT_MENU ends

;==============================================================================
; DATA SEGMENT
;==============================================================================
.data
; MASM keywords for syntax highlighting
masmKeywords \
    db 'mov',0, 'add',0, 'sub',0, 'mul',0, 'div',0, 'inc',0, 'dec',0
    db 'push',0, 'pop',0, 'call',0, 'ret',0, 'jmp',0, 'jz',0, 'jnz',0
    db 'cmp',0, 'test',0, 'lea',0, 'and',0, 'or',0, 'xor',0, 'not',0
    db 'shl',0, 'shr',0, 'sal',0, 'sar',0, 'rol',0, 'ror',0, 'rcl',0, 'rcr',0
    db 0

masmDirectives \
    db '.data',0, '.code',0, '.stack',0, '.data?',0, '.const',0
    db '.model',0, '.386',0, '.486',0, '.586',0, '.mmx',0, '.xmm',0
    db 'proc',0, 'endp',0, 'struct',0, 'ends',0, 'equ',0, 'macro',0, 'endm',0
    db 0
masmRegisters \
    db 'eax',0, 'ebx',0, 'ecx',0, 'edx',0, 'esi',0, 'edi',0, 'ebp',0, 'esp',0
    db 'ax',0, 'bx',0, 'cx',0, 'dx',0, 'si',0, 'di',0, 'bp',0, 'sp',0
    db 'al',0, 'bl',0, 'cl',0, 'dl',0, 'ah',0, 'bh',0, 'ch',0, 'dh',0
    db 'r8',0, 'r9',0, 'r10',0, 'r11',0, 'r12',0, 'r13',0, 'r14',0, 'r15',0
    db 0

bracketPairs    db '<>', '()', '{}', '[]', '""', "''"

szLineNumberClass db 'RawrXDLineNumbers',0
szMinimapClass    db 'RawrXDMinimap',0
szSplitPaneClass  db 'RawrXDSplitPane',0
szConsolas        db 'Consolas',0
szStatic          db 'STATIC',0

.data?
hEdit           dd ?
hLineNumberWnd  dd ?
hMinimapWnd     dd ?
hStatusBar      dd ?

; Multi-cursor support
cursors         db (SIZEOF TEXT_CURSOR) * MAX_CURSORS dup(?)
activeCursors   dd ?
primaryCursor   dd ?

; Line management
lineBuffer      db (SIZEOF LINE_INFO) * MAX_LINES dup(?)
totalLines      dd ?
visibleLines    dd ?

; Text buffer
TOTAL_TEXT_SIZE     equ MAX_LINE_LENGTH * MAX_LINES
textBuffer      db TOTAL_TEXT_SIZE dup(?)
textLength      dd ?

; Folding system
foldRegions     db (SIZEOF FOLD_REGION) * MAX_FOLD_LEVELS dup(?)
foldCount       dd ?

; Minimap data
minimapLines    db (SIZEOF MINIMAP_LINE) * MAX_LINES dup(?)
minimapHeight   dd ?

; Bracket matching
bracketStack    dd 256 dup(?)
stackTop        dd ?

; Tab system
tabControls     db (SIZEOF TAB_CONTROL) * MAX_TABS dup(?)
activeTab       dd ?
tabCount        dd ?

; Command palette
commandPalette  db (SIZEOF COMMAND_ITEM) * MAX_COMMANDS dup(?)
commandCount    dd ?
paletteVisible  dd ?
paletteFilter   db 128 dup(?)
paletteSelection dd ?

; Key bindings
keyBindings     db (SIZEOF KEY_BINDING) * MAX_KEYBINDINGS dup(?)
bindingCount    dd ?

; Split panes
splitPanes      db (SIZEOF SPLIT_PANE) * 8 dup(?)
paneCount       dd ?

; Context menus
contextMenus    db (SIZEOF CONTEXT_MENU) * 16 dup(?)
menuCount       dd ?

.code

; Prototypes (required for INVOKE before PROC is encountered)
AddCommand PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
AddCursor PROTO :DWORD, :DWORD

;==============================================================================
; Phase 1.1: Line Numbers Window
;==============================================================================
;==============================================================================
LineNumbersWndProc proc uses ebx esi edi hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    local ps:PAINTSTRUCT
    local rect:RECT
    local hdc:HDC
    local hFont:HFONT
    local hOldFont:HFONT
    local lineNum:DWORD
    local yPos:DWORD
    local buffer[32]:BYTE
    
    .if uMsg == WM_PAINT
        invoke BeginPaint, hWnd, addr ps
        mov hdc, eax
        
        ; Create monospace font
        invoke CreateFont, 14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, \
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, \
                         DEFAULT_QUALITY, FIXED_PITCH or FF_MODERN, addr szConsolas
        mov hFont, eax
        invoke SelectObject, hdc, hFont
        mov hOldFont, eax
        
        ; Get client area
        invoke GetClientRect, hWnd, addr rect
        
        ; Set text color and background
        invoke SetTextColor, hdc, 00808080h  ; Gray text
        invoke SetBkColor, hdc, 00F0F0F0h    ; Light gray background
        invoke SetBkMode, hdc, OPAQUE
        
        ; Fill background
        invoke CreateSolidBrush, 00F0F0F0h
        push eax
        invoke FillRect, hdc, addr rect, eax
        pop eax
        invoke DeleteObject, eax
        
        ; Draw line numbers
        mov lineNum, 1
        mov yPos, 0
        
LineLoop:
        mov eax, lineNum
        cmp eax, totalLines
        jg  LineLoopDone
        mov eax, rect.bottom
        cmp yPos, eax
        jae LineLoopDone

        ; Check if line is visible (not folded)
        mov eax, lineNum
        dec eax
        mov ebx, SIZEOF LINE_INFO
        mul ebx
        lea esi, lineBuffer
        add esi, eax
        assume esi:ptr LINE_INFO
        
        .if [esi].visible == TRUE
            invoke wsprintf, addr buffer, CSTR("%d"), lineNum
            invoke lstrlen, addr buffer
            invoke TextOut, hdc, 5, yPos, addr buffer, eax
            add yPos, 16  ; Line height
        .endif
        
        inc lineNum
        jmp LineLoop
LineLoopDone:
        
        invoke SelectObject, hdc, hOldFont
        invoke DeleteObject, hFont
        invoke EndPaint, hWnd, addr ps
        mov eax, 0
        ret
        
    .elseif uMsg == WM_ERASEBKGND
        mov eax, 1
        ret
        
    .endif
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
LineNumbersWndProc endp

;==============================================================================
; Phase 1.2: Minimap Implementation
;==============================================================================
MinimapWndProc proc uses ebx esi edi hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    local ps:PAINTSTRUCT
    local rect:RECT
    local hdc:HDC
    local hBrush:HBRUSH
    local lineIndex:DWORD
    local yPos:DWORD
    
    .if uMsg == WM_PAINT
        invoke BeginPaint, hWnd, addr ps
        mov hdc, eax
        
        invoke GetClientRect, hWnd, addr rect
        
        ; Fill background
        invoke CreateSolidBrush, 00282828h
        mov hBrush, eax
        invoke FillRect, hdc, addr rect, hBrush
        invoke DeleteObject, hBrush
        
        ; Draw minimap lines
        mov lineIndex, 0
        mov yPos, 0
        
MiniLoop:
        mov eax, lineIndex
        cmp eax, totalLines
        jae MiniLoopDone
        mov eax, rect.bottom
        cmp yPos, eax
        jae MiniLoopDone

        ; Calculate line hash for syntax coloring
        mov eax, lineIndex
        mov ebx, SIZEOF MINIMAP_LINE
        mul ebx
        lea esi, minimapLines
        add esi, eax
        assume esi:ptr MINIMAP_LINE
        
        ; Simple hash-based syntax highlighting
        mov eax, [esi].hash
        and eax, 07Fh
        add eax, 00505050h
        invoke CreateSolidBrush, eax
        push eax
        
        ; Draw line representation
        mov eax, [esi].nLen
        .if eax > MINIMAP_WIDTH
            mov eax, MINIMAP_WIDTH
        .endif
        
        push eax
        invoke Rectangle, hdc, 2, yPos, eax, yPos
        add yPos, 2
        pop eax
        
        pop eax
        invoke DeleteObject, eax
        
        inc lineIndex
        jmp MiniLoop
MiniLoopDone:
        
        invoke EndPaint, hWnd, addr ps
        mov eax, 0
        ret
        
    .elseif uMsg == WM_ERASEBKGND
        mov eax, 1
        ret
        
    .endif
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
MinimapWndProc endp

;==============================================================================
; Phase 1.3: Bracket Matching Engine
;==============================================================================
FindMatchingBracket proc uses ebx esi edi startPos:DWORD, bracketType:DWORD
    local pos:DWORD
    local depth:DWORD
    local char:BYTE
    local openChar:BYTE
    local closeChar:BYTE
    
    ; Get bracket pair characters
    mov eax, bracketType
    shl eax, 1
    lea esi, bracketPairs
    add esi, eax
    mov al, byte ptr [esi]
    mov openChar, al
    mov al, byte ptr [esi+1]
    mov closeChar, al
    
    ; Scan forward for matching bracket
    mov eax, startPos
    mov pos, eax
    mov depth, 1
    
    .while depth > 0
        mov eax, pos
        .if eax >= textLength
            mov eax, -1
            ret
        .endif
        
        lea esi, textBuffer
        mov edx, esi
        add edx, eax
        mov al, byte ptr [edx]
        
        movzx ebx, openChar
        .if al == bl
            inc depth
        .else
            movzx ebx, closeChar
            .if al == bl
                dec depth
                .if depth == 0
                    mov eax, pos
                    ret
                .endif
            .endif
        .endif
        
        inc pos
    .endw
    
    mov eax, -1
    ret
FindMatchingBracket endp

;==============================================================================
; Phase 1.4: Multi-Cursor Support
;==============================================================================
AddCursor proc uses ebx esi edi line:DWORD, column:DWORD
    .if activeCursors < MAX_CURSORS
        mov eax, activeCursors
        mov ebx, SIZEOF TEXT_CURSOR
        imul ebx
        lea edi, cursors
        add edi, eax
        assume edi:ptr TEXT_CURSOR
        
        mov eax, line
        mov [edi].line, eax
        mov eax, column
        mov [edi].column, eax
        mov [edi].anchorLine, eax
        mov [edi].anchorCol, eax
        mov [edi].active, 1
        
        inc activeCursors
        mov eax, activeCursors
        dec eax
        ret
    .else
        mov eax, -1
        ret
    .endif
AddCursor endp

RemoveCursor proc uses ebx esi edi index:DWORD
    local i:DWORD
    
    mov eax, index
    .if eax < activeCursors
        mov ebx, SIZEOF TEXT_CURSOR
        imul ebx
        lea edi, cursors
        add edi, eax
        assume edi:ptr TEXT_CURSOR
        
        mov [edi].active, 0
        
        ; Compact cursor array
        mov eax, index
        mov i, eax
        .while eax < activeCursors
            dec activeCursors
            mov eax, i
            .break .if eax >= activeCursors
            
            ; Move next cursor to current position
            mov eax, i
            inc eax
            mov ebx, SIZEOF TEXT_CURSOR
            imul ebx
            lea esi, cursors
            add esi, eax
            
            mov eax, i
            mov ebx, SIZEOF TEXT_CURSOR
            imul ebx
            lea edi, cursors
            add edi, eax
            
            mov ecx, SIZEOF TEXT_CURSOR
            rep movsb
            
            mov eax, i
            inc eax
            mov i, eax
        .endw
    .endif
    
    ret
RemoveCursor endp

;==============================================================================
; Phase 1.5: Tab System
;==============================================================================
CreateTab proc uses ebx esi edi filePath:DWORD
    local tabIndex:DWORD
    local nameBuffer[MAX_PATH]:BYTE
    
    .if tabCount < MAX_TABS
        mov eax, tabCount
        mov tabIndex, eax
        
        mov ebx, SIZEOF TAB_CONTROL
        imul ebx
        lea edi, tabControls
        add edi, eax
        assume edi:ptr TAB_CONTROL
        
        ; Create tab window
        invoke CreateWindowEx, 0, addr szStatic, NULL, \
                              WS_CHILD or WS_VISIBLE or SS_OWNERDRAW, \
                              0, 0, 100, 25, hEdit, NULL, NULL, NULL
        mov [edi].hWnd, eax
        
        ; Set file path
        .if filePath != NULL
            invoke lstrcpyn, addr [edi].filePath, filePath, MAX_PATH
        .else
            invoke wsprintf, addr nameBuffer, CSTR("Untitled%d"), tabIndex
            invoke lstrcpyn, addr [edi].filePath, addr nameBuffer, MAX_PATH
        .endif
        
        mov [edi].modified, 0
        mov [edi].lineCount, 0
        mov [edi].cursorPos.x, 0
        mov [edi].cursorPos.y, 0
        
        inc tabCount
        mov eax, tabIndex
        mov activeTab, eax
        
        invoke InvalidateRect, [edi].hWnd, NULL, TRUE
        mov eax, tabIndex
        ret
    .else
        mov eax, -1
        ret
    .endif
CreateTab endp

;==============================================================================
; Phase 1.6: Command Palette
;==============================================================================
InitCommandPalette proc uses ebx esi edi
    ; Initialize command count
    mov commandCount, 0
    
    ; File commands
    invoke AddCommand, CSTR("File: New"), CSTR("File"), CSTR("Ctrl+N"), 1001, 1
    invoke AddCommand, CSTR("File: Open"), CSTR("File"), CSTR("Ctrl+O"), 1002, 1
    invoke AddCommand, CSTR("File: Save"), CSTR("File"), CSTR("Ctrl+S"), 1003, 1
    
    ; Edit commands
    invoke AddCommand, CSTR("Edit: Undo"), CSTR("Edit"), CSTR("Ctrl+Z"), 1101, 1
    invoke AddCommand, CSTR("Edit: Redo"), CSTR("Edit"), CSTR("Ctrl+Y"), 1102, 1
    invoke AddCommand, CSTR("Edit: Cut"), CSTR("Edit"), CSTR("Ctrl+X"), 1105, 1
    invoke AddCommand, CSTR("Edit: Copy"), CSTR("Edit"), CSTR("Ctrl+C"), 1106, 1
    invoke AddCommand, CSTR("Edit: Paste"), CSTR("Edit"), CSTR("Ctrl+V"), 1107, 1
    
    ret
InitCommandPalette endp

AddCommand proc uses ebx esi edi pName:DWORD, pCategory:DWORD, pHotkey:DWORD, cmdId:DWORD, enabled:DWORD
    .if commandCount < MAX_COMMANDS
        mov eax, commandCount
        mov ebx, SIZEOF COMMAND_ITEM
        imul ebx
        lea edi, commandPalette
        add edi, eax
        assume edi:ptr COMMAND_ITEM
        
        invoke lstrcpyn, addr [edi].szName, pName, 64
        invoke lstrcpyn, addr [edi].szCategory, pCategory, 32
        invoke lstrcpyn, addr [edi].szHotkey, pHotkey, 16
        mov eax, cmdId
        mov [edi].commandId, eax
        mov eax, enabled
        mov [edi].enabled, eax
        
        inc commandCount
        mov eax, TRUE
        ret
    .else
        mov eax, FALSE
        ret
    .endif
AddCommand endp

;==============================================================================
; Phase 1.7: Initialization and Integration
;==============================================================================
InitEditorEnhancements proc uses ebx esi edi hParent:DWORD
    local wc:WNDCLASSEX
    local rect:RECT
    
    ; Register line numbers window class
    mov wc.cbSize, SIZEOF WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW or CS_DBLCLKS
    mov eax, offset LineNumbersWndProc
    mov wc.lpfnWndProc, eax
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    push NULL
    pop wc.hInstance
    push NULL
    pop wc.hIcon
    push NULL
    pop wc.hCursor
    push NULL
    pop wc.hbrBackground
    push NULL
    pop wc.lpszMenuName
    lea eax, szLineNumberClass
    mov wc.lpszClassName, eax
    push NULL
    pop wc.hIconSm
    invoke RegisterClassEx, addr wc
    
    ; Register minimap window class
    mov eax, offset MinimapWndProc
    mov wc.lpfnWndProc, eax
    lea eax, szMinimapClass
    mov wc.lpszClassName, eax
    invoke RegisterClassEx, addr wc
    
    ; Create enhancement windows
    invoke GetClientRect, hParent, addr rect
    
    ; Line numbers window
    invoke CreateWindowEx, 0, addr szLineNumberClass, NULL, \
                          WS_CHILD or WS_VISIBLE, \
                          0, 0, 50, rect.bottom, hParent, NULL, NULL, NULL
    mov hLineNumberWnd, eax
    
    ; Minimap window
    mov eax, rect.right
    sub eax, MINIMAP_WIDTH
    invoke CreateWindowEx, 0, addr szMinimapClass, NULL, \
                          WS_CHILD or WS_VISIBLE, \
                          eax, 0, MINIMAP_WIDTH, rect.bottom, \
                          hParent, NULL, NULL, NULL
    mov hMinimapWnd, eax
    
    ; Initialize editor state
    mov totalLines, 1
    mov visibleLines, 1
    mov activeCursors, 1
    mov primaryCursor, 0
    mov tabCount, 0
    mov paneCount, 0
    mov commandCount, 0
    mov bindingCount, 0
    mov menuCount, 0
    
    ; Create default cursor
    invoke AddCursor, 0, 0
    
    ; Initialize command palette
    invoke InitCommandPalette
    
    ret
InitEditorEnhancements endp

;==============================================================================
; EXPORTS
;==============================================================================
public InitEditorEnhancements
public AddCursor
public RemoveCursor
public CreateTab
public AddCommand
public LineNumbersWndProc
public MinimapWndProc
public totalLines
public lineBuffer
public textBuffer
public textLength
public cursors
public activeCursors
public primaryCursor

end
