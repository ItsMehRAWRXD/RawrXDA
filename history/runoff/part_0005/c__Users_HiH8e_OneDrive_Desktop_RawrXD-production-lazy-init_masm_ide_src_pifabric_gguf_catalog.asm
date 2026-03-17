; pifabric_gguf_catalog.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc

includelib kernel32.lib
includelib user32.lib

PUBLIC GgufCatalog_Init
PUBLIC GgufCatalog_AddModel
PUBLIC GgufCatalog_Clear
PUBLIC GgufCatalog_GetCount

.data
MAX_MODELS EQU 64

GgufCatalog STRUCT
    count DWORD ?
    handles DWORD MAX_MODELS DUP(?)
GgufCatalog ENDS

g_Catalog GgufCatalog <0>

.code

GgufCatalog_Init PROC
    mov g_Catalog.count,0
    mov eax,1
    ret
GgufCatalog_Init ENDP

GgufCatalog_AddModel PROC hFile:DWORD
    mov eax,g_Catalog.count
    cmp eax,MAX_MODELS
    jae @fail
    mov ecx,eax
    imul ecx,4
    mov edx,OFFSET g_Catalog.handles
    add edx,ecx
    mov [edx],hFile
    inc g_Catalog.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
GgufCatalog_AddModel ENDP

GgufCatalog_Clear PROC
    mov g_Catalog.count,0
    mov eax,1
    ret
GgufCatalog_Clear ENDP

GgufCatalog_GetCount PROC
    mov eax,g_Catalog.count
    ret
GgufCatalog_GetCount ENDP

END
