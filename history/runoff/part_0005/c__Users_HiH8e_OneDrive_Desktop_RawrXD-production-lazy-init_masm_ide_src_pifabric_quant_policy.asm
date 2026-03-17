; pifabric_quant_policy.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc

; Quantization format IDs (aligned with reverse quant engine)
QUANT_FMT_F32       EQU 0
QUANT_FMT_F16       EQU 1
QUANT_FMT_Q4_0      EQU 2
QUANT_FMT_Q4_1      EQU 3
QUANT_FMT_Q2_0      EQU 16

PUBLIC  PiFabric_SelectQuantLevel

PiFabric_SelectQuantLevel PROC dwTargetTier:DWORD, dwLatencyMs:DWORD
    mov eax, dwTargetTier
    cmp eax, 0
    je  @high
    cmp eax, 1
    je  @mid
    jmp @low
@high:
    mov eax, QUANT_FMT_F16   ; highest quality: unquantized FP16
    ret
@mid:
    mov eax, QUANT_FMT_Q4_0  ; mid quality: Q4
    ret
@low:
    mov eax, QUANT_FMT_Q2_0  ; lowest quality: Q2
    ret
PiFabric_SelectQuantLevel ENDP

END