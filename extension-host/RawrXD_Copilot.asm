; RawrXD_Copilot.asm - Native AI/Copilot (replaces bigdaddyg-copilot + cursor-multi-ai)
; WinHTTP POST to localhost:11434/api/generate. Stream tokens to pfnCallback.
; Commands: openChat (fixed from typo opencaht). Exports for Extension Host.

OPTION CASEMAP:NONE

INCLUDELIB kernel32.lib
INCLUDELIB winhttp.lib

; WinHTTP constants
WINHTTP_ACCESS_TYPE_DEFAULT_PROXY EQU 0
WINHTTP_ADDREQ_FLAG_ADD           EQU 20000000h
WINHTTP_ADDREQ_FLAG_REPLACE       EQU 80000000h
WINHTTP_NO_ADDITIONAL_HEADERS     EQU 0

EXTERN WinHttpOpen:PROC
EXTERN WinHttpConnect:PROC
EXTERN WinHttpOpenRequest:PROC
EXTERN WinHttpAddRequestHeaders:PROC
EXTERN WinHttpSendRequest:PROC
EXTERN WinHttpReceiveResponse:PROC
EXTERN WinHttpQueryDataAvailable:PROC
EXTERN WinHttpReadData:PROC
EXTERN WinHttpCloseHandle:PROC
EXTERN WinHttpSetTimeouts:PROC
EXTERN MultiByteToWideChar:PROC

.DATA
ALIGN 8
g_hMainWnd    DQ 0

; Wide strings for WinHTTP (UTF-16LE)
ALIGN 2
wUserAgent    DW 'R','a','w','r','X','D','-','C','o','p','i','l','o','t','/','1','.','0',0
wLocalhost    DW 'l','o','c','a','l','h','o','s','t',0
wPathGen      DW '/','a','p','i','/','g','e','n','e','r','a','t','e',0
wPost         DW 'P','O','S','T',0
wContentType  DW 'C','o','n','t','e','n','t','-','T','y','p','e',':',' '
              DW 'a','p','p','l','i','c','a','t','i','o','n','/','j','s','o','n',0

; JSON template (29 + msg + 17 chars)
szJsonPrefix  BYTE '{"model":"llama3.2","prompt":"',0
szJsonSuffix  BYTE '","stream":true}',0

.DATA?
ALIGN 16
g_dwRead      DWORD 0
g_dwAvail     DWORD 0
g_chunk       BYTE 4096 DUP(?)
g_jsonBody    BYTE 32768 DUP(?)
g_wideHost    DW 256 DUP(?)

.CODE

;----------------------------------------------------------------------
; _strlen - RCX=ptr, returns RAX=len
;----------------------------------------------------------------------
_strlen PROC
    xor eax, eax
    test rcx, rcx
    jz @F
@@: cmp byte ptr [rcx+rax], 0
    je @F
    inc rax
    jmp @B
@@: ret
_strlen ENDP

;----------------------------------------------------------------------
; Build JSON body: {"model":"llama3.2","prompt":"<msg>","stream":true}
; RCX=pszMessage, RDX=output buffer, R8=capacity
; Returns RAX=body length or 0
;----------------------------------------------------------------------
BuildJsonBody PROC
    push rbx
    push rsi
    push rdi
    push r15
    sub rsp, 48h
    mov rbx, rcx
    mov rdi, rdx
    mov r15, rdx
    mov r9d, r8d
    ; Copy prefix (29 chars)
    lea rsi, szJsonPrefix
    mov ecx, 29
    cmp r9d, 256
    jb @@fail
    rep movsb
    ; Escape and copy message
    mov rcx, rbx
    call _strlen
    mov r10, rax
    xor edx, edx
    test r10, r10
    jz @@add_suffix
@@copy: movzx eax, byte ptr [rbx+rdx]
    cmp al, '"'
    je @@escape
    cmp al, '\'
    je @@escape
    cmp al, 13
    je @@next
    cmp al, 10
    je @@next
    mov [rdi], al
    inc rdi
@@next: inc edx
    cmp edx, r10d
    jb @@copy
    jmp @@add_suffix
@@escape:
    mov byte ptr [rdi], '\'
    inc rdi
    mov [rdi], al
    inc rdi
    inc edx
    cmp edx, r10d
    jb @@copy
@@add_suffix:
    lea rsi, szJsonSuffix
    mov ecx, 17
    rep movsb
    mov rax, rdi
    sub rax, r15
    jmp @@out
@@fail:
    xor eax, eax
@@out:
    add rsp, 48h
    pop r15
    pop rdi
    pop rsi
    pop rbx
    ret
BuildJsonBody ENDP

.DATA
szRespKey2    BYTE '"response":"',0   ; 12 chars
.DATA?
g_tokenBuf    BYTE 256 DUP(?)

.CODE
;----------------------------------------------------------------------
; Parse "response":"X" from NDJSON line, extract token, call callback
; RCX=chunk ptr, RDX=len, R8=pfnCallback, R9=callback context (pszMessage)
;----------------------------------------------------------------------
ParseAndCallback PROC
    ; Simplified: scan for "response":" and extract until next "
    push rbx
    push rsi
    push rdi
    sub rsp, 48h
    mov rbx, rcx
    mov ecx, edx
    mov r10, r8
    mov r11, r9
    xor edx, edx
@@: cmp edx, ecx
    jge @@done
    ; Look for "response":
    lea rsi, szRespKey2
    mov edi, edx
    mov r8d, 12
@@cmp: mov al, [rbx+rdi]
    cmp al, [rsi]
    jne @@next
    inc edi
    inc rsi
    dec r8d
    jnz @@cmp
    ; Found "response":" - edi points to value start (first char of token)
    cmp edi, ecx
    jge @@done
    ; edi points to start of value after ":"
    lea rsi, g_tokenBuf
    xor r8d, r8d
@@val: movzx eax, byte ptr [rbx+rdi]
    cmp al, '"'
    je @@got_token
    cmp al, '\'
    je @@esc
    mov [rsi+r8], al
    inc r8
    inc edi
    cmp edi, ecx
    jb @@val
    jmp @@done
@@esc: inc edi
    cmp edi, ecx
    jge @@done
    movzx eax, byte ptr [rbx+rdi]
    mov [rsi+r8], al
    inc r8
    inc edi
    jmp @@val
@@got_token:
    mov byte ptr [rsi+r8], 0
    test r8d, r8d
    jz @@next
    mov rcx, rsi
    mov rdx, r11
    call r10
    jmp @@done
@@next: inc edx
    jmp @B
@@done:
    add rsp, 48h
    pop rdi
    pop rsi
    pop rbx
    ret
ParseAndCallback ENDP

;----------------------------------------------------------------------
; ExtensionInit(hParentWnd)
;----------------------------------------------------------------------
ExtensionInit PROC hParentWnd:QWORD
    mov g_hMainWnd, rcx
    xor eax, eax
    ret
ExtensionInit ENDP

;----------------------------------------------------------------------
; ExtensionActivate
;----------------------------------------------------------------------
ExtensionActivate PROC
    mov eax, 1
    ret
ExtensionActivate ENDP

;----------------------------------------------------------------------
; ExtensionExecuteCommand(pszCommand, pvData) - openChat, beaconChat
;----------------------------------------------------------------------
ExtensionExecuteCommand PROC pszCommand:QWORD, pvData:QWORD
    ; openChat / beaconChat - return 1 (host will call HandleChat)
    mov eax, 1
    ret
ExtensionExecuteCommand ENDP

;----------------------------------------------------------------------
; ExtensionHandleChat(pszMessage, pfnCallback)
; POST to localhost:11434/api/generate, stream tokens to callback
; pfnCallback(pszToken, pszMessage) called per token
;----------------------------------------------------------------------
ExtensionHandleChat PROC pszMessage:QWORD, pfnCallback:QWORD
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 88h
    mov r12, rcx
    mov r13, rdx
    test r12, r12
    jz @@fail
    test r13, r13
    jz @@fail
    ; Build JSON body
    lea rdx, g_jsonBody
    mov r8d, 32768
    call BuildJsonBody
    test rax, rax
    jz @@fail
    mov ebx, eax
    ; WinHttpOpen
    lea rcx, wUserAgent
    mov edx, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
    xor r8, r8
    xor r9, r9
    call WinHttpOpen
    test rax, rax
    jz @@fail
    mov rsi, rax
    ; WinHttpConnect(session, localhost, 11434, 0)
    mov rcx, rsi
    lea rdx, wLocalhost
    mov r8d, 11434
    xor r9d, r9d
    call WinHttpConnect
    test rax, rax
    jz @@close_sess
    mov rdi, rax
    ; WinHttpOpenRequest(connect, POST, /api/generate, ...)
    mov rcx, rdi
    lea rdx, wPost
    lea r8, wPathGen
    xor r9d, r9d
    mov qword ptr [rsp+28h], 0
    mov qword ptr [rsp+30h], 0
    call WinHttpOpenRequest
    test rax, rax
    jz @@close_conn
    push rax
    ; WinHttpAddRequestHeaders Content-Type
    mov rcx, rax
    lea rdx, wContentType
    mov r8d, -1
    mov r9d, WINHTTP_ADDREQ_FLAG_ADD OR WINHTTP_ADDREQ_FLAG_REPLACE
    call WinHttpAddRequestHeaders
    ; WinHttpSetTimeouts 5s, 10s, 30s, 300s
    pop rcx
    push rcx
    mov edx, 5000
    mov r8d, 10000
    mov r9d, 30000
    mov dword ptr [rsp+28h], 300000
    call WinHttpSetTimeouts
    ; WinHttpSendRequest
    pop rcx
    push rcx
    xor rdx, rdx
    xor r8d, r8d
    lea r9, g_jsonBody
    mov dword ptr [rsp+28h], ebx
    mov dword ptr [rsp+30h], ebx
    call WinHttpSendRequest
    test eax, eax
    jz @@close_req
    ; WinHttpReceiveResponse
    pop rcx
    push rcx
    xor rdx, rdx
    call WinHttpReceiveResponse
    test eax, eax
    jz @@close_req
    ; Read loop
@@read_loop:
    pop rcx
    push rcx
    lea rdx, g_dwAvail
    call WinHttpQueryDataAvailable
    test eax, eax
    jz @@close_req
    cmp g_dwAvail, 0
    je @@done_ok
    pop rcx
    push rcx
    lea rdx, g_chunk
    mov r8d, g_dwAvail
    cmp r8d, 4096
    jbe @@read_ok
    mov r8d, 4096
@@read_ok:
    lea r9, g_dwRead
    mov qword ptr [rsp+28h], 0
    call WinHttpReadData
    test eax, eax
    jz @@close_req
    cmp g_dwRead, 0
    je @@done_ok
    ; Parse chunk, call callback
    lea rcx, g_chunk
    mov edx, g_dwRead
    mov r8, r13
    mov r9, r12
    call ParseAndCallback
    jmp @@read_loop
@@done_ok:
    pop rcx
    call WinHttpCloseHandle
@@close_conn:
    mov rcx, rdi
    call WinHttpCloseHandle
@@close_sess:
    mov rcx, rsi
    call WinHttpCloseHandle
    mov eax, 1
    jmp @@out
@@close_req:
    pop rcx
    call WinHttpCloseHandle
    jmp @@close_conn
@@fail:
    xor eax, eax
@@out:
    add rsp, 88h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ExtensionHandleChat ENDP

END
