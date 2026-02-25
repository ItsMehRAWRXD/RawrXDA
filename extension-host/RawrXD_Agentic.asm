; RawrXD_Agentic.asm - Native agent orchestrator (replaces rawrz-agentic + cursor-simple-ai)
; ExtensionExecuteCommand: agentic.compile -> CreateProcess build, capture output
; ExtensionHandleChat: POST to Ollama localhost:11434/api/generate, stream tokens
; Exports for RawrXD_ExtensionHost.

OPTION CASEMAP:NONE

INCLUDELIB kernel32.lib
INCLUDELIB winhttp.lib

; Win32 constants
STARTF_USESTDHANDLES EQU 100h
CREATE_NO_WINDOW     EQU 08000000h
GENERIC_READ         EQU 80000000h
GENERIC_WRITE        EQU 40000000h
FILE_ATTRIBUTE_NORMAL EQU 80h
PIPE_ACCESS_INBOUND  EQU 1
WINHTTP_ACCESS_TYPE_DEFAULT_PROXY EQU 0
WINHTTP_ADDREQ_FLAG_ADD           EQU 20000000h
WINHTTP_ADDREQ_FLAG_REPLACE       EQU 80000000h
HANDLE_FLAG_INHERIT               EQU 1

EXTERN CreatePipe:PROC
EXTERN SetHandleInformation:PROC
EXTERN CreateProcessA:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetStdHandle:PROC
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

.DATA
ALIGN 8
g_hMainWnd   DQ 0
szBuildCmd   BYTE "cmd /c msbuild /p:Configuration=Debug /verbosity:minimal",0
szAgenticModel BYTE "llama3.2",0

; Wide strings
ALIGN 2
wUserAgent   DW 'R','a','w','r','X','D','-','A','g','e','n','t','/','1','.','0',0
wLocalhost   DW 'l','o','c','a','l','h','o','s','t',0
wPathGen     DW '/','a','p','i','/','g','e','n','e','r','a','t','e',0
wPost        DW 'P','O','S','T',0
wContentType DW 'C','o','n','t','e','n','t','-','T','y','p','e',':',' '
             DW 'a','p','p','l','i','c','a','t','i','o','n','/','j','s','o','n',0

szJsonPrefix BYTE '{"model":"llama3.2","prompt":"',0
szJsonSuffix BYTE '","stream":true}',0
szRespKey    BYTE '"response":"',0

.DATA?
ALIGN 16
g_dwRead     DWORD ?
g_dwAvail    DWORD ?
g_chunk      BYTE 4096 DUP(?)
g_jsonBody   BYTE 32768 DUP(?)
g_tokenBuf   BYTE 256 DUP(?)
g_secAttr    SECURITY_ATTRIBUTES <>
g_startInfo  STARTUPINFOA <>
g_procInfo   PROCESS_INFORMATION <>
g_hPipeRd    QWORD ?
g_hPipeWr    QWORD ?
g_outputBuf  BYTE 8192 DUP(?)

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
; Build JSON body for Ollama
;----------------------------------------------------------------------
BuildJsonBody PROC
    push rbx
    push rsi
    push rdi
    push r15
    sub rsp, 48h
    mov rbx, rcx
    lea rdi, g_jsonBody
    mov r15, rdi
    lea rsi, szJsonPrefix
    mov ecx, 29
    rep movsb
    mov rcx, rbx
    call _strlen
    mov r10, rax
    xor edx, edx
    test r10, r10
    jz @@suffix
@@: movzx eax, byte ptr [rbx+rdx]
    cmp al, '"'
    je @@esc
    cmp al, '\'
    je @@esc
    cmp al, 13
    je @@n
    cmp al, 10
    je @@n
    mov [rdi], al
    inc rdi
@@n: inc edx
    cmp edx, r10d
    jb @B
    jmp @@suffix
@@esc: mov byte ptr [rdi], '\'
    inc rdi
    mov [rdi], al
    inc rdi
    inc edx
    cmp edx, r10d
    jb @B
@@suffix: lea rsi, szJsonSuffix
    mov ecx, 17
    rep movsb
    mov rax, rdi
    sub rax, r15
    add rsp, 48h
    pop r15
    pop rdi
    pop rsi
    pop rbx
    ret
BuildJsonBody ENDP

;----------------------------------------------------------------------
; Parse NDJSON, call callback per token
;----------------------------------------------------------------------
ParseTokens PROC
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
    lea rsi, szRespKey
    mov edi, edx
    mov r8d, 12
@@c: mov al, [rbx+rdi]
    cmp al, [rsi]
    jne @@next
    inc edi
    inc rsi
    dec r8d
    jnz @@c
    lea rsi, g_tokenBuf
    xor r8d, r8d
@@v: movzx eax, byte ptr [rbx+rdi]
    cmp al, '"'
    je @@got
    cmp al, '\'
    je @@e
    mov [rsi+r8], al
    inc r8
    inc edi
    cmp edi, ecx
    jb @@v
    jmp @@done
@@e: inc edi
    cmp edi, ecx
    jge @@done
    movzx eax, byte ptr [rbx+rdi]
    mov [rsi+r8], al
    inc r8
    inc edi
    jmp @@v
@@got: mov byte ptr [rsi+r8], 0
    test r8d, r8d
    jz @@next
    mov rcx, rsi
    mov rdx, r11
    call r10
@@next: inc edx
    jmp @B
@@done: add rsp, 48h
    pop rdi
    pop rsi
    pop rbx
    ret
ParseTokens ENDP

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
; ExtensionExecuteCommand(pszCommand, pvData) - agentic.compile, etc.
; agentic.compile: CreateProcess for build, capture stdout
;----------------------------------------------------------------------
ExtensionExecuteCommand PROC pszCommand:QWORD, pvData:QWORD
    push rbx
    push rsi
    push rdi
    sub rsp, 188h
    mov rbx, rcx
    mov r11, rdx
    test rbx, rbx
    jz @@fail
    ; Check for "agentic.compile"
    lea rsi, szAgenticCompile
    mov edi, 15
@@cmp: mov al, [rbx]
    cmp al, [rsi]
    jne @@fail
    inc rbx
    inc rsi
    dec edi
    jnz @@cmp
    ; CreatePipe for stdout
    lea rcx, g_secAttr
    mov dword ptr [rcx], 24
    mov qword ptr [rcx+8], 0
    mov dword ptr [rcx+16], 1
    lea rdx, g_hPipeRd
    lea r8, g_hPipeWr
    lea r9, g_secAttr
    call CreatePipe
    test eax, eax
    jz @@fail
    mov rcx, g_hPipeRd
    mov edx, HANDLE_FLAG_INHERIT
    xor r8d, r8d
    call SetHandleInformation
    ; Zero STARTUPINFO
    lea rdi, g_startInfo
    xor eax, eax
    mov ecx, 104/8
    rep stosq
    mov dword ptr g_startInfo, 104
    mov dword ptr g_startInfo.dwFlags, STARTF_USESTDHANDLES
    mov rax, g_hPipeWr
    mov g_startInfo.hStdOutput, rax
    mov g_startInfo.hStdError, rax
    ; CreateProcess
    xor rcx, rcx
    lea rdx, szBuildCmd
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+28h], 1
    mov dword ptr [rsp+30h], CREATE_NO_WINDOW
    mov qword ptr [rsp+38h], 0
    mov qword ptr [rsp+40h], r11
    lea rax, g_startInfo
    mov qword ptr [rsp+48h], rax
    lea rax, g_procInfo
    mov qword ptr [rsp+50h], rax
    call CreateProcessA
    test eax, eax
    jz @@close_pipe
    mov rcx, g_hPipeWr
    call CloseHandle
    ; Read stdout
    mov rcx, g_hPipeRd
    lea rdx, g_outputBuf
    mov r8d, 8191
    lea r9, g_dwRead
    mov qword ptr [rsp+28h], 0
    call ReadFile
    mov rcx, g_hPipeRd
    call CloseHandle
    mov rcx, g_procInfo.hProcess
    call CloseHandle
    mov rcx, g_procInfo.hThread
    call CloseHandle
    mov eax, 1
    jmp @@out
@@close_pipe: mov rcx, g_hPipeWr
    call CloseHandle
    mov rcx, g_hPipeRd
    call CloseHandle
@@fail: xor eax, eax
@@out: add rsp, 188h
    pop rdi
    pop rsi
    pop rbx
    ret
szAgenticCompile BYTE "agentic.compile",0
ExtensionExecuteCommand ENDP

;----------------------------------------------------------------------
; ExtensionHandleChat(pszMessage, pfnCallback) - Ollama streaming
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
    mov rcx, r12
    call BuildJsonBody
    test rax, rax
    jz @@fail
    mov ebx, eax
    lea rcx, wUserAgent
    xor edx, edx
    xor r8, r8
    xor r9, r9
    call WinHttpOpen
    test rax, rax
    jz @@fail
    mov rsi, rax
    mov rcx, rsi
    lea rdx, wLocalhost
    mov r8d, 11434
    xor r9d, r9d
    call WinHttpConnect
    test rax, rax
    jz @@cls
    mov rdi, rax
    mov rcx, rdi
    lea rdx, wPost
    lea r8, wPathGen
    xor r9d, r9d
    mov qword ptr [rsp+28h], 0
    mov qword ptr [rsp+30h], 0
    call WinHttpOpenRequest
    test rax, rax
    jz @@clc
    push rax
    mov rcx, rax
    lea rdx, wContentType
    mov r8d, -1
    mov r9d, WINHTTP_ADDREQ_FLAG_ADD OR WINHTTP_ADDREQ_FLAG_REPLACE
    call WinHttpAddRequestHeaders
    pop rcx
    push rcx
    mov edx, 5000
    mov r8d, 10000
    mov r9d, 30000
    mov dword ptr [rsp+28h], 300000
    call WinHttpSetTimeouts
    pop rcx
    push rcx
    xor rdx, rdx
    xor r8d, r8d
    lea r9, g_jsonBody
    mov dword ptr [rsp+28h], ebx
    mov dword ptr [rsp+30h], ebx
    call WinHttpSendRequest
    test eax, eax
    jz @@clr
    pop rcx
    push rcx
    xor rdx, rdx
    call WinHttpReceiveResponse
    test eax, eax
    jz @@clr
@@rl: pop rcx
    push rcx
    lea rdx, g_dwAvail
    xor r8d, r8d
    call WinHttpQueryDataAvailable
    test eax, eax
    jz @@clr
    cmp g_dwAvail, 0
    je @@ok
    pop rcx
    push rcx
    lea rdx, g_chunk
    mov r8d, g_dwAvail
    cmp r8d, 4096
    jbe @@rd
    mov r8d, 4096
@@rd: lea r9, g_dwRead
    mov qword ptr [rsp+28h], 0
    call WinHttpReadData
    test eax, eax
    jz @@clr
    cmp g_dwRead, 0
    je @@ok
    lea rcx, g_chunk
    mov edx, g_dwRead
    mov r8, r13
    mov r9, r12
    call ParseTokens
    jmp @@rl
@@ok: pop rcx
    call WinHttpCloseHandle
    mov rcx, rdi
    call WinHttpCloseHandle
    mov rcx, rsi
    call WinHttpCloseHandle
    mov eax, 1
    jmp @@out
@@clr: pop rcx
    call WinHttpCloseHandle
@@clc: mov rcx, rdi
    call WinHttpCloseHandle
@@cls: mov rcx, rsi
    call WinHttpCloseHandle
@@fail: xor eax, eax
@@out: add rsp, 88h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ExtensionHandleChat ENDP

END
