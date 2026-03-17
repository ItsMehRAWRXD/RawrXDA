; Minimal MASM entry point to satisfy linker
.686
.model flat, stdcall
option casemap:none

ExitProcess PROTO :DWORD

.code
start:
    push 0
    call ExitProcess

end start
