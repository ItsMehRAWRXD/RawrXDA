; pifabric_stats.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc

PUBLIC  PiFabric_Stats_Reset
PUBLIC  PiFabric_Stats_Update
PUBLIC  PiFabric_Stats_Copy

PiFabricStats STRUCT
    dwTokens      DWORD ?
    dwBytes       DWORD ?
    dwCalls       DWORD ?
    dwErrors      DWORD ?
    dwLatencyMs   DWORD ?
    dwFlags       DWORD ?
PiFabricStats ENDS

.data
g_FabricStats  PiFabricStats <0,0,0,0,0,0>

.code

PiFabric_Stats_Reset PROC
    lea eax, g_FabricStats
    xor ecx, ecx
    mov [eax].PiFabricStats.dwTokens, ecx
    mov [eax].PiFabricStats.dwBytes,  ecx
    mov [eax].PiFabricStats.dwCalls,  ecx
    mov [eax].PiFabricStats.dwErrors, ecx
    mov [eax].PiFabricStats.dwLatencyMs, ecx
    mov [eax].PiFabricStats.dwFlags, ecx
    mov eax, 1
    ret
PiFabric_Stats_Reset ENDP

PiFabric_Stats_Update PROC dwBytes:DWORD, dwTokens:DWORD, dwLatencyMs:DWORD, dwErrorFlag:DWORD
    lea eax, g_FabricStats
    add [eax].PiFabricStats.dwBytes,  dwBytes
    add [eax].PiFabricStats.dwTokens, dwTokens
    inc [eax].PiFabricStats.dwCalls
    add [eax].PiFabricStats.dwLatencyMs, dwLatencyMs
    test dwErrorFlag, dwErrorFlag
    jz   @ok
    inc [eax].PiFabricStats.dwErrors
@ok:
    mov eax, 1
    ret
PiFabric_Stats_Update ENDP

PiFabric_Stats_Copy PROC pOut:DWORD
    mov eax, pOut
    test eax, eax
    jz   @fail
    lea esi, g_FabricStats
    mov edi, eax
    mov ecx, SIZEOF PiFabricStats
    rep movsb
    mov eax, 1
    ret
@fail:
    xor eax, eax
    ret
PiFabric_Stats_Copy ENDP

END