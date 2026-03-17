; pifabric_compression_profiles.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC CompressionProfiles_Init
PUBLIC CompressionProfiles_Add
PUBLIC CompressionProfiles_GetCount
PUBLIC CompressionProfiles_Clear

.data
MAX_PROFILES EQU 32

Profile STRUCT
    id   DWORD ?
    ratio DWORD ?
Profile ENDS

Profiles STRUCT
    count DWORD ?
    items Profile MAX_PROFILES DUP(<0,0>)
Profiles ENDS

g_Profiles Profiles <0>

.code

CompressionProfiles_Init PROC
    mov g_Profiles.count,0
    mov eax,1
    ret
CompressionProfiles_Init ENDP

CompressionProfiles_Add PROC id:DWORD,ratio:DWORD
    mov eax,g_Profiles.count
    cmp eax,MAX_PROFILES
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Profile
    mov edx,OFFSET g_Profiles.items
    add edx,ecx
    mov [edx].Profile.id,id
    mov [edx].Profile.ratio,ratio
    inc g_Profiles.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
CompressionProfiles_Add ENDP

CompressionProfiles_GetCount PROC
    mov eax,g_Profiles.count
    ret
CompressionProfiles_GetCount ENDP

CompressionProfiles_Clear PROC
    mov g_Profiles.count,0
    mov eax,1
    ret
CompressionProfiles_Clear ENDP

END