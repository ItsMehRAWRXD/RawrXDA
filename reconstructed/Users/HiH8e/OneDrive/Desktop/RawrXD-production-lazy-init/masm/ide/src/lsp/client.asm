; ============================================================================
; RawrXD Agentic IDE - LSP Client Implementation
; Pure MASM - Language Server Protocol Client
; ⚠️  STUB/DISABLED: LSP networking requires WinSock2 which is not currently
;     integrated into the MASM32 include path. LSP client stub returns success
;     but provides no actual protocol communication.
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc

includelib \masm32\lib\kernel32.lib

; NOTE: WinSock2 support deferred

; Define path size for structures used in this module
.const
    MAX_PATH_SIZE EQU 260

; ============================================================================
; Constants for LSP
; ============================================================================

; LSP message types
LSP_TYPE_REQUEST        equ 1
LSP_TYPE_RESPONSE       equ 2
LSP_TYPE_NOTIFICATION   equ 3

; LSP methods
LSP_METHOD_INITIALIZE   equ 1
LSP_METHOD_INITIALIZED  equ 2
LSP_METHOD_TEXTDOC_DIDOPEN equ 3
LSP_METHOD_TEXTDOC_DIDCHANGE equ 4
LSP_METHOD_TEXTDOC_HOVER equ 5
LSP_METHOD_TEXTDOC_DEFINITION equ 6
LSP_METHOD_TEXTDOC_REFERENCES equ 7
LSP_METHOD_TEXTDOC_COMPLETION equ 8
LSP_METHOD_SHUTDOWN     equ 9
LSP_METHOD_EXIT         equ 10

; LSP error codes
LSP_ERROR_PARSE         equ -32700
LSP_ERROR_INVALID_REQUEST equ -32600
LSP_ERROR_METHOD_NOT_FOUND equ -32601
LSP_ERROR_INVALID_PARAMS equ -32602
LSP_ERROR_INTERNAL_ERROR equ -32603

; ============================================================================
; Structures for LSP
; ============================================================================

LSP_HEADER struct
    szContentLength db 32 dup(?)  ; Content-Length: \d+\r\n
    szContentType   db 64 dup(?)  ; Content-Type: application/vscode-jsonrpc\r\n
    szSeparator     db 2 dup(?)   ; \r\n
LSP_HEADER ends

LSP_MESSAGE struct
    dwType          dd ?    ; Request, Response, or Notification
    dwMethod        dd ?    ; Method identifier
    dwId            dd ?    ; Message ID (for requests/responses)
    szMethodStr     db 64 dup(?)  ; Method string
    szParams        db 2048 dup(?) ; JSON parameters
    szResult        db 4096 dup(?) ; JSON result
    dwErrorCode     dd ?    ; Error code (if applicable)
    szErrorMsg      db 256 dup(?)  ; Error message
LSP_MESSAGE ends

LSP_CLIENT struct
    hSocket         dd ?    ; Socket handle
    szServerAddr    db 64 dup(?)   ; Server address
    dwServerPort    dd ?    ; Server port
    dwNextId        dd ?    ; Next message ID
    bConnected      dd ?    ; Connection status
    szWorkspace     db MAX_PATH_SIZE dup(?) ; Workspace root
    szLanguageId    db 32 dup(?)   ; Language identifier
    dwProcessId     dd ?    ; Client process ID
    hReadThread     dd ?    ; Read thread handle
    bReading        dd ?    ; Reading flag
LSP_CLIENT ends

LSP_DOCUMENT struct
    szFilePath      db MAX_PATH_SIZE dup(?) ; File path
    szLanguageId    db 32 dup(?)   ; Language identifier
    dwVersion       dd ?    ; Document version
    szContent       db 65536 dup(?) ; Document content
    dwLineCount     dd ?    ; Number of lines
    bOpened         dd ?    ; Document opened flag
LSP_DOCUMENT ends

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    szLSPContentType    db "Content-Type: application/vscode-jsonrpc", 13, 10, 0
    szLSPContentLength  db "Content-Length: ", 0
    szLSPSeparator      db 13, 10, 0
    szLSPInitialize     db "initialize", 0
    szLSPInitialized    db "initialized", 0
    szLSPTextDocDidOpen db "textDocument/didOpen", 0
    szLSPTextDocDidChange db "textDocument/didChange", 0
    szLSPTextDocHover   db "textDocument/hover", 0
    szLSPTextDocDef     db "textDocument/definition", 0
    szLSPTextDocRefs    db "textDocument/references", 0
    szLSPTextDocComp    db "textDocument/completion", 0
    szLSPShutdown       db "shutdown", 0
    szLSPExit           db "exit", 0
    
    ; Default LSP server settings
    szDefaultServer     db "127.0.0.1", 0
    dwDefaultPort       dd 9000
    szDefaultLanguage   db "cpp", 0
    szDefaultWorkspace  db ".", 0
    
    ; Error messages
    szLSPErrorConnect   db "Failed to connect to LSP server", 0
    szLSPErrorSend      db "Failed to send LSP message", 0
    szLSPErrorReceive   db "Failed to receive LSP message", 0
    szLSPErrorParse     db "Failed to parse LSP message", 0
    szLSPSuccess        db "LSP operation completed successfully", 0
    szQueryTitle        db "LSP Client", 0
    
    ; JSON templates
    szInitializeRequest db "{", 13, 10
                       db "  \"jsonrpc\": \"2.0\",", 13, 10
                       db "  \"id\": %d,", 13, 10
                       db "  \"method\": \"initialize\",", 13, 10
                       db "  \"params\": {", 13, 10
                       db "    \"processId\": %d,", 13, 10
                       db "    \"clientInfo\": {", 13, 10
                       db "      \"name\": \"RawrXD Agentic IDE\",", 13, 10
                       db "      \"version\": \"1.0.0\"", 13, 10
                       db "    },", 13, 10
                       db "    \"rootUri\": \"file://%s\",", 13, 10
                       db "    \"capabilities\": {", 13, 10
                       db "      \"textDocument\": {", 13, 10
                       db "        \"hover\": {", 13, 10
                       db "          \"dynamicRegistration\": true,", 13, 10
                       db "          \"contentFormat\": [\"markdown\", \"plaintext\"]", 13, 10
                       db "        }", 13, 10
                       db "      }", 13, 10
                       db "    }", 13, 10
                       db "  }", 13, 10
                       db "}", 0

.data?
    g_pLSPClient        dd ?    ; Pointer to current LSP client
    g_dwClientCount     dd ?    ; Number of LSP clients
    g_bWSAInitialized   dd ?    ; Winsock initialized flag

; ============================================================================
; PROCEDURES
; ============================================================================

; ============================================================================
; LSPClient_Init - Initialize LSP client system
; Returns: TRUE in eax on success
; ============================================================================
LSPClient_Init proc
    LOCAL wsaData:WSADATA
    LOCAL result:DWORD
    
    ; Initialize global variables
    mov g_pLSPClient, 0
    mov g_dwClientCount, 0
    mov g_bWSAInitialized, FALSE
    
    ; Initialize Winsock
    invoke WSAStartup, 0202h, addr wsaData
    mov result, eax
    test eax, eax
    jnz @Exit
    
    mov g_bWSAInitialized, TRUE
    mov eax, TRUE  ; Success
    
@Exit:
    ret
LSPClient_Init endp

; ============================================================================
; LSPClient_Create - Create a new LSP client
; Input: pszServerAddr, dwPort, pszWorkspace, pszLanguageId
; Returns: Pointer to LSP_CLIENT in eax, NULL on error
; ============================================================================
LSPClient_Create proc pszServerAddr:DWORD, dwPort:DWORD, pszWorkspace:DWORD, pszLanguageId:DWORD
    LOCAL pClient:DWORD
    LOCAL sin:SOCKADDR_IN
    
    ; Allocate client structure
    MemAlloc sizeof LSP_CLIENT
    mov pClient, eax
    test eax, eax
    jz @Exit
    
    ; Initialize structure
    MemZero pClient, sizeof LSP_CLIENT
    
    ; Copy parameters
    mov eax, pClient
    assume eax:ptr LSP_CLIENT
    szCopy addr [eax].szServerAddr, pszServerAddr
    mov ecx, dwPort
    mov [eax].dwServerPort, ecx
    szCopy addr [eax].szWorkspace, pszWorkspace
    szCopy addr [eax].szLanguageId, pszLanguageId
    invoke GetCurrentProcessId
    mov [eax].dwProcessId, eax
    mov [eax].dwNextId, 1
    mov [eax].bConnected, FALSE
    mov [eax].bReading, FALSE
    assume eax:nothing
    
    mov eax, pClient
    inc g_dwClientCount
    
@Exit:
    ret
LSPClient_Create endp

; ============================================================================
; LSPClient_Connect - Connect to LSP server
; Input: pClient
; Returns: TRUE in eax on success
; ============================================================================
LSPClient_Connect proc pClient:DWORD
    LOCAL sin:SOCKADDR_IN
    LOCAL hSocket:DWORD
    LOCAL result:DWORD
    
    ; Validate input
    .if pClient == 0
        xor eax, eax
        ret
    .endif
    
    ; Create socket
    invoke socket, AF_INET, SOCK_STREAM, 0
    mov hSocket, eax
    cmp eax, INVALID_SOCKET
    je @Error
    
    ; Set up server address
    MemZero addr sin, sizeof SOCKADDR_IN
    mov sin.sin_family, AF_INET
    mov eax, pClient
    assume eax:ptr LSP_CLIENT
    invoke inet_addr, addr [eax].szServerAddr
    mov sin.sin_addr, eax
    mov ecx, [eax].dwServerPort
    mov sin.sin_port, cx
    assume eax:nothing
    
    ; Connect to server
    invoke connect, hSocket, addr sin, sizeof SOCKADDR_IN
    mov result, eax
    test eax, eax
    jnz @ConnectError
    
    ; Store socket handle
    mov eax, pClient
    assume eax:ptr LSP_CLIENT
    mov [eax].hSocket, hSocket
    mov [eax].bConnected, TRUE
    assume eax:nothing
    
    mov eax, TRUE  ; Success
    jmp @Exit
    
@ConnectError:
    invoke closesocket, hSocket
    
@Error:
    mov eax, FALSE
    
@Exit:
    ret
LSPClient_Connect endp

; ============================================================================
; LSPClient_Disconnect - Disconnect from LSP server
; Input: pClient
; ============================================================================
LSPClient_Disconnect proc pClient:DWORD
    LOCAL hSocket:DWORD
    
    .if pClient != 0
        mov eax, pClient
        assume eax:ptr LSP_CLIENT
        mov eax, [eax].hSocket
        mov hSocket, eax
        assume eax:nothing
        
        .if hSocket != 0 && hSocket != INVALID_SOCKET
            invoke closesocket, hSocket
        .endif
        
        mov eax, pClient
        assume eax:ptr LSP_CLIENT
        mov [eax].hSocket, 0
        mov [eax].bConnected, FALSE
        assume eax:nothing
    .endif
    
    ret
LSPClient_Disconnect endp

; ============================================================================
; LSPClient_SendMessage - Send LSP message
; Input: pClient, pMessage
; Returns: TRUE in eax on success
; ============================================================================
LSPClient_SendMessage proc pClient:DWORD, pMessage:DWORD
    LOCAL szBuffer db 8192 dup(0)
    LOCAL szHeader db 256 dup(0)
    LOCAL dwLength:DWORD
    LOCAL dwSent:DWORD
    LOCAL result:DWORD
    
    ; Validate inputs
    .if pClient == 0 || pMessage == 0
        xor eax, eax
        ret
    .endif
    
    ; Check connection
    mov eax, pClient
    assume eax:ptr LSP_CLIENT
    .if [eax].bConnected == FALSE
        assume eax:nothing
        xor eax, eax
        ret
    .endif
    assume eax:nothing
    
    ; Format message (simplified JSON)
    mov eax, pMessage
    assume eax:ptr LSP_MESSAGE
    szCopy addr szBuffer, addr [eax].szParams
    assume eax:nothing
    
    ; Calculate content length
    szLen addr szBuffer
    mov dwLength, eax
    
    ; Format header
    szCopy addr szHeader, addr szLSPContentLength
    invoke wsprintf, addr szHeader[100], "%d", dwLength
    szCat addr szHeader, addr szHeader[100]
    szCat addr szHeader, addr szLSPSeparator
    szCat addr szHeader, addr szLSPContentType
    szCat addr szHeader, addr szLSPSeparator
    szCat addr szHeader, addr szLSPSeparator
    
    ; Send header
    szLen addr szHeader
    invoke send, [eax].hSocket, addr szHeader, eax, 0
    mov result, eax
    cmp eax, SOCKET_ERROR
    je @Error
    
    ; Send content
    invoke send, [eax].hSocket, addr szBuffer, dwLength, 0
    mov result, eax
    cmp eax, SOCKET_ERROR
    je @Error
    
    mov eax, TRUE  ; Success
    jmp @Exit
    
@Error:
    mov eax, FALSE
    
@Exit:
    ret
LSPClient_SendMessage endp

; ============================================================================
; LSPClient_Initialize - Send initialize request
; Input: pClient
; Returns: TRUE in eax on success
; ============================================================================
LSPClient_Initialize proc pClient:DWORD
    LOCAL message:LSP_MESSAGE
    LOCAL szRequest db 2048 dup(0)
    LOCAL pClientPtr:DWORD
    LOCAL dwProcessId:DWORD
    LOCAL szWorkspace db MAX_PATH_SIZE dup(0)
    
    ; Validate input
    .if pClient == 0
        xor eax, eax
        ret
    .endif
    
    mov pClientPtr, pClient
    
    ; Get client info
    mov eax, pClient
    assume eax:ptr LSP_CLIENT
    mov eax, [eax].dwProcessId
    mov dwProcessId, eax
    szCopy addr szWorkspace, addr [eax].szWorkspace
    assume eax:nothing
    
    ; Format initialize request
    invoke wsprintf, addr szRequest, addr szInitializeRequest, 1, dwProcessId, addr szWorkspace
    szCopy addr message.szParams, addr szRequest
    mov message.dwType, LSP_TYPE_REQUEST
    mov message.dwMethod, LSP_METHOD_INITIALIZE
    mov message.dwId, 1
    
    ; Send message
    invoke LSPClient_SendMessage, pClientPtr, addr message
    ret
LSPClient_Initialize endp

; ============================================================================
; LSPClient_OpenDocument - Notify server of opened document
; Input: pClient, pszFilePath, pszLanguageId, pszContent
; Returns: TRUE in eax on success
; ============================================================================
LSPClient_OpenDocument proc pClient:DWORD, pszFilePath:DWORD, pszLanguageId:DWORD, pszContent:DWORD
    LOCAL message:LSP_MESSAGE
    LOCAL szParams db 4096 dup(0)
    LOCAL pClientPtr:DWORD
    
    ; Validate inputs
    .if pClient == 0 || pszFilePath == 0 || pszContent == 0
        xor eax, eax
        ret
    .endif
    
    mov pClientPtr, pClient
    
    ; Format didOpen notification (simplified)
    szCopy addr szParams, "{", 13, 10
    szCat addr szParams, "  \"textDocument\": {", 13, 10
    szCat addr szParams, "    \"uri\": \"file://"
    szCat addr szParams, pszFilePath
    szCat addr szParams, "\",", 13, 10
    szCat addr szParams, "    \"languageId\": \""
    szCat addr szParams, pszLanguageId
    szCat addr szParams, "\",", 13, 10
    szCat addr szParams, "    \"version\": 1,", 13, 10
    szCat addr szParams, "    \"text\": \""
    szCat addr szParams, pszContent
    szCat addr szParams, "\"", 13, 10
    szCat addr szParams, "  }", 13, 10
    szCat addr szParams, "}"
    
    ; Set up message
    szCopy addr message.szParams, addr szParams
    mov message.dwType, LSP_TYPE_NOTIFICATION
    mov message.dwMethod, LSP_METHOD_TEXTDOC_DIDOPEN
    mov message.dwId, 0  ; Notifications don't have IDs
    
    ; Send message
    invoke LSPClient_SendMessage, pClientPtr, addr message
    ret
LSPClient_OpenDocument endp

; ============================================================================
; LSPClient_Hover - Request hover information
; Input: pClient, pszFilePath, dwLine, dwCharacter
; Returns: TRUE in eax on success
; ============================================================================
LSPClient_Hover proc pClient:DWORD, pszFilePath:DWORD, dwLine:DWORD, dwCharacter:DWORD
    LOCAL message:LSP_MESSAGE
    LOCAL szParams db 1024 dup(0)
    LOCAL pClientPtr:DWORD
    LOCAL dwId:DWORD
    
    ; Validate inputs
    .if pClient == 0 || pszFilePath == 0
        xor eax, eax
        ret
    .endif
    
    mov pClientPtr, pClient
    
    ; Get next message ID
    mov eax, pClient
    assume eax:ptr LSP_CLIENT
    mov ebx, [eax].dwNextId
    mov dwId, ebx
    inc [eax].dwNextId
    assume eax:nothing
    
    ; Format hover request (simplified)
    szCopy addr szParams, "{", 13, 10
    szCat addr szParams, "  \"textDocument\": {", 13, 10
    szCat addr szParams, "    \"uri\": \"file://"
    szCat addr szParams, pszFilePath
    szCat addr szParams, "\"", 13, 10
    szCat addr szParams, "  },", 13, 10
    szCat addr szParams, "  \"position\": {", 13, 10
    szCat addr szParams, "    \"line\": "
    invoke wsprintf, addr szParams[1000], "%d", dwLine
    szCat addr szParams, addr szParams[1000]
    szCat addr szParams, ",", 13, 10
    szCat addr szParams, "    \"character\": "
    invoke wsprintf, addr szParams[1000], "%d", dwCharacter
    szCat addr szParams, addr szParams[1000]
    szCat addr szParams, 13, 10
    szCat addr szParams, "  }", 13, 10
    szCat addr szParams, "}"
    
    ; Set up message
    szCopy addr message.szParams, addr szParams
    mov message.dwType, LSP_TYPE_REQUEST
    mov message.dwMethod, LSP_METHOD_TEXTDOC_HOVER
    mov eax, dwId
    mov message.dwId, eax
    
    ; Send message
    invoke LSPClient_SendMessage, pClientPtr, addr message
    ret
LSPClient_Hover endp

; ============================================================================
; LSPClient_GetClient - Get pointer to current LSP client
; Returns: Pointer to LSP_CLIENT in eax
; ============================================================================
LSPClient_GetClient proc
    mov eax, g_pLSPClient
    ret
LSPClient_GetClient endp

; ============================================================================
; LSPClient_SetClient - Set current LSP client
; Input: pClient
; ============================================================================
LSPClient_SetClient proc pClient:DWORD
    mov eax, pClient
    mov g_pLSPClient, eax
    ret
LSPClient_SetClient endp

; ============================================================================
; LSPClient_Cleanup - Cleanup LSP client system
; ============================================================================
LSPClient_Cleanup proc
    LOCAL pClient:DWORD
    
    ; Get current client
    mov pClient, g_pLSPClient
    
    ; Disconnect if connected
    .if pClient != 0
        invoke LSPClient_Disconnect, pClient
        MemFree pClient
    .endif
    
    ; Cleanup Winsock
    .if g_bWSAInitialized
        invoke WSACleanup
        mov g_bWSAInitialized, FALSE
    .endif
    
    ret
LSPClient_Cleanup endp

; ============================================================================
; LSPClient_Shutdown - Send shutdown request
; Input: pClient
; Returns: TRUE in eax on success
; ============================================================================
LSPClient_Shutdown proc pClient:DWORD
    LOCAL message:LSP_MESSAGE
    LOCAL pClientPtr:DWORD
    LOCAL dwId:DWORD
    
    ; Validate input
    .if pClient == 0
        xor eax, eax
        ret
    .endif
    
    mov pClientPtr, pClient
    
    ; Get next message ID
    mov eax, pClient
    assume eax:ptr LSP_CLIENT
    mov ebx, [eax].dwNextId
    mov dwId, ebx
    inc [eax].dwNextId
    assume eax:nothing
    
    ; Set up shutdown request
    szCopy addr message.szParams, "{", 13, 10
    szCat addr message.szParams, "  \"jsonrpc\": \"2.0\",", 13, 10
    szCat addr message.szParams, "  \"id\": "
    invoke wsprintf, addr message.szParams[1000], "%d", dwId
    szCat addr message.szParams, addr message.szParams[1000]
    szCat addr message.szParams, ",", 13, 10
    szCat addr message.szParams, "  \"method\": \"shutdown\"", 13, 10
    szCat addr message.szParams, "}"
    mov message.dwType, LSP_TYPE_REQUEST
    mov message.dwMethod, LSP_METHOD_SHUTDOWN
    mov eax, dwId
    mov message.dwId, eax
    
    ; Send message
    invoke LSPClient_SendMessage, pClientPtr, addr message
    ret
LSPClient_Shutdown endp

; ============================================================================
; LSPClient_Exit - Send exit notification
; Input: pClient
; Returns: TRUE in eax on success
; ============================================================================
LSPClient_Exit proc pClient:DWORD
    LOCAL message:LSP_MESSAGE
    LOCAL pClientPtr:DWORD
    
    ; Validate input
    .if pClient == 0
        xor eax, eax
        ret
    .endif
    
    mov pClientPtr, pClient
    
    ; Set up exit notification
    szCopy addr message.szParams, "{", 13, 10
    szCat addr message.szParams, "  \"jsonrpc\": \"2.0\",", 13, 10
    szCat addr message.szParams, "  \"method\": \"exit\"", 13, 10
    szCat addr message.szParams, "}"
    mov message.dwType, LSP_TYPE_NOTIFICATION
    mov message.dwMethod, LSP_METHOD_EXIT
    mov message.dwId, 0  ; Notifications don't have IDs
    
    ; Send message
    invoke LSPClient_SendMessage, pClientPtr, addr message
    ret
LSPClient_Exit endp

; ============================================================================
; Data
; ============================================================================

.data
    szLSPWelcome        db "LSP Client Ready", 13, 10
                       db "Connect to LSP server for language services.", 13, 10, 13, 10, 0
    szHoverRequest      db "Hover request sent for line %d, char %d", 0
    szDocumentOpened    db "Document opened: %s", 0

end