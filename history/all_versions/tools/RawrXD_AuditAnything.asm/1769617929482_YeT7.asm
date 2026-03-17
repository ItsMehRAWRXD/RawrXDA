;  RawrXD_AuditAnything.asm  -- file or raw text → Ollama
extrn GetCommandLineW:proc
extrn GetStdHandle:proc
extrn WriteConsoleA:proc
extrn ExitProcess:proc
extrn CreateFileW:proc
extrn GetFileSize:proc
extrn VirtualAlloc:proc
extrn VirtualFree:proc
extrn ReadFile:proc
extrn CloseHandle:proc
extrn WSAStartup:proc
extrn socket:proc
extrn connect:proc
extrn send:proc
extrn recv:proc
extrn closesocket:proc
extrn WSACleanup:proc
extrn inet_pton:proc
extrn htons:proc
extrn lstrlenW:proc
extrn WideCharToMultiByte:proc

OllamaHost      db  "127.0.0.1",0
OllamaPort      equ 11434
HttpHeader      db  "POST /api/generate HTTP/1.1",13,10
                db  "Host: 127.0.0.1:11434",13,10
                db  "Content-Type: application/json",13,10
                db  "Content-Length: ",0
ContentLenStr   db  16 dup(0)
                db  13,10,13,10,0

JsonPrefix      db  '{"model":"dolphin3:latest","prompt":"Audit the following '
LangDetect      db  64 dup(0)
JsonMid         db  ' for obfuscation, shellcode, backdoors, AMSI bypass, dynamic invoke, crypto, IoCs. Return JSON: {summary,risks,techniques,IoCs}.\\n\\nCode:\\n'
RawText         db  524288 dup(0)   ; 512 KB max
JsonSuffix      db  "\\n\\nAudit:\\n","\"}",0

.code
main proc
        sub     rsp,48h
        mov     rcx,290h            ; WSADATA
        mov     rdx,2
        call    WSAStartup

        ; ---- grab whole CLI ----
        call    GetCommandLineW
        mov     rsi,rax
        
        ; skip exe name (find first space or end of string)
@@:     lodsw
        test    ax, ax
        jz      SkipDone
        cmp     ax, ' '
        jne     @B
SkipDone:
        ; rsi now points at arguments or end of string
        ; Skip leading spaces
@@:     lodsw
        cmp     ax, ' '
        je      @B
        sub     rsi, 2              ; back up to non-space

        ; convert to UTF-8 for RawText
        mov     rcx, rsi
        call    lstrlenW
        mov     r8, rax             ; cchWideChar
        mov     r9, rsi             ; lpWideCharStr
        
        mov     rcx, 65001          ; CodePage = CP_UTF8
        xor     rdx, rdx            ; dwFlags
        lea     rax, [RawText]
        mov     qword ptr [rsp+20h], rax ; lpMultiByteStr
        mov     qword ptr [rsp+28h], 524288 ; cbMultiByte
        xor     rax, rax
        mov     qword ptr [rsp+30h], rax ; lpDefaultChar
        mov     qword ptr [rsp+38h], rax ; lpUsedDefaultChar
        call    WideCharToMultiByte
        
        mov     FileSize, rax       ; bytes written
        lea     rbx, RawText
        mov     byte ptr [rbx+rax], 0

        ; ---- try to open as file ----
        mov     rcx, rsi
        mov     edx, 80000000h      ; GENERIC_READ
        mov     r8, 1               ; FILE_SHARE_READ
        xor     r9, r9              ; lpSecurityAttributes
        mov     qword ptr [rsp+20h], 3 ; OPEN_EXISTING
        mov     qword ptr [rsp+28h], 80h ; FILE_ATTRIBUTE_NORMAL
        mov     qword ptr [rsp+30h], 0 ; hTemplateFile
        call    CreateFileW
        mov     rbx, rax
        cmp     rax, -1
        je      BuildJson

        ; ---- file exists ----
        mov     rcx, rbx
        xor     edx, edx
        call    GetFileSize
        mov     FileSize, rax
        
        mov     rcx, rbx
        mov     rdx, offset RawText
        mov     r8, FileSize
        lea     r9, [BytesRead]
        mov     qword ptr [rsp+20h], 0
        call    ReadFile
        call    CloseHandle

BuildJson:
        ; ---- detect language ----
        mov     rcx, offset LangDetect
        call    DetectLang

        ; ---- assemble JSON ----
        mov     rsi, offset JsonPrefix
        mov     rdi, offset Payload
        call    strcpy
        dec     rdi

        mov     rsi, offset LangDetect
        call    strcpy
        dec     rdi

        mov     rsi, offset JsonMid
        call    strcpy
        dec     rdi

        mov     rsi, offset RawText
        mov     rcx, FileSize
        rep     movsb

        mov     rsi, offset JsonSuffix
        call    strcpy
        dec     rdi

        mov     rax, offset Payload
        sub     rdi, rax
        mov     PayloadLen, rdi

        ; ---- content-length ----
        mov     rcx, offset ContentLenStr
        mov     rdx, PayloadLen
        call    u64toa

        ; ---- TCP connect ----
        mov     ecx, 2               ; AF_INET
        mov     edx, 1               ; SOCK_STREAM
        mov     r8d, 6               ; IPPROTO_TCP
        call    socket
        mov     rbx, rax             ; sock

        mov     word ptr [rsp+20h], 2        ; AF_INET
        mov     ax, OllamaPort
        xchg    al, ah
        mov     word ptr [rsp+22h], ax       ; port
        lea     rcx, [rsp+24h]
        mov     rdx, offset OllamaHost
        call    inet_pton
        mov     dword ptr [rsp+24h], eax

        lea     rdx, [rsp+20h]
        mov     rcx, rbx
        mov     r8d, 16
        call    connect

        ; ---- send headers ----
        mov     rcx, rbx
        mov     rdx, offset HttpHeader
        mov     r8, offset ContentLenStr
        sub     r8, offset HttpHeader
        add     r8, 16 ; ContentLenStr
        add     r8, 4  ; \r\n\r\n
        xor     r9, r9
        call    send

        ; ---- send payload ----
        mov     rcx, rbx
        mov     rdx, offset Payload
        mov     r8, PayloadLen
        xor     r9, r9
        call    send

        ; ---- stream response ----
StreamLoop:
        mov     rcx, rbx
        lea     rdx, [rsp+50h]
        mov     r8d, 512
        xor     r9, r9
        call    recv
        test    rax, rax
        jle     Done
        
        mov     r8, rax
        mov     rcx, -11            ; STD_OUTPUT_HANDLE
        call    GetStdHandle
        mov     rcx, rax
        lea     rdx, [rsp+50h]
        lea     r9, [CharsWritten]
        mov     qword ptr [rsp+20h], 0
        call    WriteConsoleA
        jmp     StreamLoop

Done:
        call    closesocket
        call    WSACleanup
        xor     ecx, ecx
        call    ExitProcess
main endp

DetectLang proc
        push    rsi
        push    rdi
        mov     rdi, offset RawText
        mov     rcx, FileSize
        mov     al, '.'
        repne   scasb
        jne     .raw
        
        lea     rsi, [rdi]            ; after .
        
        ; ps1
        mov     rax, '1sp'
        cmp     word ptr [rsi], ax
        je      .ps
        ; js
        mov     rax, 'sj'
        cmp     word ptr [rsi], ax
        je      .js
        ; py
        mov     rax, 'yp'
        cmp     word ptr [rsi], ax
        je      .py
        ; cpp
        mov     rax, 'ppc'
        cmp     dword ptr [rsi], eax
        je      .cpp
        
        jmp     .raw

.ps:    mov     rsi, offset lang_ps
        jmp     .copy
.js:    mov     rsi, offset lang_js
        jmp     .copy
.py:    mov     rsi, offset lang_py
        jmp     .copy
.cpp:   mov     rsi, offset lang_cpp
        jmp     .copy
.raw:   mov     rsi, offset lang_raw
.copy:  mov     rdi, rcx
        call    strcpy
        pop     rdi
        pop     rsi
        ret
DetectLang endp

strcpy  proc
.copy:  lodsb
        stosb
        test    al, al
        jnz     .copy
        ret
strcpy  endp

u64toa  proc
        push    rbx
        push    rcx
        push    rdx
        mov     rax, rdx
        mov     rbx, 10
        mov     r8, rcx
        xor     rcx, rcx
.loop:  xor     rdx, rdx
        div     rbx
        add     dl, '0'
        push    rdx
        inc     rcx
        test    rax, rax
        jnz     .loop
        mov     rdi, r8
.store: pop     rax
        stosb
        loop    .store
        mov     byte ptr [rdi], 0
        pop     rdx
        pop     rcx
        pop     rbx
        ret
u64toa  endp

.data
FileSize        dq  0
BytesRead       dq  0
Payload         db  640000 dup(0)
PayloadLen      dq  0
PayloadEnd      label byte
lang_ps         db  "PowerShell", 0
lang_js         db  "JavaScript", 0
lang_py         db  "Python", 0
lang_cpp        db  "C/C++", 0
lang_raw        db  "source", 0
CharsWritten    dq  0
                end
