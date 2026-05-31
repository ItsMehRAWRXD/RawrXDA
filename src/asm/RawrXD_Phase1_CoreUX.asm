OPTION CASEMAP:NONE
OPTION DOTNAME

; =============================================================================
; RawrXD Phase 1: Core UX (MASM64, Zero-Dependency Runtime)
; - Inline ghost text overlay
; - Win32 chat sidebar
; - Slash command registry/execution
; - WinHTTP Ollama POST client
;
; Integration hooks expected from host editor:
;   Editor_GetCursorPixelPos(HWND editor, QWORD line, QWORD col, DWORD* outX, DWORD* outY)
;   Editor_GetLinePrefixA(HWND editor, QWORD line, BYTE* outBuf, DWORD outCap) -> RAX=len
;   Editor_InsertTextA(HWND editor, BYTE* text, DWORD len)
; =============================================================================

; -----------------------------------------------------------------------------
; Win32 constants
; -----------------------------------------------------------------------------
TRUE                        equ 1
FALSE                       equ 0
NULL                        equ 0
INVALID_HANDLE_VALUE        equ -1

WM_CREATE                   equ 0001h
WM_DESTROY                  equ 0002h
WM_SIZE                     equ 0005h
WM_PAINT                    equ 000Fh
WM_COMMAND                  equ 0111h
WM_GETTEXT                  equ 000Dh
WM_SETTEXT                  equ 000Ch
WM_USER                     equ 0400h

WS_CHILD                    equ 40000000h
WS_VISIBLE                  equ 10000000h
WS_BORDER                   equ 00800000h
WS_VSCROLL                  equ 00200000h
WS_POPUP                    equ 80000000h
WS_EX_CLIENTEDGE            equ 00000200h
WS_EX_LAYERED               equ 00080000h
WS_EX_TRANSPARENT           equ 00000020h
WS_EX_NOACTIVATE            equ 08000000h

SW_HIDE                     equ 0
SW_SHOW                     equ 5

SWP_NOSIZE                  equ 0001h
SWP_NOMOVE                  equ 0002h
SWP_NOZORDER                equ 0004h
SWP_NOACTIVATE              equ 0010h
SWP_SHOWWINDOW              equ 0040h

BS_PUSHBUTTON               equ 00000000h
ES_MULTILINE                equ 0004h
ES_AUTOVSCROLL              equ 0040h
ES_WANTRETURN               equ 1000h

FW_NORMAL                   equ 400
DEFAULT_CHARSET             equ 1
OUT_DEFAULT_PRECIS          equ 0
CLIP_DEFAULT_PRECIS         equ 0
CLEARTYPE_QUALITY           equ 5
DEFAULT_PITCH               equ 0
FF_MODERN                   equ 30h
TRANSPARENT                 equ 1

COLOR_WINDOW                equ 5
SRCCOPY                     equ 00CC0020h

WINHTTP_ACCESS_TYPE_NO_PROXY      equ 1
WINHTTP_NO_PROXY_NAME             equ 0
WINHTTP_NO_PROXY_BYPASS           equ 0
WINHTTP_NO_REFERER                equ 0
WINHTTP_DEFAULT_ACCEPT_TYPES      equ 0
WINHTTP_ADDREQ_FLAG_ADD           equ 20000000h

; -----------------------------------------------------------------------------
; Phase constants
; -----------------------------------------------------------------------------
GHOST_MAX_SUGGESTION_LEN    equ 512
GHOST_DEBOUNCE_MS           equ 300
GHOST_LINE_HEIGHT           equ 18
GHOST_FONT_SIZE             equ 11
GHOST_COLOR_SUGGESTION      equ 00C0C0C0h

CHAT_MAX_HISTORY            equ 1000
CHAT_MAX_MESSAGE_LEN        equ 8192
CHAT_INPUT_HEIGHT           equ 60
CHAT_LINE_SPACING           equ 4

SLASH_CMD_MAX               equ 32
SLASH_CMD_NAME_MAX          equ 32
SLASH_CMD_DESC_MAX          equ 128
SLASH_CMD_TEMPLATE_MAX      equ 512

HTTP_BUF_SIZE               equ 65536
OLLAMA_PORT                 equ 11434

IDC_CHAT_INPUT              equ 1001
IDC_CHAT_SUBMIT             equ 1002

WM_GHOST_UPDATE             equ WM_USER + 100
WM_GHOST_ACCEPT             equ WM_USER + 101
WM_GHOST_DISMISS            equ WM_USER + 102
WM_CHAT_SUBMIT              equ WM_USER + 103
WM_CHAT_STREAM_TOKEN        equ WM_USER + 104
WM_SLASH_COMPLETE           equ WM_USER + 105

; -----------------------------------------------------------------------------
; Struct layouts (manual offsets)
; -----------------------------------------------------------------------------
; GHOST_STATE
GS_hWndParent               equ 0
GS_hWndOverlay              equ 8
GS_lineNumber               equ 16
GS_columnNumber             equ 24
GS_pixelX                   equ 32
GS_pixelY                   equ 36
GS_userPrefix               equ 40
GS_suggestionText           equ GS_userPrefix + 256
GS_suggestionLen            equ GS_suggestionText + GHOST_MAX_SUGGESTION_LEN
GS_isVisible                equ GS_suggestionLen + 8
GS_isPending                equ GS_isVisible + 1
GS_pad0                     equ GS_isPending + 1
GS_lastTypingTick           equ GS_pad0 + 6
GS_hFontSuggestion          equ GS_lastTypingTick + 8
GS_hInferenceThread         equ GS_hFontSuggestion + 8
GS_inferenceBuf             equ GS_hInferenceThread + 8
GS_sizeof                   equ GS_inferenceBuf + GHOST_MAX_SUGGESTION_LEN

; CHAT_MESSAGE
CM_msgType                  equ 0
CM_timestamp                equ 8
CM_content                  equ 16
CM_contentLen               equ CM_content + CHAT_MAX_MESSAGE_LEN
CM_isStreaming              equ CM_contentLen + 8
CM_sizeof                   equ CM_isStreaming + 8

; SLASH_COMMAND
SC_name                     equ 0
SC_description              equ SC_name + SLASH_CMD_NAME_MAX
SC_promptTemplate           equ SC_description + SLASH_CMD_DESC_MAX
SC_handlerPtr               equ SC_promptTemplate + SLASH_CMD_TEMPLATE_MAX
SC_requiresContext          equ SC_handlerPtr + 8
SC_sizeof                   equ SC_requiresContext + 8

; WNDCLASSEXA (x64)
WCE_cbSize                  equ 0
WCE_style                   equ 4
WCE_lpfnWndProc             equ 8
WCE_cbClsExtra              equ 16
WCE_cbWndExtra              equ 20
WCE_hInstance               equ 24
WCE_hIcon                   equ 32
WCE_hCursor                 equ 40
WCE_hbrBackground           equ 48
WCE_lpszMenuName            equ 56
WCE_lpszClassName           equ 64
WCE_hIconSm                 equ 72
WCE_sizeof                  equ 80

; PAINTSTRUCT (x64)
PAINTSTRUCT_sizeof          equ 72

; -----------------------------------------------------------------------------
; Imports
; -----------------------------------------------------------------------------
EXTERN __imp_GetModuleHandleA:QWORD
EXTERN __imp_RegisterClassExA:QWORD
EXTERN __imp_CreateWindowExA:QWORD
EXTERN __imp_DefWindowProcA:QWORD
EXTERN __imp_ShowWindow:QWORD
EXTERN __imp_SetWindowPos:QWORD
EXTERN __imp_InvalidateRect:QWORD
EXTERN __imp_BeginPaint:QWORD
EXTERN __imp_EndPaint:QWORD
EXTERN __imp_SetBkMode:QWORD
EXTERN __imp_SetTextColor:QWORD
EXTERN __imp_TextOutA:QWORD
EXTERN __imp_CreateFontA:QWORD
EXTERN __imp_SelectObject:QWORD
EXTERN __imp_DeleteObject:QWORD
EXTERN __imp_GetTickCount64:QWORD
EXTERN __imp_PostMessageA:QWORD
EXTERN __imp_CreateThread:QWORD
EXTERN __imp_CloseHandle:QWORD
EXTERN __imp_SendMessageA:QWORD
EXTERN __imp_GetClientRect:QWORD
EXTERN __imp_GetSystemTimeAsFileTime:QWORD

EXTERN __imp_WinHttpOpen:QWORD
EXTERN __imp_WinHttpConnect:QWORD
EXTERN __imp_WinHttpOpenRequest:QWORD
EXTERN __imp_WinHttpAddRequestHeaders:QWORD
EXTERN __imp_WinHttpSendRequest:QWORD
EXTERN __imp_WinHttpReceiveResponse:QWORD
EXTERN __imp_WinHttpQueryDataAvailable:QWORD
EXTERN __imp_WinHttpReadData:QWORD
EXTERN __imp_WinHttpCloseHandle:QWORD

; Host/editor callbacks
EXTERN Editor_GetCursorPixelPos:PROC
EXTERN Editor_GetLinePrefixA:PROC
EXTERN Editor_InsertTextA:PROC

; -----------------------------------------------------------------------------
; Globals
; -----------------------------------------------------------------------------
.data
align 16
g_ghostState                 db GS_sizeof dup(0)

align 16
g_chatHistory                db CM_sizeof * CHAT_MAX_HISTORY dup(0)
g_chatMsgCount               dq 0
g_chatMsgHead                dq 0

g_slashCommands              db SC_sizeof * SLASH_CMD_MAX dup(0)
g_slashCmdCount              dq 0

g_hWndChat                   dq 0
g_hWndChatInput              dq 0
g_hWndChatSubmit             dq 0
g_hInferenceThread           dq 0

g_chatInputBuf               db CHAT_MAX_MESSAGE_LEN dup(0)
g_inferencePromptBuf         db 16384 dup(0)
g_slashPromptBuf             db 8192 dup(0)
g_helpTextBuf                db 4096 dup(0)

g_httpResponseBuf            db HTTP_BUF_SIZE dup(0)

; Small scratch
scratchA                     db 256 dup(0)

; Classes/titles
szGhostClass                 db "RawrXD_GhostOverlay", 0
szChatClass                  db "RawrXD_ChatSidebar", 0
szEditClass                  db "EDIT", 0
szButtonClass                db "BUTTON", 0
szChatTitle                  db "RawrXD Chat", 0
szSubmitText                 db "Send", 0
szFontName                   db "Consolas", 0

; Prompt/header strings
szPromptComplete             db "Complete this code: ", 0
szSystemPrompt               db "You are a helpful coding assistant. Be concise and accurate.", 13, 10, 13, 10, 0
szHelpHeader                 db "Available commands:", 0
szSlashPrefix                db "/", 0

; Default slash commands
szCmdTest                    db "test", 0
szDescTest                   db "Generate unit tests for current file", 0
szPromptTest                 db "Generate comprehensive unit tests for the following code:", 13, 10, "%s", 0

szCmdExplain                 db "explain", 0
szDescExplain                db "Explain selected code", 0
szPromptExplain              db "Explain what this code does:", 13, 10, "%s", 0

szCmdRefactor                db "refactor", 0
szDescRefactor               db "Refactor for clarity", 0
szPromptRefactor             db "Refactor this code to improve readability and maintainability:", 13, 10, "%s", 0

szCmdFix                     db "fix", 0
szDescFix                    db "Fix errors in code", 0
szPromptFix                  db "Fix any errors or bugs in this code:", 13, 10, "%s", 0

szCmdDoc                     db "doc", 0
szDescDoc                    db "Generate documentation", 0
szPromptDoc                  db "Generate documentation comments for this code:", 13, 10, "%s", 0

szCmdOptimize                db "optimize", 0
szDescOptimize               db "Optimize performance", 0
szPromptOptimize             db "Optimize this code for better performance:", 13, 10, "%s", 0

; WinHTTP wide strings
wszAgent                     dw 'R','a','w','r','X','D','/','1','.','0',0
wszHost                      dw '1','2','7','.','0','.','0','.','1',0
wszPost                      dw 'P','O','S','T',0
wszPathGenerate              dw '/','a','p','i','/','g','e','n','e','r','a','t','e',0
wszJsonHeader                dw 'C','o','n','t','e','n','t','-','T','y','p','e',':',' ','a','p','p','l','i','c','a','t','i','o','n','/','j','s','o','n',13,10,0

.code

; -----------------------------------------------------------------------------
; Utilities
; -----------------------------------------------------------------------------
String_LengthA PROC
    ; RCX=str
    mov rax, rcx
@@:
    cmp byte ptr [rax], 0
    je @F
    inc rax
    jmp @B
@@:
    sub rax, rcx
    ret
String_LengthA ENDP

String_CopyA PROC
    ; RCX=src, RDX=dst, RAX=dst_end(no NUL)
    mov r8, rcx
    mov r9, rdx
@@:
    mov al, byte ptr [r8]
    mov byte ptr [r9], al
    inc r8
    inc r9
    test al, al
    jnz @B
    lea rax, [r9-1]
    ret
String_CopyA ENDP

String_CopyNA PROC
    ; RCX=src, RDX=dst, R8=maxLen
    ; always NUL-terminates if maxLen>0
    test r8, r8
    jz @copy_done
    mov r9, r8
    dec r9
    xor r10, r10
@@:
    test r9, r9
    jz @force_nul
    mov al, byte ptr [rcx+r10]
    mov byte ptr [rdx+r10], al
    inc r10
    dec r9
    test al, al
    jnz @B
    jmp @copy_done
@force_nul:
    mov byte ptr [rdx+r10], 0
@copy_done:
    ret
String_CopyNA ENDP

String_EqualsA PROC
    ; RCX=a, RDX=b, returns EAX=1 equal
@@:
    mov al, byte ptr [rcx]
    mov dl, byte ptr [rdx]
    cmp al, dl
    jne @ne
    test al, al
    je @eq
    inc rcx
    inc rdx
    jmp @B
@eq:
    mov eax, 1
    ret
@ne:
    xor eax, eax
    ret
String_EqualsA ENDP

GetUnixTimestamp PROC
    ; RAX = seconds since epoch
    sub rsp, 32
    lea rcx, [rsp]
    call qword ptr [__imp_GetSystemTimeAsFileTime]
    mov rax, qword ptr [rsp]
    ; FILETIME now in RAX (100ns since 1601)
    mov r8, 116444736000000000
    sub rax, r8
    xor rdx, rdx
    mov rcx, 10000000
    div rcx
    add rsp, 32
    ret
GetUnixTimestamp ENDP

; -----------------------------------------------------------------------------
; Internal helper: Circular slot pointer
; RCX = head index
; RAX = ptr CHAT_MESSAGE
; -----------------------------------------------------------------------------
Chat_GetSlotPtr PROC
    mov rax, rcx
    imul rax, CM_sizeof
    lea rdx, g_chatHistory
    add rax, rdx
    ret
Chat_GetSlotPtr ENDP

; -----------------------------------------------------------------------------
; Ghost overlay window proc
; -----------------------------------------------------------------------------
GhostText_WndProc PROC
    ; RCX=hWnd, EDX=msg, R8=wParam, R9=lParam
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14

    cmp edx, WM_PAINT
    je ghost_wm_paint
    cmp edx, WM_GHOST_UPDATE
    je ghost_wm_update
    cmp edx, WM_GHOST_DISMISS
    je ghost_wm_dismiss
    cmp edx, WM_GHOST_ACCEPT
    je ghost_wm_accept

    call qword ptr [__imp_DefWindowProcA]
    jmp ghost_wm_done

ghost_wm_update:
    ; Copy inference buffer -> suggestion
    lea rcx, [g_ghostState+GS_inferenceBuf]
    lea rdx, [g_ghostState+GS_suggestionText]
    mov r8d, GHOST_MAX_SUGGESTION_LEN
    call String_CopyNA
    lea rcx, [g_ghostState+GS_suggestionText]
    call String_LengthA
    mov qword ptr [g_ghostState+GS_suggestionLen], rax
    mov byte ptr [g_ghostState+GS_isVisible], 1
    call GhostText_UpdatePosition
    xor eax, eax
    jmp ghost_wm_done

ghost_wm_dismiss:
    mov byte ptr [g_ghostState+GS_isVisible], 0
    mov rcx, qword ptr [g_ghostState+GS_hWndOverlay]
    mov edx, SW_HIDE
    call qword ptr [__imp_ShowWindow]
    xor eax, eax
    jmp ghost_wm_done

ghost_wm_accept:
    mov rcx, qword ptr [g_ghostState+GS_hWndParent]
    lea rdx, [g_ghostState+GS_suggestionText]
    mov r8d, dword ptr [g_ghostState+GS_suggestionLen]
    call Editor_InsertTextA
    mov byte ptr [g_ghostState+GS_isVisible], 0
    mov rcx, qword ptr [g_ghostState+GS_hWndOverlay]
    mov edx, SW_HIDE
    call qword ptr [__imp_ShowWindow]
    xor eax, eax
    jmp ghost_wm_done

ghost_wm_paint:
    sub rsp, 128
    lea rdx, [rsp]
    call qword ptr [__imp_BeginPaint]
    mov r10, rax ; HDC

    mov rcx, r10
    mov edx, TRANSPARENT
    call qword ptr [__imp_SetBkMode]

    mov rcx, r10
    mov edx, GHOST_COLOR_SUGGESTION
    call qword ptr [__imp_SetTextColor]

    mov rcx, r10
    mov rdx, qword ptr [g_ghostState+GS_hFontSuggestion]
    call qword ptr [__imp_SelectObject]

    mov rcx, r10
    xor edx, edx
    xor r8d, r8d
    lea r9, [g_ghostState+GS_suggestionText]
    mov eax, dword ptr [g_ghostState+GS_suggestionLen]
    mov dword ptr [rsp+32], eax
    call qword ptr [__imp_TextOutA]

    mov rcx, qword ptr [g_ghostState+GS_hWndOverlay]
    lea rdx, [rsp]
    call qword ptr [__imp_EndPaint]
    add rsp, 128
    xor eax, eax
    jmp ghost_wm_done

ghost_wm_done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
GhostText_WndProc ENDP

; -----------------------------------------------------------------------------
; Chat sidebar window proc
; -----------------------------------------------------------------------------
ChatSidebar_WndProc PROC
    ; RCX=hWnd, EDX=msg, R8=wParam, R9=lParam
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14

    cmp edx, WM_COMMAND
    je chat_wm_command
    cmp edx, WM_SIZE
    je chat_wm_size
    cmp edx, WM_PAINT
    je chat_wm_paint
    cmp edx, WM_CHAT_STREAM_TOKEN
    je chat_wm_stream

    call qword ptr [__imp_DefWindowProcA]
    jmp chat_wm_done

chat_wm_command:
    mov eax, r8d
    and eax, 0FFFFh
    cmp eax, IDC_CHAT_SUBMIT
    je chat_do_submit
    cmp eax, IDC_CHAT_INPUT
    jne chat_cmd_done

chat_do_submit:
    call ChatSidebar_OnSubmit
chat_cmd_done:
    xor eax, eax
    jmp chat_wm_done

chat_wm_size:
    call ChatSidebar_LayoutControls
    xor eax, eax
    jmp chat_wm_done

chat_wm_stream:
    mov rcx, qword ptr [g_hWndChat]
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call qword ptr [__imp_InvalidateRect]
    xor eax, eax
    jmp chat_wm_done

chat_wm_paint:
    sub rsp, 128
    lea rdx, [rsp]
    call qword ptr [__imp_BeginPaint]
    mov r10, rax

    ; Render compact transcript as plain lines (user/assistant/system)
    xor r11, r11                           ; row
    mov r12, qword ptr [g_chatMsgCount]
    test r12, r12
    jz chat_paint_done

    mov r13, qword ptr [g_chatMsgHead]
    sub r13, r12
    jns @F
    add r13, CHAT_MAX_HISTORY
@@:
    ; iterate count
chat_paint_loop:
    mov rax, r13
    cmp rax, CHAT_MAX_HISTORY
    jb @F
    sub rax, CHAT_MAX_HISTORY
@@:
    imul rax, CM_sizeof
    lea rdx, g_chatHistory
    lea r14, [rdx+rax]

    ; Y = 8 + row*(16+spacing)
    mov eax, r11d
    imul eax, (16 + CHAT_LINE_SPACING)
    add eax, 8

    mov rcx, r10
    mov edx, 8
    mov r8d, eax
    lea r9, [r14+CM_content]
    mov eax, dword ptr [r14+CM_contentLen]
    mov dword ptr [rsp+32], eax
    call qword ptr [__imp_TextOutA]

    inc r13
    inc r11
    dec r12
    jnz chat_paint_loop

chat_paint_done:
    mov rcx, qword ptr [g_hWndChat]
    lea rdx, [rsp]
    call qword ptr [__imp_EndPaint]
    add rsp, 128
    xor eax, eax
    jmp chat_wm_done

chat_wm_done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ChatSidebar_WndProc ENDP

; -----------------------------------------------------------------------------
; GhostText_UpdatePosition
; -----------------------------------------------------------------------------
GhostText_UpdatePosition PROC
    ; Uses global state
    mov eax, dword ptr [g_ghostState+GS_pixelX]
    mov r10d, dword ptr [g_ghostState+GS_pixelY]
    add r10d, GHOST_LINE_HEIGHT

    mov ecx, dword ptr [g_ghostState+GS_suggestionLen]
    imul ecx, 8
    add ecx, 20

    mov rcx, qword ptr [g_ghostState+GS_hWndOverlay]
    xor rdx, rdx                            ; hWndInsertAfter
    mov r8d, eax                            ; X
    mov r9d, r10d                           ; Y
    sub rsp, 48
    mov dword ptr [rsp+32], ecx             ; cx
    mov dword ptr [rsp+40], 24              ; cy
    mov dword ptr [rsp+48], SWP_NOZORDER or SWP_NOACTIVATE or SWP_SHOWWINDOW
    call qword ptr [__imp_SetWindowPos]
    add rsp, 48

    mov rcx, qword ptr [g_ghostState+GS_hWndOverlay]
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call qword ptr [__imp_InvalidateRect]

    ret
GhostText_UpdatePosition ENDP

; -----------------------------------------------------------------------------
; Ghost inference worker
; RCX = &g_ghostState
; -----------------------------------------------------------------------------
GhostText_InferenceThread PROC
    push rbx
    sub rsp, 64
    mov rbx, rcx

    ; Build minimal JSON body in g_inferencePromptBuf:
    ; {"model":"phi3","prompt":"<prefix>","stream":false}
    lea rcx, szJsonPrefixA
    lea rdx, g_inferencePromptBuf
    call String_CopyA

    ; append user prefix
    lea rcx, [rbx+GS_userPrefix]
    mov rdx, rax
    call String_CopyA

    ; append suffix
    lea rcx, szJsonSuffixA
    mov rdx, rax
    call String_CopyA

    ; POST and parse response into inferenceBuf
    lea rcx, g_inferencePromptBuf
    lea rdx, g_httpResponseBuf
    mov r8d, HTTP_BUF_SIZE
    call HTTP_PostOllama

    ; If failed, clear suggestion
    cmp rax, 0
    jle infer_fail

    lea rcx, g_httpResponseBuf
    lea rdx, [rbx+GS_inferenceBuf]
    mov r8d, GHOST_MAX_SUGGESTION_LEN
    call Json_ExtractResponseA
    jmp infer_post

infer_fail:
    mov byte ptr [rbx+GS_inferenceBuf], 0

infer_post:
    mov rcx, qword ptr [rbx+GS_hWndParent]
    mov edx, WM_GHOST_UPDATE
    xor r8d, r8d
    xor r9d, r9d
    call qword ptr [__imp_PostMessageA]

    mov byte ptr [rbx+GS_isPending], 0

    add rsp, 64
    pop rbx
    xor eax, eax
    ret
GhostText_InferenceThread ENDP

; -----------------------------------------------------------------------------
; Public API: GhostText_Initialize
; RCX=hWndParent
; -----------------------------------------------------------------------------
PUBLIC GhostText_Initialize
GhostText_Initialize PROC
    push rbx
    push rdi
    sub rsp, 192
    mov rbx, rcx

    mov qword ptr [g_ghostState+GS_hWndParent], rbx

    ; Register class
    lea rdi, [rsp]
    mov ecx, WCE_sizeof/8
    xor rax, rax
    rep stosq

    mov dword ptr [rsp+WCE_cbSize], WCE_sizeof
    mov dword ptr [rsp+WCE_style], 0
    lea rax, GhostText_WndProc
    mov qword ptr [rsp+WCE_lpfnWndProc], rax
    xor rcx, rcx
    call qword ptr [__imp_GetModuleHandleA]
    mov qword ptr [rsp+WCE_hInstance], rax
    mov qword ptr [rsp+WCE_hbrBackground], COLOR_WINDOW+1
    lea rax, szGhostClass
    mov qword ptr [rsp+WCE_lpszClassName], rax

    lea rcx, [rsp]
    call qword ptr [__imp_RegisterClassExA]

    ; Create transparent child overlay
    mov ecx, WS_EX_TRANSPARENT or WS_EX_NOACTIVATE
    lea rdx, szGhostClass
    xor r8d, r8d
    mov r9d, WS_CHILD or WS_VISIBLE
    mov dword ptr [rsp+32], 0
    mov dword ptr [rsp+40], 0
    mov dword ptr [rsp+48], 400
    mov dword ptr [rsp+56], 24
    mov qword ptr [rsp+64], rbx
    mov qword ptr [rsp+72], 0
    mov qword ptr [rsp+80], 0
    mov qword ptr [rsp+88], 0
    call qword ptr [__imp_CreateWindowExA]
    mov qword ptr [g_ghostState+GS_hWndOverlay], rax
    test rax, rax
    jz ghost_init_fail

    ; font
    mov ecx, -GHOST_FONT_SIZE
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    mov dword ptr [rsp+32], FW_NORMAL
    mov dword ptr [rsp+40], 0
    mov dword ptr [rsp+48], 0
    mov dword ptr [rsp+56], 0
    mov dword ptr [rsp+64], DEFAULT_CHARSET
    mov dword ptr [rsp+72], OUT_DEFAULT_PRECIS
    mov dword ptr [rsp+80], CLIP_DEFAULT_PRECIS
    mov dword ptr [rsp+88], CLEARTYPE_QUALITY
    mov dword ptr [rsp+96], DEFAULT_PITCH or FF_MODERN
    lea rax, szFontName
    mov qword ptr [rsp+104], rax
    call qword ptr [__imp_CreateFontA]
    mov qword ptr [g_ghostState+GS_hFontSuggestion], rax

    mov byte ptr [g_ghostState+GS_isVisible], 0
    mov byte ptr [g_ghostState+GS_isPending], 0
    call qword ptr [__imp_GetTickCount64]
    mov qword ptr [g_ghostState+GS_lastTypingTick], rax

    mov eax, TRUE
    jmp ghost_init_done

ghost_init_fail:
    xor eax, eax

ghost_init_done:
    add rsp, 192
    pop rdi
    pop rbx
    ret
GhostText_Initialize ENDP

; -----------------------------------------------------------------------------
; Public API: GhostText_OnTyping
; RCX=hWndEditor, RDX=line, R8=col, R9=typedChar
; -----------------------------------------------------------------------------
PUBLIC GhostText_OnTyping
GhostText_OnTyping PROC
    push rbx
    sub rsp, 96

    mov qword ptr [g_ghostState+GS_hWndParent], rcx
    mov qword ptr [g_ghostState+GS_lineNumber], rdx
    mov qword ptr [g_ghostState+GS_columnNumber], r8

    ; query cursor pixels
    mov rbx, rcx
    mov rcx, rbx
    mov rdx, qword ptr [g_ghostState+GS_lineNumber]
    mov r8, qword ptr [g_ghostState+GS_columnNumber]
    lea r9, [g_ghostState+GS_pixelX]
    call Editor_GetCursorPixelPos

    call qword ptr [__imp_GetTickCount64]
    mov r10, qword ptr [g_ghostState+GS_lastTypingTick]
    sub rax, r10
    cmp rax, GHOST_DEBOUNCE_MS
    jb typing_debounce

    ; get line prefix
    mov rcx, rbx
    mov rdx, qword ptr [g_ghostState+GS_lineNumber]
    lea r8, [g_ghostState+GS_userPrefix]
    mov r9d, 256
    call Editor_GetLinePrefixA
    cmp rax, 3
    jb typing_hide

    mov byte ptr [g_ghostState+GS_isPending], 1
    call qword ptr [__imp_GetTickCount64]
    mov qword ptr [g_ghostState+GS_lastTypingTick], rax

    ; launch worker
    xor ecx, ecx
    xor edx, edx
    lea r8, GhostText_InferenceThread
    lea r9, g_ghostState
    mov dword ptr [rsp+32], 0
    mov qword ptr [rsp+40], 0
    call qword ptr [__imp_CreateThread]
    mov qword ptr [g_ghostState+GS_hInferenceThread], rax
    jmp typing_done

typing_debounce:
    call GhostText_UpdatePosition
    jmp typing_done

typing_hide:
    mov byte ptr [g_ghostState+GS_isVisible], 0
    mov rcx, qword ptr [g_ghostState+GS_hWndOverlay]
    mov edx, SW_HIDE
    call qword ptr [__imp_ShowWindow]

typing_done:
    add rsp, 96
    pop rbx
    ret
GhostText_OnTyping ENDP

; -----------------------------------------------------------------------------
; Public API: ChatSidebar_Initialize
; RCX=parent, RDX=width, R8=height
; -----------------------------------------------------------------------------
PUBLIC ChatSidebar_Initialize
ChatSidebar_Initialize PROC
    push rbx
    push rdi
    sub rsp, 192
    mov rbx, rcx
    mov r10d, edx
    mov r11d, r8d

    ; register sidebar class
    lea rdi, [rsp]
    mov ecx, WCE_sizeof/8
    xor rax, rax
    rep stosq

    mov dword ptr [rsp+WCE_cbSize], WCE_sizeof
    lea rax, ChatSidebar_WndProc
    mov qword ptr [rsp+WCE_lpfnWndProc], rax
    xor rcx, rcx
    call qword ptr [__imp_GetModuleHandleA]
    mov qword ptr [rsp+WCE_hInstance], rax
    mov qword ptr [rsp+WCE_hbrBackground], COLOR_WINDOW+1
    lea rax, szChatClass
    mov qword ptr [rsp+WCE_lpszClassName], rax
    lea rcx, [rsp]
    call qword ptr [__imp_RegisterClassExA]

    ; create sidebar child at fixed right side
    mov ecx, WS_EX_CLIENTEDGE
    lea rdx, szChatClass
    lea r8, szChatTitle
    mov r9d, WS_CHILD or WS_VISIBLE or WS_VSCROLL
    mov dword ptr [rsp+32], 0
    mov dword ptr [rsp+40], 0
    mov dword ptr [rsp+48], r10d
    mov dword ptr [rsp+56], r11d
    mov qword ptr [rsp+64], rbx
    mov qword ptr [rsp+72], 0
    mov qword ptr [rsp+80], 0
    mov qword ptr [rsp+88], 0
    call qword ptr [__imp_CreateWindowExA]
    mov qword ptr [g_hWndChat], rax

    ; input box
    xor ecx, ecx
    lea rdx, szEditClass
    xor r8d, r8d
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_AUTOVSCROLL or ES_WANTRETURN
    mov dword ptr [rsp+32], 8
    mov eax, r11d
    sub eax, CHAT_INPUT_HEIGHT
    sub eax, 8
    mov dword ptr [rsp+40], eax
    mov eax, r10d
    sub eax, 84
    mov dword ptr [rsp+48], eax
    mov dword ptr [rsp+56], CHAT_INPUT_HEIGHT-8
    mov rax, qword ptr [g_hWndChat]
    mov qword ptr [rsp+64], rax
    mov qword ptr [rsp+72], IDC_CHAT_INPUT
    mov qword ptr [rsp+80], 0
    mov qword ptr [rsp+88], 0
    call qword ptr [__imp_CreateWindowExA]
    mov qword ptr [g_hWndChatInput], rax

    ; submit button
    xor ecx, ecx
    lea rdx, szButtonClass
    lea r8, szSubmitText
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov eax, r10d
    sub eax, 68
    mov dword ptr [rsp+32], eax
    mov eax, r11d
    sub eax, CHAT_INPUT_HEIGHT
    sub eax, 8
    mov dword ptr [rsp+40], eax
    mov dword ptr [rsp+48], 60
    mov dword ptr [rsp+56], CHAT_INPUT_HEIGHT-8
    mov rax, qword ptr [g_hWndChat]
    mov qword ptr [rsp+64], rax
    mov qword ptr [rsp+72], IDC_CHAT_SUBMIT
    mov qword ptr [rsp+80], 0
    mov qword ptr [rsp+88], 0
    call qword ptr [__imp_CreateWindowExA]
    mov qword ptr [g_hWndChatSubmit], rax

    mov qword ptr [g_chatMsgCount], 0
    mov qword ptr [g_chatMsgHead], 0

    mov eax, TRUE
    add rsp, 192
    pop rdi
    pop rbx
    ret
ChatSidebar_Initialize ENDP

ChatSidebar_LayoutControls PROC
    ; Minimal placeholder: controls are fixed in this phase module.
    ret
ChatSidebar_LayoutControls ENDP

; -----------------------------------------------------------------------------
; Public API: ChatSidebar_AddMessage
; RCX=msgType, RDX=content, R8=contentLen
; -----------------------------------------------------------------------------
PUBLIC ChatSidebar_AddMessage
ChatSidebar_AddMessage PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32

    mov bl, cl
    mov rsi, rdx
    mov rdi, r8

    mov rcx, qword ptr [g_chatMsgHead]
    call Chat_GetSlotPtr
    mov r11, rax

    mov byte ptr [r11+CM_msgType], bl
    call GetUnixTimestamp
    mov qword ptr [r11+CM_timestamp], rax
    mov qword ptr [r11+CM_contentLen], rdi
    mov byte ptr [r11+CM_isStreaming], 0

    lea rdx, [r11+CM_content]
    mov rcx, rsi
    mov r8, rdi
    cmp r8, CHAT_MAX_MESSAGE_LEN-1
    jbe @F
    mov r8, CHAT_MAX_MESSAGE_LEN-1
@@:
    call String_CopyNA

    ; advance head/count
    inc qword ptr [g_chatMsgHead]
    mov rax, qword ptr [g_chatMsgHead]
    cmp rax, CHAT_MAX_HISTORY
    jb @F
    mov qword ptr [g_chatMsgHead], 0
@@:
    mov rax, qword ptr [g_chatMsgCount]
    cmp rax, CHAT_MAX_HISTORY
    jae msg_no_count
    inc qword ptr [g_chatMsgCount]
msg_no_count:

    mov rcx, qword ptr [g_hWndChat]
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call qword ptr [__imp_InvalidateRect]

    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
ChatSidebar_AddMessage ENDP

; -----------------------------------------------------------------------------
; Public API: ChatSidebar_StreamToken
; RCX=token, RDX=tokenLen
; -----------------------------------------------------------------------------
PUBLIC ChatSidebar_StreamToken
ChatSidebar_StreamToken PROC
    ; Phase 1 keeps non-streaming finalize path. Append as assistant line.
    mov r8, rdx
    mov rdx, rcx
    mov ecx, 1
    jmp ChatSidebar_AddMessage
ChatSidebar_StreamToken ENDP

; -----------------------------------------------------------------------------
; Public API: ChatSidebar_OnSubmit
; -----------------------------------------------------------------------------
PUBLIC ChatSidebar_OnSubmit
ChatSidebar_OnSubmit PROC
    push rbx
    sub rsp, 32

    mov rcx, qword ptr [g_hWndChatInput]
    mov edx, WM_GETTEXT
    mov r8d, CHAT_MAX_MESSAGE_LEN
    lea r9, g_chatInputBuf
    call qword ptr [__imp_SendMessageA]
    mov rbx, rax
    test rbx, rbx
    jz submit_done

    cmp byte ptr [g_chatInputBuf], '/'
    je submit_slash

    xor ecx, ecx
    lea rdx, g_chatInputBuf
    mov r8, rbx
    call ChatSidebar_AddMessage

    ; trigger synchronous inference for phase 1
    lea rcx, g_chatInputBuf
    mov rdx, rbx
    call ChatSidebar_StartInference
    jmp submit_clear

submit_slash:
    lea rcx, g_chatInputBuf
    mov rdx, rbx
    call SlashCommand_Execute

submit_clear:
    mov rcx, qword ptr [g_hWndChatInput]
    mov edx, WM_SETTEXT
    xor r8d, r8d
    lea r9, szEmpty
    call qword ptr [__imp_SendMessageA]

submit_done:
    add rsp, 32
    pop rbx
    ret
ChatSidebar_OnSubmit ENDP

; -----------------------------------------------------------------------------
; Inference: non-streaming chat path
; RCX=prompt, RDX=promptLen
; -----------------------------------------------------------------------------
ChatSidebar_StartInference PROC
    push rbx
    sub rsp, 32
    mov rbx, rcx

    ; Build minimal payload
    lea rcx, szJsonPrefixA
    lea rdx, g_inferencePromptBuf
    call String_CopyA
    mov rdx, rax
    mov rcx, rbx
    call String_CopyA
    mov rdx, rax
    lea rcx, szJsonSuffixA
    call String_CopyA

    lea rcx, g_inferencePromptBuf
    lea rdx, g_httpResponseBuf
    mov r8d, HTTP_BUF_SIZE
    call HTTP_PostOllama

    cmp rax, 0
    jle infer_chat_fail

    lea rcx, g_httpResponseBuf
    lea rdx, g_slashPromptBuf
    mov r8d, sizeof g_slashPromptBuf
    call Json_ExtractResponseA

    mov ecx, 1
    lea rdx, g_slashPromptBuf
    lea rcx, g_slashPromptBuf
    call String_LengthA
    mov r8, rax
    mov ecx, 1
    lea rdx, g_slashPromptBuf
    call ChatSidebar_AddMessage
    jmp infer_chat_done

infer_chat_fail:
    mov ecx, 2
    lea rdx, szInferenceFail
    lea rcx, szInferenceFail
    call String_LengthA
    mov r8, rax
    mov ecx, 2
    lea rdx, szInferenceFail
    call ChatSidebar_AddMessage

infer_chat_done:
    add rsp, 32
    pop rbx
    ret
ChatSidebar_StartInference ENDP

; -----------------------------------------------------------------------------
; Public API: SlashCommand_Register
; RCX=name, RDX=description, R8=promptTemplate, R9=handlerPtr
; -----------------------------------------------------------------------------
PUBLIC SlashCommand_Register
SlashCommand_Register PROC
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 32

    mov r12, rcx
    mov r13, rdx
    mov r14, r8

    mov rbx, qword ptr [g_slashCmdCount]
    cmp rbx, SLASH_CMD_MAX
    jae slash_reg_fail

    imul rbx, SC_sizeof
    lea r10, g_slashCommands
    lea r10, [r10+rbx]

    ; name
    mov rcx, r12
    lea rdx, [r10+SC_name]
    mov r8d, SLASH_CMD_NAME_MAX
    call String_CopyNA

    ; desc
    mov rcx, r13
    lea rdx, [r10+SC_description]
    mov r8d, SLASH_CMD_DESC_MAX
    call String_CopyNA

    ; prompt template (original R8)
    mov rcx, r14
    lea rdx, [r10+SC_promptTemplate]
    mov r8d, SLASH_CMD_TEMPLATE_MAX
    call String_CopyNA

    mov qword ptr [r10+SC_handlerPtr], r9
    mov byte ptr [r10+SC_requiresContext], 1

    inc qword ptr [g_slashCmdCount]
    mov eax, TRUE
    jmp slash_reg_done

slash_reg_fail:
    xor eax, eax

slash_reg_done:
    add rsp, 32
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
SlashCommand_Register ENDP

; -----------------------------------------------------------------------------
; Public API: SlashCommand_RegisterDefaults
; -----------------------------------------------------------------------------
PUBLIC SlashCommand_RegisterDefaults
SlashCommand_RegisterDefaults PROC
    sub rsp, 32

    lea rcx, szCmdTest
    lea rdx, szDescTest
    lea r8, szPromptTest
    xor r9d, r9d
    call SlashCommand_Register

    lea rcx, szCmdExplain
    lea rdx, szDescExplain
    lea r8, szPromptExplain
    xor r9d, r9d
    call SlashCommand_Register

    lea rcx, szCmdRefactor
    lea rdx, szDescRefactor
    lea r8, szPromptRefactor
    xor r9d, r9d
    call SlashCommand_Register

    lea rcx, szCmdFix
    lea rdx, szDescFix
    lea r8, szPromptFix
    xor r9d, r9d
    call SlashCommand_Register

    lea rcx, szCmdDoc
    lea rdx, szDescDoc
    lea r8, szPromptDoc
    xor r9d, r9d
    call SlashCommand_Register

    lea rcx, szCmdOptimize
    lea rdx, szDescOptimize
    lea r8, szPromptOptimize
    xor r9d, r9d
    call SlashCommand_Register

    add rsp, 32
    ret
SlashCommand_RegisterDefaults ENDP

; -----------------------------------------------------------------------------
; Public API: SlashCommand_ShowHelp
; -----------------------------------------------------------------------------
PUBLIC SlashCommand_ShowHelp
SlashCommand_ShowHelp PROC
    push rbx
    sub rsp, 32

    lea rcx, szHelpHeader
    lea rdx, g_helpTextBuf
    call String_CopyA
    mov rbx, rax

    xor r10, r10
help_loop:
    cmp r10, qword ptr [g_slashCmdCount]
    jae help_done

    mov word ptr [rbx], 0A0Dh
    add rbx, 2

    mov rax, r10
    imul rax, SC_sizeof
    lea rdx, g_slashCommands
    lea rcx, [rdx+rax+SC_name]
    mov rdx, rbx
    call String_CopyA
    mov rbx, rax

    mov byte ptr [rbx], ':'
    mov byte ptr [rbx+1], ' '
    add rbx, 2

    mov rax, r10
    imul rax, SC_sizeof
    lea rdx, g_slashCommands
    lea rcx, [rdx+rax+SC_description]
    mov rdx, rbx
    call String_CopyA
    mov rbx, rax

    inc r10
    jmp help_loop

help_done:
    mov ecx, 2
    lea rdx, g_helpTextBuf
    lea rcx, g_helpTextBuf
    call String_LengthA
    mov r8, rax
    mov ecx, 2
    lea rdx, g_helpTextBuf
    call ChatSidebar_AddMessage

    add rsp, 32
    pop rbx
    ret
SlashCommand_ShowHelp ENDP

; -----------------------------------------------------------------------------
; Public API: SlashCommand_Execute
; RCX=input, RDX=inputLen
; -----------------------------------------------------------------------------
PUBLIC SlashCommand_Execute
SlashCommand_Execute PROC
    push rbx
    sub rsp, 64

    mov rbx, rcx
    lea r9, scratchA
    cmp byte ptr [rbx], '/'
    jne slash_exec_fail
    inc rbx

    ; extract command token into scratchA
    xor r10d, r10d
parse_cmd:
    mov al, byte ptr [rbx+r10]
    test al, al
    je cmd_token_done
    cmp al, ' '
    je cmd_token_done
    cmp r10d, 250
    jae cmd_token_done
    mov byte ptr [r9+r10], al
    inc r10
    jmp parse_cmd
cmd_token_done:
    mov byte ptr [r9+r10], 0

    xor r11, r11
search_cmd:
    cmp r11, qword ptr [g_slashCmdCount]
    jae cmd_not_found

    mov rax, r11
    imul rax, SC_sizeof
    lea rdx, g_slashCommands
    lea rcx, [rdx+rax+SC_name]
    mov rdx, r9
    call String_EqualsA
    test eax, eax
    jnz cmd_found

    inc r11
    jmp search_cmd

cmd_found:
    ; Build a prompt from template + args tail
    mov rax, r11
    imul rax, SC_sizeof
    lea rdx, g_slashCommands
    lea rcx, [rdx+rax+SC_promptTemplate]
    lea rdx, g_slashPromptBuf
    call String_CopyA
    mov rdx, rax

    ; find args start after command token and optional space
    add rbx, r10
    cmp byte ptr [rbx], ' '
    jne @F
    inc rbx
@@:
    mov rcx, rbx
    call String_LengthA
    test rax, rax
    jz no_args

    ; append newline + args
    mov byte ptr [rdx], 13
    mov byte ptr [rdx+1], 10
    add rdx, 2
    mov rcx, rbx
    call String_CopyA

no_args:
    lea rcx, g_slashPromptBuf
    lea rcx, g_slashPromptBuf
    call String_LengthA
    mov rdx, rax
    lea rcx, g_slashPromptBuf
    call ChatSidebar_StartInference
    jmp slash_exec_ok

cmd_not_found:
    call SlashCommand_ShowHelp
    jmp slash_exec_ok

slash_exec_fail:
    xor eax, eax
    jmp slash_exec_done

slash_exec_ok:
    mov eax, TRUE

slash_exec_done:
    add rsp, 64
    pop rbx
    ret
SlashCommand_Execute ENDP

; -----------------------------------------------------------------------------
; Public API: HTTP_PostOllama
; RCX=jsonPayloadA, RDX=responseBuf, R8=responseBufSize
; RAX=bytes or -1
; -----------------------------------------------------------------------------
PUBLIC HTTP_PostOllama
HTTP_PostOllama PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 96

    mov rbx, rcx ; payload
    mov rsi, rdx ; response buf
    mov rdi, r8  ; response cap

    ; session
    lea rcx, wszAgent
    mov edx, WINHTTP_ACCESS_TYPE_NO_PROXY
    xor r8d, r8d
    xor r9d, r9d
    mov dword ptr [rsp+32], 0
    call qword ptr [__imp_WinHttpOpen]
    mov r12, rax
    test r12, r12
    jz http_fail

    ; connect
    mov rcx, r12
    lea rdx, wszHost
    mov r8d, OLLAMA_PORT
    xor r9d, r9d
    call qword ptr [__imp_WinHttpConnect]
    mov r13, rax
    test r13, r13
    jz http_cleanup_session

    ; open request
    mov rcx, r13
    lea rdx, wszPost
    lea r8, wszPathGenerate
    xor r9d, r9d
    mov qword ptr [rsp+32], WINHTTP_NO_REFERER
    mov qword ptr [rsp+40], WINHTTP_DEFAULT_ACCEPT_TYPES
    mov qword ptr [rsp+48], 0
    call qword ptr [__imp_WinHttpOpenRequest]
    mov r14, rax
    test r14, r14
    jz http_cleanup_connect

    ; add header
    mov rcx, r14
    lea rdx, wszJsonHeader
    mov r8, -1
    mov r9d, WINHTTP_ADDREQ_FLAG_ADD
    call qword ptr [__imp_WinHttpAddRequestHeaders]

    ; payload len
    mov rcx, rbx
    call String_LengthA
    mov r15, rax

    ; send request
    mov rcx, r14
    xor edx, edx
    xor r8d, r8d
    mov r9, rbx
    mov qword ptr [rsp+32], r15
    mov qword ptr [rsp+40], r15
    mov qword ptr [rsp+48], 0
    call qword ptr [__imp_WinHttpSendRequest]
    test eax, eax
    jz http_cleanup_request

    ; receive response
    mov rcx, r14
    xor rdx, rdx
    call qword ptr [__imp_WinHttpReceiveResponse]
    test eax, eax
    jz http_cleanup_request

    xor r15, r15 ; total bytes
http_read_loop:
    mov rcx, r14
    lea rdx, [rsp+64] ; avail DWORD
    call qword ptr [__imp_WinHttpQueryDataAvailable]
    test eax, eax
    jz http_read_done

    mov eax, dword ptr [rsp+64]
    test eax, eax
    jz http_read_done

    mov r10d, eax
    mov r11, rdi
    sub r11, r15
    test r11, r11
    jle http_read_done
    cmp r10, r11
    jbe @F
    mov r10, r11
@@:
    mov rcx, r14
    lea rdx, [rsi+r15]
    mov r8d, r10d
    lea r9, [rsp+68] ; read DWORD
    call qword ptr [__imp_WinHttpReadData]
    test eax, eax
    jz http_read_done

    mov eax, dword ptr [rsp+68]
    add r15, rax
    jmp http_read_loop

http_read_done:
    ; NUL terminate if space
    cmp r15, rdi
    jae @F
    mov byte ptr [rsi+r15], 0
@@:
    mov rax, r15
    jmp http_cleanup

http_cleanup_request:
    mov rcx, r14
    call qword ptr [__imp_WinHttpCloseHandle]
http_cleanup_connect:
    mov rcx, r13
    call qword ptr [__imp_WinHttpCloseHandle]
http_cleanup_session:
    mov rcx, r12
    call qword ptr [__imp_WinHttpCloseHandle]
http_fail:
    mov rax, -1
http_cleanup:
    add rsp, 96
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
HTTP_PostOllama ENDP

; -----------------------------------------------------------------------------
; JSON helper: extract first "response":"..." to dest
; RCX=srcJson, RDX=dest, R8=destCap
; -----------------------------------------------------------------------------
Json_ExtractResponseA PROC
    push rbx
    push rsi
    push rdi

    mov rsi, rcx
    mov rdi, rdx
    mov rbx, r8

find_key:
    mov al, byte ptr [rsi]
    test al, al
    je no_key
    cmp al, '"'
    jne adv
    ; cheap match "response":"
    cmp byte ptr [rsi+1], 'r'
    jne adv
    cmp byte ptr [rsi+2], 'e'
    jne adv
    cmp byte ptr [rsi+3], 's'
    jne adv
    cmp byte ptr [rsi+4], 'p'
    jne adv
    cmp byte ptr [rsi+5], 'o'
    jne adv
    cmp byte ptr [rsi+6], 'n'
    jne adv
    cmp byte ptr [rsi+7], 's'
    jne adv
    cmp byte ptr [rsi+8], 'e'
    jne adv
    cmp byte ptr [rsi+9], '"'
    jne adv
    cmp byte ptr [rsi+10], ':'
    jne adv
    cmp byte ptr [rsi+11], '"'
    jne adv
    add rsi, 12
    jmp copy_val
adv:
    inc rsi
    jmp find_key

copy_val:
    test rbx, rbx
    jz done
    dec rbx
copy_loop:
    test rbx, rbx
    jz term
    mov al, byte ptr [rsi]
    test al, al
    je term
    cmp al, '"'
    je term
    cmp al, 5Ch
    jne plain
    ; skip escape slash, copy next char if present
    inc rsi
    mov al, byte ptr [rsi]
    test al, al
    je term
plain:
    mov byte ptr [rdi], al
    inc rdi
    inc rsi
    dec rbx
    jmp copy_loop
term:
    mov byte ptr [rdi], 0
    jmp done

no_key:
    mov byte ptr [rdi], 0

done:
    pop rdi
    pop rsi
    pop rbx
    ret
Json_ExtractResponseA ENDP

; -----------------------------------------------------------------------------
; Static JSON fragments
; -----------------------------------------------------------------------------
.data
szJsonPrefixA                db '{"model":"phi3","prompt":"',0
szJsonSuffixA                db '","stream":false}',0
szEmpty                      db 0
szInferenceFail              db "[system] inference failed",0

END
