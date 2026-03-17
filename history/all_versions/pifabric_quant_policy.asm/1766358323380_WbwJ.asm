; pifabric_quant_policy.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc

PUBLIC  PiFabric_SelectQuantLevel

PiFabric_SelectQuantLevel PROC dwTargetTier:DWORD, dwLatencyMs:DWORD
    mov eax, dwTargetTier
    cmp eax, 0
    je  @high
    cmp eax, 1
    je  @mid
    jmp @low
@high:
    mov eax, 6      ; Q6 / high quality
    ret
@mid:
    mov eax, 4      ; Q4
    ret
@low:
    mov eax, 2      ; Q2
    ret
PiFabric_SelectQuantLevel ENDP

END