.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc

includelib kernel32.lib
includelib user32.lib

; ============================================================================
; PiFabric Core Runtime - Self-Optimizing Model Fabric
; Complete rewrite with improved handle management and method cycling
; ============================================================================

PUBLIC  PiFabric_Init
PUBLIC  PiFabric_Open
PUBLIC  PiFabric_Close
PUBLIC  PiFabric_Stream
PUBLIC  PiFabric_IsOpen
PUBLIC  PiFabric_GetStats
PUBLIC  PiFabric_SetTier
PUBLIC  PiFabric_CycleMethod
PUBLIC  PiFabric_AttachModelContext

; ============================================================================
; Constants
; ============================================================================

PIFABRIC_MAGIC          EQU 0x50694661     ; "PiFa"
PIFABRIC_METHOD_DISC    EQU 0x01
PIFABRIC_METHOD_MEMORY  EQU 0x02
PIFABRIC_METHOD_MMAP    EQU 0x04
PIFABRIC_METHOD_HYBRID  EQU 0x08

PIFABRIC_CHAIN_SEQUENTIAL EQU 0
PIFABRIC_CHAIN_ADAPTIVE   EQU 1
PIFABRIC_CHAIN_PARALLEL   EQU 2

PIFABRIC_TIER_QUALITY   EQU 0
PIFABRIC_TIER_BALANCED  EQU 1
PIFABRIC_TIER_FAST      EQU 2

; Compression algorithm IDs (aligned with piram_compression_hooks)
PIRAM_ALGO_NONE         EQU 0
PIRAM_ALGO_RLE          EQU 1
PIRAM_ALGO_HUFFMAN      EQU 2
PIRAM_ALGO_LZ77         EQU 3
PIRAM_ALGO_DEFLATE      EQU 4
PIRAM_ALGO_ADAPTIVE     EQU 5

; ============================================================================
; Structures
; ============================================================================

PiFabricHandle STRUCT
    dwMagic         DWORD ?
    hChain          DWORD ?
    pChain          DWORD ?
    pModelContext   DWORD ?
    dwMethod        DWORD ?
    dwChainMode     DWORD ?
    dwTier          DWORD ?
    dwFlags         DWORD ?
    dwState         DWORD ?
    fileSizeLo      DWORD ?
    fileSizeHi      DWORD ?
    nTensorCount    DWORD ?
    dwCompressPasses DWORD ?
    dwThreadCount   DWORD ?
    dwQuantFormat   DWORD ?    ; selected quant format (QUANT_FMT_*)
    reserved1       DWORD ?
PiFabricHandle ENDS

; ================================================================
; Global state
; ================================================================

.data

    g_PiFabricHandle PiFabricHandle <0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0>
    g_dwInitialized DWORD 0
    g_bCompInit    DWORD 0

; ================================================================
; External dependencies
; ================================================================

EXTERN  GGUFChain_LoadModel:PROC
EXTERN  GGUFChain_StreamChunk:PROC
EXTERN  GGUFChain_CloseModel:PROC
EXTERN  PiramHooks_Init:PROC
EXTERN  PiramHooks_SetAlgorithm:PROC
EXTERN  PiramHooks_EnableAdaptive:PROC
EXTERN  PiFabric_SelectQuantLevel:PROC

; ================================================================
; CODE SECTION
; ================================================================

.code

; ============================================================================
; PiFabric_Init - Initialize PiFabric system
; ============================================================================

PiFabric_Init PROC
    mov eax, g_dwInitialized
    test eax, eax
    jnz @already

    mov eax, 1
    mov g_dwInitialized, eax

    ; Initialize magic
    mov eax, OFFSET g_PiFabricHandle
    mov dword ptr [eax].PiFabricHandle.dwMagic, PIFABRIC_MAGIC

@already:
    mov eax, 1
    ret
PiFabric_Init ENDP

; ============================================================================
; PiFabric_Open - Open a model through the fabric
; in:
;   lpPath          = model file path
;   dwMethodMask    = enabled loading methods
;   dwChainMode     = chain mode
; out:
;   eax             = PiFabricHandle (global), 0 on error
; ============================================================================

PiFabric_Open PROC USES esi edi ebx lpPath:DWORD, dwMethodMask:DWORD, dwChainMode:DWORD

    call PiFabric_Init

    ; Get global handle
    mov edi, OFFSET g_PiFabricHandle

    ; Load model via GGUFChain
    push dwChainMode
    push dwMethodMask
    push lpPath
    call GGUFChain_LoadModel
    test eax, eax
    jz @fail

    mov [edi].PiFabricHandle.hChain, eax
    mov [edi].PiFabricHandle.pChain, eax
    mov [edi].PiFabricHandle.dwMethod, dwMethodMask
    mov [edi].PiFabricHandle.dwChainMode, dwChainMode
    mov [edi].PiFabricHandle.dwTier, PIFABRIC_TIER_BALANCED
    mov [edi].PiFabricHandle.dwState, 1
    mov [edi].PiFabricHandle.dwFlags, 1
    mov [edi].PiFabricHandle.dwCompressPasses, 4
    mov [edi].PiFabricHandle.dwThreadCount, 4

    mov eax, edi
    ret

@fail:
    xor eax, eax
    ret

PiFabric_Open ENDP

; ============================================================================
; PiFabric_Close - Close fabric handle
; ============================================================================

PiFabric_Close PROC USES esi edi ebx

    mov esi, OFFSET g_PiFabricHandle

    mov eax, [esi].PiFabricHandle.hChain
    test eax, eax
    jz @skip_close

    push eax
    call GGUFChain_CloseModel

@skip_close:
    ; Free model context if present
    mov edi, [esi].PiFabricHandle.pModelContext
    test edi, edi
    jz @skip_context
    push edi
    call GlobalFree

@skip_context:
    ; Clear handle
    xor eax, eax
    mov [esi].PiFabricHandle.hChain, eax
    mov [esi].PiFabricHandle.pModelContext, eax
    mov [esi].PiFabricHandle.dwState, 3

    mov eax, 1
    ret

PiFabric_Close ENDP

; ============================================================================
; PiFabric_IsOpen - Check if fabric is open
; ============================================================================

PiFabric_IsOpen PROC

    mov eax, OFFSET g_PiFabricHandle
    mov ecx, [eax].PiFabricHandle.dwState
    test ecx, ecx
    jz @closed

    mov eax, 1
    ret

@closed:
    xor eax, eax
    ret

PiFabric_IsOpen ENDP

; ============================================================================
; PiFabric_Stream - Stream tensor data
; in:
;   dwTensorIndex   = tensor index
;   pDst            = destination buffer
;   dwBytes         = bytes to read
; out:
;   eax             = bytes copied
; ============================================================================

PiFabric_Stream PROC USES esi edi ebx dwTensorIndex:DWORD, pDst:DWORD, dwBytes:DWORD

    mov esi, OFFSET g_PiFabricHandle

    mov eax, [esi].PiFabricHandle.dwState
    cmp eax, 1
    jne @fail

    mov eax, [esi].PiFabricHandle.hChain
    test eax, eax
    jz @fail

    ; Stream through chain
    push dwBytes
    push pDst
    push eax
    call GGUFChain_StreamChunk

    ret

@fail:
    xor eax, eax
    ret

PiFabric_Stream ENDP

; ============================================================================
; PiFabric_AttachModelContext - Attach resolved model context to fabric
; in:
;   pModelContext   = GGUF_MODEL_CONTEXT pointer
; out:
;   eax             = 1 success, 0 failure
; ============================================================================

PiFabric_AttachModelContext PROC USES esi pModelContext:DWORD

    mov esi, OFFSET g_PiFabricHandle
    mov eax, pModelContext
    mov [esi].PiFabricHandle.pModelContext, eax

    mov eax, 1
    ret

PiFabric_AttachModelContext ENDP

; ============================================================================
; PiFabric_GetStats - Get fabric statistics
; in:
;   pStatsBuffer    = output buffer
; out:
;   eax             = 1 success
; ============================================================================

PiFabric_GetStats PROC USES esi edi pStatsBuffer:DWORD

    mov esi, OFFSET g_PiFabricHandle
    mov edi, pStatsBuffer
    test edi, edi
    jz @fail

    mov eax, [esi].PiFabricHandle.dwTier
    mov [edi], eax
    mov eax, [esi].PiFabricHandle.dwCompressPasses
    mov [edi + 4], eax
    mov eax, [esi].PiFabricHandle.dwThreadCount
    mov [edi + 8], eax

    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

PiFabric_GetStats ENDP

; ============================================================================
; PiFabric_SetTier - Set quality tier
; in:
;   dwTier          = tier (0=quality, 1=balanced, 2=fast)
; out:
;   eax             = 1 success, 0 invalid
; ============================================================================

PiFabric_SetTier PROC USES esi dwTier:DWORD

    mov esi, OFFSET g_PiFabricHandle
    mov eax, dwTier

    cmp eax, 2
    ja @fail

    mov [esi].PiFabricHandle.dwTier, eax

    ; One-time compression init
    cmp g_bCompInit, 1
    je @init_done
    call PiramHooks_Init
    mov g_bCompInit, 1
@init_done:

    ; Select quantization format based on tier/latency (latency placeholder=0)
    push 0                      ; dwLatencyMs placeholder
    push dwTier
    call PiFabric_SelectQuantLevel
    mov [esi].PiFabricHandle.dwQuantFormat, eax

    ; Adjust compression and thread counts
    cmp eax, PIFABRIC_TIER_QUALITY
    jne @not_quality
    mov [esi].PiFabricHandle.dwCompressPasses, 11
    mov [esi].PiFabricHandle.dwThreadCount, 8
    push PIRAM_ALGO_DEFLATE
    call PiramHooks_SetAlgorithm
    push 0
    call PiramHooks_EnableAdaptive
    jmp @success

@not_quality:
    cmp eax, PIFABRIC_TIER_BALANCED
    jne @not_balanced
    mov [esi].PiFabricHandle.dwCompressPasses, 4
    mov [esi].PiFabricHandle.dwThreadCount, 4
    push PIRAM_ALGO_ADAPTIVE
    call PiramHooks_SetAlgorithm
    push 1
    call PiramHooks_EnableAdaptive
    jmp @success

@not_balanced:
    mov [esi].PiFabricHandle.dwCompressPasses, 2
    mov [esi].PiFabricHandle.dwThreadCount, 2
    push PIRAM_ALGO_RLE
    call PiramHooks_SetAlgorithm
    push 0
    call PiramHooks_EnableAdaptive

@success:
    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

PiFabric_SetTier ENDP

; ============================================================================
; PiFabric_CycleMethod - Cycle to next loading method
; out:
;   eax             = next method to try
; ============================================================================

PiFabric_CycleMethod PROC USES esi ebx

    mov esi, OFFSET g_PiFabricHandle
    mov eax, [esi].PiFabricHandle.fileSizeLo

    cmp eax, 0x1000000
    jb @use_memory

    cmp eax, 0x40000000
    jb @use_hybrid

    ; Large file: try disc
    mov eax, PIFABRIC_METHOD_DISC
    ret

@use_memory:
    mov eax, PIFABRIC_METHOD_MEMORY
    ret

@use_hybrid:
    mov eax, PIFABRIC_METHOD_HYBRID
    ret

PiFabric_CycleMethod ENDP

END

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
