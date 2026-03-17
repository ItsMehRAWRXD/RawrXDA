\.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc

includelib kernel32.lib
includelib user32.lib

; ----------------------------------------------------------------
; PiFabric Core Runtime
; ----------------------------------------------------------------

PUBLIC  PiFabric_Init
PUBLIC  PiFabric_Open
PUBLIC  PiFabric_Stream
PUBLIC  PiFabric_Close

; ----------------------------------------------------------------
; Data structures
; ----------------------------------------------------------------

PiFabricHandle STRUCT
    hChain      DWORD ?
    pChain      DWORD ?
    dwMethod    DWORD ?
    dwChainMode DWORD ?
    dwTier      DWORD ?
    dwFlags     DWORD ?
PiFabricHandle ENDS

; ----------------------------------------------------------------
; Global state
; ----------------------------------------------------------------

data

g_PiFabricHandle PiFabricHandle <0,0,0,0,0,0>

; ----------------------------------------------------------------
; External dependencies
; ----------------------------------------------------------------

EXTERN  GGUFChain_LoadModel:PROC
EXTERN  GGUFChain_StreamChunk:PROC
EXTERN  GGUFChain_CloseModel:PROC
EXTERN  PiFabric_ThreadPool_Queue:PROC
EXTERN  PiFabric_ThreadPool_Init:PROC
EXTERN  PiFabric_ThreadPool_Shutdown:PROC
EXTERN  PiFabric_Stats_Reset:PROC
EXTERN  PiFabric_Stats_Update:PROC

; ----------------------------------------------------------------
; PiFabric_Init
; ----------------------------------------------------------------

PiFabric_Init PROC
    ; Initialize thread pool and stats
    invoke PiFabric_ThreadPool_Init, 4
    invoke PiFabric_Stats_Reset
    mov eax, 1
    ret
PiFabric_Init ENDP

; ----------------------------------------------------------------
; PiFabric_Open
; ----------------------------------------------------------------

PiFabric_Open PROC USES esi edi ebx lpPath:DWORD, dwMethodMask:DWORD, dwChainMode:DWORD
    ; Load model via GGUFChain
    push dwChainMode
    push dwMethodMask
    push lpPath
    call GGUFChain_LoadModel
    test eax, eax
    jz @fail
    mov esi, eax ; chain handle
    ; Store in global handle
    mov edi, OFFSET g_PiFabricHandle
    mov [edi].PiFabricHandle.hChain, esi
    mov [edi].PiFabricHandle.pChain, esi
    mov [edi].PiFabricHandle.dwMethod, dwMethodMask
    mov [edi].PiFabricHandle.dwChainMode, dwChainMode
    mov [edi].PiFabricHandle.dwTier, 0
    mov [edi].PiFabricHandle.dwFlags, 1
    mov eax, esi
    ret
@fail:
    xor eax, eax
    ret
PiFabric_Open ENDP

; ----------------------------------------------------------------
; PiFabric_Stream
; ----------------------------------------------------------------

PiFabric_Stream PROC USES esi edi ebx hFabric:DWORD, pDst:DWORD, dwBytes:DWORD
    mov esi, hFabric
    test esi, esi
    jz @fail
    ; Get chain handle
    mov edi, [esi].PiFabricHandle.hChain
    test edi, edi
    jz @fail
    ; Stream chunk
    push dwBytes
    push pDst
    push edi
    call GGUFChain_StreamChunk
    ; Update stats
    push dwBytes
    push 0 ; tokens placeholder
    push 0 ; latency placeholder
    push 0 ; error flag
    call PiFabric_Stats_Update
    mov eax, dwBytes
    ret
@fail:
    xor eax, eax
    ret
PiFabric_Stream ENDP

; ----------------------------------------------------------------
; PiFabric_Close
; ----------------------------------------------------------------

PiFabric_Close PROC USES esi edi ebx hFabric:DWORD
    mov esi, hFabric
    test esi, esi
    jz @done
    mov edi, [esi].PiFabricHandle.hChain
    test edi, edi
    jz @done
    ; Close chain
    push edi
    call GGUFChain_CloseModel
    ; Reset handle
    mov [esi].PiFabricHandle.hChain, 0
    mov [esi].PiFabricHandle.pChain, 0
    mov [esi].PiFabricHandle.dwFlags, 0
@done:
    xor eax, eax
    ret
PiFabric_Close ENDP

END
