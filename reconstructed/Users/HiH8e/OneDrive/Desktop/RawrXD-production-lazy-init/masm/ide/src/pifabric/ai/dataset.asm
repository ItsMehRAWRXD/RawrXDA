; pifabric_ai_dataset.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC AiDataset_Init
PUBLIC AiDataset_Add
PUBLIC AiDataset_GetCount
PUBLIC AiDataset_Clear

.data
MAX_DATASET EQU 128

DatasetEntry STRUCT
    id DWORD ?
    size DWORD ?
DatasetEntry ENDS

Dataset STRUCT
    count DWORD ?
    entries DatasetEntry MAX_DATASET DUP(<0,0>)
Dataset ENDS

g_Dataset Dataset <0>

.code

AiDataset_Init PROC
    mov g_Dataset.count,0
    mov eax,1
    ret
AiDataset_Init ENDP

AiDataset_Add PROC id:DWORD,size:DWORD
    mov eax,g_Dataset.count
    cmp eax,MAX_DATASET
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF DatasetEntry
    mov edx,OFFSET g_Dataset.entries
    add edx,ecx
    mov [edx].DatasetEntry.id,id
    mov [edx].DatasetEntry.size,size
    inc g_Dataset.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
AiDataset_Add ENDP

AiDataset_GetCount PROC
    mov eax,g_Dataset.count
    ret
AiDataset_GetCount ENDP

AiDataset_Clear PROC
    mov g_Dataset.count,0
    mov eax,1
    ret
AiDataset_Clear ENDP

END