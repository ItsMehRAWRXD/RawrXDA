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

.data
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
JsonMid         db  ' for obfuscation, RISKS, TECHNIQUES, IoCs. Return JSON: summary, risks, techniques, IoCs.', 10, 10, 'Code:', 10, 10, 0
RawText         db  524288 dup(0)   ; 512 KB max
JsonSuffix      db  10, 10, 'Audit:', 10, '"}', 0
Payload         db  640000 dup(0)
PayloadLen      dq  0
PayloadEnd      label byte
lang_ps         db  "PowerShell", 0
lang_js         db  "JavaScript", 0
lang_py         db  "Python", 0
lang_cpp        db  "C/C++", 0
lang_raw        db  "source", 0
CharsWritten    dq  0
FileSize        dq  0
BytesRead       dq  0

.code
main proc
        sub     rsp,58h             ; space for arguments + alignment
        mov     rcx, 290h           ; WSADATA size
        mov     rdx, 2
        call    WSAStartup

        ; ---- grab whole CLI ----
        call    GetCommandLineW
        mov     rsi, rax
        
        ; skip exe name
        lodsw
        cmp     ax, '"'
        je      SkipQuoted
@@:     cmp     ax, ' '
        je      SkipSpaces
        test    ax, ax
        jz      ArgsFound
        lodsw
        jmp     @B
SkipQuoted:
@@:     lodsw
        cmp     ax, '"'
        je      SkipDoneQuoted
        test    ax, ax
        jz      ArgsFound
        jmp     @B
SkipDoneQuoted:
        lodsw
SkipSpaces:
@@:     cmp     ax, ' '
        jne     BackUp
        lodsw
        jmp     @B
BackUp: sub     rsi, 2
ArgsFound:
        mov     qword ptr [rsp+48h], rsi ; save args UTF-16 pointer

        ; convert to UTF-8 for RawText (fallback/audit)
        mov     rcx, rsi
        call    lstrlenW
        mov     r8, rax             ; cchWideChar
        mov     rcx, 65001          ; CP_UTF8
        xor     rdx, rdx
        mov     r9, rsi             ; lpWideCharStr
        lea     rax, [RawText]
        mov     qword ptr [rsp+20h], rax
        mov     qword ptr [rsp+28h], 524288
        xor     rax, rax
        mov     qword ptr [rsp+30h], rax
        mov     qword ptr [rsp+38h], rax
        call    WideCharToMultiByte
        mov     FileSize, rax
        
        ; ---- try to open as file ----
        mov     rcx, qword ptr [rsp+48h]
        mov     edx, 80000000h      ; GENERIC_READ
        mov     r8, 1               ; FILE_SHARE_READ
        xor     r9, r9
        mov     qword ptr [rsp+20h], 3 ; OPEN_EXISTING
        mov     qword ptr [rsp+28h], 80h ; NORMAL
        mov     qword ptr [rsp+30h], 0
        call    CreateFileW
        mov     rbx, rax
        cmp     rax, -1
        je      BuildJson

        ; ---- file exists, read it ----
        mov     rcx, rbx
        xor     edx, edx
        call    GetFileSize
        mov     FileSize, rax
        
        mov     rcx, rbx
        mov     rdx, offset RawText
        mov     r8, rax
        lea     r9, [BytesRead]
        mov     qword ptr [rsp+20h], 0
        call    ReadFile
        call    CloseHandle

BuildJson:
        ; ---- assemble JSON ----
        mov     rcx, offset LangDetect
        call    DetectLang

        lea     rdi, [Payload]
        mov     rsi, offset JsonPrefix
        call    strcpy_no_null
        mov     rsi, offset LangDetect
        call    strcpy_no_null
        mov     rsi, offset JsonMid
        call    strcpy_no_null
        
        mov     rsi, offset RawText
        mov     rcx, FileSize
        rep     movsb
        
        mov     rsi, offset JsonSuffix
        call    strcpy_no_null
        
        lea     rax, [Payload]
        mov     rcx, rdi
        sub     rcx, rax
        mov     PayloadLen, rcx

        ; ---- content-length ----
        mov     rcx, offset ContentLenStr
        mov     rdx, PayloadLen
        call    u64toa

        ; ---- TCP connect ----
        mov     ecx, 2
        mov     edx, 1
        mov     r8d, 6
        call    socket
        mov     rbx, rax

        mov     word ptr [rsp+20h], 2
        mov     ax, OllamaPort
        xchg    al, ah
        mov     word ptr [rsp+22h], ax
        lea     rcx, [rsp+24h]
        mov     rdx, offset OllamaHost
        call    inet_pton
        mov     dword ptr [rsp+24h], eax

        lea     rdx, [rsp+20h]
        mov     rcx, rbx
        mov     r8d, 16
        call    connect

        ; ---- send request ----
        mov     rcx, rbx
        mov     rdx, offset HttpHeader
        mov     r8, offset ContentLenStr
        sub     r8, offset HttpHeader
        add     r8, 16 ; ContentLenStr
        add     r8, 4  ; \r\n\r\n
        xor     r9, r9
        call    send

        mov     rcx, rbx
        mov     rdx, offset Payload
        mov     r8, PayloadLen
        xor     r9, r9
        call    send

        ; ---- stream results ----
StreamLoop:
        mov     rcx, rbx
        lea     rdx, [rsp+50h]
        mov     r8d, 1024
        xor     r9, r9
        call    recv
        test    rax, rax
        jle     Done
        
        mov     r8, rax
        mov     rcx, -11
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
        add     rdi, rcx
        dec     rdi
        mov     al, '.'
        std
        repne   scasb
        cld
        jne     .raw
        
        lea     rsi, [rdi+2]
        ; quick checks for ps1, js, py, cpp
        ; (using small-endian constants)
        mov     al, [rsi]
        cmp     al, 'p'
        je      .p_start
        cmp     al, 'j'
        je      .j_start
        cmp     al, 'c'
        je      .c_start
        jmp     .raw
.p_start:
        mov     ax, [rsi+1]
        cmp     ax, '1s'
        je      .ps
        cmp     al, 'y'
        je      .py
        jmp     .raw
.j_start:
        cmp     byte ptr [rsi+1], 's'
        je      .js
        jmp     .raw
.c_start:
        mov     ax, [rsi+1]
        cmp     ax, 'pp'
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
@@:     lodsb
        stosb
        test    al, al
        jnz     @B
        pop     rdi
        pop     rsi
        ret
DetectLang endp

strcpy_no_null proc
@@:     lodsb
        test    al, al
        jz      @F
        stosb
        jmp     @B
@@:     ret
strcpy_no_null endp

strcpy proc
@@:     lodsb
        stosb
        test    al, al
        jnz     @B
        ret
strcpy endp

u64toa  proc
        push    rbx
        push    rcx
        push    rdx
        mov     rax, rdx
        mov     rbx, 10
        mov     r8, rcx
        xor     rcx, rcx
@@:     xor     rdx, rdx
        div     rbx
        add     dl, '0'
        push    rdx
        inc     rcx
        test    rax, rax
        jnz     @B
        
        mov     rdi, r8
@@:     pop     rax
        stosb
        loop    @B
        mov     byte ptr [rdi], 0
        pop     rdx
        pop     rcx
        pop     rbx
        ret
u64toa  endp

end
