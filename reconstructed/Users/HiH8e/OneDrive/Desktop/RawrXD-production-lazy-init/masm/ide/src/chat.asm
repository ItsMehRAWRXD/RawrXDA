; ============================================================================
; RawrXD Agentic IDE - Chat Panel Implementation (Pure MASM)
; Real-time LLM chat interface
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comctl32.inc
include \masm32\include\richedit.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comctl32.lib
includelib \masm32\lib\riched20.lib

.data
include constants.inc
include structures.inc
include macros.inc

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    szChatClass       db "RichEdit20W", 0
    szInputClass      db "EDIT", 0
    szSendButton      db "Send", 0
    szClearButton     db "Clear", 0
    szUserPrefix      db "User: ", 0
    szAgentPrefix     db "Agent: ", 0
    szThinking        db "Thinking...", 13, 10, 0
    
    ; Chat state
    g_hChatPanel      dd 0
    g_hChatInput      dd 0
    g_hSendButton     dd 0
    g_hClearButton    dd 0
    g_bChatConnected  dd 0

.data?
    g_szChatHistory   db 16384 dup(?)

; ============================================================================
; PROCEDURES
; ============================================================================

; ============================================================================
; CreateChatPanel - Create chat panel
; Returns: Panel handle in eax
; ============================================================================
CreateChatPanel proc
    LOCAL dwStyle:DWORD
    LOCAL hChat:DWORD
    LOCAL hInput:DWORD
    LOCAL hSend:DWORD
    LOCAL hClear:DWORD
    
    ; Create chat display (RichEdit)
    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_AUTOVSCROLL or ES_READONLY
    
    CreateWnd szChatClass, NULL, dwStyle, 0, 0, 400, 300, hMainWindow, IDC_CHAT
    mov hChat, eax
    mov g_hChatPanel, eax
    
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Set font
    invoke SendMessage, hChat, WM_SETFONT, hMainFont, TRUE
    
    ; Create input field
    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_AUTOHSCROLL
    
    CreateWnd szInputClass, NULL, dwStyle, 0, 300, 300, 25, hMainWindow, 0
    mov hInput, eax
    mov g_hChatInput, eax
    
    ; Set font
    invoke SendMessage, hInput, WM_SETFONT, hMainFont, TRUE
    
    ; Create send button
    mov dwStyle, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    
    CreateWnd "BUTTON", addr szSendButton, dwStyle, 300, 300, 50, 25, hMainWindow, 0
    mov hSend, eax
    mov g_hSendButton, eax
    
    ; Create clear button
    CreateWnd "BUTTON", addr szClearButton, dwStyle, 350, 300, 50, 25, hMainWindow, 0
    mov hClear, eax
    mov g_hClearButton, eax
    
    ; Initialize chat
    call Chat_Initialize
    
    mov eax, hChat
    ret
CreateChatPanel endp

; ============================================================================
; Chat_Initialize - Initialize chat panel
; ============================================================================
Chat_Initialize proc
    LOCAL szWelcome db 256 dup(0)
    
    invoke wsprintfA, addr szWelcome, addr szWelcomeFormat, addr szAgentPrefix
    invoke SendMessage, g_hChatPanel, WM_SETTEXT, 0, addr szWelcome
    
    ret
Chat_Initialize endp

; ============================================================================
; Chat_SendMessage - Send message to chat
; Input: pszMessage
; ============================================================================
Chat_SendMessage proc pszMessage:DWORD
    LOCAL szFullMessage db 512 dup(0)
    
    ; Add user prefix
    szCopy addr szFullMessage, addr szUserPrefix
    szCat addr szFullMessage, pszMessage
    szCat addr szFullMessage, addr szNewLine
    
    ; Append to chat
    invoke Chat_AppendText, addr szFullMessage
    
    ; Clear input
    invoke SendMessage, g_hChatInput, WM_SETTEXT, 0, addr szEmpty
    
    ; Show thinking
    invoke Chat_AppendText, addr szThinking
    
    ; Send to agent (async)
    invoke Chat_SendToAgent, pszMessage
    
    ret
Chat_SendMessage endp

; ============================================================================
; Chat_SendToAgent - Send message to agent (async)
; Input: pszMessage
; ============================================================================
Chat_SendToAgent proc pszMessage:DWORD
    LOCAL hThread:DWORD
    
    invoke CreateThread, NULL, 0, offset ChatAgentWorker, pszMessage, 0, NULL
    mov hThread, eax
    
    .if hThread != 0
        invoke CloseHandle, hThread
    .endif
    
    ret
Chat_SendToAgent endp

; ============================================================================
; ChatAgentWorker - Worker thread for agent response
; ============================================================================
ChatAgentWorker proc pszMessage:DWORD
    LOCAL szResponse db 1024 dup(0)
    LOCAL szFullResponse db 1536 dup(0)
    
    ; Simulate agent thinking (would call ModelInvoker)
    invoke Sleep, 1000
    
    ; Generate response (placeholder)
    invoke wsprintfA, addr szResponse, addr szResponseFormat, pszMessage
    
    ; Build full response with prefix
    szCopy addr szFullResponse, addr szAgentPrefix
    szCat addr szFullResponse, addr szResponse
    szCat addr szFullResponse, addr szNewLine
    
    ; Remove thinking message and add response
    invoke Chat_ReplaceLastLine, addr szFullResponse
    
    ret
ChatAgentWorker endp

; ============================================================================
; Chat_AppendText - Append text to chat
; Input: pszText
; ============================================================================
Chat_AppendText proc pszText:DWORD
    LOCAL dwTextLength:DWORD
    LOCAL dwCurrentLength:DWORD
    
    ; Get current text length
    invoke SendMessage, g_hChatPanel, WM_GETTEXTLENGTH, 0, 0
    mov dwCurrentLength, eax
    
    ; Get text to append length
    szLen pszText
    mov dwTextLength, eax
    
    ; Append text
    invoke SendMessage, g_hChatPanel, EM_SETSEL, dwCurrentLength, dwCurrentLength
    invoke SendMessage, g_hChatPanel, EM_REPLACESEL, FALSE, pszText
    
    ; Scroll to bottom
    invoke SendMessage, g_hChatPanel, EM_SCROLL, SB_BOTTOM, 0
    
    ret
Chat_AppendText endp

; ============================================================================
; Chat_ReplaceLastLine - Replace last line in chat
; Input: pszNewText
; ============================================================================
Chat_ReplaceLastLine proc pszNewText:DWORD
    LOCAL dwTextLength:DWORD
    LOCAL dwLastLineStart:DWORD
    LOCAL dwLastLineLength:DWORD
    
    ; Get text length
    invoke SendMessage, g_hChatPanel, WM_GETTEXTLENGTH, 0, 0
    mov dwTextLength, eax
    
    ; Find start of last line
    invoke SendMessage, g_hChatPanel, EM_LINEINDEX, -1, 0
    mov dwLastLineStart, eax
    
    ; Get last line length
    invoke SendMessage, g_hChatPanel, EM_LINELENGTH, dwLastLineStart, 0
    mov dwLastLineLength, eax
    
    ; Select last line
    invoke SendMessage, g_hChatPanel, EM_SETSEL, dwLastLineStart, dwLastLineStart + dwLastLineLength
    
    ; Replace with new text
    invoke SendMessage, g_hChatPanel, EM_REPLACESEL, FALSE, pszNewText
    
    ret
Chat_ReplaceLastLine endp

; ============================================================================
; Chat_Clear - Clear chat history
; ============================================================================
Chat_Clear proc
    invoke Chat_Initialize
    ret
Chat_Clear endp

; ============================================================================
; Chat_GetHistory - Get chat history
; Returns: Pointer to history in eax
; ============================================================================
Chat_GetHistory proc
    invoke SendMessage, g_hChatPanel, WM_GETTEXT, sizeof g_szChatHistory, addr g_szChatHistory
    lea eax, g_szChatHistory
    ret
Chat_GetHistory endp

; ============================================================================
; Chat_SetAgentResponse - Set agent response (for external use)
; Input: pszResponse
; ============================================================================
Chat_SetAgentResponse proc pszResponse:DWORD
    LOCAL szFullResponse db 1536 dup(0)
    
    ; Build full response
    szCopy addr szFullResponse, addr szAgentPrefix
    szCat addr szFullResponse, pszResponse
    szCat addr szFullResponse, addr szNewLine
    
    ; Replace thinking message
    invoke Chat_ReplaceLastLine, addr szFullResponse
    
    ret
Chat_SetAgentResponse endp

; ============================================================================
; Chat_OnSendButton - Handle send button click
; ============================================================================
Chat_OnSendButton proc
    LOCAL szMessage db 512 dup(0)
    
    ; Get message from input
    invoke SendMessage, g_hChatInput, WM_GETTEXT, sizeof szMessage, addr szMessage
    
    ; Check if message is not empty
    szLen addr szMessage
    .if eax > 0
        invoke Chat_SendMessage, addr szMessage
    .endif
    
    ret
Chat_OnSendButton endp

; ============================================================================
; Chat_OnClearButton - Handle clear button click
; ============================================================================
Chat_OnClearButton proc
    invoke Chat_Clear
    ret
Chat_OnClearButton endp

; ============================================================================
; Chat_Cleanup - Cleanup chat resources
; ============================================================================
Chat_Cleanup proc
    ret
Chat_Cleanup endp

; ============================================================================
; Data for chat
; ============================================================================

.data
    szWelcomeFormat   db "%sWelcome to RawrXD Agentic Chat!", 13, 10
                      db "Type a message and press Send to chat with the AI agent.", 13, 10, 13, 10, 0
    szResponseFormat  db "I received your message: '%s'. How can I help you further?", 0
    szEmpty           db "", 0
    szNewLine         db 13, 10, 0

end
