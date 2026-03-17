; pifabric_compression_lab.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC CompressionLab_Init
PUBLIC CompressionLab_AddProfile
PUBLIC CompressionLab_Run
PUBLIC CompressionLab_Clear

.data
MAX_PROFILES EQU 16

CompressionProfile STRUCT
    id   DWORD ?
    ratio DWORD ?
CompressionProfile ENDS

CompressionLab STRUCT
    count DWORD ?
    profiles CompressionProfile MAX_PROFILES DUP(<0,0>)
CompressionLab ENDS

g_Lab CompressionLab <0>

.code

CompressionLab_Init PROC
    mov g_Lab.count,0
    mov eax,1
    ret
CompressionLab_Init ENDP

CompressionLab_AddProfile PROC id:DWORD,ratio:DWORD
    mov eax,g_Lab.count
    cmp eax,MAX_PROFILES
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF CompressionProfile
    mov edx,OFFSET g_Lab.profiles
    add edx,ecx
    mov [edx].CompressionProfile.id,id
    mov [edx].CompressionProfile.ratio,ratio
    inc g_Lab.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
CompressionLab_AddProfile ENDP

CompressionLab_Run PROC
    ; simplistic run: just return count
    mov eax,g_Lab.count
    ret
CompressionLab_Run ENDP

CompressionLab_Clear PROC
    mov g_Lab.count,0
    mov eax,1
    ret
CompressionLab_Clear ENDP

END