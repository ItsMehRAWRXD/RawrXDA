; =============================================================================
; RawrXD_QUICFS.asm - QUIC STREAM Frame to Filesystem Mapping
; Maps QUIC STREAM frames directly to local file I/O for remote development
; =============================================================================

INCLUDE ksamd64.inc

EXTRN   WriteFile:PROC
EXTRN   SetFilePointerEx:PROC
EXTRN   CreateFileA:PROC
EXTRN   CloseHandle:PROC

; QUIC frame types
QUIC_STREAM_FRAME       EQU 0x08
QUIC_STREAM_FIN         EQU 1
QUIC_STREAM_LEN         EQU 2
QUIC_STREAM_OFF         EQU 4

; Win32 constants
FILE_BEGIN              EQU 0
GENERIC_WRITE           EQU 0x40000000
FILE_SHARE_READ         EQU 1
OPEN_ALWAYS             EQU 4

.code

PUBLIC QUICStreamToFile
QUICStreamToFile PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx        ; frame
    mov     rdi, rdx        ; len
    mov     rsi, r8         ; file handle

    ; Parse frame type (first byte)
    movzx   eax, BYTE PTR [rbx]
    test    al, 80h         ; Check STREAM bit (0x80)
    jz      @@not_stream

    ; Extract Stream ID (varint)
    inc     rbx
    dec     rdi
    call    ReadVarInt      ; Returns stream ID in RAX, advances RBX

    ; Extract Offset if OFF bit set
    test    al, QUIC_STREAM_OFF
    jz      @@no_offset
    call    ReadVarInt      ; Offset
    ; Seek file to this offset
    mov     rcx, rsi
    mov     rdx, rax        ; Offset
    mov     r8d, FILE_BEGIN
    call    SetFilePointerEx

@@no_offset:
    ; Extract Length if LEN bit set, else use remaining
    test    al, QUIC_STREAM_LEN
    jz      @@use_remaining
    call    ReadVarInt      ; Length
    mov     rdi, rax
    jmp     @@write_data

@@use_remaining:
    ; Use rest of packet as data

@@write_data:
    ; WriteFile(handle, buffer, len, &written, NULL)
    mov     rcx, rsi
    mov     rdx, rbx        ; Data start
    mov     r8, rdi         ; Length
    lea     r9, [rsp+32]    ; &written
    mov     QWORD PTR [rsp+48], 0  ; lpOverlapped
    call    WriteFile

    mov     rax, 1          ; Success

@@exit:
    lea     rsp, [rbp - 24]
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret

@@not_stream:
    xor     eax, eax        ; Not a stream frame
    jmp     @@exit
QUICStreamToFile ENDP

; Read QUIC variable-length integer
; rbx = buffer ptr (updated), returns rax = value
ReadVarInt PROC
    movzx   eax, BYTE PTR [rbx]
    test    al, 0C0h
    jz      @@1byte
    cmp     al, 40h
    jb      @@2byte
    cmp     al, 80h
    jb      @@4byte

    ; 8-byte
    mov     rax, [rbx]
    bswap   rax
    and     rax, 03FFFFFFFFFFFFFFh
    add     rbx, 8
    ret

@@1byte:
    and     eax, 3Fh
    inc     rbx
    ret

@@2byte:
    movzx   eax, WORD PTR [rbx]
    xchg    al, ah
    and     eax, 3FFFh
    add     rbx, 2
    ret

@@4byte:
    mov     eax, [rbx]
    bswap   eax
    and     eax, 3FFFFFFFh
    add     rbx, 4
    ret
ReadVarInt ENDP

END