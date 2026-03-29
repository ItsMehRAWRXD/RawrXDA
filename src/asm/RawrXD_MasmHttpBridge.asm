; RawrXD_MasmHttpBridge.asm
; Minimal MASM x64 HTTP POST bridge for model APIs.
; Export:
;   int RawrXD_MasmHttpPostA(
;       const char* host,
;       unsigned short port,
;       int secure,
;       const char* path,
;       const char* body,
;       char* outBuf,
;       unsigned int outCap);

OPTION CASEMAP:NONE

EXTERN InternetOpenA:PROC
EXTERN InternetConnectA:PROC
EXTERN HttpOpenRequestA:PROC
EXTERN HttpSendRequestA:PROC
EXTERN InternetReadFile:PROC
EXTERN InternetCloseHandle:PROC
EXTERN lstrlenA:PROC

PUBLIC RawrXD_MasmHttpPostA

INTERNET_OPEN_TYPE_PRECONFIG EQU 0
INTERNET_SERVICE_HTTP        EQU 3

INTERNET_FLAG_RELOAD         EQU 080000000h
INTERNET_FLAG_NO_CACHE_WRITE EQU 004000000h
INTERNET_FLAG_KEEP_CONNECTION EQU 000400000h
INTERNET_FLAG_SECURE         EQU 000800000h

.data
ALIGN 8
szUserAgent  BYTE "RawrXD-MASM-HTTP/1.0",0
szMethodPost BYTE "POST",0
szHeaderCT   BYTE "Content-Type: application/json",13,10,0
g_readBuffer BYTE 2048 DUP(0)

.code

RawrXD_MasmHttpPostA PROC
    ; RCX host, RDX port, R8D secure, R9 path
    ; [arg5 body, arg6 outBuf, arg7 outCap] on stack
    push rbx
    sub rsp, 70h

    ; Save incoming args to locals.
    mov [rsp+40h], rcx       ; host
    mov [rsp+48h], dx        ; port (WORD)
    mov [rsp+4Ch], r8d       ; secure
    mov [rsp+50h], r9        ; path

    mov qword ptr [rsp+58h], 0 ; hSession
    mov qword ptr [rsp+60h], 0 ; hConnect
    mov qword ptr [rsp+68h], 0 ; hRequest

    ; Validate pointers / cap.
    mov rax, [rsp+40h]
    test rax, rax
    jz fail
    mov rax, [rsp+50h]
    test rax, rax
    jz fail
    mov rax, [rsp+0A0h]      ; body
    test rax, rax
    jz fail
    mov rax, [rsp+0A8h]      ; outBuf
    test rax, rax
    jz fail
    mov eax, dword ptr [rsp+0B0h] ; outCap
    test eax, eax
    jz fail

    ; hSession = InternetOpenA(...)
    lea rcx, szUserAgent
    mov edx, INTERNET_OPEN_TYPE_PRECONFIG
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp+20h], 0
    call InternetOpenA
    test rax, rax
    jz fail
    mov [rsp+58h], rax

    ; hConnect = InternetConnectA(hSession, host, port, ...)
    mov rcx, [rsp+58h]
    mov rdx, [rsp+40h]
    movzx r8d, word ptr [rsp+48h]
    xor r9d, r9d
    mov qword ptr [rsp+20h], 0
    mov dword ptr [rsp+28h], INTERNET_SERVICE_HTTP
    mov dword ptr [rsp+30h], 0
    mov qword ptr [rsp+38h], 0
    call InternetConnectA
    test rax, rax
    jz cleanup
    mov [rsp+60h], rax

    ; Compute request flags.
    mov eax, INTERNET_FLAG_RELOAD
    or eax, INTERNET_FLAG_NO_CACHE_WRITE
    or eax, INTERNET_FLAG_KEEP_CONNECTION
    cmp dword ptr [rsp+4Ch], 0
    je no_secure
    or eax, INTERNET_FLAG_SECURE
no_secure:
    mov [rsp+34h], eax

    ; hRequest = HttpOpenRequestA(hConnect, "POST", path, ...)
    mov rcx, [rsp+60h]
    lea rdx, szMethodPost
    mov r8, [rsp+50h]
    xor r9d, r9d
    mov qword ptr [rsp+20h], 0
    mov qword ptr [rsp+28h], 0
    mov eax, [rsp+34h]
    mov dword ptr [rsp+30h], eax
    mov qword ptr [rsp+38h], 0
    call HttpOpenRequestA
    test rax, rax
    jz cleanup
    mov [rsp+68h], rax

    ; bodyLen = lstrlenA(body)
    mov rcx, [rsp+0A0h]
    call lstrlenA
    mov [rsp+2Ch], eax

    ; hLen = lstrlenA(header)
    lea rcx, szHeaderCT
    call lstrlenA
    mov [rsp+30h], eax

    ; HttpSendRequestA(hRequest, header, hLen, body, bodyLen)
    mov rcx, [rsp+68h]
    lea rdx, szHeaderCT
    mov r8d, dword ptr [rsp+30h]
    mov r9, [rsp+0A0h]
    mov eax, dword ptr [rsp+2Ch]
    mov dword ptr [rsp+20h], eax
    call HttpSendRequestA
    test eax, eax
    jz cleanup

    ; total = 0
    xor ebx, ebx

read_loop:
    mov dword ptr [rsp+38h], 0 ; bytesRead

    ; InternetReadFile(hRequest, g_readBuffer, 2048, &bytesRead)
    mov rcx, [rsp+68h]
    lea rdx, g_readBuffer
    mov r8d, 2048
    lea r9, [rsp+38h]
    call InternetReadFile
    test eax, eax
    jz cleanup_fail

    mov eax, dword ptr [rsp+38h]
    test eax, eax
    jz read_done

    ; remaining = outCap - 1 - total
    mov ecx, dword ptr [rsp+0B0h]
    dec ecx
    sub ecx, ebx
    jle read_done

    ; toCopy = min(bytesRead, remaining)
    mov edx, dword ptr [rsp+38h]
    cmp edx, ecx
    jbe copy_ready
    mov edx, ecx
copy_ready:

    ; dst = outBuf + total
    mov r10, [rsp+0A8h]
    lea r10, [r10+rbx]
    lea r11, g_readBuffer
    mov ecx, edx

copy_loop:
    test ecx, ecx
    jz copy_done
    mov al, byte ptr [r11]
    mov byte ptr [r10], al
    inc r11
    inc r10
    dec ecx
    jmp copy_loop

copy_done:
    add ebx, edx

    ; Stop if out buffer is full (minus NUL).
    mov eax, dword ptr [rsp+0B0h]
    dec eax
    cmp ebx, eax
    jae read_done
    jmp read_loop

read_done:
    ; NUL-terminate and return byte count.
    mov rax, [rsp+0A8h]
    mov byte ptr [rax+rbx], 0
    mov eax, ebx
    jmp cleanup

cleanup_fail:
    mov rax, [rsp+0A8h]
    test rax, rax
    jz fail
    mov byte ptr [rax], 0
    mov eax, -6
    jmp cleanup

fail:
    mov rax, [rsp+0A8h]
    test rax, rax
    jz fail_ret
    mov byte ptr [rax], 0
fail_ret:
    mov eax, -1

cleanup:
    mov rcx, [rsp+68h]
    test rcx, rcx
    jz close_connect
    call InternetCloseHandle

close_connect:
    mov rcx, [rsp+60h]
    test rcx, rcx
    jz close_session
    call InternetCloseHandle

close_session:
    mov rcx, [rsp+58h]
    test rcx, rcx
    jz done
    call InternetCloseHandle

done:
    add rsp, 70h
    pop rbx
    ret
RawrXD_MasmHttpPostA ENDP

END
