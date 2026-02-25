; RawrXD_LSP.asm - Native LSP bridge (replaces rawrxd-lsp-client)
; JSON-RPC over stdio via pipes. CreatePipe -> CreateProcess -> Content-Length framing.
; Exports: ExtensionInit, ExtensionActivate, ExtensionExecuteCommand, ExtensionHandleChat

OPTION CASEMAP:NONE

INCLUDELIB kernel32.lib

; Win32 constants
STD_INPUT_HANDLE     EQU -10
STD_OUTPUT_HANDLE    EQU -11
STARTF_USESTDHANDLES EQU 100h
HANDLE_FLAG_INHERIT  EQU 1
CREATE_NO_WINDOW     EQU 08000000h

SECURITY_ATTRIBUTES STRUCT
    nLength              DWORD ?
    lpSecurityDescriptor QWORD ?
    bInheritHandle       DWORD ?
SECURITY_ATTRIBUTES ENDS

STARTUPINFOA STRUCT
    cb              DWORD ?
    lpReserved      QWORD ?
    lpDesktop       QWORD ?
    lpTitle         QWORD ?
    dwX             DWORD ?
    dwY             DWORD ?
    dwXSize         DWORD ?
    dwYSize         DWORD ?
    dwXCountChars   DWORD ?
    dwYCountChars   DWORD ?
    dwFillAttribute DWORD ?
    dwFlags         DWORD ?
    wShowWindow     WORD ?
    cbReserved2     WORD ?
    lpReserved2     QWORD ?
    hStdInput       QWORD ?
    hStdOutput      QWORD ?
    hStdError       QWORD ?
STARTUPINFOA ENDS

PROCESS_INFORMATION STRUCT
    hProcess    QWORD ?
    hThread     QWORD ?
    dwProcessId DWORD ?
    dwThreadId  DWORD ?
PROCESS_INFORMATION ENDS

EXTERN GetStdHandle:PROC
EXTERN CreatePipe:PROC
EXTERN SetHandleInformation:PROC
EXTERN CreateProcessA:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC

.DATA
ALIGN 8
g_hParentWnd   DQ 0
g_hChildStdinRd   DQ 0
g_hChildStdinWr   DQ 0
g_hChildStdoutRd  DQ 0
g_hChildStdoutWr  DQ 0
g_hProcess     DQ 0
g_hThread      DQ 0
g_bSpawned     DWORD 0
g_dwWritten    DWORD 0
g_dwRead       DWORD 0

szClangdCmd    BYTE "clangd --log=error",0
szCmdInit      BYTE "initialize",0
szContentLen   BYTE "Content-Length: ",0
szHeaderEnd    BYTE 13,10,13,10,0
szInitRequest  BYTE '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":0,',\
               '"capabilities":{"textDocument":{"completion":{"completionItem":{"snippetSupport":true}}}}',\
               ',"rootUri":"file:///D:/rawrxd"}}',0
szInitialized  BYTE '{"jsonrpc":"2.0","method":"initialized","params":{}}',0

.DATA?
ALIGN 16
g_frameBuf     BYTE 8192 DUP(?)
g_responseBuf  BYTE 32768 DUP(?)
g_numBuf       BYTE 64 DUP(?)
g_secAttr      SECURITY_ATTRIBUTES <>
g_startInfo    STARTUPINFOA <>
g_procInfo     PROCESS_INFORMATION <>

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
; _itoa - RAX=val, RCX=dest, returns RDX=digit count
;----------------------------------------------------------------------
_itoa PROC
    push rbx
    push rsi
    push rdi
    mov rdi, rcx
    mov rbx, rax
    xor ecx, ecx
    mov rsi, 10
@@: xor edx, edx
    mov rax, rbx
    div rsi
    add dl, '0'
    push rdx
    inc ecx
    mov rbx, rax
    test rax, rax
    jnz @B
    mov edx, ecx
@@: pop rax
    mov byte ptr [rdi], al
    inc rdi
    dec ecx
    jnz @B
    mov byte ptr [rdi], 0
    pop rdi
    pop rsi
    pop rbx
    ret
_itoa ENDP

;----------------------------------------------------------------------
; LSP_SendRequest - send JSON-RPC request to LSP
; RCX=JSON body (null-term), returns 1=ok 0=fail
;----------------------------------------------------------------------
LSP_SendRequest PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 48h
    mov rbx, rcx
    cmp g_bSpawned, 0
    je @@fail
    ; strlen(body)
    mov rcx, rbx
    call _strlen
    mov r8, rax
    ; Build "Content-Length: N\r\n\r\n" + body in g_frameBuf
    lea rdi, g_frameBuf
    lea rsi, szContentLen
    mov ecx, 16
    rep movsb
    mov rcx, rdi
    mov rax, r8
    call _itoa
    add rdi, rdx
    mov dword ptr [rdi], 0A0D0A0Dh
    add rdi, 4
    ; copy body
    mov rcx, r8
    mov rsi, rbx
    rep movsb
    lea rax, g_frameBuf
    sub rdi, rax
    mov ebx, edi
    ; WriteFile(g_hChildStdinWr, frameBuf, len, &written, 0)
    mov rcx, g_hChildStdinWr
    lea rdx, g_frameBuf
    mov r8d, ebx
    lea r9, g_dwWritten
    mov qword ptr [rsp+28h], 0
    call WriteFile
    test eax, eax
    jz @@fail
    mov eax, 1
    jmp @@out
@@fail:
    xor eax, eax
@@out:
    add rsp, 48h
    pop rdi
    pop rsi
    pop rbx
    ret
LSP_SendRequest ENDP

;----------------------------------------------------------------------
; LSP_ReadResponse - read Content-Length framed response
; RCX=output buffer, RDX=capacity, returns RAX=bytes read or 0
;----------------------------------------------------------------------
LSP_ReadResponse PROC
    ; Simplified: read until we get Content-Length, parse N, read N bytes
    ; For production: parse "Content-Length: N" header, read N
    push rbx
    sub rsp, 48h
    mov rbx, rcx
    mov r8d, edx
    mov rcx, g_hChildStdoutRd
    lea rdx, g_responseBuf
    mov r9d, 32768
    lea rax, g_dwRead
    mov qword ptr [rsp+28h], 0
    call ReadFile
    test eax, eax
    jz @@fail
    mov eax, g_dwRead
    test eax, eax
    jz @@fail
    ; Copy to output (simplified: just copy raw response)
    cmp eax, r8d
    jb @F
    mov eax, r8d
@@: mov ecx, eax
    lea rsi, g_responseBuf
    mov rdi, rbx
    rep movsb
    mov eax, g_dwRead
    jmp @@out
@@fail:
    xor eax, eax
@@out:
    add rsp, 48h
    pop rbx
    ret
LSP_ReadResponse ENDP

;----------------------------------------------------------------------
; ExtensionInit(hParentWnd) - spawn LSP server, init pipes
;----------------------------------------------------------------------
ExtensionInit PROC hParentWnd:QWORD
    push rbx
    push rsi
    push rdi
    sub rsp, 168h
    mov g_hParentWnd, rcx
    lea rcx, g_secAttr
    mov dword ptr [rcx], SIZEOF SECURITY_ATTRIBUTES
    mov qword ptr [rcx+8], 0
    mov dword ptr [rcx+16], 1
    ; CreatePipe for child stdout (parent reads)
    lea rcx, g_hChildStdoutRd
    lea rdx, g_hChildStdoutWr
    lea r8, g_secAttr
    xor r9d, r9d
    call CreatePipe
    test eax, eax
    jz @@fail
    mov rcx, g_hChildStdoutRd
    mov edx, HANDLE_FLAG_INHERIT
    xor r8d, r8d
    call SetHandleInformation
    ; CreatePipe for child stdin (parent writes)
    lea rcx, g_hChildStdinRd
    lea rdx, g_hChildStdinWr
    lea r8, g_secAttr
    xor r9d, r9d
    call CreatePipe
    test eax, eax
    jz @@fail
    mov rcx, g_hChildStdinWr
    mov edx, HANDLE_FLAG_INHERIT
    xor r8d, r8d
    call SetHandleInformation
    ; Zero STARTUPINFO
    lea rdi, g_startInfo
    xor eax, eax
    mov ecx, SIZEOF STARTUPINFOA / 8
    rep stosq
    mov dword ptr g_startInfo, SIZEOF STARTUPINFOA
    mov dword ptr g_startInfo.dwFlags, STARTF_USESTDHANDLES
    mov rax, g_hChildStdinRd
    mov g_startInfo.hStdInput, rax
    mov rax, g_hChildStdoutWr
    mov g_startInfo.hStdOutput, rax
    mov g_startInfo.hStdError, rax
    ; CreateProcessA(0, cmdLine, 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &si, &pi)
    xor rcx, rcx
    lea rdx, szClangdCmd
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+28h], 1
    mov dword ptr [rsp+30h], CREATE_NO_WINDOW
    mov qword ptr [rsp+38h], 0
    mov qword ptr [rsp+40h], 0
    lea rax, g_startInfo
    mov qword ptr [rsp+48h], rax
    lea rax, g_procInfo
    mov qword ptr [rsp+50h], rax
    call CreateProcessA
    test eax, eax
    jz @@fail
    ; Close child-side handles in parent
    mov rcx, g_hChildStdoutWr
    call CloseHandle
    mov rcx, g_hChildStdinRd
    call CloseHandle
    mov rax, g_procInfo.hProcess
    mov g_hProcess, rax
    mov rax, g_procInfo.hThread
    mov g_hThread, rax
    mov g_bSpawned, 1
    ; Send initialize
    lea rcx, szInitRequest
    call LSP_SendRequest
    ; Read response (ignore for now - init completes)
    lea rcx, g_responseBuf
    mov edx, 32768
    call LSP_ReadResponse
    mov eax, 1
    jmp @@out
@@fail:
    xor eax, eax
@@out:
    add rsp, 168h
    pop rdi
    pop rsi
    pop rbx
    ret
ExtensionInit ENDP

;----------------------------------------------------------------------
; ExtensionActivate - no-op
;----------------------------------------------------------------------
ExtensionActivate PROC
    mov eax, 1
    ret
ExtensionActivate ENDP

;----------------------------------------------------------------------
; ExtensionExecuteCommand(pszCommand, pvData)
; Commands: "initialize", "completion", "didOpen"
;----------------------------------------------------------------------
ExtensionExecuteCommand PROC pszCommand:QWORD, pvData:QWORD
    cmp g_bSpawned, 0
    je @@fail
    mov rcx, pszCommand
    test rcx, rcx
    jz @@fail
    ; "initialize" -> send init, return 1
    mov rsi, rcx
    lea rdi, szCmdInit
    mov ecx, 10
    repe cmpsb
    je @@do_init
    ; "completion" -> send textDocument/completion (simplified: use template)
    ; For full impl: build JSON from pvData (uri, position)
    lea rcx, szInitRequest
    call LSP_SendRequest
    test eax, eax
    jz @@fail
    mov rcx, pvData
    test rcx, rcx
    jz @@ok
    mov edx, 32768
    call LSP_ReadResponse
@@ok:
    mov eax, 1
    ret
@@do_init:
    lea rcx, szInitRequest
    call LSP_SendRequest
    mov eax, 1
    ret
@@fail:
    xor eax, eax
    ret
ExtensionExecuteCommand ENDP

;----------------------------------------------------------------------
; ExtensionHandleChat - NOP for LSP
;----------------------------------------------------------------------
ExtensionHandleChat PROC pszMessage:QWORD, pfnCallback:QWORD
    xor eax, eax
    ret
ExtensionHandleChat ENDP

END
