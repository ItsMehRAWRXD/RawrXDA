; pifabric_gguf_catalog.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

; ----------------------------------------------------------------
; Simple GGUF catalog loader (JSON based)
; ----------------------------------------------------------------

PUBLIC  GGUF_Catalog_Load
PUBLIC  GGUF_Catalog_GetByName

; Catalog entry
GGUFCatalogEntry STRUCT
    szName   BYTE 64 DUP(?)
    szPath   BYTE 256 DUP(?)
GGUFCatalogEntry ENDS

; Catalog
GGUFCatalog STRUCT
    nEntries DWORD ?
    entries   GGUFCatalogEntry *
GGUFCatalog ENDS

.data

g_Catalog GGUFCatalog <0,0>

.code

GGUF_Catalog_Load PROC USES esi edi ebx lpPath:DWORD
    ; For brevity, this stub just sets nEntries=0
    mov [g_Catalog].GGUFCatalog.nEntries, 0
    mov eax, 1
    ret
GGUF_Catalog_Load ENDP

GGUF_Catalog_GetByName PROC USES esi edi ebx szName:DWORD
    ; Stub: return 0
    xor eax, eax
    ret
GGUF_Catalog_GetByName ENDP

END
