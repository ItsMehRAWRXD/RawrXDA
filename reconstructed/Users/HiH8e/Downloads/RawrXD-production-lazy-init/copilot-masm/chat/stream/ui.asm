;======================================================================
; chat_stream_ui.asm - Real-Time Chat Streaming UI
;======================================================================
INCLUDE windows.inc

.CONST
WM_STREAM_TOKEN          EQU WM_USER + 200
STREAM_BUFFER_SIZE       EQU 8192
CALL_BUFFER_SIZE         EQU 65536

.DATA?
hChatPanel               QWORD ?
hRichEdit                QWORD ?
streamBuffer             DB STREAM_BUFFER_SIZE DUP(?)
streamPos                DWORD ?
isStreaming              DWORD ?
typingAnimationFrame     DWORD ?
; Thinking UI
hThinkingBox             QWORD ?
thinkingVisible          DWORD ?
g_thinkingEnabled       DWORD 1    ; enabled by default
thinkingBuffer          DB 4096 DUP(?)
thinkingPos             DWORD ?
currentCallBuffer       DB CALL_BUFFER_SIZE DUP(?)
currentCallPos          DWORD ?
currentCallActive       DWORD ?

.DATA
THINKING_STR            DB "Thinking...",0
EDIT_CLASS              DB "EDIT",0
EMPTY_STR               DB "",0
CONSOLAS_STR            DB "Consolas",0
SEGOE_UI_STR            DB "Segoe UI",0

.CODE

ChatStreamManager_Init PROC hPanel:QWORD, hRichEditCtrl:QWORD
    mov hChatPanel, hPanel
    mov hRichEdit, hRichEditCtrl
    mov streamPos, 0
    mov isStreaming, 0
    ret
ChatStreamManager_Init ENDP

ChatStreamManager_OnToken PROC pToken:QWORD, tokenLen:DWORD
    LOCAL pAppend:QWORD
    
    ; Check if we need to allocate more space
    mov eax, streamPos
    add eax, tokenLen
    .if eax >= STREAM_BUFFER_SIZE
        ; Flush current buffer to UI
        invoke ChatStreamManager_FlushBuffer
    .endif
    
    ; If a call session is active, append to call buffer (preferred)
    mov eax, dword ptr currentCallActive
    .if eax != 0
        ; Append token to current call buffer
        mov rcx, currentCallPos
        lea rax, currentCallBuffer
        add rax, rcx
        mov pAppend, rax
        mov rcx, pAppend
        mov rdx, pToken
        mov r8d, tokenLen
        call RtlCopyMemory
        add currentCallPos, tokenLen
        ; Update thinking box if visible
        mov eax, dword ptr thinkingVisible
        .if eax != 0
            mov rcx, hThinkingBox
            mov rdx, currentCallBuffer
            call SetWindowTextA
            ret
        .endif
        ret
    .endif

    ; If thinking is enabled and visible, write to thinking buffer first
    mov eax, dword ptr g_thinkingEnabled
    .if eax != 0
        mov eax, thinkingVisible
        .if eax != 0
            ; Append token to thinking buffer
            mov rcx, thinkingPos
            lea rax, thinkingBuffer
            add rax, rcx
            mov pAppend, rax
            mov rcx, pAppend
            mov rdx, pToken
            mov r8d, tokenLen
            call RtlCopyMemory
            add thinkingPos, tokenLen
            ; Update thinking control text (simple strategy: replace)
            mov rcx, hThinkingBox
            mov rdx, thinkingBuffer
            call SetWindowTextA
            ret
        .endif
    .endif

    ; Append token to stream buffer
    mov rcx, streamPos
    lea rax, streamBuffer
    add rax, rcx
    mov pAppend, rax

    invoke RtlCopyMemory, pAppend, pToken, tokenLen

    add streamPos, tokenLen
    mov isStreaming, 1
    
    ; Post message to UI thread
    mov rcx, hChatPanel
    mov edx, WM_STREAM_TOKEN
    mov r8, pToken
    mov r9d, tokenLen
    call PostMessageA
    
    ret
ChatStreamManager_OnToken ENDP

; Adapter so HTTP client code can call ChatStream_OnToken
ChatStream_OnToken PROC pToken:QWORD, tokenLen:DWORD
    invoke ChatStreamManager_OnToken, pToken, tokenLen
    ret
ChatStream_OnToken ENDP

ChatStreamManager_FlushBuffer PROC
    .if streamPos == 0
        ret
    .endif
    
    ; Send EM_SETSEL to move caret to end
    invoke SendMessageA, hRichEdit, EM_SETSEL, -1, -1
    
    ; Send EM_REPLACESEL to append text
    invoke SendMessageA, hRichEdit, EM_REPLACESEL, 0, OFFSET streamBuffer
    
    ; Clear buffer
    mov streamPos, 0
    
    ret
ChatStreamManager_FlushBuffer ENDP

ChatStreamManager_StartCall PROC pModelName:QWORD
    mov currentCallPos, 0
    mov currentCallActive, 1
    ; If thinking enabled, create thinking box placeholder
    mov eax, dword ptr g_thinkingEnabled
    .if eax != 0
        lea rcx, THINKING_STR
        call ChatStreamManager_ShowThinking
    .endif
    ret
ChatStreamManager_StartCall ENDP

ChatStreamManager_FinishCall PROC success:DWORD
    mov eax, ecx
    .if eax != 0
        ; Append the collected call buffer to the main chat area
        ; Move caret to end and append the collected buffer
        invoke SendMessageA, hRichEdit, EM_SETSEL, -1, -1
        invoke SendMessageA, hRichEdit, EM_REPLACESEL, 0, OFFSET currentCallBuffer
    .endif
    ; Clear call buffer and state
    mov currentCallPos, 0
    mov currentCallActive, 0
    ; Hide thinking UI
    invoke ChatStreamManager_HideThinking
    ret
ChatStreamManager_FinishCall ENDP

ChatStreamManager_ShowTypingAnimation PROC
    LOCAL hDC:QWORD
    LOCAL rect:RECT
    
    mov rcx, hRichEdit
    call GetDC
    
    mov hDC, rax
    lea rcx, rect
    call GetClientRect
    
    ; Draw typing dots animation
    mov rcx, hDC
    mov edx, rect.right
    sub edx, 60
    mov r8d, rect.bottom
    sub r8d, 20
    invoke DrawTypingDots, rdx, r8
    
    call ReleaseDC, hRichEdit, hDC
    
    ret
ChatStreamManager_ShowTypingAnimation ENDP

ChatStreamManager_ShowThinking PROC pThinkingText:QWORD
    LOCAL hCtl:QWORD

    ; Create thinking box if not present
    mov eax, thinkingVisible
    .if eax == 0
        ; CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, width, height, hWndParent, hMenu, hInstance, lpParam)
        mov rcx, WS_EX_CLIENTEDGE
        lea rdx, EDIT_CLASS
        lea r8, EMPTY_STR
        mov r9d, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | WS_VSCROLL
        push 0
        push ghInstance
        push IDC_THINKING_BOX
        push hChatPanel
        push 120
        push 580
        push 10
        push 10
        call CreateWindowExA
        add rsp, 8*8
        mov hCtl, rax
        mov qword ptr hThinkingBox, rax
        ; Set monospace font (Consolas) for code-style look
        mov rcx, hCtl
        ; CreateFontA parameters: nHeight, nWidth, nEscapement, nOrientation, fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut, fdwCharSet, fdwOutputPrecision, fdwClipPrecision, fdwQuality, fdwPitchAndFamily, lpszFace
        mov rdx, 0
        mov r8, 0
        mov r9, 0
        push OFFSET CONSOLAS_STR
        push (FIXED_PITCH or FF_MODERN)
        push DEFAULT_QUALITY
        push CLIP_DEFAULT_PRECIS
        push OUT_DEFAULT_PRECIS
        push DEFAULT_CHARSET
        push 0
        push 0
        push 0
        push 16
        call CreateFontA
        add rsp, 8*10
        mov rdx, rax
        mov rcx, hCtl
        mov r8, rax
        mov r9d, TRUE
        call SendMessageA
        mov thinkingVisible, 1
    .endif

    ; Set initial text
    mov rcx, hThinkingBox
    mov rdx, pThinkingText
    call SetWindowTextA
    ; Reset thinking buffer state
    mov thinkingPos, 0
    ret
ChatStreamManager_ShowThinking ENDP

ChatStreamManager_HideThinking PROC
    mov eax, thinkingVisible
    .if eax != 0
        mov rcx, hThinkingBox
        call DestroyWindow
        mov thinkingVisible, 0
    .endif
    ret
ChatStreamManager_HideThinking ENDP

ChatStreamManager_SetThinkingToggle PROC enabled:DWORD
    mov dword ptr g_thinkingEnabled, ecx
    ret
ChatStreamManager_SetThinkingToggle ENDP

; Apply message styling: white background, black text, prefer UTF-8-capable control (RichEdit recommended)
ChatStreamManager_ApplyMessageStyle PROC hTarget:QWORD
    ; NOTE: For true UTF-8 support, use RichEdit (Unicode) and wide strings. This is a placeholder that sets a reasonable font.
    mov rcx, hTarget
    invoke CreateFontA, 12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 0, `Segoe UI`
    mov rdx, rax
    call SendMessageA, rcx, WM_SETFONT, rdx, TRUE
    ret
ChatStreamManager_ApplyMessageStyle ENDP

ChatStreamManager_FinishStream PROC
    mov isStreaming, 0
    invoke ChatStreamManager_FlushBuffer
    
    ; Hide typing indicator
    call InvalidateRect, hRichEdit, 0, TRUE
    
    ret
ChatStreamManager_FinishStream ENDP

END
