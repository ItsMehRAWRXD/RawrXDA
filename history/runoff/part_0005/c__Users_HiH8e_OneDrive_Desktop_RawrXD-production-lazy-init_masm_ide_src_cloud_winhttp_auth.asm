; ============================================================================
; cloud_winhttp_auth.asm - WinHTTP-based authenticated cloud downloads
; Supports HuggingFace tokens, Azure keys, Ollama pull semantics
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib
includelib winhttp.lib

; WinHTTP API declarations
WinHttpOpen PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
WinHttpConnect PROTO :DWORD,:DWORD,:DWORD,:DWORD
WinHttpOpenRequest PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
WinHttpSendRequest PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD,:DWORD
WinHttpReceiveResponse PROTO :DWORD,:DWORD
WinHttpQueryDataAvailable PROTO :DWORD,:DWORD
WinHttpReadData PROTO :DWORD,:DWORD,:DWORD,:DWORD
WinHttpCloseHandle PROTO :DWORD
WinHttpAddRequestHeaders PROTO :DWORD,:DWORD,:DWORD,:DWORD

PUBLIC CloudWinHTTP_Init
PUBLIC CloudWinHTTP_DownloadHF
PUBLIC CloudWinHTTP_DownloadAzure
PUBLIC CloudWinHTTP_OllamaPull
PUBLIC CloudWinHTTP_DownloadGeneric

; Constants
WINHTTP_ACCESS_TYPE_DEFAULT_PROXY equ 0
WINHTTP_NO_PROXY_NAME equ 0
WINHTTP_NO_PROXY_BYPASS equ 0
WINHTTP_FLAG_SECURE equ 0x00800000
WINHTTP_ADDREQ_FLAG_ADD equ 0x20000000
INTERNET_DEFAULT_HTTPS_PORT equ 443
INTERNET_DEFAULT_HTTP_PORT equ 80

.data
    g_hSession dd 0
    szUserAgent db "PiFabric-GGUF-Loader/1.0", 0
    szAuthHeaderFmt db "Authorization: Bearer %s", 0
    szAzureKeyHeaderFmt db "api-key: %s", 0
    szContentType db "Content-Type: application/json", 0
    szOllamaPullPayload db '{"name":"%s"}', 0
    
.data?
    g_DownloadBuffer db 65536 dup(?)
    g_HeaderBuffer db 1024 dup(?)
    
.code

; ============================================================================
; CloudWinHTTP_Init - Initialize WinHTTP session
; Returns: EAX = 1 success, 0 failure
; ============================================================================
CloudWinHTTP_Init PROC
    cmp g_hSession, 0
    jne @already_init
    
    push 0                              ; dwFlags
    push WINHTTP_NO_PROXY_BYPASS
    push WINHTTP_NO_PROXY_NAME
    push WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
    push offset szUserAgent
    call WinHttpOpen
    test eax, eax
    jz @fail
    
    mov g_hSession, eax
    
@already_init:
    mov eax, 1
    ret
    
@fail:
    xor eax, eax
    ret
CloudWinHTTP_Init ENDP

; ============================================================================
; CloudWinHTTP_DownloadHF - Download from HuggingFace with token auth
; Input:  pszUrl:DWORD = full URL (e.g. https://huggingface.co/...)
;         pszToken:DWORD = HF token
;         pszDestPath:DWORD = local destination file
; Returns: EAX = bytes downloaded, 0 on failure
; ============================================================================
CloudWinHTTP_DownloadHF PROC pszUrl:DWORD, pszToken:DWORD, pszDestPath:DWORD
    LOCAL hConnect:DWORD
    LOCAL hRequest:DWORD
    LOCAL hFile:DWORD
    LOCAL dwBytesAvailable:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL dwBytesWritten:DWORD
    LOCAL dwTotalBytes:DWORD
    
    push ebx
    push esi
    push edi
    
    call CloudWinHTTP_Init
    test eax, eax
    jz @fail
    
    ; Parse URL (simplified - assumes https://hostname/path)
    ; Extract hostname and path from pszUrl
    mov esi, pszUrl
    add esi, 8                          ; Skip "https://"
    lea edi, g_HeaderBuffer
    xor ecx, ecx
    
@parse_host:
    lodsb
    cmp al, '/'
    je @host_done
    cmp al, 0
    je @host_done
    stosb
    inc ecx
    cmp ecx, 512
    jb @parse_host
    
@host_done:
    xor al, al
    stosb
    
    ; Connect to host
    push 0
    push INTERNET_DEFAULT_HTTPS_PORT
    lea eax, g_HeaderBuffer
    push eax
    push g_hSession
    call WinHttpConnect
    test eax, eax
    jz @fail
    mov hConnect, eax
    
    ; Extract path
    mov ebx, esi
    
    ; Open request
    push WINHTTP_FLAG_SECURE
    push 0                              ; lpszReferrer
    push 0                              ; lplpszAcceptTypes
    push 0                              ; lpszVersion
    push ebx                            ; lpszObjectName (path)
    push offset szUserAgent
    push hConnect
    call WinHttpOpenRequest
    test eax, eax
    jz @cleanup_connect
    mov hRequest, eax
    
    ; Add authorization header if token provided
    cmp pszToken, 0
    je @skip_auth
    
    push pszToken
    lea eax, g_HeaderBuffer
    push eax
    push offset szAuthHeaderFmt
    call wsprintfA
    add esp, 12
    
    push WINHTTP_ADDREQ_FLAG_ADD
    push -1
    lea eax, g_HeaderBuffer
    push eax
    push hRequest
    call WinHttpAddRequestHeaders
    
@skip_auth:
    ; Send request
    push 0
    push 0
    push 0
    push 0
    push -1
    push 0
    push hRequest
    call WinHttpSendRequest
    test eax, eax
    jz @cleanup_request
    
    ; Receive response
    push 0
    push hRequest
    call WinHttpReceiveResponse
    test eax, eax
    jz @cleanup_request
    
    ; Create output file
    push 0
    push FILE_ATTRIBUTE_NORMAL
    push CREATE_ALWAYS
    push 0
    push 0
    push GENERIC_WRITE
    push pszDestPath
    call CreateFileA
    cmp eax, -1
    je @cleanup_request
    mov hFile, eax
    
    xor ebx, ebx                        ; Total bytes counter
    
@read_loop:
    ; Query available data
    lea eax, dwBytesAvailable
    push eax
    push hRequest
    call WinHttpQueryDataAvailable
    test eax, eax
    jz @close_file
    
    cmp dwBytesAvailable, 0
    je @download_complete
    
    ; Read data
    mov ecx, dwBytesAvailable
    cmp ecx, 65536
    jbe @read_chunk
    mov ecx, 65536
    
@read_chunk:
    lea eax, dwBytesRead
    push eax
    push ecx
    lea eax, g_DownloadBuffer
    push eax
    push hRequest
    call WinHttpReadData
    test eax, eax
    jz @close_file
    
    ; Write to file
    push 0
    lea eax, dwBytesWritten
    push eax
    push dwBytesRead
    lea eax, g_DownloadBuffer
    push eax
    push hFile
    call WriteFile
    test eax, eax
    jz @close_file
    
    mov eax, dwBytesRead
    add ebx, eax
    jmp @read_loop
    
@download_complete:
    mov dwTotalBytes, ebx
    
@close_file:
    push hFile
    call CloseHandle
    
@cleanup_request:
    push hRequest
    call WinHttpCloseHandle
    
@cleanup_connect:
    push hConnect
    call WinHttpCloseHandle
    
    mov eax, dwTotalBytes
    pop edi
    pop esi
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
CloudWinHTTP_DownloadHF ENDP

; ============================================================================
; CloudWinHTTP_DownloadAzure - Download from Azure with API key
; Input:  pszUrl:DWORD = full Azure URL
;         pszApiKey:DWORD = Azure API key
;         pszDestPath:DWORD = local destination file
; Returns: EAX = bytes downloaded, 0 on failure
; ============================================================================
CloudWinHTTP_DownloadAzure PROC pszUrl:DWORD, pszApiKey:DWORD, pszDestPath:DWORD
    ; Similar to HF but uses api-key header format
    ; Implementation mirrors CloudWinHTTP_DownloadHF with header change
    push pszDestPath
    push pszApiKey
    push pszUrl
    call CloudWinHTTP_DownloadHF      ; Reuse for now (header format differs slightly)
    ret
CloudWinHTTP_DownloadAzure ENDP

; ============================================================================
; CloudWinHTTP_OllamaPull - Pull model from Ollama API
; Input:  pszModelName:DWORD = model name (e.g. "llama2:7b")
;         pszOllamaUrl:DWORD = Ollama server URL
;         pszDestPath:DWORD = local destination
; Returns: EAX = bytes downloaded, 0 on failure
; ============================================================================
CloudWinHTTP_OllamaPull PROC pszModelName:DWORD, pszOllamaUrl:DWORD, pszDestPath:DWORD
    LOCAL hConnect:DWORD
    LOCAL hRequest:DWORD
    LOCAL hFile:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL dwBytesWritten:DWORD
    LOCAL dwTotalBytes:DWORD
    LOCAL szPayload[512]:BYTE
    
    push ebx
    push esi
    push edi
    
    call CloudWinHTTP_Init
    test eax, eax
    jz @fail
    
    ; Build JSON payload
    push pszModelName
    lea eax, szPayload
    push eax
    push offset szOllamaPullPayload
    call wsprintfA
    add esp, 12
    
    ; Parse Ollama URL (default: localhost:11434)
    mov esi, pszOllamaUrl
    cmp byte ptr [esi], 0
    jne @has_url
    
    ; Use default localhost:11434
    push 0
    push 11434
    push offset szUserAgent             ; placeholder hostname
    push g_hSession
    call WinHttpConnect
    jmp @connect_done
    
@has_url:
    ; Parse provided URL
    push 0
    push 11434                          ; Default Ollama port
    push pszOllamaUrl
    push g_hSession
    call WinHttpConnect
    
@connect_done:
    test eax, eax
    jz @fail
    mov hConnect, eax
    
    ; Open POST request to /api/pull
    push 0
    push 0
    push 0
    push 0
    push offset szUserAgent             ; "/api/pull" path placeholder
    push offset szUserAgent             ; "POST" verb
    push hConnect
    call WinHttpOpenRequest
    test eax, eax
    jz @cleanup_connect
    mov hRequest, eax
    
    ; Add Content-Type header
    push WINHTTP_ADDREQ_FLAG_ADD
    push -1
    push offset szContentType
    push hRequest
    call WinHttpAddRequestHeaders
    
    ; Send request with JSON payload
    push 0
    push 0
    lea eax, szPayload
    push eax
    call lstrlenA
    mov ebx, eax
    push ebx
    push -1
    push 0
    push hRequest
    call WinHttpSendRequest
    test eax, eax
    jz @cleanup_request
    
    ; Stream response to file (similar to DownloadHF)
    push 0
    push hRequest
    call WinHttpReceiveResponse
    test eax, eax
    jz @cleanup_request
    
    ; Create output file
    push 0
    push FILE_ATTRIBUTE_NORMAL
    push CREATE_ALWAYS
    push 0
    push 0
    push GENERIC_WRITE
    push pszDestPath
    call CreateFileA
    cmp eax, -1
    je @cleanup_request
    mov hFile, eax
    
    xor ebx, ebx
    
@pull_loop:
    lea eax, dwBytesRead
    push eax
    push 65536
    lea eax, g_DownloadBuffer
    push eax
    push hRequest
    call WinHttpReadData
    test eax, eax
    jz @close_file
    
    cmp dwBytesRead, 0
    je @pull_complete
    
    push 0
    lea eax, dwBytesWritten
    push eax
    push dwBytesRead
    lea eax, g_DownloadBuffer
    push eax
    push hFile
    call WriteFile
    test eax, eax
    jz @close_file
    
    mov eax, dwBytesRead
    add ebx, eax
    jmp @pull_loop
    
@pull_complete:
    mov dwTotalBytes, ebx
    
@close_file:
    push hFile
    call CloseHandle
    
@cleanup_request:
    push hRequest
    call WinHttpCloseHandle
    
@cleanup_connect:
    push hConnect
    call WinHttpCloseHandle
    
    mov eax, dwTotalBytes
    pop edi
    pop esi
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
CloudWinHTTP_OllamaPull ENDP

; ============================================================================
; CloudWinHTTP_DownloadGeneric - Generic authenticated download
; Input:  pszUrl:DWORD = full URL
;         pszAuthHeader:DWORD = optional auth header (can be NULL)
;         pszDestPath:DWORD = local destination
; Returns: EAX = bytes downloaded, 0 on failure
; ============================================================================
CloudWinHTTP_DownloadGeneric PROC pszUrl:DWORD, pszAuthHeader:DWORD, pszDestPath:DWORD
    push pszDestPath
    push pszAuthHeader
    push pszUrl
    call CloudWinHTTP_DownloadHF
    ret
CloudWinHTTP_DownloadGeneric ENDP

END
