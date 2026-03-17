; pifabric_gguf_loader_integration.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

; ----------------------------------------------------------------
; Wrapper around existing GGUF loader to resolve data pointers
; ----------------------------------------------------------------

PUBLIC  GGUF_LoaderWithResolver
PUBLIC  GGUF_PatchExistingLoader

EXTERN  GGUF_LoadParsed:PROC
EXTERN  GGUF_ResolverComplete:PROC

GGUFParsedInfo STRUCT
    pBase       DWORD ?
    pHeader     DWORD ?
    pKVMeta     DWORD ?
    pTensorMeta DWORD ?
    pTensorArr  DWORD ?
    nTensors    DWORD ?
    headerSize  DWORD ?
    fileSizeLo  DWORD ?
    fileSizeHi  DWORD ?
GGUFParsedInfo ENDS

GGUFModel STRUCT
    pBase       DWORD ?
    pHeader     DWORD ?
    pKVMeta     DWORD ?
    pTensorMeta DWORD ?
    pData       DWORD ?
    fileSizeLo  DWORD ?
    fileSizeHi  DWORD ?
    nTensors    DWORD ?
    pTensorArr  DWORD ?
    dwFlags     DWORD ?
GGUFModel ENDS

.code

GGUF_LoaderWithResolver PROC USES esi edi ebx lpPath:DWORD
    ; Allocate parsed info
    sub esp, SIZEOF GGUFParsedInfo
    mov esi, esp

    ; Call existing loader
    push esi
    push lpPath
    call GGUF_LoadParsed
    test eax, eax
    jz @fail

    ; Resolve pointers
    push [esi].GGUFParsedInfo.pTensorArr
    push [esi].GGUFParsedInfo.nTensors
    push [esi].GGUFParsedInfo.pBase
    push [esi].GGUFParsedInfo.headerSize
    push [esi].GGUFParsedInfo.fileSizeLo
    push [esi].GGUFParsedInfo.fileSizeHi
    push [esi].GGUFParsedInfo.pHeader
    push [esi].GGUFParsedInfo.pKVMeta
    push [esi].GGUFParsedInfo.pTensorMeta
    push esi
    call GGUF_ResolverComplete
    test eax, eax
    jz @free

    ; Allocate model struct
    push SIZEOF GGUFModel
    push GMEM_ZEROINIT
    call GlobalAlloc
    test eax, eax
    jz @free
    mov edi, eax

    ; Copy resolved data into model
    mov [edi].GGUFModel.pBase, [esi].GGUFParsedInfo.pBase
    mov [edi].GGUFModel.pHeader, [esi].GGUFParsedInfo.pHeader
    mov [edi].GGUFModel.pKVMeta, [esi].GGUFParsedInfo.pKVMeta
    mov [edi].GGUFModel.pTensorMeta, [esi].GGUFParsedInfo.pTensorMeta
    mov [edi].GGUFModel.pData, [esi].GGUFParsedInfo.pBase
    mov [edi].GGUFModel.fileSizeLo, [esi].GGUFParsedInfo.fileSizeLo
    mov [edi].GGUFModel.fileSizeHi, [esi].GGUFParsedInfo.fileSizeHi
    mov [edi].GGUFModel.nTensors, [esi].GGUFParsedInfo.nTensors
    mov [edi].GGUFModel.pTensorArr, [esi].GGUFParsedInfo.pTensorArr
    mov [edi].GGUFModel.dwFlags, 1

    add esp, SIZEOF GGUFParsedInfo
    mov eax, edi
    ret

@free:
    add esp, SIZEOF GGUFParsedInfo
@fail:
    xor eax, eax
    ret

GGUF_LoaderWithResolver ENDP

; Patch point shim
GGUF_PatchExistingLoader PROC
    mov eax, OFFSET GGUF_LoaderWithResolver
    ret
GGUF_PatchExistingLoader ENDP

END
