; =========================
; FILE: rawr_ipc.asm  (SYSTEM 5: IPC framing over named pipe)
; =========================
option casemap:none
include rawr_rt.inc
include rawr_imports.inc

; Frame:
;   u32 magic = 'RXIP' (0x50495852)
;   u32 type
;   u32 size
;   u32 crc  (simple xor32 over payload dwords, tail bytes folded)
;   payload[size]
RXIP_MAGIC  equ 050495852h

.code

; UINT32 rawr_crc32_xor(LPCVOID p, UINT32 n)
; RCX=p, EDX=n
public rawr_crc32_xor
rawr_crc32_xor proc
    ; leaf
    xor     eax, eax
    test    rcx, rcx
    jz      _done
    test    edx, edx
    jz      _done

    mov     r8, rcx
    mov     r9d, edx

    ; dword loop
    mov     ecx, r9d
    shr     ecx, 2
    jz      _tail
_dwl:
    xor     eax, dword ptr [r8]
    add     r8, 4
    dec     ecx
    jnz     _dwl

_tail:
    and     r9d, 3
    jz      _done
    ; fold remaining bytes into low bits
    mov     ecx, r9d
    xor     edx, edx
_byt:
    movzx   edx, byte ptr [r8]
    xor     eax, edx
    inc     r8
    dec     ecx
    jnz     _byt
_done:
    ret
rawr_crc32_xor endp

; HANDLE rawr_pipe_server_create(LPCWSTR name, DWORD inbuf, DWORD outbuf)
; RCX=name, EDX=inbuf, R8D=outbuf
public rawr_pipe_server_create
rawr_pipe_server_create proc
    RAWR_PROLOGUE 0

    ; CreateNamedPipeW(name, duplex, byte, 1 inst, outbuf, inbuf, 0, NULL)
    mov     r9d, 1
    mov     dword ptr [rsp+20h], r8d         ; nOutBufferSize
    mov     dword ptr [rsp+28h], edx         ; nInBufferSize
    mov     qword ptr [rsp+30h], 0           ; default timeout
    mov     qword ptr [rsp+38h], 0           ; lpSecurityAttributes
    mov     edx, (PIPE_ACCESS_DUPLEX)
    mov     r8d, (PIPE_TYPE_BYTE or PIPE_READMODE_BYTE or PIPE_WAIT)
    call    CreateNamedPipeW
    RAWR_EPILOGUE 0
rawr_pipe_server_create endp

; BOOL rawr_pipe_server_accept(HANDLE h)
; RCX=h
public rawr_pipe_server_accept
rawr_pipe_server_accept proc
    RAWR_PROLOGUE 0
    ; ConnectNamedPipe(h, NULL)  -> if client already connected, returns 0 with ERROR_PIPE_CONNECTED
    xor     rdx, rdx
    call    ConnectNamedPipe
    test    eax, eax
    jnz     _ok
    call    GetLastError
    ; ERROR_PIPE_CONNECTED = 535
    cmp     eax, 535
    je      _ok
    xor     eax, eax
    RAWR_EPILOGUE 0
_ok:
    mov     eax, 1
    RAWR_EPILOGUE 0
rawr_pipe_server_accept endp

; BOOL rawr_pipe_send_frame(HANDLE h, DWORD type, LPCVOID payload, DWORD size)
; RCX=h, EDX=type, R8=payload, R9D=size
public rawr_pipe_send_frame
rawr_pipe_send_frame proc
    RAWR_PROLOGUE 40h
    RAWR_SAVE_NONVOL

    mov     r12, rcx        ; h
    mov     r13d, edx       ; type
    mov     r14, r8         ; payload
    mov     r15d, r9d       ; size

    ; build header at [rsp+20h] (16 bytes)
    mov     dword ptr [rsp+20h], RXIP_MAGIC
    mov     dword ptr [rsp+24h], r13d
    mov     dword ptr [rsp+28h], r15d

    ; crc
    mov     rcx, r14
    mov     edx, r15d
    call    rawr_crc32_xor
    mov     dword ptr [rsp+2Ch], eax

    ; WriteFile(h, header, 16, &w, NULL)
    mov     rcx, r12
    lea     rdx, [rsp+20h]
    mov     r8d, 16
    lea     r9,  [rsp+30h]
    mov     qword ptr [rsp+38h], 0
    call    WriteFile
    test    eax, eax
    jz      _fail

    ; Write payload if size>0
    test    r15d, r15d
    jz      _ok
    mov     rcx, r12
    mov     rdx, r14
    mov     r8d, r15d
    lea     r9,  [rsp+30h]
    mov     qword ptr [rsp+38h], 0
    call    WriteFile
    test    eax, eax
    jz      _fail

_ok:
    mov     eax, 1
    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 40h

_fail:
    xor     eax, eax
    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 40h
rawr_pipe_send_frame endp

end