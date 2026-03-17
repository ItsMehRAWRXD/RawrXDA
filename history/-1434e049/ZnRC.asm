; ============================================================================
; winhttp_download.asm
; WinHTTP-based authenticated downloader (dynamic winhttp.dll loading)
; Supports Bearer token header and streaming to file.
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc

includelib \masm32\lib\kernel32.lib

PUBLIC WinHTTP_DownloadToFileA
PUBLIC WinHTTP_DownloadToFileBearerA

MAX_HOST_CHARS EQU 256
MAX_PATH_CHARS EQU 2048
MAX_HEADER_CHARS EQU 2048

.data
    g_hWinHttp            dd 0
    g_bWinHttpResolved    dd 0

    pWinHttpOpen              dd 0
    pWinHttpCloseHandle       dd 0
    pWinHttpConnect           dd 0
    pWinHttpOpenRequest       dd 0
    pWinHttpSendRequest       dd 0
    pWinHttpReceiveResponse   dd 0
    pWinHttpQueryDataAvailable dd 0
    pWinHttpReadData          dd 0
    pWinHttpAddRequestHeaders dd 0
    pWinHttpQueryHeaders      dd 0

    szWinHttpDll          db "winhttp.dll",0
    szWinHttpOpen         db "WinHttpOpen",0
    szWinHttpCloseHandle  db "WinHttpCloseHandle",0
    szWinHttpConnect      db "WinHttpConnect",0
    szWinHttpOpenRequest  db "WinHttpOpenRequest",0
    szWinHttpSendRequest  db "WinHttpSendRequest",0
    szWinHttpReceiveResponse db "WinHttpReceiveResponse",0
    szWinHttpQueryDataAvailable db "WinHttpQueryDataAvailable",0
    szWinHttpReadData     db "WinHttpReadData",0
    szWinHttpAddRequestHeaders db "WinHttpAddRequestHeaders",0
    szWinHttpQueryHeaders db "WinHttpQueryHeaders",0

    szUA                 db "RawrXD/1.0",0
    szGET                db "GET",0

    ; wide buffers
.data?
    wHost    dw MAX_HOST_CHARS dup(?)
    wPath    dw MAX_PATH_CHARS dup(?)
    wUA      dw 64 dup(?)
    wGET     dw 8 dup(?)
    wHeader  dw MAX_HEADER_CHARS dup(?)

.code

; ------------------------------------------------------------
; Helpers: minimal URL parse (http[s]://host/path)
; ------------------------------------------------------------
ParseUrlA PROC lpUrl:DWORD, lpHost:DWORD, cchHost:DWORD, lpPath:DWORD, cchPath:DWORD, pIsHttps:DWORD, pPort:DWORD
    push ebx
    push esi
    push edi

    mov esi, lpUrl

    ; default
    mov eax, pIsHttps
    mov dword ptr [eax], 0
    mov eax, pPort
    mov dword ptr [eax], 80

    ; scheme check
    ; if starts with https://
    mov eax, dword ptr [esi]
    ; naive compare "http"
    ; check 'h''t''t''p'
    cmp byte ptr [esi], 'h'
    jne @Fail
    cmp byte ptr [esi+1], 't'
    jne @Fail
    cmp byte ptr [esi+2], 't'
    jne @Fail
    cmp byte ptr [esi+3], 'p'
    jne @Fail

    cmp byte ptr [esi+4], 's'
    jne @Http
    cmp byte ptr [esi+5], ':'
    jne @Fail
    cmp byte ptr [esi+6], '/'
    jne @Fail
    cmp byte ptr [esi+7], '/'
    jne @Fail

    add esi, 8
    mov eax, pIsHttps
    mov dword ptr [eax], 1
    mov eax, pPort
    mov dword ptr [eax], 443
    jmp @AfterScheme

@Http:
    cmp byte ptr [esi+4], ':'
    jne @Fail
    cmp byte ptr [esi+5], '/'
    jne @Fail
    cmp byte ptr [esi+6], '/'
    jne @Fail
    add esi, 7

@AfterScheme:
    ; parse host until '/' or ':' or 0
    mov edi, lpHost
    mov ebx, cchHost
    dec ebx
    js @Fail

@HostLoop:
    mov al, [esi]
    test al, al
    jz @HostDone
    cmp al, '/'
    je @HostDone
    cmp al, ':'
    je @PortParse
    cmp ebx, 0
    je @Fail
    mov [edi], al
    inc edi
    inc esi
    dec ebx
    jmp @HostLoop

@PortParse:
    ; parse explicit port
    inc esi
    xor ecx, ecx
@PortDigits:
    mov al, [esi]
    cmp al, '0'
    jb @PortDone
    cmp al, '9'
    ja @PortDone
    mov edx, ecx
    shl ecx, 3
    lea ecx, [ecx+edx*2]
    sub al, '0'
    movzx eax, al
    add ecx, eax
    inc esi
    jmp @PortDigits
@PortDone:
    mov eax, pPort
    mov [eax], ecx
    jmp @HostDone

@HostDone:
    mov byte ptr [edi], 0

    ; path
    mov edi, lpPath
    mov ebx, cchPath
    dec ebx
    js @Fail

    mov al, [esi]
    cmp al, '/'
    je @CopyPath
    ; no path => "/"
    cmp ebx, 0
    je @Fail
    mov byte ptr [edi], '/'
    inc edi
    mov byte ptr [edi], 0
    jmp @Ok

@CopyPath:
@PathLoop:
    mov al, [esi]
    test al, al
    jz @PathDone
    cmp ebx, 0
    je @Fail
    mov [edi], al
    inc edi
    inc esi
    dec ebx
    jmp @PathLoop
@PathDone:
    mov byte ptr [edi], 0

@Ok:
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret

@Fail:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
ParseUrlA ENDP

; ------------------------------------------------------------
; Ensure winhttp.dll loaded and procs resolved
; ------------------------------------------------------------
WinHTTP_EnsureResolved PROC
    cmp g_bWinHttpResolved, 1
    je @Ok

    invoke LoadLibraryA, addr szWinHttpDll
    mov g_hWinHttp, eax
    test eax, eax
    jz @Fail

    invoke GetProcAddress, g_hWinHttp, addr szWinHttpOpen
    mov pWinHttpOpen, eax
    invoke GetProcAddress, g_hWinHttp, addr szWinHttpCloseHandle
    mov pWinHttpCloseHandle, eax
    invoke GetProcAddress, g_hWinHttp, addr szWinHttpConnect
    mov pWinHttpConnect, eax
    invoke GetProcAddress, g_hWinHttp, addr szWinHttpOpenRequest
    mov pWinHttpOpenRequest, eax
    invoke GetProcAddress, g_hWinHttp, addr szWinHttpSendRequest
    mov pWinHttpSendRequest, eax
    invoke GetProcAddress, g_hWinHttp, addr szWinHttpReceiveResponse
    mov pWinHttpReceiveResponse, eax
    invoke GetProcAddress, g_hWinHttp, addr szWinHttpQueryDataAvailable
    mov pWinHttpQueryDataAvailable, eax
    invoke GetProcAddress, g_hWinHttp, addr szWinHttpReadData
    mov pWinHttpReadData, eax
    invoke GetProcAddress, g_hWinHttp, addr szWinHttpAddRequestHeaders
    mov pWinHttpAddRequestHeaders, eax
    invoke GetProcAddress, g_hWinHttp, addr szWinHttpQueryHeaders
    mov pWinHttpQueryHeaders, eax

    ; validate
    cmp pWinHttpOpen, 0
    je @Fail
    cmp pWinHttpCloseHandle, 0
    je @Fail
    cmp pWinHttpConnect, 0
    je @Fail
    cmp pWinHttpOpenRequest, 0
    je @Fail
    cmp pWinHttpSendRequest, 0
    je @Fail
    cmp pWinHttpReceiveResponse, 0
    je @Fail
    cmp pWinHttpQueryDataAvailable, 0
    je @Fail
    cmp pWinHttpReadData, 0
    je @Fail
    cmp pWinHttpAddRequestHeaders, 0
    je @Fail
    cmp pWinHttpQueryHeaders, 0
    je @Fail

    mov g_bWinHttpResolved, 1

@Ok:
    mov eax, 1
    ret
@Fail:
    xor eax, eax
    ret
WinHTTP_EnsureResolved ENDP

; ------------------------------------------------------------
; WinHTTP_DownloadToFileA - Download URL to file (no auth)
; ------------------------------------------------------------
WinHTTP_DownloadToFileA PROC lpUrl:DWORD, lpOutPath:DWORD
    push 0
    push lpOutPath
    push lpUrl
    call WinHTTP_DownloadToFileBearerA
    add esp, 12
    ret
WinHTTP_DownloadToFileA ENDP

; ------------------------------------------------------------
; WinHTTP_DownloadToFileBearerA - Download URL to file with Bearer token
; lpBearerToken may be NULL.
; ------------------------------------------------------------
WinHTTP_DownloadToFileBearerA PROC lpUrl:DWORD, lpOutPath:DWORD, lpBearerToken:DWORD
    LOCAL isHttps:DWORD
    LOCAL port:DWORD
    LOCAL hSession:DWORD
    LOCAL hConnect:DWORD
    LOCAL hRequest:DWORD
    LOCAL hFile:DWORD
    LOCAL dwAvail:DWORD
    LOCAL dwRead:DWORD
    LOCAL dwWritten:DWORD
    LOCAL hostA[MAX_HOST_CHARS]:BYTE
    LOCAL pathA[MAX_PATH_CHARS]:BYTE
    LOCAL statusBuf[16]:WCHAR
    LOCAL statusLen:DWORD

    push ebx
    push esi
    push edi

    mov hSession, 0
    mov hConnect, 0
    mov hRequest, 0
    mov hFile, 0

    call WinHTTP_EnsureResolved
    test eax, eax
    jz @Fail

    lea eax, isHttps
    lea edx, port
    lea ecx, pathA
    push edx
    push eax
    push MAX_PATH_CHARS
    push ecx
    lea ecx, hostA
    push MAX_HOST_CHARS
    push ecx
    push lpUrl
    call ParseUrlA
    add esp, 28
    test eax, eax
    jz @Fail

    ; convert UA/method/host/path to wide
    invoke MultiByteToWideChar, CP_ACP, 0, addr szUA, -1, addr wUA, 64
    invoke MultiByteToWideChar, CP_ACP, 0, addr szGET, -1, addr wGET, 8
    invoke MultiByteToWideChar, CP_ACP, 0, addr hostA, -1, addr wHost, MAX_HOST_CHARS
    invoke MultiByteToWideChar, CP_ACP, 0, addr pathA, -1, addr wPath, MAX_PATH_CHARS

    ; WinHttpOpen
    ; HINTERNET WinHttpOpen(LPCWSTR ua, DWORD accessType, LPCWSTR proxy, LPCWSTR bypass, DWORD flags)
    push 0
    push 0
    push 0
    push 0
    push OFFSET wUA
    call pWinHttpOpen
    mov hSession, eax
    test eax, eax
    jz @Fail

    ; WinHttpConnect(hSession, host, port, 0)
    push 0
    mov eax, port
    push eax
    push OFFSET wHost
    push hSession
    call pWinHttpConnect
    mov hConnect, eax
    test eax, eax
    jz @Fail

    ; WinHttpOpenRequest(hConnect, method, path, NULL, NULL, NULL, flags)
    mov eax, isHttps
    test eax, eax
    jz @NoSecure
    mov ebx, WINHTTP_FLAG_SECURE
    jmp @HaveFlags
@NoSecure:
    xor ebx, ebx
@HaveFlags:
    push ebx
    push 0
    push 0
    push 0
    push 0
    push OFFSET wPath
    push OFFSET wGET
    push hConnect
    call pWinHttpOpenRequest
    mov hRequest, eax
    test eax, eax
    jz @Fail

    ; Optional Authorization header
    mov eax, lpBearerToken
    test eax, eax
    jz @Send

    ; Build header: "Authorization: Bearer <token>\r\n"
    ; wHeader = L"Authorization: Bearer " + token + L"\r\n"
    ; Convert token to wide into wHeader (after prefix)
    invoke lstrcpyW, addr wHeader, CStrW("Authorization: Bearer ")
    ; Append token
    invoke MultiByteToWideChar, CP_ACP, 0, lpBearerToken, -1, addr wPath, MAX_PATH_CHARS ; reuse wPath as temp token
    invoke lstrcatW, addr wHeader, addr wPath
    invoke lstrcatW, addr wHeader, CStrW("\r\n")

    ; AddRequestHeaders(hRequest, wHeader, -1, 0)
    push 0
    push -1
    push OFFSET wHeader
    push hRequest
    call pWinHttpAddRequestHeaders

@Send:
    ; SendRequest(hRequest, NULL, 0, NULL, 0, 0, 0)
    push 0
    push 0
    push 0
    push 0
    push 0
    push 0
    push hRequest
    call pWinHttpSendRequest
    test eax, eax
    jz @Fail

    push 0
    push hRequest
    call pWinHttpReceiveResponse
    test eax, eax
    jz @Fail

    ; Open output file
    invoke CreateFileA, lpOutPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    cmp eax, INVALID_HANDLE_VALUE
    je @Fail

@ReadLoop:
    lea eax, dwAvail
    push eax
    push hRequest
    call pWinHttpQueryDataAvailable
    test eax, eax
    jz @Done
    mov eax, dwAvail
    test eax, eax
    jz @Done

    ; read min(dwAvail, 4096)
    mov eax, dwAvail
    cmp eax, 4096
    jbe @SzOk
    mov eax, 4096
@SzOk:
    lea edx, dwRead
    push edx
    push eax
    lea ecx, hostA ; reuse hostA as IO buffer
    push ecx
    push hRequest
    call pWinHttpReadData
    test eax, eax
    jz @Done

    mov eax, dwRead
    test eax, eax
    jz @Done

    invoke WriteFile, hFile, addr hostA, dwRead, addr dwWritten, NULL
    test eax, eax
    jz @Done
    jmp @ReadLoop

@Done:
    ; cleanup handles
    cmp hFile, 0
    je @NoFile
    invoke CloseHandle, hFile
@NoFile:

    cmp hRequest, 0
    je @NoReq
    push hRequest
    call pWinHttpCloseHandle
@NoReq:

    cmp hConnect, 0
    je @NoConn
    push hConnect
    call pWinHttpCloseHandle
@NoConn:

    cmp hSession, 0
    je @Ok
    push hSession
    call pWinHttpCloseHandle

@Ok:
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret

@Fail:
    cmp hFile, 0
    je @F0
    invoke CloseHandle, hFile
@F0:
    cmp hRequest, 0
    je @F1
    push hRequest
    call pWinHttpCloseHandle
@F1:
    cmp hConnect, 0
    je @F2
    push hConnect
    call pWinHttpCloseHandle
@F2:
    cmp hSession, 0
    je @F3
    push hSession
    call pWinHttpCloseHandle
@F3:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
WinHTTP_DownloadToFileBearerA ENDP

END
