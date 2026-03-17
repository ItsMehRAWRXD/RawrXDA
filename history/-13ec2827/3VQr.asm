; GGUF_IDE_CONSOLE_HARNESS.ASM - Visualize IDE callbacks
.686
.model flat, stdcall
option casemap:none

include gguf_loader_interface.inc

includelib kernel32.lib

GetStdHandle PROTO :DWORD
WriteConsoleA PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
ExitProcess PROTO :DWORD

GGUF_IDE_SetProgressCallback PROTO :DWORD
GGUF_IDE_SetStatusCallback PROTO :DWORD
GGUF_IDE_SetModelLoadedCallback PROTO :DWORD
GGUF_IDE_SetTensorProgressCallback PROTO :DWORD
GGUF_IDE_SetMemoryCallback PROTO :DWORD
GGUF_IDE_SetCancelCallback PROTO :DWORD

GGUF_LoadModel PROTO :DWORD

.data
    STD_OUTPUT_HANDLE equ -11
    hOut dd 0
    szNL db 13,10,0
    szStart db "[HARNESS] Starting load",13,10,0
    szProg db "[PROG] ",0
    szStat db "[STAT] ",0
    szMem  db "[MEM]  ",0
    szTens db "[TENS] ",0
    szLoad db "[LOAD] ",0
    szPath db "C:\\Users\\HiH8e\\OllamaModels\\BigDaddyG-UNLEASHED-Q4_K_M.gguf",0

.data?
    written dd ?

.code
PrintStr proc p:DWORD
    push esi
    mov esi, p
    xor ecx, ecx
@@l: cmp byte ptr [esi+ecx], 0
    je @@d
    inc ecx
    jmp @@l
@@d:
    invoke WriteConsoleA, hOut, p, ecx, addr written, NULL
    pop esi
    ret
PrintStr endp

ProgressCB proc percent:DWORD, msg:DWORD
    invoke PrintStr, addr szProg
    invoke PrintStr, msg
    invoke PrintStr, addr szNL
    ret
ProgressCB endp

StatusCB proc level:DWORD, msg:DWORD
    invoke PrintStr, addr szStat
    invoke PrintStr, msg
    invoke PrintStr, addr szNL
    ret
StatusCB endp

ModelCB proc hModel:DWORD, success:DWORD
    invoke PrintStr, addr szLoad
    invoke PrintStr, addr szNL
    ret
ModelCB endp

TensorCB proc idx:DWORD, total:DWORD, msg:DWORD
    invoke PrintStr, addr szTens
    invoke PrintStr, msg
    invoke PrintStr, addr szNL
    ret
TensorCB endp

MemoryCB proc bytes:DWORD
    invoke PrintStr, addr szMem
    invoke PrintStr, addr szNL
    ret
MemoryCB endp

CancelCB proc
    xor eax, eax
    ret
CancelCB endp

start:
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hOut, eax
    invoke PrintStr, addr szStart
    
    invoke GGUF_IDE_SetProgressCallback, addr ProgressCB
    invoke GGUF_IDE_SetStatusCallback, addr StatusCB
    invoke GGUF_IDE_SetModelLoadedCallback, addr ModelCB
    invoke GGUF_IDE_SetTensorProgressCallback, addr TensorCB
    invoke GGUF_IDE_SetMemoryCallback, addr MemoryCB
    invoke GGUF_IDE_SetCancelCallback, addr CancelCB

    invoke GGUF_LoadModel, addr szPath
    invoke ExitProcess, eax
end start