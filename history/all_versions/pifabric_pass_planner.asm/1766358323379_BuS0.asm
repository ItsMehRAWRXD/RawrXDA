; pifabric_pass_planner.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc

PUBLIC  PiFabric_SelectPassCount

PiFabric_SelectPassCount PROC dwSizeLow:DWORD, dwSizeHigh:DWORD, dwTargetMB:DWORD
    mov eax, dwSizeHigh
    test eax, eax
    jnz  @huge
    mov eax, dwSizeLow
    cmp eax, 10000000h          ; ~256MB
    jbe  @light
    cmp eax, 40000000h          ; ~1GB
    jbe  @mid
    jmp  @heavy
@light:
    mov eax, 4
    ret
@mid:
    mov eax, 7
    ret
@heavy:
    mov eax, 11
    ret
@huge:
    mov eax, 11
    ret
PiFabric_SelectPassCount ENDP

END