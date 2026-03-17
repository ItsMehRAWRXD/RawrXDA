; OMEGA-POLYGLOT v3.0P (Professional Reverse Engineering Edition)
.386
.model flat, stdcall
option casemap:none

MAX_FSZ equ 10485760

include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
include C:\masm32\include\user32.inc
includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib

.data
szW db "OMEGA-POLYGLOT Professional v3.0P", 0Dh, 0Ah, 0
szM db "[1]PE Analysis [0]Exit", 0Dh, 0Ah, ">", 0
szPF db "Target: ", 0
szERR db "[-] Error", 0Dh, 0Ah, 0
szOK db "[+] Success", 0Dh, 0Ah, 0

.data?
hIn dd ?
hOut dd ?
hF dd ?
fSz dd ?
pBase dd ?
numSec dd ?
inputBuf db 512 dup(?)
tempBuf db 1024 dup(?)

.code

PrintStr proc pMsg:DWORD
    local dwWritten:DWORD
    local dwLen:DWORD
    invoke lstrlenA, pMsg
    mov dwLen, eax
    invoke WriteConsoleA, hOut, pMsg, dwLen, addr dwWritten, 0
    ret
PrintStr endp

ReadStr proc
    local dwRead:DWORD
    invoke ReadConsoleA, hIn, addr inputBuf, 510, addr dwRead, 0
    mov eax, dwRead
    cmp eax, 2
    jl @@done
    sub eax, 2
    mov byte ptr [inputBuf+eax], 0
@@done:
    ret
ReadStr endp

ReadInt proc
    local dwRead:DWORD
    invoke ReadConsoleA, hIn, addr inputBuf, 16, addr dwRead, 0
    xor eax, eax
    lea esi, inputBuf
@@loop:
    movzx ecx, byte ptr [esi]
    cmp cl, 0Dh
    je @@done
    cmp cl, 0Ah
    je @@done
    cmp cl, 0
    je @@done
    cmp cl, 30h
    jb @@done
    cmp cl, 39h
    ja @@done
    sub cl, 30h
    imul eax, 10
    movzx ecx, cl
    add eax, ecx
    inc esi
    jmp @@loop
@@done:
    ret
ReadInt endp

MainMenu proc
    local choice:DWORD
@@menuLoop:
    invoke PrintStr, addr szW
    invoke PrintStr, addr szM
    call ReadInt
    mov choice, eax
    cmp choice, 0
    je @@exit
    cmp choice, 1
    je @@opt1
    jmp @@menuLoop
@@opt1:
    invoke PrintStr, addr szOK
    jmp @@menuLoop
@@exit:
    ret
MainMenu endp

main proc
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hIn, eax
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hOut, eax
    call MainMenu
    invoke ExitProcess, 0
main endp

end main
