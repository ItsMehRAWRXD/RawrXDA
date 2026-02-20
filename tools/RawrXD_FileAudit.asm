;  RawrXD_FileAudit.asm  --  drop-in file-to-Ollama streamer
;  assemble:  ml64 /c /FoRawrXD_FileAudit.obj RawrXD_FileAudit.asm
;  link:      link /subsystem:console /entry:main RawrXD_FileAudit.obj ws2_32.lib

extrn  GetCommandLineW:proc
extrn  GetStdHandle:proc
extrn  WriteConsoleA:proc
extrn  ExitProcess:proc
extrn  CreateFileW:proc
extrn  GetFileSize:proc
extrn  VirtualAlloc:proc
extrn  VirtualFree:proc
extrn  ReadFile:proc
extrn  CloseHandle:proc
extrn  WSAStartup:proc
extrn  socket:proc
extrn  connect:proc
extrn  send:proc
extrn  recv:proc
extrn  closesocket:proc
extrn  WSACleanup:proc
extrn  inet_pton:proc
extrn  htons:proc

OllamaHost      db  "127.0.0.1",0
OllamaPort      equ 11434
OllamaPath      db  "POST /api/generate HTTP/1.1",13,10
                db  "Host: 127.0.0.1:11434",13,10
                db  "Content-Type: application/json",13,10
                db  "Content-Length: ",0
ContentLen      db  32 dup(0)
                db  13,10,13,10,0

JsonTemplate    db  '{"model":"dolphin3:latest","prompt":"Audit this PowerShell script for obfuscation, shellcode, AMSI bypass, dynamic invoke, etc. Return JSON: {summary,risks,techniques,ioCs}.\\n\\nScript:\\n'
JsonEnd         db  "\\n\\nAudit:\\n","\"}",0


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.code
main proc
        sub     rsp,28h
        mov     rcx,290h            ; WSADATA
        mov     rdx,2
        call    WSAStartup

        ; ---- get file path from CLI ----
        call    GetCommandLineW
        mov     rsi,rax
        add     rsi,2               ; skip "
        mov     rdi,offset FilePath
        mov     rcx,260
        rep     movsb

        ; ---- open file ----
        xor     ecx,ecx
        mov     rcx,offset FilePath
        xor     edx,edx
        mov     r8d,80000000h       ; GENERIC_READ
        xor     r9d,r9d
        mov     qword ptr [rsp+20h],3  ; OPEN_EXISTING
        call    CreateFileW
        mov     rbx,rax             ; hFile

        ; ---- size ----
        xor     edx,edx
        mov     rcx,rbx
        call    GetFileSize
        mov     r8,rax              ; fileSize
        mov     FileSize,rax

        ; ---- alloc ----
        xor     ecx,ecx
        mov     rdx,r8
        mov     r8d,3000h           ; MEM_COMMIT | MEM_RESERVE
        mov     r9d,4               ; PAGE_READWRITE
        call    VirtualAlloc
        mov     rdi,rax             ; buffer

        ; ---- read ----
        mov     rcx,rbx
        mov     rdx,rdi
        mov     r8,FileSize
        xor     r9d,r9d
        mov     qword ptr [rsp+20h],offset BytesRead
        call    ReadFile

        ; ---- build JSON payload ----
        mov     rsi,offset JsonTemplate
        mov     rdi,offset Payload
        call    strcpy
        mov     rsi,rdi
        mov     rdi,offset Payload
        add     rdi,PayloadLen
        mov     rcx,FileSize
        rep     movsb
        mov     rsi,offset JsonEnd
        call    strcpy
        mov     rax,offset Payload
        mov     rcx,offset PayloadEnd
        sub     rcx,rax
        mov     PayloadLen,rcx

        ; ---- content-length ----
        mov     rcx,offset ContentLen
        mov     rdx,PayloadLen
        call    u64toa

        ; ---- TCP connect ----
        mov     ecx,2               ; AF_INET
        mov     edx,1               ; SOCK_STREAM
        mov     r8d,6               ; IPPROTO_TCP
        call    socket
        mov     rbx,rax             ; sock

        mov     word ptr [rsp+20h],2        ; AF_INET
        mov     ax,OllamaPort
        xchg    al,ah
        mov     word ptr [rsp+22h],ax       ; port
        lea     rcx,[rsp+24h]
        mov     rdx,offset OllamaHost
        call    inet_pton
        mov     dword ptr [rsp+24h],eax

        lea     rdx,[rsp+20h]
        mov     rcx,rbx
        mov     r8d,16
        call    connect

        ; ---- send headers + payload ----
        mov     rcx,rbx
        mov     rdx,offset OllamaPath
        mov     r8d,OllamaPathLen
        call    send
        mov     rcx,rbx
        mov     rdx,offset Payload
        mov     r8d,PayloadLen
        call    send

        ; ---- stream response ----
StreamLoop:
        mov     rcx,rbx
        lea     rdx,[rsp+100h]
        mov     r8d,512
        call    recv
        test    rax,rax
        jle     Done
        mov     byte ptr [rsp+100h+rax],0
        mov     rcx,rax
        call    print
        jmp     StreamLoop

Done:
        call    closesocket
        call    WSACleanup
        xor     ecx,ecx
        call    ExitProcess
main endp

; ---------- helpers ----------
strcpy proc
        push    rdi
        push    rsi
.copy:  lodsb
        stosb
        test    al,al
        jnz     .copy
        pop     rsi
        pop     rdi
        ret
strcpy endp

u64toa proc
        push    rbx
        push    rcx
        push    rdx
        mov     rbx,10
        mov     r8,rcx
        mov     r9,rdx
        xor     rcx,rcx
.loop:  xor     rdx,rdx
        div     rbx
        add     dl,'0'
        push    rdx
        inc     rcx
        test    rax,rax
        jnz     .loop
        mov     rdi,r8
.store: pop     rax
        stosb
        loop    .store
        mov     byte ptr [rdi],0
        pop     rdx
        pop     rcx
        pop     rbx
        ret
u64toa endp

print proc
        push    rcx
        push    rdx
        mov     rcx,-11
        call    GetStdHandle
        mov     rcx,rax
        lea     rdx,[rsp+100h]
        mov     r8d,eax
        mov     r9d,offset CharsWritten
        call    WriteConsoleA
        pop     rdx
        pop     rcx
        ret
print endp

.data
FilePath        db  260 dup(0)
FileSize        dq  0
BytesRead       dq  0
Payload         db  640000 dup(0)   ; 640 KB max
PayloadLen      dq  0
PayloadEnd      label byte
OllamaPathLen   equ $ - OllamaPath
CharsWritten    dd  0
                end
