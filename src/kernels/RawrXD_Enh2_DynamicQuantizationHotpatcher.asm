; RawrXD_Enh2_DynamicQuantizationHotpatcher.asm
OPTION CASEMAP:NONE

RAWRXD_Q2_K EQU 2
RAWRXD_Q4_0 EQU 4
RAWRXD_Q8_0 EQU 8

.DATA
g_current_level DWORD RAWRXD_Q4_0
g_target_level  DWORD RAWRXD_Q4_0

.CODE
DynamicQuant_Initialize PROC
    mov g_current_level, edx
    mov g_target_level, edx
    xor eax, eax
    ret
DynamicQuant_Initialize ENDP

DynamicQuant_HotPatchLevel PROC
    cmp ecx, RAWRXD_Q2_K
    jb bad
    cmp ecx, RAWRXD_Q8_0
    ja bad
    mov g_target_level, ecx
    test r9d, r9d
    jz ok
    mov g_current_level, ecx
ok:
    xor eax, eax
    ret
bad:
    mov eax, 57h
    ret
DynamicQuant_HotPatchLevel ENDP

DynamicQuant_AdaptiveLayer PROC
    mov ecx, RAWRXD_Q4_0
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call DynamicQuant_HotPatchLevel
    ret
DynamicQuant_AdaptiveLayer ENDP
END
