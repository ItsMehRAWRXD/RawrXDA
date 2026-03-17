; ============================================================================
; Optimization Hex UI - Phase 5.5 Reverse Engineering Panel
; Provides a visual hex editor + digest view for binary inspection
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comdlg32.inc
include \masm32\include\gdi32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comdlg32.lib
includelib \masm32\lib\gdi32.lib

HEXUI_BYTES_PER_LINE     equ 16
HEXUI_MAX_BYTES          equ 524288        ; 512 KB safety cap

IDC_HEX_EDIT             equ 6101
IDC_HEX_OPEN             equ 6102
IDC_HEX_APPLY            equ 6103
IDC_HEX_REHASH           equ 6104
IDC_HEX_STATUS           equ 6105
IDC_HEX_DIGEST           equ 6106

.data
    g_hHexWnd            dd 0
    g_hHexEdit           dd 0
    g_hDigestStatic      dd 0
    g_hStatusStatic      dd 0
    g_hBtnOpen           dd 0
    g_hBtnApply          dd 0
    g_hBtnDigest         dd 0
    g_hOwner             dd 0
    g_hInstance          dd 0
    g_hMonoFont          dd 0
    g_bClassRegistered   dd 0

    g_szHexClass         db "RawrXD_HexPanel",0
    g_szWindowTitle      db "Optimization Engine - Hex Inspector",0
    g_szSelectFileTitle  db "Select file to analyze",0
    g_szDigestFmt        db "CRC32 %08X | Bytes %u",0
    g_szStatusFmt        db "Loaded: %s",0
    g_szTruncateNote     db " | truncated to 512 KB",0
    g_szNoFile           db "No file loaded",0
    g_szConsolas         db "Consolas",0
    szStaticClass        db "STATIC",0
    szButtonClass        db "BUTTON",0
    szEditClass          db "EDIT",0
    szBtnOpen            db "Open",0
    szBtnApply           db "Apply",0
    szBtnDigest          db "Digest",0

    g_szCurrentPath      db MAX_PATH dup(0)
    g_szStatusLine       db 256 dup(0)

.data?
    g_ofn                OPENFILENAME <>

.code

HexUi_WndProc PROTO :HWND, :UINT, :WPARAM, :LPARAM
HexUi_LoadFile PROTO :DWORD

; ----------------------------------------------------------------------------
; Helpers
; ----------------------------------------------------------------------------

HexUi_IsHexDigit PROC ch:BYTE
    mov al, ch
    cmp al, '0'
    jb  @not
    cmp al, '9'
    jbe @dec
    cmp al, 'A'
    jb  @lower
    cmp al, 'F'
    jbe @upper
@lower:
    cmp al, 'a'
    jb  @not
    cmp al, 'f'
    jbe @upper
    jmp @not
@dec:
@upper:
    mov eax, 1
    ret
@not:
    xor eax, eax
    ret
HexUi_IsHexDigit ENDP

HexUi_HexToNibble PROC ch:BYTE
    mov al, ch
    cmp al, '0'
    jb  @zero
    cmp al, '9'
    jbe @dec
    cmp al, 'A'
    jb  @lower
    cmp al, 'F'
    jbe @upper
@lower:
    sub al, 'a'
    add al, 10
    ret
@upper:
    sub al, 'A'
    add al, 10
    ret
@dec:
    sub al, '0'
    ret
@zero:
    xor eax, eax
    ret
HexUi_HexToNibble ENDP

HexUi_NibbleToChar PROC nibbleVal:DWORD
    mov al, BYTE PTR nibbleVal
    cmp al, 9
    jbe @digit
    add al, 55             ; 'A' - 10
    ret
@digit:
    add al, '0'
    ret
HexUi_NibbleToChar ENDP

HexUi_ByteToHex PROC uses eax ebx edx byteVal:DWORD, pOut:DWORD
    mov bl, BYTE PTR byteVal
    mov al, bl
    shr al, 4
    push eax
    call HexUi_NibbleToChar
    mov dl, al
    mov al, bl
    and al, 0Fh
    push eax
    call HexUi_NibbleToChar
    mov bl, al
    mov edi, pOut
    mov [edi], dl
    mov [edi+1], bl
    ret
HexUi_ByteToHex ENDP

HexUi_ComputeCrc32 PROC uses esi ebx ecx edx pData:DWORD, dwLen:DWORD
    mov esi, pData
    mov ecx, dwLen
    mov eax, 0FFFFFFFFh
@nextByte:
    cmp ecx, 0
    je  @done
    mov bl, [esi]
    inc esi
    xor al, bl
    mov edx, 8
@bitLoop:
    shr eax, 1
    jnc @skipPoly
    xor eax, 0EDB88320h
@skipPoly:
    dec edx
    jnz @bitLoop
    dec ecx
    jmp @nextByte
@done:
    not eax
    ret
HexUi_ComputeCrc32 ENDP

; ----------------------------------------------------------------------------
; Formatting
; ----------------------------------------------------------------------------

HexUi_FormatBuffer PROC uses esi edi ebx ecx edx pData:DWORD, dwLen:DWORD, pOut:DWORD
    LOCAL hexLine[HEXUI_BYTES_PER_LINE*3+2]:BYTE
    LOCAL asciiLine[HEXUI_BYTES_PER_LINE+1]:BYTE
    LOCAL lineBuf[160]:BYTE
    LOCAL offsetVal:DWORD
    LOCAL bytesLeft:DWORD
    LOCAL idx:DWORD
    LOCAL pCur:DWORD

    mov offsetVal, 0
    mov bytesLeft, dwLen
    mov esi, pData
    mov pCur, pOut

@lineLoop:
    cmp bytesLeft, 0
    je  @finish

    ; clear hex/ascii line buffers
    lea edi, hexLine
    mov ecx, sizeof hexLine
    mov al, 0
    rep stosb
    lea edi, asciiLine
    mov ecx, sizeof asciiLine
    mov al, 0
    rep stosb

    ; build one line
    lea ebx, hexLine
    lea edx, asciiLine
    mov idx, 0
@byteLoop:
    cmp bytesLeft, 0
    je  @lineDone
    cmp idx, HEXUI_BYTES_PER_LINE
    jge @lineDone

    mov al, [esi]
    push eax
    push ebx
    call HexUi_ByteToHex
    add ebx, 3            ; two hex chars + space slot
    mov BYTE PTR [ebx-1], ' '

    mov al, [esi]
    cmp al, 32
    jb  @dot
    cmp al, 126
    ja  @dot
    mov [edx], al
    jmp @advance
@dot:
    mov BYTE PTR [edx], '.'
@advance:
    inc edx
    inc esi
    inc idx
    dec bytesLeft
    jmp @byteLoop

@lineDone:
    mov BYTE PTR [ebx], 0
    mov BYTE PTR [edx], 0

    ; format line "XXXXXXXX: <hex> |<ascii>|"
    lea eax, lineBuf
    push edx              ; ascii
    lea edx, hexLine
    push edx
    push offsetVal
    push OFFSET szFmtLine
    push eax
    call wsprintfA
    add esp, 20

    mov edi, pCur
    invoke lstrcpyA, edi, ADDR lineBuf
    invoke lstrlenA, edi
    add edi, eax
    mov pCur, edi

    add offsetVal, HEXUI_BYTES_PER_LINE
    jmp @lineLoop

@finish:
    ret

szFmtLine db "%08X: %s |%s|",13,10,0
HexUi_FormatBuffer ENDP

; ----------------------------------------------------------------------------
; Window lifecycle
; ----------------------------------------------------------------------------

HexUi_RegisterClass PROC hInstance:DWORD
    cmp g_bClassRegistered, 0
    jne @done

    LOCAL wc:WNDCLASSEX
    mov wc.cbSize, SIZEOF WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    mov wc.lpfnWndProc, OFFSET HexUi_WndProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov wc.hInstance, hInstance
    mov wc.hIcon, 0
    invoke LoadCursor, NULL, IDC_ARROW
    mov wc.hCursor, eax
    mov wc.hbrBackground, COLOR_WINDOW+1
    mov wc.lpszMenuName, NULL
    mov wc.lpszClassName, OFFSET g_szHexClass
    mov wc.hIconSm, 0
    invoke RegisterClassEx, addr wc
    mov g_bClassRegistered, 1
@done:
    ret
HexUi_RegisterClass ENDP

HexUi_Init PROC hInstance:DWORD
    mov g_hInstance, hInstance
    push hInstance
    call HexUi_RegisterClass
    mov eax, 1
    ret
HexUi_Init ENDP

HexUi_Show PROC hOwner:DWORD
    mov g_hOwner, hOwner
    cmp g_hHexWnd, 0
    jne @show

    ; ensure class is ready
    push g_hInstance
    call HexUi_RegisterClass

    ; create main window
    invoke CreateWindowEx, WS_EX_APPWINDOW, ADDR g_szHexClass, ADDR g_szWindowTitle, \
           WS_OVERLAPPEDWINDOW or WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 960, 640, \
           hOwner, NULL, g_hInstance, NULL
    mov g_hHexWnd, eax
@show:
    cmp g_hHexWnd, 0
    je  @done
    invoke ShowWindow, g_hHexWnd, SW_SHOWNORMAL
    invoke SetForegroundWindow, g_hHexWnd
    invoke UpdateWindow, g_hHexWnd
@done:
    ret
HexUi_Show ENDP

HexUi_Destroy PROC
    cmp g_hHexWnd, 0
    je  @done
    invoke DestroyWindow, g_hHexWnd
    mov g_hHexWnd, 0
@done:
    ret
HexUi_Destroy ENDP

; ----------------------------------------------------------------------------
; File operations
; ----------------------------------------------------------------------------

HexUi_LoadFile PROC uses esi edi ebx ecx edx lpPath:DWORD
    LOCAL hFile:HANDLE
    LOCAL bytesRead:DWORD
    LOCAL fileSize:DWORD
    LOCAL buffer:DWORD
    LOCAL outBuf:DWORD
    LOCAL crc:DWORD

    ; remember path
    invoke lstrcpynA, ADDR g_szCurrentPath, lpPath, MAX_PATH

    invoke CreateFileA, lpPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    jne @opened
    xor eax, eax
    ret
@opened:
    mov hFile, eax
    invoke GetFileSize, hFile, NULL
    mov fileSize, eax

    mov eax, fileSize
    cmp eax, HEXUI_MAX_BYTES
    jbe @sizeOk
    mov eax, HEXUI_MAX_BYTES
@sizeOk:
    mov ecx, eax
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, ecx
    mov buffer, eax
    test eax, eax
    jz @close

    invoke ReadFile, hFile, buffer, ecx, addr bytesRead, NULL

    ; allocate output buffer (~4x input)
    mov eax, bytesRead
    shl eax, 2
    add eax, 512
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, eax
    mov outBuf, eax
    test eax, eax
    jz @freeIn

    ; format hex view
    push outBuf
    push bytesRead
    push buffer
    call HexUi_FormatBuffer

    ; set edit text
    cmp g_hHexEdit, 0
    je  @skipSet
    invoke SetWindowTextA, g_hHexEdit, outBuf
@skipSet:
    ; compute digest
    push bytesRead
    push buffer
    call HexUi_ComputeCrc32
    mov crc, eax

    ; update digest text
    invoke wsprintfA, ADDR g_szStatusLine, ADDR g_szDigestFmt, crc, bytesRead
    cmp g_hDigestStatic, 0
    je  @afterDigest
    invoke SetWindowTextA, g_hDigestStatic, ADDR g_szStatusLine
@afterDigest:
    ; status line
    invoke wsprintfA, ADDR g_szStatusLine, ADDR g_szStatusFmt, lpPath
    cmp fileSize, HEXUI_MAX_BYTES
    jbe  @statusSet
    invoke lstrcatA, ADDR g_szStatusLine, ADDR g_szTruncateNote
@statusSet:
    cmp g_hStatusStatic, 0
    je  @afterStatus
    invoke SetWindowTextA, g_hStatusStatic, ADDR g_szStatusLine
@afterStatus:

    invoke GlobalFree, outBuf
@freeIn:
    invoke GlobalFree, buffer
@close:
    invoke CloseHandle, hFile
    mov eax, 1
    ret
HexUi_LoadFile ENDP

HexUi_SelectAndLoad PROC
    LOCAL szFile[MAX_PATH]:BYTE
    LOCAL szDir[MAX_PATH]:BYTE

    ; zero structures
    lea eax, g_ofn
    mov ecx, SIZEOF OPENFILENAME / 4
    xor edx, edx
@clear:
    mov [eax], edx
    add eax, 4
    dec ecx
    jnz @clear

    mov g_ofn.lStructSize, SIZEOF OPENFILENAME
    mov eax, g_hOwner
    mov g_ofn.hwndOwner, eax
    mov g_ofn.hInstance, g_hInstance
    lea eax, szFile
    mov BYTE PTR [eax], 0
    mov g_ofn.lpstrFile, eax
    mov g_ofn.nMaxFile, MAX_PATH
    lea eax, szDir
    mov BYTE PTR [eax], 0
    mov g_ofn.lpstrInitialDir, eax
    mov g_ofn.lpstrTitle, OFFSET g_szSelectFileTitle
    mov g_ofn.Flags, OFN_FILEMUSTEXIST or OFN_PATHMUSTEXIST or OFN_HIDEREADONLY

    invoke GetOpenFileNameA, addr g_ofn
    test eax, eax
    jz @fail

    lea eax, szFile
    push eax
    call HexUi_LoadFile
    mov eax, 1
    ret
@fail:
    xor eax, eax
    ret
HexUi_SelectAndLoad ENDP

HexUi_ApplyEdits PROC uses esi edi ebx ecx edx
    LOCAL textLen:DWORD
    LOCAL textBuf:DWORD
    LOCAL byteBuf:DWORD
    LOCAL bytesOut:DWORD
    LOCAL hFile:HANDLE
    LOCAL nib1:DWORD
    LOCAL nib2:DWORD
    LOCAL ch:BYTE

    ; require current path
    cmp BYTE PTR g_szCurrentPath, 0
    jne @hasPath
    xor eax, eax
    ret
@hasPath:
    ; read edit text
    mov eax, g_hHexEdit
    test eax, eax
    jz @noedit
    invoke GetWindowTextLengthA, g_hHexEdit
    mov textLen, eax
    inc textLen
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, textLen
    mov textBuf, eax
    test eax, eax
    jz @noedit
    invoke GetWindowTextA, g_hHexEdit, textBuf, textLen

    ; allocate byte buffer
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, HEXUI_MAX_BYTES
    mov byteBuf, eax
    test eax, eax
    jz @freeText

    mov esi, textBuf
    mov edi, byteBuf
    mov bytesOut, 0

@scan:
    mov al, BYTE PTR [esi]
    cmp al, 0
    je  @parsed
    mov ch, al
    ; first nibble
    push eax
    call HexUi_IsHexDigit
    test eax, eax
    jz @advance
    mov al, ch
    push eax
    call HexUi_HexToNibble
    mov nib1, eax
    inc esi
    mov al, BYTE PTR [esi]
    cmp al, 0
    je  @parsed
    mov ch, al
    push eax
    call HexUi_IsHexDigit
    test eax, eax
    jz @advance
    mov al, ch
    push eax
    call HexUi_HexToNibble
    mov nib2, eax
    shl nib1, 4
    or  nib1, nib2
    mov al, BYTE PTR nib1
    mov [edi], al
    inc edi
    inc bytesOut
    cmp bytesOut, HEXUI_MAX_BYTES
    jb  @advance
    jmp @parsed
@advance:
    inc esi
    jmp @scan

@parsed:
    cmp bytesOut, 0
    je  @freeAll

    invoke CreateFileA, ADDR g_szCurrentPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je  @freeAll
    mov hFile, eax
    invoke WriteFile, hFile, byteBuf, bytesOut, addr ecx, NULL
    invoke CloseHandle, hFile

    ; reload to normalize view/digest
    push OFFSET g_szCurrentPath
    call HexUi_LoadFile

@freeAll:
    invoke GlobalFree, byteBuf
@freeText:
    invoke GlobalFree, textBuf
@noedit:
    mov eax, 1
    ret
HexUi_ApplyEdits ENDP

; ----------------------------------------------------------------------------
; Window proc
; ----------------------------------------------------------------------------

HexUi_WndProc PROC hWnd:HWND, uMsg:UINT, wParam:WPARAM, lParam:LPARAM
    LOCAL clientW:DWORD
    LOCAL clientH:DWORD
    LOCAL btnW:DWORD
    LOCAL btnH:DWORD

    cmp uMsg, WM_CREATE
    je  @create
    cmp uMsg, WM_SIZE
    je  @size
    cmp uMsg, WM_COMMAND
    je  @command
    cmp uMsg, WM_DESTROY
    je  @destroy

@def:
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret

@create:
    ; create controls
    invoke CreateWindowEx, 0, ADDR szStaticClass, ADDR g_szNoFile, WS_CHILD or WS_VISIBLE or SS_LEFTNOWORDWRAP, \
           8, 8, 600, 20, hWnd, IDC_HEX_STATUS, g_hInstance, NULL
    mov g_hStatusStatic, eax

    invoke CreateWindowEx, 0, ADDR szStaticClass, ADDR g_szNoFile, WS_CHILD or WS_VISIBLE or SS_LEFTNOWORDWRAP, \
           8, 32, 600, 20, hWnd, IDC_HEX_DIGEST, g_hInstance, NULL
    mov g_hDigestStatic, eax

    invoke CreateWindowEx, 0, ADDR szButtonClass, ADDR szBtnOpen, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON, \
           620, 6, 80, 24, hWnd, IDC_HEX_OPEN, g_hInstance, NULL
    mov g_hBtnOpen, eax
    invoke CreateWindowEx, 0, ADDR szButtonClass, ADDR szBtnApply, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON, \
           710, 6, 80, 24, hWnd, IDC_HEX_APPLY, g_hInstance, NULL
    mov g_hBtnApply, eax
    invoke CreateWindowEx, 0, ADDR szButtonClass, ADDR szBtnDigest, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON, \
           800, 6, 80, 24, hWnd, IDC_HEX_REHASH, g_hInstance, NULL
    mov g_hBtnDigest, eax

    invoke CreateWindowEx, WS_EX_CLIENTEDGE, ADDR szEditClass, NULL, WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_HSCROLL or ES_MULTILINE or ES_AUTOVSCROLL or ES_NOHIDESEL, \
           8, 60, 920, 540, hWnd, IDC_HEX_EDIT, g_hInstance, NULL
    mov g_hHexEdit, eax

    ; monospace font
    invoke CreateFont, -14, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_MODERN, ADDR g_szConsolas
    mov g_hMonoFont, eax
    invoke SendMessage, g_hHexEdit, WM_SETFONT, g_hMonoFont, TRUE
    invoke SendMessage, g_hDigestStatic, WM_SETFONT, g_hMonoFont, TRUE
    invoke SendMessage, g_hStatusStatic, WM_SETFONT, g_hMonoFont, TRUE

    mov g_hHexWnd, hWnd
    xor eax, eax
    ret

@size:
    mov eax, lParam
    movzx ebx, ax           ; width
    shr eax, 16
    movzx ecx, ax           ; height
    mov clientW, ebx
    mov clientH, ecx

    mov btnW, 80
    mov btnH, 24

    ; status/digest full width minus buttons
    invoke MoveWindow, g_hStatusStatic, 8, 8, clientW - 200, 20, TRUE
    invoke MoveWindow, g_hDigestStatic, 8, 32, clientW - 200, 20, TRUE
    invoke MoveWindow, g_hHexEdit, 8, 60, clientW - 16, clientH - 68, TRUE

    invoke MoveWindow, g_hBtnOpen, clientW - 180, 8, btnW, btnH, TRUE
    invoke MoveWindow, g_hBtnApply, clientW - 90, 8, btnW, btnH, TRUE
    invoke MoveWindow, g_hBtnDigest, clientW - 90, 34, btnW, btnH, TRUE
    xor eax, eax
    ret

@command:
    mov eax, wParam
    and eax, 0FFFFh
    cmp eax, IDC_HEX_OPEN
    je  @open
    cmp eax, IDC_HEX_APPLY
    je  @apply
    cmp eax, IDC_HEX_REHASH
    je  @digest
    jmp @def
@open:
    call HexUi_SelectAndLoad
    xor eax, eax
    ret
@apply:
    call HexUi_ApplyEdits
    xor eax, eax
    ret
@digest:
    cmp BYTE PTR g_szCurrentPath, 0
    je  @nodigest
    push OFFSET g_szCurrentPath
    call HexUi_LoadFile
@nodigest:
    xor eax, eax
    ret

@destroy:
    cmp g_hMonoFont, 0
    je  @nofont
    invoke DeleteObject, g_hMonoFont
    mov g_hMonoFont, 0
@nofont:
    mov g_hBtnOpen, 0
    mov g_hBtnApply, 0
    mov g_hBtnDigest, 0
    mov g_hHexWnd, 0
    xor eax, eax
    ret
HexUi_WndProc ENDP

; ----------------------------------------------------------------------------
; Exported API
; ----------------------------------------------------------------------------

PUBLIC HexUi_Init
PUBLIC HexUi_Show
PUBLIC HexUi_Destroy
PUBLIC HexUi_LoadFile
PUBLIC HexUi_SelectAndLoad
PUBLIC HexUi_ApplyEdits

END
