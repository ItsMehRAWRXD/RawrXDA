; pifabric_chunk_planner.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc

PUBLIC  PiFabric_SelectChunkSize

PiFabric_SelectChunkSize PROC dwSizeLow:DWORD, dwSizeHigh:DWORD
    mov eax, dwSizeHigh
    test eax, eax
    jnz  @big
    mov eax, dwSizeLow
    cmp eax, 0400000h       ; 4MB
    jbe  @tiny
    cmp eax, 4000000h       ; 64MB
    jbe  @small
    cmp eax, 40000000h      ; 1GB
    jbe  @medium
    jmp @large
@tiny:
    mov eax, 10000h         ; 64KB
    ret
@small:
    mov eax, 40000h         ; 256KB
    ret
@medium:
    mov eax, 100000h        ; 1MB
    ret
@large:
@big:
    mov eax, 400000h        ; 4MB
    ret
PiFabric_SelectChunkSize ENDP

END