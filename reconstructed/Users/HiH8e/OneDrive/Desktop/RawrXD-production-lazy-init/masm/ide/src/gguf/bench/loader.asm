.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

GGUF_LoadModel PROTO :DWORD
GGUF_CloseModel PROTO :DWORD
GetTickCount PROTO
ExitProcess PROTO :DWORD

.data
    szPath db "D:\\OllamaModels\\BigDaddyG-UNLEASHED-Q4_K_M.gguf",0

.data?
    tStart dd ?

.code
start:
    invoke GetTickCount
    mov tStart, eax

    invoke GGUF_LoadModel, addr szPath
    test eax, eax
    jz load_fail

    mov ebx, eax            ; model handle
    invoke GetTickCount
    sub eax, tStart         ; duration ms

    invoke GGUF_CloseModel, ebx
    invoke ExitProcess, eax ; exit code = load ms

load_fail:
    invoke ExitProcess, 1

END start
